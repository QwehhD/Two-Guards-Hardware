// wifi-test: konek ke WiFi 2.4 GHz, sinkronkan NTP, lalu tiap 5 detik cetak
// RSSI, epoch, dan apakah port broker MQTT di laptop bisa dijangkau.
//
//   pio run -e wifi-test -t upload && pio device monitor
//
// Memakai include/secrets.h (WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT).

#include <Arduino.h>
#include <WiFi.h>

#include <ctime>

#include "app/log.h"
#include "app/network.h"
#include "config.h"
#include "secrets_guard.h"

namespace {

constexpr uint32_t kReportIntervalMs = 5000;

Network network;
uint32_t lastReportMs = 0;

void scanForSsid() {
  logf("SCAN", "mencari \"%s\" di sekitar...", WIFI_SSID);
  const int found = WiFi.scanNetworks();
  bool seen = false;

  for (int i = 0; i < found; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      seen = true;
      logf("SCAN", "ditemukan: RSSI %d dBm, channel %ld, BSSID %s", WiFi.RSSI(i), static_cast<long>(WiFi.channel(i)),
           WiFi.BSSIDstr(i).c_str());
    }
  }

  if (!seen) {
    logf("SCAN", "\"%s\" TIDAK terlihat dari %d jaringan. Pastikan 2.4 GHz dan nama persis sama.", WIFI_SSID, found);
  }

  WiFi.scanDelete();
}

void report() {
  if (!network.wifiConnected()) {
    logf("INFO", "WiFi belum terhubung");

    return;
  }

  const int rssi = WiFi.RSSI();
  const char* quality = rssi >= -60 ? "bagus" : rssi >= WIFI_RSSI_POOR_DBM ? "cukup" : "BURUK, cek antena IPEX";

  char utc[32] = "-";

  if (Network::clockSynced()) {
    const time_t now = time(nullptr);
    struct tm tm;
    gmtime_r(&now, &tm);
    strftime(utc, sizeof utc, "%Y-%m-%dT%H:%M:%SZ", &tm);
  }

  WiFiClient probe;
  const uint32_t started = millis();
  const bool brokerReachable = probe.connect(MQTT_HOST, MQTT_PORT, 2000) == 1;
  const uint32_t probeMs = millis() - started;
  probe.stop();

  logf("INFO", "RSSI %d dBm (%s) | NTP %s, epoch %lld, %s | TCP %s:%d %s (%lu ms)", rssi, quality,
       Network::clockSynced() ? "ok" : "belum", static_cast<long long>(Network::epochNow()), utc, MQTT_HOST, MQTT_PORT,
       brokerReachable ? "terbuka" : "GAGAL", static_cast<unsigned long>(probeMs));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  logBootBanner("Two-Guards wifi-test");

  WiFi.mode(WIFI_STA);
  scanForSsid();

  network.begin("two-guards-wifi-test", WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  const uint32_t now = millis();
  network.tick(now);

  if (now - lastReportMs >= kReportIntervalMs) {
    lastReportMs = now;
    report();
  }

  delay(10);
}
