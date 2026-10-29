# Laporan QA — Turnforge v0.4.1

Tanggal: 30 Oktober 2026. Baseline sebelum QA: commit `53543c7`. Hasil di bawah berlaku untuk perubahan source dan laporan dalam commit QA ini.

## Hasil utama

Aturan inti, stress test, kontrol otomatis, render, dan penanganan input/aset invalid lulus. Dua defect pada mode benchmark diperbaiki. Target 60 FPS belum sepenuhnya tercapai pada pengukuran ini; pengujian perangkat input fisik dan sesi bermain manusia belum dilakukan.

| Pemeriksaan | Hasil | Bukti |
|---|---|---|
| Build Release dari direktori build baru | Lulus; object raylib/GLFW dan aplikasi dikompilasi ulang | `build/qa-release`, CMake/Ninja |
| Core-only Debug tanpa raylib | Configure/build/CTest lulus; assertion standard library aktif | `TURNFORGE_BUILD_GAME=OFF` |
| Analisis statis core | Kompilasi GCC `-fanalyzer` selesai tanpa diagnostic | `src/game.cpp` |
| Aturan inti | 20/20 skenario, 268 pemeriksaan; Release dan Debug lulus | [Hasil aturan inti](gameplay-test-results.txt) |
| Stress deterministik | 500 sesi, 64.433 aksi, 1.126.045 pemeriksaan; Release dan Debug lulus | [Hasil stress](qa-stress-results.txt) |
| Controller keyboard/mouse | 25 assertion lulus melalui event raylib dan fungsi input aplikasi | [Hasil input](qa-input-results.txt) |
| Command line dan aset invalid | 17/17 kasus lulus | [Hasil CLI/aset](qa-cli-results.json) |
| Render dan reset | 10 screenshot 1280×720; karakter, menang/kalah, Bertahan, dan 10 restart lulus | [Smoke report](smoke-verification.json) |
| Inspeksi visual | Seluruh 10 screenshot ditinjau; tidak ditemukan teks terpotong atau state hasil/perisai yang salah | [Screenshot](screenshots/02-gameplay.png) |
| Performa berkelanjutan | 180,008 detik tanpa crash; rata-rata sekitar 58,4 FPS; working set puncak 139,99 MB | [Benchmark](benchmark-verification.json), [memori](benchmark-memory.json) |
| Paket Windows dari ZIP | Lulus dari direktori baru di luar source, path berspasi, dan PATH sistem | [Hasil paket](package-verification.json) |
| Dokumentasi | Tautan lokal, gambar, code fence, serta 17 bagian dan 17 baris AC pada PRD diperiksa | [README](../README.md), [PRD](../PRD.md) |

## Defect yang diperbaiki

| ID | Masalah sebelum perbaikan | Perbaikan dan regresi |
|---|---|---|
| QA-01 | `--benchmark-seconds 0.1junk` diterima sebagai angka 0,1 dan membuka renderer | Parser harus menghabiskan seluruh string angka; kasus suffix, nonangka, NaN, infinity, overflow, dan batas durasi diperiksa sebelum window dibuka |
| QA-02 | Benchmark positif yang sangat pendek berhenti sebelum smoke sequence selesai, sehingga menghasilkan kegagalan pemeriksaan yang menyesatkan | Benchmark tetap menyelesaikan minimum 360 frame pemeriksaan; kasus durasi 0,1 detik lulus dengan input, hasil, perisai, dan window-close terverifikasi |

Pengujian input dan stress merupakan penambahan cakupan QA. Tidak ada fitur gameplay, statistik, map, atau dependency baru.

## Matriks acceptance criteria

Status PASS di tabel ini berarti pemeriksaan otomatis yang tercantum lulus; bukan pengganti seluruh checklist manual PRD.

