[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Repositori ini adalah proyek pribadi Johannes Maria Frank. Saya memakainya untuk bersenang-senang dan belajar, terutama untuk berlatih pemrograman berbasis agen pada aplikasi yang sudah ada (brownfield). README asli disimpan di `readme_original`.

Peringatan: proyek ini masih dalam pengerjaan.


# Posisi proyek saat ini (November 2026)

Port-nya sendiri sebagian besar sudah selesai: game berjalan secara native di Windows, macOS, dan Linux, tersedia dalam sepuluh bahasa, dan semua editor sudah diport. Saat ini proyek macet di karya seni, yaitu tampilan putar dari bangunan dan properti. Lihat "Tembok penghalang: karya seni" di bawah.


# Yang sudah dikerjakan

**Game**

- Seluruh game pemain tunggal beserta datanya ada di sini, dibangun dengan `zig build` (Zig + clang) sebagai program 64-bit native. Kampanye, tutorial, dan misi acak dapat dimainkan, termasuk simpan/muat, video, dan suara.

- Engine dipindahkan dari Win32 dan DirectX ke lapisan yang portabel: SDL3 untuk jendela, input, dan audio, serta renderer SDL GPU.

- Pustaka tertutup sudah hilang: FMOD, Bink, dan Stingray telah dihapus. Suara dan film kini memakai Ogg Vorbis dan Ogg Theora.

- StreamIO.dll, lapisan penyimpanan engine, diganti dengan implementasi Zig (`Sources/src/StreamIOZig`).

- C++ lama dikompilasi dengan clang dan UBSan, yang telah menemukan dan memperbaiki puluhan bug yang diam-diam ditoleransi build tahun 2003.

- Misi acak yang asli dihidupkan kembali, dan setiap bab kampanye membuka misi historisnya sesuai maksud skripnya.

- Profil pemain dapat berpindah antar mesin lewat sinkronisasi cloud.

- Multipemain LAN berfungsi. Kode GameSpy sudah dihapus, dan permainan lewat internet belum tersambung.

**Sepuluh bahasa**

- Game, semua editor, alat baris perintah beserta panduannya, dan README ini tersedia dalam bahasa Inggris, Rusia, Tionghoa (Sederhana), Hindi, Spanyol, Prancis, Jerman, Portugis (Brasil), Jepang, dan Indonesia.

- Kesembilan terjemahan dipasang sebagai mod bahasa, dengan mesin teks TrueType baru yang menggambar semua aksara ini.

- Terjemahannya dibuat oleh mesin dan masih perlu ditinjau oleh penutur asli. Koreksi sangat disambut.

**Semua editor**

- Map Editor, Resource Editor beserta seluruh sub-editornya, dan ELK, perangkat lokalisasi, ditulis ulang sebagai aplikasi Zig dengan Dear ImGui (`Sources/editor`). Semuanya berbicara dengan engine C++ lewat C ABI (`Sources/src/EditorBridge`).

- Editor MFC lama yang hanya untuk Windows sudah dihapus. Game membaca apa yang ditulis editor baru, dan pengujian memeriksanya dengan pembaca milik game sendiri.

- Alat baris perintah lama diganti dengan tujuh alat portabel untuk membuat mod: `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map`, dan `bk-sprite`. Lihat `tools/guide/README.md`.

**Opsi Remastered**

- Options > Settings > Gameplay > Remastered nonaktif secara bawaan, dan saat nonaktif game memakai aturan aslinya. Saat aktif, ia menambahkan fitur yang tidak ada di game tahun 2003, seperti memutar kamera dalam empat langkah, pencari rute tercepat, dan memindahkan infanteri langsung dari satu wadah ke wadah lain.

- Replay misi sedang dibuat; fondasinya sudah digabungkan.


# Tembok penghalang: karya seni

Memutar kamera berarti setiap bangunan dan properti harus ditampilkan dari empat sisi, tetapi karya seni tahun 2003 hanyalah kumpulan gambar tiap objek dari satu sisi saja. Sisi lainnya tidak pernah digambar.

Sejauh ini saya sudah mencoba dua pendekatan untuk ke-412 objek:

- Menyusun setiap gambar yang diputar dari piksel tahun 2003 (ditinggalkan pada 2026-10-29: setiap masalah yang selesai melahirkan masalah baru).

- Merender model 3D sederhana yang dicat dari gambar-gambar yang sama. Inilah yang dipakai proyek saat ini. Sisi yang diperlihatkan karya seni 2003 tampak benar, tetapi setiap bagian yang tidak pernah terlihat harus ditebak. Detailnya juga gagal: jendela atap yang dicat pada atap datar tidak mungkin tampak benar dari sisi lain.

Keduanya belum cukup baik. Cara yang benar adalah model 3D sungguhan untuk setiap objek, dengan detail seperti jendela atap sebagai geometri nyata dan tekstur pada setiap bidang datar, sehingga setiap sisi mengikuti dari bentuknya.

Pertanyaannya adalah dari mana model dan tekstur itu berasal:

- Memodelkannya sendiri dengan tangan atau dengan agen, sesuai rancangan tahun 2003.

- Model Blitzkrieg 2, yang juga diterbitkan oleh Nival. Render uji menunjukkan bahwa model itu dibuat dengan cara ini dan tampak bersih di keempat putaran, tetapi banyak di antaranya adalah bangunan yang berbeda dari bangunan tahun 2003 yang bernama sama, dan lisensinya tidak jelas mencakup karya seninya.

- Generator gambar-ke-3D, yang belum saya coba pada karya seni ini.

Belum ada keputusan. Jika Anda seniman 3D atau mengenal pekerjaan semacam ini, saya ingin mendengar kabar dari Anda.


# Konversi dan validasi aset Blender

Pipeline pengganti Maya40 didokumentasikan di `docs/blender-replacement.md`.

Perintah singkat:

- Konversi: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Hanya validasi: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Platform yang didukung

Berikan target dengan `-Dtarget=`. Pohon hasilnya ditata sebagai `zig-out/game/<os>/<arch>/<config>`, sehingga beberapa target dan kedua tingkat optimasi berdampingan.

| Platform | Triple target | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Game dan editor berjalan; diuji di CI |
| macOS (Apple Silicon) | `aarch64-macos` | Game dan editor berjalan; diuji di CI |
| Linux (x86_64) | `x86_64-linux-gnu` | Game dan editor berjalan; diuji di CI pada renderer perangkat lunak |
| macOS (Intel) | `x86_64-macos` | Terbangun; runner CI tidak memiliki GPU untuk game, jadi game diperiksa di Mac Intel sungguhan |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Hanya uji platform; game dan editor belum diuji |
| Windows (MinGW) | `x86_64-windows-gnu` | Hanya uji platform; engine belum dapat dikompilasi di sini |

CI menjalankan setiap target pada runner dengan arsitekturnya sendiri, sehingga suite benar-benar dijalankan dan tidak sekadar di-link. Beberapa job di Linux dan macOS saat ini gagal.

Di Linux, ikon jendela diatur di dalam program, dan itu dihormati oleh X11 dan KDE. GNOME di Wayland mengabaikannya dan mengambil ikon dock dan alt-tab dari desktop entry yang dinamai menurut app id program (`org.blitzkrieg.game`, serta `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` untuk editor). Instalasi menyertakan skrip opsional yang menulis entri tersebut dan ikonnya ke `~/.local/share`; tidak ada yang menjalankannya untuk Anda. Dari folder instalasi jalankan `Tools/install_desktop_entry.sh` (dari checkout: `tools/install_desktop_entry.sh [install-dir]`), dan `Tools/install_desktop_entry.sh --remove` membatalkannya.

Hanya `x86_64-windows-msvc` yang membutuhkan instalasi Visual Studio. Zig menyediakan toolchain, header, dan pustaka impor sendiri untuk setiap target lain, termasuk MinGW.


# Membangun dan menjalankan

1. Klon repositori beserta submodul, atau jalankan `git submodule update --init --recursive` di checkout yang sudah ada.

2. Pasang Zig 0.17. Untuk `x86_64-windows-msvc` Anda juga memerlukan toolchain MSVC dan Windows SDK (path dapat diatur lewat `-Dmsvc-include`/`-Dwindows-sdk-include` dan padanan `lib`-nya jika berbeda dari bawaan di `build.zig`).

3. Jalankan `zig build install-all -Dtarget=<triple> --release=fast` untuk game, kesembilan mod bahasa, dan semua editor, atau `zig build install-game` untuk game saja. Tanpa `-Dtarget`, build dibuat untuk host, dan hilangkan `--release=fast` untuk build debug.

4. Jalankan biner hasil build, misalnya `zig-out/game/windows/x86_64/release/Game.exe` di Windows atau `zig-out/game/macos/arm64/release/Game` di Apple Silicon (`.../debug/...` tanpa `--release=fast`). Game berjalan di jendela; `-fullscreen` tersedia sebagai opsi baris perintah.

5. `zig build package` membuat paket zip yang dapat didistribusikan, dan `zig build --help` mencantumkan setiap langkah dan pengujian.


# Riwayat sejauh ini

- Menemukan pustaka yang hilang dan membuat solusi tahun 2003 dapat dikompilasi dari kondisi bersih di Visual Studio modern, lalu memainkan tutorial pertama.

- Memindahkan build ke Zig: `build.zig` mengompilasi setiap proyek dengan clang dan menyusun tata letak game yang bisa dijalankan.

- Mengganti StreamIO.dll dengan Zig, dan membuat game dapat dimainkan dari awal sampai akhir di bawah build Zig.

- Memporting game ke 64-bit, dan menguji-mainkannya sepanjang tutorial crash demi crash dengan UBSan.

- Beralih dari Win32 ke SDL3 dan renderer SDL GPU, dan menjalankan game di macOS dan Linux.

- Memporting Map Editor, Resource Editor, dan ELK, serta menghapus versi MFC-nya. Mengganti alat baris perintah lama.

- Menghapus FMOD, Bink, dan Stingray.

- Menambahkan mesin teks dan sepuluh bahasa di seluruh game, editor, dan alat.

- Menambahkan opsi Remastered dan putaran kamera, yang membawa ke tembok yang dijelaskan di atas.


# Peta jalan

1. Melewati tembok karya seni: memutuskan dari mana model 3D dan tekstur berasal, dan mendapatkan izin untuk apa pun yang membutuhkannya.

2. Menyelesaikan replay misi.

3. Membuat setiap job CI lulus kembali.

4. Menjalankan game dan editor di Linux arm64 dan MinGW, serta menambahkan Windows arm64.

5. Menyambungkan multipemain internet lewat pengganti OpenSpy untuk GameSpy.

6. Membuat utilitas untuk mengonversi simpanan game 32-bit ke format 64-bit.

7. Terus mengganti proyek C++ dengan kode Zig, memakai port StreamIO sebagai templat.
