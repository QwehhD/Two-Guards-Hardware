#include "gate_servo.h"

#include "config.h"
#include "log.h"

bool GateServo::begin(int initialAngle) {
  // Timer LEDC 0 untuk servo; sisanya tetap bebas.
  ESP32PWM::allocateTimer(0);
  servo_.setPeriodHertz(50);

  // attach() mengembalikan nomor channel, dan channel 0 sah; cek attached().
  servo_.attach(PIN_SERVO, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);

  if (!servo_.attached()) {
    logf("SERVO", "gagal attach ke GPIO%d", PIN_SERVO);

    return false;
  }

  moveTo(initialAngle);

  return true;
}

void GateServo::moveTo(int angle) {
  servo_.write(angle);
  angle_ = angle;
}
