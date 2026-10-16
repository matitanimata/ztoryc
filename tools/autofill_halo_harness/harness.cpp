// Offline harness for the Toonz Raster (Smart Raster) brush Auto Fill.
//
// Draws closed rings with several synthetic brush profiles on a mock CM32
// raster, runs the Auto Fill, and measures what an artist would see:
//   halo  - antialiased line pixels on the INNER side left without paint
//           (ink drawn over nothing: a light rim between line and fill),
//           weighted by how transparent they are (tone / 255)
//   spill - line pixels on the OUTER side that got the fill colour,
//           weighted the same way (fill colour peeking out of the line)
//   far   - painted pixels beyond the line altogether (a real leak)
// It runs the REAL code: ztoryc_pass1.inc and stock_fill.inc are extracted
// from the sources by extract.py, the line split is the shipping header.
//
//   ./run.sh            (extract + build + run)
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <stack>
#include <vector>

struct TPoint { int x, y; TPoint(int a = 0, int b = 0) : x(a), y(b) {}
  bool operator!=(const TPoint &o) const { return x != o.x || y != o.y; } };
struct TRect { int x0, y0, x1, y1;
  TRect(int a = 0, int b = 0, int c = -1, int d = -1) : x0(a), y0(b), x1(c), y1(d) {} };
struct TPixelCM32 { int ink = 0, paint = 0, tone = 255;
  int getInk() const { return ink; } int getPaint() const { return paint; }
  int getTone() const { return tone; } void setPaint(int p) { paint = p; }
  bool isPurePaint() const { return tone == 255; }
  static int getMaxTone() { return 255; } };
struct Ras { int lx, ly; std::vector<TPixelCM32> d;
  Ras(int a, int b) : lx(a), ly(b), d(a * b) {}
  TPixelCM32 *pixels(int y) { return &d[y * lx]; }
  TRect getBounds() const { return TRect(0, 0, lx - 1, ly - 1); }
  int getLx() const { return lx; } int getLy() const { return ly; }
  int getWrap() const { return lx; } };
typedef Ras *TRasterCM32P;
struct TPalette {}; struct TTileSaverCM32 { void save(TRect) {} };

#include "stock_fill.inc"
#include "ztoryc_pass1.inc"
#include "../../toonz/sources/tnztools/autofilllinesplit.h"

