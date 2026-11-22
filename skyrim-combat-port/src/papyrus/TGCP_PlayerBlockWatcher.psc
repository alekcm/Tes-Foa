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
; Что делает дополнительно (с 0.1.3): при начале блока игрока пробует «щитовой баш» ближайшего
; противника через Debug.SendAnimationEvent(target, "bashStart"), не чаще раза в 7 секунд.
; Это приближение дизайна «щитовой финт → баш»: событие проигрывает анимацию, но AI противника
; не переписывает. У функции есть известное предупреждение: отправка animation event актёру может
; рассинхронизировать его внутреннее состояние анимации (CK Wiki, Debug.SendAnimationEvent).
; Поэтому вызов ограничен кулдауном и проверками, а в лог пишется результат каждой попытки.
;
; Финт живёт в этом скрипте временно, чтобы не требовалось заново открывать CK: отдельный скрипт
; потребовал бы добавить его на алиас вручную. Разделим, когда появится второй алиас/способность.

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

GlobalVariable Property PlayerBlocking Auto

Bool Registered

; --- параметры «финта» ---
Float LastFeintTime
Float FeintCooldown = 7.0
Float FeintRadius = 250.0

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
        TryFeint()
    ElseIf asEventName == "blockStop"
        PlayerBlocking.SetValue(0.0)
        Debug.Trace("[TGCP] player block stop (" + asEventName + ")")
    EndIf
EndEvent

; Пробует изобразить «щитовой баш» ближайшего противника. Ничего не делает, если рядом никого,
; если ближайший не дерётся с игроком, если нет линии видимости или не прошёл кулдаун.
Function TryFeint()
    Float now = Utility.GetCurrentRealTime()
    If now - LastFeintTime < FeintCooldown
        Debug.Trace("[TGCP] feint: cooldown, ещё " + (FeintCooldown - (now - LastFeintTime)) + " c")
        Return
    EndIf

    Actor player = GetActorReference()
    If player == None
        Debug.Trace("[TGCP] feint: alias has no actor")
        Return
    EndIf

    Actor target = Game.FindClosestActorFromRef(player, FeintRadius)
    If target == None
        Debug.Trace("[TGCP] feint: no actor within " + FeintRadius + " units")
        Return
    EndIf
    If target == player
        Debug.Trace("[TGCP] feint: closest actor is the player")
        Return
    EndIf
    If target.GetCombatTarget() != player
        Debug.Trace("[TGCP] feint: closest actor is not fighting the player: " + target)
        Return
    EndIf
    If !player.HasLOS(target)
        Debug.Trace("[TGCP] feint: no line of sight to " + target)
        Return
    EndIf

    LastFeintTime = now
    Debug.Trace("[TGCP] feint: sending bashStart to " + target)
    Debug.SendAnimationEvent(target, "bashStart")
    Debug.Trace("[TGCP] feint: bashStart sent")
EndFunction
