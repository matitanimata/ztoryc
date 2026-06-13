#pragma once

//=============================================================================
// BrushProfiler — strumentazione leggera della pipeline di disegno (task 51).
//
// Header-only, a costo ZERO quando il flag d'ambiente ZTORYC_BRUSH_PROFILE non
// è impostato (un solo branch su un int cached).  Misura tre stadi della
// catena tablet → dab → schermo e stampa min/med/max/avg ogni N campioni su
// stderr:
//   dab_compute  — durata di ToonzRasterBrushTool::leftButtonDrag (event
//                  handler → raster aggiornato)
//   paintGL      — durata di SceneViewer::paintGL
//   evt->paint   — latenza end-to-end: arrivo del QTabletEvent fino al termine
//                  del primo paintGL successivo (input latency percepita)
//
// Uso:  ZTORYC_BRUSH_PROFILE=1 ./Ztoryc.app/Contents/MacOS/Ztoryc
//=============================================================================

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace BrushProfiler {

inline bool enabled() {
  static int e = -1;
  if (e < 0) {
    const char *v = ::getenv("ZTORYC_BRUSH_PROFILE");
    e             = (v && v[0] && v[0] != '0') ? 1 : 0;
  }
  return e == 1;
}

inline double nowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

//! Accumula campioni e ne stampa la sintesi ogni reportEvery aggiunte.
struct Stat {
  const char *name;
  int reportEvery;
  std::vector<double> samples;
  explicit Stat(const char *n, int every = 120) : name(n), reportEvery(every) {}
  void add(double ms) {
    samples.push_back(ms);
    if ((int)samples.size() >= reportEvery) flush();
  }
  void flush() {
    if (samples.empty()) return;
    std::vector<double> s = samples;
    std::sort(s.begin(), s.end());
    double sum = 0.0;
    for (double v : s) sum += v;
    std::fprintf(stderr,
                 "[brushprof] %-12s n=%4zu  min=%6.2f  med=%6.2f  "
                 "max=%6.2f  avg=%6.2f ms\n",
                 name, s.size(), s.front(), s[s.size() / 2], s.back(),
                 sum / s.size());
    samples.clear();
  }
};

//! Misura il tempo speso in uno scope (logga alla distruzione).
struct ScopeTimer {
  Stat &st;
  double t0;
  bool on;
  explicit ScopeTimer(Stat &s) : st(s), on(enabled()) {
    if (on) t0 = nowMs();
  }
  ~ScopeTimer() {
    if (on) st.add(nowMs() - t0);
  }
};

// Le Stat sono function-local static dentro funzioni inline: una sola istanza
// condivisa per TU che le usa.  dab_compute vive in libtnztools, le altre nel
// binario principale: nessuno stato cross-libreria richiesto.
inline Stat &dabStat() {
  static Stat s("dab_compute");
  return s;
}
inline Stat &paintStat() {
  static Stat s("paintGL");
  return s;
}
inline Stat &evtPaintStat() {
  static Stat s("evt->paint");
  return s;
}

//! Timestamp dell'ultimo QTabletEvent di disegno, azzerato dal primo paintGL
//! che lo consuma → misura la latenza end-to-end.
inline double &lastEventMs() {
  static double v = 0.0;
  return v;
}

//! RAII da mettere in cima a paintGL: logga la durata del paint e, se c'è un
//! evento tablet in attesa, la latenza evento→paint.
struct PaintScope {
  double t0;
  bool on;
  PaintScope() : on(enabled()) {
    if (on) t0 = nowMs();
  }
  ~PaintScope() {
    if (!on) return;
    double end = nowMs();
    paintStat().add(end - t0);
    double le = lastEventMs();
    if (le > 0.0) {
      evtPaintStat().add(end - le);
      lastEventMs() = 0.0;
    }
  }
};

}  // namespace BrushProfiler
