// TGCombatPort — SKSE-плагин (DLL).
//
// Правила механики (решение пользователя 22.11.2026):
//   1. Игрок блокирует НЕ щитом (оружием/кулаками) → щитоносец выбивает его из блока.
//   2. Игрок блокирует ЩИТОМ → башем не выбивают; щит пробивает силовая атака того,
//      у кого булава или двуручный молот.
//   3. Игрок не блокирует → плагин не вмешивается.
//
// Чем подтверждено (docs/reference/):
//   * «выбить из блока» в движке умеет только стаггер: «Staggering interrupts whatever action was
//     being taken by the target» (UESP Skyrim:Combat) — см. wiki/uesp-combat.md;
//   * баш в ванили даёт лишь шанс стаггера (UESP Skyrim:Block#Bashing) — wiki/uesp-block.md;
//   * силовая атака блок не снимает, а умножает его эффективность на 0.66
//     (fBlockPowerAttackMult, CK Wiki GameSetting) — wiki/ck-gamesetting.md;
//   * архетип Stagger: magnitude 0..1, duration = пауза до следующего стаггера (CK Wiki Magic Effect);
//   * настоящий удар движок сообщает событием RE::TESHitEvent с флагами kHitBlocked / kPowerAttack /
//     kBashAttack (RE/T/TESHitEvent.h) — это основной триггер плагина;
//   * animation event атакой не является: заказ баша через NotifyAnimationGraph попаданий не создаёт
//     (лог 0.6.2, docs/reference/measured-in-game.md).
//
// Хуков (патчинга кода игры) нет: 0.1.1 патчила Actor::GetAttackChance и вызывала вылет.
//
// Допущения, которые нельзя проверить без игры, помечены AS-n и перечислены в
// docs/reference/ASSUMPTIONS.md. Каждая функция описана в docs/reference/CODE_AUDIT.md.

