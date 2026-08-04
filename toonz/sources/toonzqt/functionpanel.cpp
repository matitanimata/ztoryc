

#include "toonzqt/functionpanel.h"

// TnzQt includes
#include "toonzqt/functionselection.h"
#include "toonzqt/functionsegmentviewer.h"
#include "toonzqt/imageutils.h"
#include "functionpaneltools.h"
#include "toonz/tstageobject.h"
#include "toonzqt/gutil.h"
#include "toonzqt/functionsheet.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/doubleparamcmd.h"
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"
#include "toonz/txsheet.h"

// TnzBase includes
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include "tunit.h"

// TnzCore includes
#include "tcommon.h"

#include "tools/toolcommandids.h"
#include "tools/cursormanager.h"
#include "tools/cursors.h"

// Qt includes
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QListWidget>
#include <QMap>
#include <QLabel>
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/dvdialog.h"
#include "toonz/txsheetexpr.h"
#include "toonz/doubleparamcmd.h"
#include "texpression.h"
#include <QSettings>

#include <cmath>

namespace {

void drawCircle(QPainter &painter, double x, double y, double r) {
  painter.drawEllipse(x - r, y - r, 2 * r, 2 * r);
}
void drawCircle(QPainter &painter, const QPointF &p, double r) {
  painter.drawEllipse(p.x() - r, p.y() - r, 2 * r, 2 * r);
}
void drawSquare(QPainter &painter, double x, double y, double r) {
  painter.drawRect(x - r, y - r, 2 * r, 2 * r);
}
void drawSquare(QPainter &painter, const QPointF &p, double r) {
  drawSquare(painter, p.x(), p.y(), r);
}

void drawRoundedSquare(QPainter &painter, const QPointF &p, double r) {
  painter.drawRoundedRect(p.x() - r, p.y() - r, 2 * r, 2 * r, 99, 99,
                          Qt::RelativeSize);
}

double norm2(const QPointF &p) { return p.x() * p.x() + p.y() * p.y(); }

class FunctionPanelZoomer final : public ImageUtils::ShortcutZoomer {
  FunctionPanel *m_panel;

public:
  FunctionPanelZoomer(FunctionPanel *panel)
      : ShortcutZoomer(panel), m_panel(panel) {}

  bool zoom(bool zoomin, bool resetZoom) override {
    if (resetZoom)
      m_panel->fitGraphToWindow();
    else {
      double f  = 1.25;
      double sc = zoomin ? f : 1.0 / f;
      QPoint center(m_panel->width() / 2, m_panel->height() / 2);
      m_panel->zoom(sc, sc, center);
    }

    return true;
  }
};

}  // namespace

//=============================================================================
//
// Ruler
//
//-----------------------------------------------------------------------------

class Ruler {
  double m_minValue, m_step;
  int m_labelPeriod, m_labelOffset, m_tickCount;

  double m_unit, m_pan, m_vOrigin;
  int m_x0, m_x1;
  int m_minLabelDistance, m_minDistance;
  double m_minStep;

public:
  Ruler();

  // unit,pan define the world to viewport transformation: pixel = value * unit
  // + pan
  // note: unit can be <0 (e.g. in the vertical rulers)
  // use vOrigin!=0 when there is a offset between values and labels
  // (e.g. frame=0 is visualized as 1)
  void setTransform(double unit, double pan, double vOrigin = 0) {
    m_unit    = unit;
    m_pan     = pan;
    m_vOrigin = vOrigin;
  }

  void setRange(int x0, int x1) {
    m_x0 = x0;
    m_x1 = x1;
  }  // [x0,x1] is the pixel range

  // set minimum distance (pixel) between two consecutive labels
  void setMinLabelDistance(int distance) { m_minLabelDistance = distance; }
  // set minimum distance (pixel) between two consecutive ticks
  void setMinDistance(int distance) { m_minDistance = distance; }

  // use setMinStep to define a minimum tick (e.g. for integer rulers as 'frame'
  // call setMinStep(1);)
  void setMinStep(double step) { m_minStep = step; }

  void compute();  // call compute() once, before calling the following methods

  int getTickCount() const { return m_tickCount; }
  double getTick(int index) const { return m_minValue + index * m_step; }
  bool isLabel(int index) const {
    return ((m_labelOffset + index) % m_labelPeriod) == 0;
  }
};

//-----------------------------------------------------------------------------

Ruler::Ruler()
    : m_minValue(0)
    , m_step(1)
    , m_labelPeriod(2)
    , m_labelOffset(0)
    , m_tickCount(0)
    , m_unit(1)
    , m_pan(0)
    , m_x0(0)
    , m_x1(100)
    , m_minLabelDistance(20)
    , m_minDistance(5)
    , m_minStep(0) {}

//-----------------------------------------------------------------------------

void Ruler::compute() {
  assert(m_x0 < m_x1);
  assert(m_unit != 0.0);
  assert(m_minLabelDistance > 0);
  assert(m_minDistance >= 0);
  // compute m_step (world distance between two adjacent ticks)
  // and m_labelPeriod (number of ticks between two adjacent labels)

  // the distance (world units) between two labels must be >=
  // minLabelWorldDistance
  const double absUnit               = std::abs(m_unit);
  const double minLabelWorldDistance = m_minLabelDistance / absUnit;
  const double minWorldDistance      = m_minDistance / absUnit;

  // we want the minimum step with:
  //    step*labelPeriod (i.e. label distance) >=  minLabelWorldDistance
  //    step (i.e. tick distance) >= minWorldDistance
  // Note: labelPeriod alternates between 5 and 2 => labelPeriod' =
  // 7-labelPeriod
  m_step        = 1;
  m_labelPeriod = 5;
  if (m_step * m_labelPeriod >= minLabelWorldDistance &&
      m_step >= minWorldDistance) {
    while (m_step >= minLabelWorldDistance &&
           m_step / (7 - m_labelPeriod) >= minWorldDistance) {
      m_labelPeriod = 7 - m_labelPeriod;
      m_step /= m_labelPeriod;
    }
  } else {
    do {
      m_step *= m_labelPeriod;
      m_labelPeriod =
          7 - m_labelPeriod;  // m_labelPeriod alternates between 5 and 2
    } while (m_step * m_labelPeriod < minLabelWorldDistance ||
             m_step < minWorldDistance);
  }

  if (m_step >= minLabelWorldDistance) {
    m_labelPeriod = 1;
  }

  if (m_step * m_labelPeriod < m_minStep) {
    m_step        = m_minStep;
    m_labelPeriod = 1;
  } else if (m_step < m_minStep) {
    m_step *= m_labelPeriod;
    m_labelPeriod = 1;
  }

  // compute range
  double v0 = (m_x0 - m_pan) / m_unit;  // left margin (world units)
  double v1 = (m_x1 - m_pan) / m_unit;  // right margin (world units)
  if (m_unit < 0) std::swap(v0, v1);
  int i0 =
      tfloor((v0 - m_vOrigin) / m_step);  // largest tick <=v0 is i0 * m_step
  int i1 =
      tceil((v1 - m_vOrigin) / m_step);  // smallest tick >=v1 is i1 * m_step
  m_minValue  = i0 * m_step + m_vOrigin;
  m_tickCount = i1 - i0 + 1;

  m_labelOffset = i0 >= 0 ? (i0 % m_labelPeriod)
                          : (m_labelPeriod - ((-i0) % m_labelPeriod));
}

//=============================================================================

//=============================================================================
//
// FunctionPanel::Gadget
//
//-----------------------------------------------------------------------------

FunctionPanel::Gadget::Gadget(FunctionPanel::Handle handle, int kIndex,
                              const QPointF &p, int rx, int ry,
                              const QPointF &pointPos)
    : m_handle(handle)
    , m_kIndex(kIndex)
    , m_hitRegion(QRect((int)p.x() - rx, (int)p.y() - ry, 2 * rx, 2 * ry))
    , m_pos(p)
    , m_pointPos(pointPos)
    , m_channel(0)
    , m_keyframePosition(0) {}

//=============================================================================
//
// FunctionPanel
//
//-----------------------------------------------------------------------------

FunctionPanel::FunctionPanel(QWidget *parent, bool isFloating)
    : QDialog(parent)
    , m_functionTreeModel(0)
    , m_viewTransform()
    , m_valueAxisX(50)
    , m_frameAxisY(50)
    , m_graphViewportY(50)
    , m_frameHandle(0)
    , m_xsheetHandle(0)
    , m_dragTool(0)
    , m_currentFrameStatus(0)
    , m_selection(0)
    , m_curveShape(SMOOTH)
    , m_speedGraphVisible(false)
    , m_speedGraphHeight(110)
    , m_isFloating(isFloating) {
  setWindowTitle(tr("Function Curves"));

  m_viewTransform.translate(50, 200);
  m_viewTransform.scale(5, -1);

  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  m_highlighted.handle = None;
  m_highlighted.gIndex = -1;
  m_cursor.visible     = false;
  m_cursor.frame = m_cursor.value = 0;
  m_curveLabel.text               = "";
  m_curveLabel.curve              = 0;

  if (m_isFloating) {
    // load the dialog size
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath("popups.ini"));
    QSettings settings(toQString(fp), QSettings::IniFormat);

    setGeometry(
        settings.value("FunctionCurves", QRect(500, 500, 400, 300)).toRect());
  }
}

//-----------------------------------------------------------------------------

FunctionPanel::~FunctionPanel() {
  if (m_isFloating) {
    // save the dialog size
    TFilePath fp(ToonzFolder::getMyModuleDir() + TFilePath("popups.ini"));
    QSettings settings(toQString(fp), QSettings::IniFormat);

    settings.setValue("FunctionCurves", geometry());
  }

  delete m_dragTool;
}

//-----------------------------------------------------------------------------

double FunctionPanel::getPixelRatio(TDoubleParam *curve) const {
  double framePixelSize = xToFrame(1) - xToFrame(0);
  assert(framePixelSize > 0);
  double valuePixelSize = fabs(yToValue(curve, 1) - yToValue(curve, 0));
  assert(valuePixelSize > 0);
  return framePixelSize / valuePixelSize;
}

//-----------------------------------------------------------------------------

double FunctionPanel::frameToX(double f) const {
  return m_viewTransform.m11() * f + m_viewTransform.dx();
}

//-----------------------------------------------------------------------------

double FunctionPanel::xToFrame(double x) const {
  return (x - m_viewTransform.dx()) / m_viewTransform.m11();
}

//-----------------------------------------------------------------------------

double FunctionPanel::valueToY(TDoubleParam *curve, double v) const {
  const double bigNumber = 1.0e9;
  TMeasure *m            = curve->getMeasure();
  if (m) {
    const TUnit *unit = m->getCurrentUnit();
    v                 = unit->convertTo(v);
  }
  return tcrop(m_viewTransform.m22() * v + m_viewTransform.dy(), -bigNumber,
               bigNumber);
}

//-----------------------------------------------------------------------------

double FunctionPanel::yToValue(TDoubleParam *curve, double y) const {
  double v    = (y - m_viewTransform.dy()) / m_viewTransform.m22();
  TMeasure *m = curve->getMeasure();
  if (m) {
    const TUnit *unit = m->getCurrentUnit();
    v                 = unit->convertFrom(v);
  }
  return v;
}

//-----------------------------------------------------------------------------

