#include "storyboardpanel.h"
#include "ztoryshotops.h"
#include "ztorylightgizmo.h"
#include "ztorydialoguehighlighter.h"
#include "ztorylipsync.h"
#include "ztoryassetimport.h"
#include <QMenu>
#include "toonz/toonzfolders.h"
#include <QProgressDialog>
#include "kitsuclient.h"  // post-export upload to Kitsu

// QXlsx (vendored, MIT) — production spreadsheet export.
#include "xlsxdocument.h"
#include "xlsxformat.h"
#include "xlsxdatavalidation.h"
#include "xlsxconditionalformatting.h"
#include "xlsxworksheet.h"
#include "xlsxcellrange.h"
#include "xlsxcellreference.h"

#include "tundo.h"

#include <QUuid>
#include <QPointer>
#include "tapp.h"
#include "outputsettingspopup.h"
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
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshmeshcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/levelset.h"
#include "tsound_io.h"
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
#include "toonzqt/dvscrollwidget.h"
#include "toonzqt/filefield.h"
#include "toonz/preferences.h"
#include <QStandardPaths>

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QShortcut>
#include <QRadioButton>
#include "iocommand.h"
#include "exportscenepopup.h"
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
#include <QInputDialog>
#include <QFileDialog>
#include <QPainter>
#include <QPdfWriter>
#include <QPageLayout>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QWindow>
#include <QFile>
#include <QTextStream>
#include <QXmlStreamWriter>
#include <QUrl>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QApplication>
#include <QLineEdit>
#include <QGroupBox>
#include <QButtonGroup>
#include <QFormLayout>
#include <QFileInfo>
#include <QDir>
#include "toutputproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/boardsettings.h"
#include "menubarcommandids.h"
#include <QColorDialog>
#include <QEventLoop>
#include <cmath>

// Definita piu' sotto, accanto a refreshSpeakersLabel(): serve al costruttore
// del campo dialogo, che viene prima.
static void ztoryOfferSpeakerAlias(QWidget *parent, QTextEdit *field,
                                   const QPoint &pos);


// Persisted number of columns in the Board grid (the spin in the toolbar).
// Stored in user env so the layout is remembered across sessions.
TEnv::IntVar ZtoryBoardColumns("ZtoryBoardColumns", 3);
// "Compact view": one card per shot (collapse panels). Persisted.
TEnv::IntVar ZtoryBoardCollapsePanels("ZtoryBoardCollapsePanels", 0);

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

  // ◀ ▶ panel navigator — only shown in the collapsed "Compact view" Board view.
  auto makeNavBtn = [this](const QString &glyph) {
    QPushButton *b = new QPushButton(glyph, this);
    b->setFixedSize(16, 16);
    b->setFocusPolicy(Qt::NoFocus);
    b->setStyleSheet(
        "QPushButton{background:#444;color:#ddd;border-radius:2px;font-size:10px;"
        "padding:0;}QPushButton:hover{background:#666;}"
        "QPushButton:disabled{color:#666;}");
    b->hide();
    return b;
  };
  m_prevPanelBtn = makeNavBtn(QString::fromUtf8("◀"));  // ◀
  m_nextPanelBtn = makeNavBtn(QString::fromUtf8("▶"));  // ▶
  connect(m_prevPanelBtn, &QPushButton::clicked, this,
          [this]() { emit panelNavRequested(m_shotIndex, -1); });
  connect(m_nextPanelBtn, &QPushButton::clicked, this,
          [this]() { emit panelNavRequested(m_shotIndex, +1); });

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
  hl->addWidget(m_prevPanelBtn);
  hl->addWidget(m_panelLabel);
  hl->addWidget(m_nextPanelBtn);
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
  // Chi parla si ricava dal testo, come in sceneggiatura. E' una convenzione,
  // quindi deve VEDERSI anche qui: il Board e' il posto dove lo script si
  // incolla davvero, e una convenzione che fallisce in silenzio e' peggio di un
  // campo in piu'. Stessa etichetta dello Shot Board.
  // Il nome si colora dove sta, dentro il campo.
  new ZtoryDialogueHighlighter(m_dialogField->document());
  // Tasto destro sul campo: se c'e' una selezione, si puo' forzarla su un
  // personaggio. Il menu standard (taglia/copia/incolla) resta.
  m_dialogField->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_dialogField, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &p) {
            ztoryOfferSpeakerAlias(this, m_dialogField, p);
            refreshSpeakersLabel();
          });
  m_speakersLabel = new QLabel(this);
  m_speakersLabel->setWordWrap(true);
  m_speakersLabel->setStyleSheet("font-size:10px;");
  m_speakersLabel->hide();
  layout->addWidget(m_speakersLabel);

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
  connect(m_dialogField, &QTextEdit::textChanged, [this]() {
    refreshSpeakersLabel();
    emit dataChanged(m_shotIndex, m_panelIndex);
  });
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
  // Base look only, and set ONCE (this runs from the constructor). The
  // selection highlight is painted in paintEvent instead of swapping the
  // stylesheet: setStyleSheet() reparses the CSS and re-polishes this widget
  // plus every child (preview label, three text fields, spin box…), with Qt's
  // selector matching splitting strings per rule per node. Doing that per
  // selection change dominated the profile — clicking clips in the Animatic
  // timeline spent ~68% of its time inside QCss::StyleSelector.
  setStyleSheet("PanelWidget{background:#2b2b2b;border:1px solid #555;border-radius:3px;}"
                "PanelWidget:hover{border:1px solid #888;}");
}

void PanelWidget::paintEvent(QPaintEvent *e) {
  QWidget::paintEvent(e);  // stylesheet background/border first
  if (!m_selected) return;

  // Selection highlight drawn over the base border: a 2px inset frame in the
  // Ztoryc orange, matching the old box-shadow look.
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor(0xe0, 0x5a, 0x00), 2.0);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), 3.0, 3.0);
}

void PanelWidget::rescalePreview() {
  int w = width() - 8;
  if (w <= 0) w = 150;
  double aspect =
      ZtoryShotOps::cameraAspect(TApp::instance()->getCurrentScene()->getScene());
  int h = qMax(1, qRound(w / aspect));
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

void PanelWidget::setPanelNavigator(bool enabled, int total) {
  // Show the arrows only in collapsed view when the shot has more than one
  // panel; disable each arrow at the ends.
  bool show = enabled && total > 1;
  m_prevPanelBtn->setVisible(show);
  m_nextPanelBtn->setVisible(show);
  if (show) {
    m_prevPanelBtn->setEnabled(m_panelIndex > 0);
    m_nextPanelBtn->setEnabled(m_panelIndex < total - 1);
  }
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
  if (m_selected == sel) return;  // no-op changes must not cost a repaint
  m_selected = sel;
  update();  // highlight lives in paintEvent — no stylesheet work
}

void PanelWidget::setDialog(const QString &t) {
  m_dialogField->blockSignals(true);
  m_dialogField->setPlainText(t);
  m_dialogField->blockSignals(false);
  refreshSpeakersLabel();
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

//-----------------------------------------------------------------------------
// «Seleziona il nome e forzalo su un personaggio» (richiesta di Franco,
// 2026-08-15). Negli script i nomi non coincidono mai del tutto con quelli del
// tracker — «PRINCIPESSA» nel copione, «PRINCENERENTOLA» fra gli asset — e le
// due alternative sarebbero correggere il copione o rinominare l'asset: due
// cose che non si fanno per far contento un riconoscimento.
// L'alias vale per TUTTO il progetto: se e' quel personaggio qui, lo e' anche
// negli altri quaranta pannelli.
static void ztoryOfferSpeakerAlias(QWidget *parent, QTextEdit *field,
                                   const QPoint &pos) {
  ZtoryModel *m = ZtoryModel::instance();
  QMenu *menu = field->createStandardContextMenu();
  const QString sel = field->textCursor().selectedText().trimmed();

  if (!sel.isEmpty() && sel.length() <= 40) {
    menu->addSeparator();
    QMenu *sub = menu->addMenu(
        QObject::tr("Treat «%1» as character").arg(sel));
    QHash<QAction *, QString> byAction;
    const QString curAlias = m->speakerAlias(sel);
    for (const Asset &a : m->assets()) {
      if (a.type.compare("Character", Qt::CaseInsensitive) != 0) continue;
      QAction *act = sub->addAction(a.name);
      act->setCheckable(true);
      act->setChecked(!curAlias.isEmpty() && curAlias == a.uuid);
      byAction.insert(act, a.uuid);
    }
    QAction *clearAct = nullptr;
    if (!curAlias.isEmpty()) {
      sub->addSeparator();
      clearAct = sub->addAction(QObject::tr("Remove this alias"));
    }
    if (sub->isEmpty()) sub->setEnabled(false);

    QAction *chosen = menu->exec(field->viewport()->mapToGlobal(pos));
    if (chosen) {
      if (chosen == clearAct) {
        m->setSpeakerAlias(sel, QString());
        m->saveProjectDb();
      } else if (byAction.contains(chosen)) {
        m->setSpeakerAlias(sel, byAction.value(chosen));
        m->saveProjectDb();
      }
      // Ridisegna: l'evidenziazione deve cambiare colore SUBITO, altrimenti
      // non si capisce se l'assegnazione ha avuto effetto.
      field->document()->markContentsDirty(0, field->toPlainText().length());
    }
    delete menu;
    return;
  }
  menu->exec(field->viewport()->mapToGlobal(pos));
  delete menu;
}

void PanelWidget::refreshSpeakersLabel() {
  if (!m_speakersLabel) return;
  // Stessa regola dell'evidenziatore: se lui tace, tace anche l'avviso.
  if (!ZtoryDialogueHighlighter::enabled()) { m_speakersLabel->hide(); return; }
  ZtoryModel *m = ZtoryModel::instance();
  QStringList known, unknown;
  for (const DialogueLine &dl : m->parseDialogue(m_dialogField->toPlainText())) {
    if (dl.character.isEmpty()) continue;
    if (dl.matched) { if (!known.contains(dl.character)) known << dl.character; }
    else if (!unknown.contains(dl.character)) unknown << dl.character;
  }
  Q_UNUSED(known);
  // I riconosciuti NON si elencano: si vedono gia' in verde nel testo, e una
  // riga in piu' faceva ballare l'altezza del pannello a ogni ridisegno.
  // Resta l'avviso per i soli non riconosciuti — l'unico caso su cui agire, e
  // che in un campo lungo e scrollato resterebbe fuori vista.
  if (unknown.isEmpty()) { m_speakersLabel->hide(); return; }
  m_speakersLabel->setText(
      QString("<span style='color:#F5A623;'>%1</span>")
          .arg(tr("Not a character: %1").arg(unknown.join(", "))));
  m_speakersLabel->show();
}
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
      double dAspect =
          ZtoryShotOps::cameraAspect(TApp::instance()->getCurrentScene()->getScene());
      int dragW = 160, dragH = qMax(1, qRound(dragW / dAspect));
      drag->setPixmap(pm.scaled(dragW, dragH, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation));
      drag->setHotSpot(QPoint(dragW / 2, dragH / 2));
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
    , m_comboViewer(nullptr)
{
  setObjectName("StoryboardPanel");
  setFocusPolicy(Qt::StrongFocus);
  m_collapsePanels = ((int)ZtoryBoardCollapsePanels != 0);  // restore "Compact view"

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

  // Quanti pannelli al secondo puo' generare un livello in ANIMAZIONE PIENA.
  // Sui livelli che cambiano piu' lentamente ogni cambio di disegno fa un
  // pannello come sempre: questo tetto riguarda solo chi cambia a ogni
  // fotogramma, dove «un pannello per disegno» ne farebbe uno per fotogramma.
  QLabel *ppsLabel = new QLabel(tr("Panels/s:"));
  ppsLabel->setStyleSheet("color:#ccc;font-size:11px;");
  ppsLabel->setToolTip(
      tr("Maximum panels per second generated by fully animated levels.\n"
         "Drawing changes on slower levels, column keys and camera moves\n"
         "always create a panel."));
  m_maxPanelsPerSecSpin = new QSpinBox();
  m_maxPanelsPerSecSpin->setRange(1, 24);
  m_maxPanelsPerSecSpin->setValue(
      QSettings().value("Ztoryc/MaxPanelsPerSecond", 1).toInt());
  m_maxPanelsPerSecSpin->setFixedWidth(45);
  m_maxPanelsPerSecSpin->setToolTip(ppsLabel->toolTip());
  m_maxPanelsPerSecSpin->setStyleSheet("background:#333;color:#ddd;border:1px solid #555;");

  // "Compact view" toggle: one card per shot (collapse panels), with ◀ ▶ on the
  // card to flip panels in place — keeps the Board light on animated scenes.
  m_collapsePanelsButton = new QToolButton();
  m_collapsePanelsButton->setIcon(createQIcon("ztoryc_compact_view"));
  m_collapsePanelsButton->setIconSize(QSize(18, 18));
  m_collapsePanelsButton->setCheckable(true);
  m_collapsePanelsButton->setChecked(m_collapsePanels);
  m_collapsePanelsButton->setFixedSize(28, 28);
  m_collapsePanelsButton->setToolTip(
      tr("Compact view: show one card per shot (the current panel); use the "
         "◀ ▶ arrows on the card to flip through the shot's panels"));
  m_collapsePanelsButton->setStyleSheet(
      "QToolButton{background:transparent;border:none;border-radius:4px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#3a6ea5;}");
  connect(m_collapsePanelsButton, &QToolButton::toggled, this,
          &StoryboardPanel::onToggleCollapsePanels);

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

    m_exportSpreadsheetButton = new QToolButton();
  m_exportSpreadsheetButton->setIcon(createQIcon("ztoryc_export_spreadsheet"));
  m_exportSpreadsheetButton->setIconSize(QSize(20, 20));
  m_exportSpreadsheetButton->setFixedSize(28, 28);
  m_exportSpreadsheetButton->setToolTip(tr("Export Storyboard Spreadsheet (XLSX)"));
  m_exportSpreadsheetButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_techniqueButton = new QToolButton();
  m_techniqueButton->setIcon(createQIcon("ztoryc_technique"));
  m_techniqueButton->setIconSize(QSize(20, 20));
  m_techniqueButton->setFixedSize(28, 28);
  m_techniqueButton->setToolTip(tr("Asset breakdown tagging (coming soon)"));
  // Workflow is now set per-shot in the Production Tracker (Shots tab). The
  // button is kept (disabled) as the future home for asset breakdown tagging.
  m_techniqueButton->setEnabled(false);
  m_techniqueButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

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
  m_camLabelButton->setIcon(createQIcon("ztoryc_cam_moves"));
  m_camLabelButton->setIconSize(QSize(18, 18));
  m_camLabelButton->setCheckable(true);
  m_camLabelButton->setChecked(m_showCamMoveType);
  m_camLabelButton->setFixedSize(28, 28);
  m_camLabelButton->setToolTip(tr("Show camera-move labels (Trk In, Pan…)"));
  m_camLabelButton->setStyleSheet(
      "QToolButton{background:transparent;border:none;border-radius:4px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#666;}");

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
  m_lightShowButton->setIcon(createQIcon("ztoryc_light_arrow"));
  m_lightShowButton->setIconSize(QSize(18, 18));
  m_lightShowButton->setCheckable(true);
  m_lightShowButton->setChecked(m_showLights);
  m_lightShowButton->setFixedSize(28, 28);
  m_lightShowButton->setToolTip(tr("Show light-direction arrows (L)"));
  m_lightShowButton->setStyleSheet(
      "QToolButton{background:transparent;border:none;border-radius:4px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#666;}");

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
  tb->addSpacing(8);
  tb->addWidget(ppsLabel);
  tb->addWidget(m_maxPanelsPerSecSpin);
  tb->addSpacing(8);
  tb->addWidget(m_collapsePanelsButton);
  // No stretch here: the toolbar is wrapped in a DvScrollWidget below, so its
  // natural content width must drive the overflow scroll arrows. A stretch
  // would absorb all extra space and the bar would never report an overflow.
  tb->addSpacing(12);
  tb->addWidget(m_lightEditButton);
  tb->addWidget(m_lightColorButton);
  tb->addWidget(m_lightShowButton);
  tb->addSpacing(8);
  tb->addWidget(m_camLabelButton);
  tb->addSpacing(8);
  tb->addWidget(m_techniqueButton);
  tb->addWidget(m_exportPdfButton);
  tb->addWidget(m_exportSpreadsheetButton);
  tb->addWidget(m_exportShotsButton);
  tb->addWidget(m_exportAnimaticButton);

  // Overflow: wrap the toolbar in Tahoma's DvScrollWidget so a narrow Board
  // shows side scroll arrows instead of clipping / stacking icons — same UX as
  // the native panels' toolbars. (Not a QToolBar: keeps our QHBoxLayout + the
  // runtime setVisible dedup intact.)
  tb->setContentsMargins(2, 0, 2, 0);
  QWidget *tbWidget = new QWidget();
  tbWidget->setLayout(tb);
  DvScrollWidget *tbScroll = new DvScrollWidget();
  tbScroll->setWidget(tbWidget);
  mainLayout->addWidget(tbScroll);

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
    // If a stroke is in progress (mouse/stylus button down), the synchronous
    // detect + thumbnail render would stall the UI thread mid-stroke and
    // corrupt the line (task 49). Requeue and try again after the release.
    if (QApplication::mouseButtons() != Qt::NoButton) {
      m_panelDetectTimer->start();
      return;
    }
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
  connect(m_exportSpreadsheetButton, &QToolButton::clicked, this, &StoryboardPanel::onExportSpreadsheet);
  connect(m_techniqueButton, &QToolButton::clicked, this, &StoryboardPanel::onSetTechnique);
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
    emit ZtoryModel::instance()->overlayDisplayChanged();
  });
  // Mirror overlay-display changes made from other panels (Shot Board):
  // re-read QSettings, realign button states (blocked: no echo) and re-bake
  // only when something actually differs from our current state.
  connect(ZtoryModel::instance(), &ZtoryModel::overlayDisplayChanged, this,
          [this]() {
    bool showLights = QSettings().value("Ztoryc/ShowLightDirection", true).toBool();
    bool showCam    = QSettings().value("Ztoryc/ShowCamMoveType", true).toBool();
    QString color   = QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
    PanelWidget::setLightColor(color);
    m_lightColorButton->setStyleSheet(
        QString("QToolButton{background:%1;border:1px solid #555;"
                "border-radius:4px;margin:6px;}"
                "QToolButton:hover{border:1px solid #aaa;}").arg(color));
    if (showLights == m_showLights && showCam == m_showCamMoveType) return;
    m_showLights      = showLights;
    m_showCamMoveType = showCam;
    m_lightShowButton->blockSignals(true);
    m_lightShowButton->setChecked(showLights);
    m_lightShowButton->blockSignals(false);
    m_camLabelButton->blockSignals(true);
    m_camLabelButton->setChecked(showCam);
    m_camLabelButton->blockSignals(false);
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
    emit ZtoryModel::instance()->overlayDisplayChanged();
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
    emit ZtoryModel::instance()->overlayDisplayChanged();
  });
  connect(m_columnsPerRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StoryboardPanel::onColumnsChanged);
  connect(m_maxPanelsPerSecSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int v) {
    QSettings().setValue("Ztoryc/MaxPanelsPerSecond", v);
    // Il tetto cambia i CONFINI, quindi vanno rifatti i pannelli di tutti gli
    // shot, non solo ridisegnate le anteprime.
    for (int si = 0; si < (int)m_shots.size(); si++) detectAndUpdatePanels(si);
    updateVisiblePreviews();
  });
  connect(m_numberingCombo, QOverload<int>::of(&QComboBox::activated),
          this, &StoryboardPanel::onNumberingChanged);
  connect(m_numberingBtn, &QToolButton::clicked,
          this, &StoryboardPanel::onNumberingConfig);
  updateNumberingLock();  // freeze numbering if this project already has Kitsu shots
  // Re-apply the lock whenever the project DB (re)loads — that's when the Kitsu
  // shot ids become known (e.g. right after a push from the Connect dialog).
  connect(ZtoryModel::instance(), &ZtoryModel::productionReloaded, this,
          &StoryboardPanel::updateNumberingLock);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this]() {
            // Scena nuova: i render della precedente non servono piu' a nessuno.
            ZtoryModel::instance()->clearPanelRenderCache();
            refreshFromScene();
          });
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
  // ⚠️ IMPORTARE qualcosa in uno shot non cambia fotogramma, quindi il
  // riquadro restava quello di prima: due personaggi importati come
  // sotto-scene non comparivano nell'anteprima finche' non si cambiava
  // workflow, che rifa' tutto (Franco, 2026-08-17).
  //
  // Si aggancia `castChanged` — che scatta quando il cast della scena cambia,
  // cioe' proprio quando arriva o se ne va un livello — e NON `xsheetChanged`,
  // che AGENTS.md vieta apposta: quello scatta a ogni pennellata e il render
  // sincrono spezzava la linea a meta' tratto.
  //
  // Costa poco perche' il render e' in cache con una chiave che contiene il
  // CONTENUTO: se non e' cambiato niente, la richiesta si risolve in una
  // lettura di hash e non in un render.
  // Rifare le anteprime del Board dal LIVELLO PRINCIPALE, che e' da dove il
  // Board si guarda.
  auto refreshPreviewsFromMain = [this]() {
    // ⚠️ Il timer sopra ESCE SUBITO se non si e' dentro una sotto-scena
    // (`getAncestorCount() == 0`): serve l'informazione dell'antenato per
    // sapere QUALE shot e' sporco. Ma il Board lo si guarda dal livello
    // principale, quindi agganciarci e basta voleva dire non fare niente.
    //
    // Dal livello principale non si sa quale shot sia cambiato — e non serve
    // saperlo: si buttano le pixmap di quelli VISIBILI e si lascia che
    // updateVisiblePreviews() li rifaccia. Costa poco perche' il render e' in
    // cache con una chiave che contiene il contenuto: cio' che non e' cambiato
    // si risolve in una lettura di hash.
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    if (sc && sc->getChildStack()->getAncestorCount() > 0) {
      m_panelDetectTimer->start();
      return;
    }
    for (auto &shot : m_shots)
      for (PanelWidget *pw : shot.panels)
        if (pw) pw->setPreviewPixmap(QPixmap());
    updateVisiblePreviews();
  };
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::castChanged, this,
          refreshPreviewsFromMain);
  // L'occhietto di una colonna non cambia il CAST: la sua notifica e'
  // xsheetChanged. Agganciarla qui non viola la regola di AGENTS.md — quella
  // vieta di rifare le miniature a ogni pennellata DENTRO una sotto-scena, e
  // questo ramo gira solo al livello principale, dove non si disegna.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this, refreshPreviewsFromMain]() {
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    if (sc && sc->getChildStack()->getAncestorCount() == 0)
      refreshPreviewsFromMain();
  });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this](){
    disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
               this, nullptr);
    connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
            this, &StoryboardPanel::onXsheetChanged);
    // While inside a sub-scene, each xsheet change (drawing, erasing) only
    // marks the shot dirty — NO detect-timer restart (task 49): the 1s timer
    // fired mid-stroke and the synchronous detect+render stalled the line.
    // Detect runs on frameSwitched, on return to the Board, and in showEvent
    // (m_dirtyShotCol) — every moment a stale thumbnail could actually be seen.
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    if (sc && sc->getChildStack()->getAncestorCount() > 0) {
      connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
              this, [this, sc](){
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
      // refresh forzato al ritorno nel Board (showEvent + m_dirtyShotCol).
      // NON avviare updatePreview subito: renderXsheetFrame è sincrono e
      // blocca l'UI thread mentre l'utente disegna i primi tratti (task 49).
      // Il m_panelDetectTimer (1000ms) parte già da frameSwitched e aggiornerà
      // il thumbnail dopo la prima pausa dal disegno.
      AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
      if (node) {
        m_dirtyShotCol = node->m_col;
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
    {m_exportSpreadsheetButton, "Export a storyboard spreadsheet (.xlsx): one row per shot in THIS storyboard with thumbnail, timing, technique and per-task Kitsu status. For the whole project (all storyboards + all tabs) use Export Project Spreadsheet in the Production Tracker"},
    {m_techniqueButton,      "Assign a production technique (Tradigital, Cut-out, 3D…) to the selected shot(s); it drives which tasks apply in the spreadsheet"},
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
  // Guard: a shot with no panels (or an out-of-range panelIdx) made
  // panels[panelIdx].duration dereference a null vector buffer → EXC_BAD_ACCESS
  // (observed on Add Shot from a second/floating Board panel whose local m_shots
  // had drifted out of sync). Bail instead of crashing.
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size() || panelIdx < 0 ||
      panelIdx >= (int)m_shots[shotIdx].data.panels.size())
    return;
  Shot &shot = m_shots[shotIdx];
  // Built DETACHED, then reparented once at the end of this function.
  //
  // A layout reparents every child it receives, and QWidget::setParent runs
  // reparentFocusWidgets, which walks the focus chain of the whole WINDOW. With
  // m_container already in the window, that chain holds every panel already on
  // the board, so each of this panel's ~8 children (three of them QTextEdit)
  // paid an O(total widgets) walk — panel creation was quadratic in the number
  // of panels. Profiling Add Shot on a heavy storyboard put the bulk of the
  // time in reparentFocusWidgets; building detached and joining the hierarchy
  // once took that operation from ~20s to ~12s.
  PanelWidget *pw = new PanelWidget(nullptr);
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
  // Join the hierarchy now that the panel is fully built: ONE focus-chain walk
  // for the whole subtree instead of one per child. m_container takes ownership
  // (so a panel never leaks if rebuildGrid does not place it), and the widget
  // stays hidden until rebuildGrid() adds it to the layout and calls show().
  pw->setParent(m_container);
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
  connect(pw, &PanelWidget::panelNavRequested, this,
          &StoryboardPanel::onPanelNavRequested);
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
    if (model->autoRenumber() && cfg.resetOnSeqChange)
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
    if (model->autoRenumber()) {
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
    if (m_collapsePanels) {
      // "Compact view" view: a single card per shot showing the current panel.
      if (shot.panels.empty()) continue;
      int total = (int)shot.panels.size();
      shot.viewPanel = qBound(0, shot.viewPanel, total - 1);
      for (int pi = 0; pi < total; pi++) {
        PanelWidget *pw = shot.panels[pi];
        if (pi == shot.viewPanel) {
          m_grid->addWidget(pw, row, col);
          pw->setPanelNavigator(true, total);
          pw->show();
          pw->updateGeometry();
        } else {
          pw->setPanelNavigator(false, total);
          pw->hide();
        }
      }
      col++;
      if (col >= m_columnsPerRow) { col = 0; row++; }
    } else {
      for (PanelWidget *pw : shot.panels) {
        pw->setPanelNavigator(false, (int)shot.panels.size());
        m_grid->addWidget(pw, row, col);
        pw->show();
        pw->updateGeometry();
        col++;
        if (col >= m_columnsPerRow) { col = 0; row++; }
      }
    }
  }
  m_container->adjustSize();
  QTimer::singleShot(200, this, [this](){
    applyPanelWidths();
  });
}

void StoryboardPanel::applyPanelWidths() {
  if (m_shots.empty() || !m_scrollArea) return;
  const int available =
      m_scrollArea->viewport()->width() - 8 * (m_columnsPerRow + 1);
  const int colW = qMax(150, available / m_columnsPerRow);
  for (Shot &shot : m_shots)
    for (PanelWidget *pw : shot.panels) {
      // isHidden(), NOT !isVisible(): the test must catch only the panels we
      // deliberately hid (the collapsed-away ones in compact view). isVisible()
      // is also false for every panel while the Board sits in another room, so
      // a shot created from the Thumbnail room was skipped here and kept its
      // natural size — visibly narrower than the rest until a resize.
      if (pw->isHidden()) continue;
      pw->setFixedWidth(colW);
      pw->rescalePreview();
    }
  m_container->adjustSize();
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
  // Force any pending grid relayout to be applied NOW. After an interactive shot
  // op the panels are rebuilt and a LayoutRequest is posted; if this runs before
  // that request is processed (e.g. the op originated from the Animatic, so the
  // Board got no paint/layout cycle of its own) mapTo() would return stale
  // positions and the viewport test would render the wrong panels — or none.
  if (m_grid) m_grid->activate();
  QRect vpRect = m_scrollArea->viewport()->rect();

  // Collect what needs rendering, then hand it to the pump: each preview is a
  // synchronous scene render on THIS thread, so rendering the whole visible set
  // in one loop locks the window for the duration (on a slow machine long
  // enough for Windows to grey it out as "not responding"). Same total work,
  // spread one per event-loop turn.
  m_pendingPreviews.clear();
  for (int si = 0; si < (int)m_shots.size(); si++) {
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++) {
      PanelWidget *pw = m_shots[si].panels[pi];
      if (!pw) continue;
      // In "Compact view" view the non-current panels are hidden — never render
      // them (and their geometry would be stale anyway).
      if (!pw->isVisible()) continue;
      // Skip if already has a thumbnail — re-render only on explicit request
      // (window resize → previewRerenderNeeded) or manual refresh.
      if (!pw->previewPixmap().isNull()) continue;
      // Map the panel widget rect to viewport coordinates.
      QRect widgetRect = QRect(pw->mapTo(m_scrollArea->viewport(),
                                         QPoint(0, 0)), pw->size());
      if (vpRect.intersects(widgetRect))
        m_pendingPreviews.push_back(std::make_pair(si, pi));
    }
  }

  if (!m_pendingPreviews.empty() && !m_previewPumpArmed) {
    m_previewPumpArmed = true;
    QTimer::singleShot(0, this, [this]() { pumpPendingPreviews(); });
  }
}

