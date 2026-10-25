# Glossary: Spanish (Español)

Scope: terms of the main menu, the mission briefing, unit types and ranks, so that the overlays (S02 T04), the checker (S03) and the documents (S08) use one translation per term. The English column is the 2003 game's wording.

Review state: machine seed, needs native review. The menu terms are what the `es` overlay uses; nothing here has been checked by a native speaker.

Typography (S03 T01): neutral international Spanish, `usted` for the player, no voseo. Sentences carry the opening marks `¿` and `¡` and the letters `ñ` and `Ñ`: like the accented letters they are not in the shipped Cyrillic `1.tfd` files (the test `the shipped fonts carry the punctuation Spanish typography uses` in tools/zig/l10n_font.zig checks what is: U+00AB/U+00BB, U+2019, U+2026, U+2013 and U+2014), all four are cp1252 and the language mods' Latin font draws them, so the glyph rule accepts them and l10n-game-reads-it-es measures that no glyph is missing. Quotations and operation names use the guillemets `«texto»` without spaces inside; the apostrophe is the ASCII `'`; U+2011, U+202F and U+2009 are not cp1252 and are never used. Hotkey tokens (`<Esc>`, `<Shift>`, `<Space>`, `<F5>`), `#n` counters and the key letters in `[Q]` stay as the game reads them, and so does every line break. Abbreviations that a 2003 box forces are written with a period (`Jug.`, `Tam.`, `mun. prim.`); a text is never cut short. Ordinals use `.ª`: `3.ª clase`. Ranks follow the Spanish army (`sargento primero`, `subteniente`, `teniente mayor` for the Soviet senior lieutenant, `starshina` for the Soviet foreman); German ranks keep the German word capitalised (`Oberfeldwebel`). Recurring words outside the tables, which the glossary cannot take as rows (the nine glossaries keep equal row counts): scout = explorador, squad = pelotón, replay = repetición, statistics = estadísticas, hire = reclutar, depot = depósito, water tank = tanque de agua, pillbox = casamata, ranging = reglaje, upgrade = mejora.

Historical names (S03 T05, scenarios/chapters): one Spanish spelling each, Operación Barbarroja, Járkov, Stalingrado, Leningrado, Kursk, Normandía, las Ardenas, Moscú, Bastoña, Dunkerque, Dniéper, Projórovka, Iasi-Kishiniov (no `ș`: not cp1252), Seelow, Dánzig, Cirenaica, Bielorrusia, Rumanía; operations keep their foreign name when the Spanish literature does (`Fall Gelb`, `Weiss`, `Overlord`, `Torch`, `Crusader`, `Bagration`) and translate when it does not (Diadema, Júpiter, Tifón, Ciudadela). Counts of aircraft read `unidades de aviación` so the glossary term stays in the sentence; `tanques` for tanks; thousands are written with a period (`3.300.000`).

## Unit types

| English term | Translation | Note |
|---|---|---|
| infantry | infantería |  |
| tank | tanque |  |
| artillery | artillería |  |
| anti-tank gun | cañón antitanque |  |
| truck | camión |  |
| aircraft | aviación |  |
| sniper | francotirador |  |
| engineer | ingeniero |  |

## Ranks

| English term | Translation | Note |
|---|---|---|
| private | soldado raso | no-term-check: also an ordinary English word |
| corporal | cabo |  |
| sergeant | sargento |  |
| lieutenant | teniente |  |
| captain | capitán |  |
| major | mayor | no-term-check: also an ordinary English word |
| colonel | coronel |  |
| general | general | no-term-check: also an ordinary English word |

## Interface

| English term | Translation | Note |
|---|---|---|
| main menu | Menú principal |  |
| new game | Nueva partida |  |
| campaign | Campaña |  |
| chapter | Capítulo |  |
| mission | Misión |  |
| objectives | Objetivos |  |
| briefing | Informe de la misión |  |
| start | Comenzar |  |
| back | Atrás | no-term-check: also an ordinary English word |
| options | Opciones |  |
| settings | Ajustes |  |
| load game | Cargar partida |  |
| save game | Guardar partida |  |
| exit game | Salir |  |
| credits | Créditos |  |
| cutscenes | Escenas |  |
| custom game | Partida personalizada |  |
| multiplayer | Multijugador |  |
| tutorial | Tutorial |  |
