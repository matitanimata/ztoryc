#include "storyboardpanel.h"
#include "ztorylightgizmo.h"

#include "tundo.h"
#include "tapp.h"
#include "tenv.h"
#include "toonz/toonzscene.h"
#include "toonz/stage.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/childstack.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjecttree.h"
#include "columncommand.h"
#include "toonz/tstageobject.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshmeshcolumn.h"
#include "toonzqt/stageobjectsdata.h"
#include "tfxattributes.h"
#include "toonz/fxdag.h"
#include "expressionreferencemanager.h"
#include "toonz/tframehandle.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "mainwindow.h"
#include "toonzqt/gutil.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QShortcut>
#include <QRadioButton>
#include "iocommand.h"
#include "subscenecommand.h"
#include "columnselection.h"
#include "toonz/tproject.h"
#include "tsystem.h"
#include "tsystem.h"
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <set>
#include <QLabel>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QSettings>
#include <QFrame>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPdfWriter>
#include <QPageLayout>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QWindow>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QApplication>
#include <QLineEdit>
#include <QGroupBox>
#include <QButtonGroup>
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/boardsettings.h"
#include "menubarcommandids.h"
#include <QColorDialog>
#include <QEventLoop>
#include <cmath>

// Persisted number of columns in the Board grid (the spin in the toolbar).
// Stored in user env so the layout is remembered across sessions.
TEnv::IntVar ZtoryBoardColumns("ZtoryBoardColumns", 3);

// Strip leading alphabetic characters from a label; optionally capture the prefix.
// E.g. "SH010" → "010" (prefix="SH"),  "010" → "010" (prefix=""),  "SQ001" → "001"
static QString stripAlphaPrefix(const QString &s, QString *prefix = nullptr) {
  int i = 0;
  while (i < s.length() && s[i].isLetter()) i++;
  if (prefix) *prefix = s.left(i);
  return s.mid(i);
}

// Merge helpers defined in ztoryanimatic.cpp (non-static so they can be shared)
void materializeCells(TXshChildLevel *cl, int duration, bool fillToEnd = false);
void trimChildXsheetTo(TXshChildLevel *cl, int keepFrames);
void mergeChildXsheetContent(TXshChildLevel *dstCl, TXshChildLevel *srcCl,
                              int dstOffset, int srcDuration);

// PanelWidget light edit-mode shared state (set from the Board toolbar).
bool    PanelWidget::s_lightEditMode = false;
QString PanelWidget::s_lightColor    = "#FFC34D";