//-----------------------------------------------------------------------------

void StoryboardPanel::pumpPendingPreviews() {
  m_previewPumpArmed = false;
  if (m_pendingPreviews.empty()) return;

  const std::pair<int, int> job = m_pendingPreviews.front();
  m_pendingPreviews.pop_front();

  // Re-validate at pop time. A queue entry is only a pair of indices, and the
  // grid may have been rebuilt (shot added, panel deleted, view toggled) since
  // it was queued — so a stale entry must be dropped, not trusted. This is what
  // lets updateVisiblePreviews() simply refill the queue without any explicit
  // cancellation handshake.
  const int si = job.first, pi = job.second;
  if (si >= 0 && si < (int)m_shots.size() &&
      pi >= 0 && pi < (int)m_shots[si].panels.size()) {
    PanelWidget *pw = m_shots[si].panels[pi];
    if (pw && pw->isVisible() && pw->previewPixmap().isNull())
      updatePreview(si, pi);
  }

  if (!m_pendingPreviews.empty()) {
    m_previewPumpArmed = true;
    QTimer::singleShot(0, this, [this]() { pumpPendingPreviews(); });
  }
}

// Forward declarations — implementations just before detectAndUpdatePanels()
static void computeCameraMove(TXsheet *, PanelData &, int, ToonzScene *);
static void classifyCameraMove(PanelData &);
static void applyCameraOverlay(QPixmap &, const PanelData &, int panelIdx,
                               bool showTypeLabel = true, double labelPxSize = 0);

// Quanti livelli di sotto-scena si scendono cercando il disegno vero. Tre
// bastano (shot → personaggio → gruppo) e mettono un tetto al costo.
static const int kZtoryExposedDepth = 3;