void FunctionPanel::pan(int dx, int dy) {
  QTransform m;
  m.translate(dx, dy);
  m_viewTransform *= m;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::zoom(double sx, double sy, const QPoint &center) {
  QTransform m;
  m.translate(center.x(), center.y());
  m.scale(sx, sy);
  m.translate(-center.x(), -center.y());
  m_viewTransform *= m;
  update();
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve, double frame,
                                 double value) const {
  return QPointF(frameToX(frame), valueToY(curve, value));
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve, double frame) const {
  return getWinPos(curve, frame, curve->getValue(frame));
}

//-----------------------------------------------------------------------------

QPointF FunctionPanel::getWinPos(TDoubleParam *curve,
                                 const TDoubleKeyframe &kf) const {
  return getWinPos(curve, kf.m_frame, kf.m_value);
}

//-----------------------------------------------------------------------------

int FunctionPanel::getCurveDistance(TDoubleParam *curve, const QPoint &winPos) {
  double frame  = xToFrame(winPos.x());
  double value  = curve->getValue(frame);
  double curveY = valueToY(curve, value);
  return std::abs(curveY - winPos.y());
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionPanel::findClosestChannel(
    const QPoint &winPos, int maxWinDistance) {
  FunctionTreeModel::Channel *closestChannel = 0;
  int minDistance                            = maxWinDistance;
  int i;
  for (i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    TDoubleParam *curve = channel->getParam();
    int distance        = getCurveDistance(curve, winPos);
    if (distance < minDistance) {
      closestChannel = channel;
      minDistance    = distance;
    }
  }
  return closestChannel;
}

//-----------------------------------------------------------------------------

TDoubleParam *FunctionPanel::findClosestCurve(const QPoint &winPos,
                                              int maxWinDistance) {
  FunctionTreeModel::Channel *closestChannel =
      findClosestChannel(winPos, maxWinDistance);
  return closestChannel ? closestChannel->getParam() : 0;
}

//-----------------------------------------------------------------------------

// return the gadget index (-1 if no gadget is close enough)
QVector<qreal> FunctionPanel::columnDashPattern(
    FunctionTreeModel::Channel *channel) const {
  if (!channel || !m_functionTreeModel) return QVector<qreal>();

  TreeModel::Item *column =
      FunctionTreeModel::columnScopeOf(channel->getChannelGroup());
  if (!column) return QVector<qreal>();

  // The CURRENT column is drawn solid throughout. Seeing at a glance which
  // curves belong to the column being worked on matters more than telling the
  // other columns apart from one another -- and it makes the rule sayable
  // without an exception: colour is the channel, dash is a column that is not
  // the current one, weight is the current curve.
  // The stage object comes FIRST, because it is the one that follows the
  // xsheet: picking a column there is how you say which column you are working
  // on. The current channel only moves when a curve is clicked in the tree or
  // the graph, so reading it alone left the column just picked dashed.
  TreeModel::Item *currentColumn = 0;
  if (TStageObject *currentObject = m_functionTreeModel->getCurrentStageObject())
    currentColumn = m_functionTreeModel->getStageObjectChannelGroup(currentObject);
  if (!currentColumn)
    if (FunctionTreeModel::Channel *current =
            m_functionTreeModel->getCurrentChannel())
      currentColumn =
          FunctionTreeModel::columnScopeOf(current->getChannelGroup());
  if (currentColumn && column == currentColumn) return QVector<qreal>();

  // Position among the OTHER columns currently drawn, so that with two of them
  // on screen they take the two most distinct patterns whichever columns they
  // are. Solid is not among the choices: it belongs to the current column.
  int index = 0;
  for (int c = 0; c < m_functionTreeModel->getActiveChannelCount(); c++) {
    FunctionTreeModel::Channel *other = m_functionTreeModel->getActiveChannel(c);
    if (!other) continue;
    TreeModel::Item *otherColumn =
        FunctionTreeModel::columnScopeOf(other->getChannelGroup());
    if (!otherColumn || otherColumn == column) break;
    if (otherColumn == currentColumn) continue;  // takes no pattern

    bool alreadySeen = false;
    for (int p = 0; p < c; p++) {
      FunctionTreeModel::Channel *prev =
          m_functionTreeModel->getActiveChannel(p);
      if (prev && FunctionTreeModel::columnScopeOf(prev->getChannelGroup()) ==
                      otherColumn) {
        alreadySeen = true;
        break;
      }
    }
    if (!alreadySeen) index++;
  }

  static const QVector<qreal> patterns[] = {
      QVector<qreal>() << 6 << 3,                        // dashed
      QVector<qreal>() << 1 << 3,                        // dotted
      QVector<qreal>() << 6 << 3 << 1 << 3,              // dash-dot
      QVector<qreal>() << 6 << 3 << 1 << 3 << 1 << 3};   // dash-dot-dot
  const int patternCount = (int)(sizeof(patterns) / sizeof(patterns[0]));
  return patterns[index % patternCount];
}

//-----------------------------------------------------------------------------

FunctionTreeModel::Channel *FunctionPanel::findChannelWithKeyframeAt(
    const QPoint &winPos, int maxDistance) {
  if (!m_functionTreeModel) return 0;

  FunctionTreeModel::Channel *best = 0;
  double bestDistance              = maxDistance;

  for (int c = 0; c < m_functionTreeModel->getActiveChannelCount(); c++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(c);
    TDoubleParam *curve = channel ? channel->getParam() : 0;
    if (!curve) continue;

    for (int i = 0; i < curve->getKeyframeCount(); i++) {
      const QPointF p = getWinPos(curve, curve->getKeyframe(i));
      const double d  = (p - QPointF(winPos)).manhattanLength();
      if (d < bestDistance) {
        bestDistance = d;
        best         = channel;
      }
    }
  }
  return best;
}

//-----------------------------------------------------------------------------

int FunctionPanel::findClosestGadget(const QPoint &winPos, Handle &handle,
                                     int maxDistance) {
  // search only handles close enough (i.e. distance<maxDistance)
  int minDistance = maxDistance;
  int k           = -1;
  for (int i = 0; i < m_gadgets.size(); i++) {
    if (m_gadgets[i].m_hitRegion.contains(winPos)) {
      QPoint p;
      double d = (m_gadgets[i].m_hitRegion.center() - winPos).manhattanLength();
      if (d < minDistance) {
        k           = i;
        minDistance = d;
      }
    }
  }
  if (k >= 0) {
    handle = m_gadgets[k].m_handle;
    return k;  // m_gadgets[k].m_kIndex;
  } else {
    handle = None;
    return -1;
  }
}

//-----------------------------------------------------------------------------

TDoubleParam *FunctionPanel::getCurrentCurve() const {
  FunctionTreeModel::Channel *currentChannel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!currentChannel)
    return 0;
  else
    return currentChannel->getParam();
}

//-----------------------------------------------------------------------------

QPainterPath FunctionPanel::getSegmentPainterPath(TDoubleParam *curve,
                                                  int segmentIndex, int x0,
                                                  int x1) {
  double frame0 = xToFrame(x0), frame1 = xToFrame(x1);
  int kCount = curve->getKeyframeCount();
  int step   = 1;
  if (kCount > 0) {
    if (segmentIndex < 0)
      frame1 = std::min(
          frame1, curve->keyframeIndexToFrame(0));  // before first keyframe
    else if (segmentIndex >= kCount - 1)
      frame0 = std::max(frame0, curve->keyframeIndexToFrame(
                                    kCount - 1));  // after last keyframe
    else {
      // between keyframes
      TDoubleKeyframe kf = curve->getKeyframe(segmentIndex);
      frame0             = std::max(frame0, kf.m_frame);
      double f           = curve->keyframeIndexToFrame(segmentIndex + 1);
      frame1             = std::min(frame1, f);
      step               = kf.m_step;
    }
  }
  if (frame0 >= frame1) return QPainterPath();
  double frame;
  double df = xToFrame(3) - xToFrame(0);

  if (m_curveShape == SMOOTH) {
    frame = frame0;
  } else  // FRAME_BASED
  {
    frame = (double)tfloor(frame0);
    df    = std::max(df, 1.0);
  }

  QPainterPath path;
  if (0 <= segmentIndex && segmentIndex < kCount && step > 1) {
    // step>1
    path.moveTo(getWinPos(curve, frame));

    int f0        = curve->keyframeIndexToFrame(segmentIndex);
    int vFrame    = f0 + tfloor(tfloor(frame - f0), step);
    double vValue = curve->getValue(vFrame);
    assert(vFrame <= frame);
    assert(vFrame + step > frame);
    while (vFrame + step < frame1) {
      vValue = curve->getValue(vFrame);
      path.lineTo(getWinPos(curve, vFrame, vValue));
      vFrame += step;
      path.lineTo(getWinPos(curve, vFrame, vValue));
      vValue = curve->getValue(vFrame);
      path.lineTo(getWinPos(curve, vFrame, vValue));
    }
    path.lineTo(getWinPos(curve, frame1, vValue));
    path.lineTo(getWinPos(curve, frame1, curve->getValue(frame1, true)));
  } else {
    // step = 1
    path.moveTo(getWinPos(curve, frame));
    while (frame + df < frame1) {
      frame += df;
      path.lineTo(getWinPos(curve, frame));
    }
    path.lineTo(getWinPos(curve, frame1, curve->getValue(frame1, true)));
  }
  return path;
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawCurrentFrame(QPainter &painter) {
  int currentframe = 0;
  if (m_frameHandle) currentframe = m_frameHandle->getFrame();
  int x = frameToX(currentframe);
  if (m_currentFrameStatus == 0)
    painter.setPen(Qt::magenta);
  else if (m_currentFrameStatus == 1)
    painter.setPen(Qt::white);
  else
    painter.setPen(m_selectedColor);
  int y = m_graphViewportY + 1;
  painter.drawLine(x - 1, y, x - 1, height());
  painter.drawLine(x + 1, y, x + 1, height());
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawFrameGrid(QPainter &painter) {
  QFontMetrics fm(painter.font());

  // ruler background
  painter.setPen(Qt::NoPen);
  painter.setBrush(getRulerBackground());
  painter.drawRect(0, 0, width(), m_frameAxisY);

  // draw ticks and labels
  Ruler ruler;
  ruler.setTransform(m_viewTransform.m11(), m_viewTransform.dx(), -1);
  ruler.setRange(m_valueAxisX, width());
  ruler.setMinLabelDistance(fm.horizontalAdvance("-8888") + 2);
  ruler.setMinDistance(5);
  ruler.setMinStep(1);
  ruler.compute();
  for (int i = 0; i < ruler.getTickCount(); i++) {
    double f     = ruler.getTick(i);
    bool isLabel = ruler.isLabel(i);
    int x        = frameToX(f);
    painter.setPen(m_textColor);
    int y = m_frameAxisY;
    painter.drawLine(x, y - (isLabel ? 4 : 2), x, y);
    painter.setPen(getFrameLineColor());
    painter.drawLine(x, m_graphViewportY, x, height());
    if (isLabel) {
      painter.setPen(m_textColor);
      QString labelText = QString::number(f + 1);
      painter.drawText(x - fm.horizontalAdvance(labelText) / 2, y - 6,
                       labelText);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawValueGrid(QPainter &painter) {
  TDoubleParam *curve = getCurrentCurve();
  if (!curve) return;

  QFontMetrics fm(painter.font());

  // ruler background
  painter.setPen(Qt::NoPen);
  painter.setBrush(getRulerBackground());
  painter.drawRect(0, 0, m_valueAxisX, height());

  Ruler ruler;
  ruler.setTransform(m_viewTransform.m22(), m_viewTransform.dy());
  ruler.setRange(m_graphViewportY, height());
  ruler.setMinLabelDistance(fm.height() + 2);
  ruler.setMinDistance(5);
  ruler.compute();

  painter.setBrush(Qt::NoBrush);
  for (int i = 0; i < ruler.getTickCount(); i++) {
    double v     = ruler.getTick(i);
    bool isLabel = ruler.isLabel(i);
    int y        = tround(m_viewTransform.m22() * v +
                          m_viewTransform.dy());  // valueToY(curve, v);
    painter.setPen(m_textColor);
    int x = m_valueAxisX;
    painter.drawLine(x - (isLabel ? 5 : 2), y, x, y);

    painter.setPen(getValueLineColor());
    painter.drawLine(x, y, width(), y);

    if (isLabel) {
      painter.setPen(m_textColor);
      QString labelText = QString::number(v);
      painter.drawText(std::max(0, x - 5 - fm.horizontalAdvance(labelText)),
                       y + fm.height() / 2, labelText);
    }
  }
  if (false && ruler.getTickCount() > 10) {
    double value = ruler.getTick(9);
    double dv    = fabs(ruler.getTick(10));
    double frame = 10;
    double df    = dv * getPixelRatio(curve);
    QPointF p0   = getWinPos(curve, frame, value);
    QPointF p1   = getWinPos(curve, frame + df, value + dv);

    painter.setPen(Qt::magenta);
    painter.drawRect(p0.x(), p0.y(), (p1 - p0).x(), (p1 - p0).y());
  }
}

//-----------------------------------------------------------------------------

namespace {

//! A channel that drives nothing is drawn faded, the same thing the tree says
//! about it in the list. Kept a little stronger than the tree's text: a
//! hairline curve vanishes at an alpha a glyph still survives.
QColor fadeIfInert(QColor color, const FunctionTreeModel::Channel *channel) {
  if (channel && channel->isInert()) color.setAlpha(100);
  return color;
}

}  // namespace

//-----------------------------------------------------------------------------

void FunctionPanel::drawOtherCurves(QPainter &painter) {
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setBrush(Qt::NoBrush);
  int x0 = m_valueAxisX;
  int x1 = width();

  QPen solidPen;
  QPen dashedPen;
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);

  for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    if (channel->isCurrent()) continue;
    TDoubleParam *curve = channel->getParam();
    QColor color =
        curve == m_curveLabel.curve ? m_selectedColor : getOtherCurvesColor();
    solidPen.setColor(getChannelColor(channel, false));
    dashedPen.setColor(getChannelColor(channel, false));

    // Colour says WHICH channel, dash pattern says WHICH COLUMN. The palette
    // is per channel name -- every X is firebrick -- so with two columns
    // overlaid their X curves were the same line twice. The pattern comes from
    // the owning group, so a column's curves all share one and the eye can
    // follow a column across the graph.
    solidPen.setStyle(Qt::SolidLine);
    solidPen.setDashPattern(QVector<qreal>());
    if (const QVector<qreal> pattern = columnDashPattern(channel);
        !pattern.isEmpty())
      solidPen.setDashPattern(pattern);

    painter.setBrush(Qt::NoBrush);

    int kCount = curve->getKeyframeCount();
    if (kCount == 0) {
      // no control points
      painter.setPen(dashedPen);
      painter.drawPath(getSegmentPainterPath(curve, 0, x0, x1));
    }
    // draw control points and handles
    else {
      for (int k = -1; k < kCount; k++) {
        if (k < 0 || k >= kCount - 1)
          painter.setPen(dashedPen);
        else {
          // A picked segment is drawn heavy on the other curves too, or a
          // selection spanning several curves would only be visible on the
          // current one -- and you could not tell what the interpolation
          // command is about to hit.
          solidPen.setWidth(getSelection()->isSegmentSelected(curve, k) ? 3 : 0);
          painter.setPen(solidPen);
        }
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      }
      painter.setPen(m_textColor);
      painter.setBrush(m_subColor);
      for (int k = 0; k < kCount; k++) {
        double frame = curve->keyframeIndexToFrame(k);
        QPointF p    = getWinPos(curve, frame, curve->getValue(frame));

        // Selected keys are marked HERE too. Only the current curve drew its
        // keys through the gadgets, which carry the selection colour, so keys
        // picked on any other curve stayed plain white -- a selection that was
        // being made and could not be seen, which reads exactly like
        // multi-selection not working at all.
        const bool isSelected = getSelection()->isSelected(curve, k);
        painter.setBrush(isSelected ? QColor(255, 126, 0) : m_subColor);

        painter.drawRect(p.x() - 2, p.y() - 2, 5, 5);
        QPointF p2 = getWinPos(curve, frame, curve->getValue(frame, true));
        if (p2.y() != p.y()) {
          painter.drawRect(p2.x() - 2, p2.y() - 2, 5, 5);
          painter.setPen(solidPen);
          painter.drawLine(p, p2);
          painter.setPen(m_textColor);
        }
      }
      painter.setBrush(m_subColor);
    }
  }
  painter.setBrush(Qt::NoBrush);
  painter.setPen(m_textColor);
  painter.setRenderHint(QPainter::Antialiasing, false);
}

//-----------------------------------------------------------------------------

void FunctionPanel::updateGadgets(TDoubleParam *curve) {
  m_gadgets.clear();

  TDoubleKeyframe oldKf;
  oldKf.m_type = TDoubleKeyframe::None;

  int keyframeCount = curve->getKeyframeCount();

  for (int i = 0; i != keyframeCount; ++i) {
    const int pointHitRadius = 10, handleHitRadius = 6;

    TDoubleKeyframe kf = curve->getKeyframe(i);
    kf.m_value         = curve->getValue(
        kf.m_frame);  // Some keyframe values do NOT correspond to the
                      // actual displayed curve value (eg with expressions)
    // Build keyframe positions
    QPointF p     = getWinPos(curve, kf.m_frame);
    QPointF pLeft = p;

    if (i == keyframeCount - 1 &&
        curve
            ->isCycleEnabled())  // This is probably OBSOLETE. I don't think the
      p = getWinPos(curve, kf.m_frame,
                    curve->getValue(
                        kf.m_frame,
                        true));  // GUI allows cycling single curves nowadays...
                                 // However, is the assignment correct?
    // Add keyframe gadget(s)
    m_gadgets.push_back(Gadget(Point, i, p, pointHitRadius, pointHitRadius));

    TPointD currentPointRight(kf.m_frame, kf.m_value);
    TPointD currentPointLeft(currentPointRight);

    // If the previous segment or the current segment are not keyframe based,
    // the curve can have two different values in kf.m_frame
    if (i > 0 &&
        (!TDoubleKeyframe::isKeyframeBased(
             kf.m_type) ||  // Keyframe-based are the above mentioned curves
         !TDoubleKeyframe::isKeyframeBased(
             curve->getKeyframe(i - 1)
                 .m_type)))  // where values stored in keyframes are not used
    {                        // to calculate the actual curve values.
      currentPointLeft.y = curve->getValue(kf.m_frame, true);
      pLeft              = getWinPos(curve, currentPointLeft);
      m_gadgets.push_back(
          Gadget(Point, i, pLeft, pointHitRadius, pointHitRadius));
    }

    // Add handle gadgets (eg the speed or ease handles)
    if (getSelection()->isSelected(curve, i)) {
      // Left handle
      switch (oldKf.m_type) {
      case TDoubleKeyframe::SpeedInOut: {
        TPointD speedIn = curve->getSpeedIn(i);
        if (norm2(speedIn) > 0) {
          QPointF q = getWinPos(curve, currentPointLeft + speedIn);
          m_gadgets.push_back(
              Gadget(SpeedIn, i, q, handleHitRadius, handleHitRadius, pLeft));
        }
        break;
      }

      case TDoubleKeyframe::EaseInOut: {
        QPointF q = getWinPos(curve, kf.m_frame + kf.m_speedIn.x);
        m_gadgets.push_back(Gadget(EaseIn, i, q, 6, 15));
        break;
      }

      case TDoubleKeyframe::EaseInOutPercentage: {
        double easeIn = kf.m_speedIn.x * (kf.m_frame - oldKf.m_frame) * 0.01;
        QPointF q     = getWinPos(curve, kf.m_frame + easeIn);
        m_gadgets.push_back(Gadget(EaseInPercentage, i, q, 6, 15));
        break;
      }
      default:
        break;
      }

      // Right handle
      if (i != keyframeCount - 1) {
        switch (kf.m_type) {
        case TDoubleKeyframe::SpeedInOut: {
          TPointD speedOut = curve->getSpeedOut(i);
          if (norm2(speedOut) > 0) {
            QPointF q = getWinPos(curve, currentPointRight + speedOut);
            m_gadgets.push_back(
                Gadget(SpeedOut, i, q, handleHitRadius, handleHitRadius, p));
          }
          break;
        }

        case TDoubleKeyframe::EaseInOut: {
          QPointF q = getWinPos(curve, kf.m_frame + kf.m_speedOut.x);
          m_gadgets.push_back(Gadget(EaseOut, i, q, 6, 15));
          break;
        }

        case TDoubleKeyframe::EaseInOutPercentage: {
          double segmentWidth = curve->keyframeIndexToFrame(i + 1) - kf.m_frame;
          double easeOut      = segmentWidth * kf.m_speedOut.x * 0.01;

          QPointF q = getWinPos(curve, kf.m_frame + easeOut);
          m_gadgets.push_back(Gadget(EaseOutPercentage, i, q, 6, 15));
          break;
        }
        default:
          break;
        }
      }
    }

    oldKf = kf;
  }

  // Add group gadgets (ie those that can be added when multiple channels share
  // the same keyframe data)
  int channelCount = m_functionTreeModel->getActiveChannelCount();

  // Using a map of vectors. Yes, really. The *ideal* way would be that of
  // copying the first keyframes
  // vector, and then comparing it with the others from each channel - keeping
  // the common data only...

  typedef std::map<double, std::vector<TDoubleKeyframe>>
      KeyframeTable;  // frame -> { keyframes }
  KeyframeTable keyframes;

  for (int i = 0; i != channelCount; ++i) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    if (!channel) continue;

    TDoubleParam *curve = channel->getParam();
    for (int j = 0; j != curve->getKeyframeCount(); ++j) {
      TDoubleKeyframe kf = curve->getKeyframe(
          j);  // Well... this stuff gets called upon *painting*  o_o'
      keyframes[kf.m_frame].push_back(
          kf);  // It's bound to be slow. Do we really need it?
    }
  }

  int groupHandleY = m_graphViewportY - 6;

  KeyframeTable::iterator it, iEnd(keyframes.end()),
      iLast(keyframes.empty() ? iEnd : --iEnd);
  for (KeyframeTable::iterator it = keyframes.begin(); it != keyframes.end();
       ++it) {
    assert(!it->second.empty());

    double frame = it->first;  // redundant, already in the key... oh well
    QPointF p(frameToX(frame), groupHandleY);

    Gadget gadget((FunctionPanel::Handle)100, -1, p, 6,
                  6);  // No idea what the '100' type value mean...
    gadget.m_keyframePosition = frame;

    m_gadgets.push_back(gadget);

    TDoubleKeyframe kf = it->second[0];

    if ((int)it->second.size() < channelCount) continue;

    // All channels had this keyframe - so, add further gadgets about stuff...

    for (int i = 1; i < channelCount; ++i) {
      // Find out if keyframes data differs
      const TDoubleKeyframe &kf2 = it->second[i];

      if (kf.m_type != kf2.m_type || kf.m_speedOut.x != kf2.m_speedOut.x)
        kf.m_type = TDoubleKeyframe::None;
      if (kf.m_prevType != kf2.m_prevType || kf.m_speedIn.x != kf2.m_speedIn.x)
        kf.m_prevType = TDoubleKeyframe::None;
    }

    // NOTE: EaseInOutPercentage are currently NOT SUPPORTED - they would be
    // harder to code and
    //       controversial, since the handle position depends on the *segment
    //       size* too.
    //       So, keyframe data could be shared, but adjacent segment lengths
    //       could not...

    if (it != iLast &&
        (kf.m_type == TDoubleKeyframe::SpeedInOut ||
         kf.m_type == TDoubleKeyframe::EaseInOut) &&
        kf.m_speedOut.x != 0) {
      QPointF p(frameToX(frame + kf.m_speedOut.x), groupHandleY);
      Gadget gadget((FunctionPanel::Handle)101, -1, p, 6, 15);  // type value...
      gadget.m_keyframePosition = frame;
      m_gadgets.push_back(gadget);
    }

    if ((kf.m_prevType == TDoubleKeyframe::SpeedInOut ||
         kf.m_prevType == TDoubleKeyframe::EaseInOut) &&
        kf.m_speedIn.x != 0) {
      QPointF p(frameToX(frame + kf.m_speedIn.x), groupHandleY);
      Gadget gadget((FunctionPanel::Handle)102, -1, p, 6, 15);  // type value...
      gadget.m_keyframePosition = frame;
      m_gadgets.push_back(gadget);
    }
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawCurrentCurve(QPainter &painter) {
  FunctionTreeModel::Channel *channel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!channel) return;
  TDoubleParam *curve = channel->getParam();

  painter.setRenderHint(QPainter::Antialiasing, true);
  QColor color = Qt::red;
  color        = getChannelColor(channel, true);
  QPen solidPen(color);
  QPen dashedPen(color);
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);
  painter.setBrush(Qt::NoBrush);

  // The current curve carries its COLUMN's pattern like any other. Drawing it
  // solid whatever column it belonged to broke the rule exactly where it is
  // read most: with a curve of column 2 current and column 3 picked in the
  // xsheet, two different columns appeared solid at once. Weight still marks
  // it as the current one -- that is what weight is for.
  const QVector<qreal> columnDashes = columnDashPattern(channel);

  int x0 = m_valueAxisX;
  int x1 = width();

  // draw curve
  int kCount = curve->getKeyframeCount();
  if (kCount == 0) {
    // no control points
    painter.setPen(dashedPen);
    painter.drawPath(getSegmentPainterPath(curve, 0, x0, x1));
  } else {
    for (int k = -1; k < kCount; k++) {
      if (k < 0 || k >= kCount - 1) {
        painter.setPen(dashedPen);
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      } else {
        TDoubleKeyframe::Type segmentType = curve->getKeyframe(k).m_type;
        QColor color                      = Qt::red;
        if (segmentType == TDoubleKeyframe::Expression ||
            segmentType == TDoubleKeyframe::SimilarShape ||
            segmentType == TDoubleKeyframe::File)
          color = QColor(185, 0, 0);
        color   = getChannelColor(channel, true);
        // The current curve is drawn heavier than the others (which stay at
        // hairline width in drawOtherCurves): with several columns overlaid
        // the colours repeat -- every X is firebrick -- and thickness is what
        // says which one the handles belong to.
        if (getSelection()->isSegmentSelected(curve, k))
          solidPen.setWidth(3);
        else
          solidPen.setWidth(2);
        solidPen.setColor(color);
        if (columnDashes.isEmpty())
          solidPen.setStyle(Qt::SolidLine);
        else
          solidPen.setDashPattern(columnDashes);
        painter.setPen(solidPen);
        painter.drawPath(getSegmentPainterPath(curve, k, x0, x1));
      }
    }
  }
  painter.setPen(QPen(m_textColor, 0));

  // draw control points
  updateGadgets(curve);
  painter.setPen(m_textColor);
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    if (g.m_handle == SpeedIn || g.m_handle == SpeedOut)
      painter.drawLine(g.m_pointPos, g.m_pos);
  }
  solidPen.setWidth(0);
  solidPen.setColor(Qt::red);
  painter.setPen(solidPen);
  for (int j = 0; j < (int)m_gadgets.size() - 1; j++)
    if (m_gadgets[j].m_handle == Point && m_gadgets[j + 1].m_handle &&
        m_gadgets[j + 1].m_handle != 100 &&
        m_gadgets[j].m_pos.x() == m_gadgets[j + 1].m_pos.x())
      painter.drawLine(m_gadgets[j].m_pos, m_gadgets[j + 1].m_pos);

  painter.setRenderHint(QPainter::Antialiasing, false);
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    int i           = g.m_kIndex;
    int r           = 1;
    QPointF p       = g.m_pos;
    double easeDx = 0, easeHeight = 15, easeTick = 2;
    bool isSelected = getSelection()->isSelected(curve, i);
    bool isHighlighted =
        m_highlighted.handle == g.m_handle && m_highlighted.gIndex == j;
    switch (g.m_handle) {
    case Point:
      painter.setBrush(isSelected ? QColor(255, 126, 0) : m_subColor);
      painter.setPen(m_textColor);
      r = isHighlighted ? 4 : 3;
      drawSquare(painter, p, r);
      break;

    case SpeedIn:
    case SpeedOut:
      painter.setBrush(m_subColor);
      painter.setPen(m_textColor);
      r = isHighlighted ? 4 : 3;
      drawCircle(painter, p, r);
      break;

    case EaseIn:
    case EaseOut:
    case EaseInPercentage:
    case EaseOutPercentage:
      painter.setBrush(Qt::NoBrush);
      painter.setPen(isHighlighted ? QColor(255, 126, 0) : m_textColor);
      painter.drawLine(p.x(), p.y() - easeHeight, p.x(), p.y() + easeHeight);
      if (g.m_handle == EaseIn || g.m_handle == EaseInPercentage)
        easeDx = easeTick;
      else
        easeDx = -easeTick;
      painter.drawLine(p.x(), p.y() - easeHeight, p.x() + easeDx,
                       p.y() - easeHeight - easeTick);
      painter.drawLine(p.x(), p.y() + easeHeight, p.x() + easeDx,
                       p.y() + easeHeight + easeTick);
      break;

    default:
      break;
    }
  }

  painter.setRenderHint(QPainter::Antialiasing, false);
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawGroupKeyframes(QPainter &painter) {
  FunctionTreeModel::Channel *channel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
  if (!channel) return;
  QColor color = Qt::red;
  QPen solidPen(color);
  QPen dashedPen(color);
  QVector<qreal> dashes;
  dashes << 4 << 4;
  dashedPen.setDashPattern(dashes);
  painter.setBrush(Qt::NoBrush);

  int x0 = m_valueAxisX;
  int x1 = width();

  solidPen.setWidth(0);
  solidPen.setColor(Qt::red);
  painter.setPen(solidPen);

  std::vector<double> keyframes;
  int y = 0;
  for (int j = 0; j < (int)m_gadgets.size(); j++) {
    const Gadget &g = m_gadgets[j];
    int i           = g.m_kIndex;
    int r           = 1;
    QPointF p       = g.m_pos;
    double easeDx = 0, easeHeight = 15, easeTick = 2;
    bool isSelected = false;  // getSelection()->isSelected(curve, i);
    bool isHighlighted =
        m_highlighted.handle == g.m_handle && m_highlighted.gIndex == j;
    painter.setBrush(isSelected ? QColor(255, 126, 0) : m_subColor);
    painter.setPen(m_textColor);
    r = isHighlighted ? 3 : 2;
    QPainterPath pp;
    int d = 2;
    int h = 4;
    switch (g.m_handle) {
    case 100:
      drawSquare(painter, p, r);
      y = p.y();
      keyframes.push_back(p.x());
      break;
    case 101:
      d = -d;
    // Note: NO break!
    case 102:
      painter.setBrush(Qt::NoBrush);
      painter.setPen(isHighlighted ? QColor(255, 126, 0) : m_textColor);
      pp.moveTo(p + QPointF(d, -h));
      pp.lineTo(p + QPointF(0, -h));
      pp.lineTo(p + QPointF(0, h));
      pp.lineTo(p + QPointF(d, h));
      painter.drawPath(pp);
      break;
    default:
      break;
    }
  }
  painter.setPen(m_textColor);
  for (int i = 0; i + 1 < (int)keyframes.size(); i++) {
    painter.drawLine(keyframes[i] + 3, y, keyframes[i + 1] - 3, y);
  }
}

//-----------------------------------------------------------------------------

//=============================================================================
//    Link Curves
//-----------------------------------------------------------------------------

namespace {

//! Which columns should follow the selection, and how.
//!
//! The flow starts from the DRIVING curves -- the ones already selected in the
//! graph -- because those always exist. Starting from the driven curve was the
//! first design and it could not work: a curve with no keyframes is not drawn,
//! so there was nothing to select, and the command asked the animator to create
//! by hand the very thing it was meant to create.
//!
//! Channels pair up BY NAME (x with x, rot with rot), so selecting x and y of
//! one column and ticking another links both at once -- which is what "make
//! this follow that" means to an animator. Delay, multiplier and offset apply
//! to every channel involved: a tail that lags by six frames lags on all axes.
class LinkCurvesDialog final : public QDialog {
public:
  QListWidget *m_targetList;
  DVGui::IntLineEdit *m_delayFld;
  DVGui::DoubleLineEdit *m_multFld, *m_offsetFld;

  LinkCurvesDialog(QWidget *parent, const QString &sourceDesc,
                   const QStringList &groupNames)
      : QDialog(parent) {
    setWindowTitle(tr("Link Curves"));

    QLabel *src = new QLabel(tr("Driven by: %1").arg(sourceDesc), this);
    src->setWordWrap(true);

    m_targetList = new QListWidget(this);
    for (const QString &n : groupNames) {
      QListWidgetItem *it = new QListWidgetItem(n, m_targetList);
      it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
      it->setCheckState(Qt::Unchecked);
    }
    m_targetList->setMinimumHeight(160);

    m_delayFld  = new DVGui::IntLineEdit(this, 0, -9999, 9999);
    m_multFld   = new DVGui::DoubleLineEdit(this, 1.0);
    m_offsetFld = new DVGui::DoubleLineEdit(this, 0.0);

    QGridLayout *lay = new QGridLayout();
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);
    lay->addWidget(src, 0, 0, 1, 2);
    lay->addWidget(new QLabel(tr("Columns to drive:"), this), 1, 0, 1, 2);
    lay->addWidget(m_targetList, 2, 0, 1, 2);
    lay->addWidget(new QLabel(tr("Delay (frames):"), this), 3, 0);
    lay->addWidget(m_delayFld, 3, 1);
    lay->addWidget(new QLabel(tr("Multiply by:"), this), 4, 0);
    lay->addWidget(m_multFld, 4, 1);
    lay->addWidget(new QLabel(tr("Add:"), this), 5, 0);
    lay->addWidget(m_offsetFld, 5, 1);

    QDialogButtonBox *bb =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb, 6, 0, 1, 2);
    setLayout(lay);
  }
};

//! The channel part of a reference: "col1.x" -> "x", "fx.blur1.value" ->
//! "value". Pairing targets to sources by this is what makes x follow x.
QString channelKeyOf(const QString &exprRefName) {
  const int dot = exprRefName.lastIndexOf('.');
  return (dot < 0) ? exprRefName : exprRefName.mid(dot + 1);
}

}  // namespace

