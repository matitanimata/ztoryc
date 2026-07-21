#pragma once

// =============================================================================
// Grammatica del diamante chiave (Ztoryc) — SORGENTE UNICA
// -----------------------------------------------------------------------------
// Il diamante porta due assi indipendenti in un solo glifo:
//
//   meta' destra vuota  = chiave PARZIALE
//   meta' sinistra      = quali sistemi: bianco trasformazione, oro posa, o
//                         bianco-sopra/oro-sotto quando un parziale tiene
//                         entrambi.
//
//   [] bianco pieno            trasformazione completa
//   [| bianco/vuoto            trasformazione parziale
//   [] bianco | oro            tutto chiaviato (chiave "All")
//   [] oro pieno               posa completa
//   [| bianco-su-oro / vuoto   entrambi parziali
//
// Sta qui, e non nei due chiamanti, perche' lo xsheet (CellArea::drawKeyframe)
// e il KeyframeNavigator del viewer devono restare la STESSA lingua: se
// divergono, l'utente vede due verita' diverse sullo stesso frame. Vale sia per
// i colori sia per il rilevamento dello stato — il bug storico
// ("ogni chiave di posa sembra parziale") stava proprio nel rilevamento.
//
// Header-only per non toccare il CMake. Usato da toonz (xsheet) e toonzqt
// (navigator), entrambi linkano tnzext.
// =============================================================================

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

#include "toonzqt/ztorytheme.h"
#include "toonz/tstageobject.h"
#include "ext/plasticskeletondeformation.h"

namespace ZtoryTheme {

struct KeyDiamond {
  QColor leftTop, leftBottom, right;  // QColor() invalido = regione vuota
};

//! \p stageFull: la trasformazione di colonna e' chiaviata su tutti i canali.
//! \p plasticAny / \p plasticFull: la posa plastic ha almeno una / tutte le
//! deformazioni di vertice chiaviate. Presuppone che una chiave ci sia (il
//! diamante si disegna solo su un frame chiaviato).
inline KeyDiamond keyDiamond(bool stageFull, bool plasticAny,
                             bool plasticFull) {
  const QColor white  = Qt::white;
  const QColor g      = gold();
  const QColor hollow = QColor();

  if (!plasticAny)  // Solo trasformazione.
    return {white, white, stageFull ? white : hollow};
  if (stageFull)  // Tutto chiaviato (la chiave "All"): bianco | oro netto.
    return {white, white, g};
  if (plasticFull)
    // La posa e' l'intenzione ed e' completa: oro pieno. Una trasformazione
    // parziale dovuta alla meccanica del drag e' deliberatamente assorbita
    // qui — e' quello che l'animatore ha chiesto con un Global Key di portata
    // Plastic.
    return {g, g, g};
  // Posa parziale sopra una trasformazione non completa: entrambi i sistemi
  // presenti ma incompleti.
  return {white, g, hollow};
}

//! Tutte e tre le regioni dello stesso colore (selezione, o marker semplice).
inline KeyDiamond keyDiamondSolid(const QColor &c) { return {c, c, c}; }

//! Stato della posa plastic di \p pegbar alla riga \p row.
//! \p any = almeno una deformazione di vertice chiaviata,
//! \p full = tutte chiaviate.
//!
//! NON PlasticSkeletonDeformation::isFullKeyframe(): quella pretende anche il
//! parametro skeleton-ids, che NESSUNO dei due percorsi che mettono una chiave
//! di posa tocca (il Plastic tool lo dice esplicitamente, e
//! TStageObject::setPlasticPoseKeyframe cammina solo i parametri di posa).
//! Usandola, ogni chiave di posa risultava parziale.
//!
//! Nota paramsTime(), non la riga grezza: i parametri plastic sono campionati
//! nel tempo-parametri dello stage object, che diverge dalla riga xsheet quando
//! c'e' un ciclo.
inline void plasticPoseState(TStageObject *pegbar, int row, bool &any,
                             bool &full) {
  any = full = false;
  if (!pegbar) return;
  const PlasticSkeletonDeformationP &psd =
      pegbar->getPlasticSkeletonDeformation();
  if (!psd) return;

  const double pf = pegbar->paramsTime(row);
  any             = psd->isKeyframe(pf);
  if (!any) return;

  full = true;
  PlasticSkeletonDeformation::vd_iterator vdt, vdEnd;
  psd->vertexDeformations(vdt, vdEnd);
  for (; vdt != vdEnd; ++vdt)
    if (!(*vdt).second->isFullKeyframe(pf)) {
      full = false;
      break;
    }
}

//! Il diamante che compete a \p pegbar alla riga \p row. Il chiamante ha gia'
//! verificato che il frame sia chiaviato.
inline KeyDiamond keyDiamondForStageObject(TStageObject *pegbar, int row) {
  bool any = false, full = false;
  plasticPoseState(pegbar, row, any, full);
  return keyDiamond(pegbar && pegbar->isFullKeyframe(row), any, full);
}

//! Riempie le tre regioni di \p path: meta' sinistra divisa in alto/basso,
//! meta' destra intera. I rect di clip sono arrotondati verso l'esterno cosi'
//! nessun pixel di cucitura tra le regioni resta non dipinto.
inline void fillKeyRegions(QPainter &p, const QPainterPath &path,
                           const KeyDiamond &d) {
  const QRectF bb  = path.boundingRect();
  const qreal midX = bb.center().x();
  const qreal midY = bb.center().y();

  auto fillRegion = [&](const QColor &fill, const QRectF &clip) {
    if (!fill.isValid()) return;  // vuota: si vede solo il contorno comune
    p.save();
    p.setClipRect(clip.adjusted(-1.0, -1.0, 1.0, 1.0), Qt::IntersectClip);
    p.fillPath(path, QBrush(fill));
    p.restore();
  };

  const qreal lw = midX - bb.left(), rw = bb.right() - midX;
  const qreal th = midY - bb.top(), bh = bb.bottom() - midY;
  fillRegion(d.leftTop, QRectF(bb.left(), bb.top(), lw, th));
  fillRegion(d.leftBottom, QRectF(bb.left(), midY, lw, bh));
  fillRegion(d.right, QRectF(midX, bb.top(), rw, bb.height()));
}

//! Diamante inscritto in \p r (punte sugli assi verticale/orizzontale).
inline QPainterPath keyDiamondPath(const QRectF &r) {
  QPainterPath path;
  path.moveTo(r.center().x(), r.top());
  path.lineTo(r.right(), r.center().y());
  path.lineTo(r.center().x(), r.bottom());
  path.lineTo(r.left(), r.center().y());
  path.closeSubpath();
  return path;
}

//! Icona quadrata di \p size px (device pixel ratio \p dpr) col diamante
//! disegnato a codice. Serve al KeyframeNavigator: i .qss dei temi forzano
//! `image: url(transparent.svg)` sui bottoni chiave, quindi le icone su file
//! non si vedrebbero comunque — dipingerle qui e' anche l'unico modo di avere
//! sei stati invece di tre.
inline QPixmap keyDiamondPixmap(const KeyDiamond &d, int size, qreal dpr,
                                const QColor &outline = QColor(0, 0, 0)) {
  QPixmap pm(qRound(size * dpr), qRound(size * dpr));
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  // Un pixel di margine per non tagliare il contorno.
  const QPainterPath path =
      keyDiamondPath(QRectF(1.0, 1.0, size - 2.0, size - 2.0));
  fillKeyRegions(p, path, d);
  if (outline.isValid()) {
    p.setPen(QPen(outline, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
  }
  return pm;
}

}  // namespace ZtoryTheme