// Firma del disegno che una colonna ESPONE DAVVERO a una data riga.
//
// Per una colonna normale e' la cella. Per una colonna che espone una
// sotto-scena non lo e': quella avanza di una riga per riga per definizione —
// espone il fotogramma 1, poi il 2, poi il 3 — quindi «la cella e' cambiata»
// sarebbe vero sempre, e uno shot da 151 fotogrammi produceva 151 pannelli.
// Si scende dentro e si compone la firma di cio' che si vede: un rig che tiene
// la posa per dodici fotogrammi resta lo stesso disegno anche mentre la
// sotto-scena scorre. (Regola decisa con Franco il 2026-08-18.)
static QString ztoryExposedSignature(TXsheet *xsh, int row, int col,
                                     int depth) {
  if (!xsh) return QString("-");
  const TXshCell cell = xsh->getCell(row, col);
  if (cell.isEmpty() || !cell.m_level) return QString("-");

  TXshChildLevel *cl = cell.m_level->getChildLevel();
  if (!cl || depth <= 0)
    return QString::fromStdWString(cell.m_level->getName()) + ":" +
           QString::number(cell.m_frameId.getNumber());

  TXsheet *sub = cl->getXsheet();
  if (!sub) return QString("-");

  // frame 1 vale la riga 0, come ovunque nello xsheet
  const int subRow = cell.m_frameId.getNumber() - 1;
  QString sig;
  for (int c = 0; c < sub->getColumnCount(); c++) {
    TXshColumn *sc = sub->getColumn(c);
    if (!sc || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
    sig += "|" + ztoryExposedSignature(sub, subRow, c, depth - 1);
  }
  return sig;
}

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

// Shared Board/Shot Board panel render: backed-out camera-move framing +
// classic overlay. Implemented here because computeCamOverlayGeom and
// applyCameraOverlay live in this file; declared in ztorylightgizmo.h.
QPixmap ztoryRenderPanelPreview(TXsheet *subXsh, const PanelData &pd,
                                int physW, int physH, int moveOrdinal,
                                bool showCamLabel, double labelPxSize,
                                const QString &subSceneName) {
  if (!subXsh || physW <= 0 || physH <= 0) return QPixmap();
  int frame = (pd.cameraMoveType != PanelData::CamNone) ? pd.camRenderFrame
                                                        : pd.startFrame;
  QPixmap px;
  CamOverlayGeom g = computeCamOverlayGeom(pd, (double)physW / physH);

  // Chiave della cache condivisa: tutto cio' che cambia il RENDER (non
  // l'overlay, che riapplichiamo sotto). Senza nome di sotto-scena non si puo'
  // identificare nulla in modo stabile → si renderizza e basta.
  QString cacheKey;
  if (!subSceneName.isEmpty()) {
    // ⚠️ NELLA CHIAVE DEVE ENTRARE ANCHE IL CONTENUTO, non solo nome, fotogramma
    // e dimensioni. Senza, importare qualcosa nella sotto-scena — due
    // personaggi, per dire — non cambia la chiave: torna il render di prima e
    // l'anteprima nel Board resta senza i personaggi. Si vedeva comparire solo
    // dove nasceva un pannello NUOVO, perche' li' cambiava il fotogramma
    // (Franco, 2026-08-17).
    //
    // La firma e' cio' che determina l'immagine: quante colonne ci sono e cosa
    // espone ciascuna a QUEL fotogramma. Costa una passata sulle colonne — una
    // manciata — e vale quanto una scansione dell'intera sotto-scena.
    QString sig = QString::number(subXsh->getColumnCount());
    for (int c = 0; c < subXsh->getColumnCount(); c++) {
      // ⚠️ ANCHE L'OCCHIETTO nella firma: accendere o spegnere una colonna non
      // tocca le celle, quindi senza questo la chiave resta identica e torna
      // l'immagine di prima. Accendendo lo schizzo si continuava a vedere
      // l'anteprima vecchia (Franco, 2026-08-17).
      TXshColumn *col = subXsh->getColumn(c);
      sig += (col && col->isCamstandVisible()) ? "|v" : "|x";
      const TXshCell cc = subXsh->getCell(frame, c);
      if (cc.isEmpty() || !cc.m_level) { sig += "-"; continue; }
      sig += QString::fromStdWString(cc.m_level->getName()) + ":" +
             QString::number(cc.m_frameId.getNumber());
    }
    cacheKey = QString("%1|%2|%3x%4|%5").arg(subSceneName).arg(frame)
                   .arg(physW).arg(physH).arg(sig);
    if (g.valid)  // la regione di camera cambia l'inquadratura renderizzata
      cacheKey += QString("|r%1,%2,%3,%4")
                      .arg(g.minX, 0, 'f', 3).arg(g.minY, 0, 'f', 3)
                      .arg(g.maxX, 0, 'f', 3).arg(g.maxY, 0, 'f', 3);
    QPixmap hit = ZtoryModel::instance()->cachedPanelRender(cacheKey);
    if (!hit.isNull()) {
      // COPIA: applyCameraOverlay dipinge sopra, e la pixmap in cache non deve
      // essere toccata — la stessa istanza la useranno gli altri pannelli.
      px = hit.copy();
      if (pd.cameraMoveType != PanelData::CamNone)
        applyCameraOverlay(px, pd, moveOrdinal, showCamLabel, labelPxSize);
      return px;
    }
  }

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
    px = IconGenerator::renderXsheetFrame(subXsh, frame,
                                          TDimension(physW, physH));
  }
  // In cache va il render NUDO, prima dell'overlay: l'overlay dipende da cose
  // che non stanno nella chiave (ordinale della lettera, etichette) ed e'
  // economico da rifare.
  if (!cacheKey.isEmpty() && !px.isNull())
    ZtoryModel::instance()->cachePanelRender(cacheKey, px);

  if (!px.isNull() && pd.cameraMoveType != PanelData::CamNone) {
    px = px.copy();  // non dipingere sulla pixmap appena messa in cache
    applyCameraOverlay(px, pd, moveOrdinal, showCamLabel, labelPxSize);
  }
  return px;
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
  // Size the raster on the SUB-xsheet camera (the one renderFrame fits into
  // the raster): a main/sub camera mismatch — divergent cameras in a reloaded
  // scene, or the sync running after this render — would otherwise bake gray
  // letterbox bands (bg color) into the thumbnail.
  int physH =
      qMax(1, qRound(physW / ZtoryShotOps::xsheetCameraAspect(subXsh)));

  // Letter index = how many camera-move panels precede this one in the shot,
  // so the first MOVE is A→B (panels without a move don't consume letters).
  int moveOrdinal = 0;
  for (int k = 0; k < panelIdx && k < (int)shot.data.panels.size(); k++)
    if (shot.data.panels[k].cameraMoveType != PanelData::CamNone) moveOrdinal++;
  QPixmap px = ztoryRenderPanelPreview(subXsh, pd, physW, physH, moveOrdinal,
                                       m_showCamMoveType, 0,
                                       QString::fromStdWString(cl->getName()));
  if (!px.isNull()) {
    if (m_showLights) ztoryApplyLightOverlay(px, pd);
    shot.panels[panelIdx]->setPreviewPixmap(px);
    // Keep Production Tracker thumbnail cache warm: panel 0 is the shot thumbnail.
    if (panelIdx == 0 && !shot.data.uuid.isEmpty())
      ZtoryModel::instance()->updateThumbCache(shot.data.uuid, px);
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

// ── Production-tracking bridge ──────────────────────────────────────────────
// ZtoryModel is the authoritative store for the fields the Production Tracker
// edits (uuid, technique, tasks). The Board's m_shots copy is only the .ztoryc
// serialization buffer; we sync model⇄Board at the load/save/export boundaries
// so panel edits round-trip (otherwise the tracker writes the model while
// save/export read the Board copy → edits silently lost).

// Generate a UUID namespaced to the storyboard source file.
// UUID v5 (SHA1-based): deterministic from (namespace, seed) pair.
// Namespace = v5(root, sourceFilename) — unique per storyboard file.
// Seed      = a fresh random UUID — unique per shot within that file.
// Result    = UUID that is unique globally AND tied to its storyboard.
// Even if two storyboards share the same random seed (due to file copy),
// different namespaces yield different final UUIDs.
static QString makeSourcedUuid(const QString &sourceFile) {
  static const QUuid kRoot("6a1c7e2f-3b4d-5e6f-7a8b-9c0d1e2f3a4b");
  QUuid ns      = QUuid::createUuidV5(kRoot, sourceFile);
  QUuid shotSeed = QUuid::createUuid();  // random, unique per shot
  return QUuid::createUuidV5(ns, shotSeed.toString()).toString(QUuid::WithoutBraces);
}

void StoryboardPanel::ensureShotUuids() {
  ZtoryModel *m = ZtoryModel::instance();

  // Build a (uuid → source) map from the project DB to detect cross-storyboard
  // uuid collisions (happens when a .ztoryc is file-copied to a new storyboard).
  QString mySource = QFileInfo(ztoryPath()).fileName();
  QHash<QString, QString> dbUuidSource;
  for (const ProjectShot &ps : m->projectShots())
    dbUuidSource[ps.uuid] = ps.source;

  auto resolveUuid = [&](QString &id) {
    bool needNew = id.isEmpty();
    if (!needNew) {
      auto it = dbUuidSource.find(id);
      if (it != dbUuidSource.end() && it.value() != mySource)
        needNew = true;  // collision: owned by another storyboard
    }
    if (needNew) {
      // Remove the stale thumbnail so the old file doesn't mislead future loads.
      m->evictThumbFromDisk(id);
      id = makeSourcedUuid(mySource);
    }
  };

  int n = qMin((int)m_shots.size(), m->shotCount());
  for (int i = 0; i < n; i++) {
    QString &bu = m_shots[i].data.uuid;
    QString &mu = m->shot(i).uuid;
    QString id  = !bu.isEmpty() ? bu : mu;
    resolveUuid(id);
    bu = mu = id;
  }
  for (int i = n; i < (int)m_shots.size(); i++) {
    QString &bu = m_shots[i].data.uuid;
    resolveUuid(bu);
  }
}

void StoryboardPanel::pushTrackingToBoard() {
  ZtoryModel *m = ZtoryModel::instance();
  int n = qMin((int)m_shots.size(), m->shotCount());
  for (int i = 0; i < n; i++) {
    const ShotData &md = m->shot(i);
    ShotData &bd       = m_shots[i].data;
    if (!md.uuid.isEmpty()) bd.uuid = md.uuid;
    bd.technique = md.technique;
    bd.tasks     = md.tasks;
  }
}

void StoryboardPanel::pullTrackingFromBoard() {
  ZtoryModel *m = ZtoryModel::instance();
  int n = qMin((int)m_shots.size(), m->shotCount());
  for (int i = 0; i < n; i++) {
    const ShotData &bd = m_shots[i].data;
    ShotData &md       = m->shot(i);
    md.uuid      = bd.uuid;
    md.technique = bd.technique;
    md.tasks     = bd.tasks;
  }
}

QPixmap StoryboardPanel::firstPanelThumbnail(int shotIdx) const {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return QPixmap();
  const Shot &shot = m_shots[shotIdx];
  if (shot.panels.empty() || !shot.panels[0]) return QPixmap();
  return shot.panels[0]->previewPixmap();
}

//-----------------------------------------------------------------------------
// «Non possono esserci due shot identici» (Franco, 2026-08-17).
//
// Nel progetto uno shot si riconosce da SEQUENZA + ETICHETTA. Due storyboard
// senza sequenza hanno tutti e due il loro SH010: nel tracker sono due righe
// uguali, per Kitsu sono lo STESSO shot (l'aggancio e' proprio seq+etichetta),
// e il breakdown di uno finisce sull'altro — successo davvero, ed e' costato
// mezza giornata a capirlo. La sequenza non e' un vezzo di numerazione: e' la
// parte dell'identita' che dice da quale storyboard viene.
//-----------------------------------------------------------------------------
bool StoryboardPanel::s_shotIdentityPromptDone = false;

void StoryboardPanel::ensureShotIdentityUnique(const QString &sourceFile) {
  if (s_shotIdentityPromptDone) return;
  ZtoryModel *model = ZtoryModel::instance();
  if (model->projectDbPath().isEmpty()) return;
  const QStringList clashes = model->collidingShotLabels(sourceFile);
  if (clashes.isEmpty()) return;

  // Una volta sola, qualunque sia la risposta: chiederlo a ogni salvataggio
  // trasformerebbe una segnalazione utile in una molestia.
  s_shotIdentityPromptDone = true;

  // Chi sono gli altri: dirlo evita la caccia. «Si chiamano come quelli di un
  // altro storyboard» senza dire QUALE lascia il problema tutto da cercare.
  QStringList othersList;
  for (const ProjectShot &ps : model->projectShots()) {
    if (ps.source == sourceFile || ps.source.isEmpty()) continue;
    QString src = ps.source;
    src.remove(QRegularExpression("\\.ztoryc$",
                                  QRegularExpression::CaseInsensitiveOption));
    if (!othersList.contains(src)) othersList << src;
  }

  const QString proposed = model->proposeFreeSequenceLabel();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Two shots with the same name"));
  dlg.setMinimumWidth(520);
  auto *lay = new QVBoxLayout(&dlg);
  auto *head = new QLabel(
      tr("In this project a shot is identified by its sequence plus its "
         "number. These shots have the same name as shots of %1:")
          .arg(othersList.isEmpty() ? tr("another storyboard")
                                    : othersList.join(", ")),
      &dlg);
  head->setWordWrap(true);
  lay->addWidget(head);

  auto *list = new QPlainTextEdit(clashes.join("\n"), &dlg);
  list->setReadOnly(true);
  list->setMaximumHeight(120);
  lay->addWidget(list);

  auto *why = new QLabel(
      tr("Give this storyboard a sequence and they become distinguishable "
         "everywhere: Production Tracker, breakdown, Kitsu. Without it the two "
         "are the same shot, and what belongs to one lands on the other."),
      &dlg);
  why->setWordWrap(true);
  lay->addWidget(why);

  auto *row = new QHBoxLayout();
  row->addWidget(new QLabel(tr("Sequence:"), &dlg));
  auto *edit = new QLineEdit(proposed, &dlg);
  row->addWidget(edit);
  row->addStretch();
  lay->addLayout(row);
  auto *note = new QLabel(
      tr("It is given to the shots that have no sequence yet; shots already in "
         "a sequence keep theirs. The Board switches to sequence numbering so "
         "you can see it — the shot numbers themselves are not renumbered."),
      &dlg);
  note->setWordWrap(true);
  note->setStyleSheet("color:#999; font-size:11px;");
  lay->addWidget(note);

  auto *bbox = new QDialogButtonBox(&dlg);
  QPushButton *apply =
      bbox->addButton(tr("Give the sequence"), QDialogButtonBox::AcceptRole);
  bbox->addButton(tr("Leave as is"), QDialogButtonBox::RejectRole);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  (void)apply;
  if (dlg.exec() != QDialog::Accepted) return;

  const QString label = edit->text().trimmed();
  if (label.isEmpty()) return;
  SequenceData *seq = model->findOrCreateSequence(label);
  if (!seq) return;

  // Solo agli shot che una sequenza non ce l'hanno: uno storyboard puo' avere
  // piu' sequenze sue (SQ010, SQ020...) e sovrascriverle tutte con una sola
  // cancellerebbe la struttura che l'autore ha dato al racconto.
  int touched = 0;
  for (int i = 0; i < model->shotCount(); i++)
    if (model->shot(i).sequenceId.isEmpty()) {
      model->shot(i).sequenceId = seq->uuid;
      touched++;
    }
  for (auto &sh : m_shots)
    if (sh.data.sequenceId.isEmpty()) sh.data.sequenceId = seq->uuid;

  // La numerazione del Board deve mostrare la sequenza, o l'utente si ritrova
  // una sequenza assegnata e nessun posto dove vederla.
  NumberingConfig nc = model->numberingConfig();
  if (nc.style != NumberingConfig::Sequence) {
    nc.style = NumberingConfig::Sequence;
    model->setNumberingConfig(nc);
  }
  refreshFromScene();
  DVGui::info(tr("%1 shot(s) are now in sequence %2.").arg(touched).arg(label));
}

void StoryboardPanel::saveZtoryc() {
  // Shot scenes (role="shot") have a companion .ztoryc authored once at export.
  // Never rewrite it here — saveZtoryc always writes role="storyboard", which
  // would corrupt the back-link (and make the shot show the SB badge / open in
  // the wrong workflow).
  if (m_currentSceneIsShot) return;
  // Stessa ragione per le scene personaggio: il loro sidecar e' la casa dei
  // mouth set (e domani delle pose registrate). Riscriverlo come storyboard
  // li cancellerebbe in silenzio — l'utente se ne accorgerebbe al lip sync
  // dello shot dopo, quando le bocche non si assegnano piu'.
  if (m_currentSceneIsCharacter) return;
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

  // First-time creation: ask user whether to register as project storyboard.
  // Only ask once per session (m_suppressProjectPublication sticky until reload).
  // Guard: skip for empty/untitled scenes (no shots yet — nothing to register).
  if (!QFile::exists(path) && !m_suppressProjectPublication && !m_shots.empty()) {
    // Only prompt if the project DB exists (i.e., there IS a multi-scene project).
    if (!ZtoryModel::instance()->projectDbPath().isEmpty()) {
      QMessageBox ask(this);
      ask.setWindowTitle(tr("Register as storyboard?"));
      ask.setText(tr("Add this scene to the project as a storyboard?\n"
                     "Its shots will appear in the Production Tracker."));
      ask.setIcon(QMessageBox::Question);
      auto *yesBtn = ask.addButton(tr("Yes — storyboard"), QMessageBox::AcceptRole);
      auto *noBtn  = ask.addButton(tr("No — local only"),  QMessageBox::RejectRole);
      Q_UNUSED(noBtn)
      ask.exec();
      if (ask.clickedButton() != yesBtn)
        m_suppressProjectPublication = true;
    }
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  xml.writeAttribute("role", "storyboard");
  // Project metadata (production + title entered by user at scene creation).
  {
    ZtoryModel *model = ZtoryModel::instance();
    // production/title/episode/season/defaultTechnique/techniques now live in
    // the project DB (production.ztrack). The .ztoryc keeps only the per-scene
    // PDF logo settings here, and still READS the old attrs for migration.
    if (!model->pdfLogoPath().isEmpty() || model->pdfNoLogo()) {
      xml.writeStartElement("project");
      if (!model->pdfLogoPath().isEmpty())
        xml.writeAttribute("pdfLogo", model->pdfLogoPath());
      if (model->pdfNoLogo())
        xml.writeAttribute("pdfNoLogo", "1");
      xml.writeEndElement();
    }
    // NOTE: the team roster now lives in the project-level DB
    // (production.ztrack), not in the per-scene .ztoryc. The <team> block is
    // still READ on load (loadZtoryc) for one-time migration of legacy scenes.
    // Assets now live in the project DB (production.ztrack), not the .ztoryc.
    // The <assets> block is still READ on load for one-time migration.
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
  // Make the Board copy reflect the model (tracker edits) before serializing,
  // and guarantee every shot has a stable uuid.
  ensureShotUuids();
  pushTrackingToBoard();
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const Shot &shot = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",      QString::number(si));
    if (!shot.data.uuid.isEmpty())
      xml.writeAttribute("uuid",     shot.data.uuid);
    xml.writeAttribute("number",     shot.data.shotNumber);
    xml.writeAttribute("label",      shot.data.shotLabel);
    xml.writeAttribute("order",      QString::number(shot.data.orderIndex));
    xml.writeAttribute("sequenceId", shot.data.sequenceId);
    if (shot.data.transitionFrames > 0)
      xml.writeAttribute("transition", QString::number(shot.data.transitionFrames));
    // Production tracking (spreadsheet / Kitsu).
    if (!shot.data.technique.isEmpty())
      xml.writeAttribute("technique", shot.data.technique);
    if (!shot.data.notes.isEmpty())
      xml.writeTextElement("shotNotes", shot.data.notes);
    if (!shot.data.vfxNotes.isEmpty())
      xml.writeTextElement("shotVfxNotes", shot.data.vfxNotes);
    for (auto it = shot.data.tasks.constBegin(); it != shot.data.tasks.constEnd(); ++it) {
      xml.writeStartElement("task");
      xml.writeAttribute("type",   it.key());
      xml.writeAttribute("status", ZtoryModel::taskStatusLabel(it.value().status));
      if (!it.value().assignees.isEmpty())
        xml.writeAttribute("assignee", it.value().assignees.join(", "));
      xml.writeEndElement();
    }
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
  // Publish structural metadata to the project DB (unless user opted out).
  if (!m_suppressProjectPublication) {
    QString sourceFile = QFileInfo(path).fileName();
    if (!sourceFile.isEmpty()) {
      // Prima di pubblicare, non dopo: pubblicare e POI dire che c'e' un
      // doppione vorrebbe dire lasciarlo nel progetto mentre lo si segnala.
      ensureShotIdentityUnique(sourceFile);
      {
        // Sync the model's shot list to THIS scene's actual shots before
        // publishing, so leftover shots from a previously-open (larger) scene
        // never get written into this project (cross-project / cross-storyboard
        // contamination).
        ZtoryModel *m0 = ZtoryModel::instance();
        std::vector<ShotData> sceneShots;
        sceneShots.reserve(m_shots.size());
        for (const auto &s : m_shots) sceneShots.push_back(s.data);
        m0->setShotsFrom(sceneShots);
      }
      ZtoryModel::instance()->publishShotsToProjectDb(sourceFile);
      // Update thumbnail cache so Production Tracker keeps thumbs after scene switch.
      ZtoryModel *m = ZtoryModel::instance();
      for (int si = 0; si < (int)m_shots.size(); si++) {
        QPixmap pm = firstPanelThumbnail(si);
        if (!pm.isNull())
          m->updateThumbCache(m_shots[si].data.uuid, pm);
      }
    }
  }
}

// Identity (.ztoryc path) of the scene we last auto-switched workflow for. Used
// to fire the room switch only once per scene open, so panel recreation from the
// switch itself — and the user's later manual switch — aren't overridden.
static QString s_lastAutoWorkflowScene;

void StoryboardPanel::loadZtoryc() {
  // Imported screenplay path read from this scene's .ztoryc.  Stays empty when
  // the scene has none — so opening a scene without a screenplay (or a brand
  // new scene) clears the Script panel instead of leaving a stale one loaded.
  QString scriptFromFile;
  m_loadingZtoryc = true;  // suppress scriptFileChanged→saveZtoryc during load
  // Reset role/back-link state so values from a previous scene don't leak (and a
  // new/empty scene is never mistaken for a shot scene).
  m_currentSceneIsShot      = false;
  m_currentSceneIsCharacter = false;
  s_shotIdentityPromptDone  = false;
  m_shotBackLinkProject   = QString();
  m_shotBackLinkUuid      = QString();
  m_shotBackLinkTaskStage = QString();
  m_shotBackLinkTechnique = QString();
  QString path = ztoryPath();
  if (path.isEmpty()) {
    // The scene has no companion .ztoryc (new/empty scene). Clear the model's
    // shot list so it doesn't keep the PREVIOUS scene's shots — otherwise the
    // next publish writes those shots into THIS project (cross-project leak).
    ZtoryModel::instance()->clearShots();
    ZtoryModel::instance()->setScriptFile(scriptFromFile);
    ZtoryModel::instance()->resetProjectLevelDefaults();
    ZtoryModel::instance()->loadProjectDb();  // load this project's DB (or migrate defaults)
    m_loadingZtoryc = false;
    emit ZtoryModel::instance()->productionReloaded();
    return;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    ZtoryModel::instance()->clearShots();
    ZtoryModel::instance()->setScriptFile(scriptFromFile);
    ZtoryModel::instance()->resetProjectLevelDefaults();
    ZtoryModel::instance()->loadProjectDb();  // load this project's DB (or migrate defaults)
    m_loadingZtoryc = false;
    emit ZtoryModel::instance()->productionReloaded();
    return;
  }
  // File exists: reset project metadata so stale values from a previous scene
  // or creation flow don't bleed into this reload. They will be repopulated
  // below if the file contains a <project> element.
  ZtoryModel::instance()->setPdfLogoPath("");
  ZtoryModel::instance()->setPdfNoLogo(false);
  // Start each scene's sequence list fresh so sequences never leak across
  // scenes. Old files (no <sequence>) leave it empty → renumberAll() recreates
  // a default sequence if needed.
  ZtoryModel::instance()->sequences().clear();
  // Reset ALL project-level data (production/season/title/episode/team/assets/
  // techniques) so nothing leaks from the previous scene/project. The .ztoryc
  // below repopulates it for migration; loadProjectDb() then overrides from the
  // project DB (or migrates these defaults if no DB exists yet).
  ZtoryModel::instance()->resetProjectLevelDefaults();

  QXmlStreamReader xml(&file);
  int si = -1, pi = -1, ai = -1;
  // role: "storyboard" (default/legacy) → publishShotsToProjectDb;
  //       "shot" (B3c exported scene) → do NOT publish (not a shot list source).
  QString sceneRole = "storyboard";
  std::vector<Technique> loadedTechs;  // technique presets from file (if any)
  QStringList loadedTeam;              // team roster from file (replaces model's)
  bool        hasTeamBlock = false;    // true once a <team> element is seen
  std::vector<Asset> loadedAssets;     // assets from file (replaces model's)
  bool        hasAssetsBlock = false;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("ztoryc")) {
        // Root element: read role + back-link attributes.
        auto a = xml.attributes();
        QString r = a.value("role").toString();
        if (!r.isEmpty()) sceneRole = r;
        m_shotBackLinkUuid      = a.value("projectShot").toString();
        m_shotBackLinkProject   = a.value("project").toString();
        m_shotBackLinkTaskStage = QString();  // read from <project> below
        m_shotBackLinkTechnique = QString();  // read from <project> below
      } else if (xml.name() == QLatin1String("project")) {
        auto a = xml.attributes();
        ZtoryModel::instance()->setProduction(a.value("production").toString());
        ZtoryModel::instance()->setTitle(a.value("title").toString());
        ZtoryModel::instance()->setEpisode(a.value("episode").toString());
        ZtoryModel::instance()->setPdfLogoPath(a.value("pdfLogo").toString());
        ZtoryModel::instance()->setPdfNoLogo(a.value("pdfNoLogo").toInt() != 0);
        if (a.hasAttribute("defaultTechnique"))
          ZtoryModel::instance()->setDefaultTechnique(
              a.value("defaultTechnique").toString());
        // B3c: read task stage from shot scene's <project> element.
        if (a.hasAttribute("taskStage"))
          m_shotBackLinkTaskStage = a.value("taskStage").toString();
        // Shot technique authored at export — drives the workflow to open in.
        if (a.hasAttribute("technique"))
          m_shotBackLinkTechnique = a.value("technique").toString();
      }
      else if (xml.name() == QLatin1String("technique")) {
        Technique t;
        t.name      = xml.attributes().value("name").toString();
        t.taskTypes = xml.attributes().value("tasks").toString()
                          .split('|', Qt::SkipEmptyParts);
        if (!t.name.isEmpty()) loadedTechs.push_back(t);
      }
      else if (xml.name() == QLatin1String("team")) {
        hasTeamBlock = true;  // a <team> exists → replace model roster (even if empty)
      }
      else if (xml.name() == QLatin1String("person")) {
        QString nm = xml.attributes().value("name").toString().trimmed();
        if (!nm.isEmpty()) loadedTeam << nm;
      }
      else if (xml.name() == QLatin1String("assets")) {
        hasAssetsBlock = true;  // an <assets> exists → replace model assets
      }
      else if (xml.name() == QLatin1String("asset")) {
        Asset as;
        as.uuid = xml.attributes().value("uuid").toString();
        if (as.uuid.isEmpty())
          as.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        as.type = xml.attributes().value("type").toString();
        as.name = xml.attributes().value("name").toString();
        QString tg = xml.attributes().value("tags").toString();
        if (!tg.isEmpty()) as.tags = tg.split('|', Qt::SkipEmptyParts);
        loadedAssets.push_back(as);
        ai = (int)loadedAssets.size() - 1;
      }
      else if (xml.name() == QLatin1String("atask")) {
        if (ai >= 0 && ai < (int)loadedAssets.size()) {
          auto a       = xml.attributes();
          QString type = a.value("type").toString();
          if (!type.isEmpty()) {
            TaskState ts;
            ts.status = ZtoryModel::taskStatusFromLabel(a.value("status").toString());
            for (const QString &p :
                 a.value("assignee").toString().split(',', Qt::SkipEmptyParts)) {
              QString t = p.trimmed();
              if (!t.isEmpty()) ts.assignees << t;
            }
            loadedAssets[ai].tasks.insert(type, ts);
          }
        }
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
          m_shots[si].data.uuid             = xml.attributes().value("uuid").toString();
          m_shots[si].data.shotNumber       = xml.attributes().value("number").toString();
          m_shots[si].data.shotLabel        = xml.attributes().value("label").toString();
          m_shots[si].data.orderIndex       = xml.attributes().value("order").toInt();
          m_shots[si].data.sequenceId       = xml.attributes().value("sequenceId").toString();
          m_shots[si].data.transitionFrames = xml.attributes().value("transition").toInt();
          m_shots[si].data.technique        = xml.attributes().value("technique").toString();
          m_shots[si].data.tasks.clear();   // refilled by <task> children below
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
      else if (xml.name() == QLatin1String("task")) {
        if (si >= 0 && si < (int)m_shots.size()) {
          auto a = xml.attributes();
          QString type = a.value("type").toString();
          if (!type.isEmpty()) {
            TaskState ts;
            ts.status   = ZtoryModel::taskStatusFromLabel(a.value("status").toString());
            const QStringList parts =
                a.value("assignee").toString().split(',', Qt::SkipEmptyParts);
            for (const QString &p : parts) {
              QString t = p.trimmed();
              if (!t.isEmpty()) ts.assignees << t;
            }
            m_shots[si].data.tasks.insert(type, ts);
          }
        }
      }
      else if (xml.name() == QLatin1String("shotNotes")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size()) m_shots[si].data.notes = t;
      }
      else if (xml.name() == QLatin1String("shotVfxNotes")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size()) m_shots[si].data.vfxNotes = t;
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

  // Replace the seeded technique presets with the ones saved in this scene
  // (only when present, so old files keep the built-in defaults).
  if (!loadedTechs.empty())
    ZtoryModel::instance()->techniques() = loadedTechs;
  if (hasTeamBlock) ZtoryModel::instance()->setTeam(loadedTeam);
  if (hasAssetsBlock) ZtoryModel::instance()->assets() = loadedAssets;

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
  // Bridge: model becomes the live store for tracking fields BEFORE any
  // saveZtoryc below (so a re-save can't push stale/empty model data over the
  // freshly-loaded Board copy). Backfill uuids for pre-uuid (legacy) scenes.
  pullTrackingFromBoard();
  ensureShotUuids();
  // Mark shot scenes BEFORE any saveZtoryc() below so the companion .ztoryc is
  // never rewritten with role="storyboard".
  m_currentSceneIsShot      = (sceneRole == "shot");
  m_currentSceneIsCharacter = (sceneRole == "character");
  // Persist the SFH-explosion repair so the scene loads cleanly next time.
  // m_currentZtoryPath is still empty here (set by refreshFromScene after we
  // return), so temporarily anchor it so saveZtoryc() can write.
  if (sfhRepaired) {
    m_currentZtoryPath = ztoryPath();
    saveZtoryc();
    m_currentZtoryPath.clear();  // refreshFromScene will set it authoritatively
  }
  // role="shot": load the project DB from the stored back-link path and, on the
  // first open, advance ONLY the first task after the storyboard (usually
  // Layout) Ready/Todo → WIP (Model A: opening the single shot scene means work
  // has started on its first pipeline step). Later tasks stay Todo.
  if (sceneRole == "shot") {
    QString shotTech;  // technique of this shot → drives the workflow to open in
    if (!m_shotBackLinkProject.isEmpty()) {
      // Load the project DB from the absolute path stored in the .ztoryc.
      QFile pf(m_shotBackLinkProject);
      if (pf.exists()) {
        ZtoryModel *m = ZtoryModel::instance();
        m->loadProjectDbFromPath(m_shotBackLinkProject);
        if (!m_shotBackLinkUuid.isEmpty()) {
          for (ProjectShot &ps : m->projectShots_rw()) {
            if (ps.uuid != m_shotBackLinkUuid) continue;
            QStringList tts;
            QString tech = ps.technique.isEmpty() ? m->defaultTechnique()
                                                  : ps.technique;
            shotTech = tech;
            if (const Technique *t = m->findTechnique(tech)) tts = t->taskTypes;
            if (!tts.isEmpty()) {
              TaskState &ts = ps.tasks[tts.first()];
              if (ts.status == TaskStatus::Todo ||
                  ts.status == TaskStatus::Ready) {
                ts.status = TaskStatus::Wip;
                m->saveProjectDb();
                emit m->taskStatusChanged();
              }
            }
            break;
          }
        }
      }
    }
    // Open the room/workflow matching the shot's technique. Prefer the technique
    // authored in the shot's .ztoryc (set at export); fall back to the project
    // DB lookup. Techniques without a dedicated workflow (Traditional, 3D/CGI,
    // Generic, Live…) fall back to 2D Tradigital. Deferred so it runs after the
    // scene load settles (switching rooms mid-load can blank the viewers).
    // Dedup: only auto-switch ONCE per scene open. loadZtoryc runs per-panel and
    // again after a room switch recreates panels — without this guard each call
    // re-schedules the command, and the late ones override the user's manual
    // room switch.
    if (ZtoryModel::autoWorkflowDetection() && path != s_lastAutoWorkflowScene) {
      s_lastAutoWorkflowScene = path;
      QString effTech = !m_shotBackLinkTechnique.isEmpty()
                            ? m_shotBackLinkTechnique
                            : shotTech;
      QString wfCmd = ZtoryModel::workflowCommand("shot", effTech);
      QTimer::singleShot(0, this, [wfCmd] {
        CommandManager::instance()->execute(wfCmd.toStdString().c_str());
      });
    }
    // Shot scenes don't publish to the project DB.
    emit ZtoryModel::instance()->productionReloaded();
    return;
  }

  // Team now lives in the project-level DB (production.ztrack), shared across
  // the project's scenes. Load it after the .ztoryc (project file wins; if it
  // doesn't exist yet, the .ztoryc team migrates into it).
  ZtoryModel::instance()->loadProjectDb();
  // Publish this storyboard's shots into the project DB so the Production
  // Tracker can aggregate across multiple storyboards. Skip role="shot" scenes
  // (exported shot .tnz from B3c): they are consumers of the project DB, not
  // sources of the shot list.
  if (sceneRole == "storyboard") {
    QString src = QFileInfo(path).fileName();
    if (!src.isEmpty()) {
      {
        // Same sync as in saveZtoryc: publish exactly THIS scene's shots.
        ZtoryModel *m0 = ZtoryModel::instance();
        std::vector<ShotData> sceneShots;
        sceneShots.reserve(m_shots.size());
        for (const auto &s : m_shots) sceneShots.push_back(s.data);
        m0->setShotsFrom(sceneShots);
      }
      ZtoryModel::instance()->publishShotsToProjectDb(src);
      ZtoryModel *m = ZtoryModel::instance();
      for (int si = 0; si < (int)m_shots.size(); si++) {
        QPixmap pm = firstPanelThumbnail(si);
        if (!pm.isNull())
          m->updateThumbCache(m_shots[si].data.uuid, pm);
      }
    }
  }
  // A storyboard scene opens directly in the Storyboard workflow (like shot
  // scenes open in their technique's workflow). Deferred so it runs after the
  // scene load settles (switching rooms mid-load can blank the viewers).
  if (sceneRole == "storyboard" && ZtoryModel::autoWorkflowDetection() &&
      path != s_lastAutoWorkflowScene) {
    s_lastAutoWorkflowScene = path;
    QTimer::singleShot(0, this, [] {
      CommandManager::instance()->execute(MI_WorkflowStoryboard);
    });
  }
  // Project/team/assets are now populated in the model. refreshFromScene does
  // NOT emit modelReset, so the Production Tracker's non-shot tabs (Team /
  // Project / Assets) would otherwise stay stale after a scene reopen.
  emit ZtoryModel::instance()->productionReloaded();
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
    // ignoreLastStop=true: drop the trailing Stop Frame Hold so timelineDuration
    // is the shot's true length. Without it the +1 widens the panel-visibility
    // filter (f < visibleEnd) below and lets a boundary frame through as a
    // phantom 1-frame panel. r0/r1 are also reused below to locate the child
    // level, so getRange always runs.
    mc->getRange(r0, r1, /*ignoreLastStop=*/true);
    timelineDuration = (r1 >= r0) ? r1 - r0 + 1 : 0;
    // NET duration: shotTrueSpan excludes any exposed cross-dissolve overlap
    // cells (the gross getRange above would inflate every panel's duration).
    int ts = 0, td = 0;
    if (ZtoryShotOps::shotTrueSpan(mainXsh, mainCol, ts, td))
      timelineDuration = td;
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
      // NET duration (dissolve overlap excluded) — same as the main branch.
      int ts = 0, td = 0;
      if (ZtoryShotOps::shotTrueSpan(anc->m_xsheet, mainCol, ts, td)) {
        timelineDuration = td;
      } else if (TXshColumn *mc = anc->m_xsheet->getColumn(mainCol)) {
        int r0 = 0, r1 = 0;
        // ignoreLastStop=true: same SFH-inflation guard as the main-xsheet branch.
        mc->getRange(r0, r1, /*ignoreLastStop=*/true);
        if (r1 >= r0) timelineDuration = r1 - r0 + 1;
      }
    }
  }

  int numCols = xsh->getColumnCount();
  int numFrames = xsh->getFrameCount();
  if (numFrames <= 0 || numCols <= 0) {
    // Sub-scene completely empty (e.g. all drawings undone). Collapse to 1 panel
    // so ghost widgets from a previous multi-panel state are removed (task 50).
    Shot &shot = m_shots[shotIdx];
    if ((int)shot.data.panels.size() != 1) {
      while ((int)shot.data.panels.size() > 1) shot.data.panels.pop_back();
      if (shot.data.panels.empty()) { PanelData pd; shot.data.panels.push_back(pd); }
      shot.data.panels[0].startFrame = 0;
      shot.data.panels[0].duration   = timelineDuration;
      for (PanelWidget *pw : shot.panels) { m_grid->removeWidget(pw); delete pw; }
      shot.panels.clear();
      addPanelWidget(shotIdx, 0);
      renumberAll();
      rebuildGrid();
      ZtoryModel::instance()->syncShotPanels(shotIdx, shot.data.panels,
                                             shot.data.shotLabel,
                                             shot.data.xsheetColumn);
      saveZtoryc();
    }
    return;
  }

  // Sound and SoundText (note) columns must not drive panel detection: the
  // cross-dissolve XD-out/XD-in note columns would otherwise create a spurious
  // panel boundary where they start/end.
  auto isCountedCol = [&](int c) -> bool {
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

  //
  // ── DOVE NASCE UN PANNELLO (regola decisa con Franco il 2026-08-18) ──────
  //
  // Un pannello di storyboard e' un DISEGNO, non un fotogramma. Un personaggio
  // animato che si muove non e' un pannello nuovo: e' lo stesso pannello che
  // prende vita. Quindi:
  //
  //  • cambio del disegno ESPOSTO su un livello che NON e' in animazione
  //    piena → confine (e per le sotto-scene «esposto» vuol dire cio' che si
  //    vede davvero dentro, non la riga che scorre — vedi
  //    ztoryExposedSignature);
  //  • chiave di colonna → confine, sempre;
  //  • movimento di camera → confine, sempre;
  //  • livelli in ANIMAZIONE PIENA → non un pannello per cambio, che sarebbe
  //    uno per fotogramma, ma una griglia regolare al ritmo scelto dall'utente
  //    («max pannelli al secondo»), di fatto un each.
  //
  const double fps =
      qMax(1.0, scene->getProperties()->getOutputProperties()->getFrameRate());
  const int maxPanelsPerSec =
      qBound(1, QSettings().value("Ztoryc/MaxPanelsPerSecond", 1).toInt(), 24);

  // Un livello e' «in animazione piena» quando cambia piu' spesso del ritmo
  // che l'utente ha scelto: sotto quella soglia i suoi cambi valgono tutti,
  // sopra si passa alla griglia regolare.
  const double durationSec = timelineDuration / fps;
  const int changeCap =
      qMax(1, (int)std::ceil(durationSec * (double)maxPanelsPerSec));

  std::set<int> boundaries;
  boundaries.insert(0);
  bool hasFullAnimation = false;

  for (int c = 0; c < numCols; c++) {
    if (!isCountedCol(c)) continue;

    std::vector<int> changes;
    QString prev = ztoryExposedSignature(xsh, 0, c, kZtoryExposedDepth);
    for (int r = 1; r < numFrames; r++) {
      QString curr = ztoryExposedSignature(xsh, r, c, kZtoryExposedDepth);
      if (curr != prev) changes.push_back(r);
      prev = curr;
    }

    if ((int)changes.size() > changeCap) {
      hasFullAnimation = true;  // ci pensa la griglia regolare, sotto
    } else {
      for (int r : changes) boundaries.insert(r);
    }

    // Le chiavi di colonna sono confine anche su un livello in animazione
    // piena: sono decisioni di messa in scena, non disegni che scorrono.
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (obj)
      for (int r = 1; r < numFrames; r++)
        if (obj->isKeyframe(r)) boundaries.insert(r);
  }

  if (useCameraKeys) {
    TStageObject *cam = xsh->getStageObject(TStageObjectId::CameraId(0));
    if (cam)
      for (int r = 1; r < numFrames; r++)
        if (cam->isKeyframe(r)) boundaries.insert(r);
  }

  if (hasFullAnimation) {
    const int step = qMax(1, (int)qRound(fps / (double)maxPanelsPerSec));
    for (int r = step; r < numFrames; r += step) boundaries.insert(r);
  }

  std::vector<int> allPanelFrames(boundaries.begin(), boundaries.end());

  // Keep only panels whose start frame falls within the timeline-visible range.
  // Panel frames are SUB-SCENE rows: with an incoming cross-dissolve the sub
  // content is shifted down by headHalf hold copies (XD-in), so the visible
  // window of sub rows is [0, headHalf + timelineDuration).
  int headHalf   = ZtoryShotOps::xdInHeadOffset(xsh);
  int visibleEnd = timelineDuration + headHalf;
  std::vector<int> panelFrames;
  for (int f : allPanelFrames)
    if (f < visibleEnd) panelFrames.push_back(f);
  if (panelFrames.empty()) panelFrames.push_back(0);

  // Per-panel durations in TIMELINE frames. Boundaries are sub rows, but the
  // partials must add up to the NET timelineDuration: the first panel absorbs
  // the head hold copies (same drawing as the real first frame), so subtract
  // them from its count.
  std::vector<int> panelDur(panelFrames.size());
  for (int i = 0; i < (int)panelFrames.size(); i++)
    panelDur[i] = (i + 1 < (int)panelFrames.size())
                      ? panelFrames[i + 1] - panelFrames[i]
                      : visibleEnd - panelFrames[i];
  if (headHalf > 0 && panelFrames[0] == 0)
    panelDur[0] = qMax(1, panelDur[0] - headHalf);

  Shot &shot = m_shots[shotIdx];
  int newPanelCount = (int)panelFrames.size();

  if (newPanelCount == (int)shot.data.panels.size()) {
    // Count unchanged — update durations only (timeline may have been resized)
    for (int i = 0; i < newPanelCount; i++) {
      shot.data.panels[i].startFrame = panelFrames[i];
      shot.data.panels[i].duration   = panelDur[i];
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
    shot.data.panels[i].duration   = panelDur[i];
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
  // Ground truth = the ordered list of child-level (shot) columns in the scene.
  // EVERY Board instance (Board room, Shot board, floating) must match it, so we
  // rebuild whenever this panel's shot list disagrees in COUNT *or ORDER*. The
  // old check compared count only: a panel whose local m_shots had the right
  // count but a shot at the wrong position (e.g. a second board that inserted at
  // its own stale selection) silently stayed desynced — showing a different
  // layout and, in the worst case, holding a shot with no panel widget.
  std::vector<int> childCols;
  // The sub-scene each shot column exposes, in the same order as childCols.
  // This is the scene's own answer to "which shots exist, and in what order",
  // expressed in identities that survive column shifts — see Shot::childLevel.
  std::vector<TXshChildLevel *> childLevels;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        childCols.push_back(col);
        childLevels.push_back(cell.m_level->getChildLevel());
        break;
      }
    }
  }

  // Fast path: recognise a plain INSERTION and handle just that, instead of
  // rebuilding every panel on the board.
  //
  // Rebuilding is what makes Add Shot expensive: each new PanelWidget (and its
  // ~8 children) is style-matched against the application-wide theme sheet, a
  // cost that profiling showed cannot be optimised away. But adding a shot does
  // not change the other shots at all — so if the scene's shot list is exactly
  // ours with one extra entry, the only correct update is to insert that one.
  //
  // Identity is Shot::childLevel, not the column index: inserting shifts every
  // later column, so index comparison cannot tell "same shot, moved" from
  // "different shot". Every shot must already carry an identity, otherwise we
  // cannot reason about it and fall through to the safe full rebuild below.
  if (!m_updating && childLevels.size() == m_shots.size() + 1) {
    bool identified = true;
    for (const Shot &s : m_shots)
      if (!s.childLevel) { identified = false; break; }

    // onShotInserted() uses one number as BOTH the shot position and the xsheet
    // column (it inserts at m_shots.begin()+col and reads xsh->getColumn(col)),
    // which only holds when the shot columns are 0..N-1 with no gaps. A scene
    // can interleave other column types — this one carries three sound columns
    // — so verify the layout before relying on that equivalence; anything else
    // takes the full rebuild, which does not make the assumption.
    for (int i = 0; identified && i < (int)childCols.size(); i++)
      if (childCols[i] != i) identified = false;

    if (identified) {
      // Walk both lists in step; the first mismatch is the insertion point, and
      // everything after it must line up again for this to BE an insertion.
      int insertAt = -1;
      bool isPlainInsert = true;
      for (int i = 0, j = 0; j < (int)childLevels.size(); j++) {
        if (i < (int)m_shots.size() && m_shots[i].childLevel == childLevels[j]) {
          i++;
          continue;
        }
        if (insertAt >= 0) {  // a second mismatch: not a plain insertion
          isPlainInsert = false;
          break;
        }
        insertAt = j;
      }
      if (isPlainInsert && insertAt >= 0) {
        qWarning("[ZTORY] onModelResequenced: shot inserted at %d -> incremental "
                 "(board keeps its %d existing panels)",
                 insertAt, (int)m_shots.size());
        onShotInserted(insertAt);
        return;
      }
    }
  }

  // Mirror image: a plain REMOVAL. Same reasoning and the same guards — one of
  // our shots is gone from the scene and the rest still line up, so the only
  // correct update is to drop that shot's panels.
  if (!m_updating && m_shots.size() == childLevels.size() + 1) {
    bool identified = true;
    for (const Shot &s : m_shots)
      if (!s.childLevel) { identified = false; break; }
    for (int i = 0; identified && i < (int)childCols.size(); i++)
      if (childCols[i] != i) identified = false;

    if (identified) {
      int removeAt = -1;
      bool isPlainRemove = true;
      for (int i = 0, j = 0; i < (int)m_shots.size(); i++) {
        if (j < (int)childLevels.size() && m_shots[i].childLevel == childLevels[j]) {
          j++;
          continue;
        }
        if (removeAt >= 0) {  // a second mismatch: not a plain removal
          isPlainRemove = false;
          break;
        }
        removeAt = i;
      }
      if (isPlainRemove && removeAt >= 0) {
        qWarning("[ZTORY] onModelResequenced: shot removed at %d -> incremental "
                 "(board keeps its %d remaining panels)",
                 removeAt, (int)m_shots.size() - 1);
        for (PanelWidget *pw : m_shots[removeAt].panels) {
          m_grid->removeWidget(pw);
          delete pw;
        }
        m_shots.erase(m_shots.begin() + removeAt);
        // Re-anchor the columns from the scene rather than shifting our own
        // numbers: childCols IS what the xsheet now holds, so there is nothing
        // to deduce and no drift to accumulate.
        for (int i = 0; i < (int)m_shots.size(); i++)
          m_shots[i].data.xsheetColumn = childCols[i];
        renumberAll();
        rebuildGrid();
        saveZtoryc();
        return;
      }
    }
  }

  bool drifted = (childCols.size() != m_shots.size());
  TStageObjectTree *tree = xsh->getStageObjectTree();
  for (int si = 0; !drifted && si < (int)m_shots.size(); si++) {
    // Drift if order differs from the scene OR a shot has no widget at all: an
    // incremental insert path (onShotInserted with a bad index) can leave a shot
    // in m_shots whose addPanelWidget bailed (guard) → no PanelWidget → invisible
    // even though the count/order match. A full rebuild recreates the widgets.
    if (m_shots[si].data.xsheetColumn != childCols[si] ||
        m_shots[si].panels.empty()) {
      drifted = true;
      break;
    }
    // Reorder detection: a move swaps column CONTENT (cells), not column indices,
    // so xsheetColumn still matches. The column's stage-object name is kept equal
    // to the shot label by updateColumnName(); compare it against this shot's
    // label — a mismatch means another panel reordered the shots underneath us.
    //
    // Only when the column actually carries an explicit name: getName() falls
    // back to "Col<N>" for unnamed columns, which never equals a shot label, so
    // comparing it flagged drift on EVERY resequence — a full Board rebuild
    // (loadZtoryc() re-reads the .ztoryc from disk) on every trim.
    TStageObject *obj =
        tree ? tree->getStageObject(TStageObjectId::ColumnId(childCols[si]), false)
             : nullptr;
    if (obj && obj->hasSpecifiedName()) {
      const QString colName = QString::fromStdString(obj->getName());
      if (!colName.isEmpty() && colName != m_shots[si].data.label()) {
        drifted = true;
        break;
      }
    }
  }
  if (drifted) {
    qWarning("[ZTORY] onModelResequenced: scene has %d shot columns, panel has %d "
             "(or order differs) -> full rebuild",
             (int)childCols.size(), (int)m_shots.size());
    refreshFromScene();
    // refreshFromScene() rebuilds the grid with blank thumbnails (lazy by
    // design, so scene LOAD never freezes). This branch only runs after an
    // interactive shot op (add / paste / delete / cut / merge), where the user
    // expects the visible thumbnails back immediately. singleShot(0) (NOT a
    // synchronous call) defers just past this event loop so the QGridLayout has
    // repositioned the rebuilt panels — otherwise updateVisiblePreviews() would
    // viewport-test stale geometry and render the wrong panels. It renders only
    // viewport panels and skips ones that already have a pixmap, so even if a
    // burst of ops fires it several times the heavy render happens once.
    QTimer::singleShot(0, this, &StoryboardPanel::onRefreshPreviews);
    return;
  }
  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int duration = 0;
    // NET duration (dissolve overlap excluded); fallback getRange with
    // ignoreLastStop=true: skip the trailing Stop Frame Hold placed by
    // ZtoryModel::resequenceXsheet() so the duration shown in the Board
    // matches the shot's actual animatic length (not inflated by +1).
    int ts = 0, td = 0;
    if (ZtoryShotOps::shotTrueSpan(xsh, col, ts, td)) {
      duration = td;
    } else {
      int r0 = 0, r1 = 0;
      column->getRange(r0, r1, /*ignoreLastStop=*/true);
      duration = r1 - r0 + 1;
    }
    // T: (total) updates for every panel; D: (partial) only for single-panel
    // shots — same rule as onXsheetChanged. This loop used to overwrite
    // panels[0].duration with the TOTAL unconditionally, so on every model
    // resequence the first panel of a multi-panel shot showed the whole shot
    // length until the shot was re-entered (detectAndUpdatePanels fixed it).
    for (PanelWidget *pw : m_shots[si].panels)
      pw->setTotalDuration(duration);
    if (m_shots[si].data.panels.size() == 1) {
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
    // ignoreLastStop=true: exclude the resequence SFH so the new shot reads its
    // true length, not length+1.
    column->getRange(r0, r1, /*ignoreLastStop=*/true);
    // Scene-side identity: the sub-scene this column exposes. Recorded so a
    // later resequence can recognise this shot wherever it ends up.
    for (int r = r0; r <= r1 && !shot.childLevel; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
        shot.childLevel = cell.m_level->getChildLevel();
    }
    PanelData pd;
    pd.duration = (r1 >= r0) ? (r1 - r0 + 1) : 24;
    shot.data.panels.push_back(pd);
  } else {
    PanelData pd; pd.duration = 24;
    shot.data.panels.push_back(pd);
  }

  // A shot can arrive already knowing how many panels it has: Send to Board
  // from the Thumbnail room builds one PanelData per selected thumbnail, with
  // its start frame and hold, before the Board hears about the insertion. Take
  // that list rather than the single panel guessed above — otherwise the Board
  // shows one panel until the user opens the shot and detectAndUpdatePanels
  // finally counts the drawings.
  {
    ZtoryModel *m = ZtoryModel::instance();
    if (col < m->shotCount() && m->shot(col).panels.size() > 1)
      shot.data.panels = m->shot(col).panels;
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
  for (int pi = 0; pi < (int)m_shots[col].data.panels.size(); pi++)
    addPanelWidget(col, pi);

  renumberAll();
  rebuildGrid();
  saveZtoryc();

  // Render the new panels' thumbnails. Deferred so the insertion returns at
  // once (updatePreview renders synchronously) but without waiting for the user
  // to open the shot, which is what used to be needed for anything to appear.
  const int nPanels = (int)m_shots[col].data.panels.size();
  QTimer::singleShot(0, this, [this, col, nPanels]() {
    if (col >= (int)m_shots.size()) return;
    for (int pi = 0; pi < nPanels && pi < (int)m_shots[col].panels.size(); pi++)
      updatePreview(col, pi);
  });
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

  // Camera shape changed (e.g. Camera Settings → square): relayout + re-render
  // all previews at the new aspect.  Cheap aspect check avoids doing this on the
  // many xsheet changes that don't touch the camera.
  double aspect = ZtoryShotOps::cameraAspect(scene);
  if (qAbs(aspect - m_lastCameraAspect) > 1e-4) {
    m_lastCameraAspect = aspect;
    for (Shot &shot : m_shots)
      for (PanelWidget *pw : shot.panels) {
        pw->setPreviewPixmap(QPixmap());  // invalidate → forces re-render
        pw->rescalePreview();             // updates label height to new aspect
      }
    updateVisiblePreviews();
  }

  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int duration = 0;
    // NET duration: shotTrueSpan excludes any exposed cross-dissolve overlap
    // cells. Fallback getRange with ignoreLastStop=true: exclude the
    // resequence SFH (matches onModelResequenced) so T:/D: show the shot's
    // true length, not length+1.
    int ts = 0, td = 0;
    if (ZtoryShotOps::shotTrueSpan(xsh, col, ts, td)) {
      duration = td;
    } else {
      int r0 = 0, r1 = 0;
      column->getRange(r0, r1, /*ignoreLastStop=*/true);
      duration = r1 - r0 + 1;
    }
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

void StoryboardPanel::setShotButtonsHidden(bool hidden) {
  // Only flip visibility — never touch the layout container or re-add widgets.
  // When an Animatic shares the room it owns the shot ops AND the Shots/Animatic
  // export buttons (moved to the timeline); the Board keeps PDF export and, in a
  // Board-only room, the full set (nothing hidden).
  QToolButton *shared[] = {m_addShotButton,     m_deleteButton,
                           m_mergeButton,       m_copyButton,
                           m_cloneButton,       m_pasteButton,
                           m_exportShotsButton, m_exportAnimaticButton};
  for (QToolButton *b : shared)
    if (b) b->setVisible(!hidden);
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
          // Svuotare la pixmap del widget non basta piu': il render vive nella
          // cache CONDIVISA, e senza questo updateVisiblePreviews() rileggerebbe
          // quello vecchio — l'anteprima resterebbe indietro rispetto al disegno.
          if (m_shots[si].childLevel)
            ZtoryModel::instance()->invalidatePanelRenders(
                QString::fromStdWString(m_shots[si].childLevel->getName()));
          for (PanelWidget *pw : m_shots[si].panels)
            pw->setPreviewPixmap(QPixmap());
          break;
        }
      }
      m_dirtyShotCol = -1;
    }
    // Normalise widths now that the viewport is real: shots added while this
    // Board lived in another room were never sized (see applyPanelWidths).
    applyPanelWidths();
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
      // PanelWidget::resizeEvent fires automatically on each setFixedWidth call:
      //   → rescalePreview() updates display immediately
      //   → previewRerenderNeeded emitted if pixmap resolution is insufficient
      applyPanelWidths();
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
  // Il puntatore identifica l'ISTANZA: senza, tre righe uguali con shots=0 non
  // dicono se e' un pannello rifatto tre volte o tre pannelli diversi che si
  // inizializzano — due diagnosi opposte.
  qWarning("[ZTORY] refreshFromScene: panel=%p shots(before)=%d ancestors=%d",
           (void *)this, (int)m_shots.size(),
           scene->getChildStack()->getAncestorCount());
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
      // The trailing Stop Frame Hold placed by resequenceXsheet() is a non-empty
      // child cell (isEmpty() only checks m_level), so it would inflate duration
      // by +1 here. Treat it as the column terminator, not a counted frame —
      // otherwise the shot reads 1 frame too long and the +1 leaks into the
      // panel-visibility filter, producing a phantom 1-frame panel.
      if (cell.getFrameId().isStopFrame()) {
        if (duration > 0) break;
        continue;
      }
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        if (!cl) cl = cell.m_level->getChildLevel();
        duration++;
      } else if (duration > 0) break;
    }
    if (!cl) continue;
    Shot shot;
    shot.childLevel = cl;  // scene-side identity, immune to column shifts
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
  // Identity-preserving label restore (Keep mode). loadZtoryc() assigned labels
  // by positional index, which misaligns after a middle insert from the timeline:
  // the new empty column steals that index's label and every later shot's drawing
  // appears to shift up (SH020 drawing → SH030). The xsheet column's stage-object
  // name IS the shot label (kept in sync by updateColumnName) and it moves WITH
  // the column on insert, so it's the reliable identity. Re-derive each shot's
  // label from its column name: existing columns keep their real label; the
  // freshly inserted column (default, non-label name) is cleared so renumberAll()
  // gives it a midpoint label between its neighbours. Auto mode reassigns all by
  // position anyway, so this only matters (and only runs) for Keep.
  if (!ZtoryModel::instance()->autoRenumber()) {
    const QString pfx = ZtoryModel::instance()->numberingConfig().shotPrefix;
    TStageObjectTree *tree = xsh->getStageObjectTree();
    for (int i = 0; i < (int)m_shots.size(); i++) {
      int col = m_shots[i].data.xsheetColumn;
      TStageObject *obj =
          tree ? tree->getStageObject(TStageObjectId::ColumnId(col), false) : nullptr;
      const QString name = obj ? QString::fromStdString(obj->getName()) : QString();
      const bool looksLikeLabel = name.startsWith(pfx) &&
                                  name.length() > pfx.length() &&
                                  name.at(pfx.length()).isDigit();
      if (looksLikeLabel) {
        m_shots[i].data.shotLabel  = name;
        m_shots[i].data.shotNumber = name;
      } else {
        m_shots[i].data.shotLabel.clear();  // fresh column → renumberAll midpoints it
      }
    }
  }
  // Freeze numbering BEFORE renumberAll: a Kitsu-linked project stores labels
  // with Keep-mode suffixes (e.g. SH040A); auto-renumber here would overwrite
  // them with clean sequential labels and desync the Kitsu links.
  updateNumberingLock();
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
  // Re-detect panels for every shot while still in the main-xsheet context, so
  // partial durations are correct right away. Without this pass the panels stay
  // whatever the placeholder/.ztoryc said — often a single panel whose partial
  // equals the shot's TOTAL duration — until the shot is entered, because the
  // panel-detect timer only fires from inside a sub-scene. Main context only:
  // in sub context detectAndUpdatePanels reads the currently open xsheet and is
  // valid only for the open shot.
  if (scene->getChildStack()->getAncestorCount() == 0)
    for (int si = 0; si < (int)m_shots.size(); si++) detectAndUpdatePanels(si);
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
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  // Shared clipboard is the single source of truth (shared with Animatic).
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
  m_pasteButton->setEnabled(!ZtoryModel::instance()->sharedClip().empty());
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
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  // Shared clipboard is the single source of truth (shared with Animatic).
  // srcCol = -1: original deleted immediately below; cutLevel keeps the sub-scene
  // alive so paste can re-insert it without losing drawings.
  std::vector<ZtoryClipEntry> shared;
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ZtoryClipEntry ze;
    ze.srcCol   = -1;
    ze.duration = m_shots[idx].data.panels.empty()
                  ? 24 : m_shots[idx].data.panels[0].duration;
    ze.isCut    = true;
    ze.isClone  = false;
    if (xsh) {
      int col = m_shots[idx].data.xsheetColumn;
      TXshColumn *xshCol = xsh->getColumn(col);
      TXshLevelColumn *lc = xshCol ? xshCol->getLevelColumn() : nullptr;
      if (lc) {
        int r0 = 0, r1 = 0;
        lc->getRange(r0, r1);
        TXshCell cell = lc->getCell(r0);
        if (!cell.isEmpty()) ze.cutLevel = cell.m_level;
      }
    }
    shared.push_back(ze);
  }
  ZtoryModel::instance()->setSharedClip(std::move(shared));
  m_pasteButton->setEnabled(!ZtoryModel::instance()->sharedClip().empty());
  onDeleteShot();  // immediately remove from board and xsheet
}

