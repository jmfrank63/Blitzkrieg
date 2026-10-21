[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Alat modding

Tujuh alat baris perintah untuk membuat mod Blitzkrieg. Alat-alat ini menggantikan alat tahun 2003 yang hanya berjalan di Windows, dan berjalan di Windows, macOS, dan Linux. Setiap alat menulis berkas yang dibaca game lewat pemuatnya sendiri; tidak ada yang membutuhkan instalasi game kecuali alat itu menerima `--data`.

Lokasinya: di direktori `Tools/` pada folder game. Di Windows folder itu berisi `bk-pak`, `bk-table`, `bk-image`, `bk-video`, dan `bk-font`; Linux dan macOS juga memiliki `bk-map` dan `bk-sprite`. Alat yang tidak ada di `Tools/` Anda dibangun dari kode sumber dengan `zig build bk-<name>` dan hasilnya ada di `zig-out/bin`.

## Aturan yang dipatuhi setiap alat

- `bk-<tool> --help` mencetak cara pakai dan keluar dengan kode 0. Baris perintah yang salah keluar dengan kode 2 dan cara pakai. Kegagalan keluar dengan kode 1 dan satu baris di stderr yang diawali `bk-<tool>: ` dan menyebut nama berkasnya.
- Jika berhasil, alat mencetak satu baris ringkasan dan tidak ada yang lain.
- Path menerima `/` atau `\`; path keluaran dicetak dengan pemisah sistem Anda.
- Alat hanya menulis keluaran yang Anda sebutkan (atau di samping masukannya bila disebutkan demikian). Alat tidak pernah menimpa keluaran yang sudah ada kecuali Anda memberikan `--force`.
- Keluarannya deterministik: masukan yang sama menghasilkan byte yang sama di setiap sistem.
- Alat yang membaca data game menerima `--data <dir>`, bawaannya `./Data`. Path di dalam peta atau mod adalah path Data milik game, bukan path di disk Anda.

Sebuah mod berada di `mods/<Name>/data/` di samping `Data/` milik game, dengan `mod.xml` yang menamainya (`MODName`, `MODVersion`), dan game menjalankannya dengan `-mod=<Name>`. Game membaca berkas di folder itu dan berkas di dalam setiap `.pak` di sana dengan cara yang sama.

## Bahasa

Bantuan dan pesan tersedia dalam sepuluh bahasa. Pilih satu dengan `--lang <code>` (misalnya `--lang=ja`), jika tidak, dengan variabel lingkungan `BK_LANG`, jika tidak, bahasa sistem Anda, jika tidak, bahasa Inggris. Awalan `usage: ` dan `bk-<tool>: `, kode keluar, dan baris ringkasan tidak berubah. Di Windows, jendela konsol lama mungkin tidak memiliki glif CJK; Windows Terminal menampilkannya.

## Alat lama, perintah baru

| Alat lama | Perintah baru |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | tidak ada; sertakan `.pak` atau zip biasa |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | eksportir Blender, lihat `docs/blender-replacement.md` |

## bk-pak: membuat dan mengekstrak arsip `.pak`

`.pak` adalah arsip zip. `bk-pak pack` menulisnya seperti pengemas rilis milik game sendiri: setiap berkas disimpan tanpa kompresi, nama diurutkan, pemisah `/`. `unpack` memeriksa seluruh arsip terlebih dahulu dan tidak menulis apa pun jika ada entri yang rusak atau akan mendarat di luar folder tujuan.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Nama di dalam arsip adalah path di bawah `<dir>`, jadi susun `<dir>` seperti folder `data` milik mod. Arsip Zip64 (lebih dari 65.535 entri atau 4 GiB) tidak didukung.

Contoh: mengemas unit mod yang diubah ke dalam mod.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: membuat font game dari font TrueType

Merender karakter 32..255 dari sebuah code page ke folder font seperti `Data/Fonts/small`: `1.tfd` (metrik, persegi panjang tekstur, jarak A/B/C, kerning), `1.tga`, dan `1_h.dds`, tekstur yang dibaca game.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` adalah tinggi sel dalam piksel, ascent ditambah descent (bawaan 20).
- `--charset` adalah `ansi` (code page 1252, bawaan), `easteurope` (1250), `russian` (1251), atau nomor code page.
- `--chars <file>` adalah berkas UTF-8: font mendapatkan setiap karakter berbeda dari Basic Multilingual Plane di dalamnya, ditambah ASCII 32..126; karakter yang tidak dimiliki face tersebut dilewati. `--set latin1`, `cyrillic`, atau `devanagari` menambahkan rentang bawaan dan dapat diulang.
- `--clusters <table>` adalah tabel klaster yang ditulis oleh ekspor language-mods (baris `U+E000<TAB>cluster`): setiap klaster menjadi satu glif di bawah karakter Private Use Area miliknya, dibentuk dengan face `--ttf`. `--script` (bawaan `Deva`) dan `--language` (bawaan `hi`) memilih pembentukannya.
- `--ttf` menerima berkas `.ttf`, `.otf`, atau `.ttc`. Nama face dicari di folder font sistem, sebisanya; berkas adalah cara yang andal.
- `--weight 600` atau lebih memilih face tebal atau menebalkannya; `--italic` memilih atau memiringkan. `--no-aa` mematikan anti-aliasing.
- Periksa lisensi font sebelum Anda menyertakannya dalam mod.

Contoh: font 16 piksel untuk sebuah mod, dari berkas font berlisensi terbuka.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: mengedit berkas statistik di spreadsheet

`export` meratakan setiap berkas XML statistik yang cocok dalam satu folder menjadi satu lembar yang dipisahkan tab: satu baris per berkas, satu kolom per bidang. Excel, LibreOffice, dan Numbers dapat membukanya. `import` menulis kembali sel yang Anda ubah ke dalam berkas, hanya mengubah byte nilai-nilai itu; komentar, urutan, dan format semua yang lain tetap seperti semula.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Nama kolom adalah path bidang: `MaxHP` adalah atribut, `KeyName;#text` teks sebuah elemen, `Guns;item(01);Ammo` entri kedua dari sebuah daftar.
- Statistik dibaca dari `<desc>` untuk `*.bld` dan `*.obt`, `<effect>` untuk `*.eff`, dan `<RPG>` untuk yang lain. Bangunan bawaan menyimpan statistiknya di `1.xml` di bawah `<desc>`, jadi ekspor dengan `--root desc`. Jenis statistik baru buatan Anda memakai `--root <name>`.
- `--only` mempertahankan dan `--ignore` membuang kolom yang diawali kata pertama pada setiap baris `<fields.txt>`.
- Saat impor, sel kosong atau `_` membiarkan nilainya. Berkas atau bidang yang hilang dilaporkan dan dilewati; `import` tidak pernah menambahkan apa pun.
- Kolom `FileName` relatif terhadap tempat Anda menjalankan `export`; jalankan `import` dari tempat yang sama. Lembar yang disimpan oleh spreadsheet (sel bertanda kutip, CRLF) dan lembar `.txt` lama diterima.

Contoh: menaikkan poin nyawa sebuah unit.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Ubah `MaxHP` pada baris `Stats/tank.xml` dari 120 menjadi 150 di spreadsheet, simpan sebagai teks yang dipisahkan tab, lalu:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` sekarang berisi `MaxHP="150"`; tidak ada byte lain yang berubah.

## bk-map: peta, minimap, dan model Blender

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` dan `to-bzm` mengonversi peta antara `.bzm` biner milik game dan `.xml` yang dapat diedit, dengan pembaca dan penulis yang sama seperti Editor Peta.
- `minimap` menggambar `<map>_c.dds`, `<map>_l.dds`, dan `<map>_h.dds`, persegi `<px>`, gambar yang ditampilkan game untuk sebuah peta.
- Direktori ditelusuri secara rekursif dan dicerminkan di bawah `--out`; tanpa `--out`, setiap keluaran ditulis di samping masukannya. Untuk peta yang memiliki `.bzm` sekaligus `.xml`, `minimap` memakai yang lebih baru, seperti yang dilakukan game.
- Perintah peta membutuhkan `Data` milik game (jalan, sungai, objek, dan set tile): jalankan dari folder game atau berikan `--data`.
- `obj2mod` mengubah OBJ dari eksportir Blender beserta berkas kerangka dan animasinya menjadi `.mod`; `validate-obj2mod` memeriksa berkas yang sama dan tidak menulis apa pun. Format berkas dan sisi Blender ada di `docs/blender-replacement.md`.

Contoh: mengedit peta sebagai XML, mengonversinya kembali, dan menggambar minimapnya.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Contoh: model dari Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: menyusun animasi sprite

Mengemas bingkai hasil render sebuah objek 2D (jembatan, pagar, bangunan, efek) ke dalam satu tekstur `<out-name>.tga` dan menjelaskan animasinya di `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` berisi `anims.txt`, satu `[section]` per animasi, dan bingkainya: bingkai `i` dari animasi `<name>` adalah `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). Animasi bernama `default` memiliki satu bingkai `<desc-dir>/1.tga`.

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

Nama section adalah slot animasi milik game (`idle`, `run`, `death`, ...); `default` dan `effect` keduanya adalah `idle`, jadi pakai hanya satu dari ketiganya. `<out-name>` adalah path dari tempat Anda menjalankan alat; foldernya dibuat.

Contoh: sebuah objek dengan putaran diam dua arah dan kematian sekali putar.

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

## bk-image: mengonversi seni tile medan

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` menukar dua bagian berbentuk belah ketupat dari sebuah tile medan, seperti yang dilakukan `OffsetRomb.exe`, untuk membawa seni tile yang digambar dalam tata letak offset lama ke tata letak game. Alat ini membaca berkas TGA warna asli dan skala abu-abu dan menulis TGA 32-bit. Tanpa `--out`, alat menimpa masukan. Menjalankannya dua kali tidak mengembalikan aslinya; simpan berkas sumber Anda.

Contoh:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: mengonversi film Bink ke Ogg Theora

Game memutar film Ogg Theora `.ogv`. `bk-video convert` mengubah setiap `.bik` di bawah sebuah folder menjadi `<name>.ogv` di sampingnya, dan trek audio pertama film itu menjadi berkas pendamping `<name>.audio.ogg`. Alat ini menjalankan `ffmpeg` dan `ffprobe`, yang Anda pasang sendiri (build standar apa pun memiliki dekoder Bink, Theora, dan Vorbis); tanpa keduanya tidak ada yang dikonversi.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` bawaannya `Data/movies`. `--no-recurse` hanya membaca folder itu.
- `--what-if` mendaftar pekerjaan dan tidak menjalankan apa pun. Keluaran yang sudah ada dipertahankan kecuali dengan `--force`.
- `--ffmpeg` dan `--ffprobe` menunjuk ke program tersebut bila tidak ada di `PATH`.
- `--key-frame-interval` adalah `-g` milik ffmpeg (bawaan 1, setiap bingkai adalah bingkai kunci).
- Film yang tidak dapat dibaca ffmpeg dilaporkan di stderr; sisanya tetap dikonversi dan alat keluar dengan kode 1.

Contoh:

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
