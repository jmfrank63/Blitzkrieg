[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Outils de modding

Sept outils en ligne de commande pour créer des mods de Blitzkrieg. Ils remplacent les outils de 2003, qui ne fonctionnaient que sous Windows, et s'exécutent sous Windows, macOS et Linux. Chacun écrit des fichiers que le jeu lit avec ses propres chargeurs ; aucun n'a besoin d'une installation du jeu, sauf s'il accepte `--data`.

Où les trouver : dans le répertoire `Tools/` du dossier du jeu. Sous Windows, ce dossier contient `bk-pak`, `bk-table`, `bk-image`, `bk-video` et `bk-font` ; Linux et macOS ont aussi `bk-map` et `bk-sprite`. Un outil absent de votre `Tools/` se compile à partir des sources avec `zig build bk-<name>` et se retrouve dans `zig-out/bin`.

## Règles communes à tous les outils

- `bk-<tool> --help` affiche l'usage et se termine avec le code 0. Une ligne de commande incorrecte se termine avec le code 2 et l'usage. Un échec se termine avec le code 1 et une ligne sur stderr qui commence par `bk-<tool>: ` et nomme le fichier.
- En cas de succès, un outil affiche une seule ligne de résumé et rien d'autre.
- Les chemins acceptent `/` ou `\` ; les chemins de sortie sont affichés avec le séparateur de votre système.
- Un outil n'écrit que les sorties que vous nommez (ou à côté de son entrée lorsque c'est indiqué). Il ne remplace jamais une sortie existante sauf si vous passez `--force`.
- La sortie est déterministe : la même entrée donne les mêmes octets sur tous les systèmes.
- Les outils qui lisent les données du jeu acceptent `--data <dir>`, par défaut `./Data`. Les chemins d'une carte ou d'un mod sont les chemins Data du jeu, pas ceux de votre disque.

Un mod se trouve dans `mods/<Name>/data/` à côté du `Data/` propre au jeu, avec un `mod.xml` qui le nomme (`MODName`, `MODVersion`), et le jeu le lance avec `-mod=<Name>`. Le jeu lit de la même façon les fichiers de ce dossier et les fichiers contenus dans chaque `.pak` qui s'y trouve.

## Langue

L'aide et les messages existent en dix langues. Choisissez-en une avec `--lang <code>` (par exemple `--lang=ja`), sinon avec la variable d'environnement `BK_LANG`, sinon la langue de votre système, sinon l'anglais. Les préfixes `usage: ` et `bk-<tool>: `, les codes de sortie et les lignes de résumé ne changent pas. Sous Windows, l'ancienne fenêtre de console peut manquer de glyphes CJK ; Windows Terminal les affiche.

## Ancien outil, nouvelle commande

| Ancien outil | Nouvelle commande |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | aucune ; fournissez le `.pak` ou un zip ordinaire |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | l'exporteur Blender, voir `docs/blender-replacement.md` |

## bk-pak : créer et extraire des archives `.pak`

Un `.pak` est une archive zip. `bk-pak pack` en écrit une comme le fait l'outil de packaging des versions du jeu lui-même : tous les fichiers stockés sans compression, noms triés, séparateurs `/`. `unpack` vérifie d'abord toute l'archive et n'écrit rien si une entrée est endommagée ou atterrirait en dehors du dossier cible.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Les noms dans l'archive sont les chemins sous `<dir>` ; organisez donc `<dir>` comme le dossier `data` du mod. Les archives Zip64 (plus de 65 535 entrées ou 4 Gio) ne sont pas prises en charge.

Exemple : empaqueter dans le mod une unité modifiée du mod.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font : créer une police de jeu à partir d'une police TrueType

Rend les caractères 32..255 d'une page de codes dans un dossier de police comme `Data/Fonts/small` : `1.tfd` (métriques, rectangles de texture, espacement A/B/C, crénage), `1.tga` et `1_h.dds`, la texture que lit le jeu.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` est la hauteur de la cellule en pixels, ascendante plus descendante (20 par défaut).
- `--charset` vaut `ansi` (page de codes 1252, la valeur par défaut), `easteurope` (1250), `russian` (1251) ou un numéro de page de codes.
- `--chars <file>` est un fichier UTF-8 : la police reçoit chaque caractère distinct du plan multilingue de base qu'il contient, plus ASCII 32..126 ; les caractères que la police n'a pas sont omis. `--set latin1`, `cyrillic` ou `devanagari` ajoute une plage intégrée et peut être répété.
- `--clusters <table>` est la table de clusters qu'a écrite l'export de language-mods (lignes `U+E000<TAB>cluster`) : chaque cluster devient un glyphe sous son caractère de la zone d'usage privé, mis en forme avec la police `--ttf`. `--script` (`Deva` par défaut) et `--language` (`hi` par défaut) choisissent la mise en forme.
- `--ttf` accepte un fichier `.ttf`, `.otf` ou `.ttc`. Un nom de police est cherché dans les dossiers de polices du système, au mieux ; un fichier est le moyen fiable.
- `--weight 600` ou plus choisit la variante grasse ou met une police en gras ; `--italic` choisit ou incline. `--no-aa` désactive le lissage.
- Vérifiez la licence de la police avant de la distribuer dans un mod.

Exemple : une police de 16 pixels pour un mod, à partir d'un fichier de police sous licence libre.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table : modifier les fichiers de statistiques dans un tableur

`export` aplatit tous les fichiers XML de statistiques correspondants d'un dossier en une seule feuille séparée par des tabulations : une ligne par fichier, une colonne par champ. Excel, LibreOffice et Numbers l'ouvrent. `import` réécrit dans les fichiers les cellules que vous avez modifiées, en ne changeant que les octets de ces valeurs ; les commentaires, l'ordre et la mise en forme de tout le reste restent comme ils étaient.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Les noms de colonnes sont des chemins de champ : `MaxHP` est un attribut, `KeyName;#text` le texte d'un élément, `Guns;item(01);Ammo` la deuxième entrée d'une liste.
- Les statistiques sont lues dans `<desc>` pour `*.bld` et `*.obt`, dans `<effect>` pour `*.eff` et dans `<RPG>` sinon. Les bâtiments livrés gardent les leurs dans `1.xml` sous `<desc>` ; exportez-les donc avec `--root desc`. Un nouveau type de statistiques de votre cru prend `--root <name>`.
- `--only` garde et `--ignore` retire les colonnes qui commencent par le premier mot de chaque ligne de `<fields.txt>`.
- À l'import, une cellule vide ou `_` laisse la valeur telle quelle. Un fichier ou un champ manquant est signalé et ignoré ; `import` n'ajoute jamais rien.
- La colonne `FileName` est relative à l'endroit où vous avez lancé `export` ; lancez `import` depuis le même endroit. Les feuilles enregistrées par un tableur (cellules entre guillemets, CRLF) et les anciennes feuilles `.txt` sont acceptées.

Exemple : augmenter les points de vie d'une unité.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Dans le tableur, faites passer `MaxHP` de la ligne `Stats/tank.xml` de 120 à 150, enregistrez-le en texte séparé par des tabulations, puis :

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` contient maintenant `MaxHP="150"` ; aucun autre octet n'a changé.

## bk-map : cartes, minicartes et modèles Blender

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` et `to-bzm` convertissent une carte entre le `.bzm` binaire du jeu et le `.xml` modifiable, avec le même lecteur et le même écrivain que l'éditeur de cartes.
- `minimap` dessine `<map>_c.dds`, `<map>_l.dds` et `<map>_h.dds`, carrés de `<px>`, les images que le jeu affiche pour une carte.
- Un répertoire est parcouru récursivement et reproduit sous `--out` ; sans `--out`, chaque sortie est écrite à côté de son entrée. Pour une carte qui a à la fois un `.bzm` et un `.xml`, `minimap` utilise le plus récent, comme le fait le jeu.
- Les commandes de carte ont besoin du `Data` du jeu (routes, rivières, objets et jeux de tuiles) : lancez-les depuis le dossier du jeu ou passez `--data`.
- `obj2mod` transforme l'OBJ de l'exporteur Blender et ses fichiers de squelette et d'animation en un `.mod` ; `validate-obj2mod` vérifie les mêmes fichiers et n'écrit rien. Les formats de fichier et le côté Blender sont décrits dans `docs/blender-replacement.md`.

Exemple : modifier une carte en XML, la reconvertir et dessiner sa minicarte.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Exemple : un modèle venant de Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite : composer des animations de sprites

Regroupe les images rendues d'un objet 2D (ponts, clôtures, bâtiments, effets) dans une seule texture `<out-name>.tga` et décrit ses animations dans `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` contient `anims.txt`, avec une `[section]` par animation, et les images : l'image `i` de l'animation `<name>` est `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). Une animation nommée `default` a l'unique image `<desc-dir>/1.tga`.

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

Les noms de section sont les emplacements d'animation du jeu (`idle`, `run`, `death`, ...) ; `default` et `effect` valent tous deux `idle`, n'en utilisez donc qu'un des trois. `<out-name>` est un chemin relatif à l'endroit où vous lancez l'outil ; son dossier est créé.

Exemple : un objet avec une boucle d'attente à deux directions et une mort à lecture unique.

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

## bk-image : convertir les tuiles de terrain

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` échange les deux moitiés en forme de losange d'une tuile de terrain, comme le faisait `OffsetRomb.exe`, pour faire passer l'art de tuiles dessiné dans l'ancienne disposition décalée à la disposition du jeu. Il lit les fichiers TGA en couleurs vraies et en niveaux de gris et écrit un TGA 32 bits. Sans `--out`, il écrase l'entrée. L'exécuter deux fois ne redonne pas l'original ; gardez votre fichier source.

Exemple :

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video : convertir les films Bink en Ogg Theora

Le jeu lit des films Ogg Theora `.ogv`. `bk-video convert` transforme chaque `.bik` d'un dossier en `<name>.ogv` à côté, et la première piste audio du film en un fichier annexe `<name>.audio.ogg`. Il exécute `ffmpeg` et `ffprobe`, que vous installez vous-même (toute version standard a le décodeur Bink, Theora et Vorbis) ; sans eux, rien n'est converti.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` vaut par défaut `Data/movies`. `--no-recurse` ne lit que ce dossier.
- `--what-if` liste les tâches et n'exécute rien. Les sorties existantes sont conservées sauf avec `--force`.
- `--ffmpeg` et `--ffprobe` désignent les programmes quand ils ne sont pas dans le `PATH`.
- `--key-frame-interval` est le `-g` de ffmpeg (1 par défaut, chaque image est une image clé).
- Un film que ffmpeg ne sait pas lire est signalé sur stderr ; les autres sont quand même convertis et l'outil se termine avec le code 1.

Exemple :

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
