#include "mqtt_link.h"

#include <cstdio>
#include <cstring>

#include "log.h"

namespace {

const char* returnCodeHint(lwmqtt_return_code_t code) {
  switch (code) {
    case LWMQTT_CONNECTION_ACCEPTED:
      return "diterima";
    case LWMQTT_IDENTIFIER_REJECTED:
      return "client ID ditolak";
    case LWMQTT_SERVER_UNAVAILABLE:
      return "broker tidak tersedia";
    case LWMQTT_BAD_USERNAME_OR_PASSWORD:
    case LWMQTT_NOT_AUTHORIZED:
      return "username/password ditolak, cek secrets.h dan password_file";
    default:
      return "lihat lwmqtt_return_code_t";
  }
}

const char* errorHint(lwmqtt_err_t error) {
  switch (error) {
    case LWMQTT_NETWORK_FAILED_CONNECT:
      return "TCP gagal: broker mati, IP salah, atau port 1883 diblok firewall";
    case LWMQTT_NETWORK_TIMEOUT:
      return "timeout menunggu broker";
    case LWMQTT_CONNECTION_DENIED:
      return "CONNACK menolak";
    case LWMQTT_FAILED_SUBSCRIPTION:
      return "SUBACK menolak, cek ACL";
    case LWMQTT_PONG_TIMEOUT:
      return "broker tidak membalas PINGREQ";
    case LWMQTT_NETWORK_FAILED_READ:
    case LWMQTT_NETWORK_FAILED_WRITE:
      return "koneksi TCP terputus";
    default:
      return "lihat lwmqtt_err_t";
  }
}

}  // namespace

void MqttLink::begin(int deviceId, const char* host, int port, const char* username, const char* password) {
  username_ = username;
  password_ = password;

  snprintf(clientId_, sizeof clientId_, "two-guards-device-%d", deviceId);
  snprintf(scanTopic_, sizeof scanTopic_, "parkir/%d/scan", deviceId);
  snprintf(statusTopic_, sizeof statusTopic_, "parkir/%d/status", deviceId);
  snprintf(commandTopic_, sizeof commandTopic_, "parkir/%d/command", deviceId);

  client_.begin(host, port, net_);
  // LWT tanpa portal_status: device yang putus tidak tahu posisi palang.
  // Tidak retained, sesuai tabel di mqtt-topics.md.
  client_.setWill(statusTopic_, "{\"online\":false}", false, 1);
  // Clean session: broker tidak menimbun command selama device offline.
  client_.setOptions(MQTT_KEEPALIVE_S, true, MQTT_TIMEOUT_MS);
  client_.onMessageAdvanced([this](MQTTClient*, char topic[], char bytes[], int length) { onMessage(topic, bytes, length); });

  logf("MQTT", "broker %s:%d, user %s, client ID %s", host, port, username, clientId_);
}

void MqttLink::tick(uint32_t nowMs, bool wifiUp, bool allowConnect, bool portalOpen) {
  if (client_.connected()) {
    client_.loop();
  }

  const bool connectedNow = client_.connected();

  if (wasConnected_ && !connectedNow) {
    const lwmqtt_err_t error = client_.lastError();
    logf("MQTT", "putus (error %d: %s)", error, errorHint(error));
    nextAttemptMs_ = nowMs;
  }

  wasConnected_ = connectedNow;

  if (connectedNow) {
    if (nowMs - lastStatusMs_ >= HEARTBEAT_INTERVAL_MS) {
      publishStatus(portalOpen);
    }

    return;
  }

  if (!wifiUp || !allowConnect || static_cast<int32_t>(nowMs - nextAttemptMs_) < 0) {
    return;
  }

  if (connectNow(portalOpen)) {
    backoff_.reset();
    wasConnected_ = true;

    return;
  }

  const uint32_t delay = backoff_.next();
  nextAttemptMs_ = millis() + delay;
  logf("MQTT", "coba lagi dalam %lu detik", static_cast<unsigned long>(delay / 1000));
}

