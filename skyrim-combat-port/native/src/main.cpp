// TGCombatPort — SKSE-плагин (DLL).
//
// Правила механики (решение пользователя 22.11.2026):
//   1. Игрок блокирует НЕ щитом (оружием/кулаками) → щитоносец-бандит выбивает его из блока
//      башем. Заказ на баш отдаёт DLL: actor->NotifyAnimationGraph("bashStart").
//   2. Игрок блокирует ЩИТОМ → башем не выбивают. Щит пробивается силовой атакой того,
//      у кого булава или двуручный молот: на 2,5 с поднимается CombatStyle.powerAttackBlockingMult
//      («вероятность силовой атаки по блокирующему»), решение принимает сам AI.
//   3. Игрок не блокирует → плагин ничего не делает.
//
// Ограничения: ближняя дистанция (≤ 200), только враждебные и уже дерущиеся NPC,
// кулдаун 7 с на каждого бандита (из дизайна), не перебиваем текущую атаку NPC.
//
// Честно про способ (1): в движке нет функции «прикажи NPC ударить щитом». Прямой заказ
// возможен только через animation event — это тот же механизм, что Debug.SendAnimationEvent,
// но вызванный из DLL, вместе с нашими проверками и кулдауном.
//
// Хуков (патчинга кода игры) в этой версии нет: 0.1.1 патчила Actor::GetAttackChance и вызывала вылет.
//
// 0.6.1 — почему баш «не выбивал»: ванильный баш блок НЕ ломает. Он снимает выносливость и даёт
// короткий ступор, но блок остаётся — ровно то, что видно в игре («бьют рукой, стаггер есть,
// из блока не вылетаю»). Выход из блока в движке даёт только эффект с архетипом Stagger
// (на время стаггера блок невозможен). Своего .esp в моде нет, поэтому берём уже загруженный
// игрой спелл со стаггером и кастуем его на игрока в момент удара щитоносца.

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

	// --- параметры (docs/skyrim_enemy_combat_design_spec.md, поправка 22.11.2026) ---
	constexpr float kMeleeRadius = 200.0f;          // дистанция баша/силовой, игровые единицы
	constexpr float kCooldownSeconds = 7.0f;        // «bash cooldown 7 s»
	constexpr float kPowerWindowSeconds = 2.5f;     // сколько держится подъём powerAttackBlockingMult
	constexpr float kPowerAttackBlockingBoost = 10.0f;

	std::atomic_bool g_enabled{ true };
	std::atomic<std::uint64_t> g_ticks{ 0 };
	std::atomic<std::uint64_t> g_bashes{ 0 };
	std::atomic<std::uint64_t> g_powerWindows{ 0 };
	std::atomic<std::uint64_t> g_rejectLogged{ 0 };

	bool HasShield(RE::Actor* a_actor);
	const RE::TESObjectWEAP* GetHeavyBlunt(RE::Actor* a_actor);
	bool IsWatchedBash(RE::Actor* a_actor);
	void OnBashLanded(RE::Actor* a_actor);
	void OnBashFinished(RE::Actor* a_actor);
	void OnPowerAttackStarted(RE::Actor* a_actor, std::string_view a_tag);

	// Слушает animation events бандита, которому только что заказали баш: подтверждение,
	// что замах реально проигрался.
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
			if (tag == "bashStart"sv) {
				OnBashLanded(actor);
			} else if (tag == "bashStop"sv) {
				// Движок не рассылает bashStart слушателям, а bashStop — рассылает.
				// Поэтому «баш проигрался» определяем по bashStop (см. лог 0.6.2).
				OnBashFinished(actor);
			} else if (tag == "bashRelease"sv) {
				logs::info("bandit anim: bashRelease ({})", ref ? ref->GetName() : "?");
			} else if (tag.starts_with("attackPowerStart"sv) || tag.starts_with("bashPowerStart"sv)) {
				OnPowerAttackStarted(actor, tag);
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	BashWatchSink g_bashSink;

	struct Watch
	{
		RE::ActorHandle   actor;
		Clock::time_point until;
	};

	std::vector<Watch> g_watches;

	struct Buff
	{
		RE::TESCombatStyle* style;
		float               oldPowerAttackBlocking;
		Clock::time_point   until;
		RE::FormID          actorID;
	};

	std::vector<Buff> g_buffs;
	// Попытки заказа баша: одна попытка не чаще 0,4 с, кулдаун 7 с — только после успеха.
	struct AttemptState
	{
		Clock::time_point lastTry{};
		int               fails{ 0 };
		Clock::time_point blockUntil{};
	};

	std::unordered_map<RE::FormID, AttemptState> g_attempts;
	std::unordered_map<RE::FormID, std::vector<std::string>> g_bashEventsByRace;
	std::atomic<std::uint64_t> g_dumpedRaces{ 0 };
	std::unordered_map<RE::FormID, Clock::time_point> g_lastPower;

	// --- 0.7.0: выход из блока делает сам плагин, а не движок ---
	//
	// Что показал лог 0.6.2 (факты, а не догадки):
	//   * bash ordered … sent=true ×5, но от графа пришёл только bashStop — bashStart
	//     слушателям не рассылается, поэтому наш обработчик не вызывался ни разу;
	//   * заказ баша проигрывает анимацию, но попадания нет: движок считает попадание
	//     по своей атаке, а animation event — это только анимация. «Бьёт щитом и не попадает»
	//     поэтому не баг, а природа механизма;
	//   * ванильная силовая атака блок тоже не ломает.
	//
	// Значит эффект надо накладывать самим, в момент, который виден в логе: bashStop.
	// Накладываем ванильный спелл с архетипом Stagger (пока актёр стаггерится, блок невозможен)
	// и сразу же измеряем результат: IsStaggering() и IsBlocking() через равные интервалы.
	// 0..1 по CK Wiki. 0.5 («стоячий» стаггер) на игрока по отзывам моддеров не срабатывает,
	// 1.0 — гарантированно видимый (на колено, ~1.5 с). Ставим 1.0.
	constexpr float kStaggerMagnitude = 1.0f;
	constexpr float kStaggerReach = 140.0f;    // ближе этого — удар выглядит попавшим
	constexpr float kStaggerCooldown = 4.0f;
	constexpr float kPowerReach = 160.0f;

	RE::SpellItem* g_staggerSpell{ nullptr };
	bool           g_staggerSearched{ false };
	std::atomic<std::uint64_t> g_staggers{ 0 };
	Clock::time_point g_lastStagger{};

	// Пробы «вышел ли игрок из блока» после наложенного стаггера.
	struct BlockProbe
	{
		Clock::time_point at;
		std::string       source;
	};
	std::vector<BlockProbe> g_blockProbes;

	// NPC с булавой/двуручником, за чьими силовыми атаками следим (правило 2).
	struct PowerWatch
	{
		RE::ActorHandle   actor;
		Clock::time_point until;
	};
	std::vector<PowerWatch> g_powerWatches;

	bool IsWatchedBash(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}
		return std::any_of(g_watches.begin(), g_watches.end(),
			[a_actor](const Watch& w) { return w.actor.get().get() == a_actor; });
	}

	// Ищем уже загруженный игрой спелл с эффектом архетипа Stagger.
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
		int            bestEffects = 100000;
		RE::SpellItem* best = nullptr;

		for (auto* spell : handler->GetFormArray<RE::SpellItem>()) {
			if (!spell || !spell->HasEffect(RE::EffectArchetype::kStagger)) {
				continue;
			}
			++total;

			const auto    effectCount = static_cast<int>(spell->effects.size());
			std::uint32_t duration = 0;
			float         magnitude = 0.0f;
			for (auto* effect : spell->effects) {
				if (effect && effect->baseEffect && effect->baseEffect->HasArchetype(RE::EffectArchetype::kStagger)) {
					duration = effect->effectItem.duration;
					magnitude = effect->effectItem.magnitude;
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

			const bool plainSpell = spell->GetSpellType() == RE::MagicSystem::SpellType::kSpell;
			const bool fireAndForget = spell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget;
			if (plainSpell && fireAndForget && effectCount < bestEffects) {
				bestEffects = effectCount;
				best = spell;
			}
		}

		g_staggerSpell = best;
		if (best) {
			logs::info("stagger: выбран спелл {} ({:08X}) из {} кандидатов, эффектов {}, magnitude={:.2f}",
				best->GetName(),
				best->GetFormID(),
				total,
				bestEffects,
				kStaggerMagnitude);
		} else {
			logs::info("stagger: подходящего спелла нет (кандидатов {}) — блок ломать нечем", total);
		}
	}

	// Накладываем стаггер на игрока от имени NPC: caster — сам NPC, target — игрок.
	// Допущения: AS-1 (стаггер снимает блок у игрока), AS-2 (magnitude 1.0 даёт видимый стаггер),
	// AS-4 (CastSpellImmediate не требует анимации каста).
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
	}

	bool StaggerOnCooldown(Clock::time_point a_now)
	{
		return g_lastStagger.time_since_epoch().count() != 0 &&
			std::chrono::duration<float>(a_now - g_lastStagger).count() < kStaggerCooldown;
	}

	// Пробы: печатаем, что стало с игроком после стаггера. Допущение AS-7 (IsStaggering у игрока). Это и есть проверка механики —
	// она сама пишет в лог, вышел ли игрок из блока.
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
			logs::info("block probe: {} blocking={} staggered={}",
				it->source,
				player->IsBlocking(),
				player->IsStaggering());
			it = g_blockProbes.erase(it);
		}
	}

	// Правило 1: щитоносец закончил замах баша.
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

		if (!withShield || !playerBlocking || playerShield || distance > kStaggerReach) {
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

		ApplyStagger(a_actor, "баш");
	}

	// bashStart слушателям не приходит (см. лог 0.6.2), оставляем только для истории.
	void OnBashLanded(RE::Actor* a_actor)
	{
		if (!a_actor || !IsWatchedBash(a_actor)) {
			return;
		}
		logs::info("bandit anim: bashStart actor={} shield={}", a_actor->GetName(), HasShield(a_actor));
	}

	// Правило 2: силовая атака булавы/двуручника по блокирующему щитом игроку.
	// Допущение AS-5 (события attackPowerStart* доходят до слушателя).
	void OnPowerAttackStarted(RE::Actor* a_actor, std::string_view a_tag)
	{
		if (!a_actor) {
			return;
		}
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->IsBlocking() || !HasShield(player)) {
			return;
		}
		if (!a_actor->IsInCombat() || !a_actor->IsHostileToActor(player)) {
			return;
		}
		const float distance = a_actor->GetPosition().GetDistance(player->GetPosition());
		if (distance > kPowerReach) {
			return;
		}
		if (!GetHeavyBlunt(a_actor)) {
			logs::info("power attack: actor={} {} — не булава/двуручник, пропуск", a_actor->GetName(), a_tag);
			return;
		}
		if (StaggerOnCooldown(Clock::now())) {
			return;
		}

		logs::info("power attack start: actor={} event={} dist={:.0f}", a_actor->GetName(), a_tag, distance);
		ApplyStagger(a_actor, "силовая атака");
	}

	// Слежка за силовыми атаками NPC с булавой/двуручником, пока игрок блокирует щитом.
	// Допущение AS-5.
	void RefreshPowerWatches(Clock::time_point a_now)
	{
		for (auto it = g_powerWatches.begin(); it != g_powerWatches.end();) {
			if (a_now < it->until) {
				++it;
				continue;
			}
			if (auto actor = it->actor.get().get()) {
				actor->RemoveAnimationGraphEventSink(&g_bashSink);
			}
			it = g_powerWatches.erase(it);
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* processLists = RE::ProcessLists::GetSingleton();
		if (!player || !processLists || !player->IsBlocking() || !HasShield(player)) {
			return;
		}

		for (const auto* handles : { &processLists->highActorHandles, &processLists->middleHighActorHandles }) {
			for (const auto& handle : *handles) {
				auto* actor = handle.get().get();
				if (!actor || actor == player || !actor->IsInCombat() || !actor->IsHostileToActor(player)) {
					continue;
				}
				if (actor->GetPosition().GetDistance(player->GetPosition()) > kPowerReach + 60.0f) {
					continue;
				}
				if (!GetHeavyBlunt(actor)) {
					continue;
				}
				if (std::any_of(g_powerWatches.begin(), g_powerWatches.end(),
						[actor](const PowerWatch& w) { return w.actor.get().get() == actor; })) {
					continue;
				}
				actor->AddAnimationGraphEventSink(&g_bashSink);
				g_powerWatches.push_back(PowerWatch{ actor->GetHandle(), a_now + std::chrono::seconds(5) });
				logs::info("power watch: следим за силовыми атаками {} ({})", actor->GetName(), actor->GetFormID());
			}
		}
	}
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

	// Булава или двуручный молот (в движке это типы kOneHandMace и kTwoHandAxe;
	// к kTwoHandAxe относится и двуручный топор — их движок не различает по типу).
	const RE::TESObjectWEAP* GetHeavyBlunt(RE::Actor* a_actor)
	{
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

	// Имена attack-событий, помеченных в данных расы как bash. Именно эти строки понимает
	// граф анимаций актёра; "bashStart" добавлен как запасной вариант.
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

	RE::TESCombatStyle* GetCombatStyle(RE::Actor* a_actor)
	{
		auto* base = a_actor->GetActorBase();
		return base ? base->GetCombatStyle() : nullptr;
	}

	bool CooldownPassed(std::unordered_map<RE::FormID, Clock::time_point>& a_map, RE::FormID a_actor, Clock::time_point a_now)
	{
		const auto it = a_map.find(a_actor);
		if (it == a_map.end()) {
			return true;
		}
		return std::chrono::duration<float>(a_now - it->second).count() >= kCooldownSeconds;
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

	void CloseExpiredBuffs(Clock::time_point a_now)
	{
		for (auto it = g_buffs.begin(); it != g_buffs.end();) {
			if (a_now < it->until) {
				++it;
				continue;
			}
			it->style->meleeData.powerAttackBlockingMult = it->oldPowerAttackBlocking;
			logs::info("power window closed: style={:08X} powAtkBlock -> {:.2f}",
				it->style->GetFormID(),
				it->oldPowerAttackBlocking);
			it = g_buffs.erase(it);
		}
	}

	// Правило 1: выбить игрока из блока, если он блокирует не щитом.
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
		// Щит должен быть в руке: пока оружие/щит не вынуты, тот же bashStart граф играет
		// кулаком, и баш «рукой» — это ровно пропущенная проверка, а не другой баш.
		// Допущение AS-6 (семантика ActorState::IsWeaponDrawn для «щит в руке»).
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
			g_watches.push_back(Watch{ a_actor->GetHandle(), a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(3.0f)) });
		}

		logs::info("bash ordered: actor={} dist={:.0f} event={} sent=true (игрок блокирует без щита)",
			a_actor->GetName(),
			a_distance,
			used);
	}

	// Правило 2: щит пробивается силовой атакой булавы/двуручного молота.
	void OpenPowerWindow(RE::Actor* a_actor, const RE::TESObjectWEAP* a_weapon, float a_distance, Clock::time_point a_now)
	{
		const auto actorID = a_actor->GetFormID();
		if (!CooldownPassed(g_lastPower, actorID, a_now)) {
			return;
		}
		if (std::any_of(g_buffs.begin(), g_buffs.end(), [actorID](const Buff& b) { return b.actorID == actorID; })) {
			return;
		}

		auto* style = GetCombatStyle(a_actor);
		if (!style) {
			return;
		}
		if (std::any_of(g_buffs.begin(), g_buffs.end(), [style](const Buff& b) { return b.style == style; })) {
			return;
		}

		auto& melee = style->meleeData;
		Buff buff{};
		buff.style = style;
		buff.oldPowerAttackBlocking = melee.powerAttackBlockingMult;
		buff.until = a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kPowerWindowSeconds));
		buff.actorID = actorID;

		melee.powerAttackBlockingMult = kPowerAttackBlockingBoost;
		g_buffs.push_back(buff);
		g_lastPower[actorID] = a_now;
		g_powerWindows.fetch_add(1, std::memory_order_relaxed);

		logs::info("power window opened: actor={} weapon={} style={:08X} powAtkBlock {:.2f} -> {:.2f} dist={:.0f} (игрок блокирует щитом)",
			a_actor->GetName(),
			a_weapon->GetName(),
			style->GetFormID(),
			buff.oldPowerAttackBlocking,
			kPowerAttackBlockingBoost,
			a_distance);
	}

	// Самодиагностика: раз в 3 с во время боя пишет, что вокруг игрока.
	// Нужна, чтобы не устраивать «найди волка и подерись» — данных хватает из любого боя.
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

		logs::info("diag: blocking={} withShield={} hostiles={}{}",
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
			if (const auto* weapon = GetHeavyBlunt(a_actor)) {
				OpenPowerWindow(a_actor, weapon, distance, a_now);
			}
			return;
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
		CloseExpiredBuffs(now);
		CloseExpiredWatches(now);

		if (!g_staggerSearched) {
			InitStaggerSpell();
		}
		ProcessBlockProbes(now);
		RefreshPowerWatches(now);

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
		return std::format("ticks={} bashes={} powerWindows={} buffs={} staggers={} staggerSpell={} enabled={}",
			g_ticks.load(std::memory_order_relaxed),
			g_bashes.load(std::memory_order_relaxed),
			g_powerWindows.load(std::memory_order_relaxed),
			g_buffs.size(),
			g_staggers.load(std::memory_order_relaxed),
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

	if (const auto papyrus = SKSE::GetPapyrusInterface(); papyrus) {
		papyrus->Register(RegisterPapyrusFunctions);
	} else {
		logs::error("Papyrus interface недоступен"sv);
	}

	return true;
}
