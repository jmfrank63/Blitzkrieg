# Glossary: Russian (Русский)

Scope: terms of the main menu, the mission briefing, unit types and ranks, so that the overlays (S02 T04), the checker (S03) and the documents (S08) use one translation per term. The English column is the 2003 game's wording.

Review state: the menu terms (the thirteen main-menu labels and the header) are the S01 overlay texts in `Localization/game/ru/`; the unit and rank terms are a machine seed, refined in M003 S06 T02 and still in need of native review.

Matching: the checker looks the Translation column up as a case-insensitive substring of the translated text. Russian inflects, so a unit term is written as a stem that every case form contains (`артиллери`, `противотанков`, `пехот`); the Note gives the dictionary form. Write `сапёр` with ё everywhere, never `сапер`. An ordinary English word (private, major, general, back) is marked `no-term-check`.

## Unit types

| English term | Translation | Note |
|---|---|---|
| infantry | пехот | пехота; stem survives пехоты, пехоте, пехотный |
| tank | танк | танк; танка, танков |
| artillery | артиллери | артиллерия; stem survives артиллерии, артиллерийский |
| anti-tank gun | противотанков | противотанковое орудие; stem survives противотанковая пушка, противотанковых |
| truck | грузовик | грузовик; грузовика, грузовики |
| aircraft | авиаци | авиация; stem survives авиации |
| sniper | снайпер | снайпер; снайпера, снайперы |
| engineer | сапёр | сапёр with ё everywhere; сапёры, сапёров |

## Ranks

| English term | Translation | Note |
|---|---|---|
| private | рядовой | no-term-check: also an ordinary English word |
| corporal | ефрейтор | ефрейтор; ефрейтора |
| sergeant | сержант | сержант; сержанта |
| lieutenant | лейтенант | лейтенант; старший лейтенант in full, ст. лейт. only on tight buttons |
| captain | капитан | капитан; капитана |
| major | майор | no-term-check: also an ordinary English word |
| colonel | полковник | полковник; полковника |
| general | генерал | no-term-check: also an ordinary English word |

## Interface

| English term | Translation | Note |
|---|---|---|
| main menu | Главное меню |  |
| new game | Новая игра |  |
| campaign | Кампани | Кампании in the menu; stem survives кампания, кампании |
| chapter | Глав | Глава; stem survives главы, главе |
| mission | Миссия |  |
| objectives | Цели миссии |  |
| briefing | Описание миссии |  |
| start | Начать |  |
| back | Назад | no-term-check: also an ordinary English word |
| options | Опции |  |
| settings | Настройки |  |
| load game | Загрузить игру | interface verb Загрузить; Загруз. only on tight buttons |
| save game | Сохранить игру | interface verb Сохранить |
| exit game | Выход | interface noun for the exit button |
| credits | Авторы |  |
| cutscenes | Ролики |  |
| custom game | Своя игра |  |
| multiplayer | Сетевая игра |  |
| tutorial | Обучение |  |

## Decisions for the translation tasks

These are not checked by the rule; T03-T08 follow them.

- Ranks: рядовой, ефрейтор, сержант, лейтенант, старший лейтенант, капитан, майор, полковник, генерал. Tight buttons may use ст. лейт.
- Armed forces: the Soviet army is Красная армия (the 2003 Russian originals write Красная Армия; keep the original capitalisation where a key is already Russian); the German one is Вермахт; the war is Великая Отечественная война.
- German vehicles in their usual Russian form: Тигр, Пантера, Фердинанд; StuG is «Штуг» in running text and САУ StuG III where the model is named in full. Designations (Pz.IV, Bf 109, T-34) stay as written.
- Interface verbs: Загрузить, Сохранить, Выход. Abbreviations allowed only where the fit rule demands it: ст. лейт., Загруз.
- Keep the Latin key names of the tutorials (R, Q, A, F, Esc, Tab, Shift) as written.
