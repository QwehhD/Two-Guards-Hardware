#pragma once

// Contoh secrets.h. Salin menjadi include/secrets.h lalu isi:
//
//   cp include/secrets.example.h include/secrets.h
//
// secrets.h sudah di-gitignore. Jangan pernah meng-commit-nya.

// WiFi 2.4 GHz (ESP32 tidak bisa 5 GHz).
constexpr const char* WIFI_SSID = "nama-wifi";
constexpr const char* WIFI_PASSWORD = "password-wifi";

// Broker Mosquitto: IP laptop/server di jaringan lokal, bukan localhost.
constexpr const char* MQTT_HOST = "192.168.2.53";
constexpr int MQTT_PORT = 1883;

// Akun device di password_file Mosquitto (lihat deploy/mosquitto/ di repo Two-Guards).
constexpr const char* MQTT_USERNAME = "device-1";
constexpr const char* MQTT_PASSWORD = "password-device-1";

// id di tabel devices. Menentukan topic parkir/{DEVICE_ID}/...
constexpr int DEVICE_ID = 1;