// ── Light-direction gizmo (task 40 FASE 3) ──────────────────────────────────
// Storyboard conic-arrow notation for light: a translucent beam wedge spreads
// from the source toward the subject (opening angle = soft/hard light) with a
// solid 3D arrow along its axis (cylindrical shaft + true cone head, whose
// silhouette is tangent to the base ellipse).  `depth` is the Z component of
// the direction: the axis foreshortens with cos(depth·90°), so ±1 collapses to
// a true head-on view drawn with the standard out-of-page/into-page notation
// (⊙ toward camera, ⊗ into the background) inside a radial light halo.
// Shading is AXIAL — as if the light itself fell along the arrow: bright at
// the source end, falling off toward the tip.  `editing` adds the sun glyph
// and the angle/depth/spread readout, shown only during the placement drag.
// Coordinates are normalized 0-1 over (W, H); colour = light temperature;
// `spreadDeg` = full beam opening angle in degrees.
void ztoryDrawLightGizmo(QPainter &p, double W, double H,
                         double tailX, double tailY,
                         double tipX, double tipY, double depth,
                         double spreadDeg, const QColor &color,
                         bool editing) {
  const double kPi = 3.14159265358979323846;  // M_PI needs _USE_MATH_DEFINES on MSVC
  QPointF P0(tailX * W, tailY * H), P1(tipX * W, tipY * H);
  QPointF d  = P1 - P0;
  double  len = std::hypot(d.x(), d.y());
  double  ref = qMin(W, H);
  if (len < ref * 0.06 || ref < 24) return;  // degenerate drag — nothing to draw

  depth     = qBound(-1.0, depth, 1.0);
  spreadDeg = qBound(12.0, spreadDeg, 90.0);
  double angleDeg = std::atan2(d.y(), d.x()) * 180.0 / kPi;
  double sunR     = qBound(5.0, ref * 0.045, 18.0);
  // True perspective foreshortening: at depth ±1 the axis collapses entirely
  // and the gizmo is seen head-on.
  double cosT   = std::cos(depth * kPi / 2.0);
  double lenEff = len * cosT;
  // Perpendicular circles project with roundness sin(tilt): straight lines
  // in pure side view (both the cone base and the tail cap read as flat
  // chords), opening into full circles head-on.
  double k      = std::fabs(std::sin(depth * kPi / 2.0));
  double fHead  = qMax(0.45, 1.0 + 0.55 * depth);
  double fTail  = qMax(0.45, 1.0 - 0.35 * depth);
  double halfSpread = spreadDeg * 0.5 * kPi / 180.0;

  p.save();
  p.setRenderHint(QPainter::Antialiasing, true);

  QPen outline(color.darker(220), 1.0);

  // ── Head-on view (light pointing straight at the camera or the background):
  // standard vector notation — circle with a dot (toward viewer) or a cross
  // (away from viewer) inside a radial halo of light.
  if (lenEff < ref * 0.05 || cosT < 0.18) {
    bool toCamera = depth > 0;
    double haloR = qBound(ref * 0.10, len * 0.45, ref * 0.40);
    double symR  = haloR * 0.45;
    QRadialGradient halo(P0, haloR);
    QColor hc0 = color; hc0.setAlpha(120);
    QColor hc1 = color; hc1.setAlpha(0);
    halo.setColorAt(0.0, hc0);
    halo.setColorAt(1.0, hc1);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(P0, haloR, haloR);
    // Legibility halo ring behind the symbol.
    p.setPen(QPen(QColor(255, 255, 255, 170), 3.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(P0, symR, symR);
    QRadialGradient disc(P0, symR, P0 - QPointF(symR * 0.3, symR * 0.3));
    disc.setColorAt(0.0, color.lighter(toCamera ? 175 : 150));
    disc.setColorAt(1.0, color);
    p.setPen(outline);
    p.setBrush(disc);
    p.drawEllipse(P0, symR, symR);
    p.setPen(QPen(color.darker(200), qMax(1.5, symR * 0.18),
                  Qt::SolidLine, Qt::RoundCap));
    if (toCamera) {
      // ⊙ — the cone apex points at the viewer.
      p.setBrush(color.darker(200));
      p.drawEllipse(P0, symR * 0.18, symR * 0.18);
    } else {
      // ⊗ — the light dives into the background.
      double c = symR * 0.55;
      p.drawLine(P0 + QPointF(-c, -c), P0 + QPointF(c, c));
      p.drawLine(P0 + QPointF(-c, c), P0 + QPointF(c, -c));
    }
    p.restore();
    if (editing) {
      // Readout: direction is head-on, so only depth target + spread matter.
      QFont fnt("Arial", 0);
      fnt.setPixelSize(qMax(7, qMin(10, (int)(W / 28))));
      fnt.setBold(true);
      p.setFont(fnt);
      QString txt = (toCamera ? QChar(0x2197) : QChar(0x2198)) +
                    QString(" 100%  %1 %2%3")
                        .arg(QChar(0x2220)).arg(qRound(spreadDeg)).arg(QChar(0x00B0));
      QPointF tp = P0 + QPointF(-p.fontMetrics().horizontalAdvance(txt) / 2.0,
                                -haloR - p.fontMetrics().descent() - 2);
      p.setPen(QColor(255, 255, 255));
      for (int ox = -1; ox <= 1; ox++)
        for (int oy = -1; oy <= 1; oy++)
          if (ox || oy) p.drawText(tp + QPointF(ox, oy), txt);
      p.setPen(color.darker(200));
      p.drawText(tp, txt);
    }
    return;
  }

  p.translate(P0);
  p.rotate(angleDeg);
  // Local frame: x along the arrow (0 = tail/source, lenEff = tip), y = side.

  double headLen = qBound(8.0, lenEff * 0.38, ref * 0.30);
  double headR   = headLen * 0.42 * fHead;
  double shaftRb = qMax(1.5, headLen * 0.42 * 0.42);  // base shaft radius
  double shaftRt = qMax(1.0, shaftRb * fTail);        // at the tail
  double shaftRh = qMax(1.0, shaftRb * fHead);        // at the cone base

  double sx = editing ? sunR * 1.6 : 0.0;  // leave room for the sun while dragging
  double bx = lenEff - headLen;            // cone base
  if (bx <= sx) bx = sx + 1;
  double tipX2 = lenEff;

  // ── Translucent beam wedge: the illuminated area.  Opening angle = spread
  // (narrow = hard spotlight, wide = soft light); fades out with distance.
  // Drag feedback only — the baked arrow stays unobtrusive on the artwork.
  if (editing) {
    double beamLen = lenEff * 1.15;
    double ey = qMin(beamLen * std::tan(halfSpread) * fHead, ref * 0.45);
    double ex = qMax(1.0, ey * k);
    QPainterPath beam;
    beam.moveTo(0, 0);
    beam.lineTo(beamLen, -ey);
    beam.arcTo(QRectF(beamLen - ex, -ey, 2 * ex, 2 * ey), 90, -180);
    beam.closeSubpath();
    QLinearGradient bg(0, 0, beamLen, 0);
    QColor b0 = color; b0.setAlpha(105);
    QColor b1 = color; b1.setAlpha(0);
    bg.setColorAt(0.0, b0);
    bg.setColorAt(1.0, b1);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawPath(beam);
  }

  // Tapered cylinder silhouette (fTail → fHead).  The end facing away from
  // the viewer is a straight chord (occluded or capped), the end facing the
  // viewer bulges with the far arc of its rim ellipse so the cylinder stays
  // round in perspective.
  double rtk = shaftRt * k, rhk = qMax(0.5, shaftRh * k);
  QPainterPath shaft;
  if (depth > 0.02) {
    // Toward the camera: the tail rim is the visible curved end.
    shaft.moveTo(bx, -shaftRh);
    shaft.lineTo(sx, -shaftRt);
    shaft.arcTo(QRectF(sx - rtk, -shaftRt, 2 * rtk, 2 * shaftRt), 90, 180);
    shaft.lineTo(bx, shaftRh);
    shaft.closeSubpath();
  } else {
    // Away from the camera (or side view): the head-end rim bulges toward
    // the tip; the tail edge is covered by the lit end-cap ellipse.
    shaft.moveTo(sx, -shaftRt);
    shaft.lineTo(bx, -shaftRh);
    shaft.arcTo(QRectF(bx - rhk, -shaftRh, 2 * rhk, 2 * shaftRh), 90, -180);
    shaft.lineTo(sx, shaftRt);
    shaft.closeSubpath();
  }

  // True cone silhouette: the sides leave the apex TANGENT to the base
  // ellipse (not to its vertical extremes), so the head reads as a solid
  // cone at every tilt instead of "a triangle over a circle".
  double ea = headR * k, eb = headR;       // base ellipse semi-axes
  double u  = tipX2 - bx;                  // apex distance from base center
  // Which side faces the viewer decides what is occluded: tilted toward the
  // camera (depth > 0) you look at the apex side — the base disc and the
  // shaft tail cap are hidden behind the solid; tilted away you see the base.
  bool frontView = depth > 0.02;
  QPainterPath cone;
  bool hasCone = (u > ea * 1.05);
  if (hasCone) {
    double ct = ea / u;                    // tangency: cosθ = a/u
    double txp = bx + ea * ct;             // touch point x
    double typ = eb * std::sqrt(qMax(0.0, 1.0 - ct * ct));
    cone.moveTo(tipX2, 0);
    cone.lineTo(txp, -typ);
    // Follow the ellipse between the two tangency points so the filled
    // lateral surface meets the silhouette with no gap or overshoot.
    // Qt arc angles are parametric: the touch point (ea·cosθ, ±eb·sinθ) is at
    // parametric angle θ = acos(ct), not at its geometric polar angle.
    double a0 = std::acos(qBound(-1.0, ct, 1.0)) * 180.0 / kPi;
    QRectF er(bx - ea, -eb, 2 * ea, 2 * eb);
    if (frontView)
      // Apex side: silhouette closes along the FAR arc of the base ellipse.
      cone.arcTo(er, a0, 360.0 - 2.0 * a0);
    else
      // Base side: lateral surface spans only the near arc (the lit base
      // disc, drawn separately, covers the rest).
      cone.arcTo(er, a0, -2.0 * a0);
    cone.closeSubpath();
  }

  // Legibility halo behind the solid arrow (the beam stays untouched).
  QPen halo(QColor(255, 255, 255, 170), 3.0);
  p.setPen(halo);
  p.setBrush(Qt::NoBrush);
  p.drawPath(shaft);
  if (hasCone) p.drawPath(cone);
  if (editing) p.drawEllipse(QPointF(0, 0), sunR, sunR);

  // Axial shading — lit at the source end, darker toward the tip.
  QLinearGradient shaftG(sx, 0, bx, 0);
  shaftG.setColorAt(0.0, color.lighter(155));
  shaftG.setColorAt(1.0, color);
  QLinearGradient coneG(bx, 0, tipX2, 0);
  coneG.setColorAt(0.0, color);
  coneG.setColorAt(1.0, color.darker(170));
  p.setPen(outline);

  // Painter's order follows the viewing side: toward the camera the cone is
  // the nearest solid and covers the shaft junction; away from it the shaft
  // (and its lit tail cap) sit in front of the cone.
  if (frontView) {
    p.setBrush(shaftG);
    p.drawPath(shaft);
    if (hasCone) {
      p.setBrush(coneG);
      p.drawPath(cone);
    } else {
      // Apex projects inside the base — nearly head-on: the lateral surface
      // fills the whole silhouette ellipse, apex marked by a dot.
      QLinearGradient flatG(bx - ea, 0, bx + ea, 0);
      flatG.setColorAt(0.0, color);
      flatG.setColorAt(1.0, color.darker(150));
      p.setBrush(flatG);
      p.drawEllipse(QPointF(bx, 0), ea, eb);
      p.setBrush(color.darker(200));
      p.drawEllipse(QPointF(tipX2, 0), eb * 0.15, eb * 0.15);
    }
  } else {
    // Lit base disc first (it faces the source), lateral surface over its
    // near arc, then the shaft on top, closed by the lit tail cap.
    p.setBrush(color.lighter(135));
    p.drawEllipse(QPointF(bx, 0), ea, eb);
    if (hasCone) {
      p.setBrush(coneG);
      p.drawPath(cone);
    }
    p.setBrush(shaftG);
    p.drawPath(shaft);
    p.setBrush(color.lighter(165));
    p.drawEllipse(QPointF(sx, 0), rtk, shaftRt);
  }

  // Sun glyph at the source — placement feedback only, not baked.
  if (editing) {
    QRadialGradient sun(QPointF(0, 0), sunR, QPointF(-sunR * 0.3, -sunR * 0.3));
    sun.setColorAt(0.0, color.lighter(175));
    sun.setColorAt(1.0, color);
    p.setBrush(sun);
    p.setPen(outline);
    p.drawEllipse(QPointF(0, 0), sunR, sunR);
    p.setPen(QPen(color.darker(140), qMax(1.0, sunR * 0.16)));
    for (int i = 0; i < 8; i++) {
      double a  = i * kPi / 4.0;
      QPointF u2(std::cos(a), std::sin(a));
      p.drawLine(u2 * sunR * 1.25, u2 * sunR * 1.75);
    }
  }
  p.restore();

  // Angle + depth + spread readout next to the source — drag feedback only.
  if (editing) {
    int deg = qRound(std::fmod(360.0 - angleDeg + 360.0, 360.0));
    QFont fnt("Arial", 0);
    fnt.setPixelSize(qMax(7, qMin(10, (int)(W / 28))));
    fnt.setBold(true);
    p.setFont(fnt);
    QString txt = QString::number(deg) + QChar(0x00B0);
    if (std::fabs(depth) > 0.01)
      txt += QString(" %1%2%")
                 .arg(depth > 0 ? QChar(0x2197) : QChar(0x2198))  // ↗ camera ↘ back
                 .arg(qRound(std::fabs(depth) * 100));
    txt += QString("  %1%2%3")
               .arg(QChar(0x2220)).arg(qRound(spreadDeg)).arg(QChar(0x00B0));
    QPointF tp = P0 - QPointF(d.x(), d.y()) * ((sunR * 2.2) / len);
    tp += QPointF(-p.fontMetrics().horizontalAdvance(txt) / 2.0,
                  p.fontMetrics().ascent() / 2.0);
    p.setPen(QColor(255, 255, 255));
    for (int ox = -1; ox <= 1; ox++)
      for (int oy = -1; oy <= 1; oy++)
        if (ox || oy) p.drawText(tp + QPointF(ox, oy), txt);
    p.setPen(color.darker(200));
    p.drawText(tp, txt);
  }
}

// Bakes the light gizmo onto a thumbnail/preview pixmap.
void ztoryApplyLightOverlay(QPixmap &px, const PanelData &pd) {
  if (!pd.hasLight || px.isNull()) return;
  QPainter p(&px);
  // QPainter on a DPR-tagged pixmap works in logical coords — pass the
  // logical size so the gizmo proportions stay DPI-independent.
  qreal dpr = px.devicePixelRatio() > 0 ? px.devicePixelRatio() : 1.0;
  ztoryDrawLightGizmo(p, px.width() / dpr, px.height() / dpr,
                      pd.lightTailX, pd.lightTailY,
                      pd.lightTipX, pd.lightTipY, pd.lightDepth,
                      pd.lightSpread, QColor(pd.lightColor));
  p.end();
}

PanelWidget::PanelWidget(QWidget *parent)
    : QFrame(parent)
    , m_shotIndex(0)
    , m_panelIndex(0)
    , m_panelCount(1)
    , m_fps(24)
    , m_selected(false)
{
  setMinimumWidth(150);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAcceptDrops(true);
  updateBorderStyle();

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(2);
  layout->setContentsMargins(4, 4, 4, 4);

  QWidget *header = new QWidget();
  header->setStyleSheet("background-color:#3a3a3a; border-radius:2px;");
  QHBoxLayout *hl = new QHBoxLayout(header);
  hl->setContentsMargins(6, 3, 6, 3);
  hl->setSpacing(4);

  auto lbl = [](const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet("color:#aaa; font-size:10px;");
    return l;
  };
  auto val = [](const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet("color:#fff; font-size:10px; font-weight:bold;");
    return l;
  };

  // SQ field — editable sequence label; shows number only (prefix stored in m_storedSeqPrefix)
  m_seqField = new QLineEdit();
  m_seqField->setFixedWidth(42);
  m_seqField->setPlaceholderText("—");
  m_seqField->setStyleSheet(
    "QLineEdit{color:#88aaff;background:#3a3a3a;border:none;font-size:10px;font-weight:bold;padding:0 2px;}"
    "QLineEdit:focus{background:#444;border:1px solid #88aaff;}");
  m_storedSeqPrefix = "SQ";  // default prefix

  // SH field — editable shot label; shows number only (prefix stored in m_storedShotPrefix)
  m_shotLabel = new QLineEdit();
  m_shotLabel->setFixedWidth(42);
  m_storedShotPrefix = "SH";  // default prefix
  m_shotLabel->setStyleSheet(
    "QLineEdit{color:#fff;background:#3a3a3a;border:none;font-size:10px;font-weight:bold;padding:0 2px;}"
    "QLineEdit:focus{background:#555;border:1px solid #888;}");
  m_panelLabel = val("1/1");

  // D: durata parziale panel — read-only, derivata dalla subscene
  m_durationSpin = new QSpinBox();
  m_durationSpin->setRange(1, 99999);
  m_durationSpin->setValue(24);
  m_durationSpin->setFixedWidth(52);
  m_durationSpin->setReadOnly(true);
  m_durationSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_durationSpin->setStyleSheet(
    "QSpinBox{background:#333;color:#aaa;border:1px solid #444;font-size:10px;padding:1px;}");

  m_durationLabel = new QLabel("00:00:00");
  m_durationLabel->setStyleSheet("color:#88aaff; font-size:10px;");

  m_totalLabel = new QLabel("T:00:00");
  m_totalLabel->setStyleSheet("color:#aaffaa; font-size:10px;");

  // T: durata totale shot — editabile solo nel panel 0
  m_totalSpin = new QSpinBox();
  m_totalSpin->setRange(1, 99999);
  m_totalSpin->setValue(24);
  m_totalSpin->setFixedWidth(52);
  m_totalSpin->setStyleSheet(
    "QSpinBox{background:#222;color:#aaffaa;border:1px solid #555;font-size:10px;padding:1px;}");

  m_matchButton = new QPushButton("\u21d4");  // ⇔ match timeline to sub-scene
  m_matchButton->setFixedSize(22, 18);
  m_matchButton->setToolTip("Match timeline duration to shot actual duration");
  m_matchButton->setStyleSheet(
    "QPushButton{background:#444;color:#ffcc55;border-radius:3px;font-size:10px;}"
    "QPushButton:hover{background:#666;}");

  m_seqLabel = lbl("SQ:");
  hl->addWidget(m_seqLabel);
  hl->addWidget(m_seqField);
  hl->addWidget(lbl("SH:"));
  hl->addWidget(m_shotLabel);
  hl->addWidget(lbl("P:"));
  hl->addWidget(m_panelLabel);
  hl->addWidget(lbl("D:"));
  hl->addWidget(m_durationSpin);
  hl->addWidget(lbl("T:"));
  hl->addWidget(m_totalSpin);
  hl->addWidget(m_matchButton);
  hl->addStretch();
  layout->addWidget(header);

  // Hide SQ row in Simple mode from the start
  bool seqMode = (ZtoryModel::instance()->numberingConfig().style == NumberingConfig::Sequence);
  m_seqLabel->setVisible(seqMode);
  m_seqField->setVisible(seqMode);

  m_previewLabel = new QLabel();
  m_previewLabel->setAlignment(Qt::AlignCenter);
  m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_previewLabel->setMinimumHeight(100);
  m_previewLabel->setStyleSheet("QLabel{background:#f0f0eb;border:none;color:#bbb;}");
  layout->addWidget(m_previewLabel);

  // Helper lambda — sets up a QTextEdit with stable layout to avoid the
  // QAbstractScrollArea layout-recursion crash that occurs when the default
  // ScrollBarAsNeeded policy oscillates between "scrollbar visible/hidden",
  // each toggle reflowing the document, retriggering layout, infinitely.
  auto makeStableTextEdit = [](const QString &placeholder) {
    QTextEdit *te = new QTextEdit();
    te->setPlaceholderText(placeholder);
    te->setFixedHeight(68);
    // Lock vertical size policy (setFixedHeight already does, but be explicit
    // so layout refreshes don't widen the constraint) and pin scroll bars to
    // stable states.
    te->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    te->setStyleSheet(
      "QTextEdit{background:#2a2a2a;color:#eee;border:1px solid #444;font-size:11px;padding:2px;}");
    return te;
  };

  layout->addWidget(makeFieldLabel("Dialog"));
  m_dialogField = makeStableTextEdit("Enter dialogue...");
  layout->addWidget(m_dialogField);

  layout->addWidget(makeFieldLabel("Action Notes"));
  m_actionField = makeStableTextEdit("Enter action notes...");
  layout->addWidget(m_actionField);

  layout->addWidget(makeFieldLabel("Notes"));
  m_notesField = makeStableTextEdit("Enter notes...");
  layout->addWidget(m_notesField);

  connect(m_matchButton, &QPushButton::clicked, this,
          [this](){ emit matchDurationRequested(m_shotIndex); });
  connect(m_shotLabel, &QLineEdit::editingFinished, [this](){
    emit dataChanged(m_shotIndex, m_panelIndex);
  });
  connect(m_seqField, &QLineEdit::editingFinished, [this](){
    QString entered = m_seqField->text().trimmed();
    if (entered.isEmpty()) return;
    // Reconstruct full sequence label: if user typed digits only, prepend stored prefix;
    // if they typed something like "SQ020", use the alpha part as the new prefix.
    QString fullLabel;
    if (!entered.isEmpty() && entered[0].isLetter()) {
      QString pfx;
      stripAlphaPrefix(entered, &pfx);
      if (!pfx.isEmpty()) m_storedSeqPrefix = pfx;
      fullLabel = entered;  // already has prefix
    } else {
      fullLabel = m_storedSeqPrefix + entered;
    }
    emit seqLabelEdited(m_shotIndex, fullLabel);
  });
  connect(m_totalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int frames){ emit totalDurationChanged(frames); });
  connect(m_durationSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &PanelWidget::onDurationSpinChanged);
  connect(m_dialogField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  connect(m_actionField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  connect(m_notesField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  setDuration(24);
  // Show the camera placeholder immediately so the panel never looks "blank".
  // rescalePreview() checks m_previewPixmap.isNull() and draws the placeholder
  // when no thumbnail has been rendered yet.  A proper QResizeEvent will re-call
  // this once the widget has its final geometry, so the placeholder is always
  // sized correctly.
  QTimer::singleShot(0, this, [this](){ rescalePreview(); });
}

QLabel* PanelWidget::makeFieldLabel(const QString &text) {
  QLabel *l = new QLabel(text);
  l->setStyleSheet(
    "color:#aaa;font-size:10px;font-weight:bold;"
    "background:#333;padding:1px 4px;border-top:1px solid #555;");
  return l;
}

QString PanelWidget::framesToTimecode(int frames) const {
  int ff = frames % m_fps;
  int ts = frames / m_fps;
  int ss = ts % 60;
  int mm = ts / 60;
  return QString("%1:%2:%3")
    .arg(mm, 2, 10, QChar(48))
    .arg(ss, 2, 10, QChar(48))
    .arg(ff, 2, 10, QChar(48));
}

void PanelWidget::updateBorderStyle() {
  if (m_selected)
    setStyleSheet("PanelWidget{background:#2b2b2b;border:1px solid #e05a00;border-radius:3px;box-shadow:0 0 0 2px #e05a00;}");
  else
    setStyleSheet("PanelWidget{background:#2b2b2b;border:1px solid #555;border-radius:3px;}"
                  "PanelWidget:hover{border:1px solid #888;}");
}

void PanelWidget::rescalePreview() {
  int w = width() - 8;
  if (w <= 0) w = 150;
  int h = w * 9 / 16;
  m_previewLabel->setFixedHeight(h);
  if (!m_previewPixmap.isNull()) {
    // HiDPI-aware display: render at physical pixel size so Retina screens are
    // pixel-perfect without blurry upscaling.
    qreal dpr = 1.0;
    if (QWindow *win = window()->windowHandle())
      dpr = win->devicePixelRatio();
    QSize physTarget(int(w * dpr), int(h * dpr));
    // m_previewPixmap is stored without a DPR tag (raw physical pixels).
    // Scale to target physical size, then tag with DPR for Qt display.
    QPixmap scaled = m_previewPixmap.scaled(
        physTarget, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    // Live rubber-band while dragging the light-direction arrow: draw the
    // in-progress gizmo over the scaled copy (the committed one is baked into
    // m_previewPixmap by updatePreview, so this never accumulates).
    if (m_lightDragging) {
      QPainter lp(&scaled);
      ztoryDrawLightGizmo(lp, scaled.width() / dpr, scaled.height() / dpr,
                          m_lightDragTail.x(), m_lightDragTail.y(),
                          m_lightDragTip.x(), m_lightDragTip.y(),
                          m_lightDragDepth, m_lightDragSpread,
                          QColor(s_lightColor), /*editing=*/true);
    }
    // Clear placeholder text/style before setting the real thumbnail.
    m_previewLabel->setText(QString());
    m_previewLabel->setStyleSheet("QLabel{background:#f0f0eb;border:none;color:#bbb;}");
    m_previewLabel->setPixmap(scaled);
  } else {
    // Placeholder: pure CSS + Unicode camera glyph — zero QPixmap allocation.
    // Avoids the 5-10s freeze caused by creating 600+ QPixmap(640×360) objects
    // synchronously in the rebuildGrid() loop on scenes with many panels.
    m_previewLabel->setPixmap(QPixmap());  // clear any stale pixmap
    m_previewLabel->setText("\U0001F3A5");  // 🎥 film camera
    m_previewLabel->setStyleSheet(
      "QLabel{background:#1e1e1e;color:#555;font-size:28px;"
      "border:none;qproperty-alignment:AlignCenter;}");
  }
}

void PanelWidget::setShotIndex(int si) { m_shotIndex = si; }

void PanelWidget::setPanelIndex(int pi, int total) {
  m_panelIndex = pi;
  m_panelCount = total;
  m_panelLabel->setText(QString("%1/%2").arg(pi + 1).arg(total));
  // T: editabile solo nel primo panel
  m_totalSpin->setReadOnly(pi != 0);
  m_totalSpin->setButtonSymbols(pi == 0 ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons);
  m_totalSpin->setStyleSheet(pi == 0
    ? "QSpinBox{background:#222;color:#aaffaa;border:1px solid #555;font-size:10px;padding:1px;}"
    : "QSpinBox{background:#333;color:#666;border:1px solid #444;font-size:10px;padding:1px;}");
}

void PanelWidget::setShotNumber(const QString &label) {
  // Split "SQ001_SH010" → sqPart="SQ001", shPart="SH010"
  // or    "SH010"       → sqPart="",      shPart="SH010"
  QString sqPart, shPart;
  int sep = label.indexOf('_');
  if (sep >= 0) {
    sqPart = label.left(sep);
    shPart = label.mid(sep + 1);
  } else {
    shPart = label;
  }

  // SH field: strip alpha prefix, store it, show only the numeric part
  QString shNum = stripAlphaPrefix(shPart, &m_storedShotPrefix);
  m_shotLabel->blockSignals(true);
  m_shotLabel->setText(shNum.isEmpty() ? shPart : shNum);
  m_shotLabel->blockSignals(false);

  // SQ field: if the label carried a SQ part, update it too
  if (!sqPart.isEmpty()) {
    QString seqNum = stripAlphaPrefix(sqPart, &m_storedSeqPrefix);
    m_seqField->blockSignals(true);
    m_seqField->setText(seqNum.isEmpty() ? sqPart : seqNum);
    m_seqField->blockSignals(false);
  }
}

QString PanelWidget::shotNumber() const {
  // Reconstruct full shot label with its stored prefix (e.g. "SH" + "010" → "SH010")
  return m_storedShotPrefix + m_shotLabel->text();
}

void PanelWidget::setFps(int fps) {
  m_fps = fps;
  m_durationLabel->setText(framesToTimecode(m_durationSpin->value()));
}

void PanelWidget::setDuration(int frames) {
  m_durationSpin->blockSignals(true);
  m_durationSpin->setValue(frames);
  m_durationSpin->blockSignals(false);
  m_durationLabel->setText(framesToTimecode(frames));
}

void PanelWidget::setTotalDuration(int frames) {
  m_totalLabel->setText("T:" + framesToTimecode(frames));
  m_totalSpin->blockSignals(true);
  m_totalSpin->setValue(frames);
  m_totalSpin->blockSignals(false);
}

void PanelWidget::setPreviewPixmap(const QPixmap &px) {
  m_previewPixmap = px;
  rescalePreview();
}

void PanelWidget::setSelected(bool sel) {
  m_selected = sel;
  updateBorderStyle();
}

void PanelWidget::setDialog(const QString &t) {
  m_dialogField->blockSignals(true);
  m_dialogField->setPlainText(t);
  m_dialogField->blockSignals(false);
}

void PanelWidget::setAction(const QString &t) {
  m_actionField->blockSignals(true);
  m_actionField->setPlainText(t);
  m_actionField->blockSignals(false);
}

void PanelWidget::setNotes(const QString &t) {
  m_notesField->blockSignals(true);
  m_notesField->setPlainText(t);
  m_notesField->blockSignals(false);
}

int     PanelWidget::duration() const { return m_durationSpin->value(); }
QString PanelWidget::dialog()   const { return m_dialogField->toPlainText(); }
QString PanelWidget::action()   const { return m_actionField->toPlainText(); }
QString PanelWidget::notes()    const { return m_notesField->toPlainText(); }

void PanelWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  // In light edit mode a quick re-drag can register as a double-click — it
  // must not open the sub-scene under the user's pointer.
  if (s_lightEditMode && m_previewLabel->geometry().contains(e->pos())) {
    e->accept();
    return;
  }
  // Double-click on preview area or header (text fields consume their own
  // double-clicks and don't propagate here) — enter the shot's sub-scene.
  // Accept the event so it does NOT bubble up to StoryboardPanel::mouseDoubleClickEvent,
  // which would immediately close the sub-scene again.
  emit editRequested(m_shotIndex);
  e->accept();
}

void PanelWidget::setSeqLabel(const QString &seq) {
  // Strip alpha prefix and show only the numeric part; store the prefix for reconstruction.
  m_seqField->blockSignals(true);
  if (seq.isEmpty()) {
    m_seqField->clear();
  } else {
    QString num = stripAlphaPrefix(seq, &m_storedSeqPrefix);
    m_seqField->setText(num.isEmpty() ? seq : num);
  }
  m_seqField->blockSignals(false);
}

void PanelWidget::setSeqVisible(bool visible) {
  m_seqLabel->setVisible(visible);
  m_seqField->setVisible(visible);
}

void PanelWidget::onDurationSpinChanged(int value) {
  m_durationLabel->setText(framesToTimecode(value));
  emit durationChanged(m_shotIndex, m_panelIndex, value);
}

// Maps a widget-local position to normalized 0-1 coordinates over the preview
// area (clamped). The preview pixmap fills the label (both are 16:9).
QPointF PanelWidget::normalizedPreviewPos(const QPoint &pos) const {
  QRect r = m_previewLabel->geometry();
  if (r.width() <= 0 || r.height() <= 0) return QPointF(0.5, 0.5);
  return QPointF(qBound(0.0, double(pos.x() - r.x()) / r.width(),  1.0),
                 qBound(0.0, double(pos.y() - r.y()) / r.height(), 1.0));
}

void PanelWidget::mousePressEvent(QMouseEvent *e) {
  // Light edit mode: clicks on the preview place/remove the light gizmo
  // instead of starting the shot drag-reorder.
  if (s_lightEditMode && m_previewLabel->geometry().contains(e->pos())) {
    if (e->button() == Qt::LeftButton) {
      emit clicked(m_shotIndex, m_panelIndex, e->modifiers());
      m_lightDragging = true;
      m_lightDragDepth = 0.0;
      m_lightDragTail = m_lightDragTip = normalizedPreviewPos(e->pos());
      e->accept();
      return;
    }
    if (e->button() == Qt::RightButton) {
      emit lightRemoveRequested(m_shotIndex, m_panelIndex);
      e->accept();
      return;
    }
  }
  if (e->button() == Qt::LeftButton) {
    emit clicked(m_shotIndex, m_panelIndex, e->modifiers());
    // Avvia drag solo senza modifier
    if (e->modifiers() == Qt::NoModifier) {
      QDrag *drag = new QDrag(this);
      QMimeData *mime = new QMimeData();
      mime->setData("application/x-ztoryc-shotindex",
                    QByteArray::number(m_shotIndex));
      drag->setMimeData(mime);
      QPixmap pm(size());
      pm.fill(Qt::transparent);
      render(&pm);
      drag->setPixmap(pm.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      drag->setHotSpot(QPoint(80, 45));
      drag->exec(Qt::MoveAction);
    }
  }
  QFrame::mousePressEvent(e);
}

void PanelWidget::mouseMoveEvent(QMouseEvent *e) {
  if (m_lightDragging) {
    m_lightDragTip = normalizedPreviewPos(e->pos());
    rescalePreview();   // cheap: scale + paint, no scene re-render
    e->accept();
    return;
  }
  QFrame::mouseMoveEvent(e);
}

void PanelWidget::mouseReleaseEvent(QMouseEvent *e) {
  if (m_lightDragging && e->button() == Qt::LeftButton) {
    m_lightDragging = false;
    QPointF d = m_lightDragTip - m_lightDragTail;
    // Commit only a real drag — a bare click would create a degenerate arrow.
    double pxLen = std::hypot(d.x() * m_previewLabel->width(),
                              d.y() * m_previewLabel->height());
    if (pxLen >= 12.0)
      emit lightPlaced(m_shotIndex, m_panelIndex,
                       m_lightDragTail.x(), m_lightDragTail.y(),
                       m_lightDragTip.x(), m_lightDragTip.y(),
                       m_lightDragDepth, m_lightDragSpread);
    else
      rescalePreview();  // discard the rubber-band
    e->accept();
    return;
  }
  QFrame::mouseReleaseEvent(e);
}

void PanelWidget::wheelEvent(QWheelEvent *e) {
  // While dragging the light arrow the wheel tilts it in Z — toward the
  // camera (wheel up) or into the background (wheel down). With Shift it
  // adjusts the beam opening angle instead (narrow = hard, wide = soft).
  if (m_lightDragging) {
    if (e->modifiers() & Qt::ShiftModifier)
      m_lightDragSpread = qBound(12.0,
          m_lightDragSpread + (e->angleDelta().y() > 0 ? 3.0 : -3.0), 90.0);
    else
      m_lightDragDepth = qBound(-1.0,
          m_lightDragDepth + (e->angleDelta().y() > 0 ? 0.1 : -0.1), 1.0);
    rescalePreview();
    e->accept();
    return;
  }
  QFrame::wheelEvent(e);
}

void PanelWidget::resizeEvent(QResizeEvent *e) {
  QFrame::resizeEvent(e);
  rescalePreview();
  // If the stored pixmap is too small for the current physical size (e.g. the
  // window was made wider), request a fresh high-res render via the board.
  if (!m_previewPixmap.isNull()) {
    qreal dpr = 1.0;
    if (QWindow *win = window()->windowHandle())
      dpr = win->devicePixelRatio();
    int requiredPhysW = int((width() - 8) * dpr);
    int storedPhysW   = m_previewPixmap.width();  // raw physical (no DPR tag)
    if (requiredPhysW > int(storedPhysW * 1.2))   // >20% larger → re-render
      emit previewRerenderNeeded(m_shotIndex, m_panelIndex);
  }
}

void PanelWidget::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasFormat("application/x-ztoryc-shotindex"))
    e->acceptProposedAction();
}

void PanelWidget::dragMoveEvent(QDragMoveEvent *e) {
  if (e->mimeData()->hasFormat("application/x-ztoryc-shotindex"))
    e->acceptProposedAction();
}

void PanelWidget::dropEvent(QDropEvent *e) {
  if (!e->mimeData()->hasFormat("application/x-ztoryc-shotindex")) return;
  int fromShot = e->mimeData()->data("application/x-ztoryc-shotindex").toInt();
  if (fromShot != m_shotIndex)
    emit dropReceived(fromShot, m_shotIndex);
  e->acceptProposedAction();
}

void PanelWidget::enterEvent(QEvent *) {
  TApp::instance()->showZtoryHint(
      tr("Double-click to open the shot and draw  |  "
         "Drag to reorder  |  Del to remove  --  "
         "Board: draw, animate, set camera  --  "
         "Timing & audio -> Animatic"));
}

void PanelWidget::leaveEvent(QEvent *) {
  TApp::instance()->clearZtoryHint();
}

StoryboardPanel::StoryboardPanel(QWidget *parent)
    : TPanel(parent)
    // Restore the user's column-count preference (default 3, clamped to the
    // spin's [1, 8] range).  The spin is created later with this value.
    , m_columnsPerRow(qBound(1, (int)ZtoryBoardColumns, 8))
    , m_selectedShotIndex(-1)
    , m_fps(24)
    , m_autoRenumber(true)
    , m_comboViewer(nullptr)
{
  setObjectName("StoryboardPanel");
  setFocusPolicy(Qt::StrongFocus);

  // Keyboard shortcuts: intercept via qApp event filter so they fire regardless
  // of which child widget (card, text field, button) currently holds keyboard focus.
  // QShortcut + WidgetWithChildrenShortcut was unreliable in Tahoma's dock system;
  // qApp filter is the only approach that works consistently.
  qApp->installEventFilter(this);

  setWindowTitle(tr("Ztoryc Board"));

  QWidget *main = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(main);
  mainLayout->setSpacing(4);
  mainLayout->setContentsMargins(6, 6, 6, 6);

  QHBoxLayout *tb = new QHBoxLayout();

  // ── Context chip: "BOARD" — visual orientation badge ─────────────────────
  {
    QLabel *chip = new QLabel("BOARD", main);
    chip->setStyleSheet(
        "QLabel{"
        "  background:#1e5c2e;"        // forest green
        "  color:#7defa0;"
        "  font-size:9px;"
        "  font-weight:bold;"
        "  letter-spacing:1px;"
        "  border-radius:3px;"
        "  padding:1px 6px;"
        "}");
    chip->setFixedHeight(18);
    chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tb->addWidget(chip);
    tb->addSpacing(8);
  }
  // ─────────────────────────────────────────────────────────────────────────

    m_addShotButton = new QToolButton();
  m_addShotButton->setIcon(createQIcon("ztoryc_add_shot"));
  m_addShotButton->setIconSize(QSize(20, 20));
  m_addShotButton->setFixedSize(28, 28);
  m_addShotButton->setToolTip(tr("Add Shot"));
  m_addShotButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  QLabel *colLabel = new QLabel("Columns:");
  colLabel->setStyleSheet("color:#ccc;font-size:11px;");
  m_columnsPerRowSpin = new QSpinBox();
  m_columnsPerRowSpin->setRange(1, 8);
  m_columnsPerRowSpin->setValue(m_columnsPerRow);
  m_columnsPerRowSpin->setFixedWidth(45);
  m_columnsPerRowSpin->setStyleSheet("background:#333;color:#ddd;border:1px solid #555;");

  m_numberingCombo = new QComboBox();
  m_numberingCombo->addItem("Auto #");
  m_numberingCombo->addItem("Keep #");
  m_numberingCombo->addItem("Renumber All");
  m_numberingCombo->setFixedWidth(110);
  m_numberingCombo->setStyleSheet(
    "QComboBox{background:#444;color:#ddd;border:1px solid #555;border-radius:3px;padding:2px 6px;}"
    "QComboBox:hover{background:#555;}"
    "QComboBox QAbstractItemView{background:#333;color:#ddd;selection-background-color:#555;}");

    m_deleteButton = new QToolButton();
  m_deleteButton->setIcon(createQIcon("ztoryc_delete_shot"));
  m_deleteButton->setIconSize(QSize(20, 20));
  m_deleteButton->setFixedSize(28, 28);
  m_deleteButton->setToolTip(tr("Delete Shot"));
  m_deleteButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_mergeButton = new QToolButton();
  m_mergeButton->setIcon(createQIcon("ztoryc_merge"));
  m_mergeButton->setIconSize(QSize(20, 20));
  m_mergeButton->setFixedSize(28, 28);
  m_mergeButton->setToolTip(tr("Merge Shots"));
  m_mergeButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");
  m_copyButton = new QToolButton();
  m_copyButton->setIcon(createQIcon("ztoryc_copy"));
  m_copyButton->setIconSize(QSize(20, 20));
  m_copyButton->setFixedSize(28, 28);
  m_copyButton->setToolTip(tr("Copy"));
  m_copyButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_cloneButton = new QToolButton();
  m_cloneButton->setIcon(createQIcon("ztoryc_clone"));
  m_cloneButton->setIconSize(QSize(20, 20));
  m_cloneButton->setFixedSize(28, 28);
  m_cloneButton->setToolTip(tr("Clone Shot"));
  m_cloneButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_pasteButton = new QToolButton();
  m_pasteButton->setIcon(createQIcon("ztoryc_paste"));
  m_pasteButton->setIconSize(QSize(20, 20));
  m_pasteButton->setFixedSize(28, 28);
  m_pasteButton->setToolTip(tr("Paste"));
  m_pasteButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  m_numberingBtn = new QToolButton();
  m_numberingBtn->setIcon(createQIcon("ztoryc_numbering"));
  m_numberingBtn->setIconSize(QSize(20, 20));
  m_numberingBtn->setFixedSize(28, 28);
  m_numberingBtn->setToolTip(tr("Numbering options"));
  m_numberingBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");


    m_exportPdfButton = new QToolButton();
  m_exportPdfButton->setIcon(createQIcon("ztoryc_export_pdf"));
  m_exportPdfButton->setIconSize(QSize(20, 20));
  m_exportPdfButton->setFixedSize(28, 28);
  m_exportPdfButton->setToolTip(tr("Export PDF"));
  m_exportPdfButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_exportShotsButton = new QToolButton();
  m_exportShotsButton->setIcon(createQIcon("ztoryc_export_shots"));
  m_exportShotsButton->setIconSize(QSize(20, 20));
  m_exportShotsButton->setFixedSize(28, 28);
  m_exportShotsButton->setToolTip(tr("Export Shots"));
  m_exportShotsButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_exportAnimaticButton = new QToolButton();
  m_exportAnimaticButton->setIcon(createQIcon("ztoryc_export_animatic"));
  m_exportAnimaticButton->setIconSize(QSize(20, 20));
  m_exportAnimaticButton->setFixedSize(28, 28);
  m_exportAnimaticButton->setToolTip(tr("Export Animatic"));
  m_exportAnimaticButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  // Toggle: show/hide the camera-move type label (Trk In, Pan…) on thumbnails.
  m_showCamMoveType =
      QSettings().value("Ztoryc/ShowCamMoveType", true).toBool();
  m_camLabelButton = new QToolButton();
  m_camLabelButton->setText("Trk");
  m_camLabelButton->setCheckable(true);
  m_camLabelButton->setChecked(m_showCamMoveType);
  m_camLabelButton->setFixedSize(28, 28);
  m_camLabelButton->setToolTip(tr("Show camera-move labels (Trk In, Pan…)"));
  m_camLabelButton->setStyleSheet(
      "QToolButton{background:transparent;color:#ccc;border:none;border-radius:4px;font-size:11px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#1e5c2e;color:#7defa0;}");

  // ── Light-direction gizmo controls (task 40 FASE 3) ──
  const QString lightBtnStyle =
      "QToolButton{background:transparent;color:#ccc;border:none;border-radius:4px;font-size:12px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#5c4a1e;color:#ffd27d;}";
  m_lightEditButton = new QToolButton();
  m_lightEditButton->setText(QString::fromUtf8("☀"));  // ☀
  m_lightEditButton->setCheckable(true);
  m_lightEditButton->setFixedSize(28, 28);
  m_lightEditButton->setToolTip(
      tr("Place light-direction arrow: drag on a panel, mouse wheel tilts it "
         "toward camera/background, Shift+wheel sets the beam spread "
         "(right-click removes)"));
  m_lightEditButton->setStyleSheet(lightBtnStyle);

  m_showLights = QSettings().value("Ztoryc/ShowLightDirection", true).toBool();
  m_lightShowButton = new QToolButton();
  m_lightShowButton->setText("L");
  m_lightShowButton->setCheckable(true);
  m_lightShowButton->setChecked(m_showLights);
  m_lightShowButton->setFixedSize(28, 28);
  m_lightShowButton->setToolTip(tr("Show light-direction arrows (L)"));
  m_lightShowButton->setStyleSheet(lightBtnStyle);

  QString lightColor =
      QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
  PanelWidget::setLightColor(lightColor);
  m_lightColorButton = new QToolButton();
  m_lightColorButton->setFixedSize(28, 28);
  m_lightColorButton->setToolTip(tr("Light colour (temperature)"));
  auto lightSwatchStyle = [](const QString &c) {
    return QString("QToolButton{background:%1;border:1px solid #555;"
                   "border-radius:4px;margin:6px;}"
                   "QToolButton:hover{border:1px solid #aaa;}").arg(c);
  };
  m_lightColorButton->setStyleSheet(lightSwatchStyle(lightColor));

  tb->addWidget(m_addShotButton);
  tb->addWidget(m_deleteButton);
  tb->addWidget(m_mergeButton);
  tb->addWidget(m_copyButton);
  tb->addWidget(m_cloneButton);
  tb->addWidget(m_pasteButton);
  tb->addSpacing(8);
  tb->addWidget(m_numberingCombo);
  tb->addWidget(m_numberingBtn);
  tb->addSpacing(8);
  tb->addWidget(colLabel);
  tb->addWidget(m_columnsPerRowSpin);
  tb->addStretch();
  tb->addWidget(m_lightEditButton);
  tb->addWidget(m_lightColorButton);
  tb->addWidget(m_lightShowButton);
  tb->addSpacing(8);
  tb->addWidget(m_camLabelButton);
  tb->addSpacing(8);
  tb->addWidget(m_exportPdfButton);
  tb->addWidget(m_exportShotsButton);
  tb->addWidget(m_exportAnimaticButton);
  mainLayout->addLayout(tb);

  QFrame *sep = new QFrame();
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet("color:#444;");
  mainLayout->addWidget(sep);

  m_scrollArea = new QScrollArea();
  m_scrollArea->setWidgetResizable(true);
  // Pin the scroll-bar policies to stable states to avoid the
  // QAbstractScrollArea layout-recursion crash (same class as the QTextEdit
  // fix above).  PanelWidget height tracks its width (aspect-ratio preview);
  // with the default ScrollBarAsNeeded the vertical scrollbar appearing/
  // disappearing changes the viewport WIDTH → changes panel width → changes
  // panel HEIGHT → re-toggles the scrollbar, oscillating forever.  On Windows
  // QScrollArea's internal setWidgetResizable eventFilter runs this loop
  // synchronously and recursively → stack overflow (0-byte crash dump, silent
  // exit).  It only bites at the transient tiny window geometry during startup
  // (Storyboard workflow chosen from the launch popup), which is why switching
  // to the room AFTER the window is sized does not crash.
  // Vertical AlwaysOn keeps the viewport width constant; Horizontal AlwaysOff
  // since the grid always reflows to the available width.
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setStyleSheet("QScrollArea{background:#1e1e1e;border:none;}");
  // StrongFocus so that setFocus() on the scroll area works: the QShortcuts
  // installed on StoryboardPanel (WidgetWithChildrenShortcut) fire when any
  // child has focus — and m_scrollArea IS a child of StoryboardPanel.
  m_scrollArea->setFocusPolicy(Qt::StrongFocus);

  m_container = new QWidget();
  m_container->setStyleSheet("background:#1e1e1e;");
  m_grid = new QGridLayout(m_container);
  m_grid->setSpacing(8);
  m_grid->setContentsMargins(8, 8, 8, 8);
  m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_scrollArea->setWidget(m_container);
  // Lazy thumbnail loading: refresh visible panels after scrolling stops.
  // Connecting valueChanged → updateVisiblePreviews() directly calls
  // renderXsheetFrame() on every scroll tick, which blocks the main thread
  // and makes scrolling sluggish on complex scenes.  Debounce: wait 250 ms
  // after the last scroll event before rendering the newly visible panels.
  {
    QTimer *scrollDebounce = new QTimer(this);
    scrollDebounce->setSingleShot(true);
    scrollDebounce->setInterval(250);
    connect(scrollDebounce, &QTimer::timeout,
            this, [this](){ updateVisiblePreviews(); });
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [scrollDebounce](int){ scrollDebounce->start(); });
  }

  // ---- EDIT PAGE (lazy - comboViewer created on first use) ----
  QWidget *editPage = new QWidget();
  editPage->setStyleSheet("background:#1a1a2a;");
  QVBoxLayout *editLayout = new QVBoxLayout(editPage);
  editLayout->setSpacing(8);
  editLayout->setContentsMargins(8, 8, 8, 8);

  QLabel *editHint = new QLabel("Shot open in viewer - draw, then click Back.");
  editHint->setStyleSheet("color:#888;font-size:11px;");
  editHint->setAlignment(Qt::AlignCenter);

  editLayout->addStretch();
  editLayout->addWidget(editHint);
  editLayout->addStretch();

  // ---- STACK ----
  m_stack = new QStackedWidget();
  m_stack->addWidget(m_scrollArea);  // 0 = board
  m_stack->addWidget(editPage);      // 1 = edit
  mainLayout->addWidget(m_stack);
  setWidget(main);

  connect(m_addShotButton, &QToolButton::clicked, this, &StoryboardPanel::onAddShot);
  m_durationCommitTimer = new QTimer(this);
  m_durationCommitTimer->setSingleShot(true);
  m_durationCommitTimer->setInterval(600);
  connect(m_durationCommitTimer, &QTimer::timeout, this, &StoryboardPanel::commitDurationUndo);

  m_panelDetectTimer = new QTimer(this);
  m_panelDetectTimer->setSingleShot(true);
  m_panelDetectTimer->setInterval(1000);
  connect(m_panelDetectTimer, &QTimer::timeout, this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene || scene->getChildStack()->getAncestorCount() == 0) return;
    AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
    if (!node) return;
    int col = node->m_col;
    m_dirtyShotCol = col;
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (m_shots[si].data.xsheetColumn == col) {
        detectAndUpdatePanels(si);
        // Invalidate stale thumbnails for this shot so updateVisiblePreviews()
        // will re-render them even if a pixmap already existed.
        for (PanelWidget *pw : m_shots[si].panels)
          pw->setPreviewPixmap(QPixmap());
        updateVisiblePreviews();
        break;
      }
    }
  });
  connect(m_deleteButton, &QToolButton::clicked, this, &StoryboardPanel::onDeleteShot);
  connect(m_mergeButton, &QToolButton::clicked, this, &StoryboardPanel::onMergeShots);
  connect(m_copyButton,   &QToolButton::clicked, this, &StoryboardPanel::onCopyShot);
  connect(m_cloneButton,  &QToolButton::clicked, this, &StoryboardPanel::onCloneShot);
  connect(m_pasteButton,  &QToolButton::clicked, this, &StoryboardPanel::onPasteShot);
  connect(m_exportPdfButton, &QToolButton::clicked, this, &StoryboardPanel::onExportPdf);
  connect(m_exportShotsButton, &QToolButton::clicked, this, &StoryboardPanel::onExportShots);
  connect(m_exportAnimaticButton, &QToolButton::clicked, this, &StoryboardPanel::onExportAnimatic);
  connect(m_camLabelButton, &QToolButton::toggled, this, [this](bool on) {
    m_showCamMoveType = on;
    QSettings().setValue("Ztoryc/ShowCamMoveType", on);
    // Invalidate thumbnails so the type label appears/disappears on re-render.
    for (auto &shot : m_shots)
      for (PanelWidget *pw : shot.panels)
        if (pw) pw->setPreviewPixmap(QPixmap());
    updateVisiblePreviews();
  });
  connect(m_lightEditButton, &QToolButton::toggled, this, [this](bool on) {
    PanelWidget::setLightEditMode(on);
    // Arrows must be visible while placing them.
    if (on && !m_lightShowButton->isChecked()) m_lightShowButton->setChecked(true);
    for (auto &shot : m_shots)
      for (PanelWidget *pw : shot.panels)
        if (pw) pw->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
  });
  connect(m_lightShowButton, &QToolButton::toggled, this, [this](bool on) {
    m_showLights = on;
    QSettings().setValue("Ztoryc/ShowLightDirection", on);
    if (!on && m_lightEditButton->isChecked()) m_lightEditButton->setChecked(false);
    // Re-bake thumbnails with/without the light gizmo.
    for (auto &shot : m_shots)
      for (PanelWidget *pw : shot.panels)
        if (pw) pw->setPreviewPixmap(QPixmap());
    updateVisiblePreviews();
  });
  connect(m_lightColorButton, &QToolButton::clicked, this, [this]() {
    QColor cur(QSettings().value("Ztoryc/LightColor", "#FFC34D").toString());
    QColor c = QColorDialog::getColor(cur, this, tr("Light colour"));
    if (!c.isValid()) return;
    QSettings().setValue("Ztoryc/LightColor", c.name());
    PanelWidget::setLightColor(c.name());
    m_lightColorButton->setStyleSheet(
        QString("QToolButton{background:%1;border:1px solid #555;"
                "border-radius:4px;margin:6px;}"
                "QToolButton:hover{border:1px solid #aaa;}").arg(c.name()));
  });
  connect(m_columnsPerRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StoryboardPanel::onColumnsChanged);
  connect(m_numberingCombo, QOverload<int>::of(&QComboBox::activated),
          this, &StoryboardPanel::onNumberingChanged);
  connect(m_numberingBtn, &QToolButton::clicked,
          this, &StoryboardPanel::onNumberingConfig);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &StoryboardPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &StoryboardPanel::onXsheetChanged);
  // Sync durations when ZtoryModel resequences (works even inside sub-scenes)
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &StoryboardPanel::onModelResequenced);
  // Persist the imported screenplay: setScriptFile() (called by the Script
  // panel on import) emits scriptFileChanged → write it into the .ztoryc.
  // A plain File>Save does not call saveZtoryc(); the .ztoryc is kept in sync
  // by each edit action instead, so the import must trigger its own save.
  // m_loadingZtoryc guards the redundant save while loadZtoryc() itself runs.
  connect(ZtoryModel::instance(), &ZtoryModel::scriptFileChanged, this,
          [this]() { if (!m_loadingZtoryc) saveZtoryc(); });
  connect(ZtoryModel::instance(), &ZtoryModel::shotAdded,
          this, &StoryboardPanel::onShotInserted);
  connect(ZtoryModel::instance(), &ZtoryModel::shotRemovedAt,
          this, &StoryboardPanel::onShotRemovedAt);
  // Reflect ZtoryModel text-field changes (typed in the Shot Board) back into
  // the Board's PanelWidgets so the two views stay in sync. m_updating guards
  // against echoes when the change originated from this Board.
  // Mirror selection changes from the Animatic timeline onto the board grid.
  connect(ZtoryModel::instance(), &ZtoryModel::sharedSelectionChanged, this,
          [this]() { applySharedSelection(); });

  connect(ZtoryModel::instance(), &ZtoryModel::shotDataChanged, this,
          [this](int si) {
    if (m_updating) return;
    if (si < 0 || si >= (int)m_shots.size()) return;
    const auto &mPanels = ZtoryModel::instance()->shot(si).panels;
    auto &shot = m_shots[si];
    for (int pi = 0; pi < (int)shot.panels.size() && pi < (int)mPanels.size() &&
                     pi < (int)shot.data.panels.size(); pi++) {
      const PanelData &src = mPanels[pi];
      PanelData       &dst = shot.data.panels[pi];
      if (dst.dialog != src.dialog) {
        dst.dialog = src.dialog;
        shot.panels[pi]->setDialog(src.dialog);
      }
      if (dst.action != src.action) {
        dst.action = src.action;
        shot.panels[pi]->setAction(src.action);
      }
      if (dst.notes != src.notes) {
        dst.notes = src.notes;
        shot.panels[pi]->setNotes(src.notes);
      }
      // Light-direction gizmo edited from the Shot Board navigator: mirror it,
      // re-bake the thumbnail and persist (text fields are saved by their own
      // Board flows; the navigator has no other path to the .ztoryc).
      if (dst.hasLight != src.hasLight ||
          dst.lightTailX != src.lightTailX || dst.lightTailY != src.lightTailY ||
          dst.lightTipX != src.lightTipX || dst.lightTipY != src.lightTipY ||
          dst.lightDepth != src.lightDepth ||
          dst.lightSpread != src.lightSpread ||
          dst.lightColor != src.lightColor) {
        dst.hasLight    = src.hasLight;
        dst.lightTailX  = src.lightTailX;
        dst.lightTailY  = src.lightTailY;
        dst.lightTipX   = src.lightTipX;
        dst.lightTipY   = src.lightTipY;
        dst.lightDepth  = src.lightDepth;
        dst.lightSpread = src.lightSpread;
        dst.lightColor  = src.lightColor;
        updatePreview(si, pi);
        saveZtoryc();
      }
    }
  });
  // Debounce timer per refresh thumbnail
  QTimer *refreshTimer = new QTimer(this);
  refreshTimer->setSingleShot(true);
  refreshTimer->setInterval(800);
  connect(refreshTimer, &QTimer::timeout, this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene || scene->getChildStack()->getAncestorCount() == 0) return;
    AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
    if (!node) return;
    int col = node->m_col;
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (m_shots[si].data.xsheetColumn == col) {
        updateVisiblePreviews();  // lazy: only render panels visible in viewport
        break;
      }
    }
  });
  connect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched,
          this, [this](){ m_panelDetectTimer->start(); });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this](){
    disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
               this, nullptr);
    connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
            this, &StoryboardPanel::onXsheetChanged);
    // While inside a sub-scene, each xsheet change (drawing, erasing) restarts
    // the detect timer so the Board thumbnail stays in sync without flooding renders.
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    if (sc && sc->getChildStack()->getAncestorCount() > 0) {
      connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
              this, [this, sc](){
        m_panelDetectTimer->start();
        // Track which shot is being edited so showEvent can invalidate its thumbnail.
        if (sc->getChildStack()->getAncestorCount() > 0) {
          AncestorNode *n = sc->getChildStack()->getAncestorInfo(0);
          if (n) m_dirtyShotCol = n->m_col;
        }
      });
    }
    // Refresh automatico thumbnail
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    int ancestorCount = scene->getChildStack()->getAncestorCount();
    if (ancestorCount == 0) {
      // Tornati al main — refresh completo
      QTimer::singleShot(200, this, &StoryboardPanel::onRefreshPreviews);
    } else {
      // Entrati in una sottoscena — registra quale shot è aperto (per il
      // refresh forzato al ritorno nel Board) e aggiorna il thumbnail corrente.
      AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
      if (node) {
        int col = node->m_col;
        m_dirtyShotCol = col;  // sempre, anche se non si disegna nulla
        for (int si = 0; si < (int)m_shots.size(); si++) {
          if (m_shots[si].data.xsheetColumn == col) {
            for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
              QTimer::singleShot(300, this, [this, si, pi](){ updatePreview(si, pi); });
            break;
          }
        }
      }
    }
  });

  // Install hover event filter on toolbar buttons to show contextual hints.
  struct BtnHint { QToolButton *btn; const char *hint; };
  for (auto bh : std::initializer_list<BtnHint>{
    {m_addShotButton,        "Add a new empty shot at the end of the sequence"},
    {m_deleteButton,         "Delete the selected shot (this cannot be undone)"},
    {m_cloneButton,          "Clone shot -> creates an independent copy with the same content"},
    {m_copyButton,           "Copy shot -- shared clipboard with the Animatic. Paste in Board or Animatic"},
    {m_pasteButton,          "Paste the last copied shot after the current selection"},
    {m_mergeButton,          "Merge two adjacent shots into one (durations are summed)"},
    {m_exportPdfButton,      "Export the Board as PDF: thumbnails, dialog, and per-panel/shot durations"},
    {m_exportShotsButton,    "Export each shot as a standalone scene"},
    {m_exportAnimaticButton, "Export the full animatic as a video with audio"},
    {m_lightEditButton,      "Light direction: drag to place the conic arrow (tail = source); wheel tilts it toward camera/background, Shift+wheel sets the beam spread (hard/soft light). Right-click removes it"},
    {m_lightShowButton,      "Show or hide the light-direction arrows on thumbnails and PDF (shortcut L)"},
    {m_lightColorButton,     "Pick the light colour -- use warm/cool tones to note the light temperature"},
  }) {
    bh.btn->installEventFilter(this);
    bh.btn->setProperty("ztoryHint", tr(bh.hint));
  }
}

