#include <Arduino.h>
#include <unity.h>

#include "aram.h"
#include "net_notify.h"

static const String TOKEN = "s3cret-token-for-tests";

static const char *FULL = R"({"token":"s3cret-token-for-tests","type":"aram","seq":7,"ig":1,
  "ch":"Yasuo","tr":"S+","wr":"58.3%",
  "of":[["Bầy Ong Nảy Bật","71.0%","+3 A",1],["Rút Kiếm Ra","57.3%","",0],["Khổng Nhân Can Đảm","50.6%","+4 S",0]],
  "cb":["S","Cú Đấm Thần Bí + Khổng Nhân Can Đảm + Quăng Quật","1/3"],
  "ct":[["Lời Nhắc Tử Vong","3/5 địch hồi máu"],["Đao Thủy Ngân","65% sát thương phép"]],
  "bd":"Nghi Thức Suy Vong > Gươm Suy Vong","ow":"Cú Đấm Thần Bí"})";

void test_full_packet_is_parsed() {
  AramData d;
  TEST_ASSERT_TRUE(parseUdpAram(FULL, TOKEN, d));
  TEST_ASSERT_TRUE(d.inGame);
  TEST_ASSERT_EQUAL_UINT32(7, d.seq);
  TEST_ASSERT_EQUAL_STRING("Yasuo", d.champion.c_str());
  TEST_ASSERT_EQUAL_STRING("S+", d.tier.c_str());
  TEST_ASSERT_EQUAL_UINT8(3, d.pickCount);
  TEST_ASSERT_EQUAL_STRING("Bầy Ong Nảy Bật", d.picks[0].name.c_str());
  TEST_ASSERT_TRUE(d.picks[0].best);
  TEST_ASSERT_FALSE(d.picks[1].best);
  TEST_ASSERT_EQUAL_STRING("+4 S", d.picks[2].note.c_str());
  TEST_ASSERT_EQUAL_STRING("S", d.comboTier.c_str());
  TEST_ASSERT_EQUAL_STRING("1/3", d.comboHave.c_str());
  TEST_ASSERT_EQUAL_UINT8(2, d.counterCount);
  TEST_ASSERT_EQUAL_STRING("Lời Nhắc Tử Vong", d.counters[0].title.c_str());
  TEST_ASSERT_EQUAL_STRING("Nghi Thức Suy Vong > Gươm Suy Vong", d.build.c_str());
}

void test_minimal_packet_without_offer() {
  AramData d;
  TEST_ASSERT_TRUE(parseUdpAram(R"({"token":"s3cret-token-for-tests","type":"aram","ig":0})", TOKEN, d));
  TEST_ASSERT_FALSE(d.inGame);
  TEST_ASSERT_EQUAL_UINT8(0, d.pickCount);
  TEST_ASSERT_EQUAL_UINT8(0, d.counterCount);
  TEST_ASSERT_EQUAL_STRING("", d.combo.c_str());
}

void test_extra_rows_are_capped() {
  AramData d;
  TEST_ASSERT_TRUE(parseUdpAram(
      R"({"token":"s3cret-token-for-tests","type":"aram","ig":1,"of":[["a"],["b"],["c"],["d"]],"ct":[["x"],["y"],["z"]]})",
      TOKEN, d));
  TEST_ASSERT_EQUAL_UINT8(ARAM_PICKS, d.pickCount);
  TEST_ASSERT_EQUAL_UINT8(ARAM_COUNTERS, d.counterCount);
}

void test_wrong_token_or_type_is_rejected() {
  AramData d;
  TEST_ASSERT_FALSE(parseUdpAram(R"({"token":"nope","type":"aram","ig":1})", TOKEN, d));
  TEST_ASSERT_FALSE(parseUdpAram(R"({"type":"aram","ig":1})", TOKEN, d));
  TEST_ASSERT_FALSE(parseUdpAram(R"({"token":"s3cret-token-for-tests","app":"A","body":"B"})", TOKEN, d));
  TEST_ASSERT_FALSE(parseUdpAram("not json", TOKEN, d));
  TEST_ASSERT_FALSE(parseUdpAram(R"({"token":"","type":"aram"})", String(""), d));
}

void test_notification_parser_still_accepts_notifications() {
  String app, from, body;
  TEST_ASSERT_TRUE(parseUdpNotify(R"({"token":"s3cret-token-for-tests","app":"Teams","from":"A","body":"B"})",
                                  TOKEN, app, from, body));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_full_packet_is_parsed);
  RUN_TEST(test_minimal_packet_without_offer);
  RUN_TEST(test_extra_rows_are_capped);
  RUN_TEST(test_wrong_token_or_type_is_rejected);
  RUN_TEST(test_notification_parser_still_accepts_notifications);
  UNITY_END();
}

void loop() {}
