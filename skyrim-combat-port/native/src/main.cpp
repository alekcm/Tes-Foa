// TGCombatPort — SKSE-плагин (DLL).
//
// Задача: реализовать механику бандита со щитом нативно, без анимационных хаков.
//
// Что делает:
//   1. Точное состояние блока актёра: TGCP_Native.IsBlocking(Actor) — читает
//      RE::Actor::IsBlocking() (то же состояние, что видит движок), а не события анимации.
//   2. Управляет решением AI о блокирующем ударе (shield bash). В движке выбор атаки идёт
//      через Actor::GetAttackChance(target, attackData); плагин перехватывает эту функцию
//      и поднимает шанс bash-атаки, когда игрок держит блок, бандит со щитом в бою с игроком
//      и дистанция ближнего боя. Анимацию и попадание при этом считает сама игра —
//      состояние анимации не ломается.
//   3. Кулдаун 7 с на актёра (по дизайну: «bash cooldown 7 s»).
//   4. Пишет диагностику в Documents\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log.
//
// Значения (boost, радиус, кулдаун) собраны в начале файла — их правят по результатам теста.

#include "pch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace
{
	using namespace std::literals;

	// --- параметры механики (из docs/skyrim_enemy_combat_design_spec.md) ---
	constexpr float kBashBoostChance = 10.0f;     // во сколько «весит» bash, пока игрок блокирует
	constexpr float kMeleeRadius = 240.0f;        // ближняя дистанция, игровые единицы
	constexpr float kBashCooldownSeconds = 7.0f;  // «bash cooldown 7 s»
	constexpr std::uint64_t kStatsEveryCalls = 4000;

	std::atomic_bool g_enabled{ true };
	std::atomic<std::uint64_t> g_calls{ 0 };
	std::atomic<std::uint64_t> g_bashCandidates{ 0 };
	std::atomic<std::uint64_t> g_boosts{ 0 };
	std::atomic<std::uint64_t> g_bashSeenLogged{ 0 };

	std::mutex g_cooldownLock;
	std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> g_lastBoost;

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

	// --- перехват Actor::GetAttackChance(Actor* target, BGSAttackData*) ---
	using GetAttackChance_t = float (*)(RE::Actor*, RE::Actor*, RE::BGSAttackData*);
	REL::Relocation<GetAttackChance_t> g_getAttackChance;

	float GetAttackChanceHook(RE::Actor* a_self, RE::Actor* a_target, RE::BGSAttackData* a_attackData)
	{
		const float original = g_getAttackChance(a_self, a_target, a_attackData);
		const auto callIndex = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;

		if (callIndex % kStatsEveryCalls == 0) {
			logs::info("hook stats: calls={} bash={} boosts={} enabled={}",
				callIndex,
				g_bashCandidates.load(std::memory_order_relaxed),
				g_boosts.load(std::memory_order_relaxed),
				g_enabled.load(std::memory_order_relaxed));
		}

		if (!g_enabled.load(std::memory_order_relaxed) || !a_self || !a_attackData) {
			return original;
		}

		const auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player || a_self == player) {
			return original;
		}
		if (!a_attackData->data.flags.any(RE::AttackData::AttackFlag::kBashAttack)) {
			return original;
		}

		const bool playerBlocking = player->IsBlocking();

		// Первые несколько bash-кандидатов пишем целиком: по ним видно, что хук вообще работает.
		if (g_bashSeenLogged.fetch_add(1, std::memory_order_relaxed) < 10) {
			logs::info("bash candidate: actor={} chance={:.2f} playerBlocking={} dist={:.0f}",
				a_self->GetName(),
				original,
				playerBlocking,
				a_self->GetPosition().GetDistance(player->GetPosition()));
		}

		if (!playerBlocking) {
			return original;
		}
		if (!a_self->IsInCombat() || !a_self->IsHostileToActor(const_cast<RE::PlayerCharacter*>(player))) {
			return original;
		}
		if (!HasShield(a_self)) {
			return original;
		}

		const float distance = a_self->GetPosition().GetDistance(player->GetPosition());
		if (distance > kMeleeRadius) {
			return original;
		}

		g_bashCandidates.fetch_add(1, std::memory_order_relaxed);

		{
			std::lock_guard lock(g_cooldownLock);
			const auto now = std::chrono::steady_clock::now();
			auto& last = g_lastBoost[a_self->GetFormID()];
			if (last.time_since_epoch().count() != 0) {
				const auto elapsed = std::chrono::duration<float>(now - last).count();
				if (elapsed < kBashCooldownSeconds) {
					return original;
				}
			}
			last = now;
		}

		g_boosts.fetch_add(1, std::memory_order_relaxed);
		logs::info("bash boost: actor={} chance {:.2f} -> {:.2f} dist={:.0f}",
			a_self->GetName(),
			original,
			kBashBoostChance,
			distance);

		return std::max(original, kBashBoostChance);
	}

	void InstallHook()
	{
		auto& trampoline = SKSE::GetTrampoline();

		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(49748, 50675) };  // Actor::GetAttackChance
		g_getAttackChance = trampoline.write_branch<5>(target.address(), reinterpret_cast<std::uintptr_t>(&GetAttackChanceHook));

		logs::info("hook installed: Actor::GetAttackChance at {:#x}", target.address());
	}

	// --- Papyrus API (скрипт TGCP_Native) ---
	bool PapyrusIsBlocking(RE::StaticFunctionTag*, RE::Actor* a_actor)
	{
		return a_actor ? a_actor->IsBlocking() : false;
	}

	void PapyrusSetEnabled(RE::StaticFunctionTag*, bool a_enabled)
	{
		g_enabled.store(a_enabled, std::memory_order_relaxed);
		logs::info("enabled={}", a_enabled);
	}

	std::string PapyrusGetStats(RE::StaticFunctionTag*)
	{
		return std::format("calls={} bash={} boosts={} enabled={}",
			g_calls.load(std::memory_order_relaxed),
			g_bashCandidates.load(std::memory_order_relaxed),
			g_boosts.load(std::memory_order_relaxed),
			g_enabled.load(std::memory_order_relaxed));
	}

	bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("IsBlocking"sv, "TGCP_Native"sv, PapyrusIsBlocking, false);
		a_vm->RegisterFunction("SetEnabled"sv, "TGCP_Native"sv, PapyrusSetEnabled, false);
		a_vm->RegisterFunction("GetStats"sv, "TGCP_Native"sv, PapyrusGetStats, false);
		logs::info("Papyrus functions registered: TGCP_Native.IsBlocking / SetEnabled / GetStats"sv);
		return true;
	}
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .trampoline = true, .trampolineSize = 128 });

	const auto version = a_skse->RuntimeVersion();
	logs::info("TGCombatPort native loaded, runtime {}.{}.{}.{}"sv,
		version.major(),
		version.minor(),
		version.patch(),
		version.build());

	if (const auto papyrus = SKSE::GetPapyrusInterface(); papyrus) {
		papyrus->Register(RegisterPapyrusFunctions);
	} else {
		logs::error("Papyrus interface недоступен"sv);
	}

	try {
		InstallHook();
	} catch (const std::exception& e) {
		logs::error("hook failed: {}", e.what());
	} catch (...) {
		logs::error("hook failed: unknown error");
	}

	return true;
}
