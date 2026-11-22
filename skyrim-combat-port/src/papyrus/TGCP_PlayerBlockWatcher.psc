; TGCP_PlayerBlockWatcher — отслеживание блока игрока.
;
; Статус: исходник написан, но НЕ компилировался и НЕ проверялся в игре. Имена событий
; ("blockStart"/"blockStop") — кандидаты, их подтверждает TGCP_AnimEventProbe.
;
; Что делает:
;   - висит на алиасе игрока в квесте мода;
;   - при старте блока ставит глобальную переменную PlayerBlocking = 1, при завершении — 0.
;
; Зачем: бандит должен делать feint именно против блока. Скрипт даёт дешёвый признак
; «игрок сейчас блокирует», который может читать любой другой скрипт мода, не подписываясь
; на события повторно.
;
; Ответное действие бандита («щитовой финт → баш») делает SKSE-плагин TGCombatPort.dll.
; Пока игрок держит блок, скрипт каждые 0.25 с зовёт TGCP_Native.Tick(): плагин поднимает
; бандитам со щитом множитель bash в их CombatStyle на 1,5 с, с кулдауном 7 с.
; Скрипт — только «часы» и признак блока; вся логика в DLL (см. native/README.md).

;
; Настройка в CK:
;   1. Quest мода, алиас на PlayerRef, этот скрипт на алиасе.
;   2. Создать GlobalVariable (Short) с EditorID TGCP_PlayerBlocking и указать её в свойстве.
;
; Имена событий проверены в игре 22.11.2026 (см. docs/papyrus_verification.md):
;   blockStart    — ни разу не пришёл;
;   blockStartOut — пришёл ровно при начале блока (опережает blockStop на длительность блока);
;   blockStop     — пришёл при завершении блока.
; Поэтому началом блока считаем любой из blockStart/blockStartOut, а концом — blockStop.
; В тексте строки лога пишется реальное имя события, чтобы было видно, что сработало.

ScriptName TGCP_PlayerBlockWatcher extends ReferenceAlias

Import TGCP_Native

GlobalVariable Property PlayerBlocking Auto

; Как часто звать TGCP_Native.Tick(), пока игрок держит блок.
Float TickInterval = 0.25

Bool Registered

Event OnInit()
    Setup()
EndEvent

; OnInit может сработать до заполнения алиаса — тогда повторяем на OnAliasInit.
Event OnAliasInit()
    Setup()
EndEvent

Function Setup()
    If Registered
        Return
    EndIf

    Actor player = GetActorReference()
    If player == None
        Debug.Trace("[TGCP] block watcher: alias has no actor")
        Return
    EndIf
    If PlayerBlocking == None
        Debug.Trace("[TGCP] block watcher: PlayerBlocking global is not set")
    EndIf

    RegisterForAnimationEvent(player, "blockStart")
    RegisterForAnimationEvent(player, "blockStartOut")
    RegisterForAnimationEvent(player, "blockStop")
    Registered = True
    Debug.Trace("[TGCP] block watcher registered on " + player)
EndFunction

Event OnAnimationEvent(ObjectReference akSource, string asEventName)
    If PlayerBlocking == None
        Return
    EndIf

    If asEventName == "blockStart" || asEventName == "blockStartOut"
        PlayerBlocking.SetValue(1.0)
        Debug.Trace("[TGCP] player block start (" + asEventName + ")")
        RegisterForSingleUpdate(TickInterval)
    ElseIf asEventName == "blockStop"
        PlayerBlocking.SetValue(0.0)
        Debug.Trace("[TGCP] player block stop (" + asEventName + ")")
    EndIf
EndEvent

; Пока игрок блокирует — крутим Tick. Как только блок снят, делаем ещё один Tick,
; чтобы плагин закрыл открытые окна, и останавливаемся.
Event OnUpdate()
    Actor player = GetActorReference()
    If player != None && TGCP_Native.IsBlocking(player)
        TGCP_Native.Tick()
        RegisterForSingleUpdate(TickInterval)
    Else
        TGCP_Native.Tick()
    EndIf
EndEvent