//-----------------------------------------------------------------------------

void FunctionPanel::linkSelectedCurves() {
  if (!m_functionTreeModel || !getSelection()) return;

  // The driving curves: the selection, or the current curve when nothing is
  // selected.
  QList<TDoubleParam *> sources = getSelection()->getSelectedCurves();
  if (sources.isEmpty()) {
    if (TDoubleParam *cur = getCurrentCurve()) sources.append(cur);
  }
  if (sources.isEmpty()) {
    DVGui::warning(tr("Select the curve (or curves) that should DRIVE, then "
                      "choose which columns follow it."));
    return;
  }

  // Their reference names, matched by pointer among the shown channels.
  QMap<TDoubleParam *, QString> sourceRef;
  QStringList sourceDesc;
  for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *ch = m_functionTreeModel->getActiveChannel(i);
    if (!ch || !ch->getParam()) continue;
    if (!sources.contains(ch->getParam())) continue;
    sourceRef[ch->getParam()] = ch->getExprRefName();
    sourceDesc.append(ch->getExprRefName());
  }
  if (sourceRef.isEmpty()) {
    DVGui::warning(tr("The driving curves must be shown in the graph."));
    return;
  }

  // Candidate targets: the stage object groups, whether animated or not -- the
  // whole point is to drive channels that have no curve yet.
  QList<FunctionTreeModel::ChannelGroup *> groups;
  QStringList groupNames;
  for (int i = 0; i < m_functionTreeModel->getStageObjectsChannelCount(); i++) {
    FunctionTreeModel::ChannelGroup *g =
        m_functionTreeModel->getStageObjectChannel(i);
    if (!g) continue;
    groups.append(g);
    groupNames.append(g->getIdName());
  }
  if (groups.isEmpty()) {
    DVGui::warning(tr("There are no columns to drive."));
    return;
  }

  LinkCurvesDialog dlg(this, sourceDesc.join(", "), groupNames);
  if (dlg.exec() != QDialog::Accepted) return;

  const int delay     = dlg.m_delayFld->getValue();
  const double mult   = dlg.m_multFld->getValue();
  const double offset = dlg.m_offsetFld->getValue();

  int linked = 0, refused = 0;
  TUndoManager::manager()->beginBlock();

  for (int gi = 0; gi < groups.size(); gi++) {
    if (dlg.m_targetList->item(gi)->checkState() != Qt::Checked) continue;
    FunctionTreeModel::ChannelGroup *group = groups[gi];

    for (QMap<TDoubleParam *, QString>::const_iterator it = sourceRef.begin();
         it != sourceRef.end(); ++it) {
      TDoubleParam *source  = it.key();
      const QString refName = it.value();
      const QString key     = channelKeyOf(refName);

      // The channel of the same name inside the target column.
      TDoubleParam *target = 0;
      for (int c = 0; c < group->getChildCount(); c++) {
        FunctionTreeModel::Channel *ch =
            dynamic_cast<FunctionTreeModel::Channel *>(group->getChild(c));
        if (!ch || !ch->getParam()) continue;
        if (channelKeyOf(ch->getExprRefName()) == key) {
          target = ch->getParam();
          break;
        }
      }
      if (!target || target == source) continue;

      // Where the link applies: the SELECTION when several keys are picked --
      // linking a stretch and hand-animating the rest is a legitimate thing to
      // want -- otherwise the whole extent of the driving curve.
      int f0 = 0, f1 = 0;
      QList<int> sel = getSelection()->getSelectedKeyIndices(source);
      if (sel.size() >= 2) {
        std::sort(sel.begin(), sel.end());
        f0 = (int)source->keyframeIndexToFrame(sel.first());
        f1 = (int)source->keyframeIndexToFrame(sel.last());
      } else if (source->getKeyframeCount() >= 2) {
        f0 = (int)source->keyframeIndexToFrame(0);
        f1 = (int)source->keyframeIndexToFrame(
            source->getKeyframeCount() - 1);
      } else
        continue;
      if (delay > 0) f1 += delay;
      if (f1 <= f0) f1 = f0 + 1;

      // Compose the expression. The frame inside a reference is 1-based, as in
      // the interface: ParamCalculatorNode subtracts the 1 itself, so writing a
      // 0-based frame here would be a silent one-frame slip.
      QString expr = refName;
      if (delay != 0)
        expr += QString("(frame %1 %2)")
                    .arg(delay > 0 ? "-" : "+")
                    .arg(std::abs(delay));
      if (mult != 1.0) expr += QString(" * %1").arg(mult);
      if (offset != 0.0)
        expr +=
            QString(" %1 %2").arg(offset > 0 ? "+" : "-").arg(std::abs(offset));

      // Refuse a cycle rather than write it: A driven by B driven by A would
      // spin forever at evaluation time, and the dialog would have been a
      // convenient way to hang the application.
      TExpression check;
      check.setGrammar(target->getGrammar());
      check.setText(expr.toStdString());
      if (dependsOn(check, target)) {
        refused++;
        continue;
      }

      // BOTH keys first, the expression LAST. createKeyframe() retypes segment
      // 0 with the default interpolation the moment a curve reaches two
      // keyframes -- writing the expression before that second key silently
      // threw it away. One setter per creation, too: createKeyframe is
      // documented to be called with no other keyframe selected on the setter.
      { KeyframeSetter s(target, m_xsheetHandle); s.createKeyframe(f0); }
      { KeyframeSetter s(target, m_xsheetHandle); s.createKeyframe(f1); }

      const int k = target->getClosestKeyframe(f0);
      if (k < 0 || target->keyframeIndexToFrame(k) != f0) continue;
      KeyframeSetter setter(target, m_xsheetHandle, k);
      setter.setExpression(expr.toStdString());
      linked++;
    }
  }
  TUndoManager::manager()->endBlock();

  if (refused > 0)
    DVGui::warning(
        tr("%1 channel(s) were left alone: linking them would have created a "
           "circular reference.")
            .arg(refused));
  else if (linked == 0)
    DVGui::warning(tr("Nothing was linked: the chosen columns have no channel "
                      "matching the selected ones."));

  if (m_xsheetHandle) m_xsheetHandle->notifyXsheetChanged();
  update();
}

