; TGCP_AnimEventProbe — пробник animation events.
;
; Назначение: до написания логики бандита и волков выяснить, какие имена событий
; реально приходят из игры и в каком порядке. Скрипт ничего не меняет в бою:
; он только пишет строки в Papyrus-лог.
;
; Что уже известно из игры (22.11.2026, версия 1 списка из 12 имён):
;   пришли:     blockStop, attackStop, bashStop, IdleStop
;   НЕ пришли:  blockStart, attackStart, attackRelease, bashStart, staggerStart,
;               staggerStop, JumpUp, JumpDown
; Поэтому список расширен: добавлены варианты blockHit*, blockAnticipate*,
; blockStartOut/blockStopOut, blockStopInstant, attackStart_Attack1, preHitFrame,
; weaponSwing, bashRelease, recoilStart, weaponDraw, weaponSheathe, AttackStart.
;
; Как использовать:
;   1. Пересобрать скрипты (python tools/build.py --name TGCombatPort --version ... --skip-package)
;      и скопировать .pex в Data\Scripts. Плагин переделывать в CK не нужно: скрипт тот же.
;   2. Загрузить игру. Тест: 3 секунды держать блок, отпустить, махнуть оружием 5 раз,
;      подержать блок и нажать атаку (bash), прыгнуть.
;   3. Найти в Papyrus.0.log строки с префиксом [TGCP].
;
; IdleStop приходит десятками раз в секунду, поэтому его запись ограничена:
; не чаще одной строки в 10 секунд. Остальные события пишутся как есть.

ScriptName TGCP_AnimEventProbe extends ReferenceAlias

String[] EventNames
Bool Registered
Float LastIdleTrace

Event OnInit()
    Setup()
EndEvent

; OnInit может сработать до заполнения алиаса, поэтому дублируем попытку здесь.
Event OnAliasInit()
    Setup()
EndEvent

Function Setup()
    If Registered
        Return
    EndIf

    EventNames = new String[24]
    EventNames[0] = "blockStart"
    EventNames[1] = "blockStop"
    EventNames[2] = "blockStartOut"
    EventNames[3] = "blockStopOut"
    EventNames[4] = "blockStopInstant"
    EventNames[5] = "blockHitStart"
    EventNames[6] = "blockHitStop"
    EventNames[7] = "blockAnticipateStart"
    EventNames[8] = "blockAnticipateStop"
    EventNames[9] = "bashStart"
    EventNames[10] = "bashStop"
    EventNames[11] = "bashRelease"
    EventNames[12] = "attackStart"
    EventNames[13] = "attackStart_Attack1"
    EventNames[14] = "AttackStart"
    EventNames[15] = "attackStop"
    EventNames[16] = "attackRelease"
    EventNames[17] = "preHitFrame"
    EventNames[18] = "weaponSwing"
    EventNames[19] = "staggerStart"
    EventNames[20] = "staggerStop"
    EventNames[21] = "recoilStart"
    EventNames[22] = "weaponDraw"
    EventNames[23] = "IdleStop"

    Actor akTarget = GetActorReference()
    If akTarget == None
        Debug.Trace("[TGCP] probe: alias has no actor yet, will retry on OnAliasInit")
        Return
    EndIf

    Int i = 0
    While i < EventNames.Length
        RegisterForAnimationEvent(akTarget, EventNames[i])
        i += 1
    EndWhile

    LastIdleTrace = 0.0
    Registered = True
    Debug.Trace("[TGCP] probe: registered " + EventNames.Length + " candidate events on " + akTarget)
EndFunction

Event OnAnimationEvent(ObjectReference akSource, string asEventName)
    If asEventName == "IdleStop"
        Float now = Utility.GetCurrentRealTime()
        If now - LastIdleTrace < 10.0
            Return
        EndIf
        LastIdleTrace = now
        Debug.Trace("[TGCP] event IdleStop (записан один раз в 10 секунд)")
        Return
    EndIf

    Debug.Trace("[TGCP] event " + asEventName + " | source=" + akSource)
EndEvent
