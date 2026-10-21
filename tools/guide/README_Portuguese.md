[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Ferramentas de modding

Sete ferramentas de linha de comando para criar mods de Blitzkrieg. Elas substituem as ferramentas de 2003, que só funcionavam no Windows, e rodam no Windows, no macOS e no Linux. Cada uma grava arquivos que o jogo lê pelos seus próprios carregadores; nenhuma precisa de uma instalação do jogo, a menos que aceite `--data`.

Onde ficam: no diretório `Tools/` da pasta do jogo. No Windows essa pasta tem `bk-pak`, `bk-table`, `bk-image`, `bk-video` e `bk-font`; Linux e macOS têm também `bk-map` e `bk-sprite`. Uma ferramenta que falte no seu `Tools/` é compilada a partir do código-fonte com `zig build bk-<name>` e vai para `zig-out/bin`.

## Regras que toda ferramenta segue

- `bk-<tool> --help` imprime o uso e termina com o código 0. Uma linha de comando errada termina com o código 2 e o uso. Uma falha termina com o código 1 e uma linha no stderr que começa com `bk-<tool>: ` e nomeia o arquivo.
- Em caso de sucesso, a ferramenta imprime uma linha de resumo e mais nada.
- Os caminhos aceitam `/` ou `\`; os caminhos de saída são impressos com o separador do seu sistema.
- Uma ferramenta grava apenas as saídas que você indicar (ou ao lado da entrada, quando isso for dito). Ela nunca substitui uma saída existente, a menos que você passe `--force`.
- A saída é determinística: a mesma entrada gera os mesmos bytes em qualquer sistema.
- As ferramentas que leem dados do jogo aceitam `--data <dir>`, por padrão `./Data`. Os caminhos em um mapa ou em um mod são os caminhos Data do jogo, não os do seu disco.

Um mod fica em `mods/<Name>/data/`, ao lado do `Data/` do próprio jogo, com um `mod.xml` que o nomeia (`MODName`, `MODVersion`), e o jogo o executa com `-mod=<Name>`. O jogo lê da mesma forma os arquivos dessa pasta e os arquivos dentro de cada `.pak` que estiver lá.

## Idioma

A ajuda e as mensagens existem em dez idiomas. Escolha um com `--lang <code>` (por exemplo `--lang=ja`); se não, pela variável de ambiente `BK_LANG`; se não, o idioma do seu sistema; se não, inglês. Os prefixos `usage: ` e `bk-<tool>: `, os códigos de saída e as linhas de resumo não mudam. No Windows, a janela de console antiga pode não ter glifos CJK; o Windows Terminal os mostra.

## Ferramenta antiga, comando novo

| Ferramenta antiga | Comando novo |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | nenhum; distribua o `.pak` ou um zip comum |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | o exportador do Blender, veja `docs/blender-replacement.md` |

## bk-pak: criar e extrair arquivos `.pak`

Um `.pak` é um arquivo zip. `bk-pak pack` grava um do mesmo modo que o empacotador de versões do próprio jogo: todos os arquivos armazenados sem compressão, nomes ordenados, separadores `/`. `unpack` verifica primeiro o arquivo inteiro e não grava nada se alguma entrada estiver danificada ou fosse parar fora da pasta de destino.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Os nomes dentro do arquivo são os caminhos abaixo de `<dir>`; portanto, organize `<dir>` como a pasta `data` do mod. Arquivos Zip64 (mais de 65.535 entradas ou 4 GiB) não são suportados.

Exemplo: empacotar no mod uma unidade alterada do mod.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: criar uma fonte do jogo a partir de uma fonte TrueType

Renderiza os caracteres 32..255 de uma página de código em uma pasta de fonte como `Data/Fonts/small`: `1.tfd` (métricas, retângulos de textura, espaçamento A/B/C, kerning), `1.tga` e `1_h.dds`, a textura que o jogo lê.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` é a altura da célula em pixels, ascendente mais descendente (padrão 20).
- `--charset` é `ansi` (página de código 1252, o padrão), `easteurope` (1250), `russian` (1251) ou um número de página de código.
- `--chars <file>` é um arquivo UTF-8: a fonte recebe todos os caracteres distintos do Plano Multilíngue Básico que ele contiver, mais ASCII 32..126; os caracteres que a fonte não tem são deixados de fora. `--set latin1`, `cyrillic` ou `devanagari` adiciona um intervalo embutido e pode ser repetido.
- `--clusters <table>` é a tabela de clusters que a exportação do language-mods gravou (linhas `U+E000<TAB>cluster`): cada cluster vira um glifo sob o seu caractere da Área de Uso Privado, moldado com a fonte `--ttf`. `--script` (padrão `Deva`) e `--language` (padrão `hi`) escolhem a modelagem.
- `--ttf` aceita um arquivo `.ttf`, `.otf` ou `.ttc`. Um nome de fonte é procurado nas pastas de fontes do sistema, sem garantia; um arquivo é o jeito confiável.
- `--weight 600` ou mais escolhe a face em negrito ou põe uma em negrito; `--italic` escolhe ou inclina. `--no-aa` desliga o suavizamento.
- Verifique a licença da fonte antes de distribuí-la em um mod.

Exemplo: uma fonte de 16 pixels para um mod, a partir de um arquivo de fonte de licença aberta.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: editar arquivos de atributos em uma planilha

`export` reúne todos os arquivos XML de atributos correspondentes de uma pasta em uma única planilha separada por tabulações: uma linha por arquivo, uma coluna por campo. Excel, LibreOffice e Numbers a abrem. `import` grava de volta nos arquivos as células que você alterou, mudando apenas os bytes desses valores; comentários, ordem e formatação de todo o resto permanecem como estavam.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Os nomes das colunas são caminhos de campo: `MaxHP` é um atributo, `KeyName;#text` o texto de um elemento, `Guns;item(01);Ammo` a segunda entrada de uma lista.
- Os atributos são lidos de `<desc>` em `*.bld` e `*.obt`, de `<effect>` em `*.eff` e de `<RPG>` nos demais. Os edifícios distribuídos guardam os seus em `1.xml` sob `<desc>`; por isso, exporte-os com `--root desc`. Um novo tipo de atributo seu usa `--root <name>`.
- `--only` mantém e `--ignore` descarta as colunas que começam com a primeira palavra de cada linha de `<fields.txt>`.
- Na importação, uma célula vazia ou `_` deixa o valor como está. Um arquivo ou campo ausente é informado e ignorado; `import` nunca acrescenta nada.
- A coluna `FileName` é relativa ao local em que você executou `export`; execute `import` a partir do mesmo local. Planilhas salvas por um programa de planilhas (células entre aspas, CRLF) e planilhas `.txt` antigas são aceitas.

Exemplo: aumentar os pontos de vida de uma unidade.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Mude `MaxHP` da linha `Stats/tank.xml` de 120 para 150 na planilha, salve-a como texto separado por tabulações e então:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` agora tem `MaxHP="150"`; nenhum outro byte mudou.

## bk-map: mapas, minimapas e modelos do Blender

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` e `to-bzm` convertem um mapa entre o `.bzm` binário do jogo e o `.xml` editável, com o mesmo leitor e gravador do Editor de Mapas.
- `minimap` desenha `<map>_c.dds`, `<map>_l.dds` e `<map>_h.dds`, quadrados de `<px>`, as imagens que o jogo mostra para um mapa.
- Um diretório é percorrido recursivamente e espelhado sob `--out`; sem `--out`, cada saída vai ao lado da sua entrada. Para um mapa que tem `.bzm` e `.xml`, `minimap` usa o mais recente, como o jogo faz.
- Os comandos de mapa precisam do `Data` do jogo (estradas, rios, objetos e conjuntos de tiles): execute-os a partir da pasta do jogo ou passe `--data`.
- `obj2mod` transforma o OBJ do exportador do Blender e os seus arquivos de esqueleto e animação em um `.mod`; `validate-obj2mod` verifica os mesmos arquivos e não grava nada. Os formatos de arquivo e o lado do Blender estão em `docs/blender-replacement.md`.

Exemplo: editar um mapa como XML, convertê-lo de volta e desenhar o seu minimapa.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Exemplo: um modelo do Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: compor animações de sprites

Reúne os quadros renderizados de um objeto 2D (pontes, cercas, edifícios, efeitos) em uma única textura `<out-name>.tga` e descreve as suas animações em `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` contém `anims.txt`, com uma `[section]` por animação, e os quadros: o quadro `i` da animação `<name>` é `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). Uma animação chamada `default` tem o único quadro `<desc-dir>/1.tga`.

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

Os nomes de seção são os slots de animação do jogo (`idle`, `run`, `death`, ...); `default` e `effect` são ambos `idle`, então use apenas um dos três. `<out-name>` é um caminho a partir do local em que você executa a ferramenta; a sua pasta é criada.

Exemplo: um objeto com um ciclo de repouso em duas direções e uma morte de execução única.

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

## bk-image: converter a arte de tiles de terreno

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` troca as duas metades em forma de losango de um tile de terreno, como fazia o `OffsetRomb.exe`, para levar a arte de tiles desenhada no antigo layout deslocado para o layout do jogo. Ele lê arquivos TGA de cores reais e de tons de cinza e grava um TGA de 32 bits. Sem `--out`, sobrescreve a entrada. Executá-lo duas vezes não devolve o original; guarde o seu arquivo de origem.

Exemplo:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: converter vídeos Bink para Ogg Theora

O jogo reproduz vídeos Ogg Theora `.ogv`. `bk-video convert` transforma cada `.bik` sob uma pasta em `<name>.ogv` ao lado dele, e a primeira faixa de áudio do vídeo em um arquivo auxiliar `<name>.audio.ogg`. Ele executa `ffmpeg` e `ffprobe`, que você mesmo instala (qualquer build comum tem o decodificador Bink, Theora e Vorbis); sem eles, nada é convertido.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` é por padrão `Data/movies`. `--no-recurse` lê apenas essa pasta.
- `--what-if` lista as tarefas e não executa nada. As saídas existentes são mantidas, a menos que se use `--force`.
- `--ffmpeg` e `--ffprobe` apontam para os programas quando eles não estão no `PATH`.
- `--key-frame-interval` é o `-g` do ffmpeg (padrão 1, todo quadro é um quadro-chave).
- Um vídeo que o ffmpeg não consegue ler é informado no stderr; os demais ainda são convertidos e a ferramenta termina com o código 1.

Exemplo:

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
