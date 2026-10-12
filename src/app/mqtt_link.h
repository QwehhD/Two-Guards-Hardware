#pragma once

#include <MQTT.h>
#include <WiFi.h>

#include <cstddef>
#include <cstdint>

#include "backoff.h"
#include "config.h"

// Koneksi MQTT ke broker: clean session, LWT {"online":false} di topic status,
// sambung ulang dengan backoff, dan publish QoS 1 yang menunggu PUBACK.
//
// Tidak ada antrean kirim: publish saat offline langsung gagal, dan publish
// yang tidak di-PUBACK tidak dikirim ulang. Scan yang terlambat sampai di
// server bisa berujung gerbang terbuka terlambat.
class MqttLink {
 public:
  void begin(int deviceId, const char* host, int port, const char* username, const char* password);

  // Dipanggil di setiap loop: melayani koneksi, mengirim heartbeat status
  // tiap HEARTBEAT_INTERVAL_MS, dan menyambung ulang. allowConnect=false
  // menunda percobaan konek (konek bisa memblok beberapa detik); koneksi yang
  // sudah ada tetap dilayani. portalOpen = posisi palang untuk pesan status.
  void tick(uint32_t nowMs, bool wifiUp, bool allowConnect, bool portalOpen);

  bool connected() { return client_.connected(); }

  bool publishScan(const char* uidHex, int64_t scannedAt);
  bool publishStatus(bool portalOpen);

  // Command yang masuk disimpan dulu oleh callback MQTT (yang tidak boleh
  // publish), lalu diambil satu per satu di loop.
  bool popCommand(char* payload, size_t size, size_t& length);

 private:
  static constexpr size_t kInboxSlots = 4;
  static constexpr size_t kInboxPayloadSize = 192;

  struct InboxEntry {
    char payload[kInboxPayloadSize];
    size_t length;
  };

  bool connectNow(bool portalOpen);
  void onMessage(char* topic, char* bytes, int length);
  bool publish(const char* topic, const char* payload);

  WiFiClient net_;
  MQTTClient client_{MQTT_BUFFER_SIZE};
  portal::Backoff backoff_{MQTT_RETRY_MIN_MS, MQTT_RETRY_MAX_MS};

  const char* username_ = nullptr;
  const char* password_ = nullptr;
  char clientId_[40] = {};
  char scanTopic_[40] = {};
  char statusTopic_[40] = {};
  char commandTopic_[40] = {};

  bool wasConnected_ = false;
  uint32_t nextAttemptMs_ = 0;
  uint32_t lastStatusMs_ = 0;

  InboxEntry inbox_[kInboxSlots] = {};
  size_t inboxHead_ = 0;
  size_t inboxCount_ = 0;
};
