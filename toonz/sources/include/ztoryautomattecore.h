#pragma once

// Auto-matte, engine A: the solid silhouette of a drawing.
//
// Input: a mask of the drawn pixels (line or paint).  Output: the pixels
// inside the drawing's outer contour, the line included.  Small gaps in the
// contour (sketches are rarely closed) are bridged with a radius:
//
//   1. the outside is what can be reached from the image border without
//      coming closer than `gap` pixels to a drawn pixel;
//   2. the outside then grows back by `gap`, up to the outer edge of the line;
//   3. the matte is everything that is not outside.
//
// It is a morphological closing restricted to the outer contour: holes inside
// the drawing (between arm and body, say) are filled too, as Storyboard Pro's
// Generate Auto-Matte does.  Openings wider than 2 x gap let the outside in.
//
// Header-only and free of Toonz types, so it can be tested offline. Also the
// gap closing of the raster brushes' Auto Fill (exteriorWithGaps below).

#include <cstdint>
#include <vector>
#include <algorithm>

namespace ZtoryAutoMatteCore {

// Chamfer 3-4 distance transform (3 = one pixel step).  dist[i] = 0 on the
// sources, approximate Euclidean distance x 3 elsewhere.
inline void chamferDistance(const std::vector<uint8_t> &source, int w, int h,
                            std::vector<int> &dist) {
  const int kInf = 1 << 29;
  dist.assign((size_t)w * h, kInf);
  for (size_t i = 0; i < dist.size(); i++)
    if (source[i]) dist[i] = 0;
  auto at = [&](int x, int y) -> int & { return dist[(size_t)y * w + x]; };
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      int &d = at(x, y);
      if (x > 0) d = std::min(d, at(x - 1, y) + 3);
      if (y > 0) {
        d = std::min(d, at(x, y - 1) + 3);
        if (x > 0) d = std::min(d, at(x - 1, y - 1) + 4);
        if (x < w - 1) d = std::min(d, at(x + 1, y - 1) + 4);
      }
    }
  for (int y = h - 1; y >= 0; y--)
    for (int x = w - 1; x >= 0; x--) {
      int &d = at(x, y);
      if (x < w - 1) d = std::min(d, at(x + 1, y) + 3);
      if (y < h - 1) {
        d = std::min(d, at(x, y + 1) + 3);
        if (x < w - 1) d = std::min(d, at(x + 1, y + 1) + 4);
        if (x > 0) d = std::min(d, at(x - 1, y + 1) + 4);
      }
    }
}

// solid: 1 where the drawing has ink or paint.  Returns 1 inside the matte.
// solidDist: chamferDistance(solid) when the caller already has it.  With
// gap 0 no distance is needed at all: «farther than 0» is «not solid», and
// growing back by 0 leaves the outside as it is — same result, two distance
// transforms fewer (review of 2026-10-23).
inline std::vector<uint8_t> computeMatte(const std::vector<uint8_t> &solid,
                                         int w, int h, int gap,
                                         const std::vector<int> *solidDist =
                                             nullptr) {
  std::vector<uint8_t> matte((size_t)w * h, 0);
  if (w <= 0 || h <= 0) return matte;
  gap = std::max(0, gap);
  const int limit = 3 * gap;  // chamfer units

  std::vector<int> ownDist;
  if (gap > 0 && !solidDist) chamferDistance(solid, w, h, ownDist);
  const std::vector<int> &dist = solidDist ? *solidDist : ownDist;

  // 1. outside: flood from the border through pixels farther than `gap`.
  std::vector<uint8_t> outside((size_t)w * h, 0);
  std::vector<int> stack;
  auto seed = [&](int x, int y) {
    const size_t i = (size_t)y * w + x;
    if (!outside[i] && !solid[i] && (gap == 0 || dist[i] > limit)) {
      outside[i] = 1;
      stack.push_back((int)i);
    }
  };
  for (int x = 0; x < w; x++) seed(x, 0), seed(x, h - 1);
  for (int y = 0; y < h; y++) seed(0, y), seed(w - 1, y);
  while (!stack.empty()) {
    const int i = stack.back();
    stack.pop_back();
    const int x = i % w, y = i / w;
    if (x > 0) seed(x - 1, y);
    if (x < w - 1) seed(x + 1, y);
    if (y > 0) seed(x, y - 1);
    if (y < h - 1) seed(x, y + 1);
  }

  // 2-3. grow the outside back by `gap`; the rest is the matte.  Without any
  // outside pixel (drawing touching every border) the whole frame is matte,
  // which is what an opaque drawing filling the frame means anyway.
  if (gap == 0) {
    for (size_t i = 0; i < matte.size(); i++) matte[i] = !outside[i];
    return matte;
  }
  std::vector<int> distOut;
  chamferDistance(outside, w, h, distOut);
  for (size_t i = 0; i < matte.size(); i++)
    matte[i] = (solid[i] || distOut[i] > limit) ? 1 : 0;
  return matte;
}

// Auto Fill with «Auto Close Gap» (raster brushes): 1 where a pixel is
// OUTSIDE — reachable from the border of the scanned area without passing an
// opening narrower than about 2 x gap; the outside then grows back up to the
// line, so a closed shape keeps all its pixels.  gap 0 = the plain flood from
// the border (stock behaviour).
inline std::vector<uint8_t> exteriorWithGaps(
    const std::vector<uint8_t> &line, int w, int h, int gap,
    const std::vector<int> *lineDist = nullptr) {
  std::vector<uint8_t> matte = computeMatte(line, w, h, gap, lineDist);
  for (auto &v : matte) v = !v;  // not matte = outside
  return matte;
}

}  // namespace ZtoryAutoMatteCore