void StoryboardPanel::addPanelWidget(int shotIdx, int panelIdx) {
  Shot &shot = m_shots[shotIdx];
  PanelWidget *pw = new PanelWidget(m_container);
  pw->setFps(m_fps);
  pw->setShotIndex(shotIdx);
  pw->setPanelIndex(panelIdx, (int)shot.data.panels.size());
  pw->setShotNumber(shot.data.label());
  // Show sequence label if one is assigned; otherwise show "—"
  {
    QString seqLbl;
    if (!shot.data.sequenceId.isEmpty()) {
      const SequenceData *seq = ZtoryModel::instance()->findSequence(shot.data.sequenceId);
      if (seq) seqLbl = seq->label;
    }
    pw->setSeqLabel(seqLbl);
  }
  pw->setDuration(shot.data.panels[panelIdx].duration);
  pw->setTotalDuration(shot.data.totalDuration());
  pw->setDialog(shot.data.panels[panelIdx].dialog);
  pw->setAction(shot.data.panels[panelIdx].action);
  pw->setNotes(shot.data.panels[panelIdx].notes);
  connectPanelWidget(pw);
  shot.panels.push_back(pw);
}

void StoryboardPanel::connectPanelWidget(PanelWidget *pw) {
  // Install event filter on text fields for text-edit undo (focusIn/focusOut).
  for (QTextEdit *te : pw->textFields())
    te->installEventFilter(this);
  connect(pw, &PanelWidget::editRequested, this, &StoryboardPanel::onEditShot);
  connect(pw, &PanelWidget::matchDurationRequested, this, &StoryboardPanel::onMatchDuration);
  connect(pw, &PanelWidget::durationChanged, this, &StoryboardPanel::onDurationChanged);
  connect(pw, &PanelWidget::totalDurationChanged, this, [this, pw](int frames){
    // Trova lo shot corrispondente e aggiorna la durata sul main xsheet
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (!m_shots[si].panels.empty() && m_shots[si].panels[0] == pw) {
        int col = m_shots[si].data.xsheetColumn;
        onDurationChanged(si, 0, frames);
        break;
      }
    }
  });
  connect(pw, &PanelWidget::clicked, this, &StoryboardPanel::onPanelClicked);
  connect(pw, &PanelWidget::dropReceived, this, &StoryboardPanel::onMoveShot);
  connect(pw, &PanelWidget::lightPlaced, this, &StoryboardPanel::onLightPlaced);
  connect(pw, &PanelWidget::lightRemoveRequested, this,
          &StoryboardPanel::onLightRemoved);
  // Widgets created while light edit mode is already active (grid rebuilds)
  // must pick up the placement cursor too.
  if (m_lightEditButton && m_lightEditButton->isChecked())
    pw->setCursor(Qt::CrossCursor);
  // Re-render at higher resolution when the panel grows beyond its stored pixmap.
  connect(pw, &PanelWidget::previewRerenderNeeded, this,
          [this](int si, int pi) {
    m_rerenderQueue.insert({si, pi});
    if (!m_rerenderTimer) {
      m_rerenderTimer = new QTimer(this);
      m_rerenderTimer->setSingleShot(true);
      connect(m_rerenderTimer, &QTimer::timeout, this, [this]() {
        for (auto &p : m_rerenderQueue)
          updatePreview(p.first, p.second);
        m_rerenderQueue.clear();
      });
    }
    m_rerenderTimer->start(200);  // coalesce rapid resize events into one render
  });
  connect(pw, &PanelWidget::dataChanged, [this](int si, int pi){
    if (si >= 0 && si < (int)m_shots.size()) {
      // Update both shotLabel (primary) and shotNumber (legacy compat)
      QString edited = m_shots[si].panels[0]->shotNumber();
      m_shots[si].data.shotLabel  = edited;
      m_shots[si].data.shotNumber = edited;
      for (PanelWidget *p : m_shots[si].panels)
        p->setShotNumber(m_shots[si].data.label());
      updateColumnName(si);
      // Sync text fields (dialog/action/notes) from the PanelWidget to the
      // data model, then push to ZtoryModel so the Shot Board (and any other
      // listener) sees the change live.
      if (pi >= 0 && pi < (int)m_shots[si].data.panels.size() &&
          pi < (int)m_shots[si].panels.size()) {
        PanelWidget *pw = m_shots[si].panels[pi];
        PanelData   &pd = m_shots[si].data.panels[pi];
        pd.dialog = pw->dialog();
        pd.action = pw->action();
        pd.notes  = pw->notes();
      }
      m_updating = true;  // guard against shotDataChanged echo
      ZtoryModel::instance()->syncShotPanels(si, m_shots[si].data.panels,
                                             m_shots[si].data.shotLabel,
                                             m_shots[si].data.xsheetColumn);
      m_updating = false;
    }
    saveZtoryc();
  });
  // Sequence field edited: cascade the sequence assignment forward until a
  // shot with a different (non-empty) sequenceId is encountered.
  connect(pw, &PanelWidget::seqLabelEdited, this,
          [this](int si, QString fullLabel) {
    if (si < 0 || si >= (int)m_shots.size()) return;
    ZtoryModel *model = ZtoryModel::instance();
    SequenceData *seq = model->findOrCreateSequence(fullLabel);
    if (!seq) return;
    // Remember what sequenceId the target shot had before the edit so we
    // know when to stop cascading (stop at the first shot that already
    // belongs to a *different* sequence).
    QString prevSeqId = m_shots[si].data.sequenceId;
    for (int i = si; i < (int)m_shots.size(); i++) {
      if (i > si && !m_shots[i].data.sequenceId.isEmpty() &&
          m_shots[i].data.sequenceId != prevSeqId)
        break;
      m_shots[i].data.sequenceId = seq->uuid;
      for (PanelWidget *pw2 : m_shots[i].panels)
        pw2->setSeqLabel(seq->label);
      if (i < model->shotCount())
        model->shot(i).sequenceId = seq->uuid;
    }
    // If resetOnSeqChange is active, renumber all shots so SH numbers
    // are recalculated relative to their (new) sequence.
    const NumberingConfig &cfg = model->numberingConfig();
    if (m_autoRenumber && cfg.resetOnSeqChange)
      renumberAll();
    model->save();
    saveZtoryc();
  });
}

void StoryboardPanel::assignBoardShotLabel(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  // Project Board shots to a plain ShotData vector so we can use the shared
  // static algorithm (which needs the full neighbour context).
  std::vector<ShotData> shotDatas;
  shotDatas.reserve(m_shots.size());
  for (const Shot &s : m_shots) shotDatas.push_back(s.data);
  ZtoryModel::assignShotLabel(shotDatas, si, ZtoryModel::instance()->numberingConfig());
  // Copy result back
  m_shots[si].data.shotLabel  = shotDatas[si].shotLabel;
  m_shots[si].data.orderIndex = shotDatas[si].orderIndex;
  m_shots[si].data.shotNumber = shotDatas[si].shotLabel;
}

void StoryboardPanel::renumberAll() {
  ZtoryModel *model             = ZtoryModel::instance();
  const NumberingConfig &cfg    = model->numberingConfig();
  const int scale               = 100;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    Shot &shot = m_shots[i];
    if (m_autoRenumber) {
      // Auto mode: reassign ALL labels with clean sequential numbering.
      // This correctly handles inserts: the new shot takes the "next" position
      // and existing shots above it get renumbered (e.g. SH020→SH030).
      // Sequence assignments survive auto-renumber — only the SH number changes.
      // If this is a brand-new shot (no sequence yet), inherit from the
      // previous shot so it lands in the same sequence automatically.
      // Shot 0 in Sequence mode gets the default sequence (e.g. "SQ01").
      if (shot.data.sequenceId.isEmpty() && cfg.style == NumberingConfig::Sequence) {
        if (i > 0 && !m_shots[i - 1].data.sequenceId.isEmpty())
          shot.data.sequenceId = m_shots[i - 1].data.sequenceId;
        else if (i == 0) {
          model->ensureDefaultSequence();
          if (!model->sequences().empty())
            shot.data.sequenceId = model->sequences().front().uuid;
        }
      }

      // Compute the shot index within its sequence (for resetOnSeqChange)
      int shotIdx = i;  // global index by default (continuous numbering)
      if (cfg.resetOnSeqChange && cfg.style == NumberingConfig::Sequence) {
        // Count how many shots before i share the same sequenceId
        shotIdx = 0;
        for (int j = 0; j < i; j++)
          if (m_shots[j].data.sequenceId == shot.data.sequenceId)
            shotIdx++;
      }
      int shotNum = cfg.startNumber + shotIdx * cfg.step;
      QString shPart = cfg.shotPrefix +
          QString("%1").arg(shotNum, cfg.padding, 10, QChar('0'));
      shot.data.shotLabel  = shPart;
      shot.data.shotNumber = shPart;
      shot.data.orderIndex = (cfg.startNumber + i * cfg.step) * scale;
    } else if (shot.data.shotLabel.isEmpty()) {
      // Keep-# mode: only assign label to slots that have none yet.
      // Use the midpoint algorithm on the Board's own shot list.
      assignBoardShotLabel(i);
    }
    updateColumnName(i);
    // Resolve sequence label for display
    QString seqLabel;
    if (!shot.data.sequenceId.isEmpty()) {
      SequenceData *seq = model->findSequence(shot.data.sequenceId);
      if (seq) seqLabel = seq->label;
    }
    bool isSeq = (cfg.style == NumberingConfig::Sequence);
    for (PanelWidget *pw : shot.panels) {
      pw->setShotIndex(i);
      pw->setShotNumber(shot.data.label());
      pw->setSeqVisible(isSeq);
      if (!seqLabel.isEmpty()) pw->setSeqLabel(seqLabel);
      pw->setPanelIndex(pw->panelIndex(), (int)shot.panels.size());
    }
  }
}

