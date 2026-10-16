#include <gtest/gtest.h>
#include <mishmesh/core/Geo.h>
#include <string.h>

using namespace mishmesh;

// Milan Duomo and Rome Colosseum, as MeshCore's 1e6 int32 coordinates.
static const int32_t MI_LAT = 45464211, MI_LON = 9191383;
static const int32_t RM_LAT = 41890210, RM_LON = 12492231;

TEST(GeoDistance, MilanToRome) {
  EXPECT_NEAR(477.0f, geoDistanceKm(MI_LAT, MI_LON, RM_LAT, RM_LON), 3.0f);
}

TEST(GeoBearing, CardinalDirections) {
  EXPECT_NEAR(0.0f,   geoBearingDeg(0, 0, 1000000, 0), 0.01f);
  EXPECT_NEAR(90.0f,  geoBearingDeg(0, 0, 0, 1000000), 0.01f);
  EXPECT_NEAR(180.0f, geoBearingDeg(0, 0, -1000000, 0), 0.01f);
  EXPECT_NEAR(270.0f, geoBearingDeg(0, 0, 0, -1000000), 0.01f);
}

TEST(GeoBearing, MilanToRomeIsSouthEast) {
  float b = geoBearingDeg(MI_LAT, MI_LON, RM_LAT, RM_LON);
  EXPECT_GT(b, 130.0f);
  EXPECT_LT(b, 150.0f);
}

TEST(GeoBearing, AlwaysInRange) {
  float b = geoBearingDeg(10000000, 179900000, 10000000, -179900000);   // across the antimeridian
  EXPECT_GE(b, 0.0f);
  EXPECT_LT(b, 360.0f);
  EXPECT_NEAR(90.0f, b, 1.0f);   // short way round, eastwards
  EXPECT_EQ(0.0f, geoBearingDeg(MI_LAT, MI_LON, MI_LAT, MI_LON));
}

TEST(GeoCompass, PointsAndWrap) {
  EXPECT_STREQ("N",  geoCompassPoint(0));
  EXPECT_STREQ("N",  geoCompassPoint(359));
  EXPECT_STREQ("NE", geoCompassPoint(45));
  EXPECT_STREQ("S",  geoCompassPoint(180));
  EXPECT_STREQ("NW", geoCompassPoint(-30));    // == 330
  EXPECT_STREQ("E",  geoCompassPoint(450));    // == 90
}

TEST(GeoFormat, Distances) {
  char b[16];
  geoFormatDistance(b, sizeof(b), 0.0f);    EXPECT_STREQ("0 m", b);
  geoFormatDistance(b, sizeof(b), 0.853f);  EXPECT_STREQ("850 m", b);
  geoFormatDistance(b, sizeof(b), 0.998f);  EXPECT_STREQ("1.0 km", b);   // never "1000 m"
  geoFormatDistance(b, sizeof(b), 1.24f);   EXPECT_STREQ("1.2 km", b);
  geoFormatDistance(b, sizeof(b), 9.96f);   EXPECT_STREQ("10 km", b);    // never "10.0 km"
  geoFormatDistance(b, sizeof(b), 123.4f);  EXPECT_STREQ("123 km", b);
  geoFormatDistance(b, sizeof(b), -5.0f);   EXPECT_STREQ("0 m", b);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
