#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
// PC test for the Coco Yeti motion detector: synthetic falls, shakes, knocks, turns.
//   g++ -std=c++17 -O2 -I../firmware/coco_yeti test_motion.cpp ../firmware/coco_yeti/coco_motion.cpp -o test_motion && ./test_motion
#include "coco_motion.h"
using namespace coco;
struct S { float ax, ay, az, gx, gy, gz; };
static float noise(float a) { return a * ((rand() % 2001) / 1000.0f - 1.0f); }
static void still(std::vector<S>& v, int ms, float ux=0, float uy=1, float uz=0) { for (int t = 0; t < ms; t += 10) v.push_back({ux + noise(0.02f), uy + noise(0.02f), uz + noise(0.02f), noise(2), noise(2), noise(2)}); }
static std::string run(const char* name, const std::vector<S>& v, const char* expect) {
  MotionDetector d; std::string got;
  for (auto& s : v) { MotionEvent e = d.update(s.ax, s.ay, s.az, s.gx, s.gy, s.gz); if (e != MotionEvent::None) { if (!got.empty()) got += ","; got += motionEventName(e); if (e == MotionEvent::Orient) { got += "("; got += orientationName(d.orientation()); got += ")"; } } }
  bool ok = got == expect;
  printf("%s %-26s got [%s]%s%s\n", ok ? "ok  " : "FAIL", name, got.c_str(), ok ? "" : "  expected ", ok ? "" : expect);
  return ok ? "" : "x";
}
int main() {
  srand(5); int fails = 0;
  { std::vector<S> v; still(v, 5000); fails += run("still", v, "").size(); }
  { std::vector<S> v; still(v, 1000); for (int i = 0; i < 3; i++) v.push_back({0.2f, 1.1f, -1.6f, 20, 0, 0}); still(v, 1500); fails += run("knock (slap)", v, "knock").size(); }
  { std::vector<S> v; still(v, 1000); for (int k = 0; k < 3; k++) { for (int i = 0; i < 3; i++) v.push_back({0.1f, 1.0f, 1.8f, 0, 0, 0}); still(v, 600); } fails += run("three knocks", v, "knock,knock,knock").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 2000; t += 10) { float a = 2.2f * sinf(t * 2 * 3.14159f * 5 / 1000); v.push_back({a + noise(0.1f), 1 + noise(0.2f), noise(0.2f), noise(100), noise(100), noise(100)}); } still(v, 1500); fails += run("shake 2 s", v, "shake,shake end").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 350; t += 10) v.push_back({noise(0.05f), noise(0.05f), noise(0.05f), noise(30), noise(30), noise(30)});
    v.push_back({0.5f, 4.5f, 1.0f, 50, 0, 0}); v.push_back({-0.3f, 3.0f, 0.4f, 0, 0, 0}); for (int t = 0; t < 150; t += 10) v.push_back({noise(0.6f), 1 + noise(0.6f), noise(0.6f), noise(80), noise(80), noise(80)}); still(v, 2000);
    fails += run("dropped on the desk", v, "fall,landed").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 200; t += 10) v.push_back({noise(0.05f), noise(0.05f), noise(0.05f), 0, 0, 0}); for (int t = 0; t < 150; t += 10) v.push_back({0, 1.5f, 0, 0, 0, 0}); still(v, 2000);
    fails += run("dropped and caught", v, "fall,caught").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 600; t += 10) v.push_back({0.3f, 1.0f, 0, 0, 650, 0}); still(v, 2000); fails += run("spun around", v, "spin").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t <= 600; t += 10) { float a = t / 600.0f * 1.5708f; v.push_back({0, cosf(a), sinf(a), 0, 0, 0}); } still(v, 2000, 0, 0, 1); fails += run("laid on its back", v, "orient(on its back)").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t <= 800; t += 10) { float a = t / 800.0f * 3.14159f; v.push_back({0, cosf(a), sinf(a), 0, 0, 120}); } still(v, 2000, 0, -1, 0); fails += run("turned upside down", v, "orient(upside down)").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t <= 600; t += 10) { float a = t / 600.0f * 1.5708f; v.push_back({0, cosf(a), -sinf(a), 0, 0, 0}); } still(v, 2000, 0, 0, -1); fails += run("put face down", v, "orient(face down)").size(); }
  { std::vector<S> v; still(v, 4000); for (int t = 0; t < 400; t += 10) v.push_back({0.15f * sinf(t / 30.0f), 1 + 0.25f * sinf(t / 50.0f), 0.1f, 25, 10, 5}); still(v, 1500); fails += run("picked up after a rest", v, "moved").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 3000; t += 10) v.push_back({noise(0.08f), 1 + noise(0.08f), noise(0.08f), noise(10), noise(10), noise(10)}); fails += run("carried gently (noise)", v, "").size(); }
  { std::vector<S> v; still(v, 1000); for (int t = 0; t < 300; t += 10) v.push_back({0.5f, 1.0f, 0, 0, 0, 0}); still(v, 1500, 0.5f/1.118f, 1/1.118f, 0); fails += run("pushed slowly", v, "").size(); }
  printf("%d failures\n", fails);
  return fails;
}