void StoryboardPanel::onCloneShot() {
  // Auto-return to main xsheet before operating
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  // Shared clipboard is the single source of truth (shared with Animatic).
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
  m_pasteButton->setEnabled(!ZtoryModel::instance()->sharedClip().empty());
}




void StoryboardPanel::onPasteShot() {
  // The shared clipboard is the single source of truth (written by both Board
  // and Animatic copy/cut/clone). Paste delegates to the shared xsheet op, then
  // rebuilds the Board from the scene — same flow as ZtoryAnimaticPanel::onPasteShots.
  const auto &shared = ZtoryModel::instance()->sharedClip();
  if (shared.empty()) return;

  auto before = captureSnapshot();

  // Auto-return to main xsheet before pasting.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  // Insert after the selected shot, or at the end.
  int insertCol = m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size()
                  ? m_shots[m_selectedShotIndex].data.xsheetColumn + 1
                  : xsh->getColumnCount();
  ZtoryShotOps::pasteSharedClip(shared, insertCol, xsh, scene);
  xsh->updateFrameCount();
  // Drop one-shot entries (cut/clone) from the shared clip; keep plain copies so
  // they can be pasted again.
  {
    auto newShared = shared;
    newShared.erase(std::remove_if(newShared.begin(), newShared.end(),
                    [](const ZtoryClipEntry &e){ return e.isCut || e.isClone; }),
                    newShared.end());
    ZtoryModel::instance()->setSharedClip(std::move(newShared));
  }
  resequenceXsheet();
  refreshFromScene();
  m_pasteButton->setEnabled(!ZtoryModel::instance()->sharedClip().empty());

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

  // Remove all current shot columns. Shot columns are always first; audio (and
  // any other real, non-sub-scene level) columns follow.
  //
  // BUG FIX (undo of Delete Shot duplicated every shot): the old detection
  // classified a column as a shot only if it held a child-level cell, and broke
  // at the first column that didn't. But an *empty* shot — a shot made only of
  // empty/red cells, a valid Ztoryc state (duration counts empty cells) — has no
  // child-level cell, so the loop stopped early and removed too few columns. The
  // snapshot re-insert below then added the full set on top of the survivors,
  // duplicating every shot past the empty one (data corruption: cloned
  // sub-scenes). Instead, treat every leading column as a removable shot until
  // the first audio column or the first column carrying a non-sub-scene level.
  int currentShotCols = 0;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (column && column->getSoundColumn()) break;  // audio ends the shot region
    bool hasRealLevel = false;  // a non-sub-scene level → not a shot column
    int fc            = xsh->getFrameCount();
    for (int r = 0; r <= fc; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (cell.isEmpty() || !cell.m_level) continue;
      if (!cell.m_level->getChildLevel()) { hasRealLevel = true; break; }
    }
    if (hasRealLevel) break;
    currentShotCols++;  // child-level shot OR empty/red-cell placeholder shot
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
  // After undo/redo the grid is rebuilt with blank thumbnails. Defer the preview
  // render past this event loop so the QGridLayout has repositioned (and, in
  // Compact view, actually shown) the cards — otherwise the viewport/visibility
  // test runs on not-yet-laid-out widgets and skips them (Compact view showed
  // stale/blank cards after undo). onRefreshPreviews renders only visible panels
  // lacking a pixmap, so it is cheap and idempotent.
  QTimer::singleShot(0, this, &StoryboardPanel::onRefreshPreviews);
  // Re-anchor the path after clearShots() cleared it.
  m_currentZtoryPath = ztoryPath();
  saveZtoryc();
}

// ── UndoBoardState ────────────────────────────────────────────────────────────

void UndoBoardState::undo() const {
  // Levels must be back in the cast before the columns that reference them.
  if (!m_removedLevels.empty()) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TLevelSet *ls     = scene ? scene->getLevelSet() : nullptr;
    if (ls)
      for (const TXshLevelP &lvl : m_removedLevels)
        if (!ls->getLevel(lvl->getName())) ls->insertLevel(lvl.getPointer());
  }
  m_panel->restoreFromSnapshot(m_before);
}

void UndoBoardState::redo() const {
  m_panel->restoreFromSnapshot(m_after);
  // Drop them from the cast again once nothing exposes them; the smart
  // pointers here keep the objects alive for a later undo.
  if (!m_removedLevels.empty()) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TLevelSet *ls     = scene ? scene->getLevelSet() : nullptr;
    if (ls)
      for (const TXshLevelP &lvl : m_removedLevels)
        ls->removeLevel(lvl.getPointer(), false);
  }
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

  // Levels exposed by the shots being deleted: their sub-scene child level plus
  // everything used inside it (the OVL drawings).  Collected BEFORE the columns
  // go away; afterwards we drop from the cast only those left with no user, so
  // deleting a shot frees its level name again instead of leaving an orphan
  // that later forces export-to-board to disambiguate (or, before the guard,
  // to hang).  Shots sharing a level (Copy) keep it alive via isLevelUsed().
  std::set<TXshLevel *> shotLevels;
  {
    ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
    TXsheet *top    = scn ? scn->getChildStack()->getTopXsheet() : nullptr;
    if (top) {
      int frameCount = top->getFrameCount();
      for (int col : xshCols)
        for (int r = 0; r <= frameCount; r++) {
          TXshCell cell = top->getCell(r, col);
          if (cell.isEmpty() || !cell.m_level) continue;
          shotLevels.insert(cell.m_level.getPointer());
          if (TXshChildLevel *cl = cell.m_level->getChildLevel())
            cl->getXsheet()->getUsedLevels(shotLevels);
          break;  // one cell is enough: the column exposes a single sub-scene
        }
    }
  }

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

  // Purge the now-unused levels from the cast (kept alive by the undo item).
  std::vector<TXshLevelP> removedLevels;
  {
    ToonzScene *scn = TApp::instance()->getCurrentScene()->getScene();
    TXsheet *top    = scn ? scn->getChildStack()->getTopXsheet() : nullptr;
    TLevelSet *ls   = scn ? scn->getLevelSet() : nullptr;
    if (top && ls)
      for (TXshLevel *lvl : shotLevels) {
        if (!lvl || top->isLevelUsed(lvl)) continue;
        removedLevels.push_back(TXshLevelP(lvl));
        ls->removeLevel(lvl, false);  // keep alive: the undo item owns it now
      }
  }

  saveZtoryc();

  auto after = captureSnapshot();
  TUndoManager::manager()->add(
      new UndoBoardState(this, tr("Delete Shot"), std::move(before), std::move(after),
                         std::move(removedLevels)));
}

void StoryboardPanel::onAddShot() {
  auto before = captureSnapshot();

  // Enforce Keep numbering if shots already live in Kitsu, so this insert can't
  // renumber existing shots out from under their Kitsu links / statuses.
  updateNumberingLock();

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
      ZtoryShotOps::syncChildCameraToMain(xsh, cl);

      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }
  // The inserted column shifts every existing shot at/after insertAt one column
  // to the right in the xsheet. Keep their stored xsheetColumn in sync (mirror
  // of onDeleteShot). Without this, onEditShot() opens the wrong sub-scene for
  // every shot after an in-the-middle insertion (e.g. click last → enter
  // penultimate).
  for (Shot &s : m_shots)
    if (s.data.xsheetColumn >= insertAt) s.data.xsheetColumn++;

  Shot shot;
  shot.data.xsheetColumn = insertAt;
  PanelData pd;
  pd.startFrame = 0;
  pd.duration = duration;
  // Assign the uuid up front (before addPanelWidget renders the first preview):
  // updatePreview only warms the Production Tracker thumbnail cache when the shot
  // has a uuid, so without this the new shot's thumbnail is never cached and the
  // tracker shows a blank cell.
  shot.data.uuid = makeSourcedUuid(QFileInfo(ztoryPath()).fileName());
  m_shots.insert(m_shots.begin() + insertAt, shot);
  addPanelWidget(insertAt, 0);
  if (!ZtoryModel::instance()->autoRenumber()) assignKeepNumbers(insertAt);
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

void StoryboardPanel::onToggleCollapsePanels(bool on) {
  m_collapsePanels        = on;
  ZtoryBoardCollapsePanels = on ? 1 : 0;  // persist across sessions
  rebuildGrid();
  // Render the previews that just became visible (collapsed cards or, when
  // turning the mode off, the panels that were hidden).
  updateVisiblePreviews();
}

void StoryboardPanel::onPanelNavRequested(int shotIdx, int delta) {
  if (!m_collapsePanels) return;
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  Shot &shot = m_shots[shotIdx];
  int total  = (int)shot.panels.size();
  if (total <= 1) return;
  int next = qBound(0, shot.viewPanel + delta, total - 1);
  if (next == shot.viewPanel) return;

  PanelWidget *oldPw = shot.panels[shot.viewPanel];
  PanelWidget *newPw = shot.panels[next];

  // Swap only this one cell instead of rebuilding the whole grid: find the
  // cell occupied by the current card and drop the next one in its place.
  int idx = m_grid->indexOf(oldPw);
  int row = 0, col = 0, rspan = 1, cspan = 1;
  if (idx >= 0) m_grid->getItemPosition(idx, &row, &col, &rspan, &cspan);

  int colW = qMax(150, oldPw->width());  // reuse the laid-out card width

  m_grid->removeWidget(oldPw);
  oldPw->setPanelNavigator(false, total);
  oldPw->hide();

  shot.viewPanel = next;

  if (idx >= 0) m_grid->addWidget(newPw, row, col);
  newPw->setFixedWidth(colW);
  newPw->setPanelNavigator(true, total);
  newPw->show();
  newPw->updateGeometry();
  // Lazily render the thumbnail the first time this panel is shown.
  if (newPw->previewPixmap().isNull())
    updatePreview(shotIdx, next);
  else
    newPw->rescalePreview();
}

void StoryboardPanel::onNumberingChanged(int comboIndex) {
  if (comboIndex == 0) {
    ZtoryModel::instance()->setAutoRenumber(true);
    // Non rinumera subito - lo farà al prossimo addShot
  } else if (comboIndex == 1) {
    ZtoryModel::instance()->setAutoRenumber(false);
  } else if (comboIndex == 2) {
    ZtoryModel::instance()->setAutoRenumber(true);
    for (int i = 0; i < (int)m_shots.size(); i++)
      m_shots[i].data.shotNumber = QString("%1").arg(i+1, 2, 10, QChar(48));
    renumberAll();
    m_numberingCombo->blockSignals(true);
    m_numberingCombo->setCurrentIndex(0);
    m_numberingCombo->blockSignals(false);
  }
  saveZtoryc();
}

void StoryboardPanel::updateNumberingLock() {
  if (!m_numberingCombo) return;
  // Once shots are created in Kitsu, lock to Keep # so existing shot labels stay
  // put — auto/renumber would shift them (SH020→SH030…) and desync the Kitsu
  // links + Production Tracker statuses. New shots get a letter suffix instead.
  const bool lock = ZtoryModel::instance()->hasKitsuShots();
  if (lock) {
    ZtoryModel::instance()->setAutoRenumber(false);
    m_numberingCombo->blockSignals(true);
    m_numberingCombo->setCurrentIndex(1);  // "Keep #"
    m_numberingCombo->blockSignals(false);
    m_numberingCombo->setEnabled(false);
    m_numberingCombo->setToolTip(
        tr("Locked to Keep # — shots exist in Kitsu, so numbering is frozen to "
           "keep their links and statuses aligned."));
  } else {
    m_numberingCombo->setEnabled(true);
    m_numberingCombo->setToolTip(QString());
    // Mirror the GLOBAL numbering mode so every panel's combo agrees (Auto=0,
    // Keep=1) — the mode lives on the model, not per-panel.
    const int want = ZtoryModel::instance()->autoRenumber() ? 0 : 1;
    if (m_numberingCombo->currentIndex() != want && want < m_numberingCombo->count()) {
      m_numberingCombo->blockSignals(true);
      m_numberingCombo->setCurrentIndex(want);
      m_numberingCombo->blockSignals(false);
    }
  }
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
  if (ZtoryModel::instance()->autoRenumber()) {
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

namespace {
// Diagnostic log for the shot-export flows: one line per asset decision,
// dumped as ztoryc_export_log.txt next to the exported scenes. Debugging aid
// for the intermittent "+extras/audio missing after export" reports — remove
// once the asset transfer is proven solid.
QStringList g_ztoryExportLog;
void exportLog(const QString &s) { g_ztoryExportLog << s; }
void dumpExportLog(const TFilePath &dir) {
  if (g_ztoryExportLog.isEmpty()) return;
  QFile f(QString::fromStdWString(
      (dir + "ztoryc_export_log.txt").getWideString()));
  if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream ts(&f);
    for (const QString &l : g_ztoryExportLog) ts << l << "\n";
  }
  g_ztoryExportLog.clear();
}
}  // namespace

// ── Audio export helper ───────────────────────────────────────────────────────
// Injects a temporary sound column into childXsh containing only the audio
// that falls within [shotR0, shotR1] of the main xsheet.
// Returns the list of column indices inserted (to be removed after save);
// the sound levels created for the injection are appended to injectedLevels
// (to be removed from the scene cast after save — the wav FILES stay, they
// are the exported shot's audio asset).
//
// The shot's slice of each audio column is rendered to a physically TRIMMED
// wav in +extras (<baseName>_audio[N].wav) and exposed with a plain
// ColumnLevel (startFrame 0, no offsets). The previous approach cloned the
// original ColumnLevels (whole mp3 + trim in startFrame/offsets): OpenToonz
// cannot decode mp3 at all and neither stock app reliably honored the
// negative-startFrame trim, so the exported column showed up empty or with
// the whole track. A trimmed wav with zero offsets survives any target, and
// the '+'-coded path rides the same capture/copy machinery as the other
// project-folder assets.
static QList<int> injectAudioForShot(ToonzScene *scene, TXsheet *mainXsh,
                                     TXsheet *childXsh, int shotR0, int shotR1,
                                     double fps, const QString &baseName,
                                     QList<TXshLevel *> &injectedLevels) {
  QList<int> injected;
  int audioIdx = 0;
  // Incoming cross-dissolve: the sub-scene carries headHalf hold copies at
  // rows 0..headHalf-1 and the REAL content starts at row headHalf. The wav
  // slice is NET ([shotR0, shotR1] comes from shotTrueSpan, extras excluded),
  // so it must be placed at row headHalf — at row 0 it would play headHalf
  // frames early. The head-hold rows stay silent (that material belongs to
  // the previous shot's dissolve window).
  int headOffset = ZtoryShotOps::xdInHeadOffset(childXsh);
  int mainCols = mainXsh->getColumnCount();
  for (int mc = 0; mc < mainCols; mc++) {
    TXshColumn *col = mainXsh->getColumn(mc);
    if (!col) continue;
    TXshSoundColumn *srcSc = col->getSoundColumn();
    if (!srcSc) continue;

    // Skip columns with no audible material in [shotR0, shotR1].
    bool overlaps = false;
    for (int li = 0; li < srcSc->getColumnLevelCount() && !overlaps; li++) {
      ColumnLevel *cl = srcSc->getColumnLevel(li);
      if (cl && cl->getVisibleStartFrame() <= shotR1 &&
          cl->getVisibleEndFrame() >= shotR0)
        overlaps = true;
    }
    if (!overlaps) continue;

    // Render the slice. toFrame is exclusive (same convention as scrub()).
    TSoundTrackP st = srcSc->getOverallSoundTrack(shotR0, shotR1 + 1, fps);
    if (!st || st->getSampleCount() == 0) continue;

    audioIdx++;
    QString wavName = baseName + "_audio" +
                      (audioIdx > 1 ? QString::number(audioIdx) : QString()) +
                      ".wav";
    TFilePath codedWav =
        TFilePath("+extras") + TFilePath(wavName.toStdWString());
    TFilePath absWav = scene->decodeFilePath(codedWav);
    try {
      TSystem::touchParentDir(absWav);
      if (TSystem::doesExistFileOrLevel(absWav))
        TSystem::removeFileOrLevel(absWav);
      TSoundTrackWriter::save(absWav, st);
    } catch (...) {
      exportLog(QString("[%1] audio-trim WRITE FAILED %2")
                    .arg(baseName, absWav.getQString()));
      continue;
    }

    // loadLevel registers the level in the cast and codes the path back to
    // +extras/<wavName> relative to the current project.
    TXshLevel *lv = nullptr;
    try {
      lv = scene->loadLevel(absWav);
    } catch (...) {
    }
    if (!lv || !lv->getSoundLevel()) {
      exportLog(QString("[%1] audio-trim LOAD FAILED %2")
                    .arg(baseName, absWav.getQString()));
      continue;
    }
    injectedLevels.append(lv);
    exportLog(QString("[%1] audio-trim wrote %2 (frames %3-%4)")
                  .arg(baseName, absWav.getQString())
                  .arg(shotR0)
                  .arg(shotR1));

    // Insert a new sound column at the end of the child xsheet.
    int newCol = childXsh->getColumnCount();
    childXsh->insertColumn(newCol, TXshColumn::eSoundType);
    TXshSoundColumn *dstSc = childXsh->getColumn(newCol)->getSoundColumn();
    if (!dstSc) continue;
    dstSc->setFrameRate(fps);
    // adoptLevel() is the public counterpart of the protected
    // insertColumnLevel(): it takes ownership and places the visible start at
    // the target frame — headOffset, where the shot's real content begins.
    dstSc->adoptLevel(new ColumnLevel(lv->getSoundLevel(), 0, 0, 0, fps),
                      headOffset);

    // Mark column as reserved audio (visible in xsheet but !a drawing col)
    TStageObject *obj = childXsh->getStageObjectTree()
                          ->getStageObject(TStageObjectId::ColumnId(newCol), false);
    if (obj) obj->setName("_audio_main_");

    injected.append(newCol);
  }
  return injected;
}

// Toglie le colonne aggiunte SOLO per l'export (audio iniettato, lip sync), in
// ordine decrescente perche' togliere dall'inizio sposta sotto i piedi gli
// indici che restano. Chi ne toglie due gruppi cominci da quello con gli
// indici piu' alti: fra un gruppo e l'altro il rimescolamento avviene lo
// stesso.
// L'etichetta con cui uno shot si chiama davvero. Nei due popup di export il
// range si sceglieva con due contatori 1..N: per dire «da sh010 a sh030» si
// doveva scrivere «da 1 a 3», cioe' tradurre a mente la numerazione dello
// storyboard in un ordinale. Le tendine mostrano l'etichetta e non c'e' piu'
// niente da tradurre (richiesta di Franco, 2026-08-17).
static QString shotRangeLabel(const ShotData &sd, const ZtoryModel *m) {
  QString seq;
  for (const SequenceData &sq : m->sequences())
    if (sq.uuid == sd.sequenceId) { seq = sq.label; break; }
  const QString lbl = sd.label();
  return seq.isEmpty() ? lbl : (seq + " " + lbl);
}

// La casella «genera il lip sync» dei due popup di export, allestita in un
// posto solo: quale sia la scelta di prima, e perche' non si possa spuntare
// quando manca il motore. Spenta e senza spiegazione, l'utente prova a
// cliccarla e non capisce.
// La scelta resta quella dell'ultima volta finche' l'applicazione e' aperta:
// chi esporta col lip sync lo rifa' quasi sempre, e ricominciare da spenta a
// ogni export e' una spunta da rimettere ogni volta.
static bool g_ztoryExportLipSync = false;

static void addLipSyncOption(QCheckBox *chk) {
  const QString why = ZtoryLipSync::unavailableReason();
  chk->setEnabled(why.isEmpty());
  chk->setChecked(why.isEmpty() && g_ztoryExportLipSync);
  chk->setToolTip(
      why.isEmpty()
          ? QObject::tr(
                "Each exported shot comes out with its dialogue columns "
                "already written: the words from the storyboard panels, the "
                "timing from the shot's audio.\n"
                "Shots with no dialogue or no audio are exported as usual — "
                "the reason is written in ztoryc_export_log.txt.\n"
                "Shots that already have lip sync columns keep the ones they "
                "have.")
          : why);
}

// La casella «importa gli asset del breakdown» dei due popup. Il nome dice
// COSA fa e DA DOVE prende: «automatic import» direbbe quando e non cosa, e si
// confonderebbe con le opzioni di organizzazione degli asset li' accanto.
static bool g_ztoryExportAssets = false;

static void addAssetImportOption(QCheckBox *chk) {
  chk->setChecked(g_ztoryExportAssets);
  chk->setToolTip(QObject::tr(
      "Each exported shot is born with the assets its breakdown lists: "
      "characters as sub-scenes, props and backgrounds as levels.\n"
      "Before exporting, the assets are checked and anything that does not "
      "resolve is reported, so you can go on without it or stop and fix it."));
}

// Il controllo PRIMA di esportare, e il rapporto. Ritorna false se l'utente ha
// scelto di interrompere per sistemare cio' che manca.
// Il rapporto e' in un dialogo con il testo scorribile e non in un QMessageBox:
// gli asset che non si risolvono possono essere decine, e nascosti dietro a
// «mostra dettagli» non li guarda nessuno.
static bool ztoryConfirmShotAssets(QWidget *parent, const QStringList &uuids) {
  const QVector<ZtoryAssetCheck> checks = ztoryCheckShotAssets(uuids);
  // Gli shot guardati, come li conosce il tracker. Servono nel messaggio del
  // caso vuoto: un progetto ha piu' storyboard, ognuno con il suo SH010, e
  // vedere quali shot sono stati controllati e' l'unico modo per accorgersi
  // che il breakdown che si ha davanti e' di un altro storyboard.
  QStringList looked;
  for (const QString &u : uuids)
    for (const ProjectShot &ps : ZtoryModel::instance()->projectShots())
      if (ps.uuid == u) {
        QString src = ps.source;
        src.remove(QRegularExpression("\\.ztoryc$",
                                      QRegularExpression::CaseInsensitiveOption));
        looked << QString("%1 %2 (%3)")
                      .arg(ps.seq.isEmpty() ? QString("—") : ps.seq, ps.label,
                           src.isEmpty() ? QObject::tr("no storyboard") : src);
        break;
      }
  // Nessun asset da nessuna parte: la casella e' spuntata e non farebbe
  // NIENTE. Dirlo prima, perche' un'opzione che non fa niente in silenzio e'
  // peggio di un errore — l'utente resta convinto di avere gli asset dentro.
  if (checks.isEmpty()) {
    QString msg = QObject::tr(
        "None of the shots being exported has anything in its breakdown, so "
        "there is nothing to import.");
    if (looked.isEmpty())
      msg += QObject::tr(
          "\n\nIn fact these shots are not in the Production Tracker at all: "
          "save the storyboard so they get published to the project.");
    else
      msg += QObject::tr("\n\nShots checked:\n%1").arg(looked.join("\n"));
    // Il breakdown si riempie per SHOT, e uno shot appartiene a UNO storyboard:
    // se nel Breakdown si vedono asset su uno shot con la stessa etichetta,
    // guardare a quale storyboard appartiene.
    msg += QObject::tr(
        "\n\nThe breakdown is filled in the Production Tracker, or pulled "
        "from Kitsu. Mind that two storyboards in the same project can both "
        "have a shot called SH010: the Breakdown tab now shows which "
        "storyboard each row belongs to.");
    return QMessageBox::question(parent,
                                 QObject::tr("Assets from the Breakdown"), msg,
                                 QMessageBox::Ok | QMessageBox::Cancel) ==
           QMessageBox::Ok;
  }
  const QString report = ztoryAssetReport(checks);
  if (report.isEmpty()) return true;  // niente da segnalare: si esporta e basta

  int missing = 0;
  for (const ZtoryAssetCheck &c : checks)
    if (!c.ok()) missing++;

  QDialog dlg(parent);
  dlg.setWindowTitle(QObject::tr("Assets from the Breakdown"));
  dlg.setMinimumSize(560, 380);
  auto *lay = new QVBoxLayout(&dlg);
  auto *head = new QLabel(
      QObject::tr("%1 of the %2 assets these shots need cannot be resolved:")
          .arg(missing)
          .arg(checks.size()),
      &dlg);
  head->setWordWrap(true);
  lay->addWidget(head);
  auto *text = new QPlainTextEdit(report, &dlg);
  text->setReadOnly(true);
  lay->addWidget(text);
  auto *foot = new QLabel(
      QObject::tr("Export anyway and the missing ones are simply skipped; the "
                  "rest is imported."),
      &dlg);
  foot->setWordWrap(true);
  lay->addWidget(foot);
  auto *bbox = new QDialogButtonBox(&dlg);
  QPushButton *go =
      bbox->addButton(QObject::tr("Export anyway"), QDialogButtonBox::AcceptRole);
  bbox->addButton(QObject::tr("Stop and fix"), QDialogButtonBox::RejectRole);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  (void)go;
  return dlg.exec() == QDialog::Accepted;
}

static void removeExportColumns(TXsheet *childXsh, QList<int> cols) {
  std::sort(cols.begin(), cols.end(), std::greater<int>());
  for (int c : cols)
    childXsh->removeColumn(c);
}

//-----------------------------------------------------------------------------
// Lip sync dal Board, senza entrare in ogni shot: le colonne restano nello
// storyboard e servono a CONTROLLARE il sincrono prima di esportare. Stessa
// catena dell'export (un giro dentro la sotto-scena e uno fuori), ma qui
// nessuno le toglie dopo.
//-----------------------------------------------------------------------------
void StoryboardPanel::onLipSyncShots() {
  const QString why = ZtoryLipSync::unavailableReason();
  if (!why.isEmpty()) {
    QMessageBox::warning(this, tr("Generate Lip Sync Columns"), why);
    return;
  }
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Generate Lip Sync Columns"),
                             tr("No shots."));
    return;
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  // La scelta «selezionati / tutti» si FA nella finestra, con i numeri veri
  // davanti: la stessa riga del popup di export, che l'utente ha gia' visto.
  // Prima la regola era implicita — «se c'e' una selezione uso quella» — e chi
  // aveva cliccato uno shot per sbaglio lanciava una passata su uno shot solo
  // credendo di averla lanciata su tutti.
  QList<int> selected;
  for (int i : m_selectedIndices)
    if (i >= 0 && i < (int)m_shots.size()) selected.append(i);
  std::sort(selected.begin(), selected.end());

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Generate Lip Sync Columns"));
  dlg.setMinimumWidth(480);
  auto *lay = new QVBoxLayout(&dlg);
  auto *txt = new QLabel(
      tr("The words come from the storyboard panels, the timing from each "
         "shot's audio. Shots that already have lip sync columns get them "
         "rewritten; shots with no dialogue or no audio are listed at the end "
         "with the reason."),
      &dlg);
  txt->setWordWrap(true);
  lay->addWidget(txt);

  auto *row     = new QHBoxLayout();
  auto *selRadio = new QRadioButton(
      tr("Selected (%1)").arg(selected.size()), &dlg);
  auto *allRadio = new QRadioButton(
      tr("All shots (%1)").arg((int)m_shots.size()), &dlg);
  selRadio->setEnabled(!selected.isEmpty());
  if (!selected.isEmpty())
    selRadio->setChecked(true);
  else
    allRadio->setChecked(true);
  row->addWidget(new QLabel(tr("Shots:"), &dlg));
  row->addWidget(selRadio);
  row->addWidget(allRadio);
  row->addStretch();
  lay->addLayout(row);

  auto *bbox = new QDialogButtonBox(&dlg);
  bbox->addButton(tr("Generate"), QDialogButtonBox::AcceptRole);
  bbox->addButton(QDialogButtonBox::Cancel);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  QList<int> indices;
  if (selRadio->isChecked())
    indices = selected;
  else
    for (int i = 0; i < (int)m_shots.size(); i++) indices.append(i);
  if (indices.isEmpty()) return;

  QProgressDialog progress(tr("Lip sync…"), tr("Cancel"), 0, indices.size(),
                           this);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);

  int done = 0, skipped = 0;
  QStringList reasons;

  for (int n = 0; n < indices.size(); n++) {
    const int i = indices[n];
    progress.setValue(n);
    const ShotData &sd = m_shots[i].data;
    progress.setLabelText(tr("Lip sync: %1").arg(sd.label()));
    qApp->processEvents();
    if (progress.wasCanceled()) break;

    const int shotCol = sd.xsheetColumn;
    int shotR0 = 0, shotR1 = -1;
    int trueStart = 0, trueDur = 0;
    if (ZtoryShotOps::shotTrueSpan(mainXsh, shotCol, trueStart, trueDur) &&
        trueDur > 0) {
      shotR0 = trueStart;
      shotR1 = trueStart + trueDur - 1;
    } else if (mainXsh->getColumn(shotCol)) {
      mainXsh->getColumn(shotCol)->getRange(shotR0, shotR1,
                                            /*ignoreLastStop=*/true);
    }
    if (shotR1 < shotR0) { skipped++; continue; }

    TApp::instance()->getCurrentColumn()->setColumnIndex(shotCol);
    TColumnSelection *colSel = new TColumnSelection();
    colSel->selectColumn(shotCol, true);
    TSelection::setCurrent(colSel);
    ztoryOpenSubXsheet();
    if (scene->getChildStack()->getAncestorCount() == 0) { skipped++; continue; }
    TXsheet *childXsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    ZtoryShotContext ctx;
    ctx.shotIndex = i;
    ctx.column    = shotCol;
    ctx.firstRow  = shotR0;
    ctx.lastRow   = shotR1;
    ctx.subXsheet = childXsh;

    ZtoryLipSync::Request req;
    const QString skipWhy =
        ztoryPrepareLipSync(ctx, ztoryShotDialogue(sd.panels), req);
    if (!skipWhy.isEmpty()) {
      skipped++;
      // Il motivo, non un conteggio: «niente dialogo» e «niente audio» si
      // sistemano in due posti diversi.
      const QString line = tr("%1: %2").arg(sd.label(), skipWhy);
      if (!reasons.contains(line)) reasons << line;
      ztoryCloseSubXsheet(1);
      continue;
    }

    QVector<ZtoryCharacterTrack> tracks;
    QString msg;
    if (ztoryRunLipSyncBlocking(req, tracks, msg)) {
      // Rigenerare vuol dire sostituire: le vecchie si tolgono solo ora che le
      // nuove ci sono davvero.
      for (int c : ztoryFindLipSyncColumns(childXsh)) childXsh->removeColumn(c);
      ztoryWriteLipSyncColumns(childXsh, tracks, shotR1 - shotR0 + 1);
      done++;
    } else {
      skipped++;
      const QString line = tr("%1: %2").arg(sd.label(), msg);
      if (!reasons.contains(line)) reasons << line;
    }
    ztoryCloseSubXsheet(1);
  }
  progress.setValue(indices.size());

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  QString text = tr("%1 shot(s) done").arg(done);
  if (skipped > 0) text += tr(", %1 skipped").arg(skipped);
  if (!reasons.isEmpty()) text += "\n\n" + reasons.join("\n");
  QMessageBox::information(this, tr("Generate Lip Sync Columns"), text);
}

