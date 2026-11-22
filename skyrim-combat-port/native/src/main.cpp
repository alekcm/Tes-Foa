// TGCombatPort — SKSE-плагин (DLL).
//
// Задача первого шага: доказать, что DLL собирается, загружается игрой и умеет
// регистрировать Papyrus-функцию. Логика боя появится после этого.
//
// Что делает сейчас:
//   - пишет строку в свой лог при загрузке (доказательство работы в игре);
//   - регистрирует Papyrus-функцию TGCP_Native.IsBlocking(Actor) — точное чтение
//     состояния блока из графа анимаций (то, чего нет в обычном Papyrus).

#include "pch.h"

namespace
{
    // Точное состояние блока: читаем переменную графа анимаций "Isblocking".
    // Именно её использует условие IsBlocking в Creation Kit.
    bool IsBlocking(RE::StaticFunctionTag*, RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        bool blocking = false;
        if (a_actor->GetGraphVariableBool("Isblocking"sv, blocking)) {
            return blocking;
        }
        return false;
    }

    bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
    {
        a_vm->RegisterFunction("IsBlocking"sv, "TGCP_Native"sv, IsBlocking, false);
        logs::info("TGCP_Native.IsBlocking зарегистрирована"sv);
        return true;
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);

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

    return true;
}
