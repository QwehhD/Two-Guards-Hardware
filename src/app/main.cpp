// Firmware portal parkir Two-Guards: ESP32 + RC522 + servo palang.
//
// Kontrak dengan server: Two-Guards/docs/mqtt-topics.md.
//
// Prinsip gagal-tertutup: palang hanya terbuka karena command open yang
// segar, belum pernah diproses, dan jamnya tersinkron; selalu menutup sendiri
// lewat timer lokal yang tidak bergantung pada WiFi/MQTT; dan setiap boot
// (termasuk reset oleh watchdog) menutup palang sebelum apa pun.

#include <Arduino.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "device_command.h"
#include "gate_servo.h"
#include "log.h"
#include "mqtt_link.h"
#include "network.h"
#include "portal_timer.h"
#include "recent_ids.h"
#include "rfid_reader.h"
#include "scan_cooldown.h"
#include "secrets_guard.h"

namespace {

// Sesudah WiFi tersambung, MQTT menunggu NTP paling lama ini sebelum tetap konek.
constexpr uint32_t kNtpGraceMs = 5000;
constexpr uint32_t kCooldownLogIntervalMs = 1000;

GateServo servo;
portal::PortalTimer portalTimer;
RfidReader reader;
Network network;
MqttLink mqtt;
portal::ScanCooldown cooldown(SCAN_COOLDOWN_MS);
portal::RecentIds<COMMAND_DEDUPE_SIZE> handledCommands;
portal::FreshnessRules freshnessRules;

uint32_t lastPollMs = 0;
uint32_t lastCooldownLogMs = 0;

void openPortal(uint32_t now) {
  if (!portalTimer.open(now, PORTAL_OPEN_DURATION_MS)) {
    logf("GATE", "masih terbuka, tutup otomatis diperpanjang %lu ms", static_cast<unsigned long>(PORTAL_OPEN_DURATION_MS));

    return;
  }

  servo.moveTo(SERVO_ANGLE_OPEN);
  logf("GATE", "BUKA (%d derajat), tutup otomatis dalam %lu ms", SERVO_ANGLE_OPEN, static_cast<unsigned long>(PORTAL_OPEN_DURATION_MS));
  mqtt.publishStatus(true);
}

void closePortalIfDue(uint32_t now) {
  if (!portalTimer.closeIfDue(now)) {
    return;
  }

  servo.moveTo(SERVO_ANGLE_CLOSED);
  logf("GATE", "TUTUP (%d derajat)", SERVO_ANGLE_CLOSED);
  mqtt.publishStatus(false);
}

void handleCommand(const char* payload, size_t length, uint32_t now) {
  portal::DeviceCommand command{};
  const portal::ParseResult parsed = portal::parseCommand(payload, length, command);

  if (parsed != portal::ParseResult::Ok) {
    logf("CMD", "diabaikan (%s): %s", portal::parseResultName(parsed), payload);

    return;
  }

  const char* action = portal::actionName(command.action);
  const long long id = static_cast<long long>(command.accessLogId);
  const bool synced = Network::clockSynced();
  const int64_t epoch = Network::epochNow();
  const portal::Freshness freshness = portal::checkFreshness(command.issuedAt, synced, epoch, freshnessRules);

  if (freshness == portal::Freshness::ClockNotSynced) {
    logf("CMD", "%s #%lld diabaikan: %s", action, id, portal::freshnessName(freshness));

    return;
  }

  const long long age = static_cast<long long>(epoch - command.issuedAt);

  if (freshness != portal::Freshness::Fresh) {
    logf("CMD", "%s #%lld diabaikan: %s (umur %lld detik)", action, id, portal::freshnessName(freshness), age);

    return;
  }

  if (handledCommands.contains(command.accessLogId)) {
    logf("CMD", "%s #%lld diabaikan: duplikat", action, id);

    return;
  }

  handledCommands.add(command.accessLogId);

  if (command.action == portal::CommandAction::Open) {
    logf("CMD", "open #%lld (umur %lld detik)", id, age);
    openPortal(now);
  } else {
    logf("CMD", "deny #%lld (umur %lld detik): palang tetap tertutup", id, age);
  }
}

void processCommands(uint32_t now) {
  char payload[200];
  size_t length = 0;

  while (mqtt.popCommand(payload, sizeof payload, length)) {
    handleCommand(payload, length, now);
  }
}

void handleScan(uint32_t now) {
  if (now - lastPollMs < RFID_POLL_INTERVAL_MS) {
    return;
  }

  lastPollMs = now;

  char uid[portal::kUidHexSize];

  if (!reader.poll(uid, sizeof uid)) {
    return;
  }

  if (!cooldown.allow(uid, now)) {
    // Kartu yang ditahan bisa terbaca berulang; cukup satu baris per detik.
    if (now - lastCooldownLogMs >= kCooldownLogIntervalMs) {
      lastCooldownLogMs = now;
      logf("SCAN", "%s diabaikan (cooldown %lu ms)", uid, static_cast<unsigned long>(SCAN_COOLDOWN_MS));
    }

    return;
  }

  // Tidak ada antrean: scan yang tertunda bisa membuka gerbang terlambat.
  if (!Network::clockSynced()) {
    logf("SCAN", "%s tidak dikirim: jam belum tersinkron NTP", uid);

    return;
  }

  if (!mqtt.connected()) {
    logf("SCAN", "%s tidak dikirim: MQTT offline", uid);

    return;
  }

  const int64_t scannedAt = Network::epochNow();

  if (mqtt.publishScan(uid, scannedAt)) {
    logf("SCAN", "%s terkirim (scanned_at %lld)", uid, static_cast<long long>(scannedAt));
  } else {
    logf("SCAN", "%s gagal terkirim, tidak diulang", uid);
  }
}

bool mqttMayConnect(uint32_t now) {
  // Konek MQTT bisa memblok beberapa detik; jangan sampai itu menunda
  // penutupan palang.
  if (portalTimer.isOpen()) {
    return false;
  }

  // Urutan boot WiFi -> NTP -> MQTT, tapi NTP yang lambat tidak menahan
  // MQTT selamanya: device tetap online, hanya scan dan command yang ditolak.
  return Network::clockSynced() || now - network.connectedSinceMs() >= kNtpGraceMs;
}

}  // namespace