void StoryboardPanel::clearShots() {
  // Clear the path FIRST so that any saveZtoryc() that fires while widgets are
  // being destroyed (e.g. QTextEdit focusOut events during delete) returns early
  // instead of writing stale data to the new scene's file.
  m_currentZtoryPath.clear();
  for (Shot &shot : m_shots)
    for (PanelWidget *pw : shot.panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
  m_shots.clear();
  m_selectedShotIndex = -1;
}

void StoryboardPanel::resequenceXsheet() {
  ZtoryModel::instance()->resequenceXsheet();
}

void StoryboardPanel::rebuildGrid() {
  for (Shot &shot : m_shots)
    for (PanelWidget *pw : shot.panels)
      m_grid->removeWidget(pw);
  int col = 0, row = 0;
  for (Shot &shot : m_shots) {
    for (PanelWidget *pw : shot.panels) {
      m_grid->addWidget(pw, row, col);
      pw->show();
      pw->updateGeometry();
      col++;
      if (col >= m_columnsPerRow) { col = 0; row++; }
    }
  }
  m_container->adjustSize();
  QTimer::singleShot(200, this, [this](){
    int available = m_scrollArea->viewport()->width() - 8 * (m_columnsPerRow + 1);
    int colW = qMax(150, available / m_columnsPerRow);
    for (Shot &shot : m_shots)
      for (PanelWidget *pw : shot.panels) {
        pw->setFixedWidth(colW);
        pw->rescalePreview();
      }
    m_container->adjustSize();
  });
}

void StoryboardPanel::selectShot(int shotIdx) {
  // Deseleziona tutti
  for (int i : m_selectedIndices)
    if (i >= 0 && i < (int)m_shots.size())
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(false);
  m_selectedIndices.clear();
  if (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
    for (PanelWidget *pw : m_shots[m_selectedShotIndex].panels)
      pw->setSelected(false);
  m_selectedShotIndex = shotIdx;
  if (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
    for (PanelWidget *pw : m_shots[m_selectedShotIndex].panels)
      pw->setSelected(true);
}

void StoryboardPanel::applySharedSelection() {
  const std::set<int> &cols = ZtoryModel::instance()->sharedSelection();
  // Map xsheet columns → shot indices.
  std::set<int> wantIdx;
  for (int si = 0; si < (int)m_shots.size(); si++)
    if (cols.count(m_shots[si].data.xsheetColumn)) wantIdx.insert(si);

  if (wantIdx == m_selectedIndices) return;  // already in sync — no repaint

  // Clear current selection visuals.
  for (int i : m_selectedIndices)
    if (i >= 0 && i < (int)m_shots.size())
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(false);
  if (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
    for (PanelWidget *pw : m_shots[m_selectedShotIndex].panels)
      pw->setSelected(false);

  m_selectedIndices = wantIdx;
  for (int i : m_selectedIndices)
    if (i >= 0 && i < (int)m_shots.size())
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(true);
  m_selectedShotIndex = wantIdx.empty() ? -1 : *wantIdx.begin();
}

void StoryboardPanel::updateVisiblePreviews() {
  // Render thumbnails only for PanelWidgets whose geometry intersects the
  // scroll-area viewport AND that don't already have a thumbnail.
  // Skipping panels with existing pixmaps avoids redundant heavy renders when
  // the user scrolls back to already-loaded panels.
  // updatePreview() is called explicitly (e.g. from previewRerenderNeeded) when
  // a higher-resolution re-render is needed after a panel resize.
  if (!m_scrollArea) return;
  QRect vpRect = m_scrollArea->viewport()->rect();

  for (int si = 0; si < (int)m_shots.size(); si++) {
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++) {
      PanelWidget *pw = m_shots[si].panels[pi];
      if (!pw) continue;
      // Skip if already has a thumbnail — re-render only on explicit request
      // (window resize → previewRerenderNeeded) or manual refresh.
      if (!pw->previewPixmap().isNull()) continue;
      // Map the panel widget rect to viewport coordinates.
      QRect widgetRect = QRect(pw->mapTo(m_scrollArea->viewport(),
                                         QPoint(0, 0)), pw->size());
      if (vpRect.intersects(widgetRect))
        updatePreview(si, pi);
    }
  }
}

// Forward declarations — implementations just before detectAndUpdatePanels()
static void computeCameraMove(TXsheet *, PanelData &, int, ToonzScene *);
static void classifyCameraMove(PanelData &);
static void applyCameraOverlay(QPixmap &, const PanelData &, int panelIdx,
                               bool showTypeLabel = true, double labelPxSize = 0);

// Spreadsheet-style camera-position letter: 0→A, 1→B … 25→Z, 26→AA …
static QString camLetter(int i) {
  QString s;
  i = qMax(0, i);
  do { s.prepend(QChar('A' + i % 26)); i = i / 26 - 1; } while (i >= 0);
  return s;
}

// Camera-move overlay geometry, in the RENDER camera's getCameraAff-local space
// (Y up). The bounding box of both apertures is padded to the thumbnail aspect
// so the backed-out render (renderXsheetFrameRegion) and the drawn START/STOP
// rectangles share one coordinate frame and stay aligned.
struct CamOverlayGeom {
  bool    valid = false;
  bool    renderedStart = true;
  TPointD renderC[4];                 // render aperture corners (Y up)
  TPointD otherC[4];                  // other  aperture corners (Y up)
  double  minX = 0, minY = 0, maxX = 0, maxY = 0;  // padded bbox
};

static CamOverlayGeom computeCamOverlayGeom(const PanelData &pd,
                                            double targetAspect) {
  CamOverlayGeom g;
  if (pd.cameraMoveType == PanelData::CamNone) return g;
  if (pd.camW < 1e-6 || pd.camH < 1e-6 || targetAspect < 1e-6) return g;

  const double hw = Stage::inch * pd.camW / 2.0;   // aperture in affine space
  const double hh = Stage::inch * pd.camH / 2.0;

  TAffine a0(pd.camA0[0], pd.camA0[1], pd.camA0[2],
             pd.camA0[3], pd.camA0[4], pd.camA0[5]);
  TAffine a1(pd.camA1[0], pd.camA1[1], pd.camA1[2],
             pd.camA1[3], pd.camA1[4], pd.camA1[5]);

  g.renderedStart = (pd.camRenderFrame == pd.startFrame);
  TAffine aRender = g.renderedStart ? a0 : a1;
  TAffine aOther  = g.renderedStart ? a1 : a0;
  TAffine M       = aRender.inv() * aOther;  // other-local → render-local

  TPointD loc[4] = {TPointD(-hw, hh), TPointD(hw, hh),
                    TPointD(hw, -hh), TPointD(-hw, -hh)};
  double minX = 1e30, minY = 1e30, maxX = -1e30, maxY = -1e30;
  for (int i = 0; i < 4; i++) {
    g.renderC[i] = loc[i];
    g.otherC[i]  = M * loc[i];
    for (const TPointD &p : {g.renderC[i], g.otherC[i]}) {
      minX = qMin(minX, p.x); maxX = qMax(maxX, p.x);
      minY = qMin(minY, p.y); maxY = qMax(maxY, p.y);
    }
  }
  double bw = maxX - minX, bh = maxY - minY;
  if (bw < 1e-6 || bh < 1e-6) return g;

  // Pad the smaller dimension so bbox aspect == thumbnail aspect (uniform map).
  double cx = (minX + maxX) / 2.0, cy = (minY + maxY) / 2.0;
  if (bw / bh < targetAspect) bw = bh * targetAspect;
  else                        bh = bw / targetAspect;
  g.minX = cx - bw / 2.0; g.maxX = cx + bw / 2.0;
  g.minY = cy - bh / 2.0; g.maxY = cy + bh / 2.0;
  g.valid = true;
  return g;
}

void StoryboardPanel::updatePreview(int shotIdx, int panelIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  Shot &shot = m_shots[shotIdx];
  if (panelIdx < 0 || panelIdx >= (int)shot.panels.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int col = shot.data.xsheetColumn;
  TXshChildLevel *cl = nullptr;
  for (int r = 0; r <= xsh->getFrameCount(); r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!cl) return;
  TXsheet *subXsh = cl->getXsheet();
  if (!subXsh) return;

  const PanelData &pd = shot.data.panels[panelIdx];
  // Camera move panels render at camRenderFrame (widest view); others at startFrame.
  int frame = (pd.cameraMoveType != PanelData::CamNone)
              ? pd.camRenderFrame
              : pd.startFrame;

  // Render at the panel widget's actual physical pixel width for crisp display
  // on both Retina and standard screens. Stored WITHOUT a DPR tag so that
  // rescalePreview() can apply DPR at draw time for the current screen.
  PanelWidget *pw = shot.panels[panelIdx];
  qreal dpr = 1.0;
  if (QWindow *win = pw->window()->windowHandle())
    dpr = win->devicePixelRatio();
  int physW = int((pw->width() - 8) * dpr);
  if (physW < 64) physW = 320;   // widget not yet laid out — use safe default
  physW = qMin(physW, 1280);
  int physH = physW * 9 / 16;

  QPixmap px;
  // For camera-move panels render "backed out" to cover both START and STOP
  // frames (so e.g. a pan shows the full traversed artwork, not a single
  // cropped frame). The same padded bbox is used by applyCameraOverlay so the
  // drawn rectangles align with the rendered content.
  CamOverlayGeom g = computeCamOverlayGeom(pd, (double)physW / physH);
  if (g.valid) {
    // localBBox is in getCameraAff-local; renderFrame works in getPlacement-
    // local, which differs by the camera Z scale → convert with TScale(zf).
    TStageObjectId camId = subXsh->getStageObjectTree()->getCurrentCameraId();
    double z  = subXsh->getZ(camId, frame);
    double zf = (1000.0 + z) / 1000.0;
    TRectD placedRect = TScale(zf) * TRectD(g.minX, g.minY, g.maxX, g.maxY);
    px = IconGenerator::renderXsheetFrameRegion(subXsh, frame,
                                                TDimension(physW, physH),
                                                placedRect);
  } else {
    px = IconGenerator::renderXsheetFrame(subXsh, frame, TDimension(physW, physH));
  }
  if (!px.isNull()) {
    // Letter index = how many camera-move panels precede this one in the shot,
    // so the first MOVE is A→B (panels without a move don't consume letters).
    int moveOrdinal = 0;
    for (int k = 0; k < panelIdx && k < (int)shot.data.panels.size(); k++)
      if (shot.data.panels[k].cameraMoveType != PanelData::CamNone) moveOrdinal++;
    applyCameraOverlay(px, pd, moveOrdinal, m_showCamMoveType);
    if (m_showLights) ztoryApplyLightOverlay(px, pd);
    shot.panels[panelIdx]->setPreviewPixmap(px);
  }
}

QString StoryboardPanel::ztoryPath() const {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return QString();
  TFilePath sp = scene->getScenePath();
  if (sp.isEmpty()) return QString();
  QString path = QString::fromStdWString(sp.getWideString());
  path.replace(QRegularExpression("\.tnz$"), ".ztoryc");
  return path;
}

void StoryboardPanel::updateColumnName(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Always use the TOP xsheet — scene->getXsheet() returns the CURRENT
  // (possibly sub) xsheet, which would rename the wrong columns when the
  // user is inside a shot at the time of the update (BUG: columns inside
  // the sub-scene were getting labeled "SH010", "SH020" etc.).
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int col = si; // la colonna corrisponde all indice dello shot
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(TStageObjectId::ColumnId(col), false);
  if (obj) {
    obj->setName(m_shots[si].data.label().toStdString());
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
}

void StoryboardPanel::syncWidgetsToData() {
  // Copy every PanelWidget's text fields into the data model before saving.
  // This is necessary because textChanged only emits dataChanged (which updates
  // shotLabel) but never writes dialog/action/notes back to data.panels[pi].
  for (int si = 0; si < (int)m_shots.size(); si++) {
    Shot &shot = m_shots[si];
    for (int pi = 0; pi < (int)shot.panels.size() && pi < (int)shot.data.panels.size(); pi++) {
      shot.data.panels[pi].dialog = shot.panels[pi]->dialog();
      shot.data.panels[pi].action = shot.panels[pi]->action();
      shot.data.panels[pi].notes  = shot.panels[pi]->notes();
    }
  }
}

void StoryboardPanel::saveZtoryc() {
  // Use m_currentZtoryPath (set at end of refreshFromScene) instead of
  // ztoryPath() so we never write m_shots data to a different scene's file.
  // While m_shots is being rebuilt (clearShots clears it), this is empty →
  // saves are suppressed, preventing cross-scene text contamination.
  if (m_currentZtoryPath.isEmpty()) return;
  // Scene-switch race guard: on sceneSwitched the Script panel rebinds
  // scriptFile to the NEW scene (clearing it when that scene has no screenplay)
  // BEFORE this panel rebinds m_currentZtoryPath/m_shots.  Without this check a
  // saveZtoryc() fired by that scriptFileChanged would write the cleared
  // scriptFile onto the PREVIOUS scene's .ztoryc — losing its imported script.
  // Only persist when m_currentZtoryPath still matches the scene that is
  // actually open (i.e. the data in m_shots/m_scriptFile belongs to it).
  if (ztoryPath() != m_currentZtoryPath) return;
  // DATA-LOSS FIREWALL (task 48): never overwrite the .ztoryc with ZERO shots
  // while the top xsheet still contains child-level columns.  An empty m_shots
  // in that state is always a transient/buggy display wipe (e.g. the undo-wipe
  // bug), not a real user action — persisting it would destroy every label,
  // dialog and note.
  if (m_shots.empty()) {
    ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
    TXsheet *topXsh = scn ? scn->getChildStack()->getTopXsheet() : nullptr;
    if (topXsh) {
      for (int c = 0; c < topXsh->getColumnCount(); c++) {
        TXshColumn *column = topXsh->getColumn(c);
        if (!column || column->isEmpty()) continue;
        int cr0 = 0, cr1 = 0;
        column->getRange(cr0, cr1);
        TXshCell cell = topXsh->getCell(cr0, c);
        if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
          qWarning("[ZTORY] saveZtoryc BLOCKED: m_shots empty but xsheet has "
                   "child columns (col %d) — refusing to wipe %s",
                   c, m_currentZtoryPath.toUtf8().constData());
          return;
        }
      }
    }
  }
  syncWidgetsToData();
  QString path = m_currentZtoryPath;
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  // Project metadata (production + title entered by user at scene creation).
  {
    ZtoryModel *model = ZtoryModel::instance();
    if (!model->production().isEmpty() || !model->title().isEmpty()) {
      xml.writeStartElement("project");
      xml.writeAttribute("production", model->production());
      xml.writeAttribute("title",      model->title());
      xml.writeEndElement();
    }
  }
  // Imported screenplay (Script panel) — project-relative path.
  {
    QString sf = ZtoryModel::instance()->scriptFile();
    if (!sf.isEmpty()) xml.writeTextElement("scriptFile", sf);
  }
  // Numbering scheme + sequence list — so the SQ/SH structure survives reload
  // (previously only per-shot number/label were saved, so sequences were lost).
  {
    ZtoryModel *model = ZtoryModel::instance();
    const NumberingConfig &cfg = model->numberingConfig();
    xml.writeStartElement("numbering");
    xml.writeAttribute("style",       QString::number((int)cfg.style));
    xml.writeAttribute("shotPrefix",  cfg.shotPrefix);
    xml.writeAttribute("seqPrefix",   cfg.seqPrefix);
    xml.writeAttribute("panelPrefix", cfg.panelPrefix);
    xml.writeAttribute("step",        QString::number(cfg.step));
    xml.writeAttribute("padding",     QString::number(cfg.padding));
    xml.writeAttribute("seqPadding",  QString::number(cfg.seqPadding));
    xml.writeAttribute("startNumber", QString::number(cfg.startNumber));
    xml.writeAttribute("seqNumber",   QString::number(cfg.seqNumber));
    xml.writeAttribute("resetOnSeqChange", cfg.resetOnSeqChange ? "1" : "0");
    xml.writeEndElement();
    for (const SequenceData &seq : model->sequences()) {
      xml.writeStartElement("sequence");
      xml.writeAttribute("uuid",  seq.uuid);
      xml.writeAttribute("label", seq.label);
      xml.writeAttribute("order", QString::number(seq.orderIndex));
      xml.writeEndElement();
    }
  }
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const Shot &shot = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",      QString::number(si));
    xml.writeAttribute("number",     shot.data.shotNumber);
    xml.writeAttribute("label",      shot.data.shotLabel);
    xml.writeAttribute("order",      QString::number(shot.data.orderIndex));
    xml.writeAttribute("sequenceId", shot.data.sequenceId);
    if (shot.data.transitionFrames > 0)
      xml.writeAttribute("transition", QString::number(shot.data.transitionFrames));
    for (int pi = 0; pi < (int)shot.data.panels.size(); pi++) {
      const PanelData &pd = shot.data.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      if (pd.cameraMoveType != PanelData::CamNone) {
        xml.writeAttribute("camMove",  QString::number((int)pd.cameraMoveType));
        xml.writeAttribute("camLabel", pd.cameraMoveLabel);
        xml.writeAttribute("camRenderFrame", QString::number(pd.camRenderFrame));
        xml.writeAttribute("camW", QString::number(pd.camW));
        xml.writeAttribute("camH", QString::number(pd.camH));
        // Store affines as space-separated doubles
        auto affToStr = [](const double a[6]) {
          return QString("%1 %2 %3 %4 %5 %6")
              .arg(a[0],0,'g',10).arg(a[1],0,'g',10).arg(a[2],0,'g',10)
              .arg(a[3],0,'g',10).arg(a[4],0,'g',10).arg(a[5],0,'g',10);
        };
        xml.writeAttribute("camA0", affToStr(pd.camA0));
        xml.writeAttribute("camA1", affToStr(pd.camA1));
      }
      if (pd.hasLight) {
        xml.writeAttribute("lightTail", QString("%1 %2")
            .arg(pd.lightTailX, 0, 'g', 6).arg(pd.lightTailY, 0, 'g', 6));
        xml.writeAttribute("lightTip", QString("%1 %2")
            .arg(pd.lightTipX, 0, 'g', 6).arg(pd.lightTipY, 0, 'g', 6));
        xml.writeAttribute("lightDepth", QString::number(pd.lightDepth, 'g', 4));
        xml.writeAttribute("lightSpread", QString::number(pd.lightSpread, 'g', 4));
        xml.writeAttribute("lightColor", pd.lightColor);
      }
      xml.writeTextElement("dialog", pd.dialog);
      xml.writeTextElement("action", pd.action);
      xml.writeTextElement("notes",  pd.notes);
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
  file.close();
}

void StoryboardPanel::loadZtoryc() {
  // Imported screenplay path read from this scene's .ztoryc.  Stays empty when
  // the scene has none — so opening a scene without a screenplay (or a brand
  // new scene) clears the Script panel instead of leaving a stale one loaded.
  QString scriptFromFile;
  m_loadingZtoryc = true;  // suppress scriptFileChanged→saveZtoryc during load
  QString path = ztoryPath();
  if (path.isEmpty()) {
    ZtoryModel::instance()->setScriptFile(scriptFromFile);
    m_loadingZtoryc = false;
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    ZtoryModel::instance()->setScriptFile(scriptFromFile);
    m_loadingZtoryc = false;
    return;
  }
  // File exists: reset project metadata so stale values from a previous scene
  // or creation flow don't bleed into this reload. They will be repopulated
  // below if the file contains a <project> element.
  ZtoryModel::instance()->setProduction("");
  ZtoryModel::instance()->setTitle("");
  // Start each scene's sequence list fresh so sequences never leak across
  // scenes. Old files (no <sequence>) leave it empty → renumberAll() recreates
  // a default sequence if needed.
  ZtoryModel::instance()->sequences().clear();

  QXmlStreamReader xml(&file);
  int si = -1, pi = -1;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("project")) {
        auto a = xml.attributes();
        ZtoryModel::instance()->setProduction(a.value("production").toString());
        ZtoryModel::instance()->setTitle(a.value("title").toString());
      }
      else if (xml.name() == QLatin1String("scriptFile")) {
        scriptFromFile = xml.readElementText();
      }
      else if (xml.name() == QLatin1String("numbering")) {
        ZtoryModel *model   = ZtoryModel::instance();
        NumberingConfig cfg = model->numberingConfig();
        auto a = xml.attributes();
        cfg.style = (NumberingConfig::Style)a.value("style").toInt();
        if (a.hasAttribute("shotPrefix"))  cfg.shotPrefix  = a.value("shotPrefix").toString();
        if (a.hasAttribute("seqPrefix"))   cfg.seqPrefix   = a.value("seqPrefix").toString();
        if (a.hasAttribute("panelPrefix")) cfg.panelPrefix = a.value("panelPrefix").toString();
        if (a.hasAttribute("step"))        cfg.step        = a.value("step").toInt();
        if (a.hasAttribute("padding"))     cfg.padding     = a.value("padding").toInt();
        if (a.hasAttribute("seqPadding"))  cfg.seqPadding  = a.value("seqPadding").toInt();
        if (a.hasAttribute("startNumber")) cfg.startNumber = a.value("startNumber").toInt();
        if (a.hasAttribute("seqNumber"))   cfg.seqNumber   = a.value("seqNumber").toInt();
        if (a.hasAttribute("resetOnSeqChange"))
          cfg.resetOnSeqChange = a.value("resetOnSeqChange").toInt() != 0;
        model->setNumberingConfig(cfg);
      }
      else if (xml.name() == QLatin1String("sequence")) {
        SequenceData seq;
        seq.uuid       = xml.attributes().value("uuid").toString();
        seq.label      = xml.attributes().value("label").toString();
        seq.orderIndex = xml.attributes().value("order").toInt();
        if (!seq.uuid.isEmpty())
          ZtoryModel::instance()->sequences().push_back(seq);
      }
      else if (xml.name() == QLatin1String("shot")) {
        si = xml.attributes().value("index").toInt();
        if (si < (int)m_shots.size()) {
          m_shots[si].data.shotNumber       = xml.attributes().value("number").toString();
          m_shots[si].data.shotLabel        = xml.attributes().value("label").toString();
          m_shots[si].data.orderIndex       = xml.attributes().value("order").toInt();
          m_shots[si].data.sequenceId       = xml.attributes().value("sequenceId").toString();
          m_shots[si].data.transitionFrames = xml.attributes().value("transition").toInt();
          // sequenceId is synced here (ZtoryModel may already have shots);
          // transitionFrames is synced later in refreshFromScene after syncShotPanels.
          if (si < ZtoryModel::instance()->shotCount())
            ZtoryModel::instance()->shot(si).sequenceId = m_shots[si].data.sequenceId;
          // Backward compat (v1-v2 files written by StoryboardPanel):
          // if shotLabel absent, use shotNumber
          if (m_shots[si].data.shotLabel.isEmpty())
            m_shots[si].data.shotLabel = m_shots[si].data.shotNumber;
        }
      }
      else if (xml.name() == QLatin1String("panel")) {
        pi = xml.attributes().value("index").toInt();
        if (si >= 0 && si < (int)m_shots.size() && pi >= 0) {
          // Aggiungi panel mancanti se necessario
          while (pi >= (int)m_shots[si].data.panels.size()) {
            PanelData pd;
            m_shots[si].data.panels.push_back(pd);
          }
          PanelData &pd = m_shots[si].data.panels[pi];
          pd.startFrame = xml.attributes().value("startFrame").toInt();
          pd.duration   = xml.attributes().value("duration").toInt();
          // Camera move overlay
          if (xml.attributes().hasAttribute("camMove")) {
            pd.cameraMoveType  = (PanelData::CameraMove)
                xml.attributes().value("camMove").toInt();
            pd.cameraMoveLabel = xml.attributes().value("camLabel").toString();
            pd.camRenderFrame  = xml.attributes().value("camRenderFrame").toInt();
            pd.camW = xml.attributes().value("camW").toDouble();
            pd.camH = xml.attributes().value("camH").toDouble();
            auto strToAff = [](const QString &s, double a[6]) {
              QStringList t = s.split(' ', Qt::SkipEmptyParts);
              for (int k = 0; k < 6 && k < t.size(); k++) a[k] = t[k].toDouble();
            };
            strToAff(xml.attributes().value("camA0").toString(), pd.camA0);
            strToAff(xml.attributes().value("camA1").toString(), pd.camA1);
            // Re-derive type/label/render-frame from the affines (single source
            // of truth) so scenes saved with an older classification self-heal.
            classifyCameraMove(pd);
          }
          // Light-direction gizmo
          if (xml.attributes().hasAttribute("lightTail")) {
            auto strToPt = [](const QString &s, double &x, double &y) {
              QStringList t = s.split(' ', Qt::SkipEmptyParts);
              if (t.size() >= 2) { x = t[0].toDouble(); y = t[1].toDouble(); }
            };
            strToPt(xml.attributes().value("lightTail").toString(),
                    pd.lightTailX, pd.lightTailY);
            strToPt(xml.attributes().value("lightTip").toString(),
                    pd.lightTipX, pd.lightTipY);
            pd.lightDepth = xml.attributes().value("lightDepth").toDouble();
            if (xml.attributes().hasAttribute("lightSpread"))
              pd.lightSpread = xml.attributes().value("lightSpread").toDouble();
            QString lc = xml.attributes().value("lightColor").toString();
            if (!lc.isEmpty()) pd.lightColor = lc;
            pd.hasLight = true;
          }
        }
      }
      else if (xml.name() == QLatin1String("dialog")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].dialog = t;
      }
      else if (xml.name() == QLatin1String("action")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].action = t;
      }
      else if (xml.name() == QLatin1String("notes")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].notes = t;
      }
    }
  }
  file.close();

  // ── SFH-explosion repair ─────────────────────────────────────────────────
  // The Stop-Frame-Hold bug (fixed in v0.2.x) wrote one PanelData entry per
  // animation frame for every resequenceXsheet() call.  This turned a 393-frame
  // shot into 393 single-frame panels in the .ztoryc file.  Opening such a scene
  // creates hundreds of PanelWidgets (each with 3 QTextEdit + 2 QSpinBox), which
  // can easily consume 20-50 GB of RAM through Qt layout machinery.
  //
  // Detection: any shot with > kSFHExplosionThreshold panels where ALL panels
  // have duration ≤ 1 frame.  That pattern cannot occur naturally (meaningful
  // storyboard panels are at least a few frames long).
  // Repair: collapse to a single panel that covers the shot's full duration.
  // The repair is also written back to disk so opening the scene is fast next time.
  // Detection thresholds for SFH-exploded shots:
  // kSFHPanelMin  — minimum panel count to even consider repair (safe floor)
  // kSFHAvgMaxDur — if avg panel duration ≤ this, the shot is SFH-exploded.
  //   SFH creates one panel per animation hold (typically 1-5 frames at 24fps).
  //   Legitimate storyboard panels are at least ~8-10 frames (1/3 of a second).
  //   The castelfiorentino scene's densest shot (SH190, 26 panels) has avg ≈ 7 f,
  //   so we use 5 as a conservative threshold: anything ≤ 5 f average is explosion.
  static constexpr int kSFHPanelMin  = 20;
  static constexpr int kSFHAvgMaxDur = 5;
  bool sfhRepaired = false;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    auto &panels = m_shots[i].data.panels;
    if ((int)panels.size() <= kSFHPanelMin) continue;
    int totalDurCheck = 0;
    for (const PanelData &pd : panels) totalDurCheck += pd.duration;
    int avgDur = (totalDurCheck > 0) ? totalDurCheck / (int)panels.size() : 0;
    if (avgDur > kSFHAvgMaxDur) continue;  // legitimate dense panels — skip
    // Compute total duration from cell data rather than trusting stale panel sums.
    int totalDur = 0;
    for (const PanelData &pd : panels) totalDur += pd.duration;
    // Preserve the dialog/action/notes of panel[0] (if non-empty).
    QString dialog = panels[0].dialog;
    QString action = panels[0].action;
    QString notes  = panels[0].notes;
    panels.clear();
    PanelData repaired;
    repaired.startFrame = 0;
    repaired.duration   = (totalDur > 0) ? totalDur : 1;
    repaired.dialog     = dialog;
    repaired.action     = action;
    repaired.notes      = notes;
    panels.push_back(repaired);
    sfhRepaired = true;
    qDebug() << "loadZtoryc: repaired SFH-exploded shot" << i
             << "collapsed" << (int)(totalDur) << "1-frame panels → 1 panel, dur=" << repaired.duration;
  }

  for (int i = 0; i < (int)m_shots.size(); i++) {
    Shot &shot = m_shots[i];
    // Rimuovi tutti i widget esistenti e ricostruisci da data
    for (PanelWidget *pw : shot.panels) { m_grid->removeWidget(pw); delete pw; }
    shot.panels.clear();
    for (int j = 0; j < (int)shot.data.panels.size(); j++) {
      addPanelWidget(i, j);
      shot.panels[j]->setDuration(shot.data.panels[j].duration);
      shot.panels[j]->setDialog(shot.data.panels[j].dialog);
      shot.panels[j]->setAction(shot.data.panels[j].action);
      shot.panels[j]->setNotes(shot.data.panels[j].notes);
    }
  }
  // Publish the screenplay for this scene — emits scriptFileChanged() so the
  // Script panel reloads it (or clears, when scriptFromFile is empty).
  ZtoryModel::instance()->setScriptFile(scriptFromFile);
  // Sync all panel data (shot labels + xsheet columns) into ZtoryModel so
  // the Shot Board thumbnail and other consumers see the correct shot.
  for (int i = 0; i < (int)m_shots.size(); i++)
    ZtoryModel::instance()->syncShotPanels(i, m_shots[i].data.panels,
                                           m_shots[i].data.shotLabel,
                                           m_shots[i].data.xsheetColumn);
  m_loadingZtoryc = false;
  // Persist the SFH-explosion repair so the scene loads cleanly next time.
  // m_currentZtoryPath is still empty here (set by refreshFromScene after we
  // return), so temporarily anchor it so saveZtoryc() can write.
  if (sfhRepaired) {
    m_currentZtoryPath = ztoryPath();
    saveZtoryc();
    m_currentZtoryPath.clear();  // refreshFromScene will set it authoritatively
  }
}

int StoryboardPanel::currentShotIndex() const {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return -1;
  ChildStack *cs = scene->getChildStack();
  if (!cs) return -1;
  int depth = cs->getAncestorCount();
  if (depth == 0) return -1;
  AncestorNode *node = cs->getAncestorInfo(depth - 1);
  if (!node) return -1;
  return node->m_col;
}


// ── Camera movement helpers ────────────────────────────────────────────────

// Fills PanelData camera fields from the sub-scene xsheet + scene camera.
// Classifies the camera move (type/label/render frame) purely from the affines
// already stored in pd (pd.camA0/camA1/camW/camH/startFrame/duration).  Kept
// separate from the xsheet read so it can also refresh stale persisted labels
// on load (.ztoryc) — the affines are the single source of truth.
static void classifyCameraMove(PanelData &pd) {
  int f0 = pd.startFrame;
  int f1 = qMax(f0, f0 + pd.duration - 1);

  TAffine a0(pd.camA0[0], pd.camA0[1], pd.camA0[2],
             pd.camA0[3], pd.camA0[4], pd.camA0[5]);
  TAffine a1(pd.camA1[0], pd.camA1[1], pd.camA1[2],
             pd.camA1[3], pd.camA1[4], pd.camA1[5]);

  // Scale: sqrt(a11^2 + a21^2)
  double s0 = std::sqrt(a0.a11*a0.a11 + a0.a21*a0.a21);
  double s1 = std::sqrt(a1.a11*a1.a11 + a1.a21*a1.a21);
  if (s0 < 1e-9) s0 = 1e-9;
  if (s1 < 1e-9) s1 = 1e-9;

  // Deltas: translation and scale change
  double dx   = a1.a13 - a0.a13;
  double dy   = a1.a23 - a0.a23;
  double dscl = (s1 - s0) / s0;

  // Normalise by camera world size at startFrame
  double camUnitW = (pd.camW > 0) ? pd.camW / s0 : 1.0;
  double camUnitH = (pd.camH > 0) ? pd.camH / s0 : 1.0;
  double normX = std::fabs(dx) / camUnitW;
  double normY = std::fabs(dy) / camUnitH;
  double normS = std::fabs(dscl);

  const double T = 0.03;  // 3% threshold for "significant" movement

  bool hasX = normX > T;
  bool hasY = normY > T;
  bool hasS = normS > T;

  if (!hasX && !hasY && !hasS) {
    pd.cameraMoveType  = PanelData::CamNone;
    pd.cameraMoveLabel = "";
    pd.camRenderFrame  = f0;
    return;
  }

  // Classify. The camera affine scale sets the WORLD aperture size
  // (world = cameraAff*cameraScale): scale UP → wider view → truck OUT;
  // scale DOWN → narrower view → truck IN.
  bool trkOut = hasS && dscl >  T;
  bool trkIn  = hasS && dscl < -T;
  bool pan    = hasX && normX > normY;
  bool tilt   = hasY && normY >= normX;

  if ((trkIn || trkOut) && !hasX && !hasY) {
    pd.cameraMoveType  = trkIn ? PanelData::CamTrkIn : PanelData::CamTrkOut;
    pd.cameraMoveLabel = trkIn ? "Trk In" : "Trk Out";
  } else if ((pan || tilt) && !hasS) {
    pd.cameraMoveType  = pan ? PanelData::CamPan : PanelData::CamTilt;
    pd.cameraMoveLabel = pan ? "Pan" : "Tilt";
  } else {
    pd.cameraMoveType  = PanelData::CamCombined;
    QStringList parts;
    if (trkIn)  parts << "Trk In";
    if (trkOut) parts << "Trk Out";
    if (pan)    parts << "Pan";
    if (tilt)   parts << "Tilt";
    pd.cameraMoveLabel = parts.join(" + ");
  }

  // Render at the WIDEST view so the narrower frame sits inside it when the
  // thumbnail is composed backed-out by applyCameraOverlay().  Use the relative
  // transform between the two camera affines (unit-consistent): rel maps
  // f1-local → f0-local; if f1 is smaller there, f0 is the wider view.
  TAffine rel       = a0.inv() * a1;
  double  relScale  = std::sqrt(std::fabs(rel.a11 * rel.a22 - rel.a12 * rel.a21));
  pd.camRenderFrame = (relScale <= 1.0) ? f0 : f1;
}

// Called after panel boundaries are confirmed from camera keyframes: reads the
// camera affines from the xsheet into pd, then classifies the move.
static void computeCameraMove(TXsheet *xsh, PanelData &pd,
                              int timelineDuration, ToonzScene *scene) {
  if (!xsh || !scene) return;

  int f0 = pd.startFrame;
  int f1 = qMax(f0, f0 + pd.duration - 1);

  TAffine a0 = xsh->getCameraAff(f0);
  TAffine a1 = xsh->getCameraAff(f1);

  pd.camA0[0]=a0.a11; pd.camA0[1]=a0.a12; pd.camA0[2]=a0.a13;
  pd.camA0[3]=a0.a21; pd.camA0[4]=a0.a22; pd.camA0[5]=a0.a23;
  pd.camA1[0]=a1.a11; pd.camA1[1]=a1.a12; pd.camA1[2]=a1.a13;
  pd.camA1[3]=a1.a21; pd.camA1[4]=a1.a22; pd.camA1[5]=a1.a23;

  TCamera *cam = scene->getCurrentCamera();
  if (cam) {
    const TDimensionD &sz = cam->getSize();
    pd.camW = sz.lx;
    pd.camH = sz.ly;
  }

  classifyCameraMove(pd);
}

// Draws the classic camera-move notation over a thumbnail that was already
// rendered "backed out" (renderXsheetFrameRegion) covering both camera frames:
// START and STOP rectangles joined by corner arrows (motion direction → STOP).
// Uses the SAME padded bbox as the render so the rects align with the content.
static void applyCameraOverlay(QPixmap &px, const PanelData &pd, int panelIdx,
                               bool showTypeLabel, double labelPxSize) {
  if (pd.cameraMoveType == PanelData::CamNone || px.isNull()) return;

  const int W = px.width();
  const int H = px.height();

  CamOverlayGeom g = computeCamOverlayGeom(pd, (double)W / H);
  if (!g.valid) return;

  // bbox aspect == W/H by construction → uniform fit, no centering offset.
  double fit  = W / (g.maxX - g.minX);
  auto toPx = [&](const TPointD &p) -> QPointF {        // Y flip → pixmap
    return QPointF((p.x - g.minX) * fit, (g.maxY - p.y) * fit);
  };

  QPointF renderPx[4], otherPx[4];
  for (int i = 0; i < 4; i++) {
    renderPx[i] = toPx(g.renderC[i]);
    otherPx[i]  = toPx(g.otherC[i]);
  }

  QPainter p(&px);
  p.setRenderHint(QPainter::Antialiasing, true);

  QPointF *startPx = g.renderedStart ? renderPx : otherPx;
  QPointF *stopPx  = g.renderedStart ? otherPx  : renderPx;

  QPen pen(QColor(220, 30, 30), 1.5);
  pen.setJoinStyle(Qt::MiterJoin);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);

  QPolygonF startPoly, stopPoly;
  for (int i = 0; i < 4; i++) { startPoly << startPx[i]; stopPoly << stopPx[i]; }
  p.drawPolygon(startPoly);
  p.drawPolygon(stopPoly);

  // Corner connector arrows: START corner → STOP corner, head at STOP.
  double ah = qMax(4.0, qMin(W, H) * 0.035);
  for (int i = 0; i < 4; i++) {
    QPointF a = startPx[i], b = stopPx[i];
    QLineF  ln(a, b);
    if (ln.length() < ah * 1.5) continue;
    p.drawLine(ln);
    double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
    QPointF h1(b.x() + ah * std::cos(ang + 2.618),
               b.y() + ah * std::sin(ang + 2.618));   // ±150°
    QPointF h2(b.x() + ah * std::cos(ang - 2.618),
               b.y() + ah * std::sin(ang - 2.618));
    p.drawLine(b, h1);
    p.drawLine(b, h2);
  }

  // Small red labels with a thin white halo for legibility. The caller can
  // request an explicit pixel size (PDF export uses ~6pt to match the text
  // fields); on screen we fall back to a small size relative to the thumbnail.
  QFont fnt("Arial", 0);
  fnt.setPixelSize(labelPxSize > 0 ? (int)labelPxSize : qMax(7, qMin(10, W / 28)));
  fnt.setBold(true);
  p.setFont(fnt);
  QFontMetrics fm(fnt);
  auto haloText = [&](double bx, double by, const QString &txt) {
    p.setPen(QColor(255, 255, 255));
    for (int ox = -1; ox <= 1; ox++)
      for (int oy = -1; oy <= 1; oy++)
        if (ox || oy) p.drawText(bx + ox, by + oy, txt);
    p.setPen(QColor(220, 30, 30));
    p.drawText(bx, by, txt);
  };

  // Camera-position letters at the top-left corner of each frame. Continuous
  // across the shot's panels: panel i is letter(i) → letter(i+1) (A→B, B→C …),
  // since one move's STOP is the next move's START. corner index 0 = aperture
  // top-left (loc[0] = (-hw, hh), Y up).
  QString startLetter = camLetter(panelIdx);
  QString stopLetter  = camLetter(panelIdx + 1);
  haloText(startPx[0].x() + 2, startPx[0].y() + fm.ascent() + 2, startLetter);
  double sx = stopPx[0].x() + 2, sy = stopPx[0].y() + fm.ascent() + 2;
  // Nudge the STOP letter down a line if its corner nearly coincides with START.
  if (qAbs(stopPx[0].y() - startPx[0].y()) < fm.height() &&
      qAbs(stopPx[0].x() - startPx[0].x()) < fm.horizontalAdvance(startLetter) + 4)
    sy += fm.height();
  haloText(sx, sy, stopLetter);

  // Movement type — bottom-left of the thumbnail (optional, toggled in Board).
  if (showTypeLabel && !pd.cameraMoveLabel.isEmpty())
    haloText(4, H - fm.descent() - 3, pd.cameraMoveLabel);

  p.end();
}

