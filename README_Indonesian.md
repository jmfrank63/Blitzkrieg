[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Repositori ini adalah proyek pribadi Johannes Maria Frank. Saya menggunakannya untuk bersenang-senang dan belajar, terutama untuk berlatih pengkodean agentik pada aplikasi brownfield. README asli disimpan di `readme_original`.

Peringatan: proyek ini masih dalam proses.



# Apa yang dapat dilakukan repositori ini hari ini

- Berisi kode sumber lengkap Blitzkrieg single-player dan data game.

- Dapat dibangun dengan bersih dari solusi `A7.sln` baru menggunakan alat MSVC modern.

- Berjalan dalam jendela asli secara default, dengan `-fullscreen` tetap tersedia sebagai opsi command-line.

- Tutorial sekarang berfungsi pada build `Debug | Win32`; unit dapat dipilih dan digerakkan sementara debugging runtime masih berlanjut.

- Termasuk pelaporan pengecualian native dan dukungan debugging modern.

- Menggunakan submodul Git untuk pustaka yang hilang.

- Mendukung debugging C++ native dan WinDbg di VS Code Insiders.

- Telah menghapus penanganan crash BugSlay warisan dan menggantinya dengan assert C++ standar.



# Menjalankan game dengan Visual Studio 2026 Insiders

1. Clone repositori bersama submodulnya, atau jalankan `git submodule update --init --recursive` pada checkout yang sudah ada.

2. Instal Visual Studio 2026 Insiders / Visual Studio 18 dengan workload Desktop development with C++, toolchain MSVC VS 2026, dan Windows 10 atau Windows 11 SDK.

3. Buka `Sources/src/A7.sln`.

4. Pilih `Debug | Win32`.

5. Build proyek `Game`, atau build seluruh solution.

6. Jalankan proyek `Game` dengan F5, atau jalankan langsung `Sources/src/Game/Debug/Game.exe`.

7. Game berjalan dalam mode windowed secara default. Tambahkan `-fullscreen` ke command arguments proyek Game jika ingin perilaku fullscreen lama.

Jika build tidak dapat menyalin DLL ke `Sources/src/Game/Debug`, tutup proses `Game.exe` yang sedang berjalan lalu build lagi.



# Sejarah sejauh ini

- Menemukan pustaka yang hilang dan menambahkannya sebagai submodul Git.

- Membangun VM Windows XP SP3 dengan Visual Studio 6 untuk mempelajari lingkungan asli.

- VS6 tidak stabil dan sering crash, jadi pengembangan dipindahkan ke alat modern.

- Menginstal Visual Studio 2010 dan mengonversi proyek `.dwr` lama menjadi `.sln`.

- Memuat solusi ke VS 2026 Insiders dan memperbarui semua dependensi.

- Dua minggu pengkodean agen membuat solusi dapat dikompilasi dari kondisi bersih tanpa kesalahan dan peringatan.

- Debugging runtime sekarang sudah melewati menu awal: tutorial dapat dimuat dan dimainkan, termasuk memilih dan menggerakkan tank.

- Pengembangan dipindahkan ke VS Code Insiders karena alatnya lebih baik.

- Mengonfigurasi dua jalur debug: C++ native dan WinDbg.

- Menghapus BugSlay karena membuat debugging lebih sulit dan, dalam satu kasus, crash setelah crash.

- BugSlay diganti dengan assert C++ standar yang sederhana.

- Dengan BugSlay dihapus, proyek sekarang siap melanjutkan debugging runtime yang terfokus hingga mencapai jalannya game sepenuhnya.



# Peta jalan

1. Terus menghapus pengecualian runtime yang tersisa di luar jalur tutorial yang sekarang sudah berfungsi.

2. Pindahkan kompilasi ke Zig.

3. Ganti FMOD, Stingray, dan Bink dengan alternatif open source.

4. Mulai mengganti proyek C++ satu per satu dengan kode Zig.