//-----------------------------------------------------------------------------

int FunctionPanel::speedGraphTop() const {
  if (!m_speedGraphVisible) return height();
  // Never eat more than half the panel: on a short panel the value graph has
  // to stay the readable one -- the speed is the companion view, not the point.
  const int h = std::min(m_speedGraphHeight, height() / 2);
  return height() - h;
}

//-----------------------------------------------------------------------------

QList<TDoubleParam *> FunctionPanel::speedGraphCurves() const {
  QList<TDoubleParam *> curves;
  if (getSelection()) curves = getSelection()->getSelectedCurves();
  if (curves.isEmpty()) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;
    if (channel && channel->getParam()) curves.append(channel->getParam());
  }
  return curves;
}

//-----------------------------------------------------------------------------

void FunctionPanel::drawSpeedGraph(QPainter &painter) {
  if (!m_speedGraphVisible) return;

  const int ox = m_valueAxisX;
  const int y0 = speedGraphTop();
  const int h  = height() - y0;
  if (h < 20) return;

  // Opaque: this covers the bottom of the value graph rather than the value
  // transform being rescaled, which would have meant touching every clip rect
  // in paintEvent and re-deriving the whole layout.
  painter.setClipping(false);
  painter.fillRect(0, y0, width(), h, getBGColor().darker(115));
  painter.setPen(m_textColor);
  painter.drawLine(0, y0, width(), y0);

  const QList<TDoubleParam *> curves = speedGraphCurves();
  if (curves.isEmpty()) return;

  // Sample one column of pixels at a time, and take the derivative from the
  // CURVE rather than from the keyframes: with an expression or a file the
  // stored keyframe values are not what the curve actually passes through.
  const double dFrame = xToFrame(1) - xToFrame(0);
  if (dFrame <= 0.0) return;
  const double hStep = std::max(0.05, dFrame * 0.5);

  std::vector<std::vector<double>> speeds(curves.size());
  double maxAbs = 0.0;
  for (int c = 0; c < curves.size(); c++) {
    TDoubleParam *curve = curves[c];
    if (!curve) continue;
    speeds[c].reserve(width() - ox + 1);
    for (int x = ox; x <= width(); x++) {
      const double f = xToFrame(x);
      const double v =
          (curve->getValue(f + hStep) - curve->getValue(f - hStep)) /
          (2.0 * hStep);
      speeds[c].push_back(v);
      maxAbs = std::max(maxAbs, fabs(v));
    }
  }
  if (maxAbs <= 0.0) {
    // A flat selection is a real answer, not an empty panel: draw the zero line
    // so it reads as "no movement" instead of "nothing here".
    painter.setPen(QPen(m_textColor, 0, Qt::DotLine));
    painter.drawLine(ox, y0 + h / 2, width(), y0 + h / 2);
    return;
  }

  // Own vertical scale, auto-fitted, with no labelled axis: what matters here
  // is the SHAPE -- where the speed jumps, dips or reverses -- and a number of
  // units-per-frame would only invite reading it as an absolute.
  const double pad   = 0.92;
  const int zeroY    = y0 + h / 2;
  const double scale = (h / 2.0) * pad / maxAbs;

  painter.setClipRect(ox + 1, y0 + 1, width() - ox - 1, h - 1);
  painter.setPen(QPen(m_textColor, 0, Qt::DotLine));
  painter.drawLine(ox, zeroY, width(), zeroY);

  for (int c = 0; c < curves.size(); c++) {
    if (speeds[c].empty()) continue;
    QPainterPath path;
    for (size_t i = 0; i < speeds[c].size(); i++) {
      const double px = ox + (double)i;
      const double py = zeroY - speeds[c][i] * scale;
      if (i == 0)
        path.moveTo(px, py);
      else
        path.lineTo(px, py);
    }
    // Same colour the curve has above, so the two halves of the split read as
    // one thing. Matched by POINTER through the active channels: there is no
    // curve->channel lookup, and matching by name would break on translation.
    QColor color = Qt::gray;
    if (m_functionTreeModel) {
      for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
        FunctionTreeModel::Channel *ch =
            m_functionTreeModel->getActiveChannel(i);
        if (ch && ch->getParam() == curves[c]) {
          color = getChannelColor(ch, true);
          break;
        }
      }
    }
    painter.setPen(QPen(color, 1.2));
    painter.drawPath(path);
  }

  painter.setClipping(false);
  painter.setPen(m_textColor);
  painter.drawText(ox + 6, y0 + 14, tr("speed"));
}

