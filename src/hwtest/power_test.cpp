// power-test: servo buka-tutup terus selama ~1 menit sambil WiFi aktif dan
// mengirim paket terus-menerus (beban arus puncak: servo + radio TX).
//
//   pio run -e power-test -t upload && pio device monitor
//
// Lolos kalau sampai "SELESAI" tanpa baris BOOT baru di tengah jalan. Kalau
// muncul "Brownout detector was triggered" atau ESP32 restart, itu masalah
// daya: jangan dimatikan brownout detector-nya, perbaiki sumber dayanya.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "app/gate_servo.h"
#include "app/log.h"
#include "app/network.h"
#include "config.h"
#include "secrets_guard.h"

namespace {

constexpr uint32_t kTestDurationMs = 60000;
constexpr uint32_t kMoveIntervalMs = 800;
constexpr uint32_t kPacketIntervalMs = 20;
// Menunggu WiFi paling lama ini; setelahnya test tetap jalan tanpa WiFi.
constexpr uint32_t kWifiWaitMs = 20000;

enum class Phase { WaitingForWifi, Running, Done };

GateServo servo;
Network network;
WiFiUDP udp;
Phase phase = Phase::WaitingForWifi;
uint32_t phaseStartedMs = 0;
uint32_t lastMoveMs = 0;
uint32_t lastPacketMs = 0;
uint32_t moves = 0;
uint32_t packets = 0;
bool servoOpen = false;

void startRunning(uint32_t now) {
  phase = Phase::Running;
  phaseStartedMs = now;
  logf("POWER", "mulai: servo bergerak tiap %lu ms selama %lu detik, WiFi %s", static_cast<unsigned long>(kMoveIntervalMs),
       static_cast<unsigned long>(kTestDurationMs / 1000), network.wifiConnected() ? "aktif" : "TIDAK terhubung");
}

void sendPacket() {
  // Paket kecil ke port discard (9) di gateway: cukup untuk membuat radio TX
  // aktif terus, tanpa membebani jaringan.
  if (!network.wifiConnected()) {
    return;
  }

  udp.beginPacket(WiFi.gatewayIP(), 9);
  udp.write(reinterpret_cast<const uint8_t*>("two-guards power-test"), 21);

  if (udp.endPacket() == 1) {
    packets++;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  logBootBanner("Two-Guards power-test");

  servo.begin(SERVO_ANGLE_CLOSED);
  network.begin("two-guards-power-test", WIFI_SSID, WIFI_PASSWORD);
  phaseStartedMs = millis();
}

void loop() {
  const uint32_t now = millis();
  network.tick(now);

  switch (phase) {
    case Phase::WaitingForWifi:
      if (network.wifiConnected() || now - phaseStartedMs >= kWifiWaitMs) {
        startRunning(now);
      }

      break;

    case Phase::Running:
      if (now - lastPacketMs >= kPacketIntervalMs) {
        lastPacketMs = now;
        sendPacket();
      }

      if (now - lastMoveMs >= kMoveIntervalMs) {
        lastMoveMs = now;
        servoOpen = !servoOpen;
        servo.moveTo(servoOpen ? SERVO_ANGLE_OPEN : SERVO_ANGLE_CLOSED);
        moves++;

        if (moves % 5 == 0) {
          logf("POWER", "%2lu detik: %lu gerakan, %lu paket, RSSI %d dBm, heap %lu", static_cast<unsigned long>((now - phaseStartedMs) / 1000),
               static_cast<unsigned long>(moves), static_cast<unsigned long>(packets), network.wifiConnected() ? WiFi.RSSI() : 0,
               static_cast<unsigned long>(ESP.getFreeHeap()));
        }
      }

      if (now - phaseStartedMs >= kTestDurationMs) {
        servo.moveTo(SERVO_ANGLE_CLOSED);
        phase = Phase::Done;
        logf("POWER", "SELESAI tanpa reset: %lu gerakan, %lu paket. Servo kembali tutup.", static_cast<unsigned long>(moves),
             static_cast<unsigned long>(packets));
      }

      break;

    case Phase::Done:
      break;
  }

  delay(2);
}
