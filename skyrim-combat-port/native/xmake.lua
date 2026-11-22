-- Сборка SKSE-плагина TGCombatPort (DLL).
-- Библиотека CommonLibSSE-NG ожидается в lib/commonlibsse-ng
-- (CI клонирует её отдельно, локально — см. native/README.md).

includes("lib/commonlibsse-ng")

set_project("TGCombatPort")
set_version("0.1.0")
set_license("GPL-3.0")
set_languages("c++23")

add_rules("mode.debug", "mode.releasedbg")

target("TGCombatPort")
    add_rules("commonlibsse-ng.plugin", {
        name = "TGCombatPort",
        author = "alekcm",
        description = "Skyrim combat port: native helpers for TGCombatPort"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
