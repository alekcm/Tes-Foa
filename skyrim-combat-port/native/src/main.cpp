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

#include "pch.h"

#include <algorithm>
#include <chrono>
#include <string>
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
			if (tag == "bashStart"sv || tag == "bashStop"sv || tag == "bashRelease"sv) {
				logs::info("bandit anim: {} ({})", tag, a_event->holder ? a_event->holder->GetName() : "?");
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

		if (!g_enabled.load(std::memory_order_relaxed)) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->IsBlocking()) {
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
		return std::format("ticks={} bashes={} powerWindows={} buffs={} enabled={}",
			g_ticks.load(std::memory_order_relaxed),
			g_bashes.load(std::memory_order_relaxed),
			g_powerWindows.load(std::memory_order_relaxed),
			g_buffs.size(),
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
