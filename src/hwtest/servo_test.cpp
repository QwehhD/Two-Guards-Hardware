// servo-test: gerakkan servo tutup -> buka -> tutup beberapa kali dengan
// sudut dari config.h, lalu terima perintah lewat serial untuk mencari sudut
// yang pas tanpa upload ulang.
//
//   pio run -e servo-test -t upload && pio device monitor
//
// Perintah (ketik lalu Enter):
//   0..180  pindah ke sudut itu
//   c       ulangi siklus tutup-buka-tutup
//   t / b   ke sudut tutup / buka dari config.h

#include <Arduino.h>

#include "app/gate_servo.h"
#include "app/log.h"
#include "config.h"

namespace {

constexpr int kCycles = 3;
constexpr uint32_t kHoldMs = 1500;

GateServo servo;
String line;

void moveAndLog(int angle, const char* label) {
  servo.moveTo(angle);
  logf("SERVO", "%s: %d derajat", label, angle);
}

void runCycles() {
  logf("SERVO", "siklus tutup(%d) -> buka(%d) -> tutup, %d kali", SERVO_ANGLE_CLOSED, SERVO_ANGLE_OPEN, kCycles);

  for (int i = 1; i <= kCycles; i++) {
    moveAndLog(SERVO_ANGLE_OPEN, "buka");
    delay(kHoldMs);
    moveAndLog(SERVO_ANGLE_CLOSED, "tutup");
    delay(kHoldMs);
  }

  logf("SERVO", "selesai. Ketik 0..180, c (siklus), t (tutup), b (buka).");
}

void handleLine(String input) {
  input.trim();

  if (input.isEmpty()) {
    return;
  }

  if (input == "c") {
    runCycles();
  } else if (input == "t") {
    moveAndLog(SERVO_ANGLE_CLOSED, "tutup");
  } else if (input == "b") {
    moveAndLog(SERVO_ANGLE_OPEN, "buka");
  } else {
    const int angle = input.toInt();

    if ((angle == 0 && input != "0") || angle < 0 || angle > 180) {
      logf("SERVO", "perintah tidak dikenal: \"%s\"", input.c_str());

      return;
    }

    moveAndLog(angle, "manual");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  logBootBanner("Two-Guards servo-test");

  if (!servo.begin(SERVO_ANGLE_CLOSED)) {
    return;
  }

  logf("SERVO", "posisi awal tutup: %d derajat (pulsa %d-%d us)", SERVO_ANGLE_CLOSED, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
  delay(1000);
  runCycles();
}

void loop() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\n' || c == '\r') {
      handleLine(line);
      line = "";
    } else if (line.length() < 16) {
      line += c;
    }
  }

  delay(10);
}