void StoryboardPanel::detectAndUpdatePanels(int shotIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;

  int mainCol = m_shots[shotIdx].data.xsheetColumn;
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  int timelineDuration;

  if (scene->getChildStack()->getAncestorCount() == 0) {
    // Called from main-xsheet context (e.g. showEvent after sub-scene closed).
    // Read the sub-scene xsheet directly from the shot's child level — do NOT
    // iterate main-xsheet cells, which change m_frameId on every row and would
    // create one panel per frame (thousands of bogus panels → hang).
    TXsheet *mainXsh = xsh;
    TXshColumn *mc = mainXsh ? mainXsh->getColumn(mainCol) : nullptr;
    if (!mc) return;
    int r0 = 0, r1 = 0;
    mc->getRange(r0, r1);
    timelineDuration = (r1 >= r0) ? r1 - r0 + 1 : 0;
    if (timelineDuration <= 0) return;
    xsh = nullptr;
    for (int r = r0; r <= r1 && !xsh; r++) {
      TXshCell cell = mainXsh->getCell(r, mainCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
        xsh = cell.m_level->getChildLevel()->getXsheet();
    }
    if (!xsh) return;
  } else {
    // Called while inside the sub-scene — existing logic.
    int depth = scene->getChildStack()->getAncestorCount();
    timelineDuration = xsh->getFrameCount();  // fallback
    AncestorNode *anc = scene->getChildStack()->getAncestorInfo(depth - 1);
    if (anc && anc->m_xsheet) {
      TXshColumn *mc = anc->m_xsheet->getColumn(mainCol);
      if (mc) {
        int r0 = 0, r1 = 0;
        mc->getRange(r0, r1);
        if (r1 >= r0) timelineDuration = r1 - r0 + 1;
      }
    }
  }

  int numCols = xsh->getColumnCount();
  int numFrames = xsh->getFrameCount();
  if (numFrames <= 0 || numCols <= 0) return;

  // Sound and SoundText (note) columns must not drive panel detection: the
  // cross-dissolve XD-out/XD-in note columns would otherwise create a spurious
  // panel boundary where they start/end.  Skip them in both passes below.
  auto isDrawingCol = [&](int c) -> bool {
    TXshColumn *col = xsh->getColumn(c);
    return col && !col->getSoundColumn() && !col->getSoundTextColumn();
  };

  // A single camera keyframe means a static framing — no actual movement,
  // so it must not create a panel boundary.  Only trigger on camera keyframes
  // when 2+ keyframes exist (i.e. there is an actual animated transition).
  int cameraKeyCount = 0;
  {
    TStageObject *cam = xsh->getStageObject(TStageObjectId::CameraId(0));
    if (cam) {
      for (int r = 0; r < numFrames; r++)
        if (cam->isKeyframe(r)) cameraKeyCount++;
    }
  }
  const bool useCameraKeys = (cameraKeyCount >= 2);

  // Collect keyframe change rows from sub-scene
  std::vector<int> allPanelFrames;
  allPanelFrames.push_back(0);
  for (int r = 1; r < numFrames; r++) {
    bool changed = false;
    for (int c = 0; c < numCols && !changed; c++) {
      if (!isDrawingCol(c)) continue;
      TXshCell prev = xsh->getCell(r - 1, c);
      TXshCell curr = xsh->getCell(r, c);
      if (prev.m_frameId != curr.m_frameId || prev.isEmpty() != curr.isEmpty())
        changed = true;
    }
    for (int c = 0; c < numCols && !changed; c++) {
      if (!isDrawingCol(c)) continue;
      TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
      if (obj && obj->isKeyframe(r)) changed = true;
    }
    if (!changed && useCameraKeys) {
      TStageObject *cam = xsh->getStageObject(TStageObjectId::CameraId(0));
      if (cam && cam->isKeyframe(r)) changed = true;
    }
    if (changed) allPanelFrames.push_back(r);
  }

  // Keep only panels whose start frame falls within the timeline-visible range.
  // Panels beyond timelineDuration are hidden from the Board.
  std::vector<int> panelFrames;
  for (int f : allPanelFrames)
    if (f < timelineDuration) panelFrames.push_back(f);
  if (panelFrames.empty()) panelFrames.push_back(0);

  Shot &shot = m_shots[shotIdx];
  int newPanelCount = (int)panelFrames.size();

  if (newPanelCount == (int)shot.data.panels.size()) {
    // Count unchanged — update durations only (timeline may have been resized)
    for (int i = 0; i < newPanelCount; i++) {
      shot.data.panels[i].startFrame = panelFrames[i];
      shot.data.panels[i].duration   = (i+1 < newPanelCount)
                                       ? panelFrames[i+1] - panelFrames[i]
                                       : timelineDuration - panelFrames[i];
      if (i < (int)shot.panels.size())
        shot.panels[i]->setDuration(shot.data.panels[i].duration);
      // Re-compute camera move data (duration/frame may have changed)
      if (useCameraKeys)
        computeCameraMove(xsh, shot.data.panels[i], timelineDuration, scene);
    }
    for (PanelWidget *pw : shot.panels)
      pw->setTotalDuration(timelineDuration);
    ZtoryModel::instance()->syncShotPanels(shotIdx, shot.data.panels,
                                           shot.data.shotLabel,
                                           shot.data.xsheetColumn);
    saveZtoryc();
    return;
  }

  // Panel count changed — rebuild panel data and widgets
  while ((int)shot.data.panels.size() < newPanelCount) {
    PanelData pd;
    shot.data.panels.push_back(pd);
  }
  while ((int)shot.data.panels.size() > newPanelCount)
    shot.data.panels.pop_back();
  for (int i = 0; i < newPanelCount; i++) {
    shot.data.panels[i].startFrame = panelFrames[i];
    shot.data.panels[i].duration   = (i+1 < newPanelCount)
                                     ? panelFrames[i+1] - panelFrames[i]
                                     : timelineDuration - panelFrames[i];
    if (useCameraKeys)
      computeCameraMove(xsh, shot.data.panels[i], timelineDuration, scene);
  }
  for (PanelWidget *pw : shot.panels) { m_grid->removeWidget(pw); delete pw; }
  shot.panels.clear();
  for (int pi = 0; pi < (int)shot.data.panels.size(); pi++)
    addPanelWidget(shotIdx, pi);
  renumberAll();
  rebuildGrid();
  ZtoryModel::instance()->syncShotPanels(shotIdx, shot.data.panels,
                                         shot.data.shotLabel,
                                         shot.data.xsheetColumn);
  saveZtoryc();
}

void StoryboardPanel::assignKeepNumbers(int insertAt) {
  int total = (int)m_shots.size();
  // When adding the very first shot there are no neighbours to inherit a label
  // from. Return early — renumberAll() will assign a proper label afterwards.
  // Without this guard: insertAt=0, total=1 → m_shots[insertAt-1] = m_shots[-1]
  // → out-of-bounds crash on Windows (UB on all platforms).
  if (total <= 1) return;
  // Assegna numeri fissi agli shot senza shotNumber basandosi sulla posizione originale
  // Gli shot prima di insertAt mantengono il loro numero, quelli dopo anche
  for (int j = 0; j < total; j++) {
    if (j != insertAt && m_shots[j].data.shotNumber.isEmpty())
      m_shots[j].data.shotNumber = QString("%1").arg(j + 1, 2, 10, QChar(48));
  }
  // Se in coda - stessa logica del caso "in mezzo"
  if (insertAt >= total - 1) {
    QString baseNum = m_shots[insertAt - 1].data.shotNumber;
    int i = baseNum.length() - 1;
    while (i >= 0 && baseNum[i].isLetter()) i--;
    QString numPart = baseNum.left(i + 1);
    // Controlla se il precedente ha gia lettere - in quel caso incrementa numero
    bool prevHasLetter = (i < baseNum.length() - 1);
    if (prevHasLetter) {
      // precedente e es. "05A" - nuovo e "05B"
      QChar letter = 'A';
      for (int j = 0; j < total - 1; j++) {
        QString n = m_shots[j].data.shotNumber;
        if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
          QChar c = n[numPart.length()];
          if (c.toLatin1() >= letter.toLatin1())
            letter = QChar(c.toLatin1() + 1);
        }
      }
      m_shots[insertAt].data.shotNumber = numPart + letter;
    } else {
      // precedente e es. "05" - nuovo e "05A"
      QChar letter = 'A';
      for (int j = 0; j < total - 1; j++) {
        QString n = m_shots[j].data.shotNumber;
        if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
          QChar c = n[numPart.length()];
          if (c.toLatin1() >= letter.toLatin1())
            letter = QChar(c.toLatin1() + 1);
        }
      }
      m_shots[insertAt].data.shotNumber = numPart + letter;
    }
    return;
  }
  // Se in testa
  if (insertAt == 0) {
    QString nextNum = m_shots[1].data.shotNumber;
    int i = nextNum.length() - 1;
    while (i >= 0 && nextNum[i].isLetter()) i--;
    m_shots[0].data.shotNumber = nextNum.left(i + 1) + "A";
    return;
  }
  // In mezzo - usa numero del precedente + lettera
  QString baseNum = m_shots[insertAt - 1].data.shotNumber;
  int i = baseNum.length() - 1;
  while (i >= 0 && baseNum[i].isLetter()) i--;
  QString numPart = baseNum.left(i + 1);
  QChar letter = 'A';
  for (int j = 0; j < total; j++) {
    if (j == insertAt) continue;
    QString n = m_shots[j].data.shotNumber;
    if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
      QChar c = n[numPart.length()];
      if (c.toLatin1() >= letter.toLatin1())
        letter = QChar(c.toLatin1() + 1);
    }
  }
  m_shots[insertAt].data.shotNumber = numPart + letter;
}

void StoryboardPanel::onModelResequenced() {
  // Same as onXsheetChanged but without the ancestor-count guard, so it works
  // even when the user is inside a sub-scene while the animatic panel resequences.
  // If shot count changed (e.g. Animatic deleted/merged shots), do a full rebuild.
  //
  // IMPORTANT: use the actual xsheet child-column count, NOT ZtoryModel::m_shots.size().
  // ZtoryModel::m_shots can be stale after copy/paste/clone sequences that bypass
  // ZtoryModel::addShot()/removeShot(). Using it as reference caused double-removal:
  // refreshFromScene() fired here AND shotRemovedAt() fired afterward → Board lost
  // one extra shot after a cross-panel merge.
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()
                   ? TApp::instance()->getCurrentScene()->getScene()->getChildStack()->getTopXsheet()
                   : nullptr;
  if (!xsh) return;
  int xshShotCount = 0;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        xshShotCount++;
        break;
      }
    }
  }
  if (xshShotCount != (int)m_shots.size()) {
    qWarning("[ZTORY] onModelResequenced: xshShotCount=%d != shots=%d -> full rebuild",
             xshShotCount, (int)m_shots.size());
    refreshFromScene();
    return;
  }
  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int r0 = 0, r1 = 0;
    // ignoreLastStop=true: skip the trailing Stop Frame Hold placed by
    // ZtoryModel::resequenceXsheet() so the duration shown in the Board
    // matches the shot's actual animatic length (not inflated by +1).
    column->getRange(r0, r1, /*ignoreLastStop=*/true);
    int duration = r1 - r0 + 1;
    if (!m_shots[si].data.panels.empty()) {
      m_shots[si].data.panels[0].duration = duration;
      if (!m_shots[si].panels.empty())
        m_shots[si].panels[0]->setDuration(duration);
    }
  }
}

void StoryboardPanel::onMergeShots() {
  if (!ZtoryModel::assertMainXsheet(true)) return;
  auto before = captureSnapshot();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Build the set of xsheet columns to merge.
  // Prefer own selection (>= 2 shots); fall back to shared selection from Animatic.
  std::vector<int> sortedCols;
  std::set<int> localIndices = m_selectedIndices;
  if (localIndices.size() < 2 && m_selectedShotIndex >= 0)
    localIndices.insert(m_selectedShotIndex);
  if (localIndices.size() >= 2) {
    for (int bi : localIndices)
      if (bi >= 0 && bi < (int)m_shots.size())
        sortedCols.push_back(m_shots[bi].data.xsheetColumn);
  } else {
    // Fall back to shared selection (last written by Animatic or Board).
    const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
    sortedCols.assign(shared.begin(), shared.end());
  }
  if (sortedCols.size() < 2) return;
  std::sort(sortedCols.begin(), sortedCols.end(), [&](int a, int b){
    int r0a = 0, r1a = 0, r0b = 0, r1b = 0;
    if (xsh->getColumn(a)) xsh->getColumn(a)->getRange(r0a, r1a);
    if (xsh->getColumn(b)) xsh->getColumn(b)->getRange(r0b, r1b);
    return r0a < r0b;
  });

  int dstCol = sortedCols[0];
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  dstColumn->getRange(dstR0, dstR1);

  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, dstCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  int appendAt    = dstR1 + 1;
  int dstDuration = dstR1 - dstR0 + 1;
  int lastFrameNum = dstDuration;

  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    srcColumn->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    TXshChildLevel *srcCl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, srcCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        srcCl = cell.m_level->getChildLevel();
        break;
      }
    }
    mergeChildXsheetContent(dstCl, srcCl, lastFrameNum, duration);
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  // Delete source columns in reverse order to keep lower indices stable
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--) {
    std::set<int> cs; cs.insert(sortedCols[i]);
    ColumnCmd::deleteColumns(cs, false, true);  // withoutUndo=true
  }

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();

  m_selectedIndices.clear();
  m_selectedShotIndex = -1;

  m_updating = true;
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    emit ZtoryModel::instance()->shotRemovedAt(sortedCols[i]);
  m_updating = false;

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Merge Shots"), std::move(before), std::move(after)));
}

void StoryboardPanel::onShotInserted(int col) {
  if (m_updating) return;  // skip if THIS Board emitted the signal (already updated)
  // Called when the animatic razor (or any external op) inserts a new shot
  // column at position 'col'.  We insert a corresponding Shot entry at that
  // position, then renumber and save — bypassing loadZtoryc() which would
  // map by stale index and assign wrong numbers to existing shots.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  if (col < 0 || col > (int)m_shots.size()) return;

  // Build new Shot from xsheet column
  Shot shot;
  shot.data.xsheetColumn = col;
  TXshColumn *column = xsh->getColumn(col);
  if (column) {
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    PanelData pd;
    pd.duration = (r1 >= r0) ? (r1 - r0 + 1) : 24;
    shot.data.panels.push_back(pd);
  } else {
    PanelData pd; pd.duration = 24;
    shot.data.panels.push_back(pd);
  }

  // Update xsheetColumn for all shots at col or later (they shifted right)
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].data.xsheetColumn >= col)
      m_shots[i].data.xsheetColumn++;

  m_shots.insert(m_shots.begin() + col, shot);
  // Copy shotLabel + orderIndex from ZtoryModel (generateShotLabel was already called there)
  ZtoryModel *model = ZtoryModel::instance();
  if (col < model->shotCount() && !model->shot(col).shotLabel.isEmpty()) {
    m_shots[col].data.shotLabel  = model->shot(col).shotLabel;
    m_shots[col].data.orderIndex = model->shot(col).orderIndex;
    m_shots[col].data.shotNumber = m_shots[col].data.shotLabel;
  }
  addPanelWidget(col, 0);

  renumberAll();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onShotRemovedAt(int col) {
  if (m_updating) return;  // skip if THIS Board emitted the signal (already updated)
  int si = -1;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    if (m_shots[i].data.xsheetColumn == col) { si = i; break; }
  }
  if (si < 0) {
    // Shot !found — Board xsheetColumn tracking is desynced (e.g. after
    // a previous cut/merge left counts off by one). Rebuild from xsheet.
    refreshFromScene();
    return;
  }

  for (PanelWidget *pw : m_shots[si].panels) {
    m_grid->removeWidget(pw);
    delete pw;
  }
  m_shots.erase(m_shots.begin() + si);

  // Columns above 'col' shifted down by 1 when the column was deleted
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].data.xsheetColumn > col)
      m_shots[i].data.xsheetColumn--;

  renumberAll();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onXsheetChanged() {
  // Update T: (timeline duration) for all shots from main xsheet column range.
  // D: (partial) is only updated for single-panel shots; multi-panel partials
  // are owned by detectAndUpdatePanels and must !be overwritten here.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->getChildStack()->getAncestorCount() != 0) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    // Always update T: display to reflect actual timeline duration
    for (PanelWidget *pw : m_shots[si].panels)
      pw->setTotalDuration(duration);
    // For single-panel shots: D: == T: (partial = total)
    if (m_shots[si].data.panels.size() == 1) {
      m_shots[si].data.panels[0].duration = duration;
      if (!m_shots[si].panels.empty())
        m_shots[si].panels[0]->setDuration(duration);
    }
    // For multi-panel shots: D: stays as computed by detectAndUpdatePanels
  }
}

void StoryboardPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  if (m_shots.empty()) {
    refreshFromScene();
  } else {
    // If a shot was opened while the Board was hidden (m_dirtyShotCol >= 0),
    // force-refresh it: detect panel changes (handles deleted drawings/panels)
    // and invalidate the thumbnail so updateVisiblePreviews() re-renders it.
    if (m_dirtyShotCol >= 0) {
      for (int si = 0; si < (int)m_shots.size(); si++) {
        if (m_shots[si].data.xsheetColumn == m_dirtyShotCol) {
          detectAndUpdatePanels(si);  // safe: now handles main-xsheet context
          for (PanelWidget *pw : m_shots[si].panels)
            pw->setPreviewPixmap(QPixmap());
          break;
        }
      }
      m_dirtyShotCol = -1;
    }
    QTimer::singleShot(200, this, &StoryboardPanel::onRefreshPreviews);
  }
}

void StoryboardPanel::resizeEvent(QResizeEvent *e) {
  TPanel::resizeEvent(e);
  // Debounce: recompute column widths 150 ms after the last resize event so we
  // don't thrash during live window dragging.
  if (!m_resizeTimer) {
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true);
    connect(m_resizeTimer, &QTimer::timeout, this, [this]() {
      if (m_shots.empty()) return;
      int available = m_scrollArea->viewport()->width() - 8 * (m_columnsPerRow + 1);
      int colW = qMax(150, available / m_columnsPerRow);
      for (Shot &shot : m_shots)
        for (PanelWidget *pw : shot.panels)
          pw->setFixedWidth(colW);
      // PanelWidget::resizeEvent fires automatically on each setFixedWidth call:
      //   → rescalePreview() updates display immediately
      //   → previewRerenderNeeded emitted if pixmap resolution is insufficient
      m_container->adjustSize();
    });
  }
  m_resizeTimer->start(150);
}

void StoryboardPanel::refreshFromScene() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Diagnostic for the undo-wipe bug (task 48): if the Board empties, the
  // console shows who rebuilt it and what the xsheet looked like.
  qWarning("[ZTORY] refreshFromScene: shots(before)=%d ancestors=%d",
           (int)m_shots.size(), scene->getChildStack()->getAncestorCount());
  // Sync fps from scene output settings — keeps timecodes correct when the
  // user has a non-24 frame rate (e.g. 25, 30).
  {
    int sceneFps = (int)std::round(
        scene->getProperties()->getOutputProperties()->getFrameRate());
    if (sceneFps > 0) {
      m_fps = sceneFps;
      ZtoryModel::instance()->setFps(sceneFps);
    }
  }
  // Note: production/title are NOT reset here. loadZtoryc() clears them only
  // when it finds an existing .ztoryc file, so that values set during scene
  // creation (startup popup) survive until the first saveZtoryc() anchors them.
  clearShots();
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int numCols = xsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshChildLevel *cl = nullptr;
    int duration = 0;
    for (int r = 0; r < xsh->getFrameCount(); r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        if (!cl) cl = cell.m_level->getChildLevel();
        duration++;
      } else if (duration > 0) break;
    }
    if (!cl) continue;
    Shot shot;
    shot.data.xsheetColumn = col;
    shot.data.shotNumber = QString("%1").arg((int)m_shots.size()+1, 2, 10, QChar(48));
    PanelData pd;
    pd.startFrame = 0;
    pd.duration = duration;
    shot.data.panels.push_back(pd);
    m_shots.push_back(shot);
    addPanelWidget((int)m_shots.size()-1, 0);
  }
  loadZtoryc();
  // Rebuild panel widgets to match the panel data loaded from .ztoryc.
  // refreshFromScene creates one placeholder widget per shot; loadZtoryc may
  // have added more panels to shot.data.panels, so we recreate all widgets.
  for (int si = 0; si < (int)m_shots.size(); si++) {
    for (PanelWidget *pw : m_shots[si].panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
    m_shots[si].panels.clear();
    for (int pi = 0; pi < (int)m_shots[si].data.panels.size(); pi++) {
      addPanelWidget(si, pi);
      // Restore text loaded by loadZtoryc() — addPanelWidget() creates a blank
      // widget so we must repopulate from data.panels which already has the text.
      m_shots[si].panels[pi]->setDuration(m_shots[si].data.panels[pi].duration);
      m_shots[si].panels[pi]->setDialog(m_shots[si].data.panels[pi].dialog);
      m_shots[si].panels[pi]->setAction(m_shots[si].data.panels[pi].action);
      m_shots[si].panels[pi]->setNotes(m_shots[si].data.panels[pi].notes);
    }
  }
  renumberAll();
  rebuildGrid();
  // Re-sync labels + xsheet columns AFTER renumberAll so ZtoryModel always
  // has the final (post-renumber) shot labels and correct column indices.
  // The earlier syncShotPanels in loadZtoryc may have had empty labels for
  // scenes without a .ztoryc file; this call makes them authoritative.
  for (int i = 0; i < (int)m_shots.size(); i++) {
    ZtoryModel::instance()->syncShotPanels(i, m_shots[i].data.panels,
                                           m_shots[i].data.shotLabel,
                                           m_shots[i].data.xsheetColumn);
    // Sync transitionFrames here (after model is fully populated) rather than
    // inside loadZtoryc() where ZtoryModel::shotCount() may still be 0.
    if (i < ZtoryModel::instance()->shotCount())
      ZtoryModel::instance()->shot(i).transitionFrames =
          m_shots[i].data.transitionFrames;
  }
  // Thumbnails are NOT rendered automatically on scene load.
  // renderXsheetFrame() is synchronous and can take several seconds per panel on
  // scenes with complex sub-xsheets (many raster layers, high resolution).
  // Auto-rendering even the first few visible panels would freeze the UI for
  // tens of seconds, making the scene appear to "load slowly" even though the
  // xsheet data itself is ready instantly.
  // Thumbnails are rendered lazily via two paths:
  //   1. Scroll stops → 250 ms debounce → updateVisiblePreviews() (skip existing)
  //   2. User clicks the Refresh Previews toolbar button → onRefreshPreviews()

  // Anchor the save path to this scene NOW that m_shots is fully populated.
  // Any saveZtoryc() that fired while m_currentZtoryPath was empty (during
  // clearShots → addShots → loadZtoryc) was correctly suppressed; from this
  // point on saves will target exactly this scene's file.
  m_currentZtoryPath = ztoryPath();

  // If production/title were set during scene creation (startup popup) but
  // not yet saved (new scene had no .ztoryc), persist them now.
  ZtoryModel *zm = ZtoryModel::instance();
  if (!zm->production().isEmpty() || !zm->title().isEmpty())
    saveZtoryc();
}

// ── qApp event filter: intercept keyboard shortcuts for the Board ────────────
// Two-phase interception:
//   1. QEvent::ShortcutOverride  — "claim" the key sequence so that
//      CommandManager's ApplicationShortcut QActions (MI_Copy etc.) never fire.
//      Returning true after accept() tells Qt to skip shortcut dispatch and
//      proceed to KeyPress on the focused widget.
//   2. QEvent::KeyPress — actually dispatch the shot operation.
// Without phase 1, Cmd+C/X/V/D are stolen by CommandManager before KeyPress
// arrives, so only Delete (which lacks a global binding in most contexts) worked.
bool StoryboardPanel::eventFilter(QObject *obj, QEvent *e) {
  const QEvent::Type t = e->type();

  // ── Toolbar button hints ──
  if (t == QEvent::Enter || t == QEvent::Leave) {
    QVariant hintV = obj->property("ztoryHint");
    if (hintV.isValid()) {
      if (t == QEvent::Enter)
        TApp::instance()->showZtoryHint(hintV.toString());
      else
        TApp::instance()->clearZtoryHint();
      return false;
    }
  }

  // ── Text field undo: capture snapshot before editing, push on focus out ──
  if (t == QEvent::FocusIn || t == QEvent::FocusOut) {
    if (qobject_cast<QTextEdit *>(obj)) {
      if (t == QEvent::FocusIn && !m_textEditing) {
        m_textEditing    = true;
        m_textUndoBefore = captureSnapshot();
      } else if (t == QEvent::FocusOut && m_textEditing) {
        commitTextUndo();
      }
    }
    return false;
  }

  if (t != QEvent::ShortcutOverride && t != QEvent::KeyPress) return false;

  // Check if focus is inside this panel's subtree
  QWidget *fw = QApplication::focusWidget();
  bool inPanel = false;
  for (QWidget *w = fw; w; w = w->parentWidget())
    if (w == this) { inPanel = true; break; }
  if (!inPanel) return false;

  // CRITICAL: if the focused widget is a text editor, let it handle every key
  // itself.  Without this, typing Backspace while editing Dialogue/Action/
  // Notes was intercepted as "delete shot" — the whole shot got wiped on a
  // routine backspace.  Same protection for Cmd+C/X/V (native text copy/cut/
  // paste should not become shot operations either).
  if (qobject_cast<QTextEdit *>(fw)   ||
      qobject_cast<QPlainTextEdit *>(fw) ||
      qobject_cast<QLineEdit *>(fw))
    return false;

  // Require at least one shot selected
  if (m_selectedShotIndex < 0 && m_selectedIndices.empty()) return false;

  QKeyEvent *ke  = static_cast<QKeyEvent *>(e);
  const bool cmd   = ke->modifiers() & Qt::ControlModifier;
  const bool shift = ke->modifiers() & Qt::ShiftModifier;
  const bool noMod = ke->modifiers() == Qt::NoModifier;

  // Identify keys we own
  const bool isDelete = noMod && (ke->key() == Qt::Key_Delete ||
                                  ke->key() == Qt::Key_Backspace);
  const bool isCopy   = cmd && !shift && ke->key() == Qt::Key_C;
  const bool isCut    = cmd && !shift && ke->key() == Qt::Key_X;
  const bool isPaste  = cmd && !shift && ke->key() == Qt::Key_V;
  const bool isClone  = cmd && !shift && ke->key() == Qt::Key_D;

  if (!isDelete && !isCopy && !isCut && !isPaste && !isClone) return false;

  // Phase 1 — ShortcutOverride: claim the key so CommandManager doesn't fire.
  // We accept and return true; Qt will then skip shortcut dispatch and send
  // KeyPress to the focused widget, which our Phase 2 will intercept.
  if (t == QEvent::ShortcutOverride) {
    ke->accept();
    return true;
  }

  // Phase 2 — KeyPress: dispatch the shot operation.
  if (isCopy)   onCopyShot();
  else if (isCut)    onCutShot();
  else if (isPaste)  onPasteShot();
  else if (isClone)  onCloneShot();
  else if (isDelete) onDeleteShot();

  ke->accept();
  return true;
}

