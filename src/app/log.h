#pragma once

#include <Arduino.h>

#include <cstdarg>

// Satu baris per event: "[   12.345] TAG   pesan", dengan waktu sejak boot.
// Untuk mengukur latensi, pio device monitor menambahkan jam laptop di depan
// tiap baris (filter "time" di platformio.ini).
inline void logf(const char* tag, const char* format, ...) __attribute__((format(printf, 2, 3)));

inline void logf(const char* tag, const char* format, ...) {
  char message[256];
  va_list args;
  va_start(args, format);
  vsnprintf(message, sizeof message, format, args);
  va_end(args);

  const uint32_t now = millis();
  Serial.printf("[%6lu.%03lu] %-5s %s\n", static_cast<unsigned long>(now / 1000), static_cast<unsigned long>(now % 1000), tag,
                message);
}

// Alasan reset terakhir, terutama untuk mengenali brownout (masalah daya).
inline const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "POWERON (daya baru menyala)";
    case ESP_RST_EXT:
      return "EXT (pin EN)";
    case ESP_RST_SW:
      return "SW (esp_restart)";
    case ESP_RST_PANIC:
      return "PANIC (crash)";
    case ESP_RST_INT_WDT:
      return "INT_WDT (watchdog interrupt)";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT (loop macet)";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT (tegangan turun, masalah daya!)";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
}

inline void logBootBanner(const char* name) {
  const esp_reset_reason_t reason = esp_reset_reason();

  Serial.println();
  logf("BOOT", "%s, reset terakhir: %s", name, resetReasonName(reason));

  if (reason == ESP_RST_BROWNOUT) {
    logf("BOOT", "!!! BROWNOUT: ESP32 restart karena tegangan turun. Ini masalah daya, bukan bug firmware.");
  }
}
