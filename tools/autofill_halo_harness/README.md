# Auto Fill halo harness (Toonz Raster brush)

Offline test for the Smart Raster brush **Auto Fill**: what happens to the
antialiased line pixels around a filled shape. It exists because five fixes
to the "halo" were shipped and regressed in June 2026 without being tested
offline first (see CHANGELOG, 2026-06-13).

```sh
./run.sh
```

`extract.py` copies the **real code** into the harness before every build:
- `ztoryc_pass1.inc`: the region search from `ToonzRasterBrushTool::leftButtonUp`;
- `stock_fill.inc`: `calcFillRow` / `threshTone` / `fillRow` from `toonzlib/fill.cpp` (reference only);
- the line split is `toonz/sources/tnztools/autofilllinesplit.h`, included as is.

It draws a closed ring with eight synthetic brush profiles and prints:

| column | meaning |
|---|---|
| halo  | inner-side antialiased line pixels left without paint, weighted by transparency (light rim inside the line) |
| spill | outer-side line pixels that got the fill colour, weighted the same way (fill colour peeking out of the line) |
| far   | painted pixels beyond the line: a real leak, must be 0 |
| holes | unpainted pixels inside the shape, must be 0 |

Exit code 1 if "regions + line split" (what ships) has a leak or holes.
The brush profiles are synthetic, so confirm on real strokes in the app.
