# Firmware portal Two-Guards

ESP32 + RC522 + servo sebagai portal parkir. Kontrak MQTT-nya ada di repo server,
`Two-Guards/docs/mqtt-topics.md`; kalau firmware dan dokumen itu berbeda, dokumen yang benar.

## Hardware

ESP32-WROOM-32U DevKit 38 pin (antena eksternal lewat konektor IPEX), RC522, servo SG90/MG90S.

| Komponen | Pin komponen | Pin ESP32 |
| --- | --- | --- |
| RC522 | SDA (SS) | GPIO 5 |
| RC522 | SCK | GPIO 18 |
| RC522 | MOSI | GPIO 23 |
| RC522 | MISO | GPIO 19 |
| RC522 | RST | GPIO 22 |
| RC522 | IRQ | tidak dipakai |
| RC522 | GND | GND |
| RC522 | 3.3V | 3V3 (**bukan** 5V) |
| Servo | coklat/hitam | GND |
| Servo | merah | 5V / VIN |
| Servo | oranye/kuning (sinyal) | GPIO 13 |

Pin juga tercatat di `include/config.h`.

Model WROOM-32U **tidak punya antena PCB**: tanpa antena eksternal di konektor IPEX, WiFi-nya
sangat lemah atau tidak tersambung sama sekali.

## Persiapan

PlatformIO Core sudah terpasang oleh ekstensi PlatformIO di VS Code. Dari terminal, pakai
`~/.platformio/penv/bin/pio`, atau tambahkan ke PATH sekali saja:

```fish
fish_add_path ~/.platformio/penv/bin
```

Rahasia ada di `include/secrets.h`, yang di-gitignore:

```bash
cp include/secrets.example.h include/secrets.h
```

lalu isi SSID dan password WiFi 2.4 GHz, IP broker (IP laptop di jaringan lokal, `ip -4 -brief addr`),
password `device-1` dari `mosquitto_passwd`, dan `DEVICE_ID`.

Framework yang dipakai Arduino, bukan ESP-IDF: builder ESP-IDF menolak path project yang
mengandung spasi ("Detected a whitespace character in project paths"), sedangkan folder ini ada
di bawah `Visual Studio Code/Two Guard/`.

## Environment

| Environment | Untuk |
| --- | --- |
| `portal` (bawaan) | Firmware utama |
| `rfid-test` | Cek RC522 menjawab, cetak UID setiap kartu dalam format final |
| `servo-test` | Siklus tutup-buka-tutup, lalu sudut bisa dicoba lewat serial |
| `wifi-test` | WiFi, NTP, RSSI, epoch, dan apakah port broker di laptop terjangkau |
| `power-test` | Servo bergerak terus ~1 menit sambil WiFi mengirim paket; mencari brownout |
| `native` | Unit test logika murni (`lib/portal_logic`) di laptop |

Test hardware memakai modul yang sama dengan firmware utama (`src/app/`), jadi yang diuji adalah
kode yang nanti benar-benar jalan.

```bash
pio run -e rfid-test -t upload      # build + upload
pio device monitor                   # Ctrl+C untuk keluar
pio run                              # build firmware utama
pio run -t upload && pio device monitor
pio test -e native                   # unit test, tanpa ESP32
```

Kalau port tidak terdeteksi otomatis, tambahkan `--upload-port /dev/ttyUSB0` atau
`-p /dev/ttyUSB0` untuk monitor. Port harus dilepas monitor sebelum upload.

`pio device monitor` menambahkan jam laptop di depan setiap baris (filter `time`), berguna untuk
mengukur latensi. Filter `esp32_exception_decoder` menerjemahkan backtrace kalau firmware crash.

## Konfigurasi non-rahasia

Semua di `include/config.h`: pin, sudut servo tutup/buka, lama portal terbuka (5 detik), cooldown
scan per kartu (3 detik), batas umur command (15 detik ke belakang, 5 detik ke depan), interval
heartbeat (30 detik), keepalive MQTT, server NTP, dan timeout watchdog.

## Perilaku firmware utama

