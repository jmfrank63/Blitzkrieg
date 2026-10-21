[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Инструменты для модов

Семь консольных инструментов для создания модов к Blitzkrieg. Они заменяют инструменты 2003 года, работавшие только в Windows, и запускаются в Windows, macOS и Linux. Каждый из них записывает файлы, которые игра читает своими собственными загрузчиками; ни одному не нужна установленная игра, если только он не принимает `--data`.

Где они находятся: в каталоге `Tools/` папки игры. В Windows там лежат `bk-pak`, `bk-table`, `bk-image`, `bk-video` и `bk-font`; в Linux и macOS есть также `bk-map` и `bk-sprite`. Инструмент, которого нет в вашем `Tools/`, собирается из исходников командой `zig build bk-<name>` и попадает в `zig-out/bin`.

## Правила, общие для всех инструментов

- `bk-<tool> --help` выводит справку и завершается с кодом 0. Неверная командная строка завершается с кодом 2 и справкой. Сбой завершается с кодом 1 и одной строкой в stderr, которая начинается с `bk-<tool>: ` и называет файл.
- При успехе инструмент выводит одну итоговую строку и больше ничего.
- В путях допустимы `/` и `\`; выходные пути печатаются с разделителем вашей системы.
- Инструмент записывает только те выходные файлы, которые вы указали (или рядом с входным, если об этом сказано). Он никогда не заменяет существующий выходной файл, если не указан `--force`.
- Результат детерминирован: одни и те же входные данные дают одни и те же байты в любой системе.
- Инструменты, читающие данные игры, принимают `--data <dir>`, по умолчанию `./Data`. Пути в карте или в моде — это пути Data игры, а не вашего диска.

Мод находится в `mods/<Name>/data/` рядом с собственным `Data/` игры, с файлом `mod.xml`, который его называет (`MODName`, `MODVersion`), и игра запускает его с `-mod=<Name>`. Игра читает файлы в этой папке и файлы внутри каждого `.pak` там одинаково.

## Язык

Справка и сообщения доступны на десяти языках. Выберите язык через `--lang <code>` (например, `--lang=ja`), иначе через переменную окружения `BK_LANG`, иначе по языку вашей системы, иначе английский. Префиксы `usage: ` и `bk-<tool>: `, коды завершения и итоговые строки не меняются. В Windows в старом окне консоли могут отсутствовать иероглифы CJK; Windows Terminal их показывает.

## Старый инструмент, новая команда

| Старый инструмент | Новая команда |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | нет; поставляйте `.pak` или обычный zip |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | экспортёр для Blender, см. `docs/blender-replacement.md` |

## bk-pak: создание и распаковка архивов `.pak`

`.pak` — это zip-архив. `bk-pak pack` записывает его так же, как собственный упаковщик релизов игры: все файлы хранятся без сжатия, имена отсортированы, разделитель `/`. `unpack` сначала проверяет весь архив и ничего не записывает, если какая-либо запись повреждена или оказалась бы вне целевой папки.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Имена внутри архива — это пути относительно `<dir>`, поэтому располагайте `<dir>` так же, как папку `data` мода. Архивы Zip64 (более 65 535 записей или 4 ГиБ) не поддерживаются.

Пример: упаковать изменённый юнит мода в мод.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: создание шрифта игры из шрифта TrueType

Отрисовывает символы 32..255 кодовой страницы в папку шрифта вроде `Data/Fonts/small`: `1.tfd` (метрики, прямоугольники текстуры, интервалы A/B/C, кернинг), `1.tga` и `1_h.dds` — текстуру, которую читает игра.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` — высота ячейки в пикселях, подъёмная часть плюс подстрочная (по умолчанию 20).
- `--charset` — это `ansi` (кодовая страница 1252, по умолчанию), `easteurope` (1250), `russian` (1251) или номер кодовой страницы.
- `--chars <file>` — файл в UTF-8: шрифт получает каждый различный символ базовой многоязычной плоскости из него плюс ASCII 32..126; символы, которых нет в гарнитуре, пропускаются. `--set latin1`, `cyrillic` или `devanagari` добавляет встроенный диапазон и может повторяться.
- `--clusters <table>` — таблица кластеров, записанная экспортом language-mods (строки `U+E000<TAB>cluster`): каждый кластер становится одним глифом под своим символом из области частного использования, оформленным гарнитурой `--ttf`. `--script` (по умолчанию `Deva`) и `--language` (по умолчанию `hi`) задают шейпинг.
- `--ttf` принимает файл `.ttf`, `.otf` или `.ttc`. Имя гарнитуры ищется в системных папках шрифтов по возможности; надёжнее указать файл.
- `--weight 600` и выше выбирает жирную гарнитуру или утолщает обычную; `--italic` выбирает курсивную или наклоняет. `--no-aa` отключает сглаживание.
- Проверьте лицензию шрифта, прежде чем поставлять его в моде.

Пример: 16-пиксельный шрифт для мода из файла шрифта с открытой лицензией.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: правка файлов характеристик в таблице

`export` разворачивает каждый подходящий XML-файл характеристик в папке в один лист с разделителями-табуляциями: одна строка на файл, один столбец на поле. Excel, LibreOffice и Numbers открывают его. `import` записывает изменённые вами ячейки обратно в файлы, меняя только байты этих значений; комментарии, порядок и форматирование всего остального остаются прежними.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Имена столбцов — пути полей: `MaxHP` — атрибут, `KeyName;#text` — текст элемента, `Guns;item(01);Ammo` — вторая запись списка.
- Характеристики читаются из `<desc>` для `*.bld` и `*.obt`, из `<effect>` для `*.eff` и из `<RPG>` в остальных случаях. Поставляемые здания хранят их в `1.xml` под `<desc>`, поэтому экспортируйте их с `--root desc`. Для нового собственного типа характеристик укажите `--root <name>`.
- `--only` оставляет, а `--ignore` отбрасывает столбцы, начинающиеся с первого слова каждой строки `<fields.txt>`.
- При импорте пустая ячейка или `_` оставляет значение как есть. Отсутствующий файл или поле сообщается и пропускается; импорт никогда ничего не добавляет.
- Столбец `FileName` отсчитывается от места, где вы запускали `export`; запускайте `import` оттуда же. Листы, сохранённые табличным редактором (ячейки в кавычках, CRLF), и старые листы `.txt` принимаются.

Пример: увеличить очки здоровья одного юнита.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Измените в таблице `MaxHP` строки `Stats/tank.xml` со 120 на 150, сохраните как текст с разделителями-табуляциями, затем:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

В `Stats/tank.xml` теперь `MaxHP="150"`; ни один другой байт не изменился.

## bk-map: карты, миникарты и модели из Blender

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` и `to-bzm` преобразуют карту между двоичным `.bzm` игры и редактируемым `.xml` тем же читателем и писателем, что и в редакторе карт.
- `minimap` рисует `<map>_c.dds`, `<map>_l.dds` и `<map>_h.dds` размером `<px>` на `<px>` — изображения, которые игра показывает для карты.
- Каталог просматривается рекурсивно и отражается в `--out`; без `--out` каждый результат кладётся рядом с входным файлом. Для карты, у которой есть и `.bzm`, и `.xml`, `minimap` берёт более новый, как и игра.
- Командам карт нужна папка `Data` игры (дороги, реки, объекты и наборы тайлов): запускайте их из папки игры или укажите `--data`.
- `obj2mod` превращает OBJ экспортёра Blender и его файлы скелета и анимации в `.mod`; `validate-obj2mod` проверяет те же файлы и ничего не записывает. Форматы файлов и сторона Blender описаны в `docs/blender-replacement.md`.

Пример: отредактировать карту как XML, преобразовать обратно и нарисовать её миникарту.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Пример: модель из Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: сборка спрайтовых анимаций

Упаковывает отрисованные кадры 2D-объекта (мосты, заборы, здания, эффекты) в одну текстуру `<out-name>.tga` и описывает его анимации в `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` содержит `anims.txt` — по одному `[section]` на анимацию — и кадры: кадр `i` анимации `<name>` — это `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). У анимации с именем `default` один кадр `<desc-dir>/1.tga`.

