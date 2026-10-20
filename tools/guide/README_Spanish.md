[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Herramientas de modding

Siete herramientas de línea de comandos para crear mods de Blitzkrieg. Sustituyen a las herramientas de 2003, que solo funcionaban en Windows, y se ejecutan en Windows, macOS y Linux. Todas escriben archivos que el juego lee con sus propios cargadores; ninguna necesita una instalación del juego, salvo que reciba `--data`.

Dónde están: en el directorio `Tools/` de la carpeta del juego. En Windows esa carpeta contiene `bk-pak`, `bk-table`, `bk-image`, `bk-video` y `bk-font`; Linux y macOS tienen además `bk-map` y `bk-sprite`. Una herramienta que falte en su `Tools/` se compila desde el código fuente con `zig build bk-<name>` y queda en `zig-out/bin`.

## Reglas que cumplen todas las herramientas

- `bk-<tool> --help` imprime el uso y termina con el código 0. Una línea de comandos incorrecta termina con el código 2 y el uso. Un fallo termina con el código 1 y una línea en stderr que empieza por `bk-<tool>: ` y nombra el archivo.
- Si todo va bien, la herramienta imprime una línea de resumen y nada más.
- Las rutas admiten `/` o `\`; las rutas de salida se imprimen con el separador de su sistema.
- Una herramienta escribe solo las salidas que usted indica (o junto a su entrada cuando así se dice). Nunca reemplaza una salida existente salvo que pase `--force`.
- La salida es determinista: la misma entrada produce los mismos bytes en cualquier sistema.
- Las herramientas que leen datos del juego aceptan `--data <dir>`, por defecto `./Data`. Las rutas de un mapa o de un mod son las rutas Data del juego, no las de su disco.

Un mod vive en `mods/<Name>/data/` junto al `Data/` propio del juego, con un `mod.xml` que lo nombra (`MODName`, `MODVersion`), y el juego lo ejecuta con `-mod=<Name>`. El juego lee por igual los archivos de esa carpeta y los archivos dentro de cada `.pak` que haya allí.

## Idioma

La ayuda y los mensajes están disponibles en diez idiomas. Elija uno con `--lang <code>` (por ejemplo `--lang=ja`); si no, con la variable de entorno `BK_LANG`; si no, el idioma de su sistema; si no, inglés. Los prefijos `usage: ` y `bk-<tool>: `, los códigos de salida y las líneas de resumen no cambian. En Windows, la ventana de consola heredada puede carecer de glifos CJK; Windows Terminal los muestra.

## Herramienta antigua, comando nuevo

| Herramienta antigua | Comando nuevo |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | ninguno; distribuya el `.pak` o un zip normal |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | el exportador de Blender, véase `docs/blender-replacement.md` |

## bk-pak: crear y extraer archivos `.pak`

Un `.pak` es un archivo zip. `bk-pak pack` escribe uno como lo hace el empaquetador de versiones del propio juego: todos los archivos sin comprimir, nombres ordenados, separadores `/`. `unpack` comprueba primero todo el archivo y no escribe nada si alguna entrada está dañada o acabaría fuera de la carpeta de destino.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Los nombres dentro del archivo son las rutas bajo `<dir>`, así que organice `<dir>` como la carpeta `data` del mod. Los archivos Zip64 (más de 65 535 entradas o 4 GiB) no se admiten.

Ejemplo: empaquetar en el mod una unidad modificada del mod.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: crear una fuente del juego a partir de una fuente TrueType

Renderiza los caracteres 32..255 de una página de códigos en una carpeta de fuente como `Data/Fonts/small`: `1.tfd` (métricas, rectángulos de textura, espaciado A/B/C, kerning), `1.tga` y `1_h.dds`, la textura que lee el juego.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` es la altura de la celda en píxeles, ascendente más descendente (por defecto 20).
- `--charset` es `ansi` (página de códigos 1252, el valor por defecto), `easteurope` (1250), `russian` (1251) o un número de página de códigos.
- `--chars <file>` es un archivo UTF-8: la fuente incluye todos los caracteres distintos del Plano Multilingüe Básico que contenga, además de ASCII 32..126; los caracteres que la tipografía no tiene se omiten. `--set latin1`, `cyrillic` o `devanagari` añade un rango integrado y puede repetirse.
- `--clusters <table>` es la tabla de clústeres que escribió la exportación de language-mods (líneas `U+E000<TAB>cluster`): cada clúster se convierte en un glifo bajo su carácter de Área de Uso Privado, con la forma que le da la tipografía de `--ttf`. `--script` (por defecto `Deva`) y `--language` (por defecto `hi`) eligen el moldeado.
- `--ttf` acepta un archivo `.ttf`, `.otf` o `.ttc`. Un nombre de tipografía se busca en las carpetas de fuentes del sistema, sin garantías; un archivo es la vía fiable.
- `--weight 600` o más elige la variante en negrita o pone una en negrita; `--italic` elige o inclina. `--no-aa` desactiva el suavizado.
- Compruebe la licencia de la fuente antes de distribuirla en un mod.

Ejemplo: una fuente de 16 píxeles para un mod, a partir de un archivo de fuente de licencia abierta.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: editar archivos de estadísticas en una hoja de cálculo

`export` aplana todos los archivos XML de estadísticas que coincidan en una carpeta en una sola hoja separada por tabuladores: una fila por archivo, una columna por campo. Excel, LibreOffice y Numbers la abren. `import` vuelve a escribir en los archivos las celdas que usted cambió, modificando solo los bytes de esos valores; los comentarios, el orden y el formato de todo lo demás se quedan como estaban.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Los nombres de columna son rutas de campo: `MaxHP` es un atributo, `KeyName;#text` el texto de un elemento, `Guns;item(01);Ammo` la segunda entrada de una lista.
- Las estadísticas se leen de `<desc>` en `*.bld` y `*.obt`, de `<effect>` en `*.eff` y de `<RPG>` en los demás. Los edificios incluidos guardan las suyas en `1.xml` bajo `<desc>`, así que expórtelos con `--root desc`. Un tipo de estadística nuevo propio usa `--root <name>`.
- `--only` conserva y `--ignore` descarta las columnas que empiezan por la primera palabra de cada línea de `<fields.txt>`.
- Al importar, una celda vacía o `_` deja el valor como está. Un archivo o campo que falte se notifica y se omite; `import` nunca añade nada.
- La columna `FileName` es relativa al lugar desde donde ejecutó `export`; ejecute `import` desde el mismo sitio. Se aceptan las hojas guardadas por una hoja de cálculo (celdas entrecomilladas, CRLF) y las hojas `.txt` antiguas.

Ejemplo: subir los puntos de vida de una unidad.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Cambie `MaxHP` de la fila `Stats/tank.xml` de 120 a 150 en la hoja de cálculo, guárdela como texto separado por tabuladores y después:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` ahora tiene `MaxHP="150"`; ningún otro byte cambió.

## bk-map: mapas, minimapas y modelos de Blender

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` y `to-bzm` convierten un mapa entre el `.bzm` binario del juego y el `.xml` editable, con el mismo lector y escritor que el Editor de mapas.
- `minimap` dibuja `<map>_c.dds`, `<map>_l.dds` y `<map>_h.dds`, cuadrados de `<px>`, las imágenes que el juego muestra para un mapa.
- Un directorio se recorre de forma recursiva y se replica bajo `--out`; sin `--out`, cada salida se escribe junto a su entrada. Para un mapa que tiene tanto `.bzm` como `.xml`, `minimap` usa el más reciente, igual que el juego.
- Los comandos de mapa necesitan el `Data` del juego (carreteras, ríos, objetos y conjuntos de teselas): ejecútelos desde la carpeta del juego o pase `--data`.
- `obj2mod` convierte el OBJ del exportador de Blender y sus archivos de esqueleto y animación en un `.mod`; `validate-obj2mod` comprueba los mismos archivos y no escribe nada. Los formatos de archivo y la parte de Blender están en `docs/blender-replacement.md`.

Ejemplo: editar un mapa como XML, convertirlo de nuevo y dibujar su minimapa.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Ejemplo: un modelo desde Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: componer animaciones de sprites

Empaqueta los fotogramas renderizados de un objeto 2D (puentes, vallas, edificios, efectos) en una sola textura `<out-name>.tga` y describe sus animaciones en `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` contiene `anims.txt`, con una `[section]` por animación, y los fotogramas: el fotograma `i` de la animación `<name>` es `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). Una animación llamada `default` tiene el único fotograma `<desc-dir>/1.tga`.

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

Los nombres de sección son las ranuras de animación del juego (`idle`, `run`, `death`, ...); `default` y `effect` son ambas `idle`, así que use solo una de las tres. `<out-name>` es una ruta relativa a donde ejecuta la herramienta; su carpeta se crea.

Ejemplo: un objeto con un bucle de reposo de dos direcciones y una muerte de una sola reproducción.

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

## bk-image: convertir arte de teselas de terreno

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` intercambia las dos mitades en forma de rombo de una tesela de terreno, como hacía `OffsetRomb.exe`, para llevar el arte de teselas dibujado en el antiguo diseño desplazado al diseño del juego. Lee archivos TGA de color verdadero y de escala de grises y escribe un TGA de 32 bits. Sin `--out` sobrescribe la entrada. Ejecutarlo dos veces no devuelve el original; conserve su archivo de origen.

Ejemplo:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: convertir películas Bink a Ogg Theora

El juego reproduce películas Ogg Theora `.ogv`. `bk-video convert` convierte cada `.bik` de una carpeta en `<name>.ogv` a su lado, y la primera pista de audio de la película en un archivo auxiliar `<name>.audio.ogg`. Ejecuta `ffmpeg` y `ffprobe`, que usted mismo instala (cualquier compilación estándar tiene el decodificador Bink, Theora y Vorbis); sin ellos no se convierte nada.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` es por defecto `Data/movies`. `--no-recurse` lee solo esa carpeta.
- `--what-if` lista los trabajos y no ejecuta nada. Las salidas existentes se conservan salvo que se use `--force`.
- `--ffmpeg` y `--ffprobe` indican los programas cuando no están en el `PATH`.
- `--key-frame-interval` es el `-g` de ffmpeg (por defecto 1, todos los fotogramas son clave).
- Una película que ffmpeg no puede leer se notifica en stderr; el resto se sigue convirtiendo y la herramienta termina con el código 1.

Ejemplo:

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
