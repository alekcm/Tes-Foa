# Зависимости и инструменты

**На старте репозитория обязательных модовых зависимостей ещё не выбрано.** Не устанавливать в список SKSE, Pandora, Nemesis, FNIS, Spriggit или CommonLibSSE-NG просто «на всякий случай». Сначала прототипировать конкретный required behavior и зафиксировать, зачем нужен каждый компонент.

| Компонент | Нужен? | Точная версия/tag/commit | Для чего используется | Runtime/ограничения | Ссылка | Проверено на целевой машине? |
|---|---|---|---|---|---|---|
| Skyrim runtime | Да | Заполнить в `SETUP.md` | Игра/совместимость | Точный exe version | https://store.steampowered.com/app/489830/ | Нет |
| Creation Kit | Скорее всего, уточнить по плану реализации | Заполнить после проверки установки | Создание/редактирование plugin/form | PC; версия зафиксировать | https://store.steampowered.com/app/1946180/ | Нет |
| Papyrus compiler/source scripts | Только если будут Papyrus scripts | TBD | Компиляция `.psc` → `.pex` | Требует целевые game/compiler scripts | CK / https://ck.uesp.net/wiki/Main_Page | Нет |
| SKSE | Только если доказана необходимость extender API | TBD | Extender API / возможная native функциональность | Версия привязана к runtime | https://skse.silverlock.org/ | Нет |
| CommonLibSSE-NG | Только если принят C++ SKSE plugin | TBD | C++ plugin API/build support | Пиновать commit/tag/runtime support | https://github.com/CharmedBaryon/CommonLibSSE-NG | Нет |
| Animation behavior generator | Только если нужен новый/изменённый behavior graph | TBD | Объединение behavior/animation patches | Выбрать ровно один поддерживаемый workflow после теста | Pandora: https://github.com/Monitor221hz/Pandora-Behaviour-Engine-Plus | Нет |
| Spriggit | Необязательно | TBD | Текстовое представление plugin records в Git | Проверить serialize → deserialize → CK/xEdit/game | https://github.com/Mutagen-Modding/Spriggit | Нет |
| Mod manager/test profile | Для воспроизводимых тестов — записать фактический | TBD | Изолированная установка и load order | Не считать пользовательский основной профиль тестовым | Ссылка по выбору пользователя | Нет |

## Заполнять строку зависимости только так

1. Записать, какое требование дизайна нуждается в зависимости.
2. Доказать это минимальным прототипом; если можно без зависимости, не добавлять её.
3. Поставить точный release/tag/version (не `latest`) и ссылку на первичный репозиторий/страницу разработчика.
4. Записать точный game runtime, поддерживаемый этой версией, и фактическую проверку сборки/запуска.
5. Обновить таблицу и `STATUS.md`; приложить команду установки/сборки, если её можно безопасно воспроизвести.