**Boot.** Servo ke posisi tutup lebih dulu, lalu WiFi, NTP, dan MQTT (clean session, LWT
`{"online":false}` di topic status). Setelah konek: publish status
`{"online":true,"portal_status":"closed"}`, lalu subscribe `parkir/{id}/command` QoS 1. Kalau NTP
belum sinkron 5 detik setelah WiFi tersambung, MQTT tetap konek; scan dan command ditolak sampai
jam tersinkron.

**Scan.** UID hex huruf besar tanpa pemisah, semua byte. Kartu yang sama tidak dikirim lagi
sebelum tidak terbaca selama 3 detik. Publish `{"uid":"...","scanned_at":<epoch>}` QoS 1. Saat
MQTT offline atau jam belum tersinkron, scan **tidak diantrekan**, hanya dicatat di serial: scan
yang tertunda bisa membuka gerbang terlambat.

**Command.** Diabaikan kalau jam belum tersinkron NTP, kalau `now - issued_at > 15`, kalau
`issued_at` lebih dari 5 detik di masa depan, atau kalau `access_log_id`-nya sudah pernah diproses
(16 terakhir diingat).

- `open`: servo buka, publish status `open`, tutup otomatis setelah 5 detik lalu publish `closed`.
  `open` yang datang saat masih terbuka memperpanjang timer.
- `deny`: servo diam, hanya dicatat.

**Heartbeat.** Status dikirim tiap 30 detik; server memakainya untuk `last_seen_at`.

**Gagal tertutup.** Timer tutup berjalan di ESP32 sendiri, tidak bergantung WiFi/MQTT. Selama
palang terbuka, firmware tidak mencoba konek MQTT (konek bisa memblok beberapa detik), jadi
penutupan tidak pernah tertunda karenanya. Loop yang macet lebih dari 15 detik me-reset ESP32
lewat watchdog, dan setiap boot dimulai dengan menutup palang.

**Sambung ulang.** WiFi dicoba lagi dengan jeda 10, 20, 40, lalu 60 detik; MQTT 1, 2, 4, ...
sampai 30 detik. Keduanya kembali ke jeda awal setelah berhasil.

### Kenapa arduino-mqtt (256dpi/MQTT)

PubSubClient tidak bisa publish QoS 1. arduino-mqtt bisa publish QoS 1 (menunggu PUBACK), LWT,
dan clean session, dan **tidak punya antrean kirim ulang**: publish yang gagal dikembalikan
sebagai error dan tidak pernah dikirim belakangan. Itu yang diinginkan untuk scan, karena scan
yang terlambat sampai di server bisa membuka gerbang terlambat. espMqttClient juga mendukung
QoS 1, tetapi menyimpan publish di antrean (bawaannya bahkan saat offline,
`EMC_ALLOW_NOT_CONNECTED_PUBLISH`) dan mengirim ulang isinya; antrean itu harus diatur dan
dikosongkan dengan hati-hati supaya scan lama tidak terkirim belakangan.

Konsekuensinya, publish QoS 1 memblok loop sampai PUBACK datang (beberapa milidetik di LAN, paling
lama `MQTT_TIMEOUT_MS`).

## Log serial

Satu baris per event: `[detik.milidetik] TAG pesan`.

| Tag | Isi |
| --- | --- |
| `BOOT` | Alasan reset terakhir (`BROWNOUT` = masalah daya), konfigurasi |
| `WIFI` / `NTP` | Konek, putus beserta alasannya, RSSI, sinkron jam |
| `MQTT` / `STAT` | Konek/putus beserta penyebab, status yang terkirim |
| `RFID` / `SCAN` | Versi chip, UID terkirim/ditolak dan alasannya |
| `CMD` / `GATE` | Command diterima/diabaikan dan alasannya, palang buka/tutup |

## Daya

"Brownout detector was triggered" atau ESP32 restart saat servo bergerak berarti masalah daya:
servo menarik arus puncak ratusan mA dari rel 5V yang sama dengan ESP32. Jangan mematikan brownout
detector. Perbaiki sumber dayanya: kapasitor 470–1000 µF di dekat servo, atau catu 5V terpisah
untuk servo dengan GND disatukan ke ESP32.
