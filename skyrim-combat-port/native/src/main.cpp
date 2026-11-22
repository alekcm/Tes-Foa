// TGCombatPort — SKSE-плагин (DLL).
//
// Механика бандита со щитом: «bash только против блока».
//
// Важно: в этой версии НЕТ перехвата функций игры (hook). Прошлая версия 0.1.1
// патчила Actor::GetAttackChance и вызывала вылет; такой способ убран полностью.
//
// Как работает теперь:
//   1. Скрипт TGCP_PlayerBlockWatcher, пока игрок держит блок, каждые 0.25 с зовёт
//      TGCP_Native.Tick().
//   2. Tick() ищет бандитов со щитом, которые враждебны игроку и стоят в ближнем бою,
//      и на 1,5 с поднимает в их CombatStyle значение bashMult (множитель «бить щитом»).
//      Это ровно те числа, которые AI использует при выборе атаки, — сам бой, урон,
//      блок и анимации остаются ванильными.
//   3. Через 1,5 с исходное значение возвращается. Повторно для того же бандита —
//      не раньше 7 с (кулдаун из дизайна).
//   4. Всё пишется в Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log.
//
// Побочный эффект, о котором надо знать: CombatStyle — общая запись, поэтому
// подъём действует на всех, кто этот стиль использует, но только 1,5 с и только
// пока игрок блокирует.

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

	// --- параметры механики (из docs/skyrim_enemy_combat_design_spec.md) ---
	constexpr float kMeleeRadius = 240.0f;        // ближняя дистанция, игровые единицы
	constexpr float kBashMultBoost = 10.0f;       // во что превращается bashMult на время окна
	constexpr float kBashWindowSeconds = 1.5f;    // сколько держится окно
	constexpr float kBashCooldownSeconds = 7.0f;  // «bash cooldown 7 s»

	std::atomic_bool g_enabled{ true };
	std::atomic<std::uint64_t> g_ticks{ 0 };
	std::atomic<std::uint64_t> g_windows{ 0 };
	std::atomic<std::uint64_t> g_candidates{ 0 };
	std::atomic<std::uint64_t> g_closeLogged{ 0 };

	struct Window
	{
		RE::TESCombatStyle* style;
		float               oldBashMult;
		Clock::time_point   until;
		RE::FormID          actor;
	};

	std::vector<Window> g_openWindows;
	std::unordered_map<RE::FormID, Clock::time_point> g_lastWindow;
	std::unordered_map<RE::FormID, std::uint64_t> g_rejectLogged;

	bool HasShield(RE::Actor* a_actor)
	{
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

	RE::TESCombatStyle* GetCombatStyle(RE::Actor* a_actor)
	{
		auto* base = a_actor->GetActorBase();
		return base ? base->GetCombatStyle() : nullptr;
	}

	bool HasOpenWindow(RE::FormID a_actor)
	{
		return std::any_of(g_openWindows.begin(), g_openWindows.end(),
			[a_actor](const Window& w) { return w.actor == a_actor; });
	}

	void CloseExpiredWindows(Clock::time_point a_now)
	{
		for (auto it = g_openWindows.begin(); it != g_openWindows.end();) {
			if (a_now < it->until) {
				++it;
				continue;
			}
			it->style->meleeData.bashMult = it->oldBashMult;
			if (g_closeLogged.fetch_add(1, std::memory_order_relaxed) < 20) {
				logs::info("bash window closed: style={:08X} bashMult -> {:.2f}",
					it->style->GetFormID(),
					it->oldBashMult);
			}
			it = g_openWindows.erase(it);
		}
	}

	void ConsiderActor(RE::Actor* a_actor, const RE::PlayerCharacter* a_player, Clock::time_point a_now)
	{
		if (!a_actor || a_actor == a_player) {
			return;
		}
		if (!a_actor->IsInCombat() || !a_actor->IsHostileToActor(const_cast<RE::PlayerCharacter*>(a_player))) {
			return;
		}
		if (!HasShield(a_actor)) {
			return;
		}

		const float distance = a_actor->GetPosition().GetDistance(a_player->GetPosition());
		if (distance > kMeleeRadius) {
			return;
		}

		g_candidates.fetch_add(1, std::memory_order_relaxed);

		const auto actorID = a_actor->GetFormID();
		if (HasOpenWindow(actorID)) {
			return;
		}

		const auto last = g_lastWindow.find(actorID);
		if (last != g_lastWindow.end()) {
			const auto elapsed = std::chrono::duration<float>(a_now - last->second).count();
			if (elapsed < kBashCooldownSeconds) {
				return;
			}
		}

		auto* style = GetCombatStyle(a_actor);
		if (!style) {
			if (g_rejectLogged[actorID]++ < 3) {
				logs::info("skip: actor={} без CombatStyle", a_actor->GetName());
			}
			return;
		}

		// Не трогаем стиль, если окно для него уже открыто другим бандитом.
		const bool styleBusy = std::any_of(g_openWindows.begin(), g_openWindows.end(),
			[style](const Window& w) { return w.style == style; });
		if (styleBusy) {
			return;
		}

		Window window{ style, style->meleeData.bashMult, a_now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(kBashWindowSeconds)), actorID };
		style->meleeData.bashMult = kBashMultBoost;
		g_openWindows.push_back(window);
		g_lastWindow[actorID] = a_now;
		g_windows.fetch_add(1, std::memory_order_relaxed);

		logs::info("bash window opened: actor={} style={:08X} bashMult {:.2f} -> {:.2f} dist={:.0f}",
			a_actor->GetName(),
			style->GetFormID(),
			window.oldBashMult,
			kBashMultBoost,
			distance);
	}

	// --- Papyrus API (скрипт TGCP_Native) ---
	bool PapyrusIsBlocking(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		return a_actor ? a_actor->IsBlocking() : false;
	}

	// Вызывается из Papyrus, пока игрок держит блок.
	void PapyrusTick(RE::StaticFunctionTag*)
	{
		g_ticks.fetch_add(1, std::memory_order_relaxed);

		const auto now = Clock::now();
		CloseExpiredWindows(now);

		if (!g_enabled.load(std::memory_order_relaxed)) {
			return;
		}

		const auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || !player->IsBlocking()) {
			return;
		}

		const auto* processLists = RE::ProcessLists::GetSingleton();
		if (!processLists) {
			return;
		}

		for (const auto* handles : { &processLists->highActorHandles, &processLists->middleHighActorHandles }) {
			for (const auto& handle : *handles) {
				ConsiderActor(handle.get().get(), player, now);
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
		return std::format("ticks={} candidates={} windows={} open={} enabled={}",
			g_ticks.load(std::memory_order_relaxed),
			g_candidates.load(std::memory_order_relaxed),
			g_windows.load(std::memory_order_relaxed),
			g_openWindows.size(),
			g_enabled.load(std::memory_order_relaxed));
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("IsBlocking"sv, "TGCP_Native"sv, PapyrusIsBlocking, false);
		a_vm->RegisterFunction("Tick"sv, "TGCP_Native"sv, PapyrusTick, false);
		a_vm->RegisterFunction("SetEnabled"sv, "TGCP_Native"sv, PapyrusSetEnabled, false);
		a_vm->RegisterFunction("GetStats"sv, "TGCP_Native"sv, PapyrusGetStats, false);
		logs::info("Papyrus functions registered: TGCP_Native IsBlocking / Tick / SetEnabled / GetStats"sv);
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
