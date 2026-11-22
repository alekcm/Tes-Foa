# CK Wiki: KnockAreaEffect - ObjectReference (копия)

Источник: <https://ck.uesp.net/wiki/KnockAreaEffect_-_ObjectReference> (скопировано 2026-11-22).
Лицензия CK Wiki (UESP): CC BY-SA.

## Определение

> **Member of:** ObjectReference Script
> Executes a knock effect on actors in an area.

```
Function KnockAreaEffect(float afMagnitude, float afRadius) native
```

> - afMagnitude: The magnitude of the knock effect (from 0 to 1).
> - afRadius: The radius of knock effect from the source reference.

Пример из вики:

```
; Execute a knock area effect around the dragon.
BobTheDragon.KnockAreaEffect(1, 3000)
```

## Как это используют моддеры ([C])

Трюк «стаггер по заказу»: поставить пустой активатор у ног цели, вызвать на нём `KnockAreaEffect`
с нужной magnitude и сразу удалить объект (иначе мусор в сохранении).
<https://www.reddit.com/r/skyrimmods/comments/7uu26b/how_to_apply_stagger_to_melee_attackers/>

## Ограничение для нас

В CommonLibSSE-NG обёртки нет: `KnockAreaEffect` встречается только как функция условий
(`TESCondition`, ID 685). То есть из DLL напрямую не вызвать — только из Papyrus.
