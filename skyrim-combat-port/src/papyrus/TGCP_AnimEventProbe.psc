; TGCP_AnimEventProbe — пробник animation events.
;
; Назначение: до написания логики бандита и волков выяснить, какие имена событий
; реально приходят из игры и в каком порядке. Скрипт ничего не меняет в бою:
; он только пишет строки в Papyrus-лог.
;
; Как использовать:
;   1. Скомпилировать вместе с остальными скриптами мода (python tools/build.py).
;   2. В Creation Kit создать Quest с алиасом на игрока (или на тестового NPC),
;      повесить на алиас этот скрипт.
;   3. Включить логи Papyrus, войти в бой, подержать блок, получить bash/атаку.
;   4. Найти в Papyrus.0.log строки с префиксом [TGCP].
;
; Имена в списке — кандидаты. Если каких-то событий нет, в логе их просто не будет;
; важно увидеть реальный набор, а не подтвердить этот список.

ScriptName TGCP_AnimEventProbe extends ReferenceAlias

String[] EventNames

Event OnInit()
    EventNames = new String[12]
    EventNames[0] = "blockStart"
    EventNames[1] = "blockStop"
    EventNames[2] = "bashStart"
    EventNames[3] = "bashStop"
    EventNames[4] = "attackStart"
    EventNames[5] = "attackStop"
    EventNames[6] = "attackRelease"
    EventNames[7] = "staggerStart"
    EventNames[8] = "staggerStop"
    EventNames[9] = "JumpUp"
    EventNames[10] = "JumpDown"
    EventNames[11] = "IdleStop"

    Actor akTarget = GetActorReference()
    If akTarget == None
        Debug.Trace("[TGCP] probe: alias has no actor, nothing registered")
        Return
    EndIf

    Int i = 0
    While i < EventNames.Length
        RegisterForAnimationEvent(akTarget, EventNames[i])
        i += 1
    EndWhile

    Debug.Trace("[TGCP] probe: registered " + EventNames.Length + " candidate events on " + akTarget)
EndEvent

Event OnAnimationEvent(ObjectReference akSource, string asEventName)
    Debug.Trace("[TGCP] anim event: " + asEventName + " | source=" + akSource + " | self=" + GetActorReference())
EndEvent
