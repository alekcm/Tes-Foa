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
	void OnPlayerHit(RE::Actor* a_attacker, REX::EnumSet<RE::TESHitEvent::Flag, std::uint8_t> a_flags);

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
			OnPlayerHit(actor, a_event->flags);
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
	// Отложенный стаггер: наш баш попаданий не создаёт, поэтому эффект применяем с небольшой
	// задержкой от начала замаха, чтобы совпасть с моментом удара.
	struct PendingStagger
	{
		RE::ActorHandle   source;
		Clock::time_point at;
	};

	std::vector<PendingStagger> g_pendingStaggers;

	// Пробы «вышел ли игрок из блока» после наложенного стаггера.
	struct BlockProbe
	{
		Clock::time_point at;
		std::string       source;
	};

	std::vector<BlockProbe> g_blockProbes;

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
			bool          foundStagger = false;
			for (auto* effect : spell->effects) {
				if (effect && effect->baseEffect && effect->baseEffect->HasArchetype(RE::EffectArchetype::kStagger)) {
					duration = effect->effectItem.duration;
					magnitude = effect->effectItem.magnitude;
					foundStagger = true;
					break;
				}
			}

			if (shown < 12) {
				++shown;
				logs::info("  stagger spell: {} ({:08X}) type={} cast={} delivery={} effects={} dur={} mag={:.2f} cost={}",
					spell->GetName(),
					spell->GetFormID(),
					static_cast<int>(spell->GetSpellType()),
					static_cast<int>(spell->GetCastingType()),
					static_cast<int>(spell->GetDelivery()),
					effectCount,
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

	// Накладываем стаггер на игрока от имени NPC: caster — сам NPC, target — игрок.
	// AS-1 опровергнуто логом 0.7.1: стаггер НЕ снимает блок у игрока, поэтому блок гасим отдельно
	// (AS-10, KickBlock). Подтверждены: AS-2 (стаггер виден), AS-4 (каст без анимации).
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

		for (const float offset : { 0.15f, 0.45f, 0.9f, 1.4f }) {
			g_blockProbes.push_back(BlockProbe{
				Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(offset)),
				a_why });
		}

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
			if (blocking && staggered) {
				// Блок вернулся, пока игрок ещё в стаггере — гасим снова (AS-10).
				KickBlock(player, it->source.c_str());
			}
			it = g_blockProbes.erase(it);
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

	// Реакция на НАСТОЯЩЕЕ попадание по игроку.
	// Допущение AS-5: движок заполняет флаги так, как описано в перечислении TESHitEvent::Flag.
	void OnPlayerHit(RE::Actor* a_attacker, REX::EnumSet<RE::TESHitEvent::Flag, std::uint8_t> a_flags)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !a_attacker) {
			return;
		}

		const bool blocked = a_flags.any(RE::TESHitEvent::Flag::kHitBlocked);
		const bool power = a_flags.any(RE::TESHitEvent::Flag::kPowerAttack);
		const bool bash = a_flags.any(RE::TESHitEvent::Flag::kBashAttack);

		if (!blocked || !player->IsBlocking()) {
			return;  // правило 3: игрок не блокирует — не вмешиваемся
		}

		const bool playerShield = HasShield(player);
		const bool attackerShield = HasShield(a_attacker);
		const auto* heavyBlunt = GetHeavyBlunt(a_attacker);

		// Правило 1: баш щитоносца по игроку, который блокирует не щитом.
		if (bash && attackerShield && !playerShield) {
			if (!StaggerOnCooldown(Clock::now())) {
				ApplyStagger(a_attacker, "баш щитоносца (попадание)");
			}
			return;
		}

		// Правило 2: силовая атака булавы/двуручника по блоку щитом.
		if (power && heavyBlunt && playerShield) {
			if (!StaggerOnCooldown(Clock::now())) {
				ApplyStagger(a_attacker, "силовая по блоку щитом (попадание)");
			}
			return;
		}

		if (g_hitLogged.fetch_add(1, std::memory_order_relaxed) < 8) {
			logs::info("hit без эффекта: actor={} bash={} power={} blocked={} игрок.щит={} оружие={}",
				a_attacker->GetName(),
				bash,
				power,
				blocked,
				playerShield,
				heavyBlunt ? heavyBlunt->GetName() : "<нет>");
		}
	}

	// Правило 1, запасной путь: наш заказанный баш попаданий не создаёт, поэтому эффект
	// накладываем по окончанию его анимации (bashStop).
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

		const float distance = a_actor->GetPosition().GetDistance(player->GetPosition());
		const bool  withShield = HasShield(a_actor);
		const bool  playerBlocking = player->IsBlocking();
		const bool  playerShield = playerBlocking && HasShield(player);

		if (!withShield || !playerBlocking || playerShield || distance > g_bashReach) {
			logs::info("bash finished: actor={} dist={:.0f} shield={} игрок.блок={} игрок.щит={} → без эффекта",
				a_actor->GetName(),
				distance,
				withShield,
				playerBlocking,
				playerShield);
			return;
		}
		if (StaggerOnCooldown(Clock::now())) {
			return;
		}

		g_pendingStaggers.push_back(PendingStagger{
			a_actor->GetHandle(),
			Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kStaggerDelaySeconds)) });
	}

	// bashStart слушателям не приходит (см. лог 0.6.2), оставляем только для истории.
	void OnBashLanded(RE::Actor* a_actor)
	{
		if (!a_actor || !IsWatchedBash(a_actor)) {
			return;
		}
		logs::info("bandit anim: bashStart actor={} shield={}", a_actor->GetName(), HasShield(a_actor));
	}

	void ProcessPendingStaggers(Clock::time_point a_now)
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			g_pendingStaggers.clear();
			return;
		}

		for (auto it = g_pendingStaggers.begin(); it != g_pendingStaggers.end();) {
			if (a_now < it->at) {
				++it;
				continue;
			}
			auto* source = it->source.get().get();
			const bool stillValid = source && !source->IsDead() &&
				player->IsBlocking() && !HasShield(player) &&
				source->GetPosition().GetDistance(player->GetPosition()) <= g_bashReach;
			if (stillValid && !StaggerOnCooldown(a_now)) {
				ApplyStagger(source, "баш щитоносца (заказ)");
			}
			it = g_pendingStaggers.erase(it);
		}
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
		ProcessPendingStaggers(now);

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