//-----------------------------------------------------------------------------

void FunctionPanel::paintEvent(QPaintEvent *e) {
  m_gadgets.clear();

  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }

  QPainter painter(this);
  QFont font(fontName, 8);
  painter.setFont(font);
  QFontMetrics fm(font);

  // define ruler sizes
  m_valueAxisX     = fm.horizontalAdvance("-888.88") + 2;
  m_frameAxisY     = fm.height() + 2;
  m_graphViewportY = m_frameAxisY + 12;
  int ox           = m_valueAxisX;
  int oy0          = m_graphViewportY;
  int oy1          = m_frameAxisY;

  // QRect bounds(0,0,width(),height());

  // draw functions background
  painter.setBrush(getBGColor());
  painter.setPen(Qt::NoPen);
  painter.drawRect(ox, oy0, width() - ox, height() - oy0);

  painter.setClipRect(ox, 0, width() - ox, height());
  drawCurrentFrame(painter);
  drawFrameGrid(painter);

  painter.setClipRect(0, oy0, width(), height() - oy0);
  drawValueGrid(painter);

  // draw axes
  painter.setClipping(false);
  painter.setPen(m_textColor);
  painter.drawLine(0, oy0, width(), oy0);
  painter.drawLine(ox, oy1, width(), oy1);
  painter.drawLine(ox, 0, ox, height());

  // draw curves
  painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);
  drawOtherCurves(painter);
  drawCurrentCurve(painter);

  painter.setClipping(false);
  painter.setClipRect(ox + 1, oy1 + 1, width() - ox - 1, oy0 - oy1 - 1);
  drawGroupKeyframes(painter);
  painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);

  // tool
  if (m_dragTool) m_dragTool->draw(painter);

  // cursor
  if (m_cursor.visible) {
    painter.setClipRect(ox + 1, oy0 + 1, width() - ox - 1, height() - oy0 - 1);
    painter.setPen(getOtherCurvesColor());
    int x = frameToX(m_cursor.frame);
    painter.drawLine(x, oy0 + 1, x, oy0 + 10);
    QString text = QString::number(tround(m_cursor.frame) + 1);
    painter.drawText(x - fm.horizontalAdvance(text) / 2, oy0 + 10 + fm.height(),
                     text);

    TDoubleParam *currentCurve = getCurrentCurve();
    if (currentCurve) {
      const TUnit *unit = 0;
      if (currentCurve->getMeasure())
        unit = currentCurve->getMeasure()->getCurrentUnit();
      double displayValue = m_cursor.value;
      if (unit) displayValue = unit->convertTo(displayValue);
      // painter.setClipRect(0,oy0,height(),height()-oy0);
      int y = valueToY(currentCurve, m_cursor.value);
      painter.drawLine(ox, y, ox + 10, y);
      painter.drawText(ox + 15, y + 4, QString::number(displayValue, 'f', 2));
    }
  }

  // curve name
  if (m_curveLabel.text != "") {
    painter.setClipRect(ox, oy0, width() - ox, height() - oy0);
    painter.setPen(m_selectedColor);
    painter.drawLine(m_curveLabel.curvePos, m_curveLabel.labelPos);
    painter.drawText(m_curveLabel.labelPos,
                     QString::fromStdString(m_curveLabel.text));
  }

  // painter.setPen(Qt::black);
  // painter.drawText(QPointF(70,70),
  //  "f0=" + QString::number(xToFrame(ox)) +
  //  " f1=" + QString::number(xToFrame(width())));

  // painter.setPen(Qt::black);
  // painter.setBrush(Qt::NoBrush);
  // painter.drawRect(ox+10,oy+10,width()-ox-20,height()-oy-20);

  // Last: it paints OVER the bottom of the value graph.
  drawSpeedGraph(painter);
}

//-----------------------------------------------------------------------------

