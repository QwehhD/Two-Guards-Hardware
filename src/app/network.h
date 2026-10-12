#pragma once

#include <cstdint>

#include "backoff.h"
#include "config.h"

// WiFi (sambung ulang sendiri dengan backoff) dan jam NTP. Non-blocking:
// begin() hanya memulai, tick() dipanggil di setiap loop.
class Network {
 public:
  void begin(const char* hostname, const char* ssid, const char* password);
  void tick(uint32_t nowMs);

  bool wifiConnected() const { return connected_; }
  // millis() saat WiFi terakhir tersambung.
  uint32_t connectedSinceMs() const { return connectedSinceMs_; }

  // true setelah SNTP berhasil sinkron sejak boot ini. Sebelum itu jam ESP32
  // mulai dari 1970, dan umur command tidak bisa dinilai.
  static bool clockSynced();
  static int64_t epochNow();

 private:
  void startAttempt(uint32_t nowMs);

  const char* ssid_ = nullptr;
  const char* password_ = nullptr;
  bool connected_ = false;
  uint32_t connectedSinceMs_ = 0;
  bool ntpStarted_ = false;
  bool clockLogged_ = false;
  uint32_t nextAttemptMs_ = 0;
  uint32_t attempts_ = 0;
  portal::Backoff backoff_{WIFI_RETRY_MIN_MS, WIFI_RETRY_MAX_MS};
};
