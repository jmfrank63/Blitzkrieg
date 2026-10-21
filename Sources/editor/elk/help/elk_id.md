# Blitzkrieg ELK

ELK, perangkat lokalisasi Blitzkrieg, adalah tempat Anda menerjemahkan teks permainan. ELK menampilkan setiap teks permainan beserta teks aslinya, Anda mengetik terjemahannya, menandai sejauh mana tiap teks, lalu memasukkan terjemahan Anda ke dalam permainan sebagai mod bahasa.

## Memulai ELK

- Dijalankan sendiri, ELK membuka basis data yang Anda pakai terakhir.
- ELK path/to/elk.xml membuka basis data itu.
- ELK --short adalah tata letak penerjemah: ELK membuka elk.xml di samping program, lebih dulu memperbaruinya dengan berkas pembaruan .upd terbaru di sana, dan menampilkan satu bilah alat dengan lima filter tetap. ELK selalu mulai dengan cara yang sama.
- ELK --developer, atau tanpa opsi sama sekali, adalah tata letak lengkap dengan semua menu, filter bernama yang Anda buat sendiri, dan daftar basis data terbaru.
- Bahasa menu, pesan, dan bantuan ELK ini dipilih di Alat > Bahasa. ELK mengingatnya; Sistem mengikuti bahasa sistem operasi Anda.

## Membuka dan menyimpan

Basis data adalah folder berisi berkas elk.xml dan satu folder untuk tiap bagian permainan, seperti game_data_base. Buka satu lewat Berkas > Buka (Ctrl+O) atau pilih di Berkas > Terbaru.

Anda tidak kehilangan pekerjaan: ELK menyimpan teks yang sedang Anda edit saat Anda pindah ke teks lain, pada Berkas > Simpan (Ctrl+S), pada Berkas > Tutup, saat Keluar, dan setiap 30 detik selama ada yang belum disimpan. Baris status di bagian bawah menunjukkan kapan teks memiliki perubahan yang belum disimpan.

## Pohon teks

Sisi kiri adalah pohon semua teks permainan, dalam folder seperti yang disimpan permainan. Klik sebuah teks untuk menampilkannya: teks aslinya di atas, deskripsinya (catatan untuk penerjemah, jika teks itu punya) di bawahnya, dan terjemahannya di bawah itu. Teks asli dan deskripsi hanya-baca, tetapi Anda dapat memilih dan menyalin darinya. Buka folder dengan panahnya atau dengan klik dua kali.

Setiap baris berwarna sesuai statusnya, dan folder berwarna sesuai teks yang paling belum selesai di dalamnya, jadi folder merah masih berisi pekerjaan.

## Status

Setiap teks memiliki satu dari empat status, dipilih dengan tombol di antara teks asli dan terjemahan:

- Belum diterjemahkan (merah): belum dikerjakan.
- Kedaluwarsa (kuning): teks asli berubah dalam pembaruan setelah diterjemahkan. Periksa terjemahan dan tetapkan lagi. Hanya pembaruan atau impor lembar kerja yang menetapkan status ini.
- Diterjemahkan (hijau): sudah diterjemahkan, belum diperiksa. Mengetik pada terjemahan menetapkan status ini.
- Disetujui (biru): sudah diterjemahkan dan diperiksa.

## Filter dan penjelajahan

Filter memilih teks mana yang dilewati perintah Jelajah. Pohon meredupkan teks yang dikecualikan filter tetapi tetap menampilkannya.

- Lima filter tetap adalah Semua, Belum diterjemahkan, Kedaluwarsa, Diterjemahkan, dan Disetujui.
- Jelajah > Berikutnya (Ctrl+N) dan Sebelumnya (Ctrl+P) menuju teks berikutnya atau sebelumnya yang dipertahankan filter, dan kursor langsung masuk ke terjemahan. Pada tata letak lengkap, Pertama dan Terakhir menuju ujung-ujungnya.
- Ciutkan butir menutup folder yang Anda tinggalkan saat menjelajah, sehingga hanya jalan menuju teks saat ini yang tetap terbuka.
- Pada tata letak lengkap, Jelajah > Buat filter... membuat filter Anda sendiri: nama, status yang dipertahankan, Hanya yang berubah, dan kondisi folder seperti medals&german (setiap bagian harus ada di jalur folder).

## Cari

Edit > Cari... (Ctrl+F, di bawah Alat pada tata letak singkat) mencari di teks asli, deskripsi, dan terjemahan semua teks menurut urutan pohon. Cari berikutnya menuju tempat berikutnya; Samakan huruf besar/kecil, Hanya kata utuh, dan Ke atas atau Ke bawah bekerja seperti di program lain. Baris status menyebut di kolom mana dan di mana teks ditemukan.

## Statistik

Tampilan > Statistik ELK menghitung teks, kata, dan karakter dari teks asli dan terjemahan, secara total dan per status, sehingga Anda dapat melihat berapa yang tersisa. Di sampingnya ada hitungan seperti saat basis data terakhir ditutup setelah menghitung, sehingga Anda dapat melihat berapa yang berubah sejak itu.

Tampilan > Pohon ELK menyembunyikan atau menampilkan daftar teks, dan Tampilan > Bilah status (tata letak lengkap) baris status; ELK mengingat keduanya.

## Pemeriksa ejaan