void FunctionPanel::mousePressEvent(QMouseEvent *e) {
  m_cursor.visible = false;

  // The speed strip is painted OVER the value graph, so without this a click
  // in it would grab whatever keyframe happens to lie underneath -- invisible
  // to the user, and a drag they never asked for. The strip is read-only, so
  // swallowing the press outright is the whole of the interaction.
  if (m_speedGraphVisible && e->pos().y() >= speedGraphTop()) return;

  // m_dragTool can be non-zero when both the left and the mid buttons are
  // pressed
  if (m_dragTool) {
    m_dragTool->release(e);
    delete m_dragTool;
    m_dragTool = nullptr;
  }

  if (e->button() == Qt::MiddleButton ||
      (e->button() == Qt::LeftButton && m_panningArmed)) {
    // mid mouse click => panning
    bool xLocked = e->pos().x() <= m_valueAxisX;
    bool yLocked = e->pos().y() <= m_valueAxisX;
    m_dragTool   = new PanDragTool(this, xLocked, yLocked);
    m_dragTool->click(e);
    return;
  } else if (e->button() == Qt::RightButton) {
    // right mouse click => open context menu
    openContextMenu(e);
    return;
  }

  QPoint winPos         = e->pos();
  Handle handle         = None;
  const int maxDistance = 20;
  int closestGadgetId   = findClosestGadget(e->pos(), handle, maxDistance);

  if (e->pos().x() > m_valueAxisX && e->pos().y() < m_frameAxisY &&
      closestGadgetId < 0 && (e->modifiers() & Qt::AltModifier) == 0) {
    // click on topbar => frame zoom
    m_dragTool = new ZoomDragTool(this, ZoomDragTool::FrameZoom);
  } else if (e->pos().x() < m_valueAxisX && e->pos().y() > m_graphViewportY) {
    // click on topbar => value zoom
    m_dragTool = new ZoomDragTool(this, ZoomDragTool::ValueZoom);
  } else if (m_currentFrameStatus == 1 && m_frameHandle != 0 &&
             closestGadgetId < 0) {
    // click on current frame => move frame
    m_currentFrameStatus = 2;
    m_dragTool           = new MoveFrameDragTool(this, m_frameHandle);
  }

  if (0 <= closestGadgetId && closestGadgetId < (int)m_gadgets.size()) {
    if (handle == 100)  // group move gadget
    {
      // Null CURVE: the group-handle mode, which takes every active channel.
      // Spelled out because the selection constructor would swallow a bare 0.
      MovePointDragTool *dragTool =
          new MovePointDragTool(this, (TDoubleParam *)0);
      dragTool->selectKeyframes(m_gadgets[closestGadgetId].m_keyframePosition);
      m_dragTool = dragTool;
    } else if (handle == 101 || handle == 102) {
      m_dragTool = new MoveGroupHandleDragTool(
          this, m_gadgets[closestGadgetId].m_keyframePosition, handle);
    }
  }

  if (m_dragTool) {
    m_dragTool->click(e);
    return;
  }

  FunctionTreeModel::Channel *currentChannel =
      m_functionTreeModel ? m_functionTreeModel->getCurrentChannel() : 0;

  // Shift on a keyframe of ANOTHER curve: aim at the keyframe, not at the
  // nearest line. The branch below only switches curve when the current one is
  // far from the cursor, so with two curves crossing -- the ordinary case in a
  // graph showing several columns -- a Shift-click on the other one's key
  // never reached it, and picking keys across curves looked broken.
  if ((e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) &&
      closestGadgetId < 0) {
    FunctionTreeModel::Channel *keyChannel =
        findChannelWithKeyframeAt(winPos, maxDistance);
    if (keyChannel && keyChannel != currentChannel) {
      keyChannel->setIsCurrent(true);
      FunctionTreeModel::ChannelGroup *keyGroup = keyChannel->getChannelGroup();
      if (keyGroup && !keyGroup->isOpen())
        keyGroup->getModel()->setExpandedItem(keyGroup->createIndex(), true);
      currentChannel = keyChannel;
      // Gadgets belong to whichever curve was current, so rebuild them before
      // looking for the keyframe this very click landed on.
      updateGadgets(keyChannel->getParam());
      closestGadgetId = findClosestGadget(winPos, handle, maxDistance);
    }
  }

  if (!currentChannel ||
      (getCurveDistance(currentChannel->getParam(), winPos) > maxDistance &&
       closestGadgetId < 0)) {
    // if current channel is undefined or its curve is too far from the clicked
    // point
    // the user is possibly trying to select a different curve
    FunctionTreeModel::Channel *channel =
        findClosestChannel(winPos, maxDistance);
    if (channel) {
      channel->setIsCurrent(true);
      // Open folder
      FunctionTreeModel::ChannelGroup *channelGroup =
          channel->getChannelGroup();
      if (!channelGroup->isOpen())
        channelGroup->getModel()->setExpandedItem(channelGroup->createIndex(),
                                                  true);
      currentChannel = channel;
      // Shift means "add to what I already picked", so switching curve must
      // not throw the selection away. A plain click still clears, exactly as
      // before.
      if (0 == (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)))
        getSelection()->selectNone();
      else {
        // ...and the gadgets still describe the curve that WAS current, so
        // this click has not yet seen the keyframe it landed on -- it would
        // fall through to "nothing clicked" and start a rubber band instead.
        // Rebuild them for the curve just switched to and look again, so one
        // Shift-click both switches curve and adds its key.
        updateGadgets(channel->getParam());
        closestGadgetId = findClosestGadget(winPos, handle, maxDistance);
      }
    }
  }

  if (currentChannel) {
    TDoubleParam *currentCurve = currentChannel->getParam();
    if (currentCurve) {
      int kIndex =
          closestGadgetId >= 0 ? m_gadgets[closestGadgetId].m_kIndex : -1;

      // The click landed on the curve that is ALREADY the current one. That
      // makes the branch above skip itself -- its condition is "the current
      // curve is far away" -- and with it the line that brings the channel's
      // column forward. So ask for it here: the current column may have moved
      // on in the meantime (picking another column in the xsheet does exactly
      // that), and clicking the curve is how one says "back to mine".
      // setIsCurrent is a no-op when there is nothing left to change.
      if (kIndex >= 0 || getCurveDistance(currentCurve, winPos) <= maxDistance)
        currentChannel->setIsCurrent(true);

      if (kIndex >= 0) {
        // keyframe clicked
        if (handle == FunctionPanel::Point) {
          // select point (if needed)
          if (!getSelection()->isSelected(currentCurve, kIndex)) {
            // shift- or ctrl-click => add to selection
            if (0 == (e->modifiers() &
                      (Qt::ShiftModifier | Qt::ControlModifier)))
              getSelection()->deselectAllKeyframes();
            getSelection()->select(currentCurve, kIndex);
          }

          // stretch the selected keys IF;
          // 1) alt key is pressed, and
          // 2) more than two keys are selected, and
          // 3) clicked key is one of the end of the selection, and
          // 4) all keys between the both end of the selection are selected
          if (e->modifiers() & Qt::AltModifier) {
            QList<int> selectedIndices =
                getSelection()->getSelectedKeyIndices(currentCurve);
            // Only from an END of the picked run: the opposite end is the
            // pivot the block scales about, so grabbing a key in the middle
            // has no sensible reading -- and used to scale anyway, about
            // whichever end the code happened to pick.
            const bool grabbedAnEnd =
                !selectedIndices.isEmpty() &&
                (selectedIndices.first() == kIndex ||
                 selectedIndices.last() == kIndex);

            // Keys picked on several curves: stretch them all by one ratio
            // about one pivot. Tried first, because the single-curve branch
            // below would silently scale only the current curve and leave the
            // rest of the selection where it was.
            if (grabbedAnEnd && getSelection()->getSelectedCurves().count() > 1) {
              MultiStretchDragTool *multi = new MultiStretchDragTool(
                  this, getSelection(), selectedIndices.first() == kIndex);
              if (multi->isValid())
                m_dragTool = multi;
              else
                delete multi;
            }
            if (!m_dragTool && selectedIndices.count() >= 3 && grabbedAnEnd &&
                (selectedIndices.last() - selectedIndices.first()) ==
                    selectedIndices.count() - 1) {
              bool moveLeft = selectedIndices.first() == kIndex;
              m_dragTool    = new StretchPointDragTool(
                  this, currentCurve, selectedIndices.first(),
                  selectedIndices.last(), moveLeft);
            }
          }

          if (!m_dragTool) {
            // move selected point(s)
            if (getSelection()->getSelectedSegment().first == 0 &&
                getSelection()->getSelectedCurves().count() > 1) {
              // Keys picked on more than one curve: drag them together, in
              // time and in value. The single-curve tool below can only hold
              // one setter and would silently leave the other curves behind.
              m_dragTool = new MovePointDragTool(this, getSelection());
            } else {
              MovePointDragTool *dragTool =
                  new MovePointDragTool(this, currentCurve);
              if (getSelection()->getSelectedSegment().first != 0) {
                // if a segment is selected then move only the clicked point
                dragTool->addKeyframe2(kIndex);
              } else {
                dragTool->setSelection(getSelection());
              }
              m_dragTool = dragTool;
            }
          }
        } else {
          m_dragTool =
              new MoveHandleDragTool(this, currentCurve, kIndex, handle);
        }
      } else {
        // no keyframe clicked
        int curveDistance =
            getCurveDistance(currentChannel->getParam(), winPos);
        bool isKeyframeable = true;
        bool isGroup        = abs(winPos.y() - (m_graphViewportY - 5)) < 5;
        if (0 != (e->modifiers() & Qt::AltModifier) &&
            (curveDistance < maxDistance || isGroup) && isKeyframeable) {
          // ALT-clicked near curve => create a new keyframe. It used to be
          // Ctrl, which Ctrl/Cmd-clicking to build up a selection needs.
          double frame = tround(xToFrame(winPos.x()));
          MovePointDragTool *dragTool =
              new MovePointDragTool(this, isGroup ? 0 : currentCurve);
          //          if(curveDistance>=maxDistance)
          //            dragTool->m_channelGroup =
          //            currentChannel->getChannelGroup();
          dragTool->createKeyframe(frame);
          dragTool->selectKeyframes(frame);
          m_dragTool = dragTool;

          /*
int kIndex = dragTool->createKeyframe(frame);
          if(kIndex!=-1)
          {
                  getSelection()->deselectAllKeyframes();
                  getSelection()->select(currentCurve, kIndex);
                  m_dragTool = dragTool;
          }
*/
          // assert(0);
        } else if (curveDistance < maxDistance) {
          // clicked near curve (but far from keyframes)
          double frame  = xToFrame(winPos.x());
          int shiftK0   = currentCurve->getPrevKeyframe(frame);
          int shiftK1   = currentCurve->getNextKeyframe(frame);
          // Shift on a segment ADDS it to the ones already picked, so that one
          // interpolation command can retype a whole run of them. No drag tool:
          // building up a selection is not the start of a move.
          if ((e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) &&
              shiftK0 >= 0 && shiftK1 == shiftK0 + 1) {
            getSelection()->addSegment(currentCurve, shiftK0);
            update();
            return;
          }
          getSelection()->deselectAllKeyframes();
          int k0 = shiftK0;
          int k1 = shiftK1;
          if (k0 >= 0 && k1 == k0 + 1) {
            // select and move the segment
            getSelection()->selectSegment(currentCurve, k0);
            MovePointDragTool *dragTool =
                new MovePointDragTool(this, currentCurve);
            dragTool->addKeyframe2(k0);
            dragTool->addKeyframe2(k1);
            m_dragTool = dragTool;
          } else {
            // start a rectangular selection
            m_dragTool = new RectSelectTool(this, currentCurve);
          }
        } else {
          // nothing clicked: drop the current curve and start a rectangular
          // selection. Without this there is always one curve drawn as
          // current, with its handles out, even after clicking away from
          // everything -- and no gesture to say "none".
          currentChannel->setIsCurrent(false);
          getSelection()->deselectAllKeyframes();
          m_dragTool = new RectSelectTool(this, currentCurve);
        }
      }
    }
  }

  // Everything above hangs off there being a CURRENT channel, and there need
  // not be one: a band dragged in empty space clears it, which is how one says
  // "select nothing". Without this, the click after that did nothing at all --
  // no deselect, and no new band either, so the only way back was to pick a
  // curve first. Self-perpetuating, since the empty click is what cleared it.
  if (!m_dragTool && !currentChannel) {
    getSelection()->deselectAllKeyframes();
    m_dragTool = new RectSelectTool(this, 0);
  }

  if (m_dragTool) m_dragTool->click(e);
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::mouseReleaseEvent(QMouseEvent *e) {
  if (m_dragTool) {
    m_dragTool->release(e);
    delete m_dragTool;
    m_dragTool = nullptr;
  }
  m_cursor.visible     = true;
  m_currentFrameStatus = 0;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::mouseMoveEvent(QMouseEvent *e) {
  if (e->buttons()) {
    if (m_dragTool) m_dragTool->drag(e);
  } else {
    m_cursor.frame   = xToFrame(e->pos().x());
    m_cursor.value   = 0;
    m_cursor.visible = true;

    TDoubleParam *currentCurve = getCurrentCurve();
    if (currentCurve) {
      Handle handle = None;
      int gIndex    = findClosestGadget(e->pos(), handle, 20);
      if (m_highlighted.handle != handle || m_highlighted.gIndex != gIndex) {
        m_highlighted.handle = handle;
        m_highlighted.gIndex = gIndex;
      }
      m_cursor.value = yToValue(currentCurve, e->pos().y());
    }

    double currentFrame = m_frameHandle ? m_frameHandle->getFrame() : 0;
    if (m_highlighted.handle == None &&
        std::abs(e->pos().x() - frameToX(currentFrame)) < 5)
      m_currentFrameStatus = 1;
    else
      m_currentFrameStatus = 0;

    updateHint(e->pos());

    FunctionTreeModel::Channel *closestChannel =
        findClosestChannel(e->pos(), 20);
    if (closestChannel && m_highlighted.handle == None) {
      TDoubleParam *curve = closestChannel->getParam();
      if (m_functionTreeModel->getActiveChannelCount() <= 1)
        //|| closestChannel == m_functionTreeModel->getCurrentChannel())
        curve = 0;
      if (curve && m_curveLabel.curve != curve) {
        m_curveLabel.curve  = curve;
        QString channelName = closestChannel->data(Qt::DisplayRole).toString();
        QString parentChannelName =
            closestChannel->getChannelGroup()->data(Qt::DisplayRole).toString();
        QString name      = parentChannelName + QString(", ") + channelName;
        m_curveLabel.text = name.toStdString();

        // in order to avoid run off the right-end of visible area
        int textWidth = fontMetrics().horizontalAdvance(name) + 30;
        double frame  = xToFrame(width() - textWidth);

        m_curveLabel.curvePos = getWinPos(curve, frame).toPoint();
        m_curveLabel.labelPos = m_curveLabel.curvePos + QPoint(20, -10);
      }
    } else {
      m_curveLabel.text  = "";
      m_curveLabel.curve = 0;
    }

    update();
  }
}

//-----------------------------------------------------------------------------

void FunctionPanel::mouseDoubleClickEvent(QMouseEvent *) { fitGraphToWindow(); }

//-----------------------------------------------------------------------------

void FunctionPanel::keyPressEvent(QKeyEvent *e) {
  FunctionPanelZoomer(this).exec(e);
}

//-----------------------------------------------------------------------------

namespace {

//! Spelled for the platform: on macOS the key is labelled Option.
static const QString kAltName =
#ifdef Q_OS_MAC
    QStringLiteral("\u2325 Option");
#else
    QStringLiteral("Alt");
#endif
static const QString kCtrlName =
#ifdef Q_OS_MAC
    QStringLiteral("\u2318 Cmd");
#else
    QStringLiteral("Ctrl");
#endif

}  // namespace

void FunctionPanel::updateHint(const QPoint &winPos) {
  // What the modifiers do depends on what is under the pointer, so the hint
  // is rebuilt as it moves. Sent only when the text changes: it is a status
  // bar, not a log.
  QString hint;

  Handle handle       = None;
  const int maxDist   = 20;
  const int gadgetId  = findClosestGadget(winPos, handle, maxDist);
  TDoubleParam *curve = getCurrentCurve();
  const int curveDist =
      curve ? getCurveDistance(curve, winPos) : maxDist + 1;

  if (gadgetId >= 0 && handle == Point)
    hint = tr("Drag to move  |  %1-click: add to selection  |  "
              "%2-drag on an end key: scale the block in time")
               .arg(kCtrlName)
               .arg(kAltName);
  else if (curveDist <= maxDist)
    hint = tr("%1-click: add the segment to the selection  |  "
              "%2-click: create a keyframe  |  "
              "Right-click: interpolation of every selected segment")
               .arg(kCtrlName)
               .arg(kAltName);
  else
    hint = tr("Drag a box: select the segments and keyframes inside it");

  if (hint == m_lastHint) return;
  m_lastHint = hint;
  // Emitted rather than pushed to the status bar directly: the hint bar is in
  // the application layer, which toonzqt cannot reach. Whoever builds the
  // panel wires this up.
  emit hintChanged(hint);
}

//-----------------------------------------------------------------------------

void FunctionPanel::enterEvent(QEvent *) {
  m_cursor.visible = true;
  m_panningArmed   = false;
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::leaveEvent(QEvent *) {
  m_cursor.visible = false;
  m_panningArmed   = false;
  setCursor(Qt::ArrowCursor);
  m_lastHint.clear();
  emit hintChanged(QString());
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::wheelEvent(QWheelEvent *e) {
  double factor = exp(0.002 * (double)e->angleDelta().y());
  zoom(factor, factor, e->position().toPoint());
}

//-----------------------------------------------------------------------------

void FunctionPanel::fitGraphToWindow(bool currentCurveOnly) {
  double f0 = 0, f1 = -1;
  double v0 = 0, v1 = -1;

  for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel =
        m_functionTreeModel->getActiveChannel(i);
    TDoubleParam *curve = channel->getParam();
    if (currentCurveOnly && curve != getCurrentCurve()) continue;

    const TUnit *unit = 0;
    if (curve->getMeasure()) unit = curve->getMeasure()->getCurrentUnit();
    int n = curve->getKeyframeCount();
    if (n == 0) {
      double v = curve->getDefaultValue();
      if (unit) v = unit->convertTo(v);
      if (v0 > v1)
        v0 = v1 = v;
      else if (v > v1)
        v1 = v;
      else if (v < v0)
        v0 = v;
    } else {
      TDoubleKeyframe k = curve->getKeyframe(0);
      double fa         = k.m_frame;
      k                 = curve->getKeyframe(n - 1);
      double fb         = k.m_frame;
      if (f0 > f1) {
        f0 = fa;
        f1 = fb;
      } else {
        f0 = std::min(f0, fa);
        f1 = std::max(f1, fb);
      }
      double v = curve->getValue(fa);
      if (unit) v = unit->convertTo(v);
      if (v0 > v1) v0 = v1 = v;
      int m = 50;
      for (int j = 0; j < m; j++) {
        double t = (double)j / (double)(m - 1);
        double v = curve->getValue((1 - t) * fa + t * fb);
        if (unit) v = unit->convertTo(v);
        v0 = std::min(v0, v);
        v1 = std::max(v1, v);
      }
    }
  }
  if (f0 >= f1 || v0 >= v1) {
    m_viewTransform = QTransform();
    m_viewTransform.translate(m_valueAxisX, 200);
    m_viewTransform.scale(5, -1);
  } else {
    double mx       = (width() - m_valueAxisX - 20) / (f1 - f0);
    double my       = -(height() - m_graphViewportY - 20) / (v1 - v0);
    // Keep value negative so vertical axis is numbered top down
    if (my > 0) my *= -1;
    double dx       = m_valueAxisX + 10 - f0 * mx;
    double dy       = m_graphViewportY + 10 - v1 * my;
    m_viewTransform = QTransform(mx, 0, 0, my, dx, dy);
  }
  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::fitSelectedPoints() { fitGraphToWindow(true); }

//-----------------------------------------------------------------------------

void FunctionPanel::fitCurve() { fitGraphToWindow(); }

//-----------------------------------------------------------------------------

void FunctionPanel::fitRegion(double f0, double v0, double f1, double v1) {}

//-----------------------------------------------------------------------------

static void setSegmentType(FunctionSelection *selection, TDoubleParam *curve,
                           int segmentIndex, TDoubleKeyframe::Type type) {
  // Several segments picked with Shift, and the one right-clicked among them:
  // retype the lot in one undo. Note the old line below DISCARDS the selection
  // before applying -- which is why picking several used to change nothing.
  if (selection->getSelectedSegmentCount() > 1 &&
      selection->isSegmentSelected(curve, segmentIndex)) {
    selection->setSelectedSegmentsType(type);
    return;
  }
  selection->selectSegment(curve, segmentIndex);
  KeyframeSetter setter(curve, segmentIndex);
  setter.setType(type);
}

//-----------------------------------------------------------------------------

void FunctionPanel::openContextMenu(QMouseEvent *e) {
  QAction linkHandlesAction(tr("Link Handles"), 0);
  QAction unlinkHandlesAction(tr("Unlink Handles"), 0);
  QAction resetHandlesAction(tr("Reset Handles"), 0);
  QAction deleteKeyframeAction(tr("Delete"), 0);
  QAction insertKeyframeAction(tr("Set Key"), 0);
  QAction activateCycleAction(tr("Activate Cycle"), 0);
  QAction deactivateCycleAction(tr("Deactivate Cycle"), 0);
  QAction setLinearAction(tr("Linear Interpolation"), 0);
  QAction setSpeedInOutAction(tr("Speed In / Speed Out Interpolation"), 0);
  QAction setEaseInOutAction(tr("Ease In / Ease Out Interpolation"), 0);
  QAction setEaseInOut2Action(tr("Ease In / Ease Out (%) Interpolation"), 0);
  QAction setExponentialAction(tr("Exponential Interpolation"), 0);
  QAction setExpressionAction(tr("Expression Interpolation"), 0);
  QAction setFileAction(tr("File Interpolation"), 0);
  QAction setConstantAction(tr("Constant Interpolation"), 0);
  QAction setSimilarShapeAction(tr("Similar Shape Interpolation"), 0);
  QAction autoBezierAction(tr("Auto Bezier"), 0);
  QAction flatAction(tr("Flat"), 0);
  QAction copyTangentsAction(tr("Copy Tangents"), 0);
  QAction pasteTangentsAction(tr("Paste Tangents"), 0);
  QAction roveAction(tr("Rove Keys"), 0);
  QAction fitSelectedAction(tr("Fit Selection"), 0);
  QAction fitAllAction(tr("Fit"), 0);
  QAction setStep1Action(tr("Step 1"), 0);
  QAction setStep2Action(tr("Step 2"), 0);
  QAction setStep3Action(tr("Step 3"), 0);
  QAction setStep4Action(tr("Step 4"), 0);
  // Filled in menu order below, so an entry's position IS its preset index.
  QVector<QAction *> easePresetActions;

  TDoubleParam *curve = getCurrentCurve();
  int segmentIndex    = -1;
  TDoubleKeyframe kf;
  double frame = xToFrame(e->pos().x());

  // Which curve this menu is about is decided BEFORE giving up for the want of
  // a current one. There need not be a current curve at all: a band dragged in
  // empty space clears it (that is how one says "select nothing"), and the
  // menu bailing out here is why right-clicking a set of segments picked that
  // way opened nothing.
  //
  // Prefer the curve whose SELECTED segment lies under the cursor. This menu
  // has always worked on the current curve, which was fine while only one
  // segment could be picked; with several picked across curves that overlap,
  // right-clicking one of them would open the menu for whichever curve
  // happened to be current and act on the wrong line.
  if (m_functionTreeModel && getSelection()->getSelectedSegmentCount() > 0) {
    const int maxDistance = 20;
    int bestDistance      = maxDistance + 1;
    for (int i = 0; i < m_functionTreeModel->getActiveChannelCount(); i++) {
      FunctionTreeModel::Channel *channel =
          m_functionTreeModel->getActiveChannel(i);
      TDoubleParam *other = channel ? channel->getParam() : 0;
      if (!other) continue;
      const int k = other->getPrevKeyframe(frame);
      if (k < 0 || k >= other->getKeyframeCount() - 1) continue;
      if (!getSelection()->isSegmentSelected(other, k)) continue;
      const int distance = getCurveDistance(other, e->pos());
      if (distance <= maxDistance && distance < bestDistance) {
        bestDistance = distance;
        curve        = other;
      }
    }
  }

  // Still nothing: fall back to whichever drawn curve runs nearest the cursor,
  // so the menu opens on what was pointed at rather than not at all.
  if (!curve) {
    if (FunctionTreeModel::Channel *channel = findClosestChannel(e->pos(), 20))
      curve = channel->getParam();
  }
  if (!curve) return;

  // build menu
  QMenu menu(0);
  if (m_highlighted.handle == Point && m_highlighted.gIndex >= 0 &&
      m_gadgets[m_highlighted.gIndex].m_handle != 100) {
    kf = curve->getKeyframe(m_gadgets[m_highlighted.gIndex].m_kIndex);
    if (kf.m_linkedHandles)
      menu.addAction(&unlinkHandlesAction);
    else
      menu.addAction(&linkHandlesAction);
    menu.addAction(&resetHandlesAction);
    menu.addAction(&deleteKeyframeAction);
  } else {
    int k0 = curve->getPrevKeyframe(frame);
    int k1 = curve->getNextKeyframe(frame);
    if (k0 == curve->getKeyframeCount() - 1)  // after last keyframe
    {
      if (curve->isCycleEnabled())
        menu.addAction(&deactivateCycleAction);
      else
        menu.addAction(&activateCycleAction);
    }
    menu.addAction(&insertKeyframeAction);
    if (k0 >= 0 && k1 >= 0) {
      menu.addSeparator();
      segmentIndex = k0;
      kf           = curve->getKeyframe(k0);
      // Which type to grey out. With ONE segment it is the type that segment
      // already has. With several picked it is the one they ALL have -- and
      // when they differ, none: every entry is then a real change for at least
      // one of them, so greying any out would put a working choice out of
      // reach.
      int shownType = (int)kf.m_type;
      if (getSelection()->getSelectedSegmentCount() > 1 &&
          getSelection()->isSegmentSelected(curve, segmentIndex))
        shownType = getSelection()->getCommonSelectedSegmentsType();

      menu.addAction(&setLinearAction);
      if (shownType == (int)TDoubleKeyframe::Linear)
        setLinearAction.setEnabled(false);
      menu.addAction(&setSpeedInOutAction);
      if (shownType == (int)TDoubleKeyframe::SpeedInOut)
        setSpeedInOutAction.setEnabled(false);
      menu.addAction(&setEaseInOutAction);
      if (shownType == (int)TDoubleKeyframe::EaseInOut)
        setEaseInOutAction.setEnabled(false);
      menu.addAction(&setEaseInOut2Action);
      if (shownType == (int)TDoubleKeyframe::EaseInOutPercentage)
        setEaseInOut2Action.setEnabled(false);
      menu.addAction(&setExponentialAction);
      if (shownType == (int)TDoubleKeyframe::Exponential)
        setExponentialAction.setEnabled(false);
      menu.addAction(&setExpressionAction);
      if (shownType == (int)TDoubleKeyframe::Expression)
        setExpressionAction.setEnabled(false);
      menu.addAction(&setSimilarShapeAction);
      if (shownType == (int)TDoubleKeyframe::SimilarShape)
        setSimilarShapeAction.setEnabled(false);
      menu.addAction(&setFileAction);
      if (shownType == (int)TDoubleKeyframe::File)
        setFileAction.setEnabled(false);
      menu.addAction(&setConstantAction);
      if (shownType == (int)TDoubleKeyframe::Constant)
        setConstantAction.setEnabled(false);
      menu.addSeparator();
      if (kf.m_step != 1) menu.addAction(&setStep1Action);
      if (kf.m_step != 2) menu.addAction(&setStep2Action);
      if (kf.m_step != 3) menu.addAction(&setStep3Action);
      if (kf.m_step != 4) menu.addAction(&setStep4Action);
      menu.addSeparator();
    }
  }
  if (!getSelection()->isEmpty()) {
    menu.addSeparator();
    menu.addAction(&autoBezierAction);
    menu.addAction(&flatAction);

    // The named easing curves. Auto Bezier decides a shape FROM the animation
    // -- it is the answer to "make this pass through smoothly". These are the
    // other request: a shape chosen because it is that shape, imposed on the
    // segment whatever the neighbours are doing.
    //
    // Submenus by family rather than fifteen entries in a row: the choice is
    // really two, which curve and which way round, and nesting asks them in
    // that order.
    QMenu *easeMenu = menu.addMenu(tr("Ease Presets"));
    {
      int presetCount           = 0;
      const EasePreset *presets = KeyframeSetter::getEasePresets(presetCount);
      QMenu *familyMenu         = 0;
      QString family;
      for (int i = 0; i < presetCount; i++) {
        const QString presetFamily = QString::fromLatin1(presets[i].m_family);
        if (!familyMenu || family != presetFamily) {
          family     = presetFamily;
          familyMenu = easeMenu->addMenu(family);
        }
        // What the entry DOES, next to what it is called. "In" and "Out" are
        // the motion-design words, where In is a slow START -- and the segment
        // editor two panels away uses the same two words the other way round,
        // so leaving them to speak for themselves would be leaving a coin toss.
        QString label;
        switch (presets[i].m_variant) {
        case EasePreset::In:
          label = tr("In (slow start)");
          break;
        case EasePreset::Out:
          label = tr("Out (slow end)");
          break;
        default:
          label = tr("In-Out (slow both ends)");
          break;
        }
        easePresetActions.append(familyMenu->addAction(label));
      }
    }

    // Shown always, greyed when they do not apply, rather than hidden. Copy
    // needs exactly one keyframe and Paste needs something already copied --
    // so hiding them meant Copy could not be found while several keys were
    // picked, and Paste could never be found at all.
    menu.addAction(&copyTangentsAction);
    copyTangentsAction.setEnabled(getSelection()->getSelectedKeyframeCount() ==
                                  1);
    menu.addAction(&pasteTangentsAction);
    pasteTangentsAction.setEnabled(getSelection()->hasCopiedTangents());

    // Only meaningful on posPath: elsewhere "constant speed" would be the
    // constant speed of one channel on its own, which for an X used together
    // with a Y is not a speed at all. Greyed rather than absent, so that it
    // can be found before there is a path to use it on.
    menu.addAction(&roveAction);
    roveAction.setEnabled(getSelection()->isSelectionOnPosPath());

    // The tangent commands change the animation; Fit only changes the view.
    menu.addSeparator();
  }
  if (!getSelection()->isEmpty()) menu.addAction(&fitSelectedAction);
  menu.addAction(&fitAllAction);

  // curve shape type
  QAction linkCurvesAction(tr("Link Curves..."), 0);
  menu.addAction(&linkCurvesAction);
  linkCurvesAction.setEnabled(!getSelection()->isEmpty() || getCurrentCurve());

  QAction speedGraphAction(tr("Show Speed Graph"), 0);
  speedGraphAction.setCheckable(true);
  speedGraphAction.setChecked(m_speedGraphVisible);
  menu.addAction(&speedGraphAction);

  QAction curveShapeSmoothAction(tr("Smooth"), 0);
  QAction curveShapeFrameBasedAction(tr("Frame Based"), 0);
  QMenu curveShapeSubmenu(tr("Curve Shape"), 0);
  menu.addSeparator();
  curveShapeSubmenu.addAction(&curveShapeSmoothAction);
  curveShapeSubmenu.addAction(&curveShapeFrameBasedAction);
  menu.addMenu(&curveShapeSubmenu);

  curveShapeSmoothAction.setCheckable(true);
  curveShapeSmoothAction.setChecked(m_curveShape == SMOOTH);
  curveShapeFrameBasedAction.setCheckable(true);
  curveShapeFrameBasedAction.setChecked(m_curveShape == FRAME_BASED);

  // Store m_highlighted due to the following exec()
  Highlighted highlighted(m_highlighted);

  // execute menu
  QAction *action = menu.exec(e->globalPos());  // Will process events, possibly
                                                // altering m_highlighted
                                                // (MAC-verified)
  if (action == &linkHandlesAction)  // Let's just *hope* that doesn't happen to
                                     // m_gadgets though...  :/
  {
    if (m_gadgets[highlighted.gIndex].m_handle != 100)
      KeyframeSetter(curve, m_gadgets[highlighted.gIndex].m_kIndex)
          .linkHandles();
  } else if (action == &unlinkHandlesAction) {
    if (m_gadgets[highlighted.gIndex].m_handle != 100)
      KeyframeSetter(curve, m_gadgets[highlighted.gIndex].m_kIndex)
          .unlinkHandles();
  } else if (action == &resetHandlesAction) {
    kf.m_speedIn  = TPointD(-5, 0);
    kf.m_speedOut = -kf.m_speedIn;
    curve->setKeyframe(kf);
  } else if (action == &deleteKeyframeAction) {
    KeyframeSetter::removeKeyframeAt(curve, kf.m_frame, m_objectHandle, m_xsheetHandle);
  } else if (action == &insertKeyframeAction) {

    bool hasDrawingKeys = curve->getName() == "W_DrawingNumber";
    int frameId         = -1;
    if (hasDrawingKeys) {
      TUndoManager::manager()->beginBlock();
      int col             = m_sheet->getColumnIndexByCurve(curve);
      TStageObject *stObj = m_sheet->getStageObject(col);
      int xcol            = stObj ? stObj->getId().getIndex() : -1;
      TXsheet *xsh        = m_xsheetHandle->getXsheet();
      if (stObj)
        xsh->addUndoDrawingNumberChange(tround(frame), stObj->getId());
      TXshCell cell =
          (xcol < 0) ? TXshCell() : xsh->getCell(tround(frame), xcol);
      frameId       = cell.getFrameId().getNumber();
    }
    KeyframeSetter setter(curve, m_xsheetHandle);
    setter.createKeyframe(tround(frame));
    if (hasDrawingKeys) {
      if (frameId >= 0) setter.setValue(frameId);
      setter.addUndo();
      TUndoManager::manager()->endBlock();
    }
  } else if (action == &activateCycleAction) {
    KeyframeSetter::enableCycle(curve, true);
  } else if (action == &deactivateCycleAction) {
    KeyframeSetter::enableCycle(curve, false);
  } else if (action == &setLinearAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Linear);
  else if (action == &setSpeedInOutAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::SpeedInOut);
  else if (action == &setEaseInOutAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::EaseInOut);
  else if (action == &setEaseInOut2Action)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::EaseInOutPercentage);
  else if (action == &setExponentialAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Exponential);
  else if (action == &setExpressionAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Expression);
  else if (action == &setSimilarShapeAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::SimilarShape);
  else if (action == &setFileAction)
    setSegmentType(getSelection(), curve, segmentIndex, TDoubleKeyframe::File);
  else if (action == &setConstantAction)
    setSegmentType(getSelection(), curve, segmentIndex,
                   TDoubleKeyframe::Constant);
  else if (action == &autoBezierAction)
    getSelection()->setSelectedKeyframesAutoBezier();
  else if (action == &flatAction)
    getSelection()->setSelectedKeyframesFlat();
  else if (easePresetActions.contains(action)) {
    int presetCount           = 0;
    const EasePreset *presets = KeyframeSetter::getEasePresets(presetCount);
    const int i               = easePresetActions.indexOf(action);
    if (i >= 0 && i < presetCount) getSelection()->applyEasePreset(presets[i]);
  } else if (action == &copyTangentsAction)
    getSelection()->copyTangents();
  else if (action == &pasteTangentsAction)
    getSelection()->pasteTangents();
  else if (action == &roveAction)
    getSelection()->distributeSelectedEvenly();
  else if (action == &fitSelectedAction)
    fitSelectedPoints();
  else if (action == &fitAllAction)
    fitCurve();
  else if (action == &setStep1Action)
    KeyframeSetter(curve, segmentIndex).setStep(1);
  else if (action == &setStep2Action)
    KeyframeSetter(curve, segmentIndex).setStep(2);
  else if (action == &setStep3Action)
    KeyframeSetter(curve, segmentIndex).setStep(3);
  else if (action == &setStep4Action)
    KeyframeSetter(curve, segmentIndex).setStep(4);

  else if (action == &linkCurvesAction)
    linkSelectedCurves();
  else if (action == &speedGraphAction)
    m_speedGraphVisible = !m_speedGraphVisible;
  else if (action == &curveShapeSmoothAction)
    m_curveShape = SMOOTH;
  else if (action == &curveShapeFrameBasedAction)
    m_curveShape = FRAME_BASED;

  update();
}

//-----------------------------------------------------------------------------

void FunctionPanel::setFrameHandle(TFrameHandle *frameHandle) {
  if (m_frameHandle == frameHandle) return;
  if (m_frameHandle) m_frameHandle->disconnect(this);
  m_frameHandle = frameHandle;
  if (isVisible() && m_frameHandle) {
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));
    update();
  }
  assert(m_selection);
  m_selection->setFrameHandle(frameHandle);
}