//-----------------------------------------------------------------------------

void StoryboardPanel::onExportShots() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export Shots"), tr("No shots to export."));
    return;
  }

  ZtoryModel *model = ZtoryModel::instance();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  // ── Dialog ───────────────────────────────────────────────────────────────
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Export Shots as Scenes"));
  dlg.setMinimumWidth(520);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setSpacing(8);
  lay->setContentsMargins(14, 14, 14, 14);
  auto *form = new QFormLayout();
  form->setLabelAlignment(Qt::AlignRight);

  // Output directory
  auto *dirRow = new QHBoxLayout();
  auto *dirEdit = new QLineEdit(&dlg);
  TFilePath scenesDir = scene->decodeFilePath(TFilePath("+scenes"));
  dirEdit->setText(QString::fromStdWString(scenesDir.getWideString()));
  auto *browseBtn = new QPushButton(tr("…"), &dlg);
  browseBtn->setFixedWidth(28);
  dirRow->addWidget(dirEdit);
  dirRow->addWidget(browseBtn);
  form->addRow(tr("Output folder:"), dirRow);
  QObject::connect(browseBtn, &QPushButton::clicked, [&] {
    QString d = QFileDialog::getExistingDirectory(&dlg, tr("Output folder"),
                                                  dirEdit->text());
    if (!d.isEmpty()) dirEdit->setText(d);
  });

  // Model A: one .tnz per shot reused across all pipeline steps. No task stage
  // is picked here — the task belongs only to the render/clip name (Output
  // Settings → "Name from project pattern"). On export the shot's tasks advance
  // Todo→Ready; on first open of the shot scene they advance Ready→Wip.

  // Version
  auto *verSpin = new QSpinBox(&dlg);
  verSpin->setRange(1, 99);
  verSpin->setValue(1);
  verSpin->setPrefix("v");
  form->addRow(tr("Version:"), verSpin);

  // Range
  auto *rangeRow = new QHBoxLayout();
  auto *allRadio   = new QRadioButton(tr("All shots"), &dlg);
  auto *rangeRadio = new QRadioButton(tr("Range:"), &dlg);
  allRadio->setChecked(true);
  auto *fromCombo = new QComboBox(&dlg);
  auto *toCombo   = new QComboBox(&dlg);
  auto *toLabel   = new QLabel(tr("to"), &dlg);
  for (const auto &sh : m_shots) {
    const QString t = shotRangeLabel(sh.data, model);
    fromCombo->addItem(t);
    toCombo->addItem(t);
  }
  toCombo->setCurrentIndex((int)m_shots.size() - 1);
  fromCombo->setEnabled(false); toCombo->setEnabled(false); toLabel->setEnabled(false);
  rangeRow->addWidget(allRadio); rangeRow->addWidget(rangeRadio);
  rangeRow->addWidget(fromCombo); rangeRow->addWidget(toLabel);
  rangeRow->addWidget(toCombo); rangeRow->addStretch();
  form->addRow(tr("Shots:"), rangeRow);
  QObject::connect(rangeRadio, &QRadioButton::toggled, [&](bool on) {
    fromCombo->setEnabled(on); toCombo->setEnabled(on); toLabel->setEnabled(on);
  });

  // Back-link checkbox (B3c)
  auto *backLinkChk = new QCheckBox(
      tr("Write project back-link (.ztoryc with role=\"shot\")"), &dlg);
  backLinkChk->setChecked(!model->projectDbPath().isEmpty());
  backLinkChk->setEnabled(!model->projectDbPath().isEmpty());
  if (model->projectDbPath().isEmpty())
    backLinkChk->setToolTip(tr("Save the scene first to create a project DB."));
  form->addRow(QString(), backLinkChk);

  // Lip sync: lo shot esportato nasce con le colonne del dialogo gia' fatte.
  auto *lipSyncChk = new QCheckBox(tr("Generate lip sync columns"), &dlg);
  addLipSyncOption(lipSyncChk);
  form->addRow(QString(), lipSyncChk);

  auto *assetsChk = new QCheckBox(
      tr("Import each shot's assets from the breakdown"), &dlg);
  addAssetImportOption(assetsChk);
  form->addRow(QString(), assetsChk);

  // Naming preview
  auto *previewLabel = new QLabel(&dlg);
  previewLabel->setStyleSheet("color: #aaa; font-size: 11px;");
  form->addRow(tr("Preview:"), previewLabel);

  lay->addLayout(form);
  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  // Model A: the exported .tnz is a single scene reused across pipeline steps,
  // so the shot file name must NOT carry the {TASK} token (the task belongs only
  // to the render/clip name, set later from Output Settings). Strip {TASK} and a
  // neighbouring separator from the project pattern.
  QString exportPattern = model->namingPattern();
  if (exportPattern.isEmpty())
    exportPattern = "{PROD}_{SEASON}_{EP}_{SEQ}_{SHOT}_{TASK}_V{VER:02}";
  exportPattern.remove(QRegularExpression(
      R"((?:[_\-.]\{TASK(?::\d+)?\})|(?:\{TASK(?::\d+)?\}[_\-.])|\{TASK(?::\d+)?\})"));

  // Update preview whenever relevant fields change
  auto updatePreview = [&] {
    if (m_shots.empty()) return;
    const ShotData &sd = m_shots[0].data;
    QString seqLabel;
    for (const SequenceData &seq : model->sequences())
      if (seq.uuid == sd.sequenceId) { seqLabel = seq.label; break; }
    QMap<QString,QString> tok;
    tok["PROD"]   = model->production();
    tok["CODE"]   = model->effectiveCode();  // derived when never filled in
    tok["SEASON"] = model->season();
    tok["EP"]     = model->episode();
    tok["SEQ"]    = seqLabel;
    tok["SHOT"]   = sd.label();
    tok["VER"]    = QString::number(verSpin->value());
    previewLabel->setText(
        ZtoryModel::resolvePattern(exportPattern, tok) + ".tnz  (first shot)");
  };
  QObject::connect(verSpin, qOverload<int>(&QSpinBox::valueChanged),
                   [&](int) { updatePreview(); });
  updatePreview();

  if (dlg.exec() != QDialog::Accepted) return;

  // ── Export ───────────────────────────────────────────────────────────────
  int from = allRadio->isChecked() ? 0 : fromCombo->currentIndex();
  int to   = allRadio->isChecked() ? (int)m_shots.size() - 1
                                   : toCombo->currentIndex();
  // Scelti a rovescio: si esporta lo stesso l'intervallo che ha in mente chi
  // l'ha scelto, invece di non esportare niente senza dire perche'.
  if (from > to) std::swap(from, to);
  QList<int> indices;
  for (int i = from; i <= to; i++) indices.append(i);

  ShotExportOptions opts;
  opts.writeLink       = backLinkChk->isChecked();
  opts.lipSync         = lipSyncChk->isChecked();
  opts.importAssets    = assetsChk->isChecked();
  g_ztoryExportLipSync = opts.lipSync;
  g_ztoryExportAssets  = opts.importAssets;

  // Il controllo degli asset viene PRIMA di toccare il disco: chi si ferma per
  // sistemare cio' che manca non deve trovarsi mezza cartella gia' esportata.
  if (opts.importAssets) {
    QStringList uuids;
    for (int i : indices)
      if (!m_shots[i].data.uuid.isEmpty()) uuids << m_shots[i].data.uuid;
    if (!ztoryConfirmShotAssets(this, uuids)) return;
  }

  int fail = 0;
  const TFilePath outDir(dirEdit->text().trimmed().toStdWString());
  QList<TFilePath> exported =
      exportShotScenesToDir(indices, outDir, verSpin->value(), opts, fail);
  dumpExportLog(outDir);

  QString msg =
      tr("Export complete: %1 shot(s) exported").arg(exported.size());
  if (fail > 0) msg += tr(", %1 failed").arg(fail);
  QMessageBox::information(this, tr("Export Shots"), msg);
}

//-----------------------------------------------------------------------------

QList<TFilePath> StoryboardPanel::exportShotScenesToDir(
    const QList<int> &indices, const TFilePath &outDirFp, int version,
    const ShotExportOptions &opts, int &fail,
    QHash<QString, QList<QPair<TFilePath, TFilePath>>> *assetCopies) {
  const bool writeLink = opts.writeLink;
  QList<TFilePath> exported;
  ZtoryModel *model = ZtoryModel::instance();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return exported;

  const QString projectDb = model->projectDbPath();
  TXsheet *mainXsh        = scene->getChildStack()->getTopXsheet();
  double fps              = (double)model->fps();

  g_ztoryExportLog.clear();
  exportLog(QString("[export] %1 shot(s) -> %2")
                .arg(indices.size())
                .arg(outDirFp.getQString()));

  if (!TFileStatus(outDirFp).doesExist()) TSystem::mkDir(outDirFp);

  (void)version;  // exported .tnz name uses the shot convention, not a version

  for (int i : indices) {
    if (i < 0 || i >= (int)m_shots.size()) { fail++; continue; }
    const ShotData &sd = m_shots[i].data;

    // The exported .tnz follows the shot naming convention: SEQ_SHOT (e.g.
    // "sq01_sh010") or just SHOT ("sh010") when the shot has no sequence. The
    // full production naming pattern is NOT used here — with empty tokens it
    // produced malformed names full of stray separators (e.g. "_01___sh010_V01").
    QString seqLabel;
    for (const SequenceData &seq : model->sequences())
      if (seq.uuid == sd.sequenceId) { seqLabel = seq.label; break; }
    const QString shotLbl = sd.label().trimmed();
    const QString seqLbl  = seqLabel.trimmed();
    QString baseName = seqLbl.isEmpty() ? shotLbl : (seqLbl + "_" + shotLbl);
    if (baseName.isEmpty()) baseName = "shot_" + sd.label();
    TFilePath outPath = outDirFp + TFilePath(baseName.toStdString() + ".tnz");

    int shotCol = sd.xsheetColumn;
    int shotR0 = 0, shotR1 = 0;
    int trueStart = 0, trueDur = 0;
    if (mainXsh &&
        ZtoryShotOps::shotTrueSpan(mainXsh, shotCol, trueStart, trueDur) &&
        trueDur > 0) {
      // Use the TRUE on-timeline span (excludes the cross-dissolve overlap cells)
      // so the injected audio matches the exported sub-scene duration. getRange()
      // includes the dissolve overlap, making the audio overshoot the shot.
      shotR0 = trueStart;
      shotR1 = trueStart + trueDur - 1;
    } else if (mainXsh && mainXsh->getColumn(shotCol)) {
      mainXsh->getColumn(shotCol)->getRange(shotR0, shotR1, /*ignoreLastStop=*/true);
    }

    TApp::instance()->getCurrentColumn()->setColumnIndex(shotCol);
    TColumnSelection *colSel = new TColumnSelection();
    colSel->selectColumn(shotCol, true);
    TSelection::setCurrent(colSel);
    ztoryOpenSubXsheet();

    if (scene->getChildStack()->getAncestorCount() == 0) { fail++; continue; }

    TXsheet *childXsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    QList<int> injectedCols;
    QList<TXshLevel *> injectedLevels;
    if (mainXsh && childXsh && shotR1 >= shotR0)
      injectedCols = injectAudioForShot(scene, mainXsh, childXsh, shotR0,
                                        shotR1, fps, baseName, injectedLevels);

    // ── Gli asset del breakdown, dentro lo shot ─────────────────────────────
    // Lo shot esportato nasce gia' popolato: i personaggi come sotto-scene, il
    // resto come livelli. Come per l'audio e il lip sync, le colonne stanno
    // nella sotto-scena CONDIVISA con lo storyboard e vanno tolte dopo il
    // salvataggio.
    ZtoryImportedAssets imported;
    int undoBefore = TUndoManager::manager()->getHistoryCount();
    if (opts.importAssets && childXsh && !sd.uuid.isEmpty()) {
      imported = ztoryImportShotAssets(sd.uuid, childXsh);
      for (const QString &line : imported.log)
        exportLog(QString("[%1] asset %2").arg(baseName, line));
    }

    // ── Lip sync: lo shot esce con le colonne parole+bocche gia' fatte ──────
    // Le colonne stanno nella sotto-scena, che e' LA STESSA dello storyboard:
    // vanno tolte dopo il salvataggio come l'audio iniettato, o al secondo
    // export lo storyboard ne avrebbe quattro, al terzo sei.
    QList<int> lipSyncCols;
    if (opts.lipSync && childXsh && shotR1 >= shotR0) {
      // Quelle che ci sono gia' — generate come controllo durante lo
      // storyboard — valgono piu' di quelle che genereremmo ora: sono quelle
      // che l'utente ha visto e magari corretto. Si esportano com'e' e non si
      // toccano.
      if (!ztoryFindLipSyncColumns(childXsh).isEmpty()) {
        exportLog(QString("[%1] lip sync: columns already in the sub-scene, kept")
                      .arg(baseName));
      } else {
        ZtoryShotContext lctx;
        lctx.shotIndex = i;
        lctx.column    = shotCol;
        lctx.firstRow  = shotR0;
        lctx.lastRow   = shotR1;
        lctx.subXsheet = childXsh;
        ZtoryLipSync::Request req;
        // Il copione dalla copia del Board, non dal modello: e' quella che
        // questo pannello sta esportando, e le due possono divergere.
        const QString why = ztoryPrepareLipSync(
            lctx, ztoryShotDialogue(m_shots[i].data.panels), req);
        if (!why.isEmpty()) {
          // Uno shot senza dialogo o senza audio non e' un errore di export:
          // e' la maggior parte degli shot. Va nel registro, non in un avviso.
          exportLog(QString("[%1] lip sync skipped: %2").arg(baseName, why));
        } else {
          QVector<ZtoryCharacterTrack> tracks;
          QString msg;
          if (ztoryRunLipSyncBlocking(req, tracks, msg)) {
            lipSyncCols = ztoryWriteLipSyncColumns(childXsh, tracks,
                                                   shotR1 - shotR0 + 1);
            exportLog(QString("[%1] lip sync: %2 — %3 columns")
                          .arg(baseName, msg)
                          .arg(lipSyncCols.size()));
          } else {
            exportLog(QString("[%1] lip sync FAILED: %2").arg(baseName, msg));
          }
        }
      }
    }

    // Capture the SOURCE file of each project-folder level (coded '+' path),
    // keyed by the level, BEFORE save. Neither takeCareSceneFolderItems (only
    // $scenefolder) nor the ResourceCollector (skips '+' paths) copies these, and
    // saving a renamed sub-scene relocates the per-scene +extras path so the
    // PNGs/audio would go missing. Run AFTER injectAudioForShot so the injected
    // animatic-audio sound level is included (trim lives in the column offsets).
    // Runs for BOTH export flows: the plain "Export Shots" misses these assets
    // too whenever the project organization makes '+' paths depend on the scene
    // name (e.g. "Assets next to each scene"). Unlike the old pre-save guessing
    // (which interfered with the native save and was gated out), this only READS
    // paths before the save and copies missing files after it — the native save
    // itself is untouched. Only the assetCopies recording stays project-export
    // specific.
    QList<QPair<TXshLevel *, TFilePath>> srcByLevel;
    if (childXsh) {
      std::set<TXshLevel *> usedLevels;
      childXsh->getUsedLevels(usedLevels);
      for (TXshLevel *lv : usedLevels) {
        if (!lv) continue;
        TFilePath coded;
        if (TXshSimpleLevel *sl = lv->getSimpleLevel())
          coded = sl->getPath();
        else if (TXshSoundLevel *snd = lv->getSoundLevel())
          coded = snd->getPath();
        else
          continue;
        if (coded.isEmpty() || coded.getWideString()[0] != L'+') continue;
        const TFilePath srcAbs = scene->decodeFilePath(coded);
        if (!srcAbs.isEmpty() && TSystem::doesExistFileOrLevel(srcAbs)) {
          srcByLevel.append(qMakePair(lv, srcAbs));
          exportLog(QString("[%1] capture coded=%2 src=%3")
                        .arg(baseName, coded.getQString(),
                             srcAbs.getQString()));
        } else {
          exportLog(QString("[%1] capture coded=%2 SRC MISSING at %3")
                        .arg(baseName, coded.getQString(),
                             srcAbs.getQString()));
        }
      }
    }

    bool saved = IoCmd::saveScene(outPath, IoCmd::SAVE_SUBXSHEET);

    // After save, copy each captured source to the level's FINAL resolved
    // location. Reading the post-save path aligns with whatever relocation the
    // save applied (instead of guessing before it), which is what made earlier
    // attempts flaky. Also record the final coded paths so the export-to-project
    // caller can copy them into the destination project after import.
    if (saved && childXsh && !srcByLevel.isEmpty()) {
      ToonzScene savedTs;
      savedTs.setProject(TProjectManager::instance()->getCurrentProject());
      savedTs.setScenePath(outPath);
      QList<QPair<TFilePath, TFilePath>> list;
      for (const auto &pr : srcByLevel) {
        TXshLevel *lv = pr.first;
        TFilePath finalCoded;
        if (TXshSimpleLevel *sl = lv->getSimpleLevel())
          finalCoded = sl->getPath();
        else if (TXshSoundLevel *snd = lv->getSoundLevel())
          finalCoded = snd->getPath();
        if (finalCoded.isEmpty()) continue;
        const TFilePath finalAbs = savedTs.decodeFilePath(finalCoded);
        if (!finalAbs.isEmpty() && finalAbs != pr.second &&
            !TSystem::doesExistFileOrLevel(finalAbs)) {
          try {
            TSystem::touchParentDir(finalAbs);
            TXshSimpleLevel::copyFiles(finalAbs, pr.second);
            if (!TSystem::doesExistFileOrLevel(finalAbs))
              TSystem::copyFile(finalAbs, pr.second);
            exportLog(QString("[%1] post-save copy %2 -> %3 : %4")
                          .arg(baseName, pr.second.getQString(),
                               finalAbs.getQString(),
                               TSystem::doesExistFileOrLevel(finalAbs)
                                   ? "OK"
                                   : "FAILED"));
          } catch (...) {
            exportLog(QString("[%1] post-save copy %2 -> %3 : EXCEPTION")
                          .arg(baseName, pr.second.getQString(),
                               finalAbs.getQString()));
          }
        } else {
          exportLog(QString("[%1] post-save %2 -> %3 : %4")
                        .arg(baseName, finalCoded.getQString(),
                             finalAbs.getQString(),
                             finalAbs == pr.second ? "same as src"
                             : TSystem::doesExistFileOrLevel(finalAbs)
                                 ? "already exists"
                                 : "empty resolve"));
        }
        if (assetCopies && finalCoded.getWideString()[0] == L'+')
          list.append(qMakePair(finalCoded, pr.second));
      }
      if (assetCopies && !list.isEmpty()) assetCopies->insert(baseName, list);
    }

    // Si tolgono a ritroso rispetto a come sono state aggiunte — lip sync,
    // asset, audio — perche' ogni gruppo sta a destra del precedente e
    // togliere prima quelli di sinistra farebbe scalare gli indici di tutti
    // gli altri: si finirebbe per togliere colonne dello storyboard.
    if (!lipSyncCols.isEmpty() && childXsh)
      removeExportColumns(childXsh, lipSyncCols);
    if (!imported.columns.isEmpty() && childXsh)
      removeExportColumns(childXsh, imported.columns);
    if (!injectedCols.isEmpty() && childXsh)
      removeExportColumns(childXsh, injectedCols);
    // I livelli importati escono anche dal cast dello storyboard: restarci
    // vorrebbe dire vedere il personaggio fra i livelli della scena dello
    // storyboard, dove non e' mai stato messo da nessuno.
    for (TXshLevel *lv : imported.levels)
      if (lv) scene->getLevelSet()->removeLevel(lv);
    // L'import passa dalle funzioni di caricamento di Tahoma, che scrivono
    // nella cronologia degli annullamenti. Le colonne pero' le abbiamo appena
    // tolte a mano: un annullamento le rimetterebbe in uno storyboard che non
    // le ha mai avute. Si toglie dalla cronologia cio' che l'import ci ha
    // messo.
    if (opts.importAssets) {
      const int added = TUndoManager::manager()->getHistoryCount() - undoBefore;
      if (added > 0) TUndoManager::manager()->popUndo(added);
    }
    // Drop the trimmed-audio levels from the storyboard scene's cast (the wav
    // files stay on disk — they are the exported shots' audio assets).
    for (TXshLevel *lv : injectedLevels)
      scene->getLevelSet()->removeLevel(lv);

    ztoryCloseSubXsheet(1);

    if (!saved) { fail++; continue; }
    exported.append(outPath);

    // B3c: write companion .ztoryc with role="shot" + back-link to project
    if (writeLink && !projectDb.isEmpty() && !sd.uuid.isEmpty()) {
      // Effective technique = the project DB value (edited in the Production
      // Tracker) is authoritative; fall back to the Board's per-shot value, then
      // the project default. This keeps the exported workflow and the per-task
      // Ready/Wip advances consistent with what the user set in the tracker.
      QString tech;
      for (const ProjectShot &ps : model->projectShots())
        if (ps.uuid == sd.uuid) { tech = ps.technique; break; }
      if (tech.isEmpty()) tech = sd.technique;
      if (tech.isEmpty()) tech = model->defaultTechnique();

      QString ztorcPath = QString::fromStdWString(outPath.getWideString());
      ztorcPath.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
      QFile f(ztorcPath);
      if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QXmlStreamWriter xml(&f);
        xml.setAutoFormatting(true);
        xml.writeStartDocument();
        xml.writeStartElement("ztoryc");
        xml.writeAttribute("version",     "2");
        xml.writeAttribute("role",        "shot");
        xml.writeAttribute("projectShot", sd.uuid);
        xml.writeAttribute("project",     projectDb);
        // Inherit project-level metadata so shot scene knows its context.
        xml.writeStartElement("project");
        xml.writeAttribute("production", model->production());
        xml.writeAttribute("season",     model->season());
        xml.writeAttribute("episode",    model->episode());
        xml.writeAttribute("title",      model->title());
        xml.writeAttribute("technique",  tech);
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeEndDocument();
        f.close();
      }

      // Export = the storyboard is locked in: mark Storyboard Done and advance
      // the first production task (usually Layout) Todo→Ready; later tasks stay
      // Todo until each predecessor is approved.
      QStringList tts;
      if (const Technique *t = model->findTechnique(tech)) tts = t->taskTypes;
      const QString firstProd = model->firstProductionTaskType(tech);
      for (ProjectShot &ps : model->projectShots_rw()) {
        if (ps.uuid != sd.uuid) continue;
        if (tts.contains("Storyboard"))
          ps.tasks["Storyboard"].status = TaskStatus::Done;
        if (!firstProd.isEmpty()) {
          TaskState &st = ps.tasks[firstProd];
          if (st.status == TaskStatus::Todo) st.status = TaskStatus::Ready;
        }
        break;
      }
    }
  }

  if (writeLink && !projectDb.isEmpty()) {
    model->saveProjectDb();
    emit model->taskStatusChanged();  // refresh the Production Tracker
  }

  return exported;
}