void setup() {
  // Palang ke posisi tutup sebelum apa pun.
  servo.begin(SERVO_ANGLE_CLOSED);

  Serial.begin(115200);
  delay(200);
  logBootBanner("Two-Guards portal");
  logf("BOOT", "device %d, servo tutup %d / buka %d derajat, buka %lu ms, cooldown scan %lu ms", DEVICE_ID, SERVO_ANGLE_CLOSED,
       SERVO_ANGLE_OPEN, static_cast<unsigned long>(PORTAL_OPEN_DURATION_MS), static_cast<unsigned long>(SCAN_COOLDOWN_MS));

  freshnessRules.maxAgeSeconds = COMMAND_MAX_AGE_S;
  freshnessRules.maxFutureSeconds = COMMAND_MAX_FUTURE_S;

  // Loop yang macet me-reset ESP32, dan boot berikutnya menutup palang.
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);

  // Chip yang belum menjawab dicoba lagi oleh healthCheck() di loop.
  reader.begin();

  char hostname[32];
  snprintf(hostname, sizeof hostname, "two-guards-device-%d", DEVICE_ID);
  network.begin(hostname, WIFI_SSID, WIFI_PASSWORD);
  mqtt.begin(DEVICE_ID, MQTT_HOST, MQTT_PORT, MQTT_USERNAME, MQTT_PASSWORD);
}

void loop() {
  esp_task_wdt_reset();

  const uint32_t now = millis();

  network.tick(now);
  mqtt.tick(now, network.wifiConnected(), mqttMayConnect(now), portalTimer.isOpen());
  processCommands(millis());
  closePortalIfDue(millis());
  handleScan(millis());
  reader.healthCheck(millis(), RFID_HEALTH_INTERVAL_MS);

  delay(1);
}