// ---- synthetic brushes ------------------------------------------------------
enum Brush { HARD, SOFT_CORE, SOFT_NOCORE, PENCIL, NOISY_SOFT, NOISY_CORE };
static std::mt19937 rng(42);
static double coverage(Brush b, double t, double w) {
  auto noise = [] { return std::uniform_real_distribution<double>(0.6, 1.0)(rng); };
  switch (b) {
  case HARD: return std::clamp(w / 2 + 0.5 - t, 0.0, 1.0);
  case PENCIL: return t <= w / 2 ? 1.0 : 0.0;
  case SOFT_CORE: return std::clamp(1.0 - std::max(0.0, t - w * 0.25) / (w * 0.75), 0.0, 1.0);
  case SOFT_NOCORE: { double c = 0.75 * std::exp(-t * t / (2 * (w / 3) * (w / 3))); return c < 0.02 ? 0 : c; }
  case NOISY_SOFT: { double c = 0.75 * std::exp(-t * t / (2 * (w / 3) * (w / 3))) * noise(); return c < 0.02 ? 0 : c; }
  case NOISY_CORE: { double c = std::clamp(1.0 - std::max(0.0, t - w * 0.25) / (w * 0.75), 0.0, 1.0);
                     return c < 1 ? c * noise() : c; }
  }
  return 0;
}
static void ring(Ras &R, double cx, double cy, double rad, double w, Brush b) {
  for (int y = 0; y < R.ly; y++) for (int x = 0; x < R.lx; x++) {
    double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy);
    double c = coverage(b, std::fabs(d - rad), w);
    if (c <= 0) continue;
    int tone = (int)std::lround(255 * (1 - c));
    TPixelCM32 &p = R.pixels(y)[x];
    if (tone < p.tone) { p.tone = tone; p.ink = 1; }
  }
}
// ---- the fills under test ---------------------------------------------------
static void stockFill(Ras &r, TPoint p, int paint) {  // fill() non-xsheet path, depth 0
  int fillDepth = 255, x = p.x, y = p.y, xa, xb, xc, xd, oldy, dy, oldxc, oldxd, tone, oldtone;
  TPixelCM32 *pix, *limit, *pix0, *oldpix; TRasterCM32P rr = &r; TRect bb = r.getBounds();
  int clicked = r.pixels(y)[x].getPaint(); if (clicked == paint) return;
  std::stack<std::pair<std::pair<int,int>,std::pair<int,int>>> seeds;
  if (calcFillRow(rr, p, xa, xb, paint, nullptr, true, false)) fillRow(rr, p, xa, xb, paint, nullptr, nullptr);
  seeds.push({{xa, xb}, {y, 1}}); seeds.push({{xa, xb}, {y, -1}});
  while (!seeds.empty()) { auto s = seeds.top(); seeds.pop(); xa = s.first.first; xb = s.first.second;
    oldy = s.second.first; dy = s.second.second; y = oldy + dy; if (y > bb.y1 || y < bb.y0) continue;
    pix = pix0 = r.pixels(y) + xa; limit = r.pixels(y) + xb; oldpix = r.pixels(oldy) + xa; x = xa;
    oldxd = std::numeric_limits<int>::min(); oldxc = std::numeric_limits<int>::max();
    while (pix <= limit) { oldtone = threshTone(*oldpix, fillDepth); tone = threshTone(*pix, fillDepth);
      bool can = pix->getPaint() != paint && tone <= oldtone && tone != 0 &&
                 (pix->getPaint() != pix->getInk() || pix->getPaint() == clicked);
      if (can) { if (calcFillRow(rr, TPoint(x, y), xc, xd, paint, nullptr, true)) fillRow(rr, TPoint(x, y), xc, xd, paint, nullptr, nullptr);
        if (xc < xa) seeds.push({{xc, xa - 1}, {y, -dy}}); if (xd > xb) seeds.push({{xb + 1, xd}, {y, -dy}});
        if (oldxd >= xc - 1) oldxd = xd; else { if (oldxd >= 0) seeds.push({{oldxc, oldxd}, {y, dy}}); oldxc = xc; oldxd = xd; }
        pix += xd - x + 1; oldpix += xd - x + 1; x += xd - x + 1;
      } else { pix++; oldpix++; x++; } }
    if (oldxd > 0) seeds.push({{oldxc, oldxd}, {y, dy}}); }
}
enum Method { SHIPPED, SHIPPED_PLUS_SPLIT, STOCK_FILL };
static void autoFill(Ras &R, Method m) {
  TRect sb = R.getBounds();
  std::vector<int> toFill = ztorycPass1(&R, 0, 0, R.lx, R.ly, sb, false);
  if (m == STOCK_FILL) {  // one stock fill per enclosed empty region
    for (int i : toFill) if (R.d[i].ink == 0 && R.d[i].paint == 0) stockFill(R, TPoint(i % R.lx, i / R.lx), 2);
    return;
  }
  if (m == SHIPPED_PLUS_SPLIT) AutoFillLineSplit::addInnerLinePixels(&R, 0, 0, R.lx, R.ly, toFill);
  for (int i : toFill) R.d[i].paint = 2;
}
struct Score { double halo = 0, spill = 0; int far = 0, holes = 0; };
static Score measure(Ras &R, double cx, double cy, double rad, double w) {
  Score s;
  for (int y = 0; y < R.ly; y++) for (int x = 0; x < R.lx; x++) {
    double d = std::hypot(x + 0.5 - cx, y + 0.5 - cy); TPixelCM32 &p = R.pixels(y)[x];
    if (d < rad && p.tone > 0 && p.tone < 255 && p.paint == 0) s.halo += p.tone / 255.0;
    if (d > rad && p.tone > 0 && p.paint != 0) s.spill += p.tone / 255.0;
    if (d > rad + w && p.paint != 0) s.far++;
    if (d < rad - w && p.tone == 255 && p.paint == 0) s.holes++;
  }
  return s;
}
int main() {
  struct Case { const char *name; Brush b; double w; } cases[] = {
      {"hard AA w4", HARD, 4},          {"hard AA w2", HARD, 2},
      {"hard AA w1.5", HARD, 1.5},      {"pencil w3", PENCIL, 3},
      {"soft + core", SOFT_CORE, 12},   {"soft no core", SOFT_NOCORE, 12},
      {"noisy soft", NOISY_SOFT, 12},   {"noisy + core", NOISY_CORE, 12}};
  const char *mname[] = {"regions only", "regions + line split", "stock fill (ref.)"};
  printf("%-14s %-22s %9s %9s %6s %6s\n", "brush", "method", "halo", "spill", "far", "holes");
  int bad = 0;
  for (auto &c : cases) for (int m = 0; m < 3; m++) {
    rng.seed(42); Ras R(240, 240); ring(R, 120, 120, 70, c.w, c.b);
    autoFill(R, (Method)m); Score s = measure(R, 120, 120, 70, c.w);
    printf("%-14s %-22s %9.1f %9.1f %6d %6d\n", c.name, mname[m], s.halo, s.spill, s.far, s.holes);
    if (m == SHIPPED_PLUS_SPLIT && (s.far || s.holes)) bad++;
  }
  return bad ? 1 : 0;
}
