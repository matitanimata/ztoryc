#include "ztorigpanel.h"

#include "tapp.h"
#include "menubarcommandids.h"

#include "toonz/preferences.h"

#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshcell.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/imagemanager.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "toonzqt/icongenerator.h"
#include "tinbetween.h"

#include "ext/plasticskeletondeformation.h"
#include "ext/plasticdeformerstorage.h"

#include "tundo.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QDebug>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QDialog>
#include <algorithm>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QAction>
#include <QMenu>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <map>
#include <queue>
#include <set>

//=============================================================================
// ZtoRigActionRow
//=============================================================================

namespace {
// The dial is a percentage on screen and a 0..1 factor in the data. Values
// beyond the ends are legitimate (over-pose, counter-pose), so the spin box
// goes further than the slider instead of clamping the model to the widget.
const int kSliderMin    = 0;
const int kSliderMax    = 100;
const double kSpinMin   = -2.0;
const double kSpinMax   = 2.0;
//! Small round icons for the two ends of the slider. Painted rather than typed:
//! a glyph depends on the font having it and lands at a different size in every
//! theme, and these two have to read as a matched pair.
//! Hollow = strength 0 (nothing of the action applied), solid = strength 1.
QIcon dotIcon(bool filled, const QColor &color) {
  const int side = 14;
  const qreal dpr = 2.0;  // painted oversized: sharp on Retina, scaled down on 1x
  QPixmap pm(side * dpr, side * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(color, 1.4));
  p.setBrush(filled ? QBrush(color) : Qt::NoBrush);
  const qreal m = 3.0;
  p.drawEllipse(QRectF(m, m, side - 2 * m, side - 2 * m));
  p.end();

  return QIcon(pm);
}

//! The remove button, same treatment: a drawn cross instead of a "×" character.
QIcon crossIcon(const QColor &color) {
  const int side = 14;
  const qreal dpr = 2.0;
  QPixmap pm(side * dpr, side * dpr);
  pm.setDevicePixelRatio(dpr);
  pm.fill(Qt::transparent);

  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QPen(color, 1.4));
  const qreal m = 4.0;
  p.drawLine(QPointF(m, m), QPointF(side - m, side - m));
  p.drawLine(QPointF(side - m, m), QPointF(m, side - m));
  p.end();

  return QIcon(pm);
}

}  // namespace

