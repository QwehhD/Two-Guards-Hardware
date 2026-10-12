#pragma once

#include <cstddef>
#include <cstdint>

namespace portal {

// Command dari server di parkir/{device_id}/command, lihat docs/mqtt-topics.md:
// {"action": "open", "access_log_id": 123, "issued_at": 1791707464}

enum class CommandAction : uint8_t { Open, Deny };

struct DeviceCommand {
  CommandAction action;
  int64_t accessLogId;
  int64_t issuedAt;
};

enum class ParseResult : uint8_t {
  Ok,
  InvalidJson,
  // Field wajib tidak ada atau tipenya salah (access_log_id dan issued_at harus integer).
  InvalidField,
  // action bukan "open" atau "deny".
  UnknownAction,
};

ParseResult parseCommand(const char* payload, size_t length, DeviceCommand& out);

enum class Freshness : uint8_t {
  Fresh,
  // Jam belum pernah tersinkron NTP: umur command tidak bisa dinilai.
  ClockNotSynced,
  // now - issued_at > maxAgeSeconds.
  Stale,
  // issued_at - now > maxFutureSeconds.
  FromFuture,
};

struct FreshnessRules {
  // 15 = TTL server 10 detik + toleransi selisih jam 5 detik.
  int64_t maxAgeSeconds = 15;
  int64_t maxFutureSeconds = 5;
};

Freshness checkFreshness(int64_t issuedAt, bool clockSynced, int64_t now, const FreshnessRules& rules);

const char* parseResultName(ParseResult result);
const char* freshnessName(Freshness freshness);
const char* actionName(CommandAction action);

}  // namespace portal
