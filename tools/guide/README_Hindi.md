[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# मॉडिंग टूल

Blitzkrieg के मॉड बनाने के लिए सात कमांड-लाइन टूल। ये 2003 के केवल Windows पर चलने वाले टूलों की जगह लेते हैं और Windows, macOS तथा Linux पर चलते हैं। यहाँ का हर टूल ऐसी फ़ाइलें लिखता है जिन्हें गेम अपने ही लोडरों से पढ़ता है; जो टूल `--data` लेता है, उसके अलावा किसी को भी गेम इंस्टॉल की ज़रूरत नहीं होती।

ये कहाँ हैं: गेम फ़ोल्डर की `Tools/` डायरेक्टरी में। Windows पर उस फ़ोल्डर में `bk-pak`, `bk-table`, `bk-image`, `bk-video` और `bk-font` हैं; Linux और macOS पर `bk-map` और `bk-sprite` भी हैं। आपके `Tools/` में जो टूल न हो, उसे सोर्स से `zig build bk-<name>` द्वारा बनाया जाता है और वह `zig-out/bin` में आता है।

## हर टूल के नियम

- `bk-<tool> --help` उपयोग-विधि छापता है और 0 के साथ बाहर निकलता है। ग़लत कमांड लाइन उपयोग-विधि के साथ 2 पर बाहर निकलती है। विफलता 1 पर बाहर निकलती है और stderr पर एक पंक्ति देती है जो `bk-<tool>: ` से शुरू होती है और फ़ाइल का नाम बताती है।
- सफल होने पर टूल एक सारांश पंक्ति छापता है और उसके अलावा कुछ नहीं।
- पथों में `/` या `\` चलता है; आउटपुट पथ आपके सिस्टम के विभाजक के साथ छापे जाते हैं।
- टूल केवल वही आउटपुट लिखता है जिनका आपने नाम दिया (या जहाँ ऐसा कहा गया हो, अपने इनपुट के बगल में)। `--force` दिए बिना वह कभी किसी मौजूदा आउटपुट को नहीं बदलता।
- आउटपुट निर्धारित (deterministic) है: एक ही इनपुट हर सिस्टम पर वही बाइट देता है।
- गेम का डेटा पढ़ने वाले टूल `--data <dir>` लेते हैं, डिफ़ॉल्ट `./Data`। किसी मैप या मॉड के पथ गेम के Data पथ होते हैं, आपकी डिस्क के नहीं।

एक मॉड गेम के अपने `Data/` के बगल में `mods/<Name>/data/` में रहता है, जिसके साथ उसका नाम बताने वाली `mod.xml` (`MODName`, `MODVersion`) होती है, और गेम उसे `-mod=<Name>` से चलाता है। गेम उस फ़ोल्डर की फ़ाइलों और वहाँ हर `.pak` के भीतर की फ़ाइलों को समान रूप से पढ़ता है।

## भाषा

सहायता और संदेश दस भाषाओं में आते हैं। `--lang <code>` से भाषा चुनें (उदाहरण के लिए `--lang=ja`), नहीं तो पर्यावरण चर `BK_LANG`, नहीं तो आपके सिस्टम की भाषा, नहीं तो अंग्रेज़ी। `usage: ` और `bk-<tool>: ` उपसर्ग, निकास कोड और सारांश पंक्तियाँ नहीं बदलतीं। Windows पर पुरानी कंसोल विंडो में CJK ग्लिफ़ न हों, यह संभव है; Windows Terminal उन्हें दिखाता है।

## पुराना टूल, नया कमांड

| पुराना टूल | नया कमांड |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | कोई नहीं; `.pak` या सादी zip भेजें |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | Blender एक्सपोर्टर, देखें `docs/blender-replacement.md` |

## bk-pak: `.pak` संग्रह बनाएँ और खोलें

`.pak` एक zip संग्रह है। `bk-pak pack` उसे उसी तरह लिखता है जैसे गेम का अपना रिलीज़ पैकेजर: हर फ़ाइल बिना संपीड़न के संग्रहीत, नाम क्रमबद्ध, विभाजक `/`। `unpack` पहले पूरे संग्रह की जाँच करता है और यदि कोई प्रविष्टि क्षतिग्रस्त हो या लक्ष्य फ़ोल्डर के बाहर पहुँचे, तो कुछ नहीं लिखता।

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

संग्रह के भीतर के नाम `<dir>` के नीचे के पथ हैं, इसलिए `<dir>` को मॉड के `data` फ़ोल्डर की तरह सजाएँ। Zip64 संग्रह (65,535 से अधिक प्रविष्टियाँ या 4 GiB) समर्थित नहीं हैं।

उदाहरण: मॉड की बदली हुई यूनिट को मॉड में पैक करें।

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: TrueType फ़ॉन्ट से गेम फ़ॉन्ट बनाएँ

किसी कोड पेज के वर्ण 32..255 को `Data/Fonts/small` जैसे फ़ॉन्ट फ़ोल्डर में रेंडर करता है: `1.tfd` (मेट्रिक्स, टेक्सचर आयत, A/B/C स्पेसिंग, कर्निंग), `1.tga` और `1_h.dds`, वह टेक्सचर जिसे गेम पढ़ता है।

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` पिक्सेल में सेल की ऊँचाई है, ascent और descent का योग (डिफ़ॉल्ट 20)।
- `--charset` है `ansi` (कोड पेज 1252, डिफ़ॉल्ट), `easteurope` (1250), `russian` (1251), या कोई कोड पेज संख्या।
- `--chars <file>` एक UTF-8 फ़ाइल है: फ़ॉन्ट में उसका हर अलग Basic Multilingual Plane वर्ण और साथ में ASCII 32..126 आते हैं; जो वर्ण फ़ेस में नहीं हैं, वे छोड़ दिए जाते हैं। `--set latin1`, `cyrillic` या `devanagari` एक अंतर्निहित रेंज जोड़ता है और दोहराया जा सकता है।
- `--clusters <table>` वह क्लस्टर तालिका है जिसे language-mods एक्सपोर्ट ने लिखा (`U+E000<TAB>cluster` पंक्तियाँ): हर क्लस्टर अपने Private Use Area वर्ण के अंतर्गत एक ग्लिफ़ बनता है, जिसे `--ttf` फ़ेस से शेप किया जाता है। `--script` (डिफ़ॉल्ट `Deva`) और `--language` (डिफ़ॉल्ट `hi`) शेपिंग चुनते हैं।
- `--ttf` एक `.ttf`, `.otf` या `.ttc` फ़ाइल लेता है। फ़ेस का नाम सिस्टम फ़ॉन्ट फ़ोल्डरों में यथासंभव खोजा जाता है; फ़ाइल देना ही भरोसेमंद तरीका है।
- `--weight 600` या अधिक बोल्ड फ़ेस चुनता है या किसी को गाढ़ा करता है; `--italic` इटैलिक चुनता है या तिरछा करता है। `--no-aa` एंटी-एलियासिंग बंद करता है।
- मॉड में फ़ॉन्ट भेजने से पहले उसका लाइसेंस जाँच लें।

उदाहरण: खुले लाइसेंस वाली फ़ॉन्ट फ़ाइल से मॉड के लिए 16 पिक्सेल का फ़ॉन्ट।

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: स्प्रेडशीट में स्टैट फ़ाइलें संपादित करें

`export` किसी फ़ोल्डर की हर मेल खाती XML स्टैट फ़ाइल को टैब-विभाजित एक शीट में समतल करता है: हर फ़ाइल की एक पंक्ति, हर फ़ील्ड का एक स्तंभ। Excel, LibreOffice और Numbers इसे खोलते हैं। `import` आपके बदले हुए सेल को वापस फ़ाइलों में लिखता है और केवल उन मानों के बाइट बदलता है; बाक़ी सब की टिप्पणियाँ, क्रम और फ़ॉर्मेटिंग जैसी थीं वैसी रहती हैं।

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- स्तंभों के नाम फ़ील्ड पथ हैं: `MaxHP` एक एट्रिब्यूट है, `KeyName;#text` किसी एलिमेंट का टेक्स्ट है, `Guns;item(01);Ammo` सूची की दूसरी प्रविष्टि है।
- स्टैट `*.bld` और `*.obt` के लिए `<desc>` से, `*.eff` के लिए `<effect>` से और बाक़ी के लिए `<RPG>` से पढ़े जाते हैं। गेम के साथ आने वाली इमारतें अपने स्टैट `1.xml` में `<desc>` के नीचे रखती हैं, इसलिए उन्हें `--root desc` के साथ एक्सपोर्ट करें। आपके अपने नए स्टैट प्रकार के लिए `--root <name>` लगता है।
- `--only` उन स्तंभों को रखता है और `--ignore` उन्हें हटाता है जो `<fields.txt>` की हर पंक्ति के पहले शब्द से शुरू होते हैं।
- इंपोर्ट पर खाली सेल या `_` मान को वैसा ही छोड़ देता है। कोई ग़ायब फ़ाइल या फ़ील्ड रिपोर्ट करके छोड़ दिया जाता है; इंपोर्ट कभी कुछ नहीं जोड़ता।
- `FileName` स्तंभ उस जगह के सापेक्ष है जहाँ आपने `export` चलाया; `import` भी वहीं से चलाएँ। स्प्रेडशीट से सहेजी गई शीटें (उद्धरण-चिह्नों वाले सेल, CRLF) और पुरानी `.txt` शीटें स्वीकार की जाती हैं।

उदाहरण: एक यूनिट के हिट पॉइंट बढ़ाएँ।

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

स्प्रेडशीट में `Stats/tank.xml` पंक्ति का `MaxHP` 120 से 150 करें, उसे टैब-विभाजित टेक्स्ट के रूप में सहेजें, फिर:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` में अब `MaxHP="150"` है; कोई और बाइट नहीं बदला।

## bk-map: मैप, मिनीमैप और Blender मॉडल

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` और `to-bzm` किसी मैप को गेम के बाइनरी `.bzm` और संपादन योग्य `.xml` के बीच बदलते हैं, उसी रीडर और राइटर से जो मैप एडिटर में है।
- `minimap` `<map>_c.dds`, `<map>_l.dds` और `<map>_h.dds` बनाता है, `<px>` वर्गाकार, यानी वे चित्र जो गेम किसी मैप के लिए दिखाता है।
- डायरेक्टरी में पुनरावर्ती खोज होती है और `--out` के नीचे उसकी प्रतिकृति बनती है; `--out` के बिना हर आउटपुट अपने इनपुट के बगल में जाता है। जिस मैप में `.bzm` और `.xml` दोनों हों, उसके लिए `minimap` नया वाला लेता है, जैसा गेम करता है।
- मैप कमांड को गेम का `Data` चाहिए (सड़कें, नदियाँ, ऑब्जेक्ट और टाइलसेट): उन्हें गेम फ़ोल्डर से चलाएँ या `--data` दें।
- `obj2mod` Blender एक्सपोर्टर के OBJ और उसकी स्केलेटन तथा एनीमेशन फ़ाइलों को `.mod` में बदलता है; `validate-obj2mod` उन्हीं फ़ाइलों की जाँच करता है और कुछ नहीं लिखता। फ़ाइल प्रारूप और Blender वाला पक्ष `docs/blender-replacement.md` में हैं।

उदाहरण: मैप को XML के रूप में संपादित करें, वापस बदलें और उसका मिनीमैप बनाएँ।

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

उदाहरण: Blender से एक मॉडल।

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: स्प्राइट एनीमेशन बनाएँ

किसी 2D ऑब्जेक्ट (पुल, बाड़, इमारतें, इफ़ेक्ट) के रेंडर किए गए फ़्रेमों को एक टेक्सचर `<out-name>.tga` में पैक करता है और उसके एनीमेशन `<out-name>.san` में वर्णित करता है।

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` में `anims.txt` होती है, हर एनीमेशन के लिए एक `[section]`, और फ़्रेम: एनीमेशन `<name>` का फ़्रेम `i` है `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...)। `default` नाम के एनीमेशन का एक ही फ़्रेम है `<desc-dir>/1.tga`।

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