ZtoRigActionRow::ZtoRigActionRow(int index, const QString &name, double value,
                                 int mode, bool isBase,
                                 const std::vector<int> &allSkelIds,
                                 const std::set<int> &onSkelIds, QWidget *parent)
    : QWidget(parent)
    , m_index(index)
    , m_mode(mode)
    , m_isBase(isBase)
    , m_skelIds(onSkelIds) {
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(4);

  const QColor fg = palette().color(QPalette::WindowText);

  // How the action is stamped. Three states, so it cycles on click rather than
  // toggling: Add (push) / Pose (recall, whole skeleton) / Part (recall, its
  // own params only).
  m_modeButton = new QToolButton(this);
  m_modeButton->setFixedWidth(52);
  m_modeButton->setToolTip(
      tr("Click to cycle how the action is stamped:\n\n"
         "Add — adds on top of wherever the character is. Stacks with\n"
         "anything, but the result depends on the pose you started from.\n\n"
         "Pose — recalls the recorded pose exactly, over the WHOLE skeleton:\n"
         "anything else posed on this skeleton is reset. For full-body poses.\n\n"
         "Part — recalls exactly, but only the parts this action recorded.\n"
         "A mouth shape lands the same after any other shape and leaves the\n"
         "eyes alone. For phonemes and per-limb poses."));
  updateModeButton();
  lay->addWidget(m_modeButton);

  // Base: this action is what strength 0 means for its skeleton. The rig's
  // real rest pose is the EXPLODED layout, which is not something to dial back
  // to — record the assembled pose and mark it here.
  m_baseButton = new QToolButton(this);
  m_baseButton->setCheckable(true);
  m_baseButton->setChecked(m_isBase);
  m_baseButton->setFixedWidth(40);
  m_baseButton->setText(tr("Base"));
  m_baseButton->setToolTip(
      tr("Use this pose as the zero of its skeleton, in place of the rest\n"
         "pose — which on an exploded rig is the scattered layout.\n"
         "One per skeleton."));
  lay->addWidget(m_baseButton);

  // Which skeletons the action may be used on. Only worth showing on a rig
  // that HAS more than one.
  if (allSkelIds.size() > 1) {
    m_skelButton = new QToolButton(this);
    m_skelButton->setPopupMode(QToolButton::InstantPopup);
    m_skelButton->setToolTip(
        tr("Skeletons this action can be applied to.\n"
           "Recording ticks the one it was authored on; widen it only for a\n"
           "pose that really does work on another view."));
    auto *menu = new QMenu(m_skelButton);

    auto *allAct = menu->addAction(tr("All skeletons"));
    allAct->setCheckable(true);
    connect(allAct, &QAction::triggered, this, [this](bool on) {
      if (!on) return;  // unticking "All" alone would leave the action nowhere
      m_skelIds.clear();
      updateSkelButton();
      emit skeletonsChanged(m_index, m_skelIds);
    });
    m_skelActions.push_back(allAct);
    menu->addSeparator();

    for (int sid : allSkelIds) {
      auto *a = menu->addAction(tr("Skeleton %1").arg(sid));
      a->setCheckable(true);
      a->setData(sid);
      connect(a, &QAction::triggered, this, [this, sid](bool on) {
        if (on)
          m_skelIds.insert(sid);
        else
          m_skelIds.erase(sid);
        updateSkelButton();
        emit skeletonsChanged(m_index, m_skelIds);
      });
      m_skelActions.push_back(a);
    }

    m_skelButton->setMenu(menu);
    updateSkelButton();
    lay->addWidget(m_skelButton);
  }

  m_label = new QLabel(name, this);
  m_label->setMinimumWidth(80);
  m_label->setToolTip(name);
  lay->addWidget(m_label);

  // Rest and Full flank the slider, so their position says what they do: 0 is
  // at the left end of the travel, 1 at the right. That leaves nothing for a
  // label to add — a dot each, and the hint on hover.
  m_restBt = new QPushButton(this);
  m_restBt->setIcon(dotIcon(false, fg));
  m_restBt->setFixedWidth(24);
  m_restBt->setFlat(true);
  m_restBt->setToolTip(
      tr("Strength 0 — the skeleton's Base pose when it has one, its rest "
         "pose otherwise."));
  lay->addWidget(m_restBt);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setRange(kSliderMin, kSliderMax);
  m_slider->setValue((int)(value * 100.0 + 0.5));
  m_slider->setToolTip(tr("Pose strength at this frame: 0 = rest, 1 = the "
                          "recorded pose.\nMoving it writes the keyframes "
                          "directly — no separate Key step."));
  lay->addWidget(m_slider, 1);

  m_fullBt = new QPushButton(this);
  m_fullBt->setIcon(dotIcon(true, fg));
  m_fullBt->setFixedWidth(24);
  m_fullBt->setFlat(true);
  m_fullBt->setToolTip(tr("Strength 1 — the pose as recorded."));
  lay->addWidget(m_fullBt);

  m_spin = new QDoubleSpinBox(this);
  m_spin->setRange(kSpinMin, kSpinMax);
  m_spin->setSingleStep(0.05);
  m_spin->setDecimals(2);
  m_spin->setValue(value);
  m_spin->setToolTip(tr("Values outside 0..1 are allowed: over-pose and "
                        "counter-pose."));
  lay->addWidget(m_spin);

  auto *removeBt = new QPushButton(this);
  removeBt->setIcon(crossIcon(fg));
  removeBt->setFixedWidth(24);
  removeBt->setFlat(true);
  removeBt->setToolTip(
      tr("Remove this action — from every skeleton it is on.\n"
         "To take it off one skeleton only, untick it in the skeleton menu."));
  lay->addWidget(removeBt);

  // Drive the spin box: its valueChanged updates the slider and emits
  // guideChanged, so the display and the model stay in step.
  connect(m_restBt, &QPushButton::clicked, this,
          [this]() { m_spin->setValue(0.0); });
  connect(m_fullBt, &QPushButton::clicked, this,
          [this]() { m_spin->setValue(1.0); });
  connect(m_slider, &QSlider::sliderPressed, this,
          [this]() { emit guideBegin(m_index); });
  connect(m_slider, &QSlider::valueChanged, this, &ZtoRigActionRow::onSlider);
  connect(m_slider, &QSlider::sliderReleased, this,
          [this]() { emit guideCommit(m_index); });
  // The spin box (and the Rest/Full buttons that drive it) is a discrete edit:
  // begin + change + commit in one, so it too gets a single undo.
  connect(m_spin,
          static_cast<void (QDoubleSpinBox::*)(double)>(
              &QDoubleSpinBox::valueChanged),
          this, &ZtoRigActionRow::onSpin);
  connect(removeBt, &QPushButton::clicked, this,
          [this]() { emit removeRequested(m_index); });
  connect(m_modeButton, &QToolButton::clicked, this,
          &ZtoRigActionRow::onModeClicked);
  connect(m_baseButton, &QToolButton::toggled, this, [this](bool on) {
    m_isBase = on;
    emit baseToggled(m_index, on);
  });

  // The base pose IS the zero: dialling it would only ever write itself.
  if (m_isBase) setBaseAppearance();
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::updateSkelButton() {
  if (!m_skelButton) return;

  if (m_skelIds.empty())
    m_skelButton->setText(tr("All"));
  else {
    QStringList parts;
    for (int sid : m_skelIds) parts << QString::number(sid);
    m_skelButton->setText(parts.join(","));
  }
  m_skelButton->setFixedWidth(m_skelIds.size() > 2 ? 64 : 48);

  for (QAction *a : m_skelActions) {
    if (a->data().isValid())
      a->setChecked(m_skelIds.count(a->data().toInt()) > 0);
    else
      a->setChecked(m_skelIds.empty());  // the "All skeletons" entry
  }
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::updateModeButton() {
  static const char *names[] = {QT_TR_NOOP("Add"), QT_TR_NOOP("Pose"),
                                QT_TR_NOOP("Part")};
  const int m = (m_mode >= 0 && m_mode <= 2) ? m_mode : 0;
  m_modeButton->setText(tr(names[m]));
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onModeClicked() {
  m_mode = (m_mode + 1) % 3;
  updateModeButton();
  emit modeChanged(m_index, m_mode);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::setBaseAppearance() {
  const bool dialable = !m_isBase;
  m_slider->setEnabled(dialable);
  m_spin->setEnabled(dialable);
  m_restBt->setEnabled(dialable);
  m_fullBt->setEnabled(dialable);
  m_modeButton->setEnabled(dialable);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::setApplicable(bool on) {
  // Remove, Base and the skeleton menu stay live on purpose: you must be able
  // to tidy up an action of another view without first switching to that view.
  m_slider->setEnabled(on && !m_isBase);
  m_spin->setEnabled(on && !m_isBase);
  m_restBt->setEnabled(on && !m_isBase);
  m_fullBt->setEnabled(on && !m_isBase);
  m_modeButton->setEnabled(on && !m_isBase);
  m_label->setEnabled(on);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::setValueSilently(double v) {
  m_updating = true;
  m_spin->setValue(v);
  m_slider->setValue((int)(v * 100.0 + 0.5));
  m_updating = false;
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onSlider(int v) {
  if (m_updating) return;
  m_updating = true;
  m_spin->setValue(v / 100.0);
  m_updating = false;
  // Live during the drag (begin/commit come from sliderPressed/Released).
  emit guideChanged(m_index, v / 100.0);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onSpin(double v) {
  if (m_updating) return;
  m_updating = true;
  m_slider->setValue((int)(v * 100.0 + 0.5));
  m_updating = false;
  // A spin edit / Rest / Full is one discrete change: bracket it so it becomes
  // a single undo, like a slider gesture.
  emit guideBegin(m_index);
  emit guideChanged(m_index, v);
  emit guideCommit(m_index);
}

//=============================================================================
// ZtoRigPanel
//=============================================================================

//============================================================================
// ZtoRigAngleTrack — l'asse sono i GRADI, non i fotogrammi
//----------------------------------------------------------------------------

namespace {
const int kLaneH      = 30;   // altezza di una corsia
const int kTopPad     = 18;   // spazio per la riga dei gradi
const int kSidePad    = 46;   // spazio a sinistra per il nome del giunto
const int kKeyR       = 6;    // mezzo lato del rombo di una chiave
const double kMinSpan = 30.0; // apertura minima dell'asse, in gradi
}  // namespace

ZtoRigAngleTrack::ZtoRigAngleTrack(QWidget *parent) : QWidget(parent) {
  setMouseTracking(true);
  setMinimumHeight(kTopPad + kLaneH);
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::setKeys(const QVector<ZtoRigTrackKey> &keys,
                               const QMap<QString, double> &currentAngles) {
  m_keys          = keys;
  m_currentAngles = currentAngles;
  setMinimumHeight(kTopPad + qMax(1, lanes().size()) * kLaneH);
  update();
}

//----------------------------------------------------------------------------

QVector<QString> ZtoRigAngleTrack::lanes() const {
  QVector<QString> out;
  for (const ZtoRigTrackKey &k : m_keys)
    if (!out.contains(k.m_driver)) out.push_back(k.m_driver);
  return out;
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::angleRange(double &lo, double &hi) const {
  lo = hi = 0.0;
  for (const ZtoRigTrackKey &k : m_keys)
    lo = qMin(lo, k.m_angle), hi = qMax(hi, k.m_angle);
  // L'indicatore deve poter uscire dal gruppo di chiavi: se il giunto e' piegato
  // oltre l'ultima correttiva, quello e' proprio cio' che si vuole vedere.
  for (double a : m_currentAngles) lo = qMin(lo, a), hi = qMax(hi, a);

  const double margin = qMax(kMinSpan * 0.25, (hi - lo) * 0.15);
  lo -= margin, hi += margin;
  if (hi - lo < kMinSpan) {
    const double c = 0.5 * (lo + hi);
    lo = c - kMinSpan * 0.5, hi = c + kMinSpan * 0.5;
  }
}

//----------------------------------------------------------------------------

double ZtoRigAngleTrack::angleToX(double angle) const {
  double lo, hi;
  angleRange(lo, hi);
  const double w = qMax(1, width() - kSidePad - 8);
  return kSidePad + w * (angle - lo) / (hi - lo);
}

double ZtoRigAngleTrack::xToAngle(int x) const {
  double lo, hi;
  angleRange(lo, hi);
  const double w = qMax(1, width() - kSidePad - 8);
  return lo + (hi - lo) * (x - kSidePad) / w;
}

int ZtoRigAngleTrack::laneTop(int lane) const {
  return kTopPad + lane * kLaneH;
}

//----------------------------------------------------------------------------

int ZtoRigAngleTrack::keyAt(const QPoint &p) const {
  const QVector<QString> ln = lanes();
  for (int i = 0; i < m_keys.size(); ++i) {
    const int lane = ln.indexOf(m_keys[i].m_driver);
    if (lane < 0) continue;
    const QPointF c(angleToX(m_keys[i].m_angle), laneTop(lane) + kLaneH * 0.5);
    if (qAbs(p.x() - c.x()) <= kKeyR + 2 && qAbs(p.y() - c.y()) <= kKeyR + 2)
      return i;
  }
  return -1;
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing, true);

  const QColor fg   = palette().color(QPalette::WindowText);
  const QColor dim  = QColor(fg.red(), fg.green(), fg.blue(), 90);
  const QColor rail = QColor(fg.red(), fg.green(), fg.blue(), 45);

  double lo, hi;
  angleRange(lo, hi);

  // Riga dei gradi. Passo scelto perche' restino cinque o sei tacche: numeri
  // troppo fitti su una traccia stretta non si leggono e basta.
  const double span = hi - lo;
  double step       = 10.0;
  const double cand[] = {5.0, 10.0, 15.0, 30.0, 45.0, 60.0, 90.0};
  for (double c : cand)
    if (span / c <= 6.0) { step = c; break; }

  p.setPen(dim);
  QFont f = p.font();
  f.setPointSizeF(qMax(7.0, f.pointSizeF() - 2.0));
  p.setFont(f);
  for (double a = std::ceil(lo / step) * step; a <= hi; a += step) {
    const double x = angleToX(a);
    p.drawLine(QPointF(x, kTopPad - 4), QPointF(x, height()));
    p.drawText(QRectF(x - 20, 0, 40, kTopPad - 5),
               Qt::AlignHCenter | Qt::AlignBottom,
               QString::number((int)qRound(a)) + QString::fromUtf8("°"));
  }

  const QVector<QString> ln = lanes();
  for (int lane = 0; lane < ln.size(); ++lane) {
    const int top = laneTop(lane);
    const double yc = top + kLaneH * 0.5;

    p.setPen(dim);
    p.drawText(QRectF(2, top, kSidePad - 6, kLaneH),
               Qt::AlignVCenter | Qt::AlignRight, ln[lane]);

    p.setPen(QPen(rail, 2.0));
    p.drawLine(QPointF(kSidePad, yc), QPointF(width() - 6, yc));

    // L'indicatore: dove sta DAVVERO il giunto adesso. E' la cosa che dice se
    // una correttiva e' spenta perche' sbagliata o solo perche' non ci si e'
    // ancora arrivati.
    auto it = m_currentAngles.find(ln[lane]);
    if (it != m_currentAngles.end()) {
      const double x = angleToX(it.value());
      p.setPen(QPen(QColor(230, 90, 90), 1.5));
      p.drawLine(QPointF(x, top + 3), QPointF(x, top + kLaneH - 3));
    }
  }

  // Le chiavi sopra tutto, cosi' l'indicatore non le nasconde.
  for (int i = 0; i < m_keys.size(); ++i) {
    const int lane = ln.indexOf(m_keys[i].m_driver);
    if (lane < 0) continue;
    const QPointF c(angleToX(m_keys[i].m_angle),
                    laneTop(lane) + kLaneH * 0.5);
    QPolygonF d;
    d << QPointF(c.x(), c.y() - kKeyR) << QPointF(c.x() + kKeyR, c.y())
      << QPointF(c.x(), c.y() + kKeyR) << QPointF(c.x() - kKeyR, c.y());

    const bool hot = (i == m_hoverKey || i == m_dragKey);
    p.setPen(QPen(fg, hot ? 1.8 : 1.2));
    p.setBrush(hot ? QBrush(fg) : QBrush(palette().color(QPalette::Window)));
    p.drawPolygon(d);
  }

  if (m_keys.isEmpty()) {
    p.setPen(dim);
    p.drawText(rect(), Qt::AlignCenter, tr("no correctives"));
  }
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) return;
  const int k = keyAt(e->pos());
  if (k < 0) return;
  m_dragKey = k;
  // Il clic porta subito il giunto li': rivedere cio' che si e' scolpito e'
  // l'uso principale, e non deve costare un gesto in piu'.
  emit keyActivated(m_keys[k].m_index);
  update();
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::mouseMoveEvent(QMouseEvent *e) {
  if (m_dragKey >= 0) {
    emit keyMoved(m_keys[m_dragKey].m_index, xToAngle(e->pos().x()));
    return;
  }
  const int h = keyAt(e->pos());
  if (h != m_hoverKey) {
    m_hoverKey = h;
    setCursor(h >= 0 ? Qt::SizeHorCursor : Qt::ArrowCursor);
    update();
  }
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::mouseReleaseEvent(QMouseEvent *) {
  m_dragKey = -1;
  update();
}

//----------------------------------------------------------------------------

void ZtoRigAngleTrack::contextMenuEvent(QContextMenuEvent *e) {
  const int k = keyAt(e->pos());
  if (k < 0) return;
  QMenu menu(this);
  QAction *del = menu.addAction(tr("Delete Corrective"));
  if (menu.exec(e->globalPos()) == del)
    emit keyRemoveRequested(m_keys[k].m_index);
}

//============================================================================
// ZtoRigPanel
//----------------------------------------------------------------------------

ZtoRigPanel::ZtoRigPanel(QWidget *parent) : TPanel(parent) {
  auto *root    = new QWidget(this);
  auto *rootLay = new QVBoxLayout(root);
  rootLay->setContentsMargins(0, 0, 0, 0);
  rootLay->setSpacing(0);

  m_tabs = new QTabWidget(root);
  rootLay->addWidget(m_tabs, 1);

  // ---- scheda Pose ----
  auto *posesTab = new QWidget(m_tabs);
  auto *lay      = new QVBoxLayout(posesTab);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(4);

  m_recordBt = new QPushButton(tr("Record Pose as Action…"), posesTab);
  m_recordBt->setToolTip(
      tr("Store the pose authored at the current frame as a new action.\n"
         "Nothing changes on screen: the new dial starts at 0."));
  lay->addWidget(m_recordBt);

  // ---- Vector pose, FIRST TEST ----
  //
  // HIDDEN unless ZTORYC_VECPOSE is set (Franco, 2026-07-27). This is the first
  // experiment, not a feature: the slider OVERWRITES the current frame's drawing
  // in place, with no undo and nothing saved. Shipping two buttons that can
  // destroy a drawing to someone who cannot know that is not a fair trade for
  // letting them try it early. Set the variable to get the box back.
  if (::getenv("ZTORYC_VECPOSE")) {
    auto *vecBox = new QWidget(posesTab);
    auto *vl     = new QHBoxLayout(vecBox);
    vl->setContentsMargins(0, 2, 0, 2);
    vl->setSpacing(4);

    auto *aBt = new QPushButton(tr("Vec A"), vecBox);
    auto *bBt = new QPushButton(tr("Vec B"), vecBox);
    aBt->setToolTip(tr("Capture the current frame's vector drawing as end A."));
    bBt->setToolTip(tr("Capture the current frame's vector drawing as end B.\n"
                       "Must be the SAME drawing with points MOVED — adding or "
                       "removing points breaks the correspondence."));
    aBt->setFixedWidth(52);
    bBt->setFixedWidth(52);
    vl->addWidget(aBt);
    vl->addWidget(bBt);

    m_vecSlider = new QSlider(Qt::Horizontal, vecBox);
    m_vecSlider->setRange(0, 100);
    m_vecSlider->setEnabled(false);
    m_vecSlider->setToolTip(
        tr("Interpolate A -> B into the CURRENT FRAME's drawing.\n"
           "Destructive and without undo: use a throwaway scene."));
    vl->addWidget(m_vecSlider, 1);

    m_vecLabel = new QLabel(tr("vector test: no ends"), vecBox);
    vl->addWidget(m_vecLabel);

    lay->addWidget(vecBox);

    connect(aBt, &QPushButton::clicked, this, &ZtoRigPanel::onVecGrabA);
    connect(bBt, &QPushButton::clicked, this, &ZtoRigPanel::onVecGrabB);
    connect(m_vecSlider, &QSlider::valueChanged, this,
            &ZtoRigPanel::onVecBlend);
  }

  m_showAllBt = new QCheckBox(tr("Show all skeletons"), posesTab);
  m_showAllBt->setToolTip(
      tr("Off: only the actions usable on the skeleton at the current frame.\n"
         "On: every action, grouped by skeleton — for tidying up the views\n"
         "you are not drawing right now."));
  m_showAllBt->setVisible(false);  // pointless on a single-skeleton rig
  lay->addWidget(m_showAllBt);
  connect(m_showAllBt, &QCheckBox::toggled, this, &ZtoRigPanel::rebuild);

  m_emptyLabel = new QLabel(
      tr("No pose action on this column.\n\n"
         "Pose the character, then Record. Undo the pose and raise the\n"
         "dial: the pose comes back — driven by the action this time."),
      posesTab);
  m_emptyLabel->setWordWrap(true);
  m_emptyLabel->setAlignment(Qt::AlignTop);
  lay->addWidget(m_emptyLabel);

  auto *rowsHost = new QWidget(posesTab);
  m_rowsLay      = new QVBoxLayout(rowsHost);
  m_rowsLay->setContentsMargins(0, 0, 0, 0);
  m_rowsLay->setSpacing(2);
  m_rowsLay->addStretch(1);

  m_scroll = new QScrollArea(posesTab);
  m_scroll->setWidgetResizable(true);
  m_scroll->setWidget(rowsHost);
  lay->addWidget(m_scroll, 1);

  m_tabs->addTab(posesTab, tr("Poses"));

  // ---- scheda Correttive ----
  auto *corrTab = new QWidget(m_tabs);
  auto *corrLay = new QVBoxLayout(corrTab);
  corrLay->setContentsMargins(4, 4, 4, 4);
  corrLay->setSpacing(4);

  m_corrEmptyLabel = new QLabel(
      tr("No joint corrective on this column.\n\n"
         "Bend a joint, turn on the corrective brush in the Plastic tool,\n"
         "and sculpt the shape back: the corrective is created here, named\n"
         "after the joint and the angle you sculpted at."),
      corrTab);
  m_corrEmptyLabel->setWordWrap(true);
  m_corrEmptyLabel->setAlignment(Qt::AlignTop);
  corrLay->addWidget(m_corrEmptyLabel);

  m_corrTrack = new ZtoRigAngleTrack(corrTab);
  m_corrTrack->setToolTip(
      tr("Correctives on the angle of their driving joint.\n"
         "Click a key to take the joint to that bend; drag it to move the\n"
         "corrective to another angle; right-click to delete.\n"
         "The red line is where the joint actually is now."));
  corrLay->addWidget(m_corrTrack, 1);

  connect(m_corrTrack, &ZtoRigAngleTrack::keyActivated, this,
          &ZtoRigPanel::onCorrectiveKeyActivated);
  connect(m_corrTrack, &ZtoRigAngleTrack::keyMoved, this,
          &ZtoRigPanel::onCorrectiveKeyMoved);
  connect(m_corrTrack, &ZtoRigAngleTrack::keyRemoveRequested, this,
          &ZtoRigPanel::onCorrectiveRemove);

  m_tabs->addTab(corrTab, tr("Correctives"));

  setWidget(root);

  connect(m_recordBt, &QPushButton::clicked, this, &ZtoRigPanel::onRecord);

  TApp *app = TApp::instance();
  connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
          SLOT(rebuild()));
  connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(rebuild()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), this,
          SLOT(rebuild()));
  // Frame changes only refresh the numbers: rebuilding here would fight the
  // user's drag and reset the scroll position on every frame during playback.
  connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
          SLOT(onFrameSwitched()));
  // Undo/redo of a Record or Remove changes which actions exist without a
  // column/scene switch: refresh (which rebuilds on a count mismatch) so the
  // rows follow the history.
  connect(TUndoManager::manager(), SIGNAL(somethingChanged()), this,
          SLOT(refreshValues()));

  rebuild();
}

//-----------------------------------------------------------------------------

PlasticSkeletonDeformationP ZtoRigPanel::currentDeformation() const {
  TApp *app = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  if (!xsh) return PlasticSkeletonDeformationP();

  const int col = app->getCurrentColumn()->getColumnIndex();
  if (col < 0) return PlasticSkeletonDeformationP();

  TStageObject *obj =
      xsh->getStageObject(TStageObjectId::ColumnId(col));
  if (!obj) return PlasticSkeletonDeformationP();

  return obj->getPlasticSkeletonDeformation();
}

//-----------------------------------------------------------------------------

std::vector<ZtoRigPanel::CharPart> ZtoRigPanel::characterParts() const {
  std::vector<CharPart> out;
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  if (!xsh) return out;

  const int col = app->getCurrentColumn()->getColumnIndex();
  if (col < 0) return out;

  // Climb to the character's top column, then take every column that reaches
  // it. Same walk PlasticTool::characterColumns does for the IK flag — the
  // character is the same one, so the definition has to be too.
  TStageObjectId topId = TStageObjectId::ColumnId(col);
  for (int guard = 0; guard < 1000; ++guard) {
    TStageObject *obj = xsh->getStageObject(topId);
    if (!obj) break;
    const TStageObjectId pid = obj->getParent();
    if (!pid.isColumn()) break;
    topId = pid;
  }

  const double frame = currentFrame();
  for (int c = 0; c < xsh->getColumnCount(); ++c) {
    TStageObjectId walk = TStageObjectId::ColumnId(c);
    bool inCharacter    = false;
    for (int guard = 0; guard < 1000; ++guard) {
      if (walk == topId) {
        inCharacter = true;
        break;
      }
      TStageObject *obj = xsh->getStageObject(walk);
      if (!obj) break;
      const TStageObjectId pid = obj->getParent();
      if (!pid.isColumn()) break;
      walk = pid;
    }
    if (!inCharacter) continue;

    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    const PlasticSkeletonDeformationP &sd = obj->getPlasticSkeletonDeformation();
    if (!sd) continue;

    CharPart part;
    part.m_sd  = sd;
    part.m_col = c;
    // Each column keeps its own clock: paramsTime is per stage object.
    part.m_frame = obj->paramsTime(frame);
    out.push_back(part);
  }
  return out;
}

//-----------------------------------------------------------------------------

int ZtoRigPanel::actionIndexByName(const PlasticSkeletonDeformationP &sd,
                                   const QString &name) {
  if (!sd || name.isEmpty()) return -1;
  for (int i = 0; i < sd->poseActionsCount(); ++i) {
    const PoseAction *act = sd->poseAction(i);
    if (act && act->m_name == name) return i;
  }
  return -1;
}

//-----------------------------------------------------------------------------

QString ZtoRigPanel::actionNameAt(int index) const {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return QString();
  const PoseAction *act = sd->poseAction(index);
  return act ? act->m_name : QString();
}

//-----------------------------------------------------------------------------

double ZtoRigPanel::currentFrame() const {
  return (double)TApp::instance()->getCurrentFrame()->getFrame();
}

double ZtoRigPanel::paramsFrame() const {
  // EVERY read or write of the deformation goes through here. The deformation's
  // params — and the skeleton-id curve with them — live in the stage object's
  // own time, which is the xsheet frame only on a plain column. Mixing the two
  // is invisible on a single-level rig and breaks a multilevel one: the pose
  // gets recorded at one time and stamped at another, and the skeleton the
  // action is bound to stops matching the skeleton the stamping sees.
  TStageObject *obj = currentStageObject();
  return obj ? obj->paramsTime(currentFrame()) : currentFrame();
}

TStageObject *ZtoRigPanel::currentStageObject() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  if (!xsh) return nullptr;
  const int col = app->getCurrentColumn()->getColumnIndex();
  if (col < 0) return nullptr;
  return xsh->getStageObject(TStageObjectId::ColumnId(col));
}

//-----------------------------------------------------------------------------

// Breadth-first over the column tree from `startCol`, invalidating each stage
// object's cached placement. Mirrors PlasticTool's own flush: the pose blend
// can move a parent, and a child column glued to it must refresh. Free so both
// the panel and the undo object can reach it.
static void ztorigFlushPlacements(TXsheet *xsh, int startCol) {
  if (!xsh || startCol < 0) return;
  std::set<int> visited;
  std::queue<int> pending;
  visited.insert(startCol);
  pending.push(startCol);
  while (!pending.empty()) {
    const int c = pending.front();
    pending.pop();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    obj->invalidate();

    const TStageObjectId pid = obj->getParent();
    if (pid.isColumn() && !visited.count(pid.getIndex())) {
      visited.insert(pid.getIndex());
      pending.push(pid.getIndex());
    }
    for (TStageObject *child : obj->getChildren()) {
      const TStageObjectId cid = child->getId();
      if (cid.isColumn() && !visited.count(cid.getIndex())) {
        visited.insert(cid.getIndex());
        pending.push(cid.getIndex());
      }
    }
  }
}

void ZtoRigPanel::flushConnectedPlacements() {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  ztorigFlushPlacements(xsh, app->getCurrentColumn()->getColumnIndex());

  // The dial GUIDE is a TDoubleParam the deformation does not observe (unlike
  // the real pose params), so moving the slider never fired onChange and the
  // MESH deformer cache stayed stale — the skeleton overlay moved but the
  // drawing did not follow until a click wrote a real param. Invalidate the
  // deformer explicitly here so the mesh re-solves on the next redraw.
  if (const PlasticSkeletonDeformationP sd = currentDeformation())
    PlasticDeformerStorage::instance()->invalidateDeformation(
        sd.getPointer(), PlasticDeformerStorage::NONE);
}

//=============================================================================
// Undo — a Record or a Remove is one step: swap the whole pose-action vector.
// Snapshotting the vector (guides shared by pointer) is simpler and safer than
// tracking per-action edits, and covers re-recording an existing action too.
//=============================================================================

namespace {

class UndoPoseActions final : public TUndo {
  TXsheetHandle *m_xshHandle;
  PlasticSkeletonDeformationP m_sd;
  int m_col;
  std::vector<PoseAction> m_before, m_after;
  // The action list alone is not enough: copying it copies the guide SMART
  // POINTERS, so before and after share one curve and a strength written at
  // record time would survive the undo. The curves travel separately.
  PlasticSkeletonDeformation::PoseKeyState m_beforeKeys, m_afterKeys;
  QString m_label;

  void apply(const std::vector<PoseAction> &actions,
             const PlasticSkeletonDeformation::PoseKeyState &keys) const {
    if (!m_sd) return;
    m_sd->setPoseActions(actions);
    m_sd->setPoseKeyState(keys);
    if (m_xshHandle && m_xshHandle->getXsheet())
      ztorigFlushPlacements(m_xshHandle->getXsheet(), m_col);
    if (m_xshHandle) m_xshHandle->notifyXsheetChanged();
  }

public:
  UndoPoseActions(TXsheetHandle *xshHandle,
                  const PlasticSkeletonDeformationP &sd, int col,
                  const std::vector<PoseAction> &before,
                  const std::vector<PoseAction> &after,
                  const PlasticSkeletonDeformation::PoseKeyState &beforeKeys,
                  const PlasticSkeletonDeformation::PoseKeyState &afterKeys,
                  const QString &label)
      : m_xshHandle(xshHandle)
      , m_sd(sd)
      , m_col(col)
      , m_before(before)
      , m_after(after)
      , m_beforeKeys(beforeKeys)
      , m_afterKeys(afterKeys)
      , m_label(label) {}

  void undo() const override { apply(m_before, m_beforeKeys); }
  void redo() const override { apply(m_after, m_afterKeys); }
  int getSize() const override {
    return (int)((m_before.size() + m_after.size()) * sizeof(PoseAction)) + 64;
  }
  QString getHistoryString() override { return m_label; }
};

// Undo for stamping a pose into plastic keys: swap the whole pose-key snapshot
// (param keyframes + guide curves). Covers the exclusive zeroing of the other
// dials too, since those live in the same snapshot.
class UndoPoseKeyState final : public TUndo {
  TXsheetHandle *m_xshHandle;
  PlasticSkeletonDeformationP m_sd;
  int m_col;
  PlasticSkeletonDeformation::PoseKeyState m_before, m_after;
  // Optional column TRANSFORM key (Global scope Stage/All). m_xformFrame < 0
  // means the gesture did not touch the transform.
  int m_xformFrame;
  bool m_beforeHad, m_afterHad;
  TStageObject::Keyframe m_beforeKey, m_afterKey;

  void restoreXform(bool had, const TStageObject::Keyframe &key) const {
    if (m_xformFrame < 0 || !m_xshHandle || !m_xshHandle->getXsheet()) return;
    TStageObject *o =
        m_xshHandle->getXsheet()->getStageObject(TStageObjectId::ColumnId(m_col));
    if (!o) return;
    if (had)
      o->setKeyframeWithoutUndo(m_xformFrame, key);
    else
      o->removeKeyframeWithoutUndo(m_xformFrame);
  }

  void apply(const PlasticSkeletonDeformation::PoseKeyState &st, bool xHad,
             const TStageObject::Keyframe &xKey) const {
    if (!m_sd) return;
    m_sd->setPoseKeyState(st);
    restoreXform(xHad, xKey);
    if (m_xshHandle && m_xshHandle->getXsheet()) {
      TXsheet *xsh = m_xshHandle->getXsheet();
      ztorigFlushPlacements(xsh, m_col);
      if (TStageObject *o = xsh->getStageObject(TStageObjectId::ColumnId(m_col)))
        o->updateKeyframes();  // keep the xsheet diamonds in sync on undo/redo
    }
    if (m_xshHandle) m_xshHandle->notifyXsheetChanged();
  }

public:
  UndoPoseKeyState(TXsheetHandle *xshHandle,
                   const PlasticSkeletonDeformationP &sd, int col,
                   const PlasticSkeletonDeformation::PoseKeyState &before,
                   const PlasticSkeletonDeformation::PoseKeyState &after,
                   int xformFrame, bool beforeHad,
                   const TStageObject::Keyframe &beforeKey, bool afterHad,
                   const TStageObject::Keyframe &afterKey)
      : m_xshHandle(xshHandle)
      , m_sd(sd)
      , m_col(col)
      , m_before(before)
      , m_after(after)
      , m_xformFrame(xformFrame)
      , m_beforeHad(beforeHad)
      , m_afterHad(afterHad)
      , m_beforeKey(beforeKey)
      , m_afterKey(afterKey) {}
  void undo() const override { apply(m_before, m_beforeHad, m_beforeKey); }
  void redo() const override { apply(m_after, m_afterHad, m_afterKey); }
  int getSize() const override { return 256; }
  QString getHistoryString() override {
    return QObject::tr("ZtoRig: Pose");
  }
};

}  // namespace

//-----------------------------------------------------------------------------

void ZtoRigPanel::rebuild() {
  rebuildCorrectives();

  for (ZtoRigActionRow *row : m_rows) {
    m_rowsLay->removeWidget(row);
    row->deleteLater();
  }
  m_rows.clear();
  for (QWidget *h : m_groupHeaders) {
    m_rowsLay->removeWidget(h);
    h->deleteLater();
  }
  m_groupHeaders.clear();

  const PlasticSkeletonDeformationP sd = currentDeformation();
  const int count = sd ? sd->poseActionsCount() : 0;

  m_recordBt->setEnabled((bool)sd);
  m_emptyLabel->setVisible(count == 0);
  m_scroll->setVisible(count > 0);

  if (!sd) {
    m_emptyLabel->setText(
        tr("This column has no plastic skeleton.\n\n"
           "Build one with the Plastic tool, then come back here."));
    m_emptyLabel->setVisible(true);
    return;
  }

  m_emptyLabel->setText(
      tr("No pose action on this column.\n\n"
         "Pose the character, then Record. Undo the pose and raise the\n"
         "dial: the pose comes back — driven by the action this time."));

  const double frame = paramsFrame();

  // One block per skeleton, ids ascending, and an action appears under EVERY
  // skeleton it applies to — repeated rather than filed under a combined
  // "Skeleton 1, 3" heading. Reading the panel then answers the question you
  // actually have: what can I use on the skeleton I am working on?
  //
  // The copies stay in step for free: rows are identified by action index, and
  // every refresh reads each row's value back from that index.
  //
  // Order is stable across frames on purpose: putting the active skeleton first
  // would reshuffle the panel under the user's hand at every drawing change.
  const std::vector<int> allIds = allSkeletonIds(sd);
  const int activeSkel          = sd->skeletonId(frame);
  m_builtSkelId                 = activeSkel;

  // The picker only means something on a rig with more than one skeleton.
  m_showAllBt->setVisible(allIds.size() > 1);
  const bool showAll = m_showAllBt->isVisible() && m_showAllBt->isChecked();

  // Default view: only the skeleton you are on. On a turnaround the other
  // views' actions are just noise taking up the panel.
  std::vector<int> skelIds = allIds;
  if (!showAll) {
    skelIds.clear();
    skelIds.push_back(activeSkel);
  }

  std::map<int, std::vector<int>> bySkel;
  std::vector<int> orphans;
  for (int i = 0; i < count; ++i) {
    const PoseAction *act = sd->poseAction(i);
    if (!act) continue;
    bool placed = false;
    for (int sid : skelIds)
      if (act->appliesTo(sid)) {
        bySkel[sid].push_back(i);
        placed = true;
      }
    // Bound only to skeletons that no longer exist: still show it, or it would
    // be invisible AND undeletable. Only worth surfacing in the full view —
    // filtered to one skeleton, an orphan is not on it by definition.
    if (!placed && showAll) orphans.push_back(i);
  }

  // Headings earn their space only when there is more than one block.
  const bool showHeaders = skelIds.size() > 1 || !orphans.empty();

  std::vector<std::pair<int, std::vector<int>>> groups;
  for (int sid : skelIds)
    if (bySkel.count(sid)) groups.push_back({sid, bySkel[sid]});
  if (!orphans.empty()) groups.push_back({-1, orphans});

  for (const auto &group : groups) {
    if (showHeaders) m_groupHeaders.push_back(addGroupHeader(group.first));

    for (int i : group.second) {
      const PoseAction *act = sd->poseAction(i);
      const double v        = sd->poseStrengthAt(i, frame);

      auto *row = new ZtoRigActionRow(i, act->m_name, v, act->m_mode,
                                      act->m_isBase, allIds, act->m_skelIds,
                                      this);
      connect(row, &ZtoRigActionRow::guideBegin, this,
              &ZtoRigPanel::onGuideBegin);
      connect(row, &ZtoRigActionRow::guideChanged, this,
              &ZtoRigPanel::onGuideChanged);
      connect(row, &ZtoRigActionRow::guideCommit, this,
              &ZtoRigPanel::onGuideCommit);
      connect(row, &ZtoRigActionRow::removeRequested, this,
              &ZtoRigPanel::onRemove);
      connect(row, &ZtoRigActionRow::modeChanged, this,
              &ZtoRigPanel::onModeChanged);
      connect(row, &ZtoRigActionRow::baseToggled, this,
              &ZtoRigPanel::onBaseToggled);
      connect(row, &ZtoRigActionRow::skeletonsChanged, this,
              &ZtoRigPanel::onSkeletonsChanged);

      m_rowsLay->insertWidget(m_rowsLay->count() - 1, row);
      m_rows.push_back(row);
    }
  }

  m_builtActionCount = count;
  updateApplicability();
}

//-----------------------------------------------------------------------------

std::vector<int> ZtoRigPanel::allSkeletonIds(
    const PlasticSkeletonDeformationP &sd) const {
  std::vector<int> ids;
  if (!sd) return ids;
  PlasticSkeletonDeformation::skelId_iterator st, sEnd;
  sd->skeletonIds(st, sEnd);
  for (; st != sEnd; ++st) ids.push_back(*st);
  std::sort(ids.begin(), ids.end());
  return ids;
}

//-----------------------------------------------------------------------------

QWidget *ZtoRigPanel::addGroupHeader(int skelId) {
  auto *header = new QWidget(this);
  auto *lay    = new QVBoxLayout(header);
  lay->setContentsMargins(4, 6, 4, 2);
  lay->setSpacing(2);

  auto *line = new QFrame(header);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  lay->addWidget(line);

  auto *title = new QLabel(
      skelId < 0 ? tr("Not on any skeleton") : tr("Skeleton %1").arg(skelId),
      header);
  QFont f = title->font();
  f.setBold(true);
  title->setFont(f);
  lay->addWidget(title);

  m_rowsLay->insertWidget(m_rowsLay->count() - 1, header);
  return header;
}

//-----------------------------------------------------------------------------

double ZtoRigPanel::sd_poseStrength(int index, double frame) const {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  return sd ? sd->poseStrengthAt(index, frame) : 0.0;
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::updateApplicability() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  const double frame = paramsFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setApplicable(sd->poseActionAppliesAt(row->index(), frame));
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::rebuildCorrectives() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  const int count = sd ? sd->meshCorrectivesCount() : 0;
  m_builtCorrectiveCount = count;

  m_corrEmptyLabel->setVisible(!sd || count == 0);
  m_corrTrack->setVisible(sd && count > 0);

  if (!sd) {
    m_corrEmptyLabel->setText(
        tr("This column has no plastic skeleton.\n\n"
           "Build one with the Plastic tool, then come back here."));
    return;
  }
  if (count == 0) {
    m_corrEmptyLabel->setText(
        tr("No joint corrective on this column.\n\n"
           "Bend a joint, turn on the corrective brush in the Plastic tool,\n"
           "and sculpt the shape back: the corrective lands here, as a key on\n"
           "the angle you sculpted it at."));
    return;
  }

  QVector<ZtoRigTrackKey> keys;
  for (int i = 0; i < count; ++i) {
    const MeshCorrective *mc = sd->meshCorrective(i);
    if (!mc) continue;
    ZtoRigTrackKey k;
    k.m_index  = i;
    k.m_driver = mc->m_driverVertexName;
    k.m_angle  = mc->m_fullAngle;  // la chiave sta dove entra a pieno regime
    keys.push_back(k);
  }
  m_corrTrack->setKeys(keys, driverAngles());
}

//-----------------------------------------------------------------------------

QMap<QString, double> ZtoRigPanel::driverAngles() const {
  QMap<QString, double> out;
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return out;

  const double frame = paramsFrame();
  for (int i = 0; i < sd->meshCorrectivesCount(); ++i) {
    const MeshCorrective *mc = sd->meshCorrective(i);
    if (!mc || out.contains(mc->m_driverVertexName)) continue;
    SkVD *vd = sd->vertexDeformation(mc->m_driverVertexName);
    if (!vd || !vd->m_params[SkVD::ANGLE]) continue;  // assente = ignoto, non 0
    out.insert(mc->m_driverVertexName,
               vd->m_params[SkVD::ANGLE]->getValue(frame));
  }
  return out;
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::refreshCorrectiveWeights() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  // Una correttiva puo' nascere sotto il pennello mentre il pannello e' aperto.
  if (sd->meshCorrectivesCount() != m_builtCorrectiveCount) {
    rebuildCorrectives();
    return;
  }
  if (!m_corrTrack->isVisible()) return;

  // Le chiavi non si muovono col fotogramma, solo gli indicatori.
  QVector<ZtoRigTrackKey> keys;
  for (int i = 0; i < sd->meshCorrectivesCount(); ++i) {
    const MeshCorrective *mc = sd->meshCorrective(i);
    if (!mc) continue;
    ZtoRigTrackKey k;
    k.m_index  = i;
    k.m_driver = mc->m_driverVertexName;
    k.m_angle  = mc->m_fullAngle;
    keys.push_back(k);
  }
  m_corrTrack->setKeys(keys, driverAngles());
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onCorrectiveKeyActivated(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  const MeshCorrective *mc = sd->meshCorrective(index);
  if (!mc) return;

  SkVD *vd = sd->vertexDeformation(mc->m_driverVertexName);
  if (!vd || !vd->m_params[SkVD::ANGLE]) return;

  // Porta il giunto alla piega a cui la correttiva e' stata scolpita: e' il
  // modo di rivederla e ritoccarla.
  //
  // ⚠️ PROVVISORIO: qui si scrive nel parametro della scena, come fanno gia' i
  // dial delle pose. Nella modalita' di rigging che stiamo progettando questa
  // sara' una posa DI LAVORO che non tocca le chiavi — e allora la riserva qui
  // sopra sparisce, invece di essere tamponata con una voce di undo.
  vd->m_params[SkVD::ANGLE]->setValue(paramsFrame(), mc->m_fullAngle);

  flushConnectedPlacements();
  refreshCorrectiveWeights();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onCorrectiveKeyMoved(int index, double angle) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  MeshCorrective *mc = sd->meshCorrective(index);
  if (!mc) return;

  mc->m_fullAngle = angle;

  // Le correttive di uno stesso giunto sono incatenate: l'angolo di riposo di
  // una e' quello di pieno della precedente. Spostare una chiave deve rifare la
  // catena, altrimenti restano intervalli scavalcati o buchi in cui non entra
  // piu' niente.
  std::vector<const MeshCorrective *> sameDriver;
  for (int i = 0; i < sd->meshCorrectivesCount(); ++i) {
    const MeshCorrective *o = sd->meshCorrective(i);
    if (o && o->m_driverVertexName == mc->m_driverVertexName)
      sameDriver.push_back(o);
  }
  for (int i = 0; i < sd->meshCorrectivesCount(); ++i) {
    MeshCorrective *o = sd->meshCorrective(i);
    if (!o || o->m_driverVertexName != mc->m_driverVertexName) continue;
    double rest = 0.0;
    for (const MeshCorrective *q : sameDriver)
      if (q != o && fabs(q->m_fullAngle) < fabs(o->m_fullAngle) &&
          fabs(q->m_fullAngle) > fabs(rest))
        rest = q->m_fullAngle;
    o->m_restAngle = rest;
  }

  flushConnectedPlacements();
  refreshCorrectiveWeights();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onCorrectiveRemove(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  const MeshCorrective *mc = sd->meshCorrective(index);
  if (!mc) return;

  // La forma scolpita se ne va e non torna: si chiede.
  const QString name = mc->m_name;
  if (QMessageBox::question(
          this, tr("Delete Corrective"),
          tr("Delete the corrective '%1'?\n\nThe sculpted shape is lost.")
              .arg(name),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) != QMessageBox::Yes)
    return;

  sd->removeMeshCorrective(index);
  flushConnectedPlacements();
  rebuildCorrectives();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::refreshValues() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  // The row count can go stale if the deformation changed behind our back
  // (undo of a Record, say): fall back to a full rebuild instead of reading
  // past the end.
  if (sd->poseActionsCount() != m_builtActionCount) {
    rebuild();
    return;
  }

  const double frame = paramsFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd->poseStrengthAt(row->index(), frame));

  refreshCorrectiveWeights();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onFrameSwitched() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  if (sd->poseActionsCount() != m_builtActionCount) {
    rebuild();
    return;
  }

  // The slider READS the pose strength off the keys at the new frame: 0 where
  // the character is at rest for this action, 1 where it is fully in the pose.
  // So it always shows where you are and can be dialled in or out.
  const double frame = paramsFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd->poseStrengthAt(row->index(), frame));

  refreshCorrectiveWeights();

  // The active skeleton can change with the frame (turnaround). Which actions
  // are even listed depends on it, so that case needs a rebuild — but only
  // that case: rebuilding on every frame would reset the scroll during
  // playback and fight a drag in progress.
  if (sd->skeletonId(frame) != m_builtSkelId)
    rebuild();
  else
    updateApplicability();
}

//-----------------------------------------------------------------------------

bool ZtoRigPanel::askRecordDetails(const std::vector<int> &allSkelIds,
                                   int activeSkelId, QString &name,
                                   std::set<int> &skelIds) {
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Record Pose as Action"));
  auto *lay = new QVBoxLayout(&dlg);

  lay->addWidget(new QLabel(tr("Action name:"), &dlg));
  auto *nameEdit = new QLineEdit(
      tr("action%1").arg(currentDeformation()->poseActionsCount() + 1), &dlg);
  nameEdit->selectAll();
  lay->addWidget(nameEdit);

  // Only ask about skeletons when there is a choice to make. The active one is
  // ticked: that is where the pose was authored and where it certainly works.
  std::vector<QCheckBox *> boxes;
  if (allSkelIds.size() > 1) {
    lay->addSpacing(6);
    auto *hint = new QLabel(tr("Can be applied to:"), &dlg);
    lay->addWidget(hint);
    for (int sid : allSkelIds) {
      auto *box = new QCheckBox(tr("Skeleton %1").arg(sid), &dlg);
      box->setChecked(sid == activeSkelId);
      lay->addWidget(box);
      boxes.push_back(box);
    }
    auto *note = new QLabel(
        tr("Tick another view only if the pose really works there: vertex\n"
           "names are shared, but lengths and offsets are not."),
        &dlg);
    note->setWordWrap(true);
    lay->addWidget(note);
  }

  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return false;
  name = nameEdit->text();
  if (name.trimmed().isEmpty()) return false;

  skelIds.clear();
  for (size_t i = 0; i < boxes.size(); ++i)
    if (boxes[i]->isChecked()) skelIds.insert(allSkelIds[i]);
  // Single-skeleton rig, or every box unticked: bind it to the one it was
  // authored on rather than leaving it applicable everywhere by accident.
  if (skelIds.empty()) skelIds.insert(activeSkelId);
  return true;
}

//-----------------------------------------------------------------------------

namespace {

//! The vector drawing being edited, or null with a reason in \p why.
/*!
  There are TWO ways a drawing is "current" and they resolve differently: with
  the level strip in front the frame handle holds a level FrameId, while in the
  xsheet it holds a scene row and the drawing has to be read out of the CELL.
  Asking for getFid() in the second case gets you nothing, which reported a
  perfectly good vector level as "not a vector level".
*/
TVectorImageP currentVectorImage(TXshSimpleLevel **outLevel, TFrameId *outFid,
                                 QString *why = 0) {
  TApp *app                = TApp::instance();
  TFrameHandle *frameH     = app->getCurrentFrame();
  TXshSimpleLevel *sl      = 0;
  TFrameId fid;

  if (frameH->isEditingLevel()) {
    TXshLevel *xl = app->getCurrentLevel() ? app->getCurrentLevel()->getLevel() : 0;
    if (xl) sl = xl->getSimpleLevel();
    fid = frameH->getFid();
  } else {
    TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet() : 0;
    if (!xsh) {
      if (why) *why = QObject::tr("no xsheet");
      return TVectorImageP();
    }
    const int row = frameH->getFrame();
    const int col = app->getCurrentColumn()->getColumnIndex();
    const TXshCell cell = xsh->getCell(row, col);
    if (cell.isEmpty()) {
      if (why) *why = QObject::tr("empty cell (row %1, col %2)").arg(row + 1).arg(col + 1);
      return TVectorImageP();
    }
    sl  = cell.getSimpleLevel();
    fid = cell.getFrameId();
  }

  if (!sl) {
    if (why) *why = QObject::tr("no level here");
    return TVectorImageP();
  }
  if (sl->getType() != PLI_XSHLEVEL) {
    if (why) *why = QObject::tr("level is not PLI (vector)");
    return TVectorImageP();
  }

  TVectorImageP vi = (TVectorImageP)sl->getFrame(fid, true);
  if (!vi) {
    if (why) *why = QObject::tr("no drawing at %1").arg(fid.getNumber());
    return TVectorImageP();
  }
  if (outLevel) *outLevel = sl;
  if (outFid) *outFid = fid;
  return vi;
}

}  // namespace

void ZtoRigPanel::onVecGrabA() {
  QString why;
  TVectorImageP vi = currentVectorImage(nullptr, nullptr, &why);
  if (!vi) {
    m_vecLabel->setText(why);
    return;
  }
  // clone() does not carry the palette — it belongs to the LEVEL, not to the
  // drawing. Without it the tween comes out with style indices pointing at
  // nothing and the result is invisible: strokes there, nothing on screen.
  m_vecA = vi->clone();
  m_vecA->setPalette(vi->getPalette());
  currentVectorImage(nullptr, &m_vecFidA, nullptr);
  m_vecSlider->setEnabled((bool)m_vecA && (bool)m_vecB);
  m_vecLabel->setText(m_vecB ? tr("A+B ready") : tr("A set"));
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onVecGrabB() {
  QString why;
  TVectorImageP vi = currentVectorImage(nullptr, nullptr, &why);
  if (!vi) {
    m_vecLabel->setText(why);
    return;
  }
  m_vecB = vi->clone();
  m_vecB->setPalette(vi->getPalette());
  currentVectorImage(nullptr, &m_vecFidB, nullptr);
  m_vecSlider->setEnabled((bool)m_vecA && (bool)m_vecB);

  // Mismatched structure is THE failure mode of this whole idea, so say it out
  // loud rather than letting the result look merely ugly.
  if (m_vecA && m_vecA->getStrokeCount() != m_vecB->getStrokeCount())
    m_vecLabel->setText(tr("A+B: %1 vs %2 strokes — will not match")
                            .arg(m_vecA->getStrokeCount())
                            .arg(m_vecB->getStrokeCount()));
  else
    m_vecLabel->setText(m_vecA ? tr("A+B ready") : tr("B set"));
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onVecBlend(int value) {
  if (!m_vecA || !m_vecB) return;

  TXshSimpleLevel *sl = 0;
  TFrameId fid;
  TVectorImageP cur = currentVectorImage(&sl, &fid, nullptr);
  if (!cur || !sl) return;

  // Never write over the frames A and B were taken from. Learned the hard way:
  // sitting on B and moving the slider replaced B with the tween, and the pose
  // was gone for good — there is no undo here.
  if (fid == m_vecFidA || fid == m_vecFidB) {
    m_vecLabel->setText(tr("stand on a THIRD drawing, not on A or B"));
    return;
  }

  const double t = value / 100.0;
  TInbetween tw(m_vecA, m_vecB);
  TVectorImageP mid = tw.tween(t);

  if (::getenv("ZTORYC_VECPOSE_DIAG"))
    qDebug().noquote() << QString(
                            "[VECPOSE] t=%1 A=%2 B=%3 mid=%4 cur=%5 "
                            "palA=%6 palMid=%7 palCur=%8")
                            .arg(t, 0, 'f', 2)
                            .arg(m_vecA ? (int)m_vecA->getStrokeCount() : -1)
                            .arg(m_vecB ? (int)m_vecB->getStrokeCount() : -1)
                            .arg(mid ? (int)mid->getStrokeCount() : -1)
                            .arg((int)cur->getStrokeCount())
                            .arg(m_vecA && m_vecA->getPalette() ? "y" : "n")
                            .arg(mid && mid->getPalette() ? "y" : "n")
                            .arg(cur->getPalette() ? "y" : "n");

  if (!mid || mid->getStrokeCount() == 0) {
    m_vecLabel->setText(tr("tween gave nothing"));
    return;
  }

  // Replace the CONTENT of the drawing that is already on screen, not the
  // image object. setFrame swaps the level's entry, but the viewer and the
  // image cache still hold the old object — the write lands somewhere nobody
  // is looking at, which is exactly what "I see nothing" looked like.
  {
    const TRectD bb = mid->getStroke(0)->getBBox();
    QString adds;
    while (cur->getStrokeCount() > 0) cur->deleteStroke(0);
    for (UINT i = 0; i < mid->getStrokeCount(); ++i) {
      // discardPoints=false: the default drops any stroke whose bbox is empty,
      // and we would rather see a degenerate stroke than silently lose the
      // drawing.
      const int r = cur->addStroke(new TStroke(*mid->getStroke(i)), false);
      adds += QString(" %1").arg(r);
    }
    if (::getenv("ZTORYC_VECPOSE_DIAG"))
      qDebug().noquote()
        << QString("[VECPOSE2] midBBox=(%1,%2)-(%3,%4) added=%5 curAfter=%6")
               .arg(bb.x0, 0, 'f', 1).arg(bb.y0, 0, 'f', 1)
               .arg(bb.x1, 0, 'f', 1).arg(bb.y1, 0, 'f', 1)
               .arg(adds)
               .arg((int)cur->getStrokeCount());
  }
  if (!cur->getPalette()) cur->setPalette(mid->getPalette());

  // Exactly what a drawing tool does after editing an image (see
  // TTool::notifyImageChanged): mark the frame as EDITED and refresh the icons.
  //
  // NOT invalidateFrame / ImageManager::invalidate. Those mean "this drawing is
  // stale, rebuild it from the source" — and the source on disk knows nothing
  // about the edit, so they threw away the interpolation and left the drawing
  // empty. That was the drawing "disappearing", and the reason only ONE slider
  // event ever reached the log: from the second one on there was no drawing
  // left to find.
  sl->touchFrame(fid);
  IconGenerator::instance()->invalidate(sl, fid);
  IconGenerator::instance()->invalidateSceneIcon();

  TApp *app = TApp::instance();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentLevel()->notifyLevelChange();
  app->getCurrentXsheet()->notifyXsheetChanged();
  // Repaint the viewer: the panel is not a tool, so nothing else asks for it.
  if (app->getCurrentTool())
    if (TTool *tool = app->getCurrentTool()->getTool()) tool->invalidate();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onRecord() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  const std::vector<int> allIds = allSkeletonIds(sd);
  const int activeSkel          = sd->skeletonId(paramsFrame());

  QString name;
  std::set<int> skelIds;
  if (!askRecordDetails(allIds, activeSkel, name, skelIds)) return;
  name = name.trimmed();

  // Record on EVERY column of the character, not just the selected one. On an
  // exploded rig the pose lives across the columns; recording only the current
  // one gave an action that moved a limb and forgot the body — and that
  // disappeared from the panel as soon as another column was selected.
  //
  // A column the pose does not move records no deltas, which is exactly right:
  // as an absolute Pose it then drives that column to its base, so switching
  // poses cannot leave a stale limb behind.
  const std::vector<CharPart> parts = characterParts();
  if (parts.empty()) return;

  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();

  for (const CharPart &part : parts) {
    const std::vector<PoseAction> before = part.m_sd->getPoseActions();
    const PlasticSkeletonDeformation::PoseKeyState beforeKeys =
        part.m_sd->getPoseKeyState();

    const int idx = part.m_sd->recordPoseAction(name, part.m_frame);
    if (PoseAction *act = part.m_sd->poseAction(idx)) {
      // The skeleton ids chosen in the dialog belong to the column the dialog
      // was opened on; another column has its own numbering, so it keeps what
      // recordPoseAction bound it to.
      if (part.m_sd == sd) act->m_skelIds = skelIds;
    }

    const std::vector<PoseAction> after = part.m_sd->getPoseActions();
    const PlasticSkeletonDeformation::PoseKeyState afterKeys =
        part.m_sd->getPoseKeyState();

    manager->add(new UndoPoseActions(
        TApp::instance()->getCurrentXsheet(), part.m_sd, part.m_col, before,
        after, beforeKeys, afterKeys, tr("ZtoRig: Record Action")));
  }

  manager->endBlock();

  TApp *app = TApp::instance();
  app->getCurrentScene()->setDirtyFlag(true);
  // The new dial is 0, so the render is unchanged; still flush so the viewer is
  // in a known-consistent state and later dial moves start from a clean cache.
  flushConnectedPlacements();
  app->getCurrentXsheet()->notifyXsheetChanged();
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideBegin(int index) {
  const QString name = actionNameAt(index);
  if (name.isEmpty()) return;

  // Snapshot for one undo per gesture: the pose params of EVERY column the
  // gesture will write, plus the column transform key at this frame (which the
  // commit may add under Global Stage/All).
  m_dragBefore.clear();
  for (const CharPart &part : characterParts())
    m_dragBefore.push_back({part.m_sd, part.m_sd->getPoseKeyState()});

  m_dragXformFrame  = (int)currentFrame();
  m_dragXformHadKey = false;
  if (TStageObject *o = currentStageObject()) {
    m_dragXformHadKey = o->isKeyframe(m_dragXformFrame);
    if (m_dragXformHadKey) m_dragXformOldKey = o->getKeyframe(m_dragXformFrame);
  }

  // Freeze the base an Add layers onto: without this the slider reads back its
  // own previous write on every move and the offset compounds (see
  // beginPoseDrag).
  for (const CharPart &part : characterParts()) {
    const int i = actionIndexByName(part.m_sd, name);
    if (i >= 0) part.m_sd->beginPoseDrag(i, part.m_frame);
  }
  m_dragActive = true;
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideChanged(int index, double value) {
  const QString name = actionNameAt(index);
  if (name.isEmpty()) return;
  if (!m_dragActive) onGuideBegin(index);

  // The slider IS the animation: write the pose straight into plastic keys at
  // the current frame, live, on every column of the character. A full Pose
  // overwrites the shared params (so Poses are mutually exclusive by
  // construction); an Add only its own, so partial controllers stack.
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  for (const CharPart &part : characterParts()) {
    const int i = actionIndexByName(part.m_sd, name);
    if (i < 0) continue;
    part.m_sd->applyPoseStrength(i, value, part.m_frame);
    if (xsh)
      if (TStageObject *o =
              xsh->getStageObject(TStageObjectId::ColumnId(part.m_col)))
        o->updateKeyframes();
  }

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  flushConnectedPlacements();  // also invalidates the mesh deformer
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideCommit(int index) {
  const QString name = actionNameAt(index);
  if (!m_dragActive || name.isEmpty()) {
    m_dragActive = false;
    return;
  }
  m_dragActive = false;

  for (const CharPart &part : characterParts()) part.m_sd->endPoseDrag();

  // Respect the Plastic tool's Global Key scope: with All (or Stage) a pose
  // also drops a TRANSFORM key on the column, exactly like posing by hand.
  const int scope                 = Preferences::instance()->getIntValue(GlobalKeyScope);
  bool afterHad                   = m_dragXformHadKey;
  TStageObject::Keyframe afterKey = m_dragXformOldKey;
  int xformFrame                  = -1;
  TStageObject *obj               = currentStageObject();
  if (obj && scope != 1 /* Stage or All */) {
    xformFrame = (int)currentFrame();
    obj->setKeyframeWithoutUndo(xformFrame);
    obj->updateKeyframes();
    afterHad = true;
    afterKey = obj->getKeyframe(xformFrame);
  }

  // One undo for the gesture, covering every column it touched.
  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();
  const int curCol = TApp::instance()->getCurrentColumn()->getColumnIndex();
  for (const auto &snap : m_dragBefore) {
    const PlasticSkeletonDeformation::PoseKeyState after =
        snap.first->getPoseKeyState();
    // The transform key belongs to the selected column only; the others pass
    // -1, which UndoPoseKeyState reads as "this gesture did not touch it".
    const bool isCurrent = (snap.first == currentDeformation());
    manager->add(new UndoPoseKeyState(
        TApp::instance()->getCurrentXsheet(), snap.first, curCol, snap.second,
        after, isCurrent ? xformFrame : -1, m_dragXformHadKey,
        m_dragXformOldKey, afterHad, afterKey));
  }
  manager->endBlock();
  m_dragBefore.clear();

  // An absolute Pose wipes what the other actions had written, and zeroes their
  // records to match: refresh every slider, not just the one just dragged.
  const double frame = paramsFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd_poseStrength(row->index(), frame));

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onModeChanged(int index, int mode) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  PoseAction *act = sd->poseAction(index);
  if (!act) return;

  // Same action, same mode on every column it spans.
  for (const CharPart &part : characterParts()) {
    const int i = actionIndexByName(part.m_sd, act->m_name);
    if (i >= 0) part.m_sd->poseAction(i)->m_mode = mode;
  }
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  flushConnectedPlacements();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onBaseToggled(int index, bool isBase) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  const QString name = actionNameAt(index);
  for (const CharPart &part : characterParts()) {
    const int i = actionIndexByName(part.m_sd, name);
    if (i >= 0) part.m_sd->setPoseActionAsBase(i, isBase);
  }
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  flushConnectedPlacements();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  // Only one action per skeleton can be the base, so another row's button may
  // have just been cleared: rebuild rather than guess which.
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onSkeletonsChanged(int index, const std::set<int> &skelIds) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  PoseAction *act = sd->poseAction(index);
  if (!act) return;

  // The tick list is per column: each has its own skeleton numbering, so only
  // the column the menu belongs to is touched.
  act->m_skelIds = skelIds;
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  // The action may have just left or joined a block, and the same action can
  // have a row under several skeletons: rebuild rather than patch.
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onRemove(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  const PoseAction *act = sd->poseAction(index);
  if (!act) return;

  if (QMessageBox::question(
          this, tr("Remove Action"),
          tr("Remove the action \"%1\"?\n\nUndo brings it back.")
              .arg(act->m_name),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  // The action spans the character: remove every part of it, or the columns
  // left behind would keep a half pose that nothing can drive any more.
  const QString name    = act->m_name;
  TApp *app             = TApp::instance();
  TUndoManager *manager = TUndoManager::manager();
  manager->beginBlock();

  for (const CharPart &part : characterParts()) {
    const int i = actionIndexByName(part.m_sd, name);
    if (i < 0) continue;

    const std::vector<PoseAction> before = part.m_sd->getPoseActions();
    const PlasticSkeletonDeformation::PoseKeyState beforeKeys =
        part.m_sd->getPoseKeyState();
    part.m_sd->removePoseAction(i);
    const std::vector<PoseAction> after = part.m_sd->getPoseActions();
    const PlasticSkeletonDeformation::PoseKeyState afterKeys =
        part.m_sd->getPoseKeyState();

    manager->add(new UndoPoseActions(app->getCurrentXsheet(), part.m_sd,
                                     part.m_col, before, after, beforeKeys,
                                     afterKeys, tr("ZtoRig: Remove Action")));
  }
  manager->endBlock();

  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
  rebuild();
}

//=============================================================================
// Factory
//=============================================================================

class ZtoRigPanelFactory final : public TPanelFactory {
public:
  ZtoRigPanelFactory() : TPanelFactory("ZtoRigPanel") {}

  TPanel *createPanel(QWidget *parent) override {
    auto *panel = new ZtoRigPanel(parent);
    panel->setObjectName("ZtoRigPanel");
    panel->setWindowTitle("ZtoRig");
    return panel;
  }

  void initialize(TPanel *) override { assert(0); }

} ztoRigPanelFactory;
