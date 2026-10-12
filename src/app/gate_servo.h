#pragma once

#include <ESP32Servo.h>

// Servo palang portal. Dipakai firmware utama, servo-test, dan power-test.
class GateServo {
 public:
  // Attach lalu langsung ke sudut awal. Sampai write pertama, LEDC belum
  // mengeluarkan pulsa, jadi pulsa pertama yang diterima servo adalah sudut
  // ini (bukan posisi tengah bawaan library).
  bool begin(int initialAngle);

  void moveTo(int angle);
  int angle() const { return angle_; }

 private:
  Servo servo_;
  int angle_ = -1;
};
