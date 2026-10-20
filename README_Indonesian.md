[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Repositori ini adalah proyek pribadi Johannes Maria Frank. Saya memakainya untuk bersenang-senang dan belajar, terutama untuk berlatih pemrograman berbasis agen pada aplikasi yang sudah ada (brownfield). README asli disimpan di `readme_original`.

Peringatan: proyek ini masih dalam pengerjaan.


# Yang bisa dilakukan repositori ini saat ini

- Berisi kode sumber lengkap mode pemain tunggal Blitzkrieg beserta data gamenya.

- Membangun seluruh game dengan `zig build` (Zig + clang) untuk enam target — Windows (MSVC dan MinGW), macOS (Intel dan Apple Silicon) dan Linux (x86_64 dan arm64) — menghasilkan tata letak instalasi yang bisa dijalankan di `zig-out/game/<os>/<arch>/<config>`. Hanya target MSVC yang membutuhkan Visual Studio; Zig membawa toolchain sendiri untuk yang lain.

- Dibangun dan dijalankan sebagai Game.exe **64-bit** native (`zig build install-game -Dtarget=x86_64-windows-msvc`); target 32-bit sudah dihentikan. Misi kampanye dan tutorial dapat dimainkan di x64, termasuk simpan/muat, video, dan suara.

- Dikirim dengan StreamIO.dll yang sepenuhnya diganti oleh implementasi Zig (`Sources/src/StreamIOZig`) — seluruh lapisan penyimpanan engine: graf objek biner untuk simpan/muat, pohon data XML, pabrik objek, variabel global, konsol, dan opsi.

- Mengompilasi C++ lama dengan clang dan UBSan aktif, yang telah menemukan dan memperbaiki puluhan bug laten yang diam-diam ditoleransi build MSVC tahun 2003 — termasuk crash pemanggilan virtual murni berusia 22 tahun pada pembongkaran tank/menara.

- Memvalidasi setiap target yang didukung di CI, menjalankan suite pengujian platform secara native di runner-nya sendiri — Linux x86_64 dan arm64, Windows MSVC dan MinGW, macOS Intel dan Apple Silicon — bukan hanya cross-compile.

- Masih dapat dibangun dengan bersih dari solusi asli `A7.sln` di bawah toolchain MSVC modern (`Debug | Win32`) sebagai acuan perilaku.

- Berjalan di jendela sungguhan secara bawaan, dengan `-fullscreen` tetap tersedia sebagai opsi baris perintah.

- Memakai submodul Git untuk pustaka yang hilang.

- Mendukung debugging C++ native dan WinDbg di VS Code Insiders, serta skrip penangkap crash tanpa antarmuka di `tools/zig/`.

- Menyediakan alur kerja bantu untuk mengemas aset film terkompresi di `VideoAssets/`.

- Telah menghapus penanganan crash BugSlay lama dan menggantinya dengan assert C++ standar.


# Konversi dan validasi aset Blender

Pipeline pengganti Maya40 didokumentasikan di `docs/blender-replacement.md`.

Perintah singkat:

- Konversi: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Hanya validasi: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Platform yang didukung

`zig build` menerima enam target. Berikan satu dengan `-Dtarget=`; pohon hasilnya ditata sebagai `zig-out/game/<os>/<arch>/<config>`, sehingga beberapa target dan kedua tingkat optimasi berdampingan tanpa saling menimpa.

| Platform | Triple target | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Dapat dimainkan |
| macOS (Apple Silicon) | `aarch64-macos` | Dapat dimainkan |
| macOS (Intel) | `x86_64-macos` | Terbangun; belum diuji main |
| Linux (x86_64) | `x86_64-linux-gnu` | Dalam pengerjaan |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Hanya lapisan platform |
| Windows (MinGW) | `x86_64-windows-gnu` | Hanya lapisan platform |

“Hanya lapisan platform” berarti target ini terbangun dan lulus suite pengujian platform, tetapi build game penuhnya belum dijalankan di sana.

Hanya `x86_64-windows-msvc` yang membutuhkan instalasi Visual Studio. Zig menyediakan toolchain, header, dan pustaka impor sendiri untuk setiap target lain, termasuk MinGW.

Setiap target diuji oleh workflow `Cross-platform validation` pada runner dengan arsitekturnya sendiri, sehingga suite benar-benar dijalankan dan tidak sekadar di-link.


# Menjalankan game dengan Zig (utama)

1. Klon repositori beserta submodul, atau jalankan `git submodule update --init --recursive` di checkout yang sudah ada.

2. Pasang Zig (0.17 atau lebih baru). Untuk `x86_64-windows-msvc` Anda juga memerlukan toolchain MSVC dan Windows SDK (path dapat diatur lewat `-Dmsvc-include`/`-Dwindows-sdk-include` dan padanan `lib`-nya jika berbeda dari bawaan di `build.zig`). Target lain tidak memerlukannya.

3. Jalankan `zig build install-game -Dtarget=<triple> --release=fast` untuk bermain, atau hilangkan `--release=fast` untuk build debug. Tanpa `-Dtarget`, build dibuat untuk host.

4. Jalankan biner hasil build — `zig-out/game/windows/x86_64/release/Game.exe` di Windows, `zig-out/game/macos/arm64/release/Game` di Apple Silicon (`.../debug/...` tanpa `--release=fast`). Pohon hasil dinamai menurut optimasi yang dimuatnya, jadi keduanya tidak pernah saling menimpa.

5. `zig build package` membuat paket zip yang dapat didistribusikan; `zig build test` menjalankan uji unit Zig dan uji asap ABI C++.


# Menjalankan game dengan Visual Studio 2026 Insiders

1. Klon repositori beserta submodul, atau jalankan `git submodule update --init --recursive` di checkout yang sudah ada.

2. Pasang Visual Studio 2026 Insiders / Visual Studio 18 dengan workload Desktop development with C++, toolchain MSVC VS 2026, dan Windows 10 atau Windows 11 SDK.

3. Buka `Sources/src/A7.sln`.

4. Pilih `Debug | Win32`.

5. Bangun proyek `Game`, atau bangun seluruh solusi.

6. Jalankan proyek `Game` dengan F5, atau jalankan `Sources/src/Game/Debug/Game.exe` secara langsung.

7. Game dimulai dalam mode jendela secara bawaan. Tambahkan `-fullscreen` ke argumen baris perintah proyek `Game` jika Anda ingin perilaku layar penuh yang lama.

Jika build tidak dapat menyalin DLL ke `Sources/src/Game/Debug`, tutup semua proses `Game.exe` yang berjalan lalu bangun lagi.


# Riwayat sejauh ini

- Menemukan pustaka yang hilang dan menambahkannya sebagai submodul Git.

- Membuat VM Windows XP SP3 dengan Visual Studio 6 untuk mempelajari lingkungan aslinya.

- VS 6 tidak stabil dan sering crash, sehingga upaya dialihkan ke perkakas modern.

- Memasang Visual Studio 2010 dan mengonversi proyek `.dwr` lama menjadi `.sln`.

- Memuat solusi ke VS 2026 Insiders dan memperbarui semua dependensi.

- Menyelesaikan dua minggu pemrograman berbasis agen agar solusi dapat dikompilasi dari kondisi bersih tanpa error dan peringatan.

- Debugging saat runtime kini sudah melewati menu awal: tutorial termuat dan dapat dimainkan, termasuk memilih dan menggerakkan tank.

- Memindahkan pengembangan ke VS Code Insiders demi perkakas yang lebih baik.

- Menyiapkan dua jalur debug: C++ native dan WinDbg.

- Menghapus BugSlay karena mempersulit debugging dan pada satu kasus crash setelah sebuah crash.

- Mengganti BugSlay dengan assert C++ standar yang sederhana.

- Tanpa BugSlay, proyek kini siap untuk debugging runtime yang terfokus hingga game berjalan penuh.

- Memindahkan build ke Zig: `build.zig` mengompilasi setiap proyek dengan clang, me-link ke toolchain MSVC, dan menyusun tata letak game yang bisa dijalankan.

- Mengganti StreamIO.dll — lapisan penyimpanan dan layanan engine — dengan implementasi Zig plus jembatan ABI C++, sekaligus memperbaiki format graf objek simpan/muat, baca/tulis XML, opsi, dan regresi performa per frame.

- Membuat game dapat dimainkan dari awal sampai akhir di bawah build Zig: kampanye, tutorial, boarding, penerbangan, siklus simpan/muat, video yang sinkron, dan minimap yang benar pada resolusi modern.

- Memporting game ke 64-bit: memperbaiki pemotongan pointer di renderer, panggilan `_control87` era x87, `#pragma pack(1)` pada tipe STL, inline assembly x86 (diganti intrinsik SSE portabel), kanal pointer unit Lua, dan ID objek format simpan.

- Menguji build x64 melalui tutorial crash demi crash, dengan UBSan mengubah setiap perilaku tak terdefinisi berusia 20 tahun menjadi file dan baris yang tepat: koordinat minimap, hashing formasi, perhitungan sudut pengeboman, dan lainnya.

- Beralih dari Win32 ke lapisan platform portabel: SDL3 untuk jendela, input, dan audio, renderer SDL GPU sebagai pengganti jalur DirectX, dan adaptor native kecil untuk tiap sistem operasi.

- Menjalankan game di macOS, tempat game dapat dimainkan di Apple Silicon, dengan Linux menyusul.

- Menambahkan Linux arm64 (kelas Raspberry Pi) dan ABI GNU Windows (MinGW) sebagai target, yang mengharuskan memisahkan “apakah ini Windows?” dari “apakah ini Visual Studio?” di seluruh build. Hal itu menyingkap bahwa macOS Intel selama ini dikompilasi tanpa satu pun adaptor Cocoa-nya, bahwa job Linux merah karena tidak ada runtime C, dan bahwa jalur C++ Linux terkunci pada triple multiarch x86_64.

- Menempatkan keenam target di bawah CI, masing-masing pada runner dengan arsitekturnya sendiri.


# Peta jalan

1. Menyelesaikan stabilisasi build 64-bit melalui tutorial dan kampanye yang tersisa.

2. Menjalankan build game penuh di Linux arm64 dan MinGW — saat ini hanya lapisan platform keduanya yang tervalidasi — dan menguji main macOS Intel.

3. Membuat utilitas untuk mengonversi simpanan game 32-bit ke format 64-bit.

4. Mengganti FMOD, Stingray, dan Bink dengan alternatif sumber terbuka.

5. Terus mengganti proyek C++ satu per satu dengan kode Zig, memakai port StreamIO sebagai templat.