| ID | Cakupan yang diverifikasi | Status |
|---|---|---|
| AC-01 | BFS memutari tembok; panjang jalur dibandingkan oracle relaksasi terpisah | PASS |
| AC-02 | Tujuan terputus/di luar map; koordinat klik di luar arena | PASS |
| AC-03 | Batas tiga langkah, tembok, dan occupancy | PASS |
| AC-04 | Gerak kedua ditolak | PASS |
| AC-05 | Serang menutup gerak dan serangan kedua | PASS |
| AC-06 | Target diagonal, jauh, mati, atau indeks invalid ditolak | PASS |
| AC-07 | Lewati aksi, urutan musuh, serta reset giliran | PASS |
| AC-08 | Musuh tanpa jalur menunggu | PASS |
| AC-09 | Occupancy terbaru dan tidak ada overlap | PASS |
| AC-10 | Menang/kalah langsung menghentikan aksi lanjutan | PASS |
| AC-11 | Restart hasil menang/kalah; sepuluh reset state dan render | PASS |
| AC-12 | Goblin mengejar dan bandit memilih sisi lain secara deterministik | PASS |
| AC-13 | Batas detour, ally mati, dan tujuan tertutup | PASS |
| AC-14 | Musuh bersebelahan, terjebak, serta pemain di sudut | PASS |
| AC-15 | Biaya Bertahan, gerak sebelum Bertahan, konflik dengan serangan | PASS |
| AC-16 | Dua hit masing-masing 1 damage; expiry dengan/tanpa hit; damage kembali 2 | PASS |
| AC-17 | State invalid, lethal hit, restart, dan Escape membersihkan perlindungan | PASS |

## Metode dan batas cakupan

Stress memakai seed `20261030`, map awal dan variasi tembok acak, batas 200 aksi per sesi, serta restart acak. Tes membandingkan replay dari state yang sama, memeriksa HP/occupancy/phase, dan memastikan penolakan hanya boleh mengubah pesan penjelasan. Sesi acak bukan simulasi pemain yang mengoptimalkan strategi; hasil menang/kalahnya tidak dipakai sebagai ukuran balance.

Input diuji menggunakan automation event raylib 5.5 melalui controller yang juga dipakai game normal. Cakupan meliputi Enter/Start, klik arena valid/invalid, G/Bertahan, Space/End Turn, R/Restart, Escape, input saat animasi/giliran musuh/hasil, serta state window-close. Event ini tidak menekan keyboard/mouse fisik atau memvalidasi callback desktop Windows secara menyeluruh.

Sepuluh screenshot diperiksa melalui visual inspection. Benchmark menggunakan hidden window, 1280×720, cap 60 FPS, dan sampling memori setiap 0,5 detik pada i5-10310U/Intel UHD. Capture awal dikecualikan dari sampel frame time. Animasi yang masih aktif pada endpoint benchmark diterima selama state valid; smoke sequence tetap harus berakhir idle.

## Target kualitas dan pekerjaan tersisa

| Area | Penilaian |
|---|---|
| Respons AI <50 ms | Lulus; maksimum keputusan terukur 0,3386 ms |
| Rendering 60 FPS | Belum sepenuhnya terpenuhi; rata-rata 17,138 ms/frame atau sekitar 58,4 FPS, p95 18,056 ms |
| Working set <256 MB | Lulus pada run ini; puncak 139,99 MB |
| Stabilitas/reset | Tidak ada crash selama run tiga menit dan sepuluh reset; bukan bukti tidak ada seluruh kemungkinan leak |
| Latensi input fisik | Belum diukur; pengujian controller otomatis sudah lulus |
| Kejelasan UI dan balance | Screenshot dan strategi menang/kalah terverifikasi; observasi pengguna dan durasi bermain aktual masih diperlukan |
| Lingkungan lain | Compiler MSVC, OS lain, driver/GPU lain, serta variasi DPI desktop belum diuji |

Build raylib 5.5 mengeluarkan warning deprecation CMake dan redefinisi macro `MAX_MESH_VERTEX_BUFFERS` dari dependency. Build selesai; source dependency tidak diubah untuk menutupi warning tersebut.

## Distribusi

ZIP diekstrak ke direktori baru di luar source tree dengan spasi pada path. Executable dijalankan dengan PATH dibatasi ke Windows/System32. Pemeriksaan input, window-close state, karakter, hasil, Bertahan, sepuluh screenshot, serta sepuluh restart lulus; proses berakhir idle dalam 7,872 detik. Hash executable, atlas, dan tiga file lisensi sesuai paket terpasang; executable terpasang sesuai build Release yang diuji. Import DLL hanya berisi GDI32, KERNEL32, ucrtbase, SHELL32, USER32, dan WINMM.

[Hasil paket](package-verification.json). ZIP akhir memuat laporan terbaru dengan runtime yang hash-nya tetap sama.

## Menjalankan ulang

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Package
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1
powershell -ExecutionPolicy Bypass -File scripts/test-cli.ps1
powershell -ExecutionPolicy Bypass -File scripts/verify.ps1 -BenchmarkSeconds 180
```

Jalankan executable paket dengan `-Executable` pada script verifier untuk menguji distribusi. Core suite dan stress test dijalankan melalui CTest oleh build script. Hasil terukur dan keterbatasan tambahan ada pada [catatan verifikasi](verification.md).
