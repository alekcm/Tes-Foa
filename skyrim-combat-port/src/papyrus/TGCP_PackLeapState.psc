; TGCP_PackLeapState — учёт «кто из стаи сейчас в прыжке».
;
; Статус: исходник написан, но НЕ компилировался и НЕ проверялся в игре.
;
; ВАЖНО, честное ограничение: этот скрипт хранит состояние и отвечает на вопрос
; «кому сейчас разрешён leap», но сам по себе НЕ запрещает combat AI волка прыгнуть.
; Отменять или не давать начать атаку Papyrus не умеет. Поэтому слой состояния написан
; так, чтобы его можно было использовать и из Papyrus (через проверку CanLeap), и позже
; из SKSE-хука, если проверка в игре покажет, что без перехвата атаки правило
; «один leap на стаю» не держится. См. docs/papyrus_api.md и docs/vertical_slice_feasibility.md.
;
; Настройка в CK:
;   1. Quest мода (TGCP_MainQuest), на него этот скрипт.
;   2. Другие скрипты получают доступ через свойство Quest и вызов функций.

ScriptName TGCP_PackLeapState extends Quest

Float Property LeapLockSeconds = 1.5 Auto

Actor ActiveLeaper
Float LockUntil = 0.0

; Пытается занять «право на leap». Возвращает True, если волк может прыгать сейчас.
Bool Function TryAcquireLeap(Actor akWolf)
    If akWolf == None
        Return False
    EndIf

    ExpireIfNeeded()

    If ActiveLeaper != None
        Return ActiveLeaper == akWolf
    EndIf

    ActiveLeaper = akWolf
    LockUntil = Utility.GetCurrentRealTime() + LeapLockSeconds
    Debug.Trace("[TGCP] leap acquired by " + akWolf)
    Return True
EndFunction

; Освобождает право, если его держит этот волк. Чужие вызовы игнорируются.
Function ReleaseLeap(Actor akWolf)
    If akWolf == None
        Return
    EndIf
    If ActiveLeaper == akWolf
        Debug.Trace("[TGCP] leap released by " + akWolf)
        ActiveLeaper = None
        LockUntil = 0.0
    EndIf
EndFunction

; Можно ли волку начинать leap прямо сейчас. Не занимает право.
Bool Function CanLeap(Actor akWolf)
    If akWolf == None
        Return False
    EndIf
    ExpireIfNeeded()
    If ActiveLeaper == None
        Return True
    EndIf
    Return ActiveLeaper == akWolf
EndFunction

Actor Function GetActiveLeaper()
    ExpireIfNeeded()
    Return ActiveLeaper
EndFunction

Bool Function IsAnyWolfLeaping()
    ExpireIfNeeded()
    Return ActiveLeaper != None
EndFunction

Function ExpireIfNeeded()
    If ActiveLeaper == None
        Return
    EndIf
    If Utility.GetCurrentRealTime() >= LockUntil
        Debug.Trace("[TGCP] leap lock expired for " + ActiveLeaper)
        ActiveLeaper = None
        LockUntil = 0.0
    EndIf
EndFunction