Alat > Ejaan (F7) memeriksa terjemahan yang sedang Anda edit dari awal. Pemeriksaan berhenti pada setiap kata yang salah eja dan menaruh saran terbaiknya di kotak Ubah menjadi; klik saran lain atau ketik sendiri. Abaikan melewati kata itu kali ini, Abaikan Semua melewatinya sampai Anda menutup ELK, Ubah menggantinya dengan teks di Ubah menjadi, dan Ubah Semua menggantinya di seluruh terjemahan. Periksa Lagi memulai dari awal. Perubahan adalah suntingan seperti ketikan dan disimpan seperti itu. Di bawahnya, setiap kata yang salah eja didaftar beserta sarannya; Tambah ke kamus menyimpan sebuah kata, seperti nama, di berkas kamus Anda sendiri custom.dic, sehingga tidak pernah ditandai lagi.

Pemeriksa ejaan membutuhkan kamus Hunspell, berkas .dic dan .aff dalam bahasa Anda, misalnya dari LibreOffice. Taruh di folder dictionaries di samping ELK, atau pilih folder lain dan bahasanya di Alat > Opsi ejaan.... Selama kamus belum ditemukan, Ejaan nonaktif dan menyebut alasannya.

## Lembar kerja

Anda dapat menerjemahkan di program lembar kerja dan membawa hasilnya kembali. Pada tata letak singkat, perintah-perintah ini, ekspor, dan Jalankan permainan ada di bawah Alat dan di bilah alat.

- Berkas > Ekspor ke lembar kerja... menulis setiap teks, beserta jalur, teks asli, terjemahan, status, dan deskripsinya, ke buku kerja .xlsx (bawaan) atau berkas .csv UTF-8.
- Berkas > Impor dari lembar kerja... membaca lembar seperti itu, atau lembar .xls lama dari ELK asli. Setiap baris dengan terjemahan menetapkan terjemahan teks itu. Baris yang teks aslinya tidak lagi cocok dengan basis data menandai teks sebagai Kedaluwarsa. Baris tanpa terjemahan dilewati. Baris status menyebut berapa baris yang dibaca dan diubah.
- Biarkan kolom Path apa adanya: kolom itu memberi tahu ELK baris tersebut milik teks yang mana.

## Memperbarui teks asli

Ketika teks permainan berubah, bawa teks asli yang baru ke basis data yang terbuka. Teks yang aslinya berubah menjadi Kedaluwarsa jika sudah diterjemahkan atau disetujui, jika tidak menjadi Belum diterjemahkan; terjemahan Anda tetap ada.

- Berkas > Impor dari permainan... membaca teks dari folder bahasa sebuah permainan (folder yang berisi Textes). Teks baru ditambahkan; tidak ada yang dihapus.
- Berkas > Impor dari PAK/UPD... membaca berkas .pak atau .upd. Namanya menentukan bagian basis data yang diperbarui: game_12.upd memperbarui game. Berkas .upd adalah pembaruan lengkap dan juga menghapus teks yang tidak lagi dimilikinya.
- Baris status menyebut berapa teks yang diimpor. Mengimpor berkas yang sama lagi tidak mengubah apa pun.

## Memasukkan terjemahan ke permainan

- Berkas > Ekspor ke mod bahasa... menulis terjemahan Anda sebagai mod: ketik nama, dan ELK menulis mods/<nama>/data/texts.pak di folder permainan. Ini cara yang aman: permainan membaca mod sebelum berkas teksnya sendiri, dan permainan asli tetap utuh.
- Pada tata letak lengkap, ketika filter saat ini mengecualikan teks, dialog ekspor menawarkan untuk menulis hanya teks yang dipertahankan filter, misalnya hanya yang Disetujui. Teks yang dikecualikan tidak ada sama sekali di mod itu.
- Di permainan, pilih mod di Opsi > Pengaturan > Permainan > Bahasa. Permainan berganti bahasa seketika dan mengingat pilihannya. Pilih bahasa asli di sana untuk kembali.
- Berkas > Pasang ke permainan... menulis Data/Texts.pak ke permainan itu sendiri, seperti ELK asli. Jika permainan memiliki berkas teks lepas yang akan menutupinya, ELK memperingatkan Anda dan menawarkan mod sebagai gantinya.
- Berkas > Jalankan permainan mengekspor ulang mod dengan suntingan terbaru Anda dan memulai permainan dengannya, di profil uji terpisah, sehingga simpanan dan pengaturan Anda sendiri tidak tersentuh.
- ELK mencari permainan di folder tempat ELK dijalankan.

## Operasi panjang

Ekspor, impor, dan Cari yang harus membaca setiap teks berjalan di latar belakang. Jendela Bekerja menunjukkan apa yang dikerjakan ELK dan sejauh mana, dan bagian ELK lainnya menunggu sampai selesai. Batal menghentikannya dengan aman: ekspor yang dibatalkan tidak menulis apa pun, dan impor yang dibatalkan mempertahankan baris yang sudah dibaca, setiap berkas utuh, sehingga Anda dapat mengimpor lembar itu lagi nanti.

## Tombol

- Ctrl+O buka, Ctrl+S simpan
- Ctrl+N atau Alt+N teks berikutnya, Ctrl+P atau Alt+P teks sebelumnya (Alt hanya selama kursor tidak berada di terjemahan)
- Ctrl+F cari
- F7 ejaan
- F1 bantuan ini
