# Окружение, в котором это проверяется

Факты про установку, на которой мод реально запускается (данные из логов `skse64.log` и свойств
`SkyrimSE.exe`). Абсолютные пути в репозиторий не попадают.

## Игра

| Параметр | Значение |
|---|---|
| Издание | Skyrim Special Edition, Steam |
| Runtime | **1.7.104.0** (свойства `SkyrimSE.exe`) |
| SKSE64 | **2.3.1**, файл `skse64_1_7_104.dll` |
| Сборка | `win64`, 64-бит |
| Язык игры | русский (названия оружия в логе: «Стальная булава») |

## Порядок загрузки (из `skse64.log`)

```
[0] Skyrim.esm
[1] Update.esm
[2] Dawnguard.esm
[3] HearthFires.esm
[4] Dragonborn.esm
[5] ccBGSSSE001-Fish.esm
[FE:0] ccQDRSSE001-SurvivalMode.esl
[FE:1] ccBGSSSE037-Curios.esl
[6] ccBGSSSE025-AdvDSGS.esm
[FE:2] _ResourcePack.esl
[7] unofficial skyrim special edition patch.esp
[8] TGCombatPort.esp
[9] alternate start - live another life.esp
```

То есть Creation Club-контент установлен (рыбалка, выживание, редкости, Saints & Seducers + ресурсы).
USSEP стоит, `TGCombatPort.esp` грузится после него.

## Что учитывать при проверке

- Других combat-оверхаулов (Wildcat, Smilodon, Ordinator) в этом порядке **нет** — иначе правила
  складывались бы с их правками блоков и стаггера, и результат был бы неоднозначным.
- Сторонние анимационные моды могут перебивать нашу анимацию укола (DAR, приоритет `90001`).
- Для сравнения версий важен один и тот же профиль: смена порядка плагинов меняет и FormID, а через
  FormID работают правила со ссылками вида `Skyrim.esm|0x…`.

## Где лежит лог

`Документы\My Games\Skyrim Special Edition\SKSE\TGCombatPort.log`
(старые сессии — `TGCombatPort-1.log`, `-2.log` и т.д., номер растёт с каждым запуском).

Логи Papyrus — там же: `Papyrus.0.log`. Их пишет только игра, наш плагин в них не пишет.
