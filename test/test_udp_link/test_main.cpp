#include <Arduino.h>
#include <unity.h>

#include "udp_link.h"

// Same bytes as host/tests/test_udp_link.py (Python seal() of the bridge).
static const char *VECTOR = "DK1 c74d1eaa068ef4ec4cf0fec56e1f112b 1700000000 9 0 1 {\"type\":\"ack\",\"seq\":5}";

void test_published_hmac_vector() {
  char mac[33];
  const char *msg = "The quick brown fox jumps over the lazy dog";
  linkMac("key", msg, strlen(msg), mac);
  TEST_ASSERT_EQUAL_STRING("f7bc83f430538424b13298e6aa6fb143", mac);
}

void test_opens_a_datagram_from_the_bridge() {
  LinkEnvelope env;
  TEST_ASSERT_TRUE(linkOpen("key", VECTOR, strlen(VECTOR), env));
  TEST_ASSERT_EQUAL_UINT32(1700000000UL, env.ts);
  TEST_ASSERT_EQUAL_UINT32(9, env.seq);
  TEST_ASSERT_EQUAL_UINT8(1, env.parts);
  TEST_ASSERT_EQUAL(strlen("{\"type\":\"ack\",\"seq\":5}"), env.chunkLen);
  TEST_ASSERT_EQUAL_INT(0, strncmp("{\"type\":\"ack\"", env.chunk, 13));
}

void test_seal_matches_the_bridge() {
  char out[LINK_DATAGRAM_MAX];
  const char *frame = "{\"type\":\"ack\",\"seq\":5}";
  const size_t n = linkSeal("key", 1700000000UL, 9, 0, 1, frame, strlen(frame), out, sizeof(out));
  TEST_ASSERT_EQUAL(strlen(VECTOR), n);
  TEST_ASSERT_EQUAL_INT(0, memcmp(VECTOR, out, n));
}

void test_rejects_wrong_token_and_tampering() {
  LinkEnvelope env;
  TEST_ASSERT_FALSE(linkOpen("other", VECTOR, strlen(VECTOR), env));
  String bad = VECTOR;
  bad.replace("\"seq\":5", "\"seq\":6");
  TEST_ASSERT_FALSE(linkOpen("key", bad.c_str(), bad.length(), env));
  TEST_ASSERT_FALSE(linkOpen("", VECTOR, strlen(VECTOR), env));
  const char *plain = "{\"token\":\"x\",\"app\":\"a\"}";
  TEST_ASSERT_FALSE(linkOpen("key", plain, strlen(plain), env));
}

void test_reassembles_parts_in_any_order() {
  char a[LINK_DATAGRAM_MAX], b[LINK_DATAGRAM_MAX];
  const size_t na = linkSeal("key", 5, 2, 0, 2, "{\"type\":", 8, a, sizeof(a));
  const size_t nb = linkSeal("key", 5, 2, 1, 2, "\"x\"}", 4, b, sizeof(b));
  LinkEnvelope ea, eb;
  TEST_ASSERT_TRUE(linkOpen("key", a, na, ea));
  TEST_ASSERT_TRUE(linkOpen("key", b, nb, eb));
  LinkReassembler r;
  String frame;
  TEST_ASSERT_FALSE(r.feed(eb, 0, frame));
  TEST_ASSERT_TRUE(r.feed(ea, 10, frame));
  TEST_ASSERT_EQUAL_STRING("{\"type\":\"x\"}", frame.c_str());
}

void test_replay_guard() {
  LinkReplayGuard g;
  TEST_ASSERT_TRUE(g.fresh(1000, 1100));
  TEST_ASSERT_FALSE(g.fresh(1000, 1200));
  TEST_ASSERT_FALSE(g.seen(1000, 1));
  g.remember(1000, 1);
  TEST_ASSERT_TRUE(g.seen(1000, 1));
  TEST_ASSERT_FALSE(g.seen(1000, 2));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_published_hmac_vector);
  RUN_TEST(test_opens_a_datagram_from_the_bridge);
  RUN_TEST(test_seal_matches_the_bridge);
  RUN_TEST(test_rejects_wrong_token_and_tampering);
  RUN_TEST(test_reassembles_parts_in_any_order);
  RUN_TEST(test_replay_guard);
  UNITY_END();
}

void loop() {}
