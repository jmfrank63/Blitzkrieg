# Glossary: Brazilian Portuguese (Português (Brasil))

Scope: terms of the main menu, the mission briefing, unit types and ranks, so that the overlays (S02 T04), the checker (S03) and the documents (S08) use one translation per term. The English column is the 2003 game's wording.

Review state: machine seed, needs native review. The menu terms are what the `pt-BR` overlay uses; nothing here has been checked by a native speaker.

Typography (S04 T01): Brazilian Portuguese with the post-2009 orthography (no trema, `ideia`, `voo`), `você` for the player, `tela`, `arquivo`, `equipe`, `caminhão`, `Salvar jogo`, `Configurações`, gerund forms. The accented letters `á â ã à ç é ê í ó ô õ ú` and their capitals are cp1252 and are drawn by the language mods' Latin font like the Spanish ones, not by the shipped Cyrillic `1.tfd` files, so the glyph rule accepts them; no character beyond those the font test in tools/zig/l10n_font.zig already asserts is used. Quotations, weapon names and operation names use straight ASCII double quotes `"texto"`, the apostrophe is the ASCII `'`, and U+2011, U+202F and U+2009 are never used. Hotkey tokens (`<Esc>`, `<Shift>`, `<Space>`, `<F5>`), `#n` counters and the key letters in `[Q]` stay as the game reads them, and so does every line break. Ranks keep the Portuguese military forms (`Sargento`, `Primeiro-sargento`, `Sargento-ajudante`, `Subtenente`, `Segundo-tenente`, `Primeiro-tenente`, `Capitão da Guarda`) and the German ranks of the German personnel stay German words (`Leutnant`, `Feldwebel`, `Hauptmann`). Recurring terms: `Missão`, `Objetivos`, `Campanha`, `Capítulo`, `Aprimorar` (upgrade), `Contratar` (hire), `esquadra` (squad), `blindagem` (armour), `fuzil` (rifle), `submetr.` (submachine gun, abbreviated in the narrow unit-name boxes), `Autopropulsados` (self-propelled guns), `Aviação`. Water tanks are `Tanque de água`, which carries the glossary string `tanque`. A plural does not carry the singular stem (`caminhões` has no `caminhão`), so a plural English word is not held to the row, and a text that needs a singular Portuguese term uses it.

## Unit types

| English term | Translation | Note |
|---|---|---|
| infantry | infantaria |  |
| tank | tanque |  |
| artillery | artilharia |  |
| anti-tank gun | canhão antitanque |  |
| truck | caminhão |  |
| aircraft | aviação |  |
| sniper | atirador de elite |  |
| engineer | engenheiro |  |

## Ranks

| English term | Translation | Note |
|---|---|---|
| private | soldado | no-term-check: also an ordinary English word |
| corporal | cabo |  |
| sergeant | sargento |  |
| lieutenant | tenente |  |
| captain | capitão |  |
| major | major | no-term-check: also an ordinary English word |
| colonel | coronel |  |
| general | general | no-term-check: also an ordinary English word |

## Interface

| English term | Translation | Note |
|---|---|---|
| main menu | Menu principal |  |
| new game | Novo jogo |  |
| campaign | Campanha |  |
| chapter | Capítulo |  |
| mission | Missão |  |
| objectives | Objetivos |  |
| briefing | Briefing |  |
| start | Iniciar |  |
| back | Voltar | no-term-check: also an ordinary English word |
| options | Opções |  |
| settings | Configurações |  |
| load game | Carregar jogo |  |
| save game | Salvar jogo |  |
| exit game | Sair |  |
| credits | Créditos |  |
| cutscenes | Cenas |  |
| custom game | Jogo personalizado |  |
| multiplayer | Multijogador |  |
| tutorial | Tutorial |  |