सेक्शन के नाम गेम के एनीमेशन स्लॉट हैं (`idle`, `run`, `death`, ...); `default` और `effect` दोनों `idle` हैं, इसलिए तीनों में से केवल एक का उपयोग करें। `<out-name>` आपके टूल चलाने की जगह से एक पथ है; उसका फ़ोल्डर बनाया जाता है।

उदाहरण: दो दिशाओं वाले आइडल लूप और एक बार चलने वाले डेथ एनीमेशन के साथ एक ऑब्जेक्ट।

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

## bk-image: भू-भाग टाइल कला बदलें

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` भू-भाग टाइल के दो समचतुर्भुज आधे हिस्सों को आपस में बदलता है, जैसा `OffsetRomb.exe` करता था, ताकि पुराने ऑफ़सेट लेआउट में बनी टाइल कला को गेम के लेआउट में लाया जा सके। यह ट्रू-कलर और ग्रेस्केल TGA फ़ाइलें पढ़ता है और 32-बिट TGA लिखता है। `--out` के बिना यह इनपुट को ओवरराइट करता है। इसे दो बार चलाने से मूल वापस नहीं मिलता; अपनी स्रोत फ़ाइल सँभालकर रखें।

उदाहरण:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: Bink मूवी को Ogg Theora में बदलें

गेम Ogg Theora `.ogv` मूवी चलाता है। `bk-video convert` किसी फ़ोल्डर के नीचे की हर `.bik` को उसके बगल में `<name>.ogv` में बदलता है, और मूवी के पहले ऑडियो ट्रैक को `<name>.audio.ogg` साइडकार में। यह `ffmpeg` और `ffprobe` चलाता है, जिन्हें आप ख़ुद इंस्टॉल करते हैं (कोई भी सामान्य बिल्ड Bink डिकोडर, Theora और Vorbis के साथ आती है); इनके बिना कुछ नहीं बदलता।

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` का डिफ़ॉल्ट `Data/movies` है। `--no-recurse` केवल उसी फ़ोल्डर को पढ़ता है।
- `--what-if` कामों की सूची देता है और कुछ नहीं चलाता। मौजूदा आउटपुट `--force` के बिना रखे जाते हैं।
- जब प्रोग्राम `PATH` में न हों, तब `--ffmpeg` और `--ffprobe` उनकी ओर इशारा करते हैं।
- `--key-frame-interval` ffmpeg का `-g` है (डिफ़ॉल्ट 1, हर फ़्रेम एक की फ़्रेम)।
- जिस मूवी को ffmpeg पढ़ न सके, उसकी रिपोर्ट stderr पर होती है; बाक़ी फिर भी बदलती हैं और टूल 1 पर बाहर निकलता है।

उदाहरण:

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