#include "pch.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
	using namespace std::literals;
	using Clock = std::chrono::steady_clock;

	// --- параметры ---
	constexpr float kMeleeRadius = 200.0f;        // радиус внимания (дизайн)
	constexpr float kCooldownSeconds = 7.0f;      // кулдаун баша на NPC (дизайн)
	constexpr float kStaggerMagnitude = 1.0f;     // сила стаггера 0..1 (CK Wiki Magic Effect)
	constexpr float kStaggerCooldown = 4.0f;      // пауза между стаггерами (дизайн: ~1 с стаггер + запас)
	constexpr float kStaggerDelaySeconds = 0.25f; // стаггер применяем на удар, а не на замах
	constexpr float kDisorientSeconds = 1.2f;     // сколько держим «дезориентацию» после пробития блока

	std::atomic_bool g_enabled{ true };
	std::atomic<std::uint64_t> g_ticks{ 0 };
	std::atomic<std::uint64_t> g_bashes{ 0 };
	std::atomic<std::uint64_t> g_staggers{ 0 };
	std::atomic<std::uint64_t> g_rejectLogged{ 0 };
	std::atomic<std::uint64_t> g_hitLogged{ 0 };
	std::atomic<std::uint64_t> g_dumpedRaces{ 0 };

	bool HasShield(RE::Actor* a_actor);
	const RE::TESObjectWEAP* GetHeavyBlunt(RE::Actor* a_actor);
	bool IsWatchedBash(RE::Actor* a_actor);
	void OnBashLanded(RE::Actor* a_actor);
	void OnBashFinished(RE::Actor* a_actor);
	void OnPlayerHit(RE::Actor* a_attacker, const RE::TESHitEvent* a_event);
	void OrderBash(RE::Actor* a_actor, float a_distance, Clock::time_point a_now);
	void ApplyStagger(RE::Actor* a_source, const char* a_why);

	// Слушает animation events актёра, которому заказали баш: единственное, что граф реально
	// рассылает, — bashStop (лог 0.6.2). Служит запасным триггером для НАШЕГО заказа,
	// потому что заказанный баш попаданий не создаёт.
	class BashWatchSink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
		{
			if (!a_event || !a_event->tag.c_str()) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const std::string_view tag{ a_event->tag.c_str() };
			auto* ref = a_event->holder;
			auto* actor = ref ? ref->As<RE::Actor>() : nullptr;

			if (tag == "bashStop"sv) {
				OnBashFinished(actor);
			} else if (tag == "bashStart"sv) {
				OnBashLanded(actor);
			} else if (tag == "bashRelease"sv) {
				logs::info("bandit anim: bashRelease ({})", ref ? ref->GetName() : "?");
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	BashWatchSink g_bashSink;

	// Настоящее попадание по игроку: движок сам сообщает, был ли удар блокирован,
	// силовым или башем (RE/T/TESHitEvent.h). Основной триггер правил 1 и 2.
	class HitSink final : public RE::BSTEventSink<RE::TESHitEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
		{
			if (!a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* victim = a_event->target.get();
			auto* attacker = a_event->cause.get();
			if (!player || !victim || !attacker || victim != player) {
				return RE::BSEventNotifyControl::kContinue;
			}
			auto* actor = attacker->As<RE::Actor>();
			if (!actor || actor == player) {
				return RE::BSEventNotifyControl::kContinue;
			}
			OnPlayerHit(actor, a_event);
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	HitSink g_hitSink;

	// Заказы баша: пока идёт окно, слушаем animation events этого актёра.
	struct Watch
	{
		RE::ActorHandle   actor;
		Clock::time_point until;
	};

	std::vector<Watch> g_watches;

	// Попытки заказа баша: одна попытка не чаще 0,4 с; кулдаун 7 с — только после принятого заказа.
	struct AttemptState
	{
		Clock::time_point lastTry{};
		int               fails{ 0 };
		Clock::time_point blockUntil{};
	};

	std::unordered_map<RE::FormID, AttemptState> g_attempts;
	std::unordered_map<RE::FormID, std::vector<std::string>> g_bashEventsByRace;

	// Дистанция баша из настроек движка (fCombatBashReach = 141 по CK Wiki), а не «на глаз».
	// Допущение AS-9: значение доступно через GameSettingCollection в рантайме.
	float g_bashReach{ 141.0f };

	// --- стаггер: единственный механизм, который реально прерывает блок ---
	RE::SpellItem*             g_staggerSpell{ nullptr };
	bool                       g_staggerSearched{ false };
	Clock::time_point          g_lastStagger{};
	// Пробы «вышел ли игрок из блока» после сработавшего правила.
	struct BlockProbe
	{
		Clock::time_point at;
		std::string       source;
		bool              kick{ false };
	};

	std::vector<BlockProbe> g_blockProbes;

	// Окно дезориентации: пока оно идёт, блок игрока гасим повторно, чтобы его нельзя было
	// удержать. Лог 0.7.2 показал, что blockStop в стаггере граф игнорирует (sent=false).
	Clock::time_point g_disorientUntil{};
	int               g_disorientKicks{ 0 };

	bool HasShield(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		for (const bool leftHand : { false, true }) {
			const auto* equipped = a_actor->GetEquippedObject(leftHand);
			if (!equipped) {
				continue;
			}
			const auto* armor = equipped->As<RE::TESObjectARMO>();
			if (armor && armor->IsShield()) {
				return true;
			}
		}
		return false;
	}

	// Булава или двуручный молот/топор. В движке это типы kOneHandMace и kTwoHandAxe
	// (двуручный топор отдельного типа не имеет) — docs/reference/commonlib-api.md.
	const RE::TESObjectWEAP* GetHeavyBlunt(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}
		for (const bool leftHand : { false, true }) {
			const auto* equipped = a_actor->GetEquippedObject(leftHand);
			if (!equipped) {
				continue;
			}
			const auto* weapon = equipped->As<RE::TESObjectWEAP>();
			if (!weapon) {
				continue;
			}
			const auto type = weapon->GetWeaponType();
			if (type == RE::WEAPON_TYPE::kOneHandMace || type == RE::WEAPON_TYPE::kTwoHandAxe) {
				return weapon;
			}
		}
		return nullptr;
	}

	// Имена атак расы, помеченных флагом kBashAttack. Таблица выгружается в лог при первом обращении.
	const std::vector<std::string>& GetBashEventNames(RE::Actor* a_actor)
	{
		static const std::vector<std::string> fallback{ "bashStart", "bash" };

		auto* race = a_actor ? a_actor->GetRace() : nullptr;
		if (!race) {
			return fallback;
		}

		const auto raceID = race->GetFormID();
		auto it = g_bashEventsByRace.find(raceID);
		if (it != g_bashEventsByRace.end()) {
			return it->second;
		}

		std::vector<std::string> names;
		std::vector<std::string> dump;
		if (race->attackDataMap) {
			for (auto& [event, attackDataPtr] : race->attackDataMap->attackDataMap) {
				auto* attack = attackDataPtr.get();
				if (!attack || !event.c_str()) {
					continue;
				}
				const bool isBash = attack->data.flags.any(RE::AttackData::AttackFlag::kBashAttack);
				if (dump.size() < 20) {
					dump.push_back(std::format("{} bash={} chance={:.2f}", event.c_str(), isBash, attack->data.attackChance));
				}
				if (isBash) {
					names.push_back(event.c_str());
				}
			}
		}

		if (g_dumpedRaces.fetch_add(1, std::memory_order_relaxed) < 4) {
			logs::info("race attack data: {} ({}), bash-событий: {}",
				race->GetName(),
				race->GetFormID(),
				names.size());
			for (const auto& line : dump) {
				logs::info("  atkd: {}", line);
			}
		}

		for (const auto& name : fallback) {
			if (std::find(names.begin(), names.end(), name) == names.end()) {
				names.push_back(name);
			}
		}

		const auto [inserted, _] = g_bashEventsByRace.emplace(raceID, std::move(names));
		return inserted->second;
	}

	bool IsWatchedBash(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		return std::any_of(g_watches.begin(), g_watches.end(),
			[a_actor](const Watch& w) { return w.actor.get().get() == a_actor; });
	}

	// Ищем уже загруженный игрой спелл с эффектом архетипа Stagger.
	// Предпочтение: ровно один эффект, у него duration = 0 (duration в этом архетипе —
	// не длительность стаггера, а пауза, CK Wiki) и magnitude > 0.
	void InitStaggerSpell()
	{
		g_staggerSearched = true;
		auto* handler = RE::TESDataHandler::GetSingleton();
		if (!handler) {
			logs::info("stagger: TESDataHandler недоступен, стаггер недоступен");
			return;
		}

		int            total = 0;
		int            shown = 0;
		RE::SpellItem* best = nullptr;
		int            bestScore = -1000;

		for (auto* spell : handler->GetFormArray<RE::SpellItem>()) {
			if (!spell || !spell->HasEffect(RE::EffectArchetype::kStagger)) {
				continue;
			}
			++total;

			const auto    effectCount = static_cast<int>(spell->effects.size());
			std::uint32_t duration = 0;
			float         magnitude = 0.0f;
			std::uint32_t area = 0;
			bool          foundStagger = false;
			for (auto* effect : spell->effects) {
				if (effect && effect->baseEffect && effect->baseEffect->HasArchetype(RE::EffectArchetype::kStagger)) {
					duration = effect->effectItem.duration;
					magnitude = effect->effectItem.magnitude;
					area = effect->effectItem.area;
					foundStagger = true;
					break;
				}
			}

			if (shown < 12) {
				++shown;
				logs::info("  stagger spell: {} ({:08X}) type={} cast={} delivery={} effects={} area={} dur={} mag={:.2f} cost={}",
					spell->GetName(),
					spell->GetFormID(),
					static_cast<int>(spell->GetSpellType()),
					static_cast<int>(spell->GetCastingType()),
					static_cast<int>(spell->GetDelivery()),
					effectCount,
					area,
					duration,
					magnitude,
					spell->data.costOverride);
			}

			if (!foundStagger) {
				continue;
			}
			if (spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) {
				continue;
			}
			// Жёсткие фильтры: ровно один эффект и нулевая площадь. Иначе спелл бьёт игрока
			// уроном и рисует эффекты (лог 0.7.2: «Топот великана» — разлом на земле).
			if (effectCount != 1 || area != 0) {
				continue;
			}

			// Чем ближе к «чистому стаггеру», тем выше оценка. Обычный спелл важнее абилки/крика.
			int score = 0;
			if (spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell) {
				score += 40;
			}
			if (effectCount == 1) {
				score += 30;
			}
			if (duration == 0) {
				score += 10;
			}
			if (magnitude > 0.0f) {
				score += 10;
			}
			score -= effectCount;

			if (score > bestScore) {
				bestScore = score;
				best = spell;
			}
		}

		g_staggerSpell = best;
		if (best) {
			logs::info("stagger: выбран спелл {} ({:08X}) из {} кандидатов, оценка {}, magnitude будет {:.2f}",
				best->GetName(),
				best->GetFormID(),
				total,
				bestScore,
				kStaggerMagnitude);
		} else {
			logs::info("stagger: подходящего спелла нет (кандидатов {}) — блок ломать нечем", total);
		}
	}

	bool StaggerOnCooldown(Clock::time_point a_now)
	{
		return g_lastStagger.time_since_epoch().count() != 0 &&
			std::chrono::duration<float>(a_now - g_lastStagger).count() < kStaggerCooldown;
	}

	// Снять блок у игрока: граф понимает blockStop (лог 0.6.2/0.7.1: событие доходит и от него
	// зависит состояние блока). Своего API для блока в движке нет — tools/verify_api.py это подтверждает.
	// Допущение AS-10: blockStop во время стаггера действительно гасит блок и он не возвращается,
	// пока игрок стаггерится.
	void KickBlock(RE::PlayerCharacter* a_player, const char* a_why)
	{
		if (!a_player || !a_player->IsBlocking()) {
			return;
		}
		const bool sent = a_player->NotifyAnimationGraph(RE::BSFixedString("blockStop"));
		logs::info("block kick: {} → blockStop sent={}", a_why, sent);
	}

	// Основной способ: переводим игрока в собственное состояние стаггера графа (staggerStart).
	// Это то состояние, которым движок сам прерывает действия (UESP: «Staggering interrupts
	// whatever action was being taken by the target»), и в отличие от спелла оно не даёт
	// ни урона, ни эффектов на земле (лог 0.7.2: «Топот великана» бил игрока и рисовал разлом).
	// Допущение AS-14.
	bool StaggerByGraph(RE::PlayerCharacter* a_player, const std::string& a_why)
	{
		if (!a_player) {
			return false;
		}
		const bool sent = a_player->NotifyAnimationGraph(RE::BSFixedString("staggerStart"));
		logs::info("stagger anim: staggerStart sent={} ({})", sent, a_why);
		return sent;
	}

	// Накладываем стаггер на игрока от имени NPC: caster — сам NPC, target — игрок.
	// AS-1 опровергнуто логом 0.7.1: стаггер НЕ снимает блок у игрока, поэтому блок гасим отдельно
	// (AS-10, KickBlock). Подтверждены: AS-2 (стаггер виден), AS-4 (каст без анимации).
	// Эффект stagger: сначала состояние графа (без урона и визуальных эффектов),
	// и только если граф отказал — спелл-стаггер.
	void Stagger(RE::Actor* a_source, const std::string& a_why)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (StaggerByGraph(player, a_why)) {
			return;
		}
		ApplyStagger(a_source, a_why.c_str());
	}

	void ApplyStagger(RE::Actor* a_source, const char* a_why)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_source || a_source->IsDead()) {
			return;
		}
		if (!g_staggerSpell) {
			logs::info("stagger: пропуск ({}) — спелла нет", a_why);
			return;
		}

		auto* caster = a_source->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		if (!caster) {
			logs::info("stagger: у {} нет caster kInstant", a_source->GetName());
			return;
		}

		caster->CastSpellImmediate(g_staggerSpell, true, player, 1.0f, false, kStaggerMagnitude, a_source);
		g_staggers.fetch_add(1, std::memory_order_relaxed);
		g_lastStagger = Clock::now();

		logs::info("stagger cast: {} caster={} spell={} ({:08X}) mag={:.2f} игрок блокирует={}",
			a_why,
			a_source->GetName(),
			g_staggerSpell->GetName(),
			g_staggerSpell->GetFormID(),
			kStaggerMagnitude,
			player->IsBlocking());

		// Стаггер у ИГРОКА блок не прерывает (лог 0.7.1: blocking=true при staggered=true),
		// поэтому снимаем блок тем же событием, которым это делает сам граф. Допущение AS-10.
		KickBlock(player, a_why);
	}

	// Пробы: печатаем, что стало с игроком после стаггера. Допущение AS-7 (IsStaggering у игрока).
	// Это и есть проверка механики: `blocking=false` = игрок вышел из блока.
	void ProcessBlockProbes(Clock::time_point a_now)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			g_blockProbes.clear();
			return;
		}
		for (auto it = g_blockProbes.begin(); it != g_blockProbes.end();) {
			if (a_now < it->at) {
				++it;
				continue;
			}
			const bool blocking = player->IsBlocking();
			const bool staggered = player->IsStaggering();
			logs::info("block probe: {} blocking={} staggered={}", it->source, blocking, staggered);
			if (it->kick && blocking && staggered) {
				// Блок вернулся, пока игрок ещё в стаггере — гасим снова (AS-10).
				KickBlock(player, it->source.c_str());
			}
			it = g_blockProbes.erase(it);
		}
	}

	// ==================== движок правил ====================
	// Правило — это ДАННЫЕ, а не код: секция в Data/SKSE/Plugins/TGCombatPort/Rules.txt.
	//   «такая-то атака такого-то врага делает то же самое» = новая секция [имя] в файле;
	//   «и ещё вот это происходит» = дописать эффект в строку do = ...
	// Код не знает ни про бандита, ни про булаву: он умеет только проверять условия
	// и запускать эффекты из kEffectTable. Добавили эффект — одна запись в таблице.

	enum class Tri { kAny, kYes, kNo };
	enum class EffectId { kStagger, kUnblock, kBash };

	constexpr unsigned kTriggerHit = 1u << 0;   // настоящее попадание (RE::TESHitEvent)
	constexpr unsigned kTriggerBash = 1u << 1;  // наш заказанный баш закончился (bashStop)

	struct EffectDef
	{
		const char* id;
		EffectId    effect;
	};

	constexpr EffectDef kEffectTable[] = {
		{ "stagger", EffectId::kStagger },  // стаггер спеллом от имени NPC (ApplyStagger)
		{ "unblock", EffectId::kUnblock },  // снять блок и удерживать снятым (KickBlock)
		{ "bash", EffectId::kBash },        // заказать NPC баш (OrderBash)
	};

	struct WeaponDef
	{
		const char*     id;
		RE::WEAPON_TYPE type;
	};

	constexpr WeaponDef kWeaponTable[] = {
		{ "fists", RE::WEAPON_TYPE::kHandToHandMelee },
		{ "sword", RE::WEAPON_TYPE::kOneHandSword },
		{ "dagger", RE::WEAPON_TYPE::kOneHandDagger },
		{ "axe", RE::WEAPON_TYPE::kOneHandAxe },
		{ "mace", RE::WEAPON_TYPE::kOneHandMace },
		{ "greatsword", RE::WEAPON_TYPE::kTwoHandSword },
		{ "battleaxe", RE::WEAPON_TYPE::kTwoHandAxe },
		{ "bow", RE::WEAPON_TYPE::kBow },
		{ "staff", RE::WEAPON_TYPE::kStaff },
		{ "crossbow", RE::WEAPON_TYPE::kCrossbow },
	};

	// Ссылка на форму из файла правил: пишется человеком (EditorID или плагин|0xID),
	// при загрузке превращается в FormID. Расшифровка остаётся для лога.
	struct FormFilter
	{
		RE::FormID  id{ 0 };
		std::string text;
	};

	// Вид атаки определяется по форме, которую движок положил в TESHitEvent::source.
	enum class AttackKind { kMelee = 1u << 0, kRanged = 1u << 1, kSpell = 1u << 2, kShout = 1u << 3, kUnarmed = 1u << 4 };

	// Что движок сообщил о попадании: флаги, источник, снаряд, раса бьющего, вид атаки.
	struct HitContext
	{
		std::uint8_t bits{ 0 };
		RE::FormID   source{ 0 };
		RE::FormID   projectile{ 0 };
		RE::FormID   race{ 0 };
		AttackKind   kind{ AttackKind::kUnarmed };
	};

	struct Rule
	{
		std::string                  name;
		unsigned                     triggers{ 0 };
		std::uint8_t                 hitRequire{ 0 };
		std::uint8_t                 hitForbid{ 0 };
		Tri                          attackerShield{ Tri::kAny };
		Tri                          playerBlocking{ Tri::kYes };
		Tri                          playerShield{ Tri::kAny };
		std::vector<RE::WEAPON_TYPE> weapons;
		std::vector<FormFilter>      races;
		std::vector<FormFilter>      sources;
		std::vector<FormFilter>      projectiles;
		unsigned                     kinds{ 0 };
		bool                         reach{ true };
		float                        cooldown{ 4.0f };
		std::vector<EffectId>        effects;
		bool                         valid{ true };
		Clock::time_point            lastFired{};
	};

	std::vector<Rule> g_rules;

	// Отложенный запуск правила: у заказанного баша нет попадания, эффект нужен в момент удара.
	struct PendingFire
	{
		RE::ActorHandle   source;
		Clock::time_point at;
		std::size_t       rule{ 0 };
	};

	std::vector<PendingFire> g_pendingFires;

	const RE::TESObjectWEAP* GetEquippedWeapon(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return nullptr;
		}
		for (const bool leftHand : { false, true }) {
			if (auto* equipped = a_actor->GetEquippedObject(leftHand)) {
				if (const auto* weapon = equipped->As<RE::TESObjectWEAP>()) {
					return weapon;
				}
			}
		}
		return nullptr;
	}

	bool MatchTri(Tri a_condition, bool a_value)
	{
		switch (a_condition) {
		case Tri::kAny: return true;
		case Tri::kYes: return a_value;
		case Tri::kNo:  return !a_value;
		}
		return false;
	}

	std::string Trim(std::string a_text)
	{
		const auto notSpace = [](unsigned char a_char) { return !std::isspace(a_char); };
		a_text.erase(a_text.begin(), std::find_if(a_text.begin(), a_text.end(), notSpace));
		a_text.erase(std::find_if(a_text.rbegin(), a_text.rend(), notSpace).base(), a_text.end());
		return a_text;
	}

	std::vector<std::string> SplitList(const std::string& a_text)
	{
		std::vector<std::string> items;
		std::string              current;
		for (const char character : a_text) {
			if (character == ',') {
				const auto item = Trim(current);
				if (!item.empty()) {
					items.push_back(item);
				}
				current.clear();
			} else {
				current.push_back(character);
			}
		}
		const auto tail = Trim(current);
		if (!tail.empty()) {
			items.push_back(tail);
		}
		return items;
	}

	bool ParseTri(const std::string& a_value, Tri& a_out)
	{
		if (a_value == "yes") { a_out = Tri::kYes; return true; }
		if (a_value == "no") { a_out = Tri::kNo; return true; }
		if (a_value == "any") { a_out = Tri::kAny; return true; }
		return false;
	}

	bool ParseWeapon(const std::string& a_value, RE::WEAPON_TYPE& a_out)
	{
		for (const auto& entry : kWeaponTable) {
			if (a_value == entry.id) {
				a_out = entry.type;
				return true;
			}
		}
		return false;
	}

	bool ParseEffect(const std::string& a_value, EffectId& a_out)
	{
		for (const auto& entry : kEffectTable) {
			if (a_value == entry.id) {
				a_out = entry.effect;
				return true;
			}
		}
		return false;
	}

	std::uint8_t HitFlagBit(const std::string& a_value)
	{
		if (a_value == "blocked") { return static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kHitBlocked); }
		if (a_value == "power") { return static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kPowerAttack); }
		if (a_value == "bash") { return static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kBashAttack); }
		if (a_value == "sneak") { return static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kSneakAttack); }
		return 0;
	}

	const char* TriText(Tri a_value)
	{
		switch (a_value) {
		case Tri::kAny: return "any";
		case Tri::kYes: return "yes";
		case Tri::kNo:  return "no";
		}
		return "?";
	}

	std::string TriggerText(unsigned a_triggers)
	{
		std::string text;
		if ((a_triggers & kTriggerHit) != 0) {
			text = "hit";
		}
		if ((a_triggers & kTriggerBash) != 0) {
			text += (text.empty() ? "" : ",") + std::string("bash");
		}
		return text.empty() ? std::string("none") : text;
	}

	std::string WeaponsText(const std::vector<RE::WEAPON_TYPE>& a_weapons)
	{
		if (a_weapons.empty()) {
			return "any";
		}
		std::string text;
		for (const auto type : a_weapons) {
			for (const auto& entry : kWeaponTable) {
				if (entry.type == type) {
					text += (text.empty() ? "" : ",") + std::string(entry.id);
					break;
				}
			}
		}
		return text;
	}

	std::string EffectsText(const std::vector<EffectId>& a_effects)
	{
		std::string text;
		for (const auto effect : a_effects) {
			for (const auto& entry : kEffectTable) {
				if (entry.effect == effect) {
					text += (text.empty() ? "" : ",") + std::string(entry.id);
					break;
				}
			}
		}
		return text;
	}

	// Допущение AS-13: LookupByEditorID и LookupFormID находят формы загруженных плагинов.
	bool ResolveForm(const std::string& a_text, RE::FormID& a_out)
	{
		a_out = 0;
		const auto bar = a_text.find('|');
		if (bar != std::string::npos) {
			// «Skyrim.esm|0x131E9» — форма из конкретного плагина, устойчиво к порядку загрузки.
			const auto plugin = Trim(a_text.substr(0, bar));
			const auto local = Trim(a_text.substr(bar + 1));
			auto*      dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return false;
			}
			try {
				a_out = dataHandler->LookupFormID(static_cast<RE::FormID>(std::stoul(local, nullptr, 16)), plugin);
			} catch (...) {
				return false;
			}
			return a_out != 0;
		}
		if (a_text.size() > 2 && a_text[0] == '0' && (a_text[1] == 'x' || a_text[1] == 'X')) {
			try {
				a_out = static_cast<RE::FormID>(std::stoul(a_text, nullptr, 16));
			} catch (...) {
				return false;
			}
			return a_out != 0;
		}
		// Обычное имя из Creation Kit, например BearRace.
		if (const auto* form = RE::TESForm::LookupByEditorID(a_text)) {
			a_out = form->GetFormID();
			return a_out != 0;
		}
		return false;
	}

	std::string NameOf(RE::FormID a_formID)
	{
		if (a_formID == 0) {
			return "00000000";
		}
		char buffer[256]{};
		const auto* form = RE::TESForm::LookupByID(a_formID);
		if (!form) {
			std::snprintf(buffer, sizeof(buffer), "%08X", a_formID);
			return buffer;
		}
		const auto* name = form->GetName();
		std::snprintf(buffer, sizeof(buffer), "%s (%08X)", name ? name : "<без имени>", a_formID);
		return buffer;
	}

	// Допущение AS-12: вид атаки выводим из формы в TESHitEvent::source:
	// оружие → ближний бой или стрельба (лук/арбалет), заклинание/свиток → магия, крик → крик,
	// снаряд (болт) → стрельба, ничего/существо → удар без оружия.
	AttackKind ClassifyAttack(RE::FormID a_source)
	{
		const auto* form = a_source != 0 ? RE::TESForm::LookupByID(a_source) : nullptr;
		if (!form) {
			return AttackKind::kUnarmed;
		}
		switch (form->GetFormType()) {
		case RE::FormType::Weapon:
			{
				const auto* weapon = form->As<RE::TESObjectWEAP>();
				const auto  type = weapon ? weapon->GetWeaponType() : RE::WEAPON_TYPE::kHandToHandMelee;
				return (type == RE::WEAPON_TYPE::kBow || type == RE::WEAPON_TYPE::kCrossbow) ?
					AttackKind::kRanged : AttackKind::kMelee;
			}
		case RE::FormType::Ammo:
			return AttackKind::kRanged;
		case RE::FormType::Spell:
		case RE::FormType::Scroll:
			return AttackKind::kSpell;
		case RE::FormType::Shout:
			return AttackKind::kShout;
		default:
			return AttackKind::kUnarmed;
		}
	}

	unsigned KindBit(AttackKind a_kind)
	{
		return static_cast<unsigned>(a_kind);
	}

	const char* KindText(AttackKind a_kind)
	{
		switch (a_kind) {
		case AttackKind::kMelee: return "melee";
		case AttackKind::kRanged: return "ranged";
		case AttackKind::kSpell: return "spell";
		case AttackKind::kShout: return "shout";
		case AttackKind::kUnarmed: return "unarmed";
		}
		return "?";
	}

	bool ParseKind(const std::string& a_value, unsigned& a_out)
	{
		a_out = 0;
		for (const auto& item : SplitList(a_value)) {
			if (item == "any") {
				continue;
			}
			if (item == "melee") { a_out |= KindBit(AttackKind::kMelee); }
			else if (item == "ranged") { a_out |= KindBit(AttackKind::kRanged); }
			else if (item == "spell") { a_out |= KindBit(AttackKind::kSpell); }
			else if (item == "shout") { a_out |= KindBit(AttackKind::kShout); }
			else if (item == "unarmed") { a_out |= KindBit(AttackKind::kUnarmed); }
			else { return false; }
		}
		return true;
	}

	bool ParseFormList(const std::string& a_value, std::vector<FormFilter>& a_out)
	{
		a_out.clear();
		for (const auto& item : SplitList(a_value)) {
			if (item == "any") {
				continue;
			}
			RE::FormID id = 0;
			if (!ResolveForm(item, id)) {
				logs::warn("Rules.txt: форма «{}» не найдена (нет такого EditorID/FormID/плагина)", item);
				return false;
			}
			a_out.push_back(FormFilter{ id, item });
		}
		return true;
	}

	bool HasForm(const std::vector<FormFilter>& a_list, RE::FormID a_id)
	{
		return std::any_of(a_list.begin(), a_list.end(), [a_id](const FormFilter& a_item) { return a_item.id == a_id; });
	}

	std::string FormsText(const std::vector<FormFilter>& a_list)
	{
		if (a_list.empty()) {
			return "any";
		}
		std::string text;
		for (const auto& item : a_list) {
			text += (text.empty() ? "" : ",") + item.text;
		}
		return text;
	}

	std::string KindsText(unsigned a_kinds)
	{
		if (a_kinds == 0) {
			return "any";
		}
		std::string text;
		for (const auto kind : { AttackKind::kMelee, AttackKind::kRanged, AttackKind::kSpell, AttackKind::kShout, AttackKind::kUnarmed }) {
			if ((a_kinds & KindBit(kind)) != 0) {
				text += (text.empty() ? "" : ",") + std::string(KindText(kind));
			}
		}
		return text;
	}

	bool ApplyKey(Rule& a_rule, const std::string& a_key, const std::string& a_value)
	{
		if (a_key == "trigger") {
			a_rule.triggers = 0;
			for (const auto& item : SplitList(a_value)) {
				if (item == "hit") {
					a_rule.triggers |= kTriggerHit;
				} else if (item == "bash") {
					a_rule.triggers |= kTriggerBash;
				} else {
					return false;
				}
			}
			return a_rule.triggers != 0;
		}
		if (a_key == "hit-flags" || a_key == "hit-not-flags") {
			std::uint8_t bits = 0;
			for (const auto& item : SplitList(a_value)) {
				const auto bit = HitFlagBit(item);
				if (bit == 0) {
					return false;
				}
				bits |= bit;
			}
			if (a_key == "hit-flags") {
				a_rule.hitRequire = bits;
			} else {
				a_rule.hitForbid = bits;
			}
			return true;
		}
		if (a_key == "attacker.race") {
			if (!ParseFormList(a_value, a_rule.races)) {
				a_rule.valid = false;
				return false;
			}
			return true;
		}
		if (a_key == "hit.source") {
			if (!ParseFormList(a_value, a_rule.sources)) {
				a_rule.valid = false;
				return false;
			}
			return true;
		}
		if (a_key == "hit.projectile") {
			if (!ParseFormList(a_value, a_rule.projectiles)) {
				a_rule.valid = false;
				return false;
			}
			return true;
		}
		if (a_key == "attack.kind") {
			unsigned kinds = 0;
			if (!ParseKind(a_value, kinds)) {
				return false;
			}
			a_rule.kinds = kinds;
			return true;
		}
		if (a_key == "attacker.shield") { return ParseTri(a_value, a_rule.attackerShield); }
		if (a_key == "player.blocking") { return ParseTri(a_value, a_rule.playerBlocking); }
		if (a_key == "player.shield") { return ParseTri(a_value, a_rule.playerShield); }
		if (a_key == "attacker.weapon") {
			a_rule.weapons.clear();
			for (const auto& item : SplitList(a_value)) {
				if (item == "any") {
					continue;
				}
				RE::WEAPON_TYPE type{};
				if (!ParseWeapon(item, type)) {
					return false;
				}
				a_rule.weapons.push_back(type);
			}
			return true;
		}
		if (a_key == "reach") {
			Tri tri{};
			if (!ParseTri(a_value, tri)) {
				return false;
			}
			a_rule.reach = tri != Tri::kNo;
			return true;
		}
		if (a_key == "cooldown") {
			try {
				a_rule.cooldown = std::stof(a_value);
			} catch (...) {
				return false;
			}
			return a_rule.cooldown >= 0.0f;
		}
		if (a_key == "do") {
			a_rule.effects.clear();
			for (const auto& item : SplitList(a_value)) {
				EffectId effect{};
				if (!ParseEffect(item, effect)) {
					return false;
				}
				a_rule.effects.push_back(effect);
			}
			return !a_rule.effects.empty();
		}
		return false;
	}

	void AddDefaultRules()
	{
		Rule bash;
		bash.name = "bandit-bash-breaks-weapon-block";
		bash.triggers = kTriggerHit | kTriggerBash;
		bash.hitRequire = static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kHitBlocked);
		bash.attackerShield = Tri::kYes;
		bash.playerBlocking = Tri::kYes;
		bash.playerShield = Tri::kNo;
		bash.effects = { EffectId::kUnblock, EffectId::kStagger };
		bash.cooldown = kStaggerCooldown;

		Rule power;
		power.name = "heavy-blunt-power-breaks-shield-block";
		power.triggers = kTriggerHit;
		power.hitRequire = static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kHitBlocked) |
			static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kPowerAttack);
		power.weapons = { RE::WEAPON_TYPE::kOneHandMace, RE::WEAPON_TYPE::kTwoHandAxe };
		power.playerBlocking = Tri::kYes;
		power.playerShield = Tri::kYes;
		power.effects = { EffectId::kUnblock, EffectId::kStagger };
		power.cooldown = kStaggerCooldown;

		g_rules = { bash, power };
	}

	void LogRules()
	{
		logs::info("правил загружено: {}", g_rules.size());
		for (const auto& rule : g_rules) {
			logs::info("  [{}]{} trigger={} hit={:02X} not={:02X} kind={} attacker.race={} attacker.weapon={} attacker.shield={} hit.source={} hit.projectile={} player.blocking={} player.shield={} reach={} do={}",
				rule.name,
				rule.valid ? "" : " (ОТКЛЮЧЕНО: ошибка в файле)",
				TriggerText(rule.triggers),
				static_cast<unsigned>(rule.hitRequire),
				static_cast<unsigned>(rule.hitForbid),
				KindsText(rule.kinds),
				FormsText(rule.races),
				WeaponsText(rule.weapons),
				TriText(rule.attackerShield),
				FormsText(rule.sources),
				FormsText(rule.projectiles),
				TriText(rule.playerBlocking),
				TriText(rule.playerShield),
				rule.reach,
				EffectsText(rule.effects));
		}
	}

	std::filesystem::path RulesPath()
	{
		std::error_code error;
		const auto      base = std::filesystem::current_path(error);
		if (error) {
			return {};
		}
		return base / "Data" / "SKSE" / "Plugins" / "TGCombatPort" / "Rules.txt";
	}

	// Допущение AS-11: файл правил лежит в Data/SKSE/Plugins/TGCombatPort/Rules.txt
	// относительно рабочего каталога игры; если его нет — работают встроенные правила.
	void LoadRules()
	{
		g_rules.clear();

		const auto    path = RulesPath();
		std::ifstream file(path);
		if (!file) {
			logs::info("Rules.txt не найден [{}], работают встроенные правила", path.empty() ? std::string("<нет пути>") : path.string());
			AddDefaultRules();
			LogRules();
			return;
		}

		std::vector<Rule> rules;
		Rule              current;
		bool              hasCurrent = false;
		int               lineNumber = 0;
		int               broken = 0;
		std::string       line;

		while (std::getline(file, line)) {
			++lineNumber;
			const auto comment = line.find_first_of("#;");
			if (comment != std::string::npos) {
				line.erase(comment);
			}
			line = Trim(line);
			if (line.empty()) {
				continue;
			}
			if (line.front() == '[' && line.back() == ']') {
				if (hasCurrent) {
					rules.push_back(current);
				}
				current = Rule{};
				current.name = Trim(line.substr(1, line.size() - 2));
				hasCurrent = true;
				continue;
			}
			const auto equal = line.find('=');
			if (equal == std::string::npos) {
				logs::warn("Rules.txt:{}: строка без «=»: {}", lineNumber, line);
				++broken;
				continue;
			}
			if (!hasCurrent) {
				logs::warn("Rules.txt:{}: ключ вне секции: {}", lineNumber, line);
				++broken;
				continue;
			}
			const auto key = Trim(line.substr(0, equal));
			const auto value = Trim(line.substr(equal + 1));
			if (!ApplyKey(current, key, value)) {
				logs::warn("Rules.txt:{}: не понял «{} = {}»", lineNumber, key, value);
				++broken;
			}
		}
		if (hasCurrent) {
			rules.push_back(current);
		}

		if (rules.empty()) {
			logs::warn("Rules.txt: {} — правил нет, работают встроенные", path.string());
			AddDefaultRules();
		} else {
			g_rules = std::move(rules);
		}
		logs::info("Rules.txt [{}]: правил {}, строк с ошибками {}", path.string(), g_rules.size(), broken);
		LogRules();
	}

	bool RuleMatches(const Rule& a_rule, unsigned a_trigger, RE::Actor* a_attacker,
		RE::PlayerCharacter* a_player, const HitContext& a_hit)
	{
		if (!a_rule.valid || !a_attacker || !a_player || a_attacker->IsDead()) {
			return false;
		}
		if ((a_rule.triggers & a_trigger) == 0) {
			return false;
		}
		if (a_trigger == kTriggerHit) {
			if ((a_hit.bits & a_rule.hitRequire) != a_rule.hitRequire) {
				return false;
			}
			if ((a_hit.bits & a_rule.hitForbid) != 0) {
				return false;
			}
		}
		if (!a_rule.races.empty() && !HasForm(a_rule.races, a_hit.race)) {
			return false;
		}
		if (!a_rule.sources.empty() && !HasForm(a_rule.sources, a_hit.source)) {
			return false;
		}
		if (!a_rule.projectiles.empty() && !HasForm(a_rule.projectiles, a_hit.projectile)) {
			return false;
		}
		if (a_rule.kinds != 0 && (a_rule.kinds & KindBit(a_hit.kind)) == 0) {
			return false;
		}
		if (!MatchTri(a_rule.attackerShield, HasShield(a_attacker))) {
			return false;
		}
		if (!MatchTri(a_rule.playerBlocking, a_player->IsBlocking())) {
			return false;
		}
		if (!MatchTri(a_rule.playerShield, HasShield(a_player))) {
			return false;
		}
		if (!a_rule.weapons.empty()) {
			const auto* weapon = GetEquippedWeapon(a_attacker);
			if (!weapon) {
				return false;
			}
			const auto type = weapon->GetWeaponType();
			if (std::find(a_rule.weapons.begin(), a_rule.weapons.end(), type) == a_rule.weapons.end()) {
				return false;
			}
		}
		if (a_rule.reach && a_attacker->GetPosition().GetDistance(a_player->GetPosition()) > g_bashReach) {
			return false;
		}
		return true;
	}

	bool RuleOnCooldown(const Rule& a_rule, Clock::time_point a_now)
	{
		return a_rule.lastFired.time_since_epoch().count() != 0 &&
			std::chrono::duration<float>(a_now - a_rule.lastFired).count() < a_rule.cooldown;
	}

	// Пробы «вышел ли игрок из блока»: одно измерение на сработавшее правило.
	// a_kick — эффект unblock просил гасить блок, пока игрок стаггерится.
	void ScheduleProbes(const std::string& a_why, bool a_kick)
	{
		for (auto& probe : g_blockProbes) {
			if (probe.source == a_why) {
				probe.kick = probe.kick || a_kick;
				return;
			}
		}
		for (const float offset : { 0.15f, 0.45f, 0.9f, 1.4f }) {
			g_blockProbes.push_back(BlockProbe{
				Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(offset)),
				a_why,
				a_kick });
		}
	}

	void RunEffect(EffectId a_effect, RE::Actor* a_source, const std::string& a_why)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		switch (a_effect) {
		case EffectId::kStagger:
			Stagger(a_source, a_why);
			break;
		case EffectId::kUnblock:
			KickBlock(player, a_why.c_str());
			break;
		case EffectId::kBash:
			if (player && a_source) {
				OrderBash(a_source, a_source->GetPosition().GetDistance(player->GetPosition()), Clock::now());
			}
			break;
		}
		if (a_effect == EffectId::kUnblock || a_effect == EffectId::kStagger) {
			g_disorientUntil = Clock::now() +
				std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kDisorientSeconds));
			g_disorientKicks = 0;
		}
		ScheduleProbes(a_why, a_effect == EffectId::kUnblock);
	}

	void FireRule(Rule& a_rule, RE::Actor* a_source, const char* a_via)
	{
		a_rule.lastFired = Clock::now();
		logs::info("rule fired: [{}] via={} actor={} effects={}",
			a_rule.name,
			a_via,
			a_source->GetName(),
			EffectsText(a_rule.effects));
		for (const auto effect : a_rule.effects) {
			RunEffect(effect, a_source, a_rule.name);
		}
	}

	void ProcessPendingFires(Clock::time_point a_now)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			g_pendingFires.clear();
			return;
		}
		for (auto it = g_pendingFires.begin(); it != g_pendingFires.end();) {
			if (a_now < it->at) {
				++it;
				continue;
			}
			auto*             source = it->source.get().get();
			const std::size_t index = it->rule;
			it = g_pendingFires.erase(it);

			if (!source || index >= g_rules.size()) {
				continue;
			}
			auto& rule = g_rules[index];
			if (!RuleMatches(rule, kTriggerBash, source, player, HitContext{})) {
				logs::info("rule skipped: [{}] источник уже не подходит", rule.name);
				continue;
			}
			if (RuleOnCooldown(rule, a_now)) {
				continue;
			}
			FireRule(rule, source, "заказ");
		}
	}

	// Пока идёт окно дезориентации, блок игрока гасим повторно: удержать его он не должен.
	// Лог 0.7.2: во время стаггера граф отвечает на blockStop sent=false, поэтому пробуем снова.
	void ProcessDisorient(Clock::time_point a_now)
	{
		if (a_now > g_disorientUntil) {
			g_disorientKicks = 0;
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->IsBlocking()) {
			return;
		}
		if (g_disorientKicks < 6) {
			++g_disorientKicks;
			KickBlock(player, "дезориентация: блок не удержать");
		}
	}

	// Чтение настроек движка: только чтение, ванильные формы не меняем.
	void ReadEngineSettings()
	{
		auto* collection = RE::GameSettingCollection::GetSingleton();
		if (!collection) {
			logs::info("настройки движка недоступны, дистанция баша = {:.0f} (значение из CK Wiki)", g_bashReach);
			return;
		}
		auto* setting = collection->GetSetting("fCombatBashReach");
		if (setting && setting->GetType() == RE::Setting::Type::kFloat) {
			const float value = setting->GetFloat();
			if (value > 0.0f) {
				g_bashReach = value;
			}
			logs::info("fCombatBashReach = {:.0f} (дистанция баша)", value);
			return;
		}
		logs::info("fCombatBashReach не найдена, дистанция баша = {:.0f} (значение из CK Wiki)", g_bashReach);
	}

	// Реакция на НАСТОЯЩЕЕ попадание по игроку: условия берутся из правил (Rules.txt).
	// Допущение AS-5: движок заполняет флаги так, как описано в перечислении TESHitEvent::Flag.
	// Допущение AS-12: вид атаки выводим из формы в TESHitEvent::source.
	void OnPlayerHit(RE::Actor* a_attacker, const RE::TESHitEvent* a_event)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_attacker || !a_event) {
			return;
		}

		HitContext hit;
		if (a_event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)) {
			hit.bits |= static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kHitBlocked);
		}
		if (a_event->flags.any(RE::TESHitEvent::Flag::kPowerAttack)) {
			hit.bits |= static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kPowerAttack);
		}
		if (a_event->flags.any(RE::TESHitEvent::Flag::kBashAttack)) {
			hit.bits |= static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kBashAttack);
		}
		if (a_event->flags.any(RE::TESHitEvent::Flag::kSneakAttack)) {
			hit.bits |= static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kSneakAttack);
		}
		hit.source = a_event->source;
		hit.projectile = a_event->projectile;
		if (const auto* race = a_attacker->GetRace()) {
			hit.race = race->GetFormID();
		}
		hit.kind = ClassifyAttack(hit.source);

		if (g_hitLogged.fetch_add(1, std::memory_order_relaxed) < 20) {
			logs::info("hit: actor={} kind={} flags={:02X} race={} source={} projectile={} игрок.блок={} игрок.щит={} враж.щит={}",
				a_attacker->GetName(),
				KindText(hit.kind),
				static_cast<unsigned>(hit.bits),
				NameOf(hit.race),
				NameOf(hit.source),
				NameOf(hit.projectile),
				player->IsBlocking(),
				HasShield(player),
				HasShield(a_attacker));
			const auto* heavyBlunt = GetHeavyBlunt(a_attacker);
			if (heavyBlunt) {
				logs::info("hit: у бьющего тяжёлое дробящее — {}", heavyBlunt->GetName());
			}
		}

		bool matched = false;
		for (auto& rule : g_rules) {
			if (!RuleMatches(rule, kTriggerHit, a_attacker, player, hit)) {
				continue;
			}
			matched = true;
			if (RuleOnCooldown(rule, Clock::now())) {
				continue;
			}
			FireRule(rule, a_attacker, "попадание");
		}

		if (!matched && (hit.bits & static_cast<std::uint8_t>(RE::TESHitEvent::Flag::kHitBlocked)) != 0 &&
			g_rejectLogged.fetch_add(1, std::memory_order_relaxed) < 8) {
			logs::info("hit без эффекта: удар был заблокирован, но ни одно правило не подошло (id выше)");
		}
	}

	// Запасной путь для нашего заказа: баш попаданий не создаёт, поэтому эффект ставим
	// в очередь на момент удара (bashStop + задержка). Условия — те же правила, trigger = bash.
	// Допущения: AS-3 (bashStop = конец замаха), AS-8 (момент совпадает с «щит достал»).
	void OnBashFinished(RE::Actor* a_actor)
	{
		if (!a_actor || !IsWatchedBash(a_actor)) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		bool matched = false;
		for (std::size_t index = 0; index < g_rules.size(); ++index) {
			auto& rule = g_rules[index];
			if (!RuleMatches(rule, kTriggerBash, a_actor, player, HitContext{})) {
				continue;
			}
			matched = true;
			if (RuleOnCooldown(rule, Clock::now())) {
				continue;
			}
			g_pendingFires.push_back(PendingFire{
				a_actor->GetHandle(),
				Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kStaggerDelaySeconds)),
				index });
		}

		if (!matched) {
			const float distance = a_actor->GetPosition().GetDistance(player->GetPosition());
			logs::info("bash finished: actor={} dist={:.0f} shield={} игрок.блок={} игрок.щит={} → ни одно правило не подошло",
				a_actor->GetName(),
				distance,
				HasShield(a_actor),
				player->IsBlocking(),
				HasShield(player));
		}
	}

	// bashStart слушателям не приходит (см. лог 0.6.2), оставляем только для истории.
	void OnBashLanded(RE::Actor* a_actor)
	{
		if (!a_actor || !IsWatchedBash(a_actor)) {
			return;
		}
		logs::info("bandit anim: bashStart actor={} shield={}", a_actor->GetName(), HasShield(a_actor));
	}

	void CloseExpiredWatches(Clock::time_point a_now)
	{
		for (auto it = g_watches.begin(); it != g_watches.end();) {
			if (a_now < it->until) {
				++it;
				continue;
			}
			if (auto actor = it->actor.get().get()) {
				actor->RemoveAnimationGraphEventSink(&g_bashSink);
			}
			it = g_watches.erase(it);
		}
	}

	// Правило 1, активная часть: заказываем баш щитоносцу, если игрок блокирует не щитом.
	// Допущение AS-6: баш проигрывается щитом только при вынутом оружии.
	void OrderBash(RE::Actor* a_actor, float a_distance, Clock::time_point a_now)
	{
		const auto actorID = a_actor->GetFormID();
		auto& state = g_attempts[actorID];

		if (a_now < state.blockUntil) {
			return;
		}
		if (state.lastTry.time_since_epoch().count() != 0 &&
			std::chrono::duration<float>(a_now - state.lastTry).count() < 0.4f) {
			return;  // не спамим граф
		}
		if (a_actor->IsAttacking() || !a_actor->Is3DLoaded()) {
			return;  // не перебиваем начатую атаку
		}
		// Пока оружие и щит не вынуты, тот же bashStart граф играет кулаком.
		const auto* actorState = a_actor->AsActorState();
		if (!actorState || !actorState->IsWeaponDrawn()) {
			if (g_rejectLogged.fetch_add(1, std::memory_order_relaxed) < 5) {
				logs::info("skip: actor={} оружие/щит не вынуты, баш будет кулаком", a_actor->GetName());
			}
			return;
		}

		state.lastTry = a_now;

		const auto& names = GetBashEventNames(a_actor);
		std::string used;
		for (const auto& name : names) {
			if (a_actor->NotifyAnimationGraph(RE::BSFixedString(name.c_str()))) {
				used = name;
				break;
			}
		}

		if (used.empty()) {
			++state.fails;
			if (state.fails >= 12) {
				state.fails = 0;
				state.blockUntil = a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kCooldownSeconds));
				std::string tried;
				for (const auto& name : names) {
					tried += (tried.empty() ? "" : ", ") + name;
				}
				logs::info("bash ordered: actor={} граф 12 раз не принял ни одно имя [{}], пауза {:.0f} c",
					a_actor->GetName(),
					tried,
					kCooldownSeconds);
			}
			return;
		}

		state.fails = 0;
		state.blockUntil = a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kCooldownSeconds));
		g_bashes.fetch_add(1, std::memory_order_relaxed);

		if (std::none_of(g_watches.begin(), g_watches.end(),
				[&](const Watch& w) { return w.actor.get().get() == a_actor; })) {
			a_actor->AddAnimationGraphEventSink(&g_bashSink);
			g_watches.push_back(Watch{
				a_actor->GetHandle(),
				a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(3.0f)) });
		}

		logs::info("bash ordered: actor={} dist={:.0f} event={} sent=true (игрок блокирует без щита)",
			a_actor->GetName(),
			a_distance,
			used);
	}

	// Самодиагностика: раз в 3 с во время боя пишет, что вокруг игрока.
	void LogDiagnostics(RE::PlayerCharacter* a_player, Clock::time_point a_now)
	{
		static Clock::time_point lastDiag{};
		if (lastDiag.time_since_epoch().count() != 0 &&
			std::chrono::duration<float>(a_now - lastDiag).count() < 3.0f) {
			return;
		}
		lastDiag = a_now;

		const auto* processLists = RE::ProcessLists::GetSingleton();
		if (!processLists) {
			return;
		}

		const bool blocking = a_player->IsBlocking();
		const bool withShield = blocking && HasShield(a_player);
		int found = 0;
		int shown = 0;
		std::string line;

		for (const auto* handles : { &processLists->highActorHandles, &processLists->middleHighActorHandles }) {
			for (const auto& handle : *handles) {
				auto* actor = handle.get().get();
				if (!actor || actor == a_player) {
					continue;
				}
				const float distance = actor->GetPosition().GetDistance(a_player->GetPosition());
				if (distance > 300.0f || !actor->IsInCombat() || !actor->IsHostileToActor(a_player)) {
					continue;
				}
				++found;
				if (shown >= 3) {
					continue;
				}
				++shown;

				const auto* weapon = actor->GetEquippedObject(false);
				const auto* armo = weapon ? weapon->As<RE::TESObjectARMO>() : nullptr;
				line += std::format("\n  {} dist={:.0f} shield={} weapon={} blocking={} attacking={}",
					actor->GetName(),
					distance,
					HasShield(actor),
					(weapon && !armo) ? weapon->GetName() : (armo && armo->IsShield() ? "<щит>" : "?"),
					actor->IsBlocking(),
					actor->IsAttacking());
			}
		}

		logs::info("diag: blocking={} withShield={} hostile={}{}",
			blocking,
			withShield,
			found,
			line);
	}

	void ConsiderActor(RE::Actor* a_actor, RE::PlayerCharacter* a_player, bool a_playerBlocksWithShield, Clock::time_point a_now)
	{
		if (!a_actor || a_actor == a_player) {
			return;
		}
		if (!a_actor->IsInCombat() || !a_actor->IsHostileToActor(a_player)) {
			return;
		}

		const float distance = a_actor->GetPosition().GetDistance(a_player->GetPosition());
		if (distance > kMeleeRadius) {
			return;
		}

		if (a_playerBlocksWithShield) {
			return;  // щит пробивает только настоящая силовая атака — её ловит HitSink
		}

		if (HasShield(a_actor)) {
			OrderBash(a_actor, distance, a_now);
		} else if (g_rejectLogged.fetch_add(1, std::memory_order_relaxed) < 5) {
			logs::info("skip: actor={} без щита, башем не выбивает", a_actor->GetName());
		}
	}

	// --- Papyrus API (скрипт TGCP_Native) ---
	bool PapyrusIsBlocking(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		return a_actor ? a_actor->IsBlocking() : false;
	}

	bool PapyrusIsBlockingWithShield(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		return a_actor && a_actor->IsBlocking() && HasShield(a_actor);
	}

	void PapyrusTick(RE::StaticFunctionTag*)
	{
		g_ticks.fetch_add(1, std::memory_order_relaxed);

		const auto now = Clock::now();
		CloseExpiredWatches(now);
		ProcessBlockProbes(now);
		ProcessPendingFires(now);
		ProcessDisorient(now);

		if (!g_staggerSearched) {
			InitStaggerSpell();
		}

		if (!g_enabled.load(std::memory_order_relaxed)) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		if (player->IsInCombat()) {
			LogDiagnostics(player, now);
		}

		if (!player->IsBlocking()) {
			return;
		}

		const auto* processLists = RE::ProcessLists::GetSingleton();
		if (!processLists) {
			return;
		}

		const bool playerBlocksWithShield = HasShield(player);
		for (const auto* handles : { &processLists->highActorHandles, &processLists->middleHighActorHandles }) {
			for (const auto& handle : *handles) {
				ConsiderActor(handle.get().get(), player, playerBlocksWithShield, now);
			}
		}
	}

	void PapyrusSetEnabled(RE::StaticFunctionTag*, bool a_enabled)
	{
		g_enabled.store(a_enabled, std::memory_order_relaxed);
		logs::info("enabled={}", a_enabled);
	}

	std::string PapyrusGetStats(RE::StaticFunctionTag*)
	{
		return std::format("ticks={} bashes={} staggers={} probes={} bashReach={:.0f} staggerSpell={} enabled={}",
			g_ticks.load(std::memory_order_relaxed),
			g_bashes.load(std::memory_order_relaxed),
			g_staggers.load(std::memory_order_relaxed),
			g_blockProbes.size(),
			g_bashReach,
			g_staggerSpell ? g_staggerSpell->GetName() : "<нет>",
			g_enabled.load(std::memory_order_relaxed));
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("IsBlocking"sv, "TGCP_Native"sv, PapyrusIsBlocking, false);
		a_vm->RegisterFunction("IsBlockingWithShield"sv, "TGCP_Native"sv, PapyrusIsBlockingWithShield, false);
		a_vm->RegisterFunction("Tick"sv, "TGCP_Native"sv, PapyrusTick, false);
		a_vm->RegisterFunction("SetEnabled"sv, "TGCP_Native"sv, PapyrusSetEnabled, false);
		a_vm->RegisterFunction("GetStats"sv, "TGCP_Native"sv, PapyrusGetStats, false);
		logs::info("Papyrus functions registered: TGCP_Native IsBlocking / IsBlockingWithShield / Tick / SetEnabled / GetStats"sv);
		return true;
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);

	const auto version = a_skse->RuntimeVersion();
	logs::info("TGCombatPort native loaded, runtime {}.{}.{}.{} (без хуков)"sv,
		version.major(),
		version.minor(),
		version.patch(),
		version.build());

	ReadEngineSettings();
	LoadRules();

	if (const auto papyrus = SKSE::GetPapyrusInterface(); papyrus) {
		papyrus->Register(RegisterPapyrusFunctions);
	} else {
		logs::error("Papyrus interface недоступен"sv);
	}

	if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton(); holder) {
		holder->AddEventSink(&g_hitSink);
		logs::info("hit sink registered (TESHitEvent)"sv);
	} else {
		logs::error("ScriptEventSourceHolder недоступен, попадания не отслеживаются"sv);
	}

	return true;
}