void StoryboardPanel::keyPressEvent(QKeyEvent *e) {
  // L toggles light-direction arrow visibility (task 40 FASE 3). Text fields
  // never propagate plain keys here, so typing "l" in notes is unaffected.
  if (e->key() == Qt::Key_L && e->modifiers() == Qt::NoModifier &&
      m_lightShowButton) {
    m_lightShowButton->toggle();
    e->accept();
    return;
  }
  TPanel::keyPressEvent(e);
}

void StoryboardPanel::mouseDoubleClickEvent(QMouseEvent *e) {
  // Double-click on the background (!on a shot card) closes the sub-scene.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  if (scene->getChildStack()->getAncestorCount() > 0) {
    CommandManager::instance()->execute("MI_CloseChild");
    return;
  }
  TPanel::mouseDoubleClickEvent(e);
}

void StoryboardPanel::onCopyShot() {
  // Auto-return to main xsheet before operating
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = false;
    e.srcColumn = idx;
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board copies.
  {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    std::vector<ZtoryClipEntry> shared;
    for (int idx : sorted) {
      if (idx < 0 || idx >= (int)m_shots.size()) continue;
      ZtoryClipEntry ze;
      ze.srcCol   = m_shots[idx].data.xsheetColumn;
      ze.duration = m_shots[idx].data.panels.empty()
                    ? 24 : m_shots[idx].data.panels[0].duration;
      ze.isCut    = false;
      ze.isClone  = false;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
}

void StoryboardPanel::onCutShot() {
  // Immediate cut: save metadata + level reference, then delete shots immediately.
  // cutLevel keeps the TXshChildLevel alive after ColumnCmd::deleteColumn so that
  // onPasteShot() can re-insert the same sub-scene (drawings preserved).
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = false;
    e.srcColumn = -1;   // original will be deleted immediately
    e.isCut     = true;
    // Grab the TXshLevel before deletion to keep the sub-scene alive
    if (xsh) {
      int col = m_shots[idx].data.xsheetColumn;
      TXshColumn *xshCol = xsh->getColumn(col);
      TXshLevelColumn *lc = xshCol ? xshCol->getLevelColumn() : nullptr;
      if (lc) {
        int r0 = 0, r1 = 0;
        lc->getRange(r0, r1);
        TXshCell cell = lc->getCell(r0);
        if (!cell.isEmpty()) e.cutLevel = cell.m_level;
      }
    }
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board cuts.
  {
    std::vector<ZtoryClipEntry> shared;
    for (const ClipboardEntry &ce : m_clipboard) {
      ZtoryClipEntry ze;
      ze.srcCol   = -1;  // immediate cut: original will be gone
      ze.duration = ce.data.panels.empty() ? 24 : ce.data.panels[0].duration;
      ze.isCut    = true;
      ze.isClone  = false;
      ze.cutLevel = ce.cutLevel;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
  onDeleteShot();  // immediately remove from board and xsheet
}

void StoryboardPanel::onCloneShot() {
  // Auto-return to main xsheet before operating
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = true;
    e.srcColumn = idx;
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board clones.
  {
    std::vector<ZtoryClipEntry> shared;
    for (int idx : sorted) {
      if (idx < 0 || idx >= (int)m_shots.size()) continue;
      ZtoryClipEntry ze;
      ze.srcCol   = m_shots[idx].data.xsheetColumn;
      ze.duration = m_shots[idx].data.panels.empty()
                    ? 24 : m_shots[idx].data.panels[0].duration;
      ze.isCut    = false;
      ze.isClone  = true;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
}

// Force a freshly-created sub-scene's camera(s) to match the MAIN xsheet's
// camera (resolution + size in inches). Without this, new shots can end up with
// a non-standard camera (e.g. 12.5×6") that no longer matches the main 16×9 →
// framing mismatch in the monitor (root cause of BUG-CAMERA). Only safe for
// BRAND-NEW empty sub-scenes — never call on a clone with camera animation, as
// changing the size would reframe its keyframes destructively.
static void syncChildCameraToMain(TXsheet *parentXsh, TXshChildLevel *cl) {
  if (!parentXsh || !cl) return;
  TXsheet *childXsh = cl->getXsheet();
  if (!childXsh) return;
  TStageObjectTree *parentTree = parentXsh->getStageObjectTree();
  TStageObjectTree *childTree  = childXsh->getStageObjectTree();
  int tmpCamId = 0;
  for (int cam = 0; cam < parentTree->getCameraCount();) {
    TStageObject *parentCamera =
        parentTree->getStageObject(TStageObjectId::CameraId(tmpCamId), false);
    if (!parentCamera) { tmpCamId++; continue; }
    if (parentCamera->getCamera()) {
      TStageObject *childObj =
          childTree->getStageObject(TStageObjectId::CameraId(tmpCamId));
      TCamera *childCamera = childObj ? childObj->getCamera() : nullptr;
      if (childCamera) {
        childCamera->setRes(parentCamera->getCamera()->getRes());
        childCamera->setSize(parentCamera->getCamera()->getSize());
      }
    }
    tmpCamId++; cam++;
  }
  childTree->setCurrentCameraId(parentTree->getCurrentCameraId());
}

static void cloneChildToPosition(int srcCol, int dstCol) {
  TApp *app          = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet *xsh       = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(srcCol);
  if (!column) return;
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  if (!lcolumn) return;
  int r0 = 0, r1 = -1;
  lcolumn->getRange(r0, r1);
  if (r0 > r1) return;
  TXshCell cell = lcolumn->getCell(r0);
  if (cell.isEmpty()) return;
  TXshChildLevel *childLevel = cell.m_level->getChildLevel();
  if (!childLevel) return;
  TXsheet *childXsh = childLevel->getXsheet();

  // Inserisci colonna vuota alla posizione target
  xsh->insertColumn(dstCol);

  // Crea nuovo child level clone
  ChildStack *childStack = scene->getChildStack();
  TXshChildLevel *newChildLevel = childStack->createChild(0, dstCol);
  TXsheet *newChildXsh = newChildLevel->getXsheet();

  // Copia contenuto colonne.  NON instradiamo la camera tramite
  // StageObjectsData: ogni child xsheet creato da createChild() ha già una
  // Camera 0 di default, quindi restoreCamera() la trova "occupata" e crea una
  // Camera 1 FANTASMA con i keyframe, lasciando la Camera 0 (quella usata per il
  // rendering) senza animazione — ecco perché lo shot clonato perdeva le chiavi
  // di camera.  La camera la copiamo a mano sotto, direttamente Camera 0→Camera 0.
  std::set<int> indices;
  for (int i = 0; i < childXsh->getColumnCount(); i++) indices.insert(i);
  std::vector<TStageObjectId> ids;
  for (int i : indices) ids.push_back(TStageObjectId::ColumnId(i));
  StageObjectsData *data = new StageObjectsData();
  data->storeObjects(ids, childXsh, 0);
  data->storeColumnFxs(indices, childXsh, 0);
  std::list<int> restoredSplineIds;
  QMap<TStageObjectId, TStageObjectId> idTable;
  QMap<TFx *, TFx *> fxTable;
  data->restoreObjects(indices, restoredSplineIds, newChildXsh,
                       StageObjectsData::eDoClone, idTable, fxTable);
  delete data;

  // Stage objects non-colonna (camera, pegbar): copia params (keyframe/
  // animazione) sull'oggetto con lo STESSO id già esistente nel clone.  Le
  // colonne sono già state ricreate da restoreObjects(); qui copiamo camera e
  // pegbar — che altrimenti andrebbero persi (la camera finirebbe su una Camera
  // fantasma via restoreCamera; i pegbar non verrebbero animati).  Mirror della
  // logica stock cloneXsheetTStageObjectTree() (in namespace anonimo, non
  // richiamabile da qui).
  {
    TStageObjectTree *srcTree = childXsh->getStageObjectTree();
    for (int i = 0; i < srcTree->getStageObjectCount(); i++) {
      TStageObject *srcObj = srcTree->getStageObject(i);
      TStageObjectId id    = srcObj->getId();
      if (id.isColumn()) continue;  // colonne già gestite da restoreObjects
      TStageObject *dstObj = newChildXsh->getStageObject(id);
      if (!dstObj) continue;
      if (id.isCamera()) *(dstObj->getCamera()) = *(srcObj->getCamera());
      TStageObjectParams *p = srcObj->getParams();
      dstObj->assignParams(p, /*doParametersClone=*/true);
      delete p;
      dstObj->setParent(childXsh->getStageObjectParent(id));
    }
  }

  newChildXsh->getFxDag()->getXsheetFx()->getAttributes()->setDagNodePos(
      childXsh->getFxDag()->getXsheetFx()->getAttributes()->getDagNodePos());
  newChildXsh->updateFrameCount();

  // Rimuovi cella creata da createChild e copia celle originali
  xsh->removeCells(0, dstCol);
  for (int r = r0; r <= r1; r++) {
    TXshCell c = lcolumn->getCell(r);
    if (c.isEmpty()) continue;
    c.m_level = newChildLevel;
    xsh->setCell(r, dstCol, c);
  }

  xsh->updateFrameCount();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
}

// Paste shared-clipboard entries (ZtoryClipEntry format) directly into xsheet.
// Uses the Board-local cloneChildToPosition() — same logic as Animatic pasteFromClip.
// After this call: xsh->updateFrameCount(), resequenceXsheet(), refreshFromScene().
static void pasteSharedClipToBoard(const std::vector<ZtoryClipEntry> &clip,
                                   int insertCol, TXsheet *xsh, ToonzScene *scene) {
  for (int ci = 0; ci < (int)clip.size(); ci++) {
    int pos = insertCol + ci;
    const ZtoryClipEntry &ce = clip[ci];
    if (ce.isCut && ce.srcCol == -1) {
      // Immediate cut: original already gone, re-insert via saved cutLevel.
      xsh->insertColumn(pos);
      if (ce.cutLevel) {
        for (int r = 0; r < ce.duration; r++)
          xsh->setCell(r, pos, TXshCell(ce.cutLevel, TFrameId(r + 1)));
      } else if (scene) {
        TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
        if (xl && xl->getChildLevel()) {
          TXshChildLevel *cl = xl->getChildLevel();
          syncChildCameraToMain(xsh, cl);  // new empty sub → match main camera
          for (int r = 0; r < ce.duration; r++)
            xsh->setCell(r, pos, TXshCell(cl, TFrameId(r + 1)));
        }
      }
    } else if (ce.isClone || ce.isCut) {
      // Clone or deferred cut: clone from source column.
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      cloneChildToPosition(srcCol, pos);
    } else {
      // Copy: share the same TXshChildLevel (shared sub-scene).
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      TXshColumn *srcColumn = xsh->getColumn(srcCol);
      if (srcColumn) {
        int r0 = 0, r1 = 0;
        srcColumn->getRange(r0, r1);
        xsh->insertColumn(pos);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, srcCol >= pos ? srcCol + 1 : srcCol);
          if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
        }
      }
    }
  }
}

void StoryboardPanel::onPasteShot() {
  auto before = captureSnapshot();

  // Shared clipboard (written by both Board and Animatic) always has priority:
  // it reflects the most recent copy/cut/clone regardless of which panel did it.
  // Local m_clipboard is used only when shared is empty (e.g. first launch).
  const auto &shared = ZtoryModel::instance()->sharedClip();
  if (!shared.empty()) {
    // Use shared clipboard — covers both Board→Board and Animatic→Board paste.
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene)
      while (scene->getChildStack()->getAncestorCount() > 0)
        CommandManager::instance()->execute("MI_CloseChild");
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    // Compute insert column: after selected shot, or at end.
    int insertCol = m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size()
                    ? m_shots[m_selectedShotIndex].data.xsheetColumn + 1
                    : xsh->getColumnCount();
    pasteSharedClipToBoard(shared, insertCol, xsh, scene);
    xsh->updateFrameCount();
    // Clear one-shot entries (cut/clone) from shared clip; keep plain copies.
    {
      auto newShared = shared;
      newShared.erase(std::remove_if(newShared.begin(), newShared.end(),
                      [](const ZtoryClipEntry &e){ return e.isCut || e.isClone; }),
                      newShared.end());
      ZtoryModel::instance()->setSharedClip(std::move(newShared));
    }
    resequenceXsheet();
    refreshFromScene();
    {
      auto after = captureSnapshot();
      TUndoManager::manager()->add(
          new UndoBoardState(this, tr("Paste Shot"), std::move(before), std::move(after)));
    }
    return;
  }
  // Shared clip is empty — fall back to local m_clipboard (legacy path).
  if (m_clipboard.empty()) return;
  // Auto-return to main xsheet before pasting
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  // m_updating=true: prevents our own onShotInserted/onShotRemovedAt from
  // reacting to the ZtoryModel signals we emit below (for other Board instances).
  m_updating = true;
  // Blocca segnali xsheet durante il paste per evitare rebuild intermedi
  disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int insertAt = m_selectedShotIndex >= 0 ? m_selectedShotIndex + 1 : (int)m_shots.size();
  for (int ci = 0; ci < (int)m_clipboard.size(); ci++) {
    int pos     = insertAt + ci;
    int origSrc = m_clipboard[ci].srcColumn;
    // Calcola srcCol: per ogni inserimento precedente che precede origSrc, origSrc si e slittato di +1
    int srcCol  = origSrc;
    for (int cj = 0; cj < ci; cj++) {
      if ((insertAt + cj) <= srcCol) srcCol++;
    }
    // Immediate-cut (srcColumn==-1): original deleted but cutLevel kept alive.
    // Re-insert the same TXshLevel so sub-scene content (drawings) is preserved.
    if (m_clipboard[ci].isCut && origSrc == -1) {
      int duration = m_clipboard[ci].data.panels.empty()
                     ? 24
                     : m_clipboard[ci].data.panels[0].duration;
      xsh->insertColumn(pos);
      if (m_clipboard[ci].cutLevel) {
        // Re-use saved level: drawings intact
        for (int r = 0; r < duration; r++)
          xsh->setCell(r, pos, TXshCell(m_clipboard[ci].cutLevel, TFrameId(r + 1)));
      } else if (scene) {
        // Fallback: create empty subscene (drawings lost, shouldn't normally happen)
        TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
        if (xl && xl->getChildLevel()) {
          TXshChildLevel *cl = xl->getChildLevel();
          syncChildCameraToMain(xsh, cl);  // new empty sub → match main camera
          for (int r = 0; r < duration; r++)
            xsh->setCell(r, pos, TXshCell(cl, TFrameId(r + 1)));
        }
      }
    }
    // Clone or deferred-cut: clone from still-alive source column.
    // Plain copy: share the same TXshChildLevel instance.
    else if (m_clipboard[ci].isClone || m_clipboard[ci].isCut) {
      cloneChildToPosition(srcCol, pos);
    } else {
      // Copy: riusa lo stesso TXshChildLevel (shared instance)
      TXshColumn *srcColumn = xsh->getColumn(srcCol);
      if (srcColumn) {
        int r0 = 0, r1 = 0;
        srcColumn->getRange(r0, r1);
        xsh->insertColumn(pos);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, srcCol >= pos ? srcCol + 1 : srcCol);
          if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
        }
      }
    }
    // Inserisci shot nel modello locale
    Shot newShot;
    newShot.data = m_clipboard[ci].data;
    newShot.data.shotNumber = "";
    newShot.data.xsheetColumn = pos;
    m_shots.insert(m_shots.begin() + pos, newShot);
    for (int pi = 0; pi < (int)newShot.data.panels.size(); pi++) {
      addPanelWidget(pos, pi);
      m_shots[pos].panels[pi]->setDialog(newShot.data.panels[pi].dialog);
      m_shots[pos].panels[pi]->setAction(newShot.data.panels[pi].action);
      m_shots[pos].panels[pi]->setNotes(newShot.data.panels[pi].notes);
      m_shots[pos].panels[pi]->setDuration(newShot.data.panels[pi].duration);
    }
    // Notify other Board instances (and Animatic)
    emit ZtoryModel::instance()->shotAdded(pos);
  }
  if (!m_autoRenumber) {
    for (int ci = 0; ci < (int)m_clipboard.size(); ci++)
      assignKeepNumbers(insertAt + ci);
  }
  renumberAll();

  // Deferred-cut (srcColumn>=0): delete originals now that clones exist.
  // Immediate-cut (srcColumn==-1): originals already deleted in onCutShot(), skip.
  bool anyCut = false;
  for (auto &ce : m_clipboard) if (ce.isCut && ce.srcColumn >= 0) { anyCut = true; break; }
  if (anyCut) {
    TXsheet *xsh2 = TApp::instance()->getCurrentXsheet()->getXsheet();
    std::vector<int> cutCols;
    for (int ci = 0; ci < (int)m_clipboard.size(); ci++) {
      if (!m_clipboard[ci].isCut || m_clipboard[ci].srcColumn < 0) continue;
      int col = m_clipboard[ci].srcColumn;
      // Adjust for each insertion that shifted this column right
      for (int cj = 0; cj < (int)m_clipboard.size(); cj++)
        if ((insertAt + cj) <= col) col++;
      cutCols.push_back(col);
    }
    std::sort(cutCols.rbegin(), cutCols.rend());
    for (int col : cutCols) {
      for (int i = 0; i < (int)m_shots.size(); i++) {
        if (m_shots[i].data.xsheetColumn == col) {
          for (PanelWidget *pw : m_shots[i].panels) { m_grid->removeWidget(pw); delete pw; }
          m_shots.erase(m_shots.begin() + i);
          for (int j = 0; j < (int)m_shots.size(); j++)
            if (m_shots[j].data.xsheetColumn > col) m_shots[j].data.xsheetColumn--;
          break;
        }
      }
      { std::set<int> cs; cs.insert(col); ColumnCmd::deleteColumns(cs, false, true); }
      emit ZtoryModel::instance()->shotRemovedAt(col);  // notify other Board instances
    }
    xsh2->updateFrameCount();
    m_clipboard.clear();
    m_pasteButton->setEnabled(false);
  }

  m_updating = false;
  // Riconnetti segnale xsheet
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Paste Shot"), std::move(before), std::move(after)));
}

// ── Undo/Redo snapshot helpers ────────────────────────────────────────────────

std::vector<ZtoryShotSnap> StoryboardPanel::captureSnapshot() {
  syncWidgetsToData();
  // ALWAYS read the TOP xsheet. Snapshots can be captured while the user is
  // inside a sub-scene (Match button, text-field focusOut, coalescing duration
  // timer firing late…): the CURRENT xsheet would then be the sub-xsheet,
  // whose cells are not child levels → every ZtoryShotSnap.level would be
  // null, and restoring such a snapshot wipes the whole storyboard
  // (restoreFromSnapshot removes all shot columns and re-inserts nothing).
  ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsh = scn ? scn->getChildStack()->getTopXsheet() : nullptr;
  std::vector<ZtoryShotSnap> snap;
  snap.reserve(m_shots.size());
  for (const Shot &shot : m_shots) {
    ZtoryShotSnap s;
    s.data     = shot.data;
    s.duration = 0;
    int col    = shot.data.xsheetColumn;
    if (xsh) {
      int frameCount = xsh->getFrameCount();
      for (int r = 0; r <= frameCount; r++) {
        TXshCell cell = xsh->getCell(r, col);
        if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()
            && !cell.getFrameId().isStopFrame()) {
          // Skip SFH cells: they have a valid child-level pointer but are not
          // real frames — counting them would inflate s.duration by 1.
          if (!s.level) s.level = cell.m_level;
          s.duration++;
        } else if (s.duration > 0) {
          break;
        }
      }
    }
    if (s.duration == 0) s.duration = 24;
    snap.push_back(std::move(s));
  }
  return snap;
}

void StoryboardPanel::restoreFromSnapshot(const std::vector<ZtoryShotSnap> &snapRef) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // CRITICAL — deep-copy the snapshot BEFORE anything else.  `snapRef` lives
  // inside the UndoBoardState being executed; when the user is inside a
  // sub-scene the MI_CloseChild below pushes a CloseChildUndo, and
  // TUndoManager::add() during an active undo() truncates the redo branch of
  // the stack, DELETING the executing UndoBoardState — the reference would
  // dangle and the re-insert loop would read freed memory (observed: every
  // level "null" → all shot columns removed, nothing re-inserted → storyboard
  // wiped).  The copy's TXshLevelP refs also keep the sub-scene levels alive
  // regardless of who frees the undo object.
  const std::vector<ZtoryShotSnap> snap = snapRef;
  {
    int valid = 0;
    for (const ZtoryShotSnap &s : snap)
      if (s.level && s.level->getChildLevel()) valid++;
    qWarning("[ZTORY] restoreFromSnapshot: snap=%d validLevels=%d shots=%d ancestors=%d",
             (int)snap.size(), valid, (int)m_shots.size(),
             scene->getChildStack()->getAncestorCount());
  }
  // Safety net: a non-empty snapshot where NO entry has a valid sub-scene
  // level is broken (e.g. captured against the wrong xsheet by an older
  // build). Applying it would destroy every shot column and save an empty
  // .ztoryc — refuse instead of wiping the storyboard.
  if (!snap.empty()) {
    bool anyLevel = false;
    for (const ZtoryShotSnap &s : snap)
      if (s.level && s.level->getChildLevel()) { anyLevel = true; break; }
    if (!anyLevel) return;
  }
  // Ensure we are at the top-level xsheet before modifying it.
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (!xsh) return;
  qWarning("[ZTORY] restore: xsh==top? %d  cols=%d frames=%d",
           xsh == scene->getChildStack()->getTopXsheet() ? 1 : 0,
           xsh->getColumnCount(), xsh->getFrameCount());

  disconnect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
             this, &StoryboardPanel::onXsheetChanged);

  clearShots();

  // Remove all current shot columns (child-level columns at indices 0..N-1).
  // Non-shot columns (audio etc.) live at higher indices and shift accordingly.
  int currentShotCols = 0;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    bool isShot = false;
    int fc = xsh->getFrameCount();
    for (int r = 0; r <= fc; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        isShot = true;
        break;
      }
    }
    if (isShot) currentShotCols++;
    else break; // shot columns are always first; stop at first non-shot
  }
  // Remove from left repeatedly (indices shift left each time).
  for (int i = 0; i < currentShotCols; i++)
    xsh->removeColumn(0);

  // Re-insert columns from snapshot.
  qWarning("[ZTORY] restore: removed %d shot cols", currentShotCols);
  for (int i = 0; i < (int)snap.size(); i++) {
    const ZtoryShotSnap &s = snap[i];
    if (!s.level || !s.level->getChildLevel()) continue;
    xsh->insertColumn(i);
    bool okSet = true;
    for (int r = 0; r < s.duration; r++)
      okSet &= xsh->setCell(r, i, TXshCell(s.level.getPointer(), TFrameId(r + 1)));
    qWarning("[ZTORY] restore: col %d dur=%d setCell ok=%d cellChild=%d",
             i, s.duration, okSet ? 1 : 0,
             (!xsh->getCell(0, i).isEmpty() &&
              xsh->getCell(0, i).m_level &&
              xsh->getCell(0, i).m_level->getChildLevel()) ? 1 : 0);
  }
  xsh->updateFrameCount();

  // Rebuild Board state from snapshot data.
  for (int i = 0; i < (int)snap.size(); i++) {
    Shot shot;
    shot.data              = snap[i].data;
    shot.data.xsheetColumn = i;
    m_shots.push_back(std::move(shot));
    for (int pi = 0; pi < (int)snap[i].data.panels.size(); pi++)
      addPanelWidget(i, pi);
  }

  m_selectedShotIndex = -1;
  m_selectedIndices.clear();

  connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &StoryboardPanel::onXsheetChanged);

  app->getCurrentXsheet()->notifyXsheetChanged();
  renumberAll();
  resequenceXsheet();
  rebuildGrid();
  // Re-anchor the path after clearShots() cleared it.
  m_currentZtoryPath = ztoryPath();
  saveZtoryc();
}

// ── UndoBoardState ────────────────────────────────────────────────────────────

void UndoBoardState::undo() const {
  m_panel->restoreFromSnapshot(m_before);
}

void UndoBoardState::redo() const {
  m_panel->restoreFromSnapshot(m_after);
}

// ─────────────────────────────────────────────────────────────────────────────

void StoryboardPanel::onDeleteShot() {
  // Raccogli indici da cancellare (selezione multipla o singola)
  std::vector<int> toDelete(m_selectedIndices.begin(), m_selectedIndices.end());
  if (toDelete.empty() && m_selectedShotIndex >= 0) toDelete.push_back(m_selectedShotIndex);
  if (toDelete.empty()) return;

  auto before = captureSnapshot();

  // Usa data.xsheetColumn (non l'indice Board) per identificare le colonne
  // da cancellare nell'xsheet. Se i due sono disallineati (dopo merge/cut),
  // usare l'indice Board cancellerebbe la colonna sbagliata.
  // Ordina per xsheet column decrescente: cancellare dall'alto mantiene
  // stabili gli indici delle colonne inferiori nelle iterazioni successive.
  std::vector<int> xshCols;
  for (int idx : toDelete) {
    if (idx >= 0 && idx < (int)m_shots.size())
      xshCols.push_back(m_shots[idx].data.xsheetColumn);
  }
  std::sort(xshCols.rbegin(), xshCols.rend());

  disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);

  for (int col : xshCols) {
    // Cerca il board shot corrispondente a questa colonna xsheet.
    int si = -1;
    for (int i = 0; i < (int)m_shots.size(); i++)
      if (m_shots[i].data.xsheetColumn == col) { si = i; break; }
    if (si < 0) continue;
    for (PanelWidget *pw : m_shots[si].panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
    m_shots.erase(m_shots.begin() + si);
    // Aggiorna xsheetColumn degli shot rimasti che erano dopo col.
    for (int i = 0; i < (int)m_shots.size(); i++)
      if (m_shots[i].data.xsheetColumn > col)
        m_shots[i].data.xsheetColumn--;
    std::set<int> colSet; colSet.insert(col);
    ColumnCmd::deleteColumns(colSet, false, true);  // withoutUndo=true: our UndoBoardState owns this
  }

  m_selectedShotIndex = -1;
  m_selectedIndices.clear();
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  renumberAll();
  ZtoryModel::instance()->resequenceXsheet();
  rebuildGrid();
  saveZtoryc();

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Delete Shot"), std::move(before), std::move(after)));
}

