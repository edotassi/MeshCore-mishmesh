#include <gtest/gtest.h>
#include <mishmesh/core/AppletStorage.h>
#include <mishmesh/core/BatteryHistory.h>
#include <string.h>

using namespace mishmesh;

static const uint32_t HALF_HOUR = BatteryHistory::BUCKET_SECS;
static const int N = BatteryHistory::BUCKETS;

namespace {
struct FakeStorage : AppletStorage {
  uint8_t data[256] = {0};
  uint8_t len = 0;
  bool    has = false;
  uint8_t load(const char*, uint8_t* dst, uint8_t cap) override {
    if (!has) return 0;
    uint8_t n = len < cap ? len : cap;
    memcpy(dst, data, n);
    return n;
  }
  bool save(const char*, const uint8_t* src, uint8_t l) override {
    len = l < sizeof(data) ? l : (uint8_t)sizeof(data);
    memcpy(data, src, len);
    has = true;
    return true;
  }
};
}  // namespace

// A sample while the clock is unsynced (epoch 0) is skipped rather than
// corrupting the bucket math.
TEST(BatteryHistory, UnsyncedClockIsSkipped) {
  BatteryHistory h;
  h.tick(0, 4000);
  EXPECT_FALSE(h.primed());
}

TEST(BatteryHistory, FirstTickPrimesAndRecordsCurrentBucket) {
  BatteryHistory h;
  h.tick(1000, 4100);
  EXPECT_TRUE(h.primed());
  EXPECT_EQ(4100, h.bucket(N - 1));   // current = last chronological slot
  EXPECT_EQ(0, h.bucket(0));          // rest still unsampled
}

// Repeated ticks within the same bucket window just overwrite it (no
// rollover), matching a gauge (not a cumulative counter).
TEST(BatteryHistory, SameWindowOverwritesCurrentBucket) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + HALF_HOUR - 1, 4050);
  EXPECT_EQ(4050, h.bucket(N - 1));
}

TEST(BatteryHistory, RolloverAdvancesOneBucket) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + HALF_HOUR, 4050);
  EXPECT_EQ(4050, h.bucket(N - 1));   // new current
  EXPECT_EQ(4100, h.bucket(N - 2));   // previous minute retained
}

// A gap (device was off) carries the last known reading forward into the
// skipped buckets instead of leaving them at the "unsampled" 0 sentinel -
// the pack doesn't actually discharge to 0V while the screen is off.
TEST(BatteryHistory, GapCarriesLastValueForward) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + 5 * HALF_HOUR, 3900);   // 5 buckets elapsed
  EXPECT_EQ(3900, h.bucket(N - 1));
  EXPECT_EQ(4100, h.bucket(N - 2));   // carried forward, not 0
  EXPECT_EQ(4100, h.bucket(N - 3));
  EXPECT_EQ(4100, h.bucket(N - 4));
  EXPECT_EQ(4100, h.bucket(N - 5));
}

// A gap far longer than the whole window is bounded (never hangs/overflows)
// and just floods the ring with the last known reading.
TEST(BatteryHistory, HugeGapIsBoundedNotInfinite) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + (uint32_t)(N + 50) * HALF_HOUR, 3800);
  EXPECT_EQ(3800, h.bucket(N - 1));
  EXPECT_EQ(4100, h.bucket(0));   // everything else flooded with the prior value
}

// A clock that jumps backward (rare, but RTC edge cases happen) doesn't
// underflow the unsigned bucket math - it just overwrites the current bucket.
TEST(BatteryHistory, ClockGoingBackwardDoesNotUnderflow) {
  BatteryHistory h;
  h.tick(10000, 4100);
  h.tick(9000, 4050);   // earlier than _bucketStart
  EXPECT_EQ(4050, h.bucket(N - 1));
}

TEST(BatteryHistory, MinMaxIgnoreUnsampledBuckets) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + HALF_HOUR, 3900);
  h.tick(1000 + 2 * HALF_HOUR, 4200);
  EXPECT_EQ(3900, h.minMv());
  EXPECT_EQ(4200, h.maxMv());
}

TEST(BatteryHistory, MinMaxZeroWhenNeverSampled) {
  BatteryHistory h;
  EXPECT_EQ(0, h.minMv());
  EXPECT_EQ(0, h.maxMv());
}

TEST(BatteryHistory, SaveThenLoadRoundTrips) {
  BatteryHistory h;
  h.tick(1000, 4100);
  h.tick(1000 + HALF_HOUR, 4050);
  h.tick(1000 + 2 * HALF_HOUR, 4000);
  FakeStorage storage;
  ASSERT_TRUE(h.save(&storage, "batt_hist"));

  BatteryHistory h2;
  ASSERT_TRUE(h2.load(&storage, "batt_hist"));
  EXPECT_TRUE(h2.primed());
  EXPECT_EQ(4100, h2.bucket(N - 3));
  EXPECT_EQ(4050, h2.bucket(N - 2));
  EXPECT_EQ(4000, h2.bucket(N - 1));

  // And it keeps ticking correctly after reload (bucketStart/head carried over).
  h2.tick(1000 + 3 * HALF_HOUR, 3950);
  EXPECT_EQ(3950, h2.bucket(N - 1));
  EXPECT_EQ(4000, h2.bucket(N - 2));
}

TEST(BatteryHistory, LoadFromEmptyStorageFails) {
  BatteryHistory h;
  FakeStorage storage;   // has = false
  EXPECT_FALSE(h.load(&storage, "batt_hist"));
}

TEST(BatteryHistory, LoadNullStorageFails) {
  BatteryHistory h;
  EXPECT_FALSE(h.load(nullptr, "batt_hist"));
  EXPECT_FALSE(h.save(nullptr, "batt_hist"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