//-----------------------------------------------------------------------------

void StoryboardPanel::onExportShotsToProject() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export Shots to New Project"),
                             tr("No shots to export."));
    return;
  }
  ZtoryModel *model = ZtoryModel::instance();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TProjectManager *pm = TProjectManager::instance();

  // ── Single dialog: destination project + shot options ────────────────────
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Export Shots to New Project"));
  dlg.setMinimumWidth(560);
  auto *lay = new QVBoxLayout(&dlg);
  lay->setSpacing(8);
  lay->setContentsMargins(14, 14, 14, 14);

  // Destination project — new (created on export) or an existing one (useful
  // to gather shots from several storyboards into a single destination).
  auto *projBox = new QGroupBox(tr("Destination project"), &dlg);
  auto *projLay = new QVBoxLayout(projBox);

  auto *destRow      = new QHBoxLayout();
  auto *newProjRadio = new QRadioButton(tr("New project"), projBox);
  auto *existRadio   = new QRadioButton(tr("Existing project"), projBox);
  // Own exclusive group: without it these share projBox's implicit auto-exclusive
  // group with the Target-application radios below, so clicking Tahoma/OpenToonz
  // would deselect New/Existing (and vice versa).
  auto *destGroup = new QButtonGroup(&dlg);
  destGroup->addButton(newProjRadio);
  destGroup->addButton(existRadio);
  newProjRadio->setChecked(true);
  destRow->addWidget(newProjRadio);
  destRow->addWidget(existRadio);
  destRow->addStretch();
  projLay->addLayout(destRow);

  // New-project fields
  auto *newProjWidget = new QWidget(projBox);
  auto *projForm      = new QFormLayout(newProjWidget);
  projForm->setLabelAlignment(Qt::AlignRight);
  projForm->setContentsMargins(0, 0, 0, 0);

  auto *nameEdit = new QLineEdit(newProjWidget);
  projForm->addRow(tr("Project name:"), nameEdit);

  QString defaultLocation =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0];
  QString prefLocation = Preferences::instance()->getDefaultProjectPath();
  if (TSystem::doesExistFileOrLevel(TFilePath(prefLocation)))
    defaultLocation = prefLocation;
  auto *locationFld = new DVGui::FileField(newProjWidget, defaultLocation);
  projForm->addRow(tr("Create in:"), locationFld);

  auto *assetOrgCombo = new QComboBox(newProjWidget);
  assetOrgCombo->addItem(tr("Project folders (drawings, extras...)"));
  assetOrgCombo->addItem(tr("Scene sub-folders inside asset folders"));
  assetOrgCombo->addItem(
      tr("Assets folder next to each scene (scenes/<scene name>)"));
  assetOrgCombo->setToolTip(
      tr("Where the exported scenes keep their assets:\n"
         "- Project folders: all scenes share the project-level folders.\n"
         "- Scene sub-folders: each scene gets its own sub-folder inside "
         "drawings/extras/inputs.\n"
         "- Next to each scene: each scene folder in scenes/ holds all its "
         "assets — handy for moving scenes between computers."));
  projForm->addRow(tr("Asset organization:"), assetOrgCombo);
  projLay->addWidget(newProjWidget);

  // Existing-project picker
  auto *projectTree = new ExportSceneTreeView(projBox);
  projectTree->setFixedHeight(180);
  projectTree->setVisible(false);
  projLay->addWidget(projectTree);

  // Target application (applies to both destinations)
  auto *targetRow    = new QHBoxLayout();
  auto *targetZtoryc = new QRadioButton(tr("Ztoryc"), projBox);
  auto *targetTahoma = new QRadioButton(tr("Tahoma2D"), projBox);
  auto *targetOT     = new QRadioButton(tr("OpenToonz"), projBox);
  // Separate exclusive group (independent of New/Existing above).
  auto *targetGroup = new QButtonGroup(&dlg);
  targetGroup->addButton(targetZtoryc);
  targetGroup->addButton(targetTahoma);
  targetGroup->addButton(targetOT);
  targetZtoryc->setChecked(true);
  targetTahoma->setToolTip(
      tr("Strips the Ztoryc-only In/Out markers and writes a "
         "tahomaproject.xml so stock Tahoma2D recognizes the project."));
  targetOT->setToolTip(
      tr("Converts the exported scenes to explicit holds and writes an "
         "OpenToonz-readable project file."));
  targetRow->addWidget(new QLabel(tr("Target application:"), projBox));
  targetRow->addWidget(targetZtoryc);
  targetRow->addWidget(targetTahoma);
  targetRow->addWidget(targetOT);
  targetRow->addStretch();
  projLay->addLayout(targetRow);
  lay->addWidget(projBox);

  // Asset folders — collapsed by default; when unchecked the exported project
  // simply inherits the current project's folder layout.
  auto *foldersBox = new QGroupBox(tr("Customize asset folders"), &dlg);
  foldersBox->setCheckable(true);
  foldersBox->setChecked(false);
  foldersBox->setToolTip(
      tr("When off, the new project uses the same folder layout as the "
         "current one (drawings, extras, inputs...)."));
  auto *foldersLay   = new QVBoxLayout(foldersBox);
  auto *foldersInner = new QWidget(foldersBox);
  auto *foldersForm  = new QFormLayout(foldersInner);
  foldersForm->setLabelAlignment(Qt::AlignRight);
  foldersForm->setContentsMargins(0, 0, 0, 0);
  QList<QPair<std::string, DVGui::FileField *>> folderFlds;
  {
    auto currentProject = pm->getCurrentProject();
    std::vector<std::string> folderNames;
    pm->getFolderNames(folderNames);
    for (const std::string &name : folderNames) {
      QString qName = QString::fromStdString(name);
      auto *ff      = new DVGui::FileField(foldersInner, qName);
      TFilePath fp  = currentProject->getFolder(name);
      if (fp != TFilePath()) ff->setPath(fp.getQString());
      folderFlds.append(qMakePair(name, ff));
      foldersForm->addRow("+" + qName + ":", ff);
    }
  }
  foldersLay->addWidget(foldersInner);
  foldersInner->setVisible(false);
  QObject::connect(foldersBox, &QGroupBox::toggled,
                   [&dlg, foldersInner](bool on) {
                     foldersInner->setVisible(on);
                     dlg.adjustSize();
                   });
  lay->addWidget(foldersBox);

  // New/Existing destination toggle: swap the project fields for the tree.
  QObject::connect(existRadio, &QRadioButton::toggled,
                   [&dlg, newProjWidget, projectTree, foldersBox](bool on) {
                     newProjWidget->setVisible(!on);
                     projectTree->setVisible(on);
                     foldersBox->setVisible(!on);
                     dlg.adjustSize();
                   });

  // Asset organization presets update the (possibly hidden) folder fields so
  // "Customize asset folders" always shows the effective layout.
  QObject::connect(
      assetOrgCombo, qOverload<int>(&QComboBox::currentIndexChanged),
      [&folderFlds, pm](int idx) {
        for (auto &f : folderFlds) {
          const std::string &name = f.first;
          // Per-scene folders only: palettes and scripts stay project-level
          // (shared color design palettes / automation scripts).
          if (name != "drawings" && name != "extras" && name != "inputs" &&
              name != "outputs" && name != "stopmotion")
            continue;
          if (idx == 2)
            f.second->setPath("scenes/$scenepath/" +
                              QString::fromStdString(name));
          else {
            TFilePath fp = pm->getCurrentProject()->getFolder(name);
            f.second->setPath(fp == TFilePath()
                                  ? QString::fromStdString(name)
                                  : fp.getQString());
          }
        }
      });

  // Shots
  auto *shotsBox  = new QGroupBox(tr("Shots"), &dlg);
  auto *shotsForm = new QFormLayout(shotsBox);
  shotsForm->setLabelAlignment(Qt::AlignRight);

  auto *verSpin = new QSpinBox(shotsBox);
  verSpin->setRange(1, 99);
  verSpin->setValue(1);
  verSpin->setPrefix("v");
  shotsForm->addRow(tr("Version:"), verSpin);

  auto *rangeRow   = new QHBoxLayout();
  auto *allRadio   = new QRadioButton(tr("All shots"), shotsBox);
  auto *selRadio   = new QRadioButton(tr("Selected"), shotsBox);
  auto *rangeRadio = new QRadioButton(tr("Range:"), shotsBox);
  allRadio->setChecked(true);
  auto *fromCombo = new QComboBox(shotsBox);
  auto *toCombo   = new QComboBox(shotsBox);
  auto *toLabel   = new QLabel(tr("to"), shotsBox);
  for (const auto &sh : m_shots) {
    const QString t = shotRangeLabel(sh.data, model);
    fromCombo->addItem(t);
    toCombo->addItem(t);
  }
  toCombo->setCurrentIndex((int)m_shots.size() - 1);
  fromCombo->setEnabled(false); toCombo->setEnabled(false); toLabel->setEnabled(false);
  selRadio->setEnabled(!m_selectedIndices.empty());
  rangeRow->addWidget(allRadio); rangeRow->addWidget(selRadio);
  rangeRow->addWidget(rangeRadio);
  rangeRow->addWidget(fromCombo); rangeRow->addWidget(toLabel);
  rangeRow->addWidget(toCombo); rangeRow->addStretch();
  shotsForm->addRow(tr("Shots:"), rangeRow);
  QObject::connect(rangeRadio, &QRadioButton::toggled, [&](bool on) {
    fromCombo->setEnabled(on); toCombo->setEnabled(on); toLabel->setEnabled(on);
  });

  // Production Tracker link — plain wording: what it writes and what happens.
  auto *trackerChk = new QCheckBox(
      tr("Update Production Tracker (writes a .ztoryc file next to each "
         "exported scene)"),
      shotsBox);
  trackerChk->setToolTip(
      tr("On export, each shot's Storyboard task is marked Done and its first "
         "production task becomes Ready in the Production Tracker.\n"
         "The .ztoryc file links the exported scene back to this project: "
         "opening the scene later advances its task to In Progress."));
  trackerChk->setChecked(!model->projectDbPath().isEmpty());
  trackerChk->setEnabled(!model->projectDbPath().isEmpty());
  if (model->projectDbPath().isEmpty())
    trackerChk->setToolTip(
        tr("Save the scene first to create a project DB."));
  shotsForm->addRow(QString(), trackerChk);

  auto *lipSyncChk = new QCheckBox(tr("Generate lip sync columns"), shotsBox);
  addLipSyncOption(lipSyncChk);
  shotsForm->addRow(QString(), lipSyncChk);

  auto *assetsChk = new QCheckBox(
      tr("Import each shot's assets from the breakdown"), shotsBox);
  addAssetImportOption(assetsChk);
  shotsForm->addRow(QString(), assetsChk);
  lay->addWidget(shotsBox);

  auto *bbox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
  lay->addWidget(bbox);
  // Validate the destination before leaving the dialog, so a bad name or a
  // missing selection does not waste a full staging pass.
  QObject::connect(bbox, &QDialogButtonBox::accepted, [&] {
    if (existRadio->isChecked()) {
      auto *node = dynamic_cast<DvDirModelFileFolderNode *>(
          projectTree->getCurrentNode());
      if (!node || !pm->isProject(node->getPath())) {
        QMessageBox::warning(&dlg, tr("Export Shots to Project"),
                             tr("The folder you selected is not a project."));
        return;
      }
      dlg.accept();
      return;
    }
    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
      QMessageBox::warning(&dlg, tr("Export Shots to New Project"),
                           tr("The project name cannot be empty."));
      return;
    }
    TFilePath projectFolder =
        TFilePath(locationFld->getPath().toStdWString()) +
        TFilePath(name.toStdWString());
    if (TSystem::doesExistFileOrLevel(
            pm->projectFolderToProjectPath(projectFolder))) {
      QMessageBox::warning(&dlg, tr("Export Shots to New Project"),
                           tr("Project '%1' already exists.").arg(name));
      return;
    }
    dlg.accept();
  });
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  if (dlg.exec() != QDialog::Accepted) return;

  QList<int> indices;
  if (selRadio->isChecked()) {
    for (int i : m_selectedIndices) indices.append(i);
  } else {
    int from = allRadio->isChecked() ? 0 : fromCombo->currentIndex();
    int to   = allRadio->isChecked() ? (int)m_shots.size() - 1
                                     : toCombo->currentIndex();
    if (from > to) std::swap(from, to);
    for (int i = from; i <= to; i++) indices.append(i);
  }
  if (indices.isEmpty()) return;

  // Il controllo degli asset PRIMA di creare il progetto di destinazione:
  // fermarsi dopo lascerebbe in giro un progetto vuoto da cancellare a mano.
  if (assetsChk->isChecked()) {
    QStringList uuids;
    for (int i : indices)
      if (i >= 0 && i < (int)m_shots.size() && !m_shots[i].data.uuid.isEmpty())
        uuids << m_shots[i].data.uuid;
    if (!ztoryConfirmShotAssets(this, uuids)) return;
  }

  ExportScenePopup::ExportTarget exportTarget =
      targetOT->isChecked() ? ExportScenePopup::ExportTarget::OpenToonz
      : targetTahoma->isChecked()
          ? ExportScenePopup::ExportTarget::Tahoma
          : ExportScenePopup::ExportTarget::Ztoryc;

  // ── Resolve the destination project (create it if new) ───────────────────
  TFilePath projectPath;
  if (existRadio->isChecked()) {
    auto *node = dynamic_cast<DvDirModelFileFolderNode *>(
        projectTree->getCurrentNode());
    if (!node) return;  // validated on accept — belt and braces
    projectPath = pm->projectFolderToProjectPath(node->getPath());
  } else {
    ExportScenePopup::NewProjectSpec spec;
    spec.name     = nameEdit->text().trimmed();
    spec.location = locationFld->getPath();
    for (const auto &f : folderFlds)
      spec.folders.append(qMakePair(f.first, f.second->getPath()));
    spec.useSubScenePath = (assetOrgCombo->currentIndex() == 1);
    spec.target          = exportTarget;
    projectPath          = ExportScenePopup::createProjectFromSpec(spec);
    if (projectPath == TFilePath()) return;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);

  // ── Stage one .tnz per shot inside the current project ───────────────────
  // The Export Scene machinery only imports scenes that belong to a project,
  // so staging lives under the current project (+scenes). The extra "scenes"
  // level makes the per-scene asset copies done on save (drawings/extras/...
  // subfolders) land inside ztoryc_export_tmp as well: one rmDirTree cleans
  // everything and re-exports never hit "file already exists" prompts.
  TFilePath stagingRoot =
      scene->decodeFilePath(TFilePath("+scenes")) + "ztoryc_export_tmp";
  if (TFileStatus(stagingRoot).doesExist()) TSystem::rmDirTree(stagingRoot);
  TFilePath stagingDir = stagingRoot + "scenes";

  int fail = 0;
  // Source files of project-folder levels (+extras/+drawings) per exported
  // scene: the export machinery won't copy these, so we do it below.
  QHash<QString, QList<QPair<TFilePath, TFilePath>>> assetCopies;
  ShotExportOptions opts;
  opts.writeLink       = trackerChk->isChecked();
  opts.lipSync         = lipSyncChk->isChecked();
  opts.importAssets    = assetsChk->isChecked();
  g_ztoryExportLipSync = opts.lipSync;
  g_ztoryExportAssets  = opts.importAssets;
  QList<TFilePath> staged = exportShotScenesToDir(
      indices, stagingDir, verSpin->value(), opts, fail, &assetCopies);
  if (staged.isEmpty()) {
    QApplication::restoreOverrideCursor();
    QMessageBox::warning(this, tr("Export Shots to New Project"),
                         tr("Could not export the selected shots."));
    TSystem::rmDirTree(stagingRoot);
    return;
  }

  // ── Copy scenes + assets into the new project ─────────────────────────────
  // Export with target Ztoryc here: the target compatibility pass re-loads
  // and re-saves each scene, so it must run only AFTER the '+' project-folder
  // asset copies below — otherwise the animatic audio (still missing at that
  // point) is loaded empty and dropped from the re-saved scene.
  std::vector<TFilePath> stagedScenes(staged.begin(), staged.end());
  std::vector<TFilePath> exported = ExportScenePopup::exportScenesToProject(
      stagedScenes, projectPath, ExportScenePopup::ExportTarget::Ztoryc);

  // Copy the project-folder levels (+extras/+drawings) that the export won't:
  // for each exported scene, resolve the (portable) coded path in the TARGET
  // project's own folder organization and copy the source frames there. This
  // works whatever the destination settings are — including an existing project
  // whose layout differs from the storyboard's source project.
  if (!assetCopies.isEmpty()) {
    TProjectManager *pmgr = TProjectManager::instance();
    TFilePath savedPrj    = pmgr->getCurrentProjectPath();
    pmgr->setCurrentProjectPath(projectPath);
    auto targetProject = pmgr->getCurrentProject();
    for (const TFilePath &newScene : exported) {
      const QString base = QString::fromStdWString(newScene.getWideName());
      auto it            = assetCopies.find(base);
      if (it == assetCopies.end()) continue;
      // Resolve the coded path in the target WITHOUT loading the scene: loading
      // re-reads resources (which aren't copied yet) → "cannot load" popups and
      // a hang. Setting the project + scene path is enough for decodeFilePath.
      ToonzScene ts;
      ts.setProject(targetProject);
      ts.setScenePath(newScene);
      for (const auto &pr : it.value()) {
        const TFilePath dstAbs = ts.decodeFilePath(pr.first);  // coded → target
        if (dstAbs.isEmpty() || dstAbs == pr.second) {
          exportLog(QString("[%1] project-copy %2 : %3")
                        .arg(base, pr.first.getQString(),
                             dstAbs.isEmpty() ? "empty resolve"
                                              : "same as src"));
          continue;
        }
        if (TSystem::doesExistFileOrLevel(dstAbs)) {
          exportLog(QString("[%1] project-copy %2 -> %3 : already exists")
                        .arg(base, pr.first.getQString(),
                             dstAbs.getQString()));
          continue;
        }
        try {
          TSystem::touchParentDir(dstAbs);
          // Image levels can be multi-frame → copyFiles; a single file (e.g. a
          // .wav sound level) may not be handled, so fall back to a plain copy.
          TXshSimpleLevel::copyFiles(dstAbs, pr.second);
          if (!TSystem::doesExistFileOrLevel(dstAbs) &&
              TSystem::doesExistFileOrLevel(pr.second))
            TSystem::copyFile(dstAbs, pr.second);
          exportLog(QString("[%1] project-copy %2 -> %3 : %4")
                        .arg(base, pr.second.getQString(), dstAbs.getQString(),
                             TSystem::doesExistFileOrLevel(dstAbs)
                                 ? "OK"
                                 : "FAILED"));
        } catch (...) {
          exportLog(QString("[%1] project-copy %2 -> %3 : EXCEPTION")
                        .arg(base, pr.second.getQString(),
                             dstAbs.getQString()));
        }
      }
    }
    pmgr->setCurrentProjectPath(savedPrj);
  }

  // Now that every asset (including the '+' project-folder copies above) is in
  // place, run the target compatibility pass (explicit holds / marker strip /
  // project file for stock Tahoma2D or OpenToonz).
  exportLog(QString("[compat] target=%1 on %2 scene(s)")
                .arg(exportTarget == ExportScenePopup::ExportTarget::OpenToonz
                         ? "OpenToonz"
                     : exportTarget == ExportScenePopup::ExportTarget::Tahoma
                         ? "Tahoma2D"
                         : "Ztoryc")
                .arg((int)exported.size()));
  ExportScenePopup::applyTargetCompatibility(exported, projectPath,
                                             exportTarget);
  dumpExportLog(projectPath.getParentDir());

  // Copy the .ztoryc back-link companions next to the exported scenes (the
  // Export Scene flow only copies the scene + its assets). Governed solely by
  // the tracker checkbox, independent of target app: an OpenToonz-targeted
  // export can still be re-opened in Ztoryc later purely to update production
  // status, even if the actual drawing/animation work happens in OT.
  if (trackerChk->isChecked()) {
    for (const TFilePath &newScene : exported) {
      TFilePath stagedZtoryc =
          stagingDir + (newScene.getWideName() + L".ztoryc");
      if (!TSystem::doesExistFileOrLevel(stagedZtoryc)) continue;
      TFilePath dstZtoryc =
          newScene.getParentDir() + (newScene.getWideName() + L".ztoryc");
      try {
        if (TSystem::doesExistFileOrLevel(dstZtoryc))
          TSystem::removeFileOrLevel(dstZtoryc);
        TSystem::copyFile(dstZtoryc, stagedZtoryc);
      } catch (...) {
        // Non-fatal: the scene is exported, only the back-link is missing.
      }
    }
  }

  TSystem::rmDirTree(stagingRoot);
  QApplication::restoreOverrideCursor();

  if (exported.empty()) {
    QMessageBox::warning(this, tr("Export Shots to New Project"),
                         tr("Could not export the selected shots."));
    return;
  }
  QString msg = tr("Exported %1 shot(s) to project: %2")
                    .arg((int)exported.size())
                    .arg(projectPath.getParentDir().getQString());
  if (fail > 0) msg += tr("\n%1 shot(s) failed.").arg(fail);
  QMessageBox::information(this, tr("Export Shots to New Project"), msg);
}

