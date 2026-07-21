#pragma once

// =============================================================================
// ZtoryTheme — palette centralizzata per i pannelli custom Ztoryc (dipinti a
// mano: timeline animatic, righello, track, board). Segue il tema Qt attivo:
// con "Abete"/"Light" restituisce toni legno chiaro, altrimenti i grigi scuri
// originali. Header-only (inline) per non toccare il CMake.
//
// Strategia: i pannelli usano grigi neutri come sfondo/testo. g(v) prende il
// LIVELLO del grigio nel tema scuro e, in tema chiaro, lo inverte per luminanza
// sulla rampa "abete caldo" (sfondo scuro -> legno chiaro, testo chiaro ->
// legno scuro). I colori semantici (playhead, razor, selezione, waveform)
// restano literal nel codice e leggibili su entrambi i temi.
// =============================================================================

#include <QColor>
#include "toonz/preferences.h"
#include "toonz/preferencesitemids.h"

namespace ZtoryTheme {

inline bool isLight() {
  QString n = Preferences::instance()->getStringValue(CurrentStyleSheetName);
  return n == "Abete" || n == "Light";
}

// Rampa abete caldo per luminanza (0-255). Allineata a stuff/config/qss/Abete.
inline QColor wood(int lum) {
  static const int A[][4] = {
      {0, 0x1E, 0x18, 0x10},   {51, 0x4A, 0x3B, 0x26},  {102, 0x7A, 0x62, 0x40},
      {140, 0xA1, 0x87, 0x5A}, {179, 0xBF, 0xA8, 0x77}, {209, 0xD8, 0xC2, 0x9A},
      {230, 0xE7, 0xD8, 0xBB}, {245, 0xF4, 0xEC, 0xDB}, {255, 0xFB, 0xF6, 0xEC},
  };
  const int N = sizeof(A) / sizeof(A[0]);
  lum = qBound(0, lum, 255);
  for (int i = 0; i + 1 < N; ++i) {
    if (lum >= A[i][0] && lum <= A[i + 1][0]) {
      double t = (A[i + 1][0] == A[i][0])
                     ? 0.0
                     : double(lum - A[i][0]) / (A[i + 1][0] - A[i][0]);
      return QColor(qRound(A[i][1] + (A[i + 1][1] - A[i][1]) * t),
                    qRound(A[i][2] + (A[i + 1][2] - A[i][2]) * t),
                    qRound(A[i][3] + (A[i + 1][3] - A[i][3]) * t));
    }
  }
  return QColor(A[N - 1][1], A[N - 1][2], A[N - 1][3]);
}

// Grigio neutro che segue il tema. v = livello del grigio nel tema scuro; in
// tema chiaro viene invertito per luminanza sulla rampa legno. Alpha preservato.
inline QColor g(int v, int a = 255) {
  if (!isLight()) return QColor(v, v, v, a);
  QColor c = wood(255 - v);
  c.setAlpha(a);
  return c;
}

// Evidenziazione shot attivo / marker. Nei temi scuri resta il magenta storico
// (#E0249B, ben leggibile sul fondo scuro); nel tema chiaro Abete usa l'oro del
// logo. WIP: la leggibilita' dell'oro su legno va ancora rifinita.
inline QColor activeShot() {
  return isLight() ? QColor(0xEC, 0xA6, 0x1C) : QColor(0xE0, 0x24, 0x9B);
}

// Accento secondario (verde salvia) — dettagli/waveform nel tema chiaro.
inline QColor accent() { return QColor(0x4E, 0x7A, 0x51); }

// Oro del logo Ztoryc. Usato per marcare gli elementi "arricchiti" rispetto al
// corrispettivo Tahoma standard — p.es. il diamante di una chiave globale che
// comprende anche la posa plastic, distinto dal diamante bianco normale.
inline QColor gold() { return QColor(0xEC, 0xA6, 0x1C); }

// Fondo del bottone "Set Key" del KeyframeNavigator quando il frame corrente ha
// una chiave. Sostituisce l'arancio Tahoma (#be7323), che nascondeva l'oro
// della posa plastic: il magenta e' abbastanza scuro e abbastanza lontano in
// tinta da lasciar leggere sia il bianco sia l'oro del diamante.
inline QColor keyBackground() { return QColor(0xB0, 0x1E, 0x9A); }
inline QColor keyBackgroundHover() { return keyBackground().lighter(125); }

}  // namespace ZtoryTheme