//-----------------------------------------------------------------------------

void FunctionPanel::showEvent(QShowEvent *) {
  if (m_frameHandle)
    connect(m_frameHandle, SIGNAL(frameSwitched()), this,
            SLOT(onFrameSwitched()));
}

//-----------------------------------------------------------------------------

void FunctionPanel::hideEvent(QHideEvent *) {
  if (m_frameHandle) m_frameHandle->disconnect(this);
}

//-----------------------------------------------------------------------------

void FunctionPanel::onFrameSwitched() { update(); }

//-----------------------------------------------------------------------------

void FunctionPanel::onFitCalled() { fitGraphToWindow(); }

//-----------------------------------------------------------------------------

QColor FunctionPanel::getChannelColor(QString name, bool active) {
  QColor color;
  if (name == "X")
    color = QColor("firebrick");
  else if (name == "Y")
    color = QColor("limegreen");
  else if (name == "Z")
    color = QColor("deepskyblue");
  else if (name == "SO")
    color = QColor("hotpink");
  else if (name == "Rotation")
    color = QColor("darkorchid");
  else if (name == "Scale")
    color = QColor("gold");
  else if (name == "Scale H")
    color = QColor("gold");
  else if (name == "Scale V")
    color = QColor("gold");
  else if (name == "Shear H")
    color = QColor("darkorange");
  else if (name == "Shear V")
    color = QColor("darkorange");
  else if (name == "Drawing #")
    color = QColor("lightgreen"); 
  else if (name == "posPath")
    color = QColor("darksalmon");
  else
    color = QColor("darkcyan");
  if (!active) color.setAlpha(180);
  return color;
}

