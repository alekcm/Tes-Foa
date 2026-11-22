; TGCP_Native — объявления функций SKSE-плагина TGCombatPort.dll.
;
; Сами функции живут в C++ (native/). Здесь только сигнатуры: Papyrus-компилятор
; проверяет по ним типы. Если DLL не установлена, вызов такой функции в логе
; Papyrus даст ошибку "native function could not be found".

ScriptName TGCP_Native Hidden

; true, если актёр прямо сейчас блокирует (состояние движка, не событие анимации).
Bool Function IsBlocking(Actor akActor) Global Native

; Один шаг механики бандита: закрыть истёкшие окна, открыть новое окно,
; если игрок блокирует и рядом есть щитоносец. Зовётся из TGCP_PlayerBlockWatcher
; каждые 0.25 с, пока игрок держит блок.
Function Tick() Global Native

; Включить/выключить механику без перезапуска игры (для тестов A/B).
Function SetEnabled(Bool abEnabled) Global Native

; Диагностика: "ticks=… candidates=… windows=… open=… enabled=…".
String Function GetStats() Global Native