namespace {
// One per-shot video clip referenced by the FCPXML timeline.
// transitionAfter = cross-dissolve length (frames) between this clip and the
// next; 0 = hard cut. The two clips overlap by that many frames in the spine so
// DaVinci inserts a real cross dissolve on import.
struct FcpxClip { QString name; QString file; int frames; int transitionAfter = 0; };
// One audio clip on a lane (each animatic sound column → its own lane, so
// dialogue / music / fx stay mixable in DaVinci).
// offset  = position on the timeline (frames from timeline start)
// frames  = visible duration of the clip (after head/tail trim)
// srcStart= in-point into the source media (frames trimmed off the head)
// srcLen  = full source length (frames) — the asset's available media
struct FcpxAudio {
  QString name; QString file;
  int offset; int frames; int srcStart; int srcLen; int lane;
};

// Write an FCPXML 1.9 timeline: per-shot clips on the spine (cumulative offsets)
// plus each audio clip as a connected clip on its own lane — a DaVinci Resolve /
// Premiere / FCP importable edit of the animatic. Times are <frames>/<fps>s.
bool writeAnimaticFcpxml(const QString &path, const QString &projectName, int fps,
                         int width, int height,
                         const std::vector<FcpxClip> &clips,
                         const std::vector<FcpxAudio> &audio) {
  if (fps <= 0) fps = 25;
  if (clips.empty()) return false;
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
  auto dur = [fps](int frames) { return QString("%1/%2s").arg(frames).arg(fps); };

  // Cross-dissolve overlaps, clamped so an overlap never exceeds either
  // neighbouring clip. trans[i] = frames clip i overlaps clip i+1.
  std::vector<int> trans(clips.size(), 0);
  bool anyTransition = false;
  for (size_t i = 0; i + 1 < clips.size(); i++) {
    int t = qBound(0, clips[i].transitionAfter,
                   qMin(clips[i].frames, clips[i + 1].frames));
    trans[i] = t;
    if (t > 0) anyTransition = true;
  }
  // Compensated (overlapped) timeline start of each clip, and a mapper from an
  // original sequential frame to its overlapped position — used to keep the
  // audio, which is placed in absolute sequential frames, synced to the video
  // once the dissolves have pulled later clips earlier.
  std::vector<int> compStart(clips.size(), 0);
  {
    int acc = 0;
    for (size_t i = 0; i < clips.size(); i++) {
      compStart[i] = acc;
      acc += clips[i].frames - (i + 1 < clips.size() ? trans[i] : 0);
    }
  }
  int total = clips.empty() ? 0 : compStart.back() + clips.back().frames;
  auto compAudioFrame = [&](int origFrame) {
    int shift = 0, seam = 0;
    for (size_t i = 0; i + 1 < clips.size(); i++) {
      seam += clips[i].frames;  // original sequential seam between i and i+1
      if (origFrame >= seam) shift += trans[i];
    }
    return qMax(0, origFrame - shift);
  };

  QXmlStreamWriter x(&f);
  x.setAutoFormatting(true);
  x.writeStartDocument();
  x.writeDTD("<!DOCTYPE fcpxml>");
  x.writeStartElement("fcpxml");
  x.writeAttribute("version", "1.9");

  x.writeStartElement("resources");
  x.writeStartElement("format");
  x.writeAttribute("id", "r1");
  x.writeAttribute("name", "FFVideoFormatRateUndefined");
  x.writeAttribute("frameDuration", QString("1/%1s").arg(fps));
  x.writeAttribute("width", QString::number(width));
  x.writeAttribute("height", QString::number(height));
  x.writeEndElement();  // format
  // Cross-dissolve effect the transition elements reference. DaVinci maps it to
  // its own cross dissolve by name on import.
  if (anyTransition) {
    x.writeStartElement("effect");
    x.writeAttribute("id", "rXd");
    x.writeAttribute("name", "Cross Dissolve");
    x.writeAttribute("uid", ".../Transitions.localized/Dissolves.localized/"
                            "Cross Dissolve.localized/Cross Dissolve.motn");
    x.writeEndElement();  // effect
  }
  int vid = 0;
  for (const auto &c : clips) {
    x.writeStartElement("asset");
    x.writeAttribute("id", QString("v%1").arg(++vid));
    x.writeAttribute("name", c.name);
    x.writeAttribute("start", "0s");
    x.writeAttribute("duration", dur(c.frames));
    x.writeAttribute("hasVideo", "1");
    x.writeAttribute("format", "r1");
    x.writeAttribute("src", QUrl::fromLocalFile(c.file).toString());
    x.writeEndElement();
  }
  int aid = 0;
  for (const auto &a : audio) {
    x.writeStartElement("asset");
    x.writeAttribute("id", QString("a%1").arg(++aid));
    x.writeAttribute("name", a.name);
    x.writeAttribute("start", "0s");
    // The asset describes the whole media file, so its duration is the full
    // source length — the clip below picks a sub-range of it via start/duration.
    x.writeAttribute("duration", dur(a.srcLen > 0 ? a.srcLen : a.frames));
    x.writeAttribute("hasAudio", "1");
    x.writeAttribute("audioSources", "1");
    x.writeAttribute("audioChannels", "2");
    x.writeAttribute("audioRate", "48000");
    x.writeAttribute("src", QUrl::fromLocalFile(a.file).toString());
    x.writeEndElement();
  }
  x.writeEndElement();  // resources

  x.writeStartElement("library");
  x.writeStartElement("event");
  x.writeAttribute("name", "Ztoryc");
  x.writeStartElement("project");
  x.writeAttribute("name", projectName + " animatic");
  x.writeStartElement("sequence");
  x.writeAttribute("format", "r1");
  x.writeAttribute("duration", dur(total));
  x.writeAttribute("tcStart", "0s");
  x.writeAttribute("tcFormat", "NDF");
  x.writeStartElement("spine");
  vid = 0;
  for (size_t i = 0; i < clips.size(); i++) {
    const auto &c = clips[i];
    x.writeStartElement("asset-clip");
    x.writeAttribute("ref", QString("v%1").arg(++vid));
    x.writeAttribute("offset", dur(compStart[i]));
    x.writeAttribute("name", c.name);
    x.writeAttribute("start", "0s");
    x.writeAttribute("duration", dur(c.frames));
    // Connected audio clips (lanes -1, -2, …) nested in the first video clip,
    // offsets relative to the timeline start. Offsets are compensated for the
    // dissolve overlaps so the audio stays synced to the pulled-earlier video.
    if (i == 0) {
      aid = 0;
      for (const auto &a : audio) {
        x.writeStartElement("asset-clip");
        x.writeAttribute("ref", QString("a%1").arg(++aid));
        x.writeAttribute("lane", QString::number(a.lane));
        x.writeAttribute("offset", dur(compAudioFrame(a.offset)));
        x.writeAttribute("name", a.name);
        // start = in-point into the source, so a trimmed/repositioned audio clip
        // plays from where the user set it in the animatic (not from 0s).
        x.writeAttribute("start", dur(a.srcStart));
        x.writeAttribute("duration", dur(a.frames));
        x.writeEndElement();
      }
    }
    x.writeEndElement();  // asset-clip (video)

    // Cross dissolve to the next clip: sits in the overlap the two clips share.
    if (i + 1 < clips.size() && trans[i] > 0) {
      x.writeStartElement("transition");
      x.writeAttribute("name", "Cross Dissolve");
      x.writeAttribute("offset", dur(compStart[i] + c.frames - trans[i]));
      x.writeAttribute("duration", dur(trans[i]));
      x.writeStartElement("filter-video");
      x.writeAttribute("ref", "rXd");
      x.writeAttribute("name", "Cross Dissolve");
      x.writeEndElement();  // filter-video
      x.writeEndElement();  // transition
    }
  }
  x.writeEndElement();  // spine
  x.writeEndElement();  // sequence
  x.writeEndElement();  // project
  x.writeEndElement();  // event
  x.writeEndElement();  // library
  x.writeEndElement();  // fcpxml
  x.writeEndDocument();
  return true;
}
}  // namespace

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

  // The shot range is meaningful for "Shot range" AND "One clip per shot" (export
  // only the clips of a sub-range), so enable it for both.
  connect(radioRange, &QRadioButton::toggled, rangeWidget, &QWidget::setEnabled);
  connect(radioEach, &QRadioButton::toggled, rangeWidget, [rangeWidget, radioRange](bool on) {
    rangeWidget->setEnabled(on || radioRange->isChecked());
  });

  // DaVinci / NLE handoff: also write an FCPXML edit referencing the per-shot clips.
  auto *fcpxmlCheck = new QCheckBox(
      tr("Also export editing timeline (.fcpxml)"), &dlg);
  fcpxmlCheck->setToolTip(
      tr("Write an FCPXML edit that places the per-shot clips on a timeline,\n"
         "importable into DaVinci Resolve, Premiere Pro or Final Cut Pro.\n"
         "Requires 'One clip per shot' (the timeline references those clips)."));
  // Only meaningful with per-shot clips: enable it only when that mode is active.
  fcpxmlCheck->setEnabled(false);
  connect(radioEach, &QRadioButton::toggled, fcpxmlCheck, [fcpxmlCheck](bool on) {
    fcpxmlCheck->setEnabled(on);
    if (!on) fcpxmlCheck->setChecked(false);
  });
  mainLay->addWidget(fcpxmlCheck);

  // Upload the per-shot clips to Kitsu right after export — matched to each
  // shot's task by name + {TASK} code, set to WFA. Needs per-shot clips and a
  // Kitsu-linked project.
  const bool kitsuLinked = ZtoryModel::instance()->isKitsuLinked();
  auto *kitsuUploadCheck =
      new QCheckBox(tr("Upload clips to Kitsu after export"), &dlg);
  // Opt-in: only show the Kitsu upload option when the project uses Kitsu.
  kitsuUploadCheck->setVisible(ZtoryModel::instance()->useKitsu());
  kitsuUploadCheck->setToolTip(
      tr("After exporting one clip per shot, upload each clip to its shot's task\n"
         "on Kitsu (matched by shot name + {TASK} code) and set it to WFA.\n"
         "Requires 'One clip per shot' and a project linked to Kitsu."));
  kitsuUploadCheck->setEnabled(false);
  connect(radioEach, &QRadioButton::toggled, kitsuUploadCheck,
          [kitsuUploadCheck, kitsuLinked](bool on) {
            kitsuUploadCheck->setEnabled(on && kitsuLinked);
            if (!on) kitsuUploadCheck->setChecked(false);
          });
  mainLay->addWidget(kitsuUploadCheck);

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
  // The native Output Settings popup (opened by "Render Settings…") is
  // non-modal: the user can change format/fps/resolution while this dialog is
  // open. There is no per-property changed signal to connect to, so poll the
  // live TOutputProperties and keep the summary in sync in real time.
  {
    auto *fmtTimer = new QTimer(&dlg);
    QObject::connect(fmtTimer, &QTimer::timeout, &dlg,
                     [refreshFormatNote]() { refreshFormatNote(); });
    fmtTimer->start(400);
  }
  {
    auto *fmtRow = new QHBoxLayout;
    fmtRow->addWidget(fmtNote, 1);
    auto *rsBtn = new QPushButton(tr("Render Settings…"), &dlg);
    rsBtn->setToolTip(tr("Set the output format, codec, fps and resolution "
                         "before exporting"));
    // A dedicated, settings-only Output Settings popup: this export dialog is
    // what actually launches the render, so hide the popup's Render / Save and
    // Render buttons to avoid two confusing ways to start one. Kept separate from
    // the menu's shared singleton (MI_OutputSettings) so hiding the buttons never
    // leaks into the normal Output Settings window.
    //
    // Parented to THIS dialog with the Window flag: it's still a separate window
    // (a QFrame needs Qt::Window to float when it has a parent), but owned by the
    // export dialog — so CLOSING it doesn't tear down the export dialog (a
    // parentless popup closing inside the dialog's local event loop was ending
    // that loop), and it dies together with the dialog (no leak, no static).
    // Capture only &dlg (a function-scope local that stays alive for the whole
    // loop.exec() below). The popup is looked up as a child of dlg rather than
    // held in a captured local — a captured QPointer would live only inside this
    // inner block and dangle by the time the click fires during loop.exec().
    connect(rsBtn, &QPushButton::clicked, &dlg, [&dlg]() {
      OutputSettingsPopup *rsPopup = dlg.findChild<OutputSettingsPopup *>();
      if (!rsPopup) {
        rsPopup = new OutputSettingsPopup(&dlg);
        rsPopup->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint);
        rsPopup->setWindowTitle(tr("Render Settings"));
        rsPopup->setRenderButtonsVisible(false);
      }
      rsPopup->show();
      rsPopup->raise();
      rsPopup->activateWindow();
    });
    fmtRow->addWidget(rsBtn);
    mainLay->addLayout(fmtRow);
  }

  // Output folder
  auto *folderRow = new QHBoxLayout;
  auto *folderEdit = new QLineEdit(defaultDir, &dlg);
  auto *folderBtn  = new QToolButton(&dlg);
  folderBtn->setText("…");
  // Connected further down, after writeBackDest is defined: picking a folder
  // must also write it into the Render Settings, otherwise the sync poll below
  // reverts it back to the (unchanged) TOutputProperties value.
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

  // ── Bidirectional sync of the export destination with Render Settings ──────
  // Output folder / Filename mirror the native Output Settings "Save in" /
  // "Name" (the non-modal popup can change them live), and edits here write
  // back into the scene's TOutputProperties.  The "+outputs"-style aliases are
  // resolved to/from absolute paths via scene->decode/codeFilePath.
  auto folderFromProp = [scene, prop]() {
    return QString::fromStdWString(
        scene->decodeFilePath(prop->getPath().getParentDir()).getWideString());
  };
  auto nameFromProp = [prop]() {
    return QString::fromStdString(prop->getPath().getName());
  };
  // Align on open (overrides the scene-derived defaults set above).
  { QString f = folderFromProp(); if (!f.isEmpty()) folderEdit->setText(f); }
  { QString n = nameFromProp();   if (!n.isEmpty()) nameEdit->setText(n); }
  // Render Settings -> Export (poll; never overwrite a field being edited).
  {
    auto *destTimer = new QTimer(&dlg);
    QObject::connect(
        destTimer, &QTimer::timeout, &dlg,
        [folderEdit, nameEdit, nameNote, prop, folderFromProp, nameFromProp]() {
          if (!folderEdit->hasFocus()) {
            QString f = folderFromProp();
            if (!f.isEmpty() && f != folderEdit->text()) folderEdit->setText(f);
          }
          if (!nameEdit->hasFocus()) {
            QString n = nameFromProp();
            if (!n.isEmpty() && n != nameEdit->text()) nameEdit->setText(n);
          }
          // Keep the extension hint in sync with the current output format.
          std::string e = prop->getPath().getType();
          if (e.empty()) e = "mp4";
          QString extNote = QString("(.%1)").arg(QString::fromStdString(e));
          if (nameNote->text() != extNote) nameNote->setText(extNote);
        });
    destTimer->start(400);
  }
  // Export -> Render Settings (write back on edit, preserving the format ext).
  auto writeBackDest = [scene, prop, folderEdit, nameEdit]() {
    QString nm = nameEdit->text().trimmed();
    if (nm.isEmpty()) return;
    std::string e = prop->getPath().getType();
    if (e.empty()) e = "mp4";
    TFilePath coded = scene->codeFilePath(
        TFilePath(folderEdit->text().toStdWString()));
    TFilePath p = (coded + TFilePath(nm.toStdWString())).withType(e);
    if (p == prop->getPath()) return;
    prop->setPath(p);
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    // Refresh the open native Render Settings popup, which rebuilds its fields
    // from TOutputProperties on sceneChanged() (OutputSettingsPopup::updateField).
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  };
  QObject::connect(folderEdit, &QLineEdit::editingFinished, &dlg, writeBackDest);
  QObject::connect(nameEdit, &QLineEdit::editingFinished, &dlg, writeBackDest);
  // Folder picker: write the chosen folder through to the Render Settings (so
  // the sync poll keeps it) and re-raise this dialog — on macOS the native file
  // dialog can leave the export dialog behind the main window on close.
  QObject::connect(folderBtn, &QToolButton::clicked, &dlg,
                   [&dlg, folderEdit, writeBackDest]() {
                     QFileDialog fd(&dlg, tr("Output folder"), folderEdit->text());
                     fd.setFileMode(QFileDialog::Directory);
                     fd.setOption(QFileDialog::ShowDirsOnly, true);
                     // Match the Render Settings folder picker label ("Choose").
                     fd.setLabelText(QFileDialog::Accept, tr("Choose"));
                     QString d;
                     if (fd.exec() == QDialog::Accepted &&
                         !fd.selectedFiles().isEmpty())
                       d = fd.selectedFiles().first();
                     dlg.raise();
                     dlg.activateWindow();
                     if (d.isEmpty()) return;
                     folderEdit->setText(d);
                     writeBackDest();
                   });

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

  // Helper: compute frame range [start, end] for shot si in main xsheet.
  // resequenceXsheet() packs shots contiguously and writes a STOP_FRAME (SFH)
  // cell at row startFrame+duration to stop the implicit hold bleeding into the
  // next shot.  That cell is NOT empty, so it must be excluded explicitly —
  // otherwise every shot's range is one frame too long (the boundary row, which
  // is also the next shot's first cell), making the per-shot montage longer than
  // the full animatic and bleeding the next shot's first frame into the clip.
  auto isRealCell = [](const TXshCell &c) {
    return !c.isEmpty() && !c.getFrameId().isStopFrame();
  };
  auto shotFrameRange = [&](int si) -> std::pair<int,int> {
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    int col = m_shots[si].data.xsheetColumn;
    int r0 = 0, r1 = 0;
    for (int r = 0; r < xsh->getFrameCount(); r++) {
      if (isRealCell(xsh->getCell(r, col))) { r0 = r; break; }
    }
    for (int r = xsh->getFrameCount() - 1; r >= 0; r--) {
      if (isRealCell(xsh->getCell(r, col))) { r1 = r; break; }
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
    // One clip per shot — render each shot SEQUENTIALLY, waiting for one render
    // to finish before starting the next.  Renders run on background threads
    // behind a non-modal progress dialog (modal breaks blocking-queued
    // connections on macOS), so firing them back-to-back let them overlap and
    // contaminate each other's output — per-shot clips ended up containing
    // every shot.  We wait on ZtoryModel::renderFinished (emitted from
    // OnRenderCompleted in rendercommand.cpp) so only one render is ever live.
    // Honour the shot range when it's enabled (export only a sub-range of clips).
    int pFrom = 0, pTo = (int)m_shots.size() - 1;
    if (rangeWidget->isEnabled()) {
      pFrom = fromCombo->currentIndex();
      pTo   = toCombo->currentIndex();
      if (pFrom > pTo) std::swap(pFrom, pTo);
    }
    for (int si = pFrom; si <= pTo && si < (int)m_shots.size(); si++) {
      auto [r0, r1] = shotFrameRange(si);
      // Use label() (= the shot name pushed to Kitsu), not the legacy
      // shotNumber, so the clip file name matches the Kitsu shot for preview
      // upload matching.  label() falls back to shotNumber when no label is set.
      QString shotNum = m_shots[si].data.label();
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

      // Block until this shot's render completes.  A timeout guards against the
      // pathological case where a render never signals completion (e.g. it
      // failed to start) so the export can never wedge the UI permanently.
      QEventLoop renderLoop;
      QMetaObject::Connection conn =
          connect(ZtoryModel::instance(), &ZtoryModel::renderFinished,
                  &renderLoop, &QEventLoop::quit);
      QTimer renderTimeout;
      renderTimeout.setSingleShot(true);
      connect(&renderTimeout, &QTimer::timeout, &renderLoop, &QEventLoop::quit);
      CommandManager::instance()->execute(MI_Render);
      renderTimeout.start(30 * 60 * 1000);  // 30-min safety cap per shot
      renderLoop.exec();
      disconnect(conn);
    }
  }

  // DaVinci/NLE handoff: write an FCPXML referencing the per-shot clips (the
  // same naming as the per-shot render), in shot order with cumulative offsets.
  if (fcpxmlCheck->isChecked()) {
    int rFrom = 0, rTo = (int)m_shots.size() - 1;
    if (mode == 1 || (mode == 2 && rangeWidget->isEnabled())) {
      rFrom = fromCombo->currentIndex();
      rTo   = toCombo->currentIndex();
      if (rFrom > rTo) std::swap(rFrom, rTo);
    }
    // Transition length (frames) between shot si and the next, read from the
    // "XD-out" note column inside the shot's sub-scene — the persisted source of
    // truth (m_shots[].data.transitionFrames is only a mirror and can be stale).
    auto shotTransitionFrames = [&](int si) -> int {
      TXsheet *xsh = scene->getChildStack()->getTopXsheet();
      int col      = m_shots[si].data.xsheetColumn;
      TXshChildLevel *cl = nullptr;
      for (int r = 0; r < xsh->getFrameCount(); r++) {
        TXshCell cell = xsh->getCell(r, col);
        if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
          cl = cell.m_level->getChildLevel();
          break;
        }
      }
      TXsheet *sub = cl ? cl->getXsheet() : nullptr;
      if (!sub) return 0;
      for (int c = 0; c < sub->getColumnCount(); c++) {
        TXshColumn *scol = sub->getColumn(c);
        if (!scol || !scol->getSoundTextColumn()) continue;
        std::string nm =
            sub->getStageObject(sub->getColumnObjectId(c))->getName();
        if (nm == "XD-out") {
          int a = 0, b = 0;
          scol->getRange(a, b);
          return (b >= a) ? (b - a + 1) * 2 : 0;  // rows = T/2
        }
      }
      return 0;
    };

    std::vector<FcpxClip> clips;
    for (int si = rFrom; si <= rTo && si < (int)m_shots.size(); si++) {
      auto [r0, r1] = shotFrameRange(si);
      // Use label() (= the shot name pushed to Kitsu), not the legacy
      // shotNumber, so the clip file name matches the Kitsu shot for preview
      // upload matching.  label() falls back to shotNumber when no label is set.
      QString shotNum = m_shots[si].data.label();
      QString seqPart;
      if (!m_shots[si].data.sequenceId.isEmpty()) {
        const SequenceData *seq =
            ZtoryModel::instance()->findSequence(m_shots[si].data.sequenceId);
        seqPart = (seq ? seq->label : m_shots[si].data.sequenceId) + "_";
      }
      FcpxClip c;
      c.name   = seqPart + shotNum;
      c.file   = QDir(outDir).filePath(sceneName + "_" + seqPart + shotNum + "." + ext);
      c.frames = r1 - r0 + 1;
      // Cross-dissolve to the next shot (0 = hard cut). Only meaningful when the
      // next shot is also part of this export range; the writer clamps it to the
      // neighbouring clip lengths.
      if (si < rTo && si + 1 < (int)m_shots.size())
        c.transitionAfter = shotTransitionFrames(si);
      if (c.frames > 0) clips.push_back(c);
    }
    // Gather audio: each main-xsheet sound column → its own lane, each of its
    // column levels → an audio clip, so dialogue / music / fx stay separate and
    // mixable in DaVinci.
    std::vector<FcpxAudio> audioClips;
    {
      TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
      const int baseFrame = shotFrameRange(rFrom).first;  // timeline origin
      int lane = 0;
      for (int mc = 0; mc < mainXsh->getColumnCount(); mc++) {
        TXshColumn *col = mainXsh->getColumn(mc);
        TXshSoundColumn *sc = col ? col->getSoundColumn() : nullptr;
        if (!sc) continue;
        --lane;  // -1, -2, -3 …
        for (int li = 0; li < sc->getColumnLevelCount(); li++) {
          ColumnLevel *cl = sc->getColumnLevel(li);
          if (!cl || !cl->getSoundLevel()) continue;
          // Honour head/tail trim: visible start/duration are the on-timeline
          // edit; the source in-point is the number of frames trimmed off the
          // head (getStartOffset), so the clip plays from where it was placed.
          int visStart = cl->getVisibleStartFrame();  // timeline frame
          int dur      = cl->getVisibleFrameCount();
          int srcStart = cl->getStartOffset();        // in-point into source
          if (dur <= 0) continue;
          QString file = QString::fromStdWString(
              scene->decodeFilePath(cl->getSoundLevel()->getPath()).getWideString());
          if (file.isEmpty()) continue;
          int tlOffset = visStart - baseFrame;
          if (tlOffset < 0) {
            // Clip starts before the exported range — trim its head further so
            // it lands at the timeline origin with the right source in-point.
            srcStart += -tlOffset;
            dur      -= -tlOffset;
            tlOffset = 0;
            if (dur <= 0) continue;
          }
          FcpxAudio a;
          a.name     = QFileInfo(file).completeBaseName();
          a.file     = file;
          a.offset   = tlOffset;
          a.frames   = dur;
          a.srcStart = srcStart;
          a.srcLen   = cl->getFrameCount();  // full source length
          a.lane     = lane;
          audioClips.push_back(a);
        }
      }
    }
    int fps = (int)prop->getFrameRate();
    int w = scene->getCurrentCamera()->getRes().lx;
    int h = scene->getCurrentCamera()->getRes().ly;
    QString fcpxmlPath = QDir(outDir).filePath(nameEdit->text() + ".fcpxml");
    if (writeAnimaticFcpxml(fcpxmlPath, sceneName, fps, w, h, clips, audioClips))
      QMessageBox::information(
          this, tr("Export editing timeline"),
          tr("Timeline written:\n%1\n\nImport it into your NLE (DaVinci Resolve, "
             "Premiere Pro or Final Cut Pro). The per-shot clips are referenced "
             "by name.")
              .arg(fcpxmlPath));
  }

  // Upload the freshly rendered per-shot clips to Kitsu (same matching as the
  // Connect dialog: shot label + {TASK} code → task, set WFA). The per-shot
  // render loop above is serialized, so the clips already exist on disk here.
  if (mode == 2 && kitsuUploadCheck->isChecked()) {
    int unmatched = 0, noId = 0;
    QVector<KitsuPreviewUpload> uploads =
        KitsuClient::buildUploadsFromFolder(outDir, unmatched, noId);
    if (uploads.isEmpty()) {
      QMessageBox::warning(
          this, tr("Upload to Kitsu"),
          noId ? tr("No clips uploaded — matched shots aren't on Kitsu yet "
                    "(push shots first).")
               : tr("No clips matched a shot name."));
    } else {
      // Optimistic local WFA mirror (the upload sets WFA on Kitsu too; a later
      // pull reconciles if an upload fails).
      auto &pshots = ZtoryModel::instance()->projectShots_rw();
      bool dirty = false;
      for (const KitsuPreviewUpload &u : uploads)
        for (ProjectShot &ps : pshots)
          if (ps.uuid == u.uuid) {
            if (ps.tasks[u.taskType].status != TaskStatus::Wfa) {
              ps.tasks[u.taskType].status = TaskStatus::Wfa;
              dirty = true;
            }
            break;
          }
      if (dirty) ZtoryModel::instance()->saveAndNotifyTasks();
      KitsuClient::instance()->uploadPreviews(
          ZtoryModel::instance()->kitsuProjectId(), uploads);
      QMessageBox::information(
          this, tr("Upload to Kitsu"),
          tr("Uploading %1 clip(s) to Kitsu…%2")
              .arg(uploads.size())
              .arg(noId ? tr("  (%1 shot(s) not on Kitsu yet)").arg(noId)
                        : QString()));
    }
  }

  // Clear the burn-in config so later renders (menu Render, preview…) are
  // unaffected. Safe even with renders still running: rendercommand.cpp
  // copied it into the MovieRenderer at setup time.
  ZtoryModel::instance()->burnIn() = ZtoryBurnInConfig();

  // Restore original output properties.  Use notifySceneChanged(false): a render
  // (and restoring prop to its pre-export value) must NOT mark the scene dirty —
  // otherwise the unsaved "*" reappears after every export and a following Save
  // can't clear it.  We still emit sceneChanged() to refresh the open Render
  // Settings popup.
  prop->setPath(origPath);
  prop->setRange(origR0, origR1, origStep);
  TApp::instance()->getCurrentScene()->notifySceneChanged(false);
}

// Resolve a (possibly scene-relative) PDF logo path to an absolute file path.
// Empty input → empty output (caller uses the bundled default).
static QString resolvePdfLogoFile(const QString &logoPath, ToonzScene *scene) {
  if (logoPath.isEmpty()) return QString();
  QFileInfo fi(logoPath);
  if (fi.isRelative() && scene) {
    QString dir = QString::fromStdWString(
        scene->getScenePath().getParentDir().getWideString());
    if (!dir.isEmpty()) return QDir(dir).filePath(logoPath);
  }
  return logoPath;
}

void StoryboardPanel::onExportPdf() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export PDF"), tr("No shots to export."));
    return;
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  // ── Pre-export options: project metadata + header logo ─────────────────────
  // Lets the user edit Production/Title (used in the page header) and choose a
  // custom logo (PNG/SVG), keep the default Ztoryc logo, or export with none.
  // Choices persist per-project in the .ztoryc.
  {
    ZtoryModel *model = ZtoryModel::instance();
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Export Storyboard PDF"));
    QVBoxLayout *lay = new QVBoxLayout(&dlg);

    QFormLayout *form    = new QFormLayout();
    QLineEdit *prodEdit  = new QLineEdit(model->production(), &dlg);
    QLineEdit *titleEdit = new QLineEdit(model->title(), &dlg);
    QLineEdit *epEdit    = new QLineEdit(model->episode(), &dlg);
    form->addRow(tr("Production:"), prodEdit);
    form->addRow(tr("Title:"),      titleEdit);
    form->addRow(tr("Episode:"),    epEdit);
    lay->addLayout(form);

    QGroupBox *logoBox    = new QGroupBox(tr("Header logo"), &dlg);
    QVBoxLayout *logoLay  = new QVBoxLayout(logoBox);
    QHBoxLayout *pathRow  = new QHBoxLayout();
    QLineEdit *logoEdit   = new QLineEdit(model->pdfLogoPath(), logoBox);
    logoEdit->setPlaceholderText(tr("(default Ztoryc logo)"));
    QPushButton *browseBtn = new QPushButton(tr("Browse…"), logoBox);
    QPushButton *clearBtn  = new QPushButton(tr("Clear"),    logoBox);
    pathRow->addWidget(logoEdit, 1);
    pathRow->addWidget(browseBtn);
    pathRow->addWidget(clearBtn);
    logoLay->addLayout(pathRow);
    QCheckBox *noLogoChk = new QCheckBox(tr("No logo (clean export)"), logoBox);
    noLogoChk->setChecked(model->pdfNoLogo());
    logoLay->addWidget(noLogoChk);
    QLabel *preview = new QLabel(logoBox);
    preview->setMinimumHeight(46);
    preview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logoLay->addWidget(preview);
    lay->addWidget(logoBox);

    auto updatePreview = [&]() {
      bool none = noLogoChk->isChecked();
      logoEdit->setEnabled(!none);
      browseBtn->setEnabled(!none);
      clearBtn->setEnabled(!none);
      if (none) { preview->setPixmap(QPixmap()); preview->setText(tr("(no logo)")); return; }
      QString custom = logoEdit->text().trimmed();
      if (custom.isEmpty()) {
        preview->setPixmap(QPixmap(":Resources/ztoryc_about.png")
                               .scaledToHeight(40, Qt::SmoothTransformation));
        return;
      }
      QPixmap pm(resolvePdfLogoFile(custom, scene));
      if (pm.isNull())
        preview->setText(tr("⚠ image not found — default logo will be used"));
      else
        preview->setPixmap(pm.scaledToHeight(40, Qt::SmoothTransformation));
    };
    connect(browseBtn, &QPushButton::clicked, &dlg, [&]() {
      QString start = logoEdit->text().trimmed();
      if (start.isEmpty() && scene)
        start = QString::fromStdWString(
            scene->getScenePath().getParentDir().getWideString());
      QString f = QFileDialog::getOpenFileName(
          &dlg, tr("Choose Logo Image"), start,
          tr("Images (*.png *.svg *.jpg *.jpeg *.bmp)"));
      if (!f.isEmpty()) { logoEdit->setText(f); updatePreview(); }
    });
    connect(clearBtn, &QPushButton::clicked, &dlg, [&]() {
      logoEdit->clear(); updatePreview();
    });
    connect(noLogoChk, &QCheckBox::toggled, &dlg, [&](bool){ updatePreview(); });
    connect(logoEdit, &QLineEdit::textChanged, &dlg, [&](const QString&){ updatePreview(); });

    QDialogButtonBox *bb = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);

    updatePreview();
    if (dlg.exec() != QDialog::Accepted) return;

    model->setProduction(prodEdit->text());
    model->setTitle(titleEdit->text());
    model->setEpisode(epEdit->text());
    model->setPdfNoLogo(noLogoChk->isChecked());
    model->setPdfLogoPath(logoEdit->text().trimmed());
    saveZtoryc();  // persist metadata + logo choice in the .ztoryc
  }

  // Build a sensible default filename from the scene name.
  QString defaultPath;
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
  const int imgH     = qMax(1, (int)qRound(cellW / ZtoryShotOps::cameraAspect(scene)));
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

  // Assets — header/footer logo: custom (per-project), default Ztoryc, or none.
  QPixmap logoSrc;
  if (!ZtoryModel::instance()->pdfNoLogo()) {
    QString custom = ZtoryModel::instance()->pdfLogoPath();
    if (!custom.isEmpty()) {
      QPixmap pm(resolvePdfLogoFile(custom, scene));
      // Broken custom path → fall back to the bundled logo (never break export).
      logoSrc = pm.isNull() ? QPixmap(":Resources/ztoryc_about.png") : pm;
    } else {
      logoSrc = QPixmap(":Resources/ztoryc_about.png");
    }
  }
  QPixmap logoPixmap = logoSrc.isNull() ? QPixmap()
      : logoSrc.scaled(mm2px(10), mm2px(10), Qt::KeepAspectRatio, Qt::SmoothTransformation);
  // Footer logo is ALWAYS the Ztoryc brand mark — independent of the header
  // logo choice (custom/none) — so every export keeps the "Made with Ztoryc"
  // attribution and repo link.
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

    // Production + Title (+ Episode when set) fields, splitting remaining width.
    QString epName = ZtoryModel::instance()->episode();
    int numSecs    = epName.isEmpty() ? 2 : 3;
    int fieldAreaW = pageW - cx - mm2px(38);
    int secW = fieldAreaW / numSecs;

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
    drawHdrField(cx,            secW, tr("Production:"), prodName);
    drawHdrField(cx + secW,     secW, tr("Title:"),      titleName);
    if (!epName.isEmpty())
      drawHdrField(cx + 2*secW, secW, tr("Episode:"),    epName);

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
    const QString repoText = "github.com/matitanimata/ztoryc";
    int tW    = fm.horizontalAdvance(ftText);
    int sepW  = fm.horizontalAdvance("  ·  ");
    int repoW = fm.horizontalAdvance(repoText);
    int g2 = mm2px(1.5);
    int totalW = (ftLogo.isNull() ? 0 : ftLogo.width() + g2) + tW + sepW + repoW;
    int fx = (pageW - totalW) / 2;
    if (!ftLogo.isNull()) {
      painter.drawPixmap(fx, fy + (footerH - ftLogo.height())/2, ftLogo);
      fx += ftLogo.width() + g2;
    }
    painter.setPen(QColor(160,160,160));
    painter.drawText(fx, fy, tW, footerH, Qt::AlignVCenter | Qt::AlignLeft, ftText);
    fx += tW;
    painter.drawText(fx, fy, sepW, footerH, Qt::AlignVCenter | Qt::AlignHCenter, "  ·  ");
    fx += sepW;
    // Repo URL shown in link colour. QPdfWriter+QPainter has no simple API for
    // clickable annotations, so this is readable text, not an active hyperlink.
    painter.setPen(QColor(120,120,200));
    QRect repoRect(fx, fy, repoW + mm2px(1), footerH);
    painter.drawText(repoRect, Qt::AlignVCenter | Qt::AlignLeft, repoText);
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
            // Raster sized on the sub-xsheet camera: a main/sub mismatch would
            // bake letterbox bands into the PDF cell (see updatePreview).
            int rH = qMax(1, qRound((cellW - 2) /
                                    ZtoryShotOps::xsheetCameraAspect(subXsh)));
            hq = IconGenerator::renderXsheetFrame(subXsh, frame,
                     TDimension(cellW - 2, rH));
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