//-----------------------------------------------------------------------------

QColor FunctionPanel::getChannelColor(FunctionTreeModel::Channel *channel,
                                      bool active) {
  QColor color = getChannelColor(channel->getShortName(), active);
  // Multiplied, not assigned: the "not the current curve" fade above has to
  // survive, or an inert curve would come out BRIGHTER than the live ones
  // around it.
  if (channel->isInert()) color.setAlpha(int(color.alpha() * 0.35));
  return color;
}

//-----------------------------------------------------------------------------

bool FunctionPanel::event(QEvent *e) {
  if (e->type() != QEvent::KeyPress && e->type() != QEvent::ShortcutOverride &&
      e->type() != QEvent::KeyRelease)
    return QDialog::event(e);

  QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);

  std::string keyStr = QKeySequence(keyEvent->key() + keyEvent->modifiers())
                           .toString()
                           .toStdString();
  QAction *action = CommandManager::instance()->getActionFromShortcut(keyStr);
  std::string actionId = CommandManager::instance()->getIdFromAction(action);

  if (actionId != T_Hand) return QDialog::event(e);

  if (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride) {
    m_panningArmed = true;
    action->setEnabled(false);
    setToolCursor(this, ToolCursor::PanCursor);
    e->accept();
    return true;
  } else if (e->type() == QEvent::KeyRelease) {
    if (!keyEvent->isAutoRepeat()) m_panningArmed = false;
    action->setEnabled(true);
    setCursor(Qt::ArrowCursor);
    e->accept();
    return true;
  }
  return QDialog::event(e);
}
