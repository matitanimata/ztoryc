#pragma once

#ifndef AUTOFILLLINESPLIT_H
#define AUTOFILLLINESPLIT_H

// Auto Fill (Toonz Raster brush): which LINE pixels get the fill colour.
//
// The Auto Fill finds the empty regions closed by a stroke and paints them.
// Painting only the empty pixels leaves the antialiased pixels on the inner
// side of the line with paint 0: their ink is drawn over nothing, so a thin
// light halo shows between line and fill.  Painting every line pixel next to
// the region instead spills colour under the outer edge of the line.
//
// The rule here splits the line down the middle: every line pixel (ink != 0)
// is assigned to the NEAREST empty pixel — measured in steps through line
// pixels, from all empty pixels at once — and it is painted only when that
// nearest empty pixel belongs to a region being filled.  Equal distance goes
// to the filled region (it removes the last halo pixels on 1-2 px lines; the
// colour then sits under the darkest part of the line).  No tone threshold is
// involved, so it holds on soft brushes without a solid core and on noisy /
// textured lines, where the scanline rule of the stock fill stops early.
//
// Measured offline on synthetic rings (tools/autofill_halo_harness): halo
// removed on hard, soft, soft without core, noisy and 1.5 px lines; never
// any colour beyond the line.
//
// Header-only and templated on the raster type so the offline harness tests
// this exact code with a mock raster.

#include <cstdint>
#include <vector>

namespace AutoFillLineSplit {

// ras         : the CM32 raster (anything with pixels(y)[x].getInk/getPaint)
// x0, y0      : origin of the scanned sub-rectangle in raster coordinates
// sw, sh      : size of the scanned sub-rectangle
// toFill      : sub-rectangle indices (y * sw + x) of the EMPTY pixels that
//               are going to be filled; on return the line pixels on the
//               inner side of the line (and still unpainted) are appended.
template <class RasterP>
void addInnerLinePixels(const RasterP &ras, int x0, int y0, int sw, int sh,
                        std::vector<int> &toFill) {
  const int n = sw * sh;
  if (n <= 0 || toFill.empty()) return;

  enum : uint8_t { None = 0, Filled = 1, Other = 2 };
  auto isLine = [&](int i) {
    return ras->pixels(y0 + i / sw)[x0 + i % sw].getInk() != 0;
  };

  std::vector<uint8_t> owner(n, None);
  for (int i : toFill) owner[i] = Filled;

  // Every empty pixel is a source: the ones being filled, and all the others
  // (outside, other regions, already painted areas).
  std::vector<int> frontier, next;
  for (int i = 0; i < n; i++) {
    if (isLine(i)) continue;
    if (owner[i] == None) owner[i] = Other;
    frontier.push_back(i);
  }

  // Grow all sources together through line pixels, one step per round.
  std::vector<uint8_t> reached(n, 0);
  for (int i : frontier) reached[i] = 1;
  while (!frontier.empty()) {
    next.clear();
    for (int i : frontier) {
      const int x = i % sw, y = i / sw;
      const int nx[4] = {x - 1, x + 1, x, x};
      const int ny[4] = {y, y, y - 1, y + 1};
      for (int k = 0; k < 4; k++) {
        if (nx[k] < 0 || ny[k] < 0 || nx[k] >= sw || ny[k] >= sh) continue;
        const int j = ny[k] * sw + nx[k];
        if (!isLine(j)) continue;
        if (!reached[j]) {
          reached[j] = 2;  // reached in this round
          owner[j]   = owner[i];
          next.push_back(j);
        } else if (reached[j] == 2 && owner[i] == Filled) {
          owner[j] = Filled;  // tie in this round: the filled region wins
        }
      }
    }
    for (int j : next) reached[j] = 1;
    frontier.swap(next);
  }

  for (int i = 0; i < n; i++) {
    if (owner[i] != Filled || !isLine(i)) continue;
    if (ras->pixels(y0 + i / sw)[x0 + i % sw].getPaint() != 0) continue;
    toFill.push_back(i);
  }
}

}  // namespace AutoFillLineSplit

#endif
