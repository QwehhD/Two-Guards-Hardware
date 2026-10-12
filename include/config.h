#pragma once

// Konfigurasi non-rahasia. Rahasia (WiFi, broker, akun MQTT, DEVICE_ID) ada di
// secrets.h. Batas command mengikuti Two-Guards/docs/mqtt-topics.md.

#include <cstddef>
#include <cstdint>

// ---- Pin (wiring: README.md) ----
constexpr int PIN_RC522_SS = 5;
constexpr int PIN_RC522_SCK = 18;
constexpr int PIN_RC522_MOSI = 23;
constexpr int PIN_RC522_MISO = 19;
constexpr int PIN_RC522_RST = 22;
constexpr int PIN_SERVO = 13;

// ---- Servo (SG90/MG90S) ----
// Sudut ditentukan lewat servo-test. Ganti di sini, bukan di kode.
constexpr int SERVO_ANGLE_CLOSED = 0;
constexpr int SERVO_ANGLE_OPEN = 90;
// Rentang pulsa untuk 0..180 derajat.
constexpr int SERVO_PULSE_MIN_US = 500;
constexpr int SERVO_PULSE_MAX_US = 2400;

// ---- Portal ----
// Lama portal terbuka setelah command open; open berikutnya memperpanjangnya.
constexpr uint32_t PORTAL_OPEN_DURATION_MS = 5000;
// Kartu yang sama tidak dikirim lagi sebelum tidak terbaca selama ini.
constexpr uint32_t SCAN_COOLDOWN_MS = 3000;
// Jeda polling reader. Tanpa kartu, satu polling memblok ~25 ms.
constexpr uint32_t RFID_POLL_INTERVAL_MS = 50;
// Seberapa sering VersionReg RC522 dicek; chip yang tidak menjawab diinisialisasi ulang.
constexpr uint32_t RFID_HEALTH_INTERVAL_MS = 5000;

// ---- Command (aturan firmware di mqtt-topics.md) ----
// now - issued_at > ini = basi. 15 = TTL server 10 detik + toleransi jam 5 detik.
constexpr int64_t COMMAND_MAX_AGE_S = 15;
// issued_at - now > ini = dari masa depan, diabaikan.
constexpr int64_t COMMAND_MAX_FUTURE_S = 5;
// Jumlah access_log_id terakhir yang diingat untuk dedupe.
constexpr size_t COMMAND_DEDUPE_SIZE = 16;

// ---- MQTT ----
// Server memakai pesan status untuk last_seen_at.
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 30000;
// Broker mengirim LWT setelah ~1,5x keepalive tanpa kabar.
constexpr int MQTT_KEEPALIVE_S = 10;
// Batas tunggu CONNACK/SUBACK/PUBACK. Publish QoS 1 memblok paling lama selama ini.
constexpr int MQTT_TIMEOUT_MS = 1500;
constexpr uint32_t MQTT_RETRY_MIN_MS = 1000;
constexpr uint32_t MQTT_RETRY_MAX_MS = 30000;
// Ukuran buffer baca/tulis client MQTT (command ~70 byte).
constexpr int MQTT_BUFFER_SIZE = 512;

// ---- WiFi ----
// Satu percobaan konek WiFi butuh beberapa detik, jadi jeda awalnya lebih panjang dari MQTT.
constexpr uint32_t WIFI_RETRY_MIN_MS = 10000;
constexpr uint32_t WIFI_RETRY_MAX_MS = 60000;
// RSSI di bawah ini dianggap buruk (cek antena IPEX).
constexpr int WIFI_RSSI_POOR_DBM = -75;

// ---- NTP ----
// issued_at dibandingkan dengan jam ini. Untuk jaringan tanpa internet,
// arahkan ke server NTP lokal (lihat README.md).
constexpr const char* NTP_SERVER_1 = "pool.ntp.org";
constexpr const char* NTP_SERVER_2 = "time.google.com";
constexpr const char* NTP_SERVER_3 = "time.cloudflare.com";
// Jam di bawah ini (1 Januari 2026) pasti belum tersinkron.
constexpr int64_t MIN_VALID_EPOCH = 1767225600;

// ---- Watchdog ----
// Loop yang macet lebih lama dari ini me-reset ESP32; setelah boot, servo
// langsung ke posisi tutup. Harus jauh di atas blok terlama (konek MQTT ~4,5 detik).
constexpr uint32_t WATCHDOG_TIMEOUT_S = 15;
