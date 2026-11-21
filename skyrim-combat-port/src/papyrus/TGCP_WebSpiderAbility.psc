; TGCP_WebSpiderAbility — кулдаун и условия применения паутины большого паука.
;
; Статус: исходник написан, но НЕ компилировался настоящим PapyrusCompiler и не проверялся
; в игре. Первый прогон компилятора может потребовать правок синтаксиса/имён функций.
;
; Что делает:
;   - висит способностью (Ability) на большом пауке;
;   - раз в секунду проверяет, есть ли боевая цель в радиусе и в прямой видимости;
;   - не чаще CooldownSeconds кастует WebSpell по цели;
;   - не кастует, если цель мертва или вне радиуса.
;
; Чего НЕ делает: не управляет leap. Условие «leap только если цель близко или замедлена»
; относится к решению combat AI и этим скриптом не реализуется — см. docs/papyrus_api.md.
;
; Настройка в CK:
;   1. MagicEffect (Ability, скрытый) с этим скриптом.
;   2. Spell типа Ability, куда входит этот эффект.
;   3. Выдать способность только большому пауку (не всем паукам).
;   4. В свойствах указать WebSpell (заклинание паутины) и при необходимости изменить
;      CooldownSeconds / MaxRange.

ScriptName TGCP_WebSpiderAbility extends ActiveMagicEffect

Spell Property WebSpell Auto
Float Property CooldownSeconds = 8.0 Auto
Float Property MaxRange = 1200.0 Auto
Bool Property RequireLineOfSight = True Auto
Float Property CheckIntervalSeconds = 1.0 Auto

Float NextCastTime = 0.0

Event OnEffectStart(Actor akTarget, Actor akCaster)
    Debug.Trace("[TGCP] web ability started on " + GetTargetActor())
    RegisterForSingleUpdate(CheckIntervalSeconds)
EndEvent

Event OnEffectFinish(Actor akTarget, Actor akCaster)
    Debug.Trace("[TGCP] web ability finished on " + akTarget)
EndEvent

Event OnUpdate()
    Actor spider = GetTargetActor()
    If spider == None
        Return
    EndIf
    If spider.IsDead() == False
        TryCastWeb(spider)
    EndIf
    RegisterForSingleUpdate(CheckIntervalSeconds)
EndEvent

Function TryCastWeb(Actor spider)
    If WebSpell == None
        Debug.Trace("[TGCP] web ability: WebSpell property is empty")
        Return
    EndIf

    If Utility.GetCurrentRealTime() < NextCastTime
        Return
    EndIf

    Actor victim = spider.GetCombatTarget()
    If victim == None
        Return
    EndIf
    If victim.IsDead()
        Return
    EndIf
    If spider.GetDistance(victim) > MaxRange
        Return
    EndIf
    If RequireLineOfSight == True && spider.HasLOS(victim) == False
        Return
    EndIf

    ; DoCombatSpellApply есть в Actor.psc Skyrim. Если в игре не появится снаряд,
    ; заменить на WebSpell.Cast(spider, victim) — см. docs/papyrus_verification.md.
    spider.DoCombatSpellApply(WebSpell, victim)
    NextCastTime = Utility.GetCurrentRealTime() + CooldownSeconds
    Debug.Trace("[TGCP] web cast by " + spider + " at " + victim)
EndFunction
