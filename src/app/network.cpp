#include "network.h"

#include <WiFi.h>
#include <esp_sntp.h>

#include <ctime>

#include "log.h"

namespace {

volatile bool ntpSynced = false;

// Alasan putus terakhir dari event WiFi, untuk log yang bisa ditindaklanjuti.
volatile uint8_t lastDisconnectReason = 0;

const char* disconnectReasonHint(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
      return "SSID tidak ditemukan (salah nama, atau WiFi 5 GHz saja?)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "autentikasi gagal (password WiFi salah?)";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "sinyal AP hilang";
    case WIFI_REASON_ASSOC_FAIL:
      return "asosiasi gagal";
    default:
      return "lihat kode alasan wifi_err_reason_t";
  }
}

void onWifiDisconnected(arduino_event_id_t, arduino_event_info_t info) {
  // ASSOC_LEAVE = kita sendiri yang memutus (WiFi.disconnect() sebelum mencoba
  // lagi); jangan sampai menimpa alasan kegagalan yang sebenarnya.
  if (info.wifi_sta_disconnected.reason != WIFI_REASON_ASSOC_LEAVE) {
    lastDisconnectReason = info.wifi_sta_disconnected.reason;
  }
}

}  // namespace

void Network::begin(const char* hostname, const char* ssid, const char* password) {
  ssid_ = ssid;
  password_ = password;

  sntp_set_time_sync_notification_cb([](struct timeval*) { ntpSynced = true; });

  WiFi.onEvent(onWifiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // setHostname harus sebelum mode().
  WiFi.setHostname(hostname);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  // Modem sleep menunda paket masuk sampai beacon berikutnya (~100-300 ms);
  // dimatikan supaya command sampai secepat mungkin.
  WiFi.setSleep(false);
  // Sambung ulang diatur sendiri di tick(), dengan backoff dan log alasannya.
  WiFi.setAutoReconnect(false);

  startAttempt(millis());
}

void Network::startAttempt(uint32_t nowMs) {
  if (lastDisconnectReason != 0) {
    logf("WIFI", "percobaan sebelumnya gagal: alasan %u, %s", lastDisconnectReason, disconnectReasonHint(lastDisconnectReason));
    lastDisconnectReason = 0;
  }

  attempts_++;
  logf("WIFI", "menghubungkan ke \"%s\" (percobaan %lu)", ssid_, static_cast<unsigned long>(attempts_));

  WiFi.disconnect();
  WiFi.begin(ssid_, password_);

  nextAttemptMs_ = nowMs + backoff_.next();
}

void Network::tick(uint32_t nowMs) {
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !connected_) {
    const int rssi = WiFi.RSSI();
    logf("WIFI", "terhubung: IP %s, RSSI %d dBm, channel %d", WiFi.localIP().toString().c_str(), rssi, WiFi.channel());

    if (rssi < WIFI_RSSI_POOR_DBM) {
      logf("WIFI", "RSSI buruk (< %d dBm). Cek antena eksternal terpasang di konektor IPEX.", WIFI_RSSI_POOR_DBM);
    }

    connectedSinceMs_ = nowMs;
    backoff_.reset();
    attempts_ = 0;
    lastDisconnectReason = 0;

    if (!ntpStarted_) {
      // UTC: issued_at dan scanned_at memakai epoch, tanpa zona waktu.
      configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
      ntpStarted_ = true;
      logf("NTP", "sinkronisasi dimulai (%s)", NTP_SERVER_1);
    }
  } else if (!connected && connected_) {
    logf("WIFI", "putus, mencoba lagi");
    nextAttemptMs_ = nowMs;
  }

  connected_ = connected;

  if (!connected && static_cast<int32_t>(nowMs - nextAttemptMs_) >= 0) {
    startAttempt(nowMs);
  }

  if (!clockLogged_ && clockSynced()) {
    clockLogged_ = true;
    logf("NTP", "jam tersinkron: epoch %lld", static_cast<long long>(epochNow()));
  }
}

bool Network::clockSynced() {
  return ntpSynced && epochNow() >= MIN_VALID_EPOCH;
}

int64_t Network::epochNow() {
  return static_cast<int64_t>(time(nullptr));
}