bool MqttLink::connectNow(bool portalOpen) {
  const uint32_t started = millis();

  if (!client_.connect(clientId_, username_, password_)) {
    const lwmqtt_err_t error = client_.lastError();

    if (error == LWMQTT_CONNECTION_DENIED) {
      const lwmqtt_return_code_t code = client_.returnCode();
      logf("MQTT", "konek ditolak broker (return code %d: %s)", code, returnCodeHint(code));
    } else {
      logf("MQTT", "konek gagal (error %d: %s)", error, errorHint(error));
    }

    return false;
  }

  logf("MQTT", "terhubung dalam %lu ms (clean session, LWT di %s)", static_cast<unsigned long>(millis() - started), statusTopic_);

  // Urutan boot: status dulu, baru subscribe command. Kalau salah satunya
  // gagal, putus tanpa DISCONNECT supaya broker tetap mengirim LWT.
  if (!publishStatus(portalOpen)) {
    net_.stop();

    return false;
  }

  if (!client_.subscribe(commandTopic_, 1)) {
    const lwmqtt_err_t error = client_.lastError();
    logf("MQTT", "subscribe %s gagal (error %d: %s)", commandTopic_, error, errorHint(error));
    net_.stop();

    return false;
  }

  logf("MQTT", "subscribe %s (QoS 1)", commandTopic_);

  return true;
}

bool MqttLink::publishScan(const char* uidHex, int64_t scannedAt) {
  // UID hanya berisi 0-9 dan A-F, jadi aman disisipkan tanpa escape JSON.
  char payload[96];
  snprintf(payload, sizeof payload, "{\"uid\":\"%s\",\"scanned_at\":%lld}", uidHex, static_cast<long long>(scannedAt));

  return publish(scanTopic_, payload);
}

bool MqttLink::publishStatus(bool portalOpen) {
  const char* payload = portalOpen ? "{\"online\":true,\"portal_status\":\"open\"}" : "{\"online\":true,\"portal_status\":\"closed\"}";

  if (!publish(statusTopic_, payload)) {
    return false;
  }

  lastStatusMs_ = millis();
  logf("STAT", "%s", portalOpen ? "online, open" : "online, closed");

  return true;
}

bool MqttLink::publish(const char* topic, const char* payload) {
  if (!client_.connected()) {
    return false;
  }

  // QoS 1: menunggu PUBACK sampai MQTT_TIMEOUT_MS. Kalau gagal, library
  // menutup koneksi; pesan ini tidak dikirim ulang.
  if (!client_.publish(topic, payload, static_cast<int>(strlen(payload)), false, 1)) {
    const lwmqtt_err_t error = client_.lastError();
    logf("MQTT", "publish ke %s gagal (error %d: %s)", topic, error, errorHint(error));

    return false;
  }

  return true;
}

void MqttLink::onMessage(char* topic, char* bytes, int length) {
  if (strcmp(topic, commandTopic_) != 0) {
    logf("MQTT", "pesan di topic tak terduga %s, diabaikan", topic);

    return;
  }

  if (length < 0 || static_cast<size_t>(length) >= kInboxPayloadSize) {
    logf("CMD", "payload %d byte terlalu panjang, diabaikan", length);

    return;
  }

  if (inboxCount_ == kInboxSlots) {
    logf("CMD", "antrean command penuh, command diabaikan");

    return;
  }

  InboxEntry& entry = inbox_[(inboxHead_ + inboxCount_) % kInboxSlots];
  memcpy(entry.payload, bytes, static_cast<size_t>(length));
  entry.payload[length] = '\0';
  entry.length = static_cast<size_t>(length);
  inboxCount_++;
}

bool MqttLink::popCommand(char* payload, size_t size, size_t& length) {
  if (inboxCount_ == 0) {
    return false;
  }

  const InboxEntry& entry = inbox_[inboxHead_];
  length = entry.length < size ? entry.length : size - 1;
  memcpy(payload, entry.payload, length);
  payload[length] = '\0';

  inboxHead_ = (inboxHead_ + 1) % kInboxSlots;
  inboxCount_--;

  return true;
}