```text
dirs=<n>                 number of directions (required)
dir<k> seq=<i>,<j>,...   frame indices of direction k (required)
frametime=<ms>           one frame's show time (default 125)
speed=<f>                translation speed (default 0)
cycled=<0|1>             cycled or one-shot (default 1)
shift=<x>,<y>            every frame's shift (default 0,0)
dir<k> shift=<x>,<y>     direction k's frames' shift (default: shift)
frame<i> shift=<x>,<y>   frame i's shift (default: its direction's)
```

Имена секций — это слоты анимации игры (`idle`, `run`, `death`, ...); `default` и `effect` оба равны `idle`, поэтому используйте только один из трёх. `<out-name>` — путь от места запуска инструмента; его папка создаётся.

Пример: объект с двунаправленным циклом покоя и однократной смертью.

```text
; smoke/anims.txt, frames in smoke/idle/000..002.tga and smoke/death/000..001.tga
[idle]
dirs=2
dir0 seq=0,1,2
dir1 seq=2,1,0
frametime=100
shift=4,8

[death]
dirs=1
dir0 seq=0,1
frametime=150
cycled=0
```

```text
$ bk-sprite compose smoke out/smoke
wrote out/smoke.tga and out/smoke.san: 2 animations, 5 frames, 32x16 atlas
```

## bk-image: преобразование тайлов рельефа

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` меняет местами две ромбовидные половины тайла рельефа, как это делал `OffsetRomb.exe`, чтобы привести графику тайлов, нарисованную в старой смещённой раскладке, к раскладке игры. Он читает TGA в истинном цвете и в градациях серого и записывает 32-битный TGA. Без `--out` он перезаписывает входной файл. Повторный запуск не возвращает оригинал; сохраняйте исходный файл.

Пример:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: преобразование роликов Bink в Ogg Theora

Игра воспроизводит ролики Ogg Theora `.ogv`. `bk-video convert` превращает каждый `.bik` в папке в `<name>.ogv` рядом с ним, а первую звуковую дорожку ролика — в файл `<name>.audio.ogg`. Он запускает `ffmpeg` и `ffprobe`, которые вы устанавливаете сами (в любой стандартной сборке есть декодер Bink, Theora и Vorbis); без них ничего не преобразуется.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` по умолчанию `Data/movies`. `--no-recurse` читает только эту папку.
- `--what-if` перечисляет задания и ничего не запускает. Существующие результаты сохраняются, если не указан `--force`.
- `--ffmpeg` и `--ffprobe` указывают на программы, если их нет в `PATH`.
- `--key-frame-interval` — это `-g` ffmpeg (по умолчанию 1, каждый кадр ключевой).
- Ролик, который ffmpeg не может прочитать, сообщается в stderr; остальные всё равно преобразуются, а инструмент завершается с кодом 1.

Пример:

```text
$ bk-video convert --input-root movies --what-if
Would convert: movies/intro.bik -> movies/intro.ogv
Would extract audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=0 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=0
$ bk-video convert --input-root movies
Converting: movies/intro.bik -> movies/intro.ogv
Extracting audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=1 Skipped=0 AudioConverted=1 AudioSkipped=0 Failed=0
```
