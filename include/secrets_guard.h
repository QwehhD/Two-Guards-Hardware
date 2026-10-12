#pragma once

// Pesan yang jelas kalau secrets.h belum dibuat, alih-alih error "file not found".
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "include/secrets.h belum ada. Salin include/secrets.example.h menjadi include/secrets.h lalu isi."
#endif
