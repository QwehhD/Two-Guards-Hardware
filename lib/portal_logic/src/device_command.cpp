#include "device_command.h"

#include <ArduinoJson.h>

#include <cstring>

namespace portal {

ParseResult parseCommand(const char* payload, size_t length, DeviceCommand& out) {
  JsonDocument doc;

  if (deserializeJson(doc, payload, length) != DeserializationError::Ok || !doc.is<JsonObject>()) {
    return ParseResult::InvalidJson;
  }

  JsonVariantConst action = doc["action"];
  JsonVariantConst accessLogId = doc["access_log_id"];
  JsonVariantConst issuedAt = doc["issued_at"];

  // is<int64_t>() menolak pecahan dan string, sama seperti server yang
  // hanya pernah mengirim integer.
  if (!action.is<const char*>() || !accessLogId.is<int64_t>() || !issuedAt.is<int64_t>()) {
    return ParseResult::InvalidField;
  }

  const char* name = action.as<const char*>();

  if (strcmp(name, "open") == 0) {
    out.action = CommandAction::Open;
  } else if (strcmp(name, "deny") == 0) {
    out.action = CommandAction::Deny;
  } else {
    return ParseResult::UnknownAction;
  }

  out.accessLogId = accessLogId.as<int64_t>();
  out.issuedAt = issuedAt.as<int64_t>();

  return ParseResult::Ok;
}

Freshness checkFreshness(int64_t issuedAt, bool clockSynced, int64_t now, const FreshnessRules& rules) {
  if (!clockSynced) {
    return Freshness::ClockNotSynced;
  }

  if (now - issuedAt > rules.maxAgeSeconds) {
    return Freshness::Stale;
  }

  if (issuedAt - now > rules.maxFutureSeconds) {
    return Freshness::FromFuture;
  }

  return Freshness::Fresh;
}

const char* parseResultName(ParseResult result) {
  switch (result) {
    case ParseResult::Ok:
      return "ok";
    case ParseResult::InvalidJson:
      return "bukan JSON objek";
    case ParseResult::InvalidField:
      return "field wajib hilang atau salah tipe";
    case ParseResult::UnknownAction:
      return "action tidak dikenal";
  }

  return "?";
}

const char* freshnessName(Freshness freshness) {
  switch (freshness) {
    case Freshness::Fresh:
      return "segar";
    case Freshness::ClockNotSynced:
      return "jam belum tersinkron NTP";
    case Freshness::Stale:
      return "basi";
    case Freshness::FromFuture:
      return "issued_at di masa depan";
  }

  return "?";
}

const char* actionName(CommandAction action) {
  return action == CommandAction::Open ? "open" : "deny";
}

}  // namespace portal
