// Unit test logika portal di laptop: pio test -e native

#include <unity.h>

#include <cstdio>
#include <cstring>

#include "backoff.h"
#include "device_command.h"
#include "portal_timer.h"
#include "rc522_version.h"
#include "recent_ids.h"
#include "scan_cooldown.h"
#include "uid_hex.h"

using namespace portal;

void setUp() {}
void tearDown() {}

// ---- uid_hex ----

void test_uid_is_uppercase_hex_without_separators_and_keeps_leading_zeros() {
  const uint8_t uid[] = {0x04, 0xA1, 0x0B, 0x3C};
  char out[kUidHexSize];

  TEST_ASSERT_EQUAL(8, formatUidHex(uid, sizeof uid, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("04A10B3C", out);
}

void test_uid_uses_every_byte_of_a_7_and_10_byte_uid() {
  const uint8_t seven[] = {0x04, 0x5F, 0x1A, 0x22, 0xB3, 0x6C, 0x80};
  const uint8_t ten[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xFF};
  char out[kUidHexSize];

  TEST_ASSERT_EQUAL(14, formatUidHex(seven, sizeof seven, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("045F1A22B36C80", out);

  TEST_ASSERT_EQUAL(20, formatUidHex(ten, sizeof ten, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("000102030405060708FF", out);
}

void test_uid_rejects_empty_oversized_and_small_buffers() {
  const uint8_t uid[11] = {0};
  char out[kUidHexSize] = "sisa";
  char small[8];

  TEST_ASSERT_EQUAL(0, formatUidHex(uid, 0, out, sizeof out));
  TEST_ASSERT_EQUAL_STRING("", out);

  TEST_ASSERT_EQUAL(0, formatUidHex(uid, 11, out, sizeof out));
  TEST_ASSERT_EQUAL(0, formatUidHex(uid, 4, small, sizeof small));
  TEST_ASSERT_EQUAL_STRING("", small);
}

// ---- parseCommand ----

void test_parses_open_and_deny() {
  DeviceCommand cmd{};
  const char open[] = R"({"action":"open","access_log_id":123,"issued_at":1791707464})";
  const char deny[] = R"({"action":"deny","access_log_id":7,"issued_at":1791707000})";

  TEST_ASSERT_EQUAL(ParseResult::Ok, parseCommand(open, strlen(open), cmd));
  TEST_ASSERT_EQUAL(CommandAction::Open, cmd.action);
  TEST_ASSERT_EQUAL_INT64(123, cmd.accessLogId);
  TEST_ASSERT_EQUAL_INT64(1791707464, cmd.issuedAt);

  TEST_ASSERT_EQUAL(ParseResult::Ok, parseCommand(deny, strlen(deny), cmd));
  TEST_ASSERT_EQUAL(CommandAction::Deny, cmd.action);
  TEST_ASSERT_EQUAL_INT64(7, cmd.accessLogId);
}

void test_rejects_malformed_commands() {
  DeviceCommand cmd{};
  const char* invalidJson[] = {"", "open", "[1,2]", R"({"action":"open")"};
  const char* invalidField[] = {
      R"({"access_log_id":1,"issued_at":1791707464})",
      R"({"action":"open","issued_at":1791707464})",
      R"({"action":"open","access_log_id":1})",
      R"({"action":"open","access_log_id":"1","issued_at":1791707464})",
      R"({"action":"open","access_log_id":1,"issued_at":"1791707464"})",
      R"({"action":"open","access_log_id":1,"issued_at":1791707464.5})",
      R"({"action":1,"access_log_id":1,"issued_at":1791707464})",
  };

  for (const char* payload : invalidJson) {
    TEST_ASSERT_EQUAL_MESSAGE(ParseResult::InvalidJson, parseCommand(payload, strlen(payload), cmd), payload);
  }

  for (const char* payload : invalidField) {
    TEST_ASSERT_EQUAL_MESSAGE(ParseResult::InvalidField, parseCommand(payload, strlen(payload), cmd), payload);
  }

  const char unknown[] = R"({"action":"OPEN","access_log_id":1,"issued_at":1791707464})";
  TEST_ASSERT_EQUAL(ParseResult::UnknownAction, parseCommand(unknown, strlen(unknown), cmd));
}

void test_parse_reads_only_the_given_length() {
  DeviceCommand cmd{};
  // Payload MQTT tidak diakhiri NUL; yang dibaca hanya sepanjang length.
  const char buffer[] = R"({"action":"open","access_log_id":5,"issued_at":100}GARBAGE)";
  const size_t length = strlen(R"({"action":"open","access_log_id":5,"issued_at":100})");

  TEST_ASSERT_EQUAL(ParseResult::Ok, parseCommand(buffer, length, cmd));
  TEST_ASSERT_EQUAL_INT64(5, cmd.accessLogId);
}

// ---- checkFreshness ----

void test_freshness_follows_the_contract() {
  const FreshnessRules rules;  // 15 detik ke belakang, 5 detik ke depan
  const int64_t now = 1791707464;

  TEST_ASSERT_EQUAL(Freshness::Fresh, checkFreshness(now, true, now, rules));
  TEST_ASSERT_EQUAL(Freshness::Fresh, checkFreshness(now - 15, true, now, rules));
  TEST_ASSERT_EQUAL(Freshness::Stale, checkFreshness(now - 16, true, now, rules));
  TEST_ASSERT_EQUAL(Freshness::Fresh, checkFreshness(now + 5, true, now, rules));
  TEST_ASSERT_EQUAL(Freshness::FromFuture, checkFreshness(now + 6, true, now, rules));
}

void test_freshness_ignores_everything_without_a_synced_clock() {
  const FreshnessRules rules;

  TEST_ASSERT_EQUAL(Freshness::ClockNotSynced, checkFreshness(1791707464, false, 1791707464, rules));
  // Jam ESP32 tanpa NTP mulai dari 1970: tanpa flag sinkron, command ini
  // akan dianggap "dari masa depan", bukan diabaikan karena jamnya.
  TEST_ASSERT_EQUAL(Freshness::ClockNotSynced, checkFreshness(1791707464, false, 12, rules));
}

// ---- RecentIds ----

void test_recent_ids_remembers_and_forgets_the_oldest() {
  RecentIds<3> ids;

  TEST_ASSERT_FALSE(ids.contains(1));

  ids.add(1);
  ids.add(2);
  ids.add(3);
  TEST_ASSERT_TRUE(ids.contains(1));
  TEST_ASSERT_TRUE(ids.contains(3));

  ids.add(4);  // menimpa 1
  TEST_ASSERT_FALSE(ids.contains(1));
  TEST_ASSERT_TRUE(ids.contains(2));
  TEST_ASSERT_TRUE(ids.contains(4));
}

void test_recent_ids_does_not_match_zero_before_anything_is_added() {
  RecentIds<4> ids;

  TEST_ASSERT_FALSE(ids.contains(0));
}

// ---- ScanCooldown ----

void test_cooldown_blocks_the_same_uid_until_it_has_been_away_long_enough() {
  ScanCooldown cooldown(3000);

  TEST_ASSERT_TRUE(cooldown.allow("AABBCCDD", 1000));
  TEST_ASSERT_FALSE(cooldown.allow("AABBCCDD", 2000));
  // Deteksi yang ditolak tetap menggeser jendela: 2000 + 3000.
  TEST_ASSERT_FALSE(cooldown.allow("AABBCCDD", 4500));
  TEST_ASSERT_TRUE(cooldown.allow("AABBCCDD", 7500));
}

void test_cooldown_is_per_uid() {
  ScanCooldown cooldown(3000);

  TEST_ASSERT_TRUE(cooldown.allow("AAAA", 0));
  TEST_ASSERT_TRUE(cooldown.allow("BBBB", 10));
  TEST_ASSERT_FALSE(cooldown.allow("AAAA", 20));
}

void test_cooldown_survives_millis_wraparound() {
  ScanCooldown cooldown(3000);

  TEST_ASSERT_TRUE(cooldown.allow("AAAA", 0xFFFFFF00u));
  TEST_ASSERT_FALSE(cooldown.allow("AAAA", 0x00000100u));  // 512 ms kemudian
  TEST_ASSERT_TRUE(cooldown.allow("AAAA", 0x00000100u + 3000));
}

void test_cooldown_evicts_the_least_recently_seen_uid_when_full() {
  ScanCooldown cooldown(3000);
  char uid[8];

  for (size_t i = 0; i < ScanCooldown::kSlots; i++) {
    snprintf(uid, sizeof uid, "U%u", static_cast<unsigned>(i));
    TEST_ASSERT_TRUE(cooldown.allow(uid, 100 + i));
  }

  // Slot penuh; U0 paling lama tidak terlihat dan tertimpa oleh UID baru.
  TEST_ASSERT_TRUE(cooldown.allow("BARU", 200));
  TEST_ASSERT_TRUE(cooldown.allow("U0", 300));
  // U2 masih diingat (U1 yang tertimpa oleh U0 barusan).
  TEST_ASSERT_FALSE(cooldown.allow("U2", 400));
}

// ---- PortalTimer ----

void test_portal_opens_and_closes_once_after_the_duration() {
  PortalTimer timer;

  TEST_ASSERT_TRUE(timer.open(1000, 5000));
  TEST_ASSERT_TRUE(timer.isOpen());
  TEST_ASSERT_FALSE(timer.closeIfDue(5999));
  TEST_ASSERT_TRUE(timer.closeIfDue(6000));
  TEST_ASSERT_FALSE(timer.isOpen());
  TEST_ASSERT_FALSE(timer.closeIfDue(7000));
}

void test_open_while_open_extends_the_timer() {
  PortalTimer timer;

  TEST_ASSERT_TRUE(timer.open(1000, 5000));
  TEST_ASSERT_FALSE(timer.open(4000, 5000));  // bukan pembukaan baru
  TEST_ASSERT_FALSE(timer.closeIfDue(6000));
  TEST_ASSERT_EQUAL_UINT32(3000, timer.remainingMs(6000));
  TEST_ASSERT_TRUE(timer.closeIfDue(9000));
}

void test_a_shorter_open_never_shortens_the_timer() {
  PortalTimer timer;

  timer.open(1000, 10000);
  timer.open(2000, 1000);
  TEST_ASSERT_FALSE(timer.closeIfDue(5000));
  TEST_ASSERT_TRUE(timer.closeIfDue(11000));
}

void test_portal_timer_survives_millis_wraparound() {
  PortalTimer timer;

  timer.open(0xFFFFF000u, 5000);  // tutup pada 0x00000388
  TEST_ASSERT_FALSE(timer.closeIfDue(0xFFFFFF00u));
  TEST_ASSERT_FALSE(timer.closeIfDue(0x00000100u));
  TEST_ASSERT_TRUE(timer.closeIfDue(0x00000388u));
}

// ---- Backoff ----

void test_backoff_doubles_up_to_the_maximum_and_resets() {
  Backoff backoff(1000, 30000);

  TEST_ASSERT_EQUAL_UINT32(1000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(2000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(4000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(8000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(16000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(30000, backoff.next());
  TEST_ASSERT_EQUAL_UINT32(30000, backoff.next());

  backoff.reset();
  TEST_ASSERT_EQUAL_UINT32(1000, backoff.next());
}

// ---- RC522 VersionReg ----

void test_rc522_version_flags_a_silent_chip() {
  TEST_ASSERT_EQUAL(Rc522Chip::NoResponse, describeRc522Version(0x00).chip);
  TEST_ASSERT_EQUAL(Rc522Chip::NoResponse, describeRc522Version(0xFF).chip);
  TEST_ASSERT_EQUAL(Rc522Chip::Genuine, describeRc522Version(0x92).chip);
  TEST_ASSERT_EQUAL(Rc522Chip::Clone, describeRc522Version(0xB2).chip);
  TEST_ASSERT_EQUAL(Rc522Chip::Unknown, describeRc522Version(0x42).chip);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_uid_is_uppercase_hex_without_separators_and_keeps_leading_zeros);
  RUN_TEST(test_uid_uses_every_byte_of_a_7_and_10_byte_uid);
  RUN_TEST(test_uid_rejects_empty_oversized_and_small_buffers);

  RUN_TEST(test_parses_open_and_deny);
  RUN_TEST(test_rejects_malformed_commands);
  RUN_TEST(test_parse_reads_only_the_given_length);

  RUN_TEST(test_freshness_follows_the_contract);
  RUN_TEST(test_freshness_ignores_everything_without_a_synced_clock);

  RUN_TEST(test_recent_ids_remembers_and_forgets_the_oldest);
  RUN_TEST(test_recent_ids_does_not_match_zero_before_anything_is_added);

  RUN_TEST(test_cooldown_blocks_the_same_uid_until_it_has_been_away_long_enough);
  RUN_TEST(test_cooldown_is_per_uid);
  RUN_TEST(test_cooldown_survives_millis_wraparound);
  RUN_TEST(test_cooldown_evicts_the_least_recently_seen_uid_when_full);

  RUN_TEST(test_portal_opens_and_closes_once_after_the_duration);
  RUN_TEST(test_open_while_open_extends_the_timer);
  RUN_TEST(test_a_shorter_open_never_shortens_the_timer);
  RUN_TEST(test_portal_timer_survives_millis_wraparound);

  RUN_TEST(test_backoff_doubles_up_to_the_maximum_and_resets);

  RUN_TEST(test_rc522_version_flags_a_silent_chip);

  return UNITY_END();
}