//=============================================================================
// Export Production Spreadsheet (.xlsx) — Kitsu-aligned shot tracking.
// One row per shot (thumbnail of the first panel, sequence, shot, timing,
// workflow, notes) plus a status+assignee pair per task type.  Task columns
// are the union of the task sets of every technique used; tasks that don't
// apply to a shot's technique render as a greyed N/A cell.  Status cells get a
// coloured fill and a TODO/READY/WIP/WFA/RETAKE/DONE dropdown.
//-----------------------------------------------------------------------------

void StoryboardPanel::onExportSpreadsheet() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export Spreadsheet"),
                            tr("No shots to export."));
    return;
  }
  ZtoryModel *model = ZtoryModel::instance();
  pushTrackingToBoard();  // reflect tracker edits (model) into the exported copy
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *mainXsh  = scene ? scene->getChildStack()->getTopXsheet() : nullptr;

  // Suggested filename: production[_episode]_spreadsheet.xlsx
  QString base = model->production().trimmed();
  QString ep   = model->episode().trimmed();
  if (!ep.isEmpty()) base = (base.isEmpty() ? ep : base + "_" + ep);
  if (base.isEmpty()) base = "spreadsheet"; else base += "_spreadsheet";
  base.replace(' ', '_').replace('/', '_');
  QString startDir = scene ? QString::fromStdWString(
      scene->getScenePath().getParentDir().getWideString()) : QString();
  QString suggested = startDir.isEmpty() ? base + ".xlsx"
                                         : startDir + "/" + base + ".xlsx";
  QString path = QFileDialog::getSaveFileName(
      this, tr("Export Production Spreadsheet"), suggested,
      tr("Excel Spreadsheet (*.xlsx)"));
  if (path.isEmpty()) return;
  if (!path.endsWith(".xlsx", Qt::CaseInsensitive)) path += ".xlsx";

  using namespace QXlsx;

  // ── Resolve per-shot technique + task columns (task data lives in the Board
  //    copy m_shots[].data, so resolve locally rather than via ZtoryModel). ───
  auto techOf = [&](int si) -> QString {
    const QString &t = m_shots[si].data.technique;
    return t.isEmpty() ? model->defaultTechnique() : t;
  };
  auto taskTypesOf = [&](int si) -> QStringList {
    const Technique *t = model->findTechnique(techOf(si));
    return t ? t->taskTypes : QStringList();
  };
  std::set<QString> usedSet;
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (const QString &tt : taskTypesOf(si)) usedSet.insert(tt);
  QStringList taskCols;
  for (const QString &tt : ZtoryModel::canonicalTaskOrder())
    if (usedSet.count(tt)) { taskCols << tt; usedSet.erase(tt); }
  for (const QString &tt : usedSet) taskCols << tt;  // custom types last

  auto statusColor = [](TaskStatus s) -> QColor {
    switch (s) {
    case TaskStatus::Ready:  return QColor("#FBC02D");  // amber  (Kitsu)
    case TaskStatus::Wip:    return QColor("#3273DC");  // blue   (Kitsu)
    case TaskStatus::Wfa:    return QColor("#AB26FF");  // purple (Kitsu)
    case TaskStatus::Retake: return QColor("#FF3860");  // red    (Kitsu)
    case TaskStatus::Done:   return QColor("#22D160");  // green  (Kitsu)
    case TaskStatus::Todo:
    default:                 return QColor("#9E9E9E");  // grey
    }
  };

  Document xlsx;

  const QStringList fixedCols = {
      tr("Thumbnail"), tr("Sequence"), tr("Shot"),  tr("Frames"),
      tr("Sec/Fr"),    tr("Workflow"), tr("Notes"), tr("VFX Notes")};
  const int firstTaskCol = fixedCols.size() + 1;  // 1-based
  const int headerRow    = 4;
  const int firstDataRow = 5;
  const int fps          = model->fps() > 0 ? model->fps() : 24;

  Format titleFmt; titleFmt.setFontBold(true); titleFmt.setFontSize(14);
  Format subFmt;   subFmt.setFontBold(true);
  Format hdrFmt;
  hdrFmt.setFontBold(true);
  hdrFmt.setFontColor(Qt::white);
  hdrFmt.setPatternBackgroundColor(QColor("#2C3E50"));
  hdrFmt.setHorizontalAlignment(Format::AlignHCenter);
  hdrFmt.setVerticalAlignment(Format::AlignVCenter);
  hdrFmt.setTextWrap(true);
  Format cellFmt; cellFmt.setVerticalAlignment(Format::AlignVCenter);
  cellFmt.setTextWrap(true);
  Format centerFmt;
  centerFmt.setHorizontalAlignment(Format::AlignHCenter);
  centerFmt.setVerticalAlignment(Format::AlignVCenter);
  Format naFmt;
  naFmt.setHorizontalAlignment(Format::AlignHCenter);
  naFmt.setVerticalAlignment(Format::AlignVCenter);
  naFmt.setFontColor(QColor("#BBBBBB"));
  naFmt.setPatternBackgroundColor(QColor("#F0F0F0"));

  const QString statusList = "\"TODO,READY,WIP,WFA,RETAKE,DONE\"";
  const TaskStatus allStatuses[] = {TaskStatus::Todo,   TaskStatus::Ready,
                                    TaskStatus::Wip,    TaskStatus::Wfa,
                                    TaskStatus::Retake, TaskStatus::Done};

  // Writes one (already-current) sheet: title, header, one row per shot in
  // shotIdxs, then status dropdowns + colour-by-value CF + auto-filter.
  auto writeSheet = [&](const QString &sheetName, const QString &subtitle,
                        const std::vector<int> &shotIdxs, const QStringList &cols) {
    xlsx.write(1, 1, model->production().isEmpty() ? tr("Production Spreadsheet")
                                                   : model->production(), titleFmt);
    if (!subtitle.isEmpty()) xlsx.write(2, 1, subtitle, subFmt);

    for (int c = 0; c < fixedCols.size(); c++)
      xlsx.write(headerRow, c + 1, fixedCols[c], hdrFmt);
    for (int t = 0; t < cols.size(); t++) {
      int sc = firstTaskCol + t * 2;
      xlsx.write(headerRow, sc,     cols[t],                hdrFmt);
      xlsx.write(headerRow, sc + 1, cols[t] + tr(" — Who"), hdrFmt);
    }
    xlsx.setRowHeight(headerRow, 28);
    xlsx.setColumnWidth(1, 23); xlsx.setColumnWidth(2, 12);
    xlsx.setColumnWidth(3, 10); xlsx.setColumnWidth(4, 8);
    xlsx.setColumnWidth(5, 11); xlsx.setColumnWidth(6, 15);
    xlsx.setColumnWidth(7, 26); xlsx.setColumnWidth(8, 26);
    for (int t = 0; t < cols.size(); t++) {
      xlsx.setColumnWidth(firstTaskCol + t * 2,     11);
      xlsx.setColumnWidth(firstTaskCol + t * 2 + 1, 12);
    }

    int row = firstDataRow;
    for (int si : shotIdxs) {
      const ShotData &sd = m_shots[si].data;

      // Thumbnail of the first panel.
      QImage thumb;
      if (mainXsh && !sd.panels.empty()) {
        TXsheet *subXsh = nullptr;
        for (int r = 0; r <= mainXsh->getFrameCount(); r++) {
          TXshCell cell = mainXsh->getCell(r, sd.xsheetColumn);
          if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
            subXsh = cell.m_level->getChildLevel()->getXsheet();
            break;
          }
        }
        if (subXsh) {
          int rH = qMax(1, qRound(140 /
                                  ZtoryShotOps::xsheetCameraAspect(subXsh)));
          QPixmap px = IconGenerator::renderXsheetFrame(
              subXsh, sd.panels[0].startFrame, TDimension(140, rH));
          if (!px.isNull()) {
            // Exact pixel size + pinned DPI so QXlsx computes a predictable EMU
            // size (renderXsheetFrame may return a retina/2× image).
            thumb = px.toImage().scaled(128, 72, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
            thumb.setDotsPerMeterX(3780);  // 96 dpi
            thumb.setDotsPerMeterY(3780);
          }
        }
      }
      // insertImage uses a 0-based cell anchor (write() is 1-based).
      if (!thumb.isNull()) xlsx.insertImage(row - 1, 0, thumb);
      xlsx.setRowHeight(row, 60);

      QString seqLabel;
      if (!sd.sequenceId.isEmpty())
        if (SequenceData *seq = model->findSequence(sd.sequenceId))
          seqLabel = seq->label;
      xlsx.write(row, 2, seqLabel,   centerFmt);
      xlsx.write(row, 3, sd.label(), centerFmt);

      int frames = sd.totalDuration();
      xlsx.write(row, 4, frames, centerFmt);
      // Sec/Fr = shot DURATION as seconds:frames (not a film position).
      xlsx.write(row, 5, QString("%1:%2").arg(frames / fps)
                             .arg(frames % fps, 2, 10, QChar('0')), centerFmt);

      // Notes: shot-level note if set, else the first panel's note.
      QString notesVal = sd.notes;
      if (notesVal.isEmpty() && !sd.panels.empty()) notesVal = sd.panels[0].notes;
      xlsx.write(row, 6, techOf(si),  centerFmt);   // Workflow (read-only)
      xlsx.write(row, 7, notesVal,    cellFmt);
      xlsx.write(row, 8, sd.vfxNotes, cellFmt);

      QStringList applicable = taskTypesOf(si);
      for (int t = 0; t < cols.size(); t++) {
        int sc = firstTaskCol + t * 2;
        const QString &tt = cols[t];
        if (!applicable.contains(tt)) {
          xlsx.write(row, sc,     QString("N/A"), naFmt);
          xlsx.write(row, sc + 1, QString(),      naFmt);
          continue;
        }
        TaskState ts = sd.tasks.value(tt);  // missing → default Todo
        Format sf;
        sf.setHorizontalAlignment(Format::AlignHCenter);
        sf.setVerticalAlignment(Format::AlignVCenter);
        sf.setFontBold(true);
        xlsx.write(row, sc,     ZtoryModel::taskStatusLabel(ts.status), sf);
        xlsx.write(row, sc + 1, ts.assignees.join(", "), centerFmt);
      }
      row++;
    }

    const int lastRow = row - 1;
    if (lastRow < firstDataRow) return;

    // Status dropdown (TODO/…/DONE) on every status column.
    for (int t = 0; t < cols.size(); t++) {
      int sc = firstTaskCol + t * 2;
      DataValidation dv(DataValidation::List);
      dv.setFormula1(statusList);
      dv.addRange(firstDataRow, sc, lastRow, sc);
      dv.setAllowBlank(true);
      xlsx.addDataValidation(dv);
    }

    // One CF block over all status columns: the fill colour follows the value.
    if (!cols.isEmpty()) {
      ConditionalFormatting cf;
      for (TaskStatus s : allStatuses) {
        Format f;
        f.setPatternBackgroundColor(statusColor(s));
        f.setFontColor(s == TaskStatus::Ready ? QColor(Qt::black)
                                              : QColor(Qt::white));
        f.setFontBold(true);
        cf.addHighlightCellsRule(ConditionalFormatting::Highlight_ContainsText,
                                 ZtoryModel::taskStatusLabel(s), f);
      }
      for (int t = 0; t < cols.size(); t++)
        cf.addRange(firstDataRow, firstTaskCol + t * 2,
                    lastRow, firstTaskCol + t * 2);
      xlsx.addConditionalFormatting(cf);
    }

    // Auto-filter on the header row + matching _FilterDatabase defined name
    // (LibreOffice needs the name to actually show the filter dropdowns).
    int lastCol = cols.isEmpty() ? fixedCols.size()
                                 : firstTaskCol + cols.size() * 2 - 1;
    if (QXlsx::Worksheet *ws = xlsx.currentWorksheet())
      ws->setAutoFilter(QXlsx::CellRange(headerRow, 1, lastRow, lastCol));
    QString fdb = QString("='%1'!%2:%3").arg(sheetName)
        .arg(QXlsx::CellReference(headerRow, 1).toString(true, true))
        .arg(QXlsx::CellReference(lastRow, lastCol).toString(true, true));
    xlsx.defineName("_xlnm._FilterDatabase", fdb, QString(), sheetName);
  };  // writeSheet

  auto sanitizeSheet = [](QString n) -> QString {
    for (QChar c : QString("\\/?*:[]")) n.replace(c, ' ');
    return n.simplified().left(31);
  };

  // Overview sheet: every shot, union of all used task columns.
  const QString overviewName = tr("All Shots");
  // A freshly-constructed QXlsx::Document has NO worksheet until the first
  // write/activeSheet() call, so sheetNames() can be empty here.  Calling
  // .first() on an empty QList is undefined behaviour — harmless on macOS
  // (shared-null QString) but an EXCEPTION_ACCESS_VIOLATION on Windows release
  // inside renameSheet's QString comparison.  Guard it: rename the default
  // sheet if present, otherwise create the named one (addSheet selects it).
  QStringList existingSheets = xlsx.sheetNames();
  if (existingSheets.isEmpty())
    xlsx.addSheet(overviewName);
  else
    xlsx.renameSheet(existingSheets.first(), overviewName);
  std::vector<int> allShots;
  for (int si = 0; si < (int)m_shots.size(); si++) allShots.push_back(si);
  QString subtitle;
  if (!model->title().isEmpty())   subtitle  = model->title();
  if (!model->episode().isEmpty()) subtitle += (subtitle.isEmpty() ? QString()
                                     : QString("  ·  ")) + tr("Episode ") + model->episode();
  writeSheet(overviewName, subtitle, allShots, taskCols);

  // One sheet per technique actually used, with only that technique's tasks.
  QStringList usedTechs;
  for (int si = 0; si < (int)m_shots.size(); si++) {
    QString tn = techOf(si);
    if (!usedTechs.contains(tn)) usedTechs << tn;
  }
  for (const QString &tn : usedTechs) {
    const Technique *t = model->findTechnique(tn);
    if (!t) continue;
    std::vector<int> shotIdxs;
    for (int si = 0; si < (int)m_shots.size(); si++)
      if (techOf(si) == tn) shotIdxs.push_back(si);
    QString sname = sanitizeSheet(tn);
    if (sname.compare(overviewName, Qt::CaseInsensitive) == 0) sname += " (wf)";
    if (!xlsx.addSheet(sname)) continue;
    writeSheet(sname, tn, shotIdxs, t->taskTypes);
  }

  if (xlsx.saveAs(path))
    QMessageBox::information(this, tr("Export Spreadsheet"),
                            tr("Exported to:\n%1").arg(path));
  else
    QMessageBox::warning(this, tr("Export Spreadsheet"),
                         tr("Failed to write:\n%1").arg(path));
}

//=============================================================================
// Export the same production data as a plain CSV (no thumbnails / colours) so
// productions can import it into their own templates or tracking systems.
//-----------------------------------------------------------------------------

void StoryboardPanel::onExportSpreadsheetCsv() {
  if (m_shots.empty()) {
    QMessageBox::information(this, tr("Export Spreadsheet CSV"),
                            tr("No shots to export."));
    return;
  }
  ZtoryModel *model = ZtoryModel::instance();
  pushTrackingToBoard();  // reflect tracker edits (model) into the exported copy
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  QString base = model->production().trimmed();
  QString ep   = model->episode().trimmed();
  if (!ep.isEmpty()) base = (base.isEmpty() ? ep : base + "_" + ep);
  if (base.isEmpty()) base = "spreadsheet"; else base += "_spreadsheet";
  base.replace(' ', '_').replace('/', '_');
  QString startDir = scene ? QString::fromStdWString(
      scene->getScenePath().getParentDir().getWideString()) : QString();
  QString suggested = startDir.isEmpty() ? base + ".csv"
                                         : startDir + "/" + base + ".csv";
  QString path = QFileDialog::getSaveFileName(
      this, tr("Export Production Spreadsheet (CSV)"), suggested,
      tr("CSV (*.csv)"));
  if (path.isEmpty()) return;
  if (!path.endsWith(".csv", Qt::CaseInsensitive)) path += ".csv";

  auto techOf = [&](int si) -> QString {
    const QString &t = m_shots[si].data.technique;
    return t.isEmpty() ? model->defaultTechnique() : t;
  };
  auto taskTypesOf = [&](int si) -> QStringList {
    const Technique *t = model->findTechnique(techOf(si));
    return t ? t->taskTypes : QStringList();
  };
  std::set<QString> usedSet;
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (const QString &tt : taskTypesOf(si)) usedSet.insert(tt);
  QStringList taskCols;
  for (const QString &tt : ZtoryModel::canonicalTaskOrder())
    if (usedSet.count(tt)) { taskCols << tt; usedSet.erase(tt); }
  for (const QString &tt : usedSet) taskCols << tt;

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, tr("Export Spreadsheet CSV"),
                         tr("Failed to write:\n%1").arg(path));
    return;
  }
  QTextStream out(&file);
  auto csv = [](QString s) -> QString {
    if (s.contains(',') || s.contains('"') || s.contains('\n')) {
      s.replace('"', "\"\"");
      return "\"" + s + "\"";
    }
    return s;
  };

  // Header.
  QStringList header = {tr("Sequence"), tr("Shot"), tr("Frames"), tr("Sec/Fr"),
                        tr("Workflow"), tr("Notes"), tr("VFX Notes")};
  for (const QString &tt : taskCols) { header << tt << (tt + tr(" Who")); }
  QStringList headerOut;
  for (const QString &h : header) headerOut << csv(h);
  out << headerOut.join(',') << '\n';

  int fps = model->fps() > 0 ? model->fps() : 24;
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const ShotData &sd = m_shots[si].data;
    QString seqLabel;
    if (!sd.sequenceId.isEmpty())
      if (SequenceData *seq = model->findSequence(sd.sequenceId))
        seqLabel = seq->label;
    int frames = sd.totalDuration();
    QString notesVal = sd.notes;
    if (notesVal.isEmpty() && !sd.panels.empty()) notesVal = sd.panels[0].notes;
    QStringList rowOut;
    rowOut << csv(seqLabel) << csv(sd.label()) << QString::number(frames)
           << csv(QString("%1:%2").arg(frames / fps)
                      .arg(frames % fps, 2, 10, QChar('0')))
           << csv(techOf(si)) << csv(notesVal) << csv(sd.vfxNotes);
    QStringList applicable = taskTypesOf(si);
    for (const QString &tt : taskCols) {
      if (!applicable.contains(tt)) { rowOut << "N/A" << QString(); continue; }
      TaskState ts = sd.tasks.value(tt);
      rowOut << ZtoryModel::taskStatusLabel(ts.status) << csv(ts.assignees.join(", "));
    }
    out << rowOut.join(',') << '\n';
  }
  file.close();
  QMessageBox::information(this, tr("Export Spreadsheet CSV"),
                          tr("Exported to:\n%1").arg(path));
}

//=============================================================================
// Storyboard Settings — edit project metadata (production / episode / title),
// the default production technique, and shot numbering after scene creation.
//-----------------------------------------------------------------------------

void StoryboardPanel::onStoryboardSettings() {
  ZtoryModel *model = ZtoryModel::instance();
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Storyboard Settings"));
  QVBoxLayout *lay = new QVBoxLayout(&dlg);

  // Same set and same order as the Production Tracker's Project page --
  // Production / Code / Season / Episode / Title / Default technique / Naming
  // pattern -- so the three places that edit this metadata cannot drift apart.
  // They all write the same singleton; only the window around them differs.
  QFormLayout *form      = new QFormLayout();
  QLineEdit *prodEdit    = new QLineEdit(model->production(), &dlg);
  QLineEdit *codeEdit    = new QLineEdit(model->effectiveCode(), &dlg);
  QLineEdit *seasonEdit  = new QLineEdit(model->season(), &dlg);
  QLineEdit *epEdit      = new QLineEdit(model->episode(), &dlg);
  QLineEdit *titleEdit   = new QLineEdit(model->title(), &dlg);
  QComboBox *techCombo   = new QComboBox(&dlg);
  QLineEdit *patternEdit = new QLineEdit(&dlg);
  patternEdit->setText(model->namingPattern().isEmpty()
                           ? model->defaultNamingPattern()
                           : model->namingPattern());
  patternEdit->setToolTip(
      tr("Tokens: {PROD} {CODE} {SEASON} {EP} {SEQ} {SHOT} {TASK} {VER}\n"
         "A field left empty leaves nothing behind, separators included."));
  codeEdit->setMaxLength(16);

  // Production and Code belong to Kitsu once the project is linked: editing
  // them locally would let the two copies diverge in silence.
  const bool kitsuOwned = model->isKitsuLinked();
  prodEdit->setReadOnly(kitsuOwned);
  codeEdit->setReadOnly(kitsuOwned);
  if (kitsuOwned) {
    const QString why = tr("Managed in Kitsu while the project is linked.");
    prodEdit->setToolTip(why);
    codeEdit->setToolTip(why);
  }

  for (const Technique &t : model->techniques()) techCombo->addItem(t.name);
  {
    int di = techCombo->findText(model->defaultTechnique());
    if (di >= 0) techCombo->setCurrentIndex(di);
  }
  form->addRow(tr("Production:"),        prodEdit);
  form->addRow(tr("Code:"),              codeEdit);
  form->addRow(tr("Season:"),            seasonEdit);
  form->addRow(tr("Episode:"),           epEdit);
  form->addRow(tr("Title:"),             titleEdit);
  form->addRow(tr("Default technique:"), techCombo);
  form->addRow(tr("Naming pattern:"),    patternEdit);
  lay->addLayout(form);

  QPushButton *numBtn = new QPushButton(tr("Shot Numbering…"), &dlg);
  connect(numBtn, &QPushButton::clicked, this,
          &StoryboardPanel::onNumberingConfig);
  lay->addWidget(numBtn);

  QDialogButtonBox *bb = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addWidget(bb);

  if (dlg.exec() != QDialog::Accepted) return;
  if (!kitsuOwned) {
    model->setProduction(prodEdit->text().trimmed());
    model->setCode(codeEdit->text().trimmed());
  }
  model->setSeason(seasonEdit->text().trimmed());
  model->setTitle(titleEdit->text().trimmed());
  model->setEpisode(epEdit->text().trimmed());
  model->setNamingPattern(patternEdit->text().trimmed());
  if (!techCombo->currentText().isEmpty())
    model->setDefaultTechnique(techCombo->currentText());
  saveZtoryc();                 // numbering / scene-level
  model->saveProjectDb();       // production/title/episode/defaultTechnique are project-level
  emit model->productionReloaded();  // refresh the Production Tracker's Project tab
}

//=============================================================================
// Set the production technique of the selected shot(s).  The technique drives
// which task columns apply in the spreadsheet (others become N/A).  An empty
// technique means "use the project default".
//-----------------------------------------------------------------------------

void StoryboardPanel::onSetTechnique() {
  std::vector<int> sel(m_selectedIndices.begin(), m_selectedIndices.end());
  if (sel.empty() && m_selectedShotIndex >= 0) sel.push_back(m_selectedShotIndex);
  if (sel.empty()) {
    QMessageBox::information(this, tr("Set Technique"),
                            tr("Select one or more shots first."));
    return;
  }
  ZtoryModel *model = ZtoryModel::instance();
  QStringList items;
  items << tr("(project default: %1)").arg(model->defaultTechnique());
  for (const Technique &t : model->techniques()) items << t.name;

  int cur = 0;
  QString curTech = (sel.front() < model->shotCount())
                        ? model->shot(sel.front()).technique
                        : QString();
  if (!curTech.isEmpty()) {
    int idx = items.indexOf(curTech);
    if (idx > 0) cur = idx;
  }
  bool ok = false;
  QString choice = QInputDialog::getItem(
      this, tr("Set Technique"),
      tr("Production technique for the selected shot(s):"), items, cur, false, &ok);
  if (!ok) return;
  QString tech = (choice == items.front()) ? QString() : choice;
  for (int si : sel)
    if (si >= 0 && si < model->shotCount())
      model->shot(si).technique = tech;  // model is authoritative; save pushes to Board
  saveZtoryc();
  emit model->taskStatusChanged();  // refresh the Production Tracker columns
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

//=============================================================================
// New Shot After Current — global command (works from any room, including
// inside a sub-scene: the artist never has to go back to the Board/Animatic
// just to say "next shot").
//-----------------------------------------------------------------------------

void StoryboardPanel::newShotAfterCurrent() {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/false)) {
    // Inside a sub-scene: remember it so we re-enter the freshly created
    // shot and the artist keeps drawing without a room/context switch.
    onAddShot();  // closes the sub-scene, inserts after the edited shot
    if (m_selectedShotIndex >= 0) onEditShot(m_selectedShotIndex);
    return;
  }
  onAddShot();
}

class ZtoryNewShotAfterCommand final : public MenuItemHandler {
public:
  ZtoryNewShotAfterCommand() : MenuItemHandler(MI_ZtoryNewShotAfter) {}
  void execute() override {
    for (QWidget *w : QApplication::allWidgets()) {
      if (auto *board = qobject_cast<StoryboardPanel *>(w)) {
        board->newShotAfterCurrent();
        return;
      }
    }
  }
} ztoryNewShotAfterCommand;

// Export commands (File ▸ Export ▸ Ztoryc) — route to the live Board panel.
namespace {
StoryboardPanel *findZtoryBoard() {
  for (QWidget *w : QApplication::allWidgets())
    if (auto *board = qobject_cast<StoryboardPanel *>(w)) return board;
  return nullptr;
}
void warnNoBoard() {
  QMessageBox::warning(nullptr, QObject::tr("Ztoryc Export"),
      QObject::tr("Open the Ztoryc Board at least once before exporting."));
}
}  // namespace

class ZtoryExportPdfCommand final : public MenuItemHandler {
public:
  ZtoryExportPdfCommand() : MenuItemHandler(MI_ZtoryExportPdf) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportPdf(); else warnNoBoard();
  }
} ztoryExportPdfCommand;

class ZtoryExportSpreadsheetXlsxCommand final : public MenuItemHandler {
public:
  ZtoryExportSpreadsheetXlsxCommand()
      : MenuItemHandler(MI_ZtoryExportSpreadsheetXlsx) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportSpreadsheet(); else warnNoBoard();
  }
} ztoryExportSpreadsheetXlsxCommand;

class ZtoryExportSpreadsheetCsvCommand final : public MenuItemHandler {
public:
  ZtoryExportSpreadsheetCsvCommand()
      : MenuItemHandler(MI_ZtoryExportSpreadsheetCsv) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportSpreadsheetCsv(); else warnNoBoard();
  }
} ztoryExportSpreadsheetCsvCommand;

class ZtoryExportShotsCommand final : public MenuItemHandler {
public:
  ZtoryExportShotsCommand() : MenuItemHandler(MI_ZtoryExportShots) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportShots(); else warnNoBoard();
  }
} ztoryExportShotsCommand;

// Il lip sync degli shot dal Board: l'altra meta' dell'opzione nell'export,
// quella che serve mentre lo storyboard e' ancora in lavorazione.
class ZtoryLipSyncShotsCommand final : public MenuItemHandler {
public:
  ZtoryLipSyncShotsCommand() : MenuItemHandler(MI_ZtoryLipSyncShots) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onLipSyncShots(); else warnNoBoard();
  }
} ztoryLipSyncShotsCommand;

class ZtoryExportShotsToProjectCommand final : public MenuItemHandler {
public:
  ZtoryExportShotsToProjectCommand()
      : MenuItemHandler(MI_ZtoryExportShotsToProject) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportShotsToProject();
    else warnNoBoard();
  }
} ztoryExportShotsToProjectCommand;

class ZtoryExportAnimaticCommand final : public MenuItemHandler {
public:
  ZtoryExportAnimaticCommand() : MenuItemHandler(MI_ZtoryExportAnimatic) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onExportAnimatic(); else warnNoBoard();
  }
} ztoryExportAnimaticCommand;

class ZtoryStoryboardSettingsCommand final : public MenuItemHandler {
public:
  ZtoryStoryboardSettingsCommand()
      : MenuItemHandler(MI_ZtoryStoryboardSettings) {}
  void execute() override {
    if (auto *b = findZtoryBoard()) b->onStoryboardSettings(); else warnNoBoard();
  }
} ztoryStoryboardSettingsCommand;
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