void StoryboardPanel::onAddShot() {
  auto before = captureSnapshot();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene && scene->getChildStack()->getAncestorCount() > 0)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  int duration = 24;
  int insertAt = (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
                 ? m_selectedShotIndex + 1
                 : (int)m_shots.size();
  if (scene && xsh) {
    TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
    if (xl && xl->getChildLevel()) {
      TXshChildLevel *cl = xl->getChildLevel();
      xsh->insertColumn(insertAt);
      for (int r = 0; r < duration; r++)
        xsh->setCell(r, insertAt, TXshCell(cl, TFrameId(r+1)));
      xsh->updateFrameCount();

      // Inizializza camera della sottoscena copiando quella del main
      syncChildCameraToMain(xsh, cl);

      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }
  Shot shot;
  shot.data.xsheetColumn = insertAt;
  PanelData pd;
  pd.startFrame = 0;
  pd.duration = duration;
  shot.data.panels.push_back(pd);
  m_shots.insert(m_shots.begin() + insertAt, shot);
  addPanelWidget(insertAt, 0);
  if (!m_autoRenumber) assignKeepNumbers(insertAt);
  renumberAll();
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
  selectShot(insertAt);

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Add Shot"), std::move(before), std::move(after)));
}

void StoryboardPanel::onEditShot(int shotIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;

  // Select this shot in the Board before entering edit mode
  selectShot(shotIdx);
  m_selectedIndices.clear();
  m_selectedIndices.insert(shotIdx);

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (!xsh) return;
  int col = m_shots[shotIdx].data.xsheetColumn;
  int row = 0;
  for (int r = 0; r < xsh->getFrameCount(); r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      row = r; break;
    }
  }
  app->getCurrentColumn()->setColumnIndex(col);
  app->getCurrentFrame()->setFrame(row);
  CommandManager::instance()->execute("MI_OpenChild");
  // Switch the viewer panel to shot view (ZtoryAnimaticViewerPanel listens).
  ZtoryModel::instance()->activateShotForViewing(col);
}

// ── Light-direction gizmo placement/removal (task 40 FASE 3) ────────────────

void StoryboardPanel::onLightPlaced(int shotIdx, int panelIdx,
                                    double tailX, double tailY,
                                    double tipX, double tipY,
                                    double depth, double spread) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  Shot &shot = m_shots[shotIdx];
  if (panelIdx < 0 || panelIdx >= (int)shot.data.panels.size()) return;
  auto before = captureSnapshot();
  PanelData &pd = shot.data.panels[panelIdx];
  pd.hasLight   = true;
  pd.lightTailX = tailX;
  pd.lightTailY = tailY;
  pd.lightTipX  = tipX;
  pd.lightTipY  = tipY;
  pd.lightDepth  = depth;
  pd.lightSpread = spread;
  pd.lightColor  = QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
  ZtoryModel::instance()->syncShotPanels(shotIdx, shot.data.panels,
                                         shot.data.shotLabel,
                                         shot.data.xsheetColumn);
  saveZtoryc();
  updatePreview(shotIdx, panelIdx);
  TUndoManager::manager()->add(new UndoBoardState(
      this, tr("Light Direction"), std::move(before), captureSnapshot()));
}

void StoryboardPanel::onLightRemoved(int shotIdx, int panelIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  Shot &shot = m_shots[shotIdx];
  if (panelIdx < 0 || panelIdx >= (int)shot.data.panels.size()) return;
  PanelData &pd = shot.data.panels[panelIdx];
  if (!pd.hasLight) return;
  auto before = captureSnapshot();
  pd.hasLight = false;
  ZtoryModel::instance()->syncShotPanels(shotIdx, shot.data.panels,
                                         shot.data.shotLabel,
                                         shot.data.xsheetColumn);
  saveZtoryc();
  updatePreview(shotIdx, panelIdx);
  TUndoManager::manager()->add(new UndoBoardState(
      this, tr("Remove Light Direction"), std::move(before), captureSnapshot()));
}

void StoryboardPanel::onMatchDuration(int shotIdx) {
  // Resize the main xsheet column to match the sub-scene's actual frame count.
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  auto before = captureSnapshot();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  int col = m_shots[shotIdx].data.xsheetColumn;
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column) return;

  // Find child level in this column
  TXshChildLevel *cl = nullptr;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  for (int r = r0; r <= r1 && !cl; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
      cl = cell.m_level->getChildLevel();
  }
  if (!cl) return;

  int actualDuration = cl->getXsheet()->getFrameCount();
  if (actualDuration <= 0) return;
  if (actualDuration == (r1 - r0 + 1)) return;  // already matching

  // Resize column: clear and set new cell count (resequenceXsheet repositions)
  int maxFrames = mainXsh->getFrameCount() + actualDuration + 10;
  for (int r = 0; r <= maxFrames; r++) mainXsh->clearCells(r, col);
  for (int r = 0; r < actualDuration; r++)
    mainXsh->setCell(r, col, TXshCell(cl, TFrameId(r + 1)));

  mainXsh->updateFrameCount();
  ZtoryModel::instance()->resequenceXsheet();
  app->getCurrentXsheet()->notifyXsheetChanged();

  // Update Board T: display
  for (PanelWidget *pw : m_shots[shotIdx].panels)
    pw->setTotalDuration(actualDuration);
  if (m_shots[shotIdx].data.panels.size() == 1) {
    m_shots[shotIdx].data.panels[0].duration = actualDuration;
    if (!m_shots[shotIdx].panels.empty())
      m_shots[shotIdx].panels[0]->setDuration(actualDuration);
  }
  saveZtoryc();

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Match Duration"), std::move(before), std::move(after)));
}

void StoryboardPanel::onBackToBoard() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  MainWindow *mw = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (mw) mw->switchToRoom("BOARD");
}

void StoryboardPanel::onPanelClicked(int shotIdx, int panelIdx, Qt::KeyboardModifiers modifiers) {
  // Focus the scroll area (a child of StoryboardPanel) so that
  // WidgetWithChildrenShortcut QShortcuts on the panel fire correctly.
  m_scrollArea->setFocus(Qt::MouseFocusReason);
  if (modifiers & Qt::ControlModifier || modifiers & Qt::MetaModifier) {
    // Ctrl+click: aggiungi/rimuovi dalla selezione
    if (m_selectedIndices.count(shotIdx)) {
      m_selectedIndices.erase(shotIdx);
      for (PanelWidget *pw : m_shots[shotIdx].panels) pw->setSelected(false);
    } else {
      m_selectedIndices.insert(shotIdx);
      for (PanelWidget *pw : m_shots[shotIdx].panels) pw->setSelected(true);
    }
    m_selectedShotIndex = shotIdx;
  } else if (modifiers & Qt::ShiftModifier) {
    // Shift+click: seleziona range
    int from = qMin(m_selectedShotIndex, shotIdx);
    int to   = qMax(m_selectedShotIndex, shotIdx);
    if (from < 0) from = shotIdx;
    for (int i = 0; i < (int)m_shots.size(); i++) {
      bool sel = (i >= from && i <= to);
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(sel);
      if (sel) m_selectedIndices.insert(i);
      else     m_selectedIndices.erase(i);
    }
  } else {
    // Click normale: deseleziona tutto e seleziona solo questo
    for (int i = 0; i < (int)m_shots.size(); i++)
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(false);
    m_selectedIndices.clear();
    selectShot(shotIdx);
    m_selectedIndices.insert(shotIdx);
  }
  // Sync Board selection to ZtoryModel so Animatic's merge button can use it.
  {
    std::set<int> cols;
    for (int i : m_selectedIndices)
      if (i >= 0 && i < (int)m_shots.size())
        cols.insert(m_shots[i].data.xsheetColumn);
    if (cols.empty() && m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
      cols.insert(m_shots[m_selectedShotIndex].data.xsheetColumn);
    ZtoryModel::instance()->setSharedSelection(std::move(cols));
  }
}

void StoryboardPanel::onDurationChanged(int shotIdx, int panelIdx, int frames) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  if (panelIdx < 0 || panelIdx >= (int)m_shots[shotIdx].data.panels.size()) return;
  // Capture "before" only on the first change in a coalescing window.
  if (m_pendingDurationBefore.empty())
    m_pendingDurationBefore = captureSnapshot();
  m_durationCommitTimer->start();  // (re)start 600ms debounce
  m_shots[shotIdx].data.panels[panelIdx].duration = frames;
  int tot = m_shots[shotIdx].data.totalDuration();
  for (PanelWidget *pw : m_shots[shotIdx].panels)
    pw->setTotalDuration(tot);
  resequenceXsheet();
  saveZtoryc();
}

void StoryboardPanel::commitDurationUndo() {
  if (m_pendingDurationBefore.empty()) return;
  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Resize Shot Duration"),
                         std::move(m_pendingDurationBefore), std::move(after)));
  m_pendingDurationBefore.clear();
}

void StoryboardPanel::commitTextUndo() {
  if (!m_textEditing) return;
  m_textEditing = false;
  auto after = captureSnapshot();
  // Only push undo if any panel's text actually changed.
  bool changed = (after.size() != m_textUndoBefore.size());
  for (size_t i = 0; i < after.size() && !changed; i++) {
    const auto &ap = after[i].data.panels;
    const auto &bp = m_textUndoBefore[i].data.panels;
    if (ap.size() != bp.size()) { changed = true; break; }
    for (size_t j = 0; j < ap.size() && !changed; j++) {
      changed = ap[j].dialog != bp[j].dialog ||
                ap[j].action != bp[j].action ||
                ap[j].notes  != bp[j].notes;
    }
  }
  if (changed) {
    TUndoManager::manager()->add(
        new UndoBoardState(this, tr("Edit Text"),
                           std::move(m_textUndoBefore), std::move(after)));
  }
}

void StoryboardPanel::onMoveShot(int fromShot, int toShot) {
  if (!ZtoryModel::assertMainXsheet(true)) return;   // warn: exit edit mode first
  if (fromShot == toShot) return;
  if (fromShot < 0 || fromShot >= (int)m_shots.size()) return;
  if (toShot < 0 || toShot >= (int)m_shots.size()) return;

  auto before = captureSnapshot();
  Shot s = m_shots[fromShot];
  m_shots.erase(m_shots.begin() + fromShot);
  m_shots.insert(m_shots.begin() + toShot, s);
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene) {
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (xsh) {
      int maxFrames = xsh->getFrameCount() + 200;
      int numCols = (int)m_shots.size();
      std::vector<std::vector<TXshCell>> cols(numCols);
      for (int c = 0; c < numCols; c++)
        for (int r = 0; r <= maxFrames; r++)
          cols[c].push_back(xsh->getCell(r, c));
      std::vector<TXshCell> tmp = cols[fromShot];
      cols.erase(cols.begin() + fromShot);
      cols.insert(cols.begin() + toShot, tmp);
      for (int c = 0; c < numCols; c++) {
        for (int r = 0; r <= maxFrames; r++) xsh->clearCells(r, c);
        for (int r = 0; r < (int)cols[c].size(); r++)
          if (!cols[c][r].isEmpty()) xsh->setCell(r, c, cols[c][r]);
      }
      xsh->updateFrameCount();
      app->getCurrentXsheet()->notifyXsheetChanged();
      // Cells were physically rearranged: update xsheetColumn to match new positions.
      for (int i = 0; i < numCols; i++)
        m_shots[i].data.xsheetColumn = i;
    }
  }
  renumberAll();
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
  selectShot(toShot);

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Move Shot"), std::move(before), std::move(after)));
}

void StoryboardPanel::onColumnsChanged(int value) {
  m_columnsPerRow = value;
  ZtoryBoardColumns = value;  // persist across sessions
  rebuildGrid();
}

void StoryboardPanel::onNumberingChanged(int comboIndex) {
  if (comboIndex == 0) {
    m_autoRenumber = true;
    // Non rinumera subito - lo farà al prossimo addShot
  } else if (comboIndex == 1) {
    m_autoRenumber = false;
  } else if (comboIndex == 2) {
    m_autoRenumber = true;
    for (int i = 0; i < (int)m_shots.size(); i++)
      m_shots[i].data.shotNumber = QString("%1").arg(i+1, 2, 10, QChar(48));
    renumberAll();
    m_numberingCombo->blockSignals(true);
    m_numberingCombo->setCurrentIndex(0);
    m_numberingCombo->blockSignals(false);
  }
  saveZtoryc();
}

void StoryboardPanel::onNumberingConfig() {
  // Inline dialog for configuring the project's shot numbering scheme.
  NumberingConfig &cfg = ZtoryModel::instance()->numberingConfig();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Shot Numbering Config"));
  dlg.setMinimumWidth(340);
  auto *lay = new QGridLayout(&dlg);
  lay->setColumnStretch(1, 1);
  lay->setSpacing(6);
  lay->setContentsMargins(12, 12, 12, 12);

  // Style
  auto *styleCB = new QComboBox(&dlg);
  styleCB->addItem(tr("Simple   (sh010, sh020…)"));
  styleCB->addItem(tr("Sequence  (sq01_sh010…)"));
  styleCB->setCurrentIndex((int)cfg.style);
  lay->addWidget(new QLabel(tr("Style:"), &dlg),    0, 0);
  lay->addWidget(styleCB,                            0, 1, 1, 3);

  // Shot prefix
  auto *shotPxFld = new QLineEdit(cfg.shotPrefix, &dlg);
  shotPxFld->setMaximumWidth(60);
  lay->addWidget(new QLabel(tr("Shot prefix:"), &dlg), 1, 0);
  lay->addWidget(shotPxFld, 1, 1);

  // Seq prefix
  auto *seqPxLabel = new QLabel(tr("Seq prefix:"), &dlg);
  auto *seqPxFld   = new QLineEdit(cfg.seqPrefix, &dlg);
  seqPxFld->setMaximumWidth(60);
  lay->addWidget(seqPxLabel, 1, 2);
  lay->addWidget(seqPxFld,   1, 3);

  // Step
  auto *stepSB = new QSpinBox(&dlg);
  stepSB->setRange(1, 1000); stepSB->setValue(cfg.step);
  lay->addWidget(new QLabel(tr("Step:"), &dlg),    2, 0);
  lay->addWidget(stepSB,                            2, 1);

  // Padding
  auto *padSB = new QSpinBox(&dlg);
  padSB->setRange(1, 6); padSB->setValue(cfg.padding);
  lay->addWidget(new QLabel(tr("Padding:"), &dlg), 2, 2);
  lay->addWidget(padSB,                             2, 3);

  // Start number
  auto *startSB = new QSpinBox(&dlg);
  startSB->setRange(1, 9999); startSB->setValue(cfg.startNumber);
  lay->addWidget(new QLabel(tr("Start #:"), &dlg), 3, 0);
  lay->addWidget(startSB,                           3, 1);

  // Seq number
  auto *seqNumSB = new QSpinBox(&dlg);
  seqNumSB->setRange(1, 999); seqNumSB->setValue(cfg.seqNumber);
  auto *seqNumLabel = new QLabel(tr("Seq #:"), &dlg);
  lay->addWidget(seqNumLabel, 3, 2);
  lay->addWidget(seqNumSB,    3, 3);

  // Reset shot counter on sequence change
  auto *resetOnSeqCB = new QCheckBox(tr("Restart shot # at each new sequence"), &dlg);
  resetOnSeqCB->setChecked(cfg.resetOnSeqChange);
  auto *resetOnSeqLabel = new QLabel("", &dlg); // spacer label for alignment
  lay->addWidget(resetOnSeqCB, 4, 0, 1, 4);

  // Show/hide seq controls based on style
  auto syncSeqVisibility = [&](int idx) {
    bool isSeq = (idx == 1);
    seqPxLabel->setVisible(isSeq);
    seqPxFld->setVisible(isSeq);
    seqNumLabel->setVisible(isSeq);
    seqNumSB->setVisible(isSeq);
    resetOnSeqCB->setVisible(isSeq);
  };
  syncSeqVisibility(styleCB->currentIndex());
  connect(styleCB, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dlg, syncSeqVisibility);

  // Buttons
  auto *btns = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(btns, 5, 0, 1, 4);
  connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return;

  // Apply changes
  cfg.style       = (NumberingConfig::Style)styleCB->currentIndex();
  cfg.shotPrefix  = shotPxFld->text().trimmed().isEmpty() ? "sh" : shotPxFld->text().trimmed();
  cfg.seqPrefix   = seqPxFld->text().trimmed().isEmpty()  ? "sq" : seqPxFld->text().trimmed();
  cfg.step             = stepSB->value();
  cfg.padding          = padSB->value();
  cfg.startNumber      = startSB->value();
  cfg.seqNumber        = seqNumSB->value();
  cfg.resetOnSeqChange = resetOnSeqCB->isChecked();

  // If in auto-renumber mode, renumber all shots immediately (also updates visibility).
  if (m_autoRenumber) {
    renumberAll();
    saveZtoryc();
  } else {
    // Even without renumbering, update SQ field visibility immediately.
    bool isSeq = (cfg.style == NumberingConfig::Sequence);
    for (Shot &s : m_shots)
      for (PanelWidget *pw : s.panels)
        pw->setSeqVisible(isSeq);
  }
}

void StoryboardPanel::onRefreshPreviews() {
  // Render only panels that are currently visible in the scroll area.
  // Calling updatePreview for ALL panels (e.g. 631 on a large scene) loads every
  // drawing from every sub-xsheet into memory — easily 20-50 GB on complex
  // projects with SFH-heavy shots.  Panels that scroll into view are handled
  // lazily by updateVisiblePreviews() which is connected to the scroll-bar signal.
  updateVisiblePreviews();
}

// ── Audio export helper ───────────────────────────────────────────────────────
// Injects a temporary sound column into childXsh containing only the audio
// that falls within [shotR0, shotR1] of the main xsheet.
// Returns the list of column indices inserted (to be removed after save).
// One column per audio column in the main xsheet that overlaps the shot range.
static QList<int> injectAudioForShot(TXsheet *mainXsh, TXsheet *childXsh,
                                     int shotR0, int shotR1, double fps) {
  QList<int> injected;
  int mainCols = mainXsh->getColumnCount();
  for (int mc = 0; mc < mainCols; mc++) {
    TXshColumn *col = mainXsh->getColumn(mc);
    if (!col) continue;
    TXshSoundColumn *srcSc = col->getSoundColumn();
    if (!srcSc) continue;

    // Collect ColumnLevels that overlap [shotR0, shotR1]
    QList<ColumnLevel *> toInsert;
    for (int li = 0; li < srcSc->getColumnLevelCount(); li++) {
      ColumnLevel *cl = srcSc->getColumnLevel(li);
      if (!cl) continue;
      int vsf = cl->getVisibleStartFrame();
      int vef = cl->getVisibleEndFrame();
      if (vsf > shotR1 || vef < shotR0) continue;  // no overlap

      // Clip to shot boundary
      int clipStart     = std::max(vsf, shotR0);
      int clipEnd       = std::min(vef, shotR1);
      int addStartOff   = clipStart - vsf;
      int addEndOff     = vef - clipEnd;

      // Clone and adjust: startFrame relative to shot start (frame 0).
      // IMPORTANT: use cl->getStartFrame() (raw, before offset), NOT vsf
      // (= startFrame + startOffset). Using vsf would shift the audible
      // region by startOffset frames, making the clip play too late AND
      // extending its visible end past the shot boundary.
      ColumnLevel *newCl = new ColumnLevel(
          cl->getSoundLevel(),
          cl->getStartFrame() - shotR0,            // startFrame relative to shot
          cl->getStartOffset() + addStartOff,      // trimmed start
          cl->getEndOffset()   + addEndOff,        // trimmed end
          fps);
      toInsert.append(newCl);
    }
    if (toInsert.isEmpty()) continue;

    // Insert a new sound column at the end of the child xsheet
    int newCol = childXsh->getColumnCount();
    childXsh->insertColumn(newCol, TXshColumn::eSoundType);
    TXshSoundColumn *dstSc = childXsh->getColumn(newCol)->getSoundColumn();
    if (!dstSc) { for (auto *c : toInsert) delete c; continue; }

    dstSc->setFrameRate(fps);
    for (ColumnLevel *cl : toInsert)
      // adoptLevel() is the public counterpart of the protected insertColumnLevel():
      // it takes ownership of cl and places its visible start at targetFrame.
      // Passing cl->getVisibleStartFrame() keeps the position we set in the constructor.
      dstSc->adoptLevel(cl, cl->getVisibleStartFrame());

    // Mark column as reserved audio (visible in xsheet but !a drawing col)
    TStageObject *obj = childXsh->getStageObjectTree()
                          ->getStageObject(TStageObjectId::ColumnId(newCol), false);
    if (obj) obj->setName("_audio_main_");

    injected.append(newCol);
  }
  return injected;
}

// Remove injected audio columns in reverse order (to keep indices stable)
static void removeInjectedAudio(TXsheet *childXsh, QList<int> cols) {
  std::sort(cols.begin(), cols.end(), std::greater<int>());
  for (int c : cols)
    childXsh->removeColumn(c);
}

void StoryboardPanel::onExportShots() {
  if (m_shots.empty()) {
    QMessageBox::information(this, "Export Shots", "No shots to export.");
    return;
  }

  // Popup selezione range
  QDialog dlg(this);
  dlg.setWindowTitle("Export Shots as Scenes");
  QVBoxLayout *lay = new QVBoxLayout(&dlg);

  QHBoxLayout *rangeLayout = new QHBoxLayout();
  QRadioButton *allRadio = new QRadioButton("All shots");
  QRadioButton *rangeRadio = new QRadioButton("Range:");
  allRadio->setChecked(true);
  QSpinBox *fromSpin = new QSpinBox(); fromSpin->setMinimum(1); fromSpin->setMaximum((int)m_shots.size()); fromSpin->setValue(1);
  QSpinBox *toSpin = new QSpinBox(); toSpin->setMinimum(1); toSpin->setMaximum((int)m_shots.size()); toSpin->setValue((int)m_shots.size());
  QLabel *toLabel = new QLabel("to");
  fromSpin->setEnabled(false); toSpin->setEnabled(false); toLabel->setEnabled(false);
  rangeLayout->addWidget(allRadio);
  rangeLayout->addWidget(rangeRadio);
  rangeLayout->addWidget(fromSpin);
  rangeLayout->addWidget(toLabel);
  rangeLayout->addWidget(toSpin);
  rangeLayout->addStretch();
  lay->addLayout(rangeLayout);

  QObject::connect(rangeRadio, &QRadioButton::toggled, [&](bool checked){
    fromSpin->setEnabled(checked); toSpin->setEnabled(checked); toLabel->setEnabled(checked);
  });

  QDialogButtonBox *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return;

  int from = allRadio->isChecked() ? 0 : fromSpin->value() - 1;
  int to   = allRadio->isChecked() ? (int)m_shots.size() - 1 : toSpin->value() - 1;

  // Export
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath scenesDir = scene->decodeFilePath(TFilePath("+scenes"));
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  // Use ZtoryModel fps (already synced from scene at load/new) to avoid pulling
  // in the TSceneProperties include just for this one call.
  double fps = (double)ZtoryModel::instance()->fps();

  int ok = 0, fail = 0;
  for (int i = from; i <= to; i++) {
    std::string shotNumStr = m_shots[i].data.shotNumber.toStdString();
    TFilePath outPath = scenesDir + TFilePath("sc" + shotNumStr + ".tnz");

    // Crea cartella scenes se non esiste
    if (!TFileStatus(outPath.getParentDir()).doesExist())
      TSystem::mkDir(outPath.getParentDir());

    // Determina range del main xsheet per questo shot.
    // ignoreLastStop=true: exclude the trailing stop-hold frame so the audio
    // range matches the rendered video duration exactly (stop-hold is not
    // rendered by the video renderer).
    int shotCol = m_shots[i].data.xsheetColumn;
    int shotR0 = 0, shotR1 = 0;
    if (mainXsh && mainXsh->getColumn(shotCol))
      mainXsh->getColumn(shotCol)->getRange(shotR0, shotR1,
                                            /*ignoreLastStop=*/true);

    // Apri sottoscena
    TApp::instance()->getCurrentColumn()->setColumnIndex(shotCol);
    TColumnSelection *colSel = new TColumnSelection();
    colSel->selectColumn(shotCol, true);
    TSelection::setCurrent(colSel);
    ztoryOpenSubXsheet();

    if (scene->getChildStack()->getAncestorCount() == 0) { fail++; continue; }

    // Inietta audio principale nel child xsheet prima del salvataggio
    TXsheet *childXsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    QList<int> injectedCols;
    if (mainXsh && childXsh && shotR1 >= shotR0)
      injectedCols = injectAudioForShot(mainXsh, childXsh, shotR0, shotR1, fps);

    bool saved = IoCmd::saveScene(outPath, IoCmd::SAVE_SUBXSHEET);
    if (saved) ok++; else fail++;

    // Rimuovi colonne audio temporanee (non devono restare nella sottoscena)
    if (!injectedCols.isEmpty() && childXsh)
      removeInjectedAudio(childXsh, injectedCols);

    ztoryCloseSubXsheet(1);
  }

  QString msg = QString("Export completato: %1 shot esportati").arg(ok);
  if (fail > 0) msg += QString(", %1 falliti").arg(fail);
  QMessageBox::information(this, "Export Shots", msg);
}

