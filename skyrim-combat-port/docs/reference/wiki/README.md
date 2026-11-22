# Локальная копия источников

Здесь лежат **скопированные вручную фрагменты** первоисточников, на которые опирается код мода.
Копия сделана не для перепубликации, а чтобы проверка была возможна без интернета и без доверия
к памяти агента.

## Что и откуда

| Файл | Источник | Лицензия источника |
|---|---|---|
| [`uesp-block.md`](uesp-block.md) | <https://en.uesp.net/wiki/Skyrim:Block> | UESP: CC BY-SA 2.5 |
| [`uesp-combat.md`](uesp-combat.md) | <https://en.uesp.net/wiki/Skyrim:Combat> | UESP: CC BY-SA 2.5 |
| [`ck-magic-effect.md`](ck-magic-effect.md) | <https://ck.uesp.net/wiki/Magic_Effect> | CK Wiki (UESP), CC BY-SA |
| [`ck-combat-style.md`](ck-combat-style.md) | <https://skyrimck.uesp.net/wiki/Combat_Style> | CK Wiki (UESP), CC BY-SA |
| [`ck-gamesetting.md`](ck-gamesetting.md) | <https://ck.uesp.net/wiki/GameSetting> | CK Wiki (UESP), CC BY-SA |
| [`ck-perk-entry-point.md`](ck-perk-entry-point.md) | <https://ck.uesp.net/wiki/Perk_Entry_Point> + UESP `Mod File Format/PERK` | CK Wiki / UESP, CC BY-SA |
| [`ck-knockareaeffect.md`](ck-knockareaeffect.md) | <https://ck.uesp.net/wiki/KnockAreaEffect_-_ObjectReference> | CK Wiki (UESP), CC BY-SA |
| [`animation-events.md`](animation-events.md) | <https://pastebin.com/Z5NwnMbQ>, fandom Idle Animations | публичные списки сообщества |

Текст в файлах — цитаты (иногда сокращённые, сокращения помечены `…`), а не пересказ.

## Чего здесь нет и почему

Полный дамп UESP и CK Wiki скопировать в этот репозиторий нельзя: это десятки тысяч страниц,
и сеть в рабочей среде открыта не ко всем хостам. Поэтому копируется **то, что используется
в решениях**, и каждая запись ссылается на конкретный раздел страницы.

## Правило пополнения

1. Нужен факт — сначала страница в `wiki/`, потом код.
2. Если страница говорит «verification needed» — так и остаётся в цитате, факт помечается **[C]**.
3. Если факта нет ни в одном источнике — он идёт в [`../ASSUMPTIONS.md`](../ASSUMPTIONS.md), а не в код
   как «очевидно».