void StoryboardPanel::onExportAnimatic() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export Animatic"), tr("No shots to export."));
    return;
  }
  if (!ZtoryModel::assertMainXsheet(true)) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties *prop = scene->getProperties()->getOutputProperties();

  // ── Build default output dir/name from scene ───────────────────���───────
  TFilePath sp       = scene->getScenePath();
  QString sceneName  = QString::fromStdWString(sp.getWideName());
  if (sceneName.isEmpty()) sceneName = "animatic";
  QString defaultDir = QString::fromStdWString(sp.getParentDir().getWideString());
  if (defaultDir.isEmpty()) defaultDir = QDir::homePath();
  // Determine extension from current output props
  QString ext = QString::fromStdString(prop->getPath().getType());
  if (ext.isEmpty()) ext = "mp4";

  // ── Dialog ───────────────────────────────��─────────────────────────────
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Export Animatic"));
  dlg.setMinimumWidth(480);
  auto *mainLay = new QVBoxLayout(&dlg);
  mainLay->setSpacing(10);
  mainLay->setContentsMargins(14, 14, 14, 14);

  // Mode
  auto *modeGroup = new QGroupBox(tr("Export mode"), &dlg);
  auto *modeVLay  = new QVBoxLayout(modeGroup);
  auto *btnGroup  = new QButtonGroup(&dlg);
  auto *radioFull  = new QRadioButton(tr("Full animatic (all shots in sequence)"), modeGroup);
  auto *radioRange = new QRadioButton(tr("Shot range:"), modeGroup);
  auto *radioEach  = new QRadioButton(tr("One clip per shot  (one file per shot)"), modeGroup);
  radioFull->setChecked(true);
  btnGroup->addButton(radioFull,  0);
  btnGroup->addButton(radioRange, 1);
  btnGroup->addButton(radioEach,  2);

  // Range selectors (shown only when radioRange is active)
  auto *rangeWidget = new QWidget(modeGroup);
  auto *rangeLay    = new QHBoxLayout(rangeWidget);
  rangeLay->setContentsMargins(20, 0, 0, 0);
  rangeLay->setSpacing(6);
  auto *fromCombo = new QComboBox(rangeWidget);
  auto *toCombo   = new QComboBox(rangeWidget);
  for (int si = 0; si < (int)m_shots.size(); si++) {
    QString label = m_shots[si].data.shotNumber;
    fromCombo->addItem(label);
    toCombo->addItem(label);
  }
  toCombo->setCurrentIndex(toCombo->count() - 1);
  rangeLay->addWidget(new QLabel(tr("from"), rangeWidget));
  rangeLay->addWidget(fromCombo);
  rangeLay->addWidget(new QLabel(tr("to"), rangeWidget));
  rangeLay->addWidget(toCombo);
  rangeLay->addStretch();
  rangeWidget->setEnabled(false);

  modeVLay->addWidget(radioFull);
  modeVLay->addWidget(radioRange);
  modeVLay->addWidget(rangeWidget);
  modeVLay->addWidget(radioEach);
  mainLay->addWidget(modeGroup);

  connect(radioRange, &QRadioButton::toggled,
          rangeWidget, &QWidget::setEnabled);

  // Render format comes from the native Render Settings — the dialog shows a
  // live summary and a button to open them. The label refreshes while the
  // (non-modal) Output Settings popup is used, so the user always confirms
  // the actual format before exporting.
  auto *fmtNote = new QLabel(&dlg);
  auto refreshFormatNote = [this, fmtNote, prop, scene]() {
    QString e = QString::fromStdString(prop->getPath().getType()).toUpper();
    if (e.isEmpty()) e = "MP4";
    fmtNote->setText(
        tr("Format: %1  |  %2 fps  |  %3×%4")
            .arg(e)
            .arg(prop->getFrameRate(), 0, 'f', 0)
            .arg(scene->getCurrentCamera()->getRes().lx)
            .arg(scene->getCurrentCamera()->getRes().ly));
  };
  refreshFormatNote();
  fmtNote->setStyleSheet("color:#ddd; font-size:11px;");
  {
    auto *fmtRow = new QHBoxLayout;
    fmtRow->addWidget(fmtNote, 1);
    auto *rsBtn = new QPushButton(tr("Render Settings…"), &dlg);
    rsBtn->setToolTip(tr("Set the output format, codec, fps and resolution "
                         "before exporting"));
    connect(rsBtn, &QPushButton::clicked, &dlg, []() {
      CommandManager::instance()->execute(MI_OutputSettings);
    });
    fmtRow->addWidget(rsBtn);
    mainLay->addLayout(fmtRow);
  }

  // Output folder
  auto *folderRow = new QHBoxLayout;
  auto *folderEdit = new QLineEdit(defaultDir, &dlg);
  auto *folderBtn  = new QToolButton(&dlg);
  folderBtn->setText("…");
  connect(folderBtn, &QToolButton::clicked, [&]() {
    QString d = QFileDialog::getExistingDirectory(&dlg, tr("Output folder"), folderEdit->text());
    if (!d.isEmpty()) folderEdit->setText(d);
  });
  folderRow->addWidget(new QLabel(tr("Output folder:"), &dlg));
  folderRow->addWidget(folderEdit, 1);
  folderRow->addWidget(folderBtn);
  mainLay->addLayout(folderRow);

  // Filename (only meaningful for Full/Range; per-shot uses shot number)
  auto *nameRow   = new QHBoxLayout;
  auto *nameEdit  = new QLineEdit(sceneName + "_animatic", &dlg);
  auto *nameNote  = new QLabel(tr("(.%1)").arg(ext), &dlg);
  nameNote->setStyleSheet("color:#aaa;");
  nameRow->addWidget(new QLabel(tr("Filename:"), &dlg));
  nameRow->addWidget(nameEdit, 1);
  nameRow->addWidget(nameNote);
  mainLay->addLayout(nameRow);

  // Inform user: per-shot uses shot number as filename
  auto *perShotNote = new QLabel(
      tr("Per-shot mode: files will be named  %1_SH010.%2,  %1_SH020.%2 …")
          .arg(sceneName).arg(ext), &dlg);
  perShotNote->setStyleSheet("color:#aaa; font-size:11px;");
  perShotNote->setVisible(false);
  mainLay->addWidget(perShotNote);
  connect(radioEach, &QRadioButton::toggled, [&](bool on) {
    nameEdit->setEnabled(!on);
    perShotNote->setVisible(on);
  });

  // Options
  auto *optLay   = new QHBoxLayout;
  auto *chkAudio = new QCheckBox(tr("Include audio"), &dlg);
  chkAudio->setChecked(true);
  chkAudio->setToolTip(tr("Audio tracks in the main xsheet are included automatically\n"
                           "when rendering from the main timeline."));
  chkAudio->setEnabled(false);  // always on — audio is automatic
  optLay->addWidget(chkAudio);
  mainLay->addLayout(optLay);

  // Burn-in overlays (Storyboard Pro-style)
  auto *burnGroup = new QGroupBox(tr("Burn-in"), &dlg);
  auto *burnLay   = new QVBoxLayout(burnGroup);
  auto *chkTC     = new QCheckBox(tr("Timecode (bottom right)"), burnGroup);
  auto *chkNames  = new QCheckBox(tr("Shot / panel name (top left, e.g. SQ010_SH010_P001)"),
                                  burnGroup);
  auto *chkClap   = new QCheckBox(tr("Clapperboard at start (Render Settings > Board)"),
                                  burnGroup);
  chkTC->setChecked(QSettings().value("Ztoryc/BurnInTimecode", false).toBool());
  chkNames->setChecked(QSettings().value("Ztoryc/BurnInShotNames", false).toBool());
  BoardSettings *boardSettings = prop->getBoardSettings();
  chkClap->setChecked(boardSettings && boardSettings->isActive());
  chkClap->setToolTip(tr("Prepends the clapperboard configured in Render Settings\n"
                         "(Board tab) to each exported clip. This toggles the same\n"
                         "scene setting as the Output Settings popup."));
  // Two-way live mirror of the scene's Board setting: toggling here applies
  // immediately; changing it from the Output Settings popup updates the
  // checkbox on the next poll tick. No divergence is possible.
  connect(chkClap, &QCheckBox::toggled, &dlg, [boardSettings](bool on) {
    if (boardSettings) boardSettings->setActive(on);
  });
  burnLay->addWidget(chkTC);
  burnLay->addWidget(chkNames);
  burnLay->addWidget(chkClap);
  mainLay->addWidget(burnGroup);

  // Poll while the dialog is open: the Output Settings popup is a separate
  // non-modal window, so format and Board changes made there must be
  // reflected here live.
  {
    QTimer *pollTimer = new QTimer(&dlg);
    pollTimer->setInterval(700);
    connect(pollTimer, &QTimer::timeout, &dlg,
            [refreshFormatNote, chkClap, boardSettings]() {
      refreshFormatNote();
      if (boardSettings) {
        chkClap->blockSignals(true);
        chkClap->setChecked(boardSettings->isActive());
        chkClap->blockSignals(false);
      }
    });
    pollTimer->start();
  }

  // Buttons
  auto *btnBox = new QDialogButtonBox(
      QDialogButtonBox::Cancel | QDialogButtonBox::Ok, &dlg);
  btnBox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
  connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  mainLay->addWidget(btnBox);

  // The dialog must be truly NON-modal: the Output Settings popup parents to
  // the same main window, so any window-modal dialog in that chain would
  // freeze it (unusable, unclosable). show() + a local event loop keeps the
  // export flow blocking for this function while every other window stays
  // interactive.
  dlg.setWindowModality(Qt::NonModal);
  {
    QEventLoop loop;
    connect(&dlg, &QDialog::finished, &loop, &QEventLoop::quit);
    dlg.show();
    dlg.raise();
    dlg.activateWindow();
    loop.exec();
  }
  if (dlg.result() != QDialog::Accepted) return;

  // Re-read the extension: the user may have changed the output format from
  // the Render Settings popup while this dialog was open.
  ext = QString::fromStdString(prop->getPath().getType());
  if (ext.isEmpty()) ext = "mp4";

  // Burn-in checkboxes → persisted defaults + scene board setting.
  QSettings().setValue("Ztoryc/BurnInTimecode",  chkTC->isChecked());
  QSettings().setValue("Ztoryc/BurnInShotNames", chkNames->isChecked());
  // (Clapperboard already applied live by the chkClap toggled connection.)

  // Publish the burn-in config consumed by rendercommand.cpp at render setup.
  {
    ZtoryBurnInConfig bi;
    bi.timecode  = chkTC->isChecked();
    bi.shotNames = chkNames->isChecked();
    if (bi.shotNames) {
      for (int si = 0; si < (int)m_shots.size(); si++) {
        // Shot frame range in the main xsheet (same scan as shotFrameRange,
        // which is declared below — duplicated here to keep diff local).
        TXsheet *xsh = scene->getChildStack()->getTopXsheet();
        int col = m_shots[si].data.xsheetColumn;
        int s0 = 0, s1 = 0;
        for (int r = 0; r < xsh->getFrameCount(); r++)
          if (!xsh->getCell(r, col).isEmpty()) { s0 = r; break; }
        for (int r = xsh->getFrameCount() - 1; r >= 0; r--)
          if (!xsh->getCell(r, col).isEmpty()) { s1 = r; break; }
        // Label prefix: SQ label (if any) + shot label.
        QString prefix;
        if (!m_shots[si].data.sequenceId.isEmpty()) {
          const SequenceData *seq =
              ZtoryModel::instance()->findSequence(m_shots[si].data.sequenceId);
          if (seq) prefix = seq->label + "_";
        }
        prefix += m_shots[si].data.label();
        const auto &panels = m_shots[si].data.panels;
        for (int pi = 0; pi < (int)panels.size(); pi++) {
          ZtoryBurnInSeg seg;
          seg.from  = s0 + panels[pi].startFrame;
          seg.to    = (pi + 1 < (int)panels.size())
                          ? s0 + panels[pi + 1].startFrame - 1
                          : s1;
          seg.to    = qMin(seg.to, s1);
          seg.label = prefix + "_" +
                      (panels[pi].panelLabel.isEmpty()
                           ? QString("P%1").arg(pi + 1, 3, 10, QChar('0'))
                           : panels[pi].panelLabel);
          if (seg.from <= seg.to) bi.segments.push_back(seg);
        }
      }
    }
    ZtoryModel::instance()->burnIn() = bi;
  }

  // ── Collect parameters ─────────────────────────────────���────────────────
  int mode       = btnGroup->checkedId();  // 0=full, 1=range, 2=each
  QString outDir = folderEdit->text();
  QDir().mkpath(outDir);

  // Save original output props
  TFilePath origPath = prop->getPath();
  int origR0, origR1, origStep;
  prop->getRange(origR0, origR1, origStep);

  // Helper: compute frame range [start, end) for shot si in main xsheet
  auto shotFrameRange = [&](int si) -> std::pair<int,int> {
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    int col = m_shots[si].data.xsheetColumn;
    int r0 = 0, r1 = 0;
    for (int r = 0; r < xsh->getFrameCount(); r++) {
      if (!xsh->getCell(r, col).isEmpty()) { r0 = r; break; }
    }
    for (int r = xsh->getFrameCount() - 1; r >= 0; r--) {
      if (!xsh->getCell(r, col).isEmpty()) { r1 = r; break; }
    }
    return {r0, r1};
  };

  if (mode == 0) {
    // Full animatic: from first cell to last
    auto [r0, _1] = shotFrameRange(0);
    auto [_2, r1] = shotFrameRange((int)m_shots.size() - 1);
    TFilePath outPath = TFilePath(outDir.toStdWString()) +
                        TFilePath((nameEdit->text() + "." + ext).toStdWString());
    prop->setPath(outPath);
    prop->setRange(r0, r1, 1);
    CommandManager::instance()->execute(MI_Render);

  } else if (mode == 1) {
    // Shot range
    int fromIdx = fromCombo->currentIndex();
    int toIdx   = toCombo->currentIndex();
    if (fromIdx > toIdx) std::swap(fromIdx, toIdx);
    auto [r0, _1] = shotFrameRange(fromIdx);
    auto [_2, r1] = shotFrameRange(toIdx);
    TFilePath outPath = TFilePath(outDir.toStdWString()) +
                        TFilePath((nameEdit->text() + "." + ext).toStdWString());
    prop->setPath(outPath);
    prop->setRange(r0, r1, 1);
    CommandManager::instance()->execute(MI_Render);

  } else {
    // One clip per shot — the audio range is now pinned inside MovieRenderer at
    // the time rasterRender() reads it (via movieRenderer.setAudioRange(r0,r1)
    // in rendercommand.cpp), so updating props for the next shot while a render
    // is running on background threads no longer corrupts the audio.
    for (int si = 0; si < (int)m_shots.size(); si++) {
      auto [r0, r1] = shotFrameRange(si);
      QString shotNum = m_shots[si].data.shotNumber;
      // sequenceId is a UUID — resolve to human-readable label
      QString seqPart;
      if (!m_shots[si].data.sequenceId.isEmpty()) {
        const SequenceData *seq =
            ZtoryModel::instance()->findSequence(m_shots[si].data.sequenceId);
        seqPart = (seq ? seq->label : m_shots[si].data.sequenceId) + "_";
      }
      QString fname = sceneName + "_" + seqPart + shotNum + "." + ext;
      TFilePath outPath = TFilePath(outDir.toStdWString()) +
                          TFilePath(fname.toStdWString());
      prop->setPath(outPath);
      prop->setRange(r0, r1, 1);
      CommandManager::instance()->execute(MI_Render);
    }
  }

  // Clear the burn-in config so later renders (menu Render, preview…) are
  // unaffected. Safe even with renders still running: rendercommand.cpp
  // copied it into the MovieRenderer at setup time.
  ZtoryModel::instance()->burnIn() = ZtoryBurnInConfig();

  // Restore original output properties
  prop->setPath(origPath);
  prop->setRange(origR0, origR1, origStep);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

void StoryboardPanel::onExportPdf() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export PDF"), tr("No shots to export."));
    return;
  }

  // Build a sensible default filename from the scene name.
  QString defaultPath;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene) {
    TFilePath sp = scene->getScenePath();
    QString sceneName = QString::fromStdWString(sp.getWideName());
    if (sceneName.isEmpty()) sceneName = "storyboard";
    QString dir = QString::fromStdWString(sp.getParentDir().getWideString());
    defaultPath = (dir.isEmpty() ? QDir::homePath() : dir)
                  + "/" + sceneName + "_storyboard.pdf";
  } else {
    defaultPath = QDir::homePath() + "/storyboard.pdf";
  }

  QString path = QFileDialog::getSaveFileName(
      nullptr, tr("Save Storyboard PDF"), defaultPath, tr("PDF (*.pdf)"));
  if (path.isEmpty()) return;

  QPdfWriter writer(path);
  writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
      QPageLayout::Landscape, QMarginsF(8, 8, 8, 8)));
  writer.setResolution(300);

  QPainter painter(&writer);
  const double dpi = writer.resolution();
  auto mm2px = [dpi](double mm) -> int { return (int)(mm * dpi / 25.4 + 0.5); };
  auto pt2px = [dpi](double pt) -> int { return (int)(pt * dpi / 72.0 + 0.5); };
  // FPS from scene
  int fps = ZtoryModel::instance()->fps();
  if (scene) {
    int sf = (int)std::round(
        scene->getProperties()->getOutputProperties()->getFrameRate());
    if (sf > 0) fps = sf;
  }
  auto framesToTC = [fps](int frames) -> QString {
    int ff = frames % fps, ts = frames / fps, ss = ts % 60, mm = ts / 60;
    return QString("%1:%2:%3")
        .arg(mm,2,10,QChar('0')).arg(ss,2,10,QChar('0')).arg(ff,2,10,QChar('0'));
  };

  const int pageW = writer.width();
  const int pageH = writer.height();
  const int cols  = 3;
  const int pad   = mm2px(2.0);   // inner text padding

  // Fixed bands
  const int headerH = mm2px(14.0);
  const int footerH = mm2px(6.0);

  // Grid occupies full width, no outer side margin
  const int gridY = headerH;
  const int gridH = pageH - headerH - footerH;

  // Cell dimensions — no gaps, pure grid
  const int cellW    = pageW / cols;
  // Always 1 row per page: cells fill the full grid height so thumbnails
  // sit in the top portion and text fields expand to fill the rest.
  const int rowsPerPage = 1;
  const int perPage     = cols * rowsPerPage;
  const int cellH    = gridH;  // each cell spans the full grid height
  const int subHdrH  = mm2px(6.5);
  const int imgH     = cellW * 9 / 16;
  // Remaining height after sub-header and thumbnail is shared among 3 fields.
  const int fieldsH  = cellH - subHdrH - imgH;
  const int fieldH   = fieldsH / 3;
  const int fldLblH  = pt2px(6.5);
  const int fldTxtH  = fieldH - fldLblH - mm2px(0.4);
  const int fldGap   = mm2px(0.4);

  // Build flat panel list
  struct PanelEntry { int si, pi; PanelWidget *pw; };
  std::vector<PanelEntry> allPanels;
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
      allPanels.push_back({si, pi, m_shots[si].panels[pi]});
  const int totalPanels = (int)allPanels.size();
  const int totalPages  = (totalPanels + perPage - 1) / perPage;

  // Assets
  QPixmap logoPixmap = QPixmap(":Resources/ztoryc_about.png")
      .scaled(mm2px(10), mm2px(10), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  QPixmap ftLogo = QPixmap(":Resources/ztoryc_about.png")
      .scaled(mm2px(3.5), mm2px(3.5), Qt::KeepAspectRatio, Qt::SmoothTransformation);

  // Production + Title: prefer user-set values from model; fall back to scene path.
  QString prodName  = ZtoryModel::instance()->production();
  QString titleName = ZtoryModel::instance()->title();
  if (scene && (prodName.isEmpty() || titleName.isEmpty())) {
    TFilePath sp = scene->getScenePath();
    QString sceneName  = QString::fromStdWString(sp.getWideName());
    QString parentDir  = QString::fromStdWString(sp.getParentDir().getWideName());
    if (prodName.isEmpty())  prodName  = parentDir.isEmpty() ? sceneName : parentDir;
    if (titleName.isEmpty()) titleName = sceneName;
  }

  // ── Draw header ──────────────────────────────────────────────────────────
  auto drawHeader = [&](int pageNum) {
    painter.fillRect(0, 0, pageW, headerH, Qt::white);
    // Bottom border line
    painter.setPen(QPen(Qt::black, mm2px(0.3)));
    painter.drawLine(0, headerH - 1, pageW, headerH - 1);

    // Logo
    int cx = pad;
    if (!logoPixmap.isNull()) {
      int ly = (headerH - logoPixmap.height()) / 2;
      painter.drawPixmap(cx, ly, logoPixmap);
      cx += logoPixmap.width() + mm2px(3.0);
    }

    // Divider after logo
    painter.setPen(QPen(QColor(180,180,180), mm2px(0.2)));
    painter.drawLine(cx - mm2px(1.5), mm2px(2), cx - mm2px(1.5), headerH - mm2px(2));

    // Production + Title fields (each takes ~1/3 of remaining width)
    int fieldAreaW = pageW - cx - mm2px(38);
    int secW = fieldAreaW / 2;

    auto drawHdrField = [&](int x, int w, const QString &label, const QString &value) {
      painter.setFont(QFont("Arial", 5.5));
      painter.setPen(QColor(100,100,100));
      painter.drawText(x, mm2px(1), w, headerH/2 - mm2px(1),
                       Qt::AlignBottom | Qt::AlignLeft, label);
      painter.setFont(QFont("Arial", 8, QFont::Bold));
      painter.setPen(Qt::black);
      painter.drawText(x, headerH/2, w, headerH/2 - mm2px(2),
                       Qt::AlignTop | Qt::AlignLeft, value);
      int lineY = headerH - mm2px(2);
      painter.setPen(QPen(QColor(180,180,180), mm2px(0.2)));
      painter.drawLine(x, lineY, x + w - mm2px(3), lineY);
    };
    drawHdrField(cx,        secW, tr("Production:"), prodName);
    drawHdrField(cx + secW, secW, tr("Title:"),      titleName);

    // Page box (right)
    int pbX = pageW - mm2px(36);
    int pbW = mm2px(36) - pad;
    int pbH = headerH - mm2px(3);
    int pbY = mm2px(1.5);
    painter.setPen(QPen(Qt::black, mm2px(0.25)));
    painter.drawRect(pbX, pbY, pbW, pbH);
    painter.setFont(QFont("Arial", 7));
    painter.setPen(Qt::black);
    painter.drawText(pbX, pbY, pbW, pbH, Qt::AlignVCenter | Qt::AlignHCenter,
        tr("Page  %1 / %2").arg(pageNum).arg(totalPages));
  };

  // ── Draw footer ──────────────────────────────────────────────────────────
  auto drawFooter = [&]() {
    int fy = pageH - footerH;
    painter.fillRect(0, fy, pageW, footerH, Qt::white);
    painter.setPen(QPen(QColor(200,200,200), mm2px(0.2)));
    painter.drawLine(0, fy, pageW, fy);

    painter.setFont(QFont("Arial", 5));
    QFontMetrics fm(painter.font());
    QString ftText = tr("Made with Ztoryc");
    int tW = fm.horizontalAdvance(ftText);
    int g2 = mm2px(1.5);
    int totalW = (ftLogo.isNull() ? 0 : ftLogo.width() + g2) + tW;
    int fx = (pageW - totalW) / 2;
    if (!ftLogo.isNull()) {
      painter.drawPixmap(fx, fy + (footerH - ftLogo.height())/2, ftLogo);
      fx += ftLogo.width() + g2;
    }
    painter.setPen(QColor(160,160,160));
    painter.drawText(fx, fy, tW + mm2px(1), footerH, Qt::AlignVCenter | Qt::AlignLeft, ftText);
  };

  // ── Pages ─────────────────────────────────────────────────────────────────
  TXsheet *mainXsh = scene ? scene->getChildStack()->getTopXsheet() : nullptr;

  for (int pageIdx = 0; pageIdx < totalPages; pageIdx++) {
    if (pageIdx > 0) writer.newPage();
    drawHeader(pageIdx + 1);
    drawFooter();

    int panelStart = pageIdx * perPage;
    int panelEnd   = qMin(panelStart + perPage, totalPanels);

    for (int idx = panelStart; idx < panelEnd; idx++) {
      int localIdx = idx - panelStart;
      int col      = localIdx % cols;
      int row      = localIdx / cols;

      int cx = col * cellW;
      int cy = gridY + row * cellH;

      const PanelEntry &pe = allPanels[idx];
      const ShotData   &sd = m_shots[pe.si].data;
      PanelWidget      *pw = pe.pw;

      // ── Sub-header ───────────────────────────────────────────────────────
      painter.fillRect(cx, cy, cellW, subHdrH, QColor(232, 232, 232));

      int panelFrames = sd.panels.size() > (size_t)pe.pi
                        ? sd.panels[pe.pi].duration : 0;
      int shotFrames  = sd.totalDuration();
      QString lblLeft  = QString("%1   P%2/%3")
          .arg(sd.label())
          .arg(pe.pi + 1)
          .arg((int)sd.panels.size());
      QString lblRight = QString("%1f %2   T %3")
          .arg(panelFrames)
          .arg(framesToTC(panelFrames))
          .arg(framesToTC(shotFrames));

      painter.setFont(QFont("Arial", 5.5, QFont::Bold));
      painter.setPen(Qt::black);
      painter.drawText(cx + pad, cy, cellW/2 - pad, subHdrH,
          Qt::AlignVCenter | Qt::AlignLeft, lblLeft);
      painter.setFont(QFont("Arial", 5.5));
      painter.drawText(cx + cellW/2, cy, cellW/2 - pad, subHdrH,
          Qt::AlignVCenter | Qt::AlignRight, lblRight);

      // ── Thumbnail ────────────────────────────────────────────────────────
      int thumbY = cy + subHdrH;
      {
        int col2 = sd.xsheetColumn;
        TXsheet *subXsh = nullptr;
        if (mainXsh) {
          for (int r = 0; r <= mainXsh->getFrameCount(); r++) {
            TXshCell cell = mainXsh->getCell(r, col2);
            if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
              subXsh = cell.m_level->getChildLevel()->getXsheet(); break;
            }
          }
        }
        const PanelData &pdRef = sd.panels[pe.pi];
        int frame = (pdRef.cameraMoveType != PanelData::CamNone)
                    ? pdRef.camRenderFrame
                    : (sd.panels.size() > (size_t)pe.pi ? sd.panels[pe.pi].startFrame : 0);
        QPixmap hq;
        if (subXsh) {
          CamOverlayGeom g =
              computeCamOverlayGeom(pdRef, (double)(cellW - 2) / (imgH - 2));
          if (pdRef.cameraMoveType != PanelData::CamNone && g.valid) {
            // Backed-out render covering both frames (same as the on-screen
            // thumbnail) so pans aren't cropped in the PDF.
            TStageObjectId camId =
                subXsh->getStageObjectTree()->getCurrentCameraId();
            double z  = subXsh->getZ(camId, frame);
            double zf = (1000.0 + z) / 1000.0;
            TRectD placedRect = TScale(zf) * TRectD(g.minX, g.minY, g.maxX, g.maxY);
            hq = IconGenerator::renderXsheetFrameRegion(
                subXsh, frame, TDimension(cellW - 2, imgH - 2), placedRect);
          } else {
            hq = IconGenerator::renderXsheetFrame(subXsh, frame,
                     TDimension(cellW - 2, imgH - 2));
          }
        }
        if (hq.isNull()) hq = pw->previewPixmap();
        if (!hq.isNull()) {
          // Apply camera overlay before scaling (so it's crisp at full res).
          // Labels at ~6pt to match the PDF text fields (Dialog/Action/Notes).
          if (pdRef.cameraMoveType != PanelData::CamNone) {
            int moveOrdinal = 0;
            for (int k = 0; k < pe.pi && k < (int)sd.panels.size(); k++)
              if (sd.panels[k].cameraMoveType != PanelData::CamNone) moveOrdinal++;
            applyCameraOverlay(hq, pdRef, moveOrdinal, m_showCamMoveType, pt2px(6));
          }
          if (m_showLights) ztoryApplyLightOverlay(hq, pdRef);
          QPixmap scaled = hq.scaled(cellW - 2, imgH - 2,
              Qt::KeepAspectRatio, Qt::SmoothTransformation);
          int tx = cx + (cellW - scaled.width()) / 2;
          int ty = thumbY + (imgH - scaled.height()) / 2;
          painter.drawPixmap(tx, ty, scaled);
        } else {
          painter.setPen(QPen(QColor(200, 200, 200), 1));
          painter.drawLine(cx, thumbY, cx+cellW, thumbY+imgH);
          painter.drawLine(cx+cellW, thumbY, cx, thumbY+imgH);
        }
      }

      // ── Text fields ──────────────────────────────────────────────────────
      int ty2 = thumbY + imgH;
      auto drawField = [&](const QString &label, const QString &text) {
        // Top separator (light)
        painter.setPen(QPen(QColor(200, 200, 200), mm2px(0.15)));
        painter.drawLine(cx, ty2, cx + cellW, ty2);
        painter.setFont(QFont("Arial", 6, QFont::Bold));
        painter.setPen(Qt::black);
        painter.drawText(cx + pad, ty2, cellW - 2*pad, fldLblH,
            Qt::AlignVCenter | Qt::AlignLeft, label);
        painter.setFont(QFont("Arial", 6));
        painter.drawText(cx + pad, ty2 + fldLblH, cellW - 2*pad, fldTxtH,
            Qt::AlignLeft | Qt::TextWordWrap, text);
        ty2 += fieldH;
      };
      drawField(tr("Dialogue:"),     pw->dialog());
      drawField(tr("Action Notes:"), pw->action());
      drawField(tr("Notes:"),        pw->notes());

      // ── Cell borders ─────────────────────────────────────────────────────
      // Vertical left: same shot as left neighbor -> thin gray, else thicker black
      bool sameAsLeft = (col > 0 && idx > panelStart && allPanels[idx-1].si == pe.si);
      double leftPenW = sameAsLeft ? mm2px(0.25) : mm2px(0.55);
      QColor leftCol  = sameAsLeft ? QColor(180, 180, 180) : Qt::black;

      // Outer rect (top + right + bottom always black)
      painter.setPen(QPen(Qt::black, mm2px(0.25)));
      painter.drawLine(cx,        cy,       cx+cellW,  cy);        // top
      painter.drawLine(cx+cellW,  cy,       cx+cellW,  cy+cellH);  // right
      painter.drawLine(cx,        cy+cellH, cx+cellW,  cy+cellH);  // bottom
      // Left border (shot-aware thickness + color)
      painter.setPen(QPen(leftCol, leftPenW));
      painter.drawLine(cx, cy, cx, cy+cellH);
      // Sub-header bottom separator
      painter.setPen(QPen(Qt::black, mm2px(0.25)));
      painter.drawLine(cx, cy+subHdrH, cx+cellW, cy+subHdrH);
    }
  }

  painter.end();
  QMessageBox::information(this, tr("Export PDF"),
      tr("Exported to:\n%1").arg(path));
}

class StoryboardPanelFactory final : public TPanelFactory {
public:
  StoryboardPanelFactory() : TPanelFactory("Storyboard") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new StoryboardPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Ztoryc Board"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} storyboardPanelFactory;
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
