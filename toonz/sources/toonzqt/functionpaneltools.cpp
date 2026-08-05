

#include "functionpaneltools.h"

// TnzQt includes
#include "toonzqt/functionselection.h"
#include "toonzqt/functionsheet.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tstageobject.h"
#include "toonz/txshcell.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QMultiMap>

//=============================================================================

MoveFrameDragTool::MoveFrameDragTool(FunctionPanel *panel,
                                     TFrameHandle *frameHandle)
    : m_panel(panel), m_frameHandle(frameHandle) {}

void MoveFrameDragTool::drag(QMouseEvent *e) {
  double frame = m_panel->xToFrame(e->pos().x());
  m_panel->getSelection()->deselectAllKeyframes();
  m_frameHandle->setFrame(frame);
}

//=============================================================================

PanDragTool::PanDragTool(FunctionPanel *panel, bool xLocked, bool yLocked)
    : m_panel(panel), m_xLocked(xLocked), m_yLocked(yLocked) {}

void PanDragTool::click(QMouseEvent *e) { m_oldPos = e->pos(); }

void PanDragTool::drag(QMouseEvent *e) {
  QPoint delta = e->pos() - m_oldPos;
  if (m_xLocked) delta.setX(0);
  if (m_yLocked) delta.setY(0);
  m_panel->pan(delta);
  m_oldPos = e->pos();
}

//=============================================================================
/*--- Ruler部分ドラッグによるズーム ---*/
void ZoomDragTool::click(QMouseEvent *e) {
  m_startPos = m_oldPos = m_startPos = e->pos();
}

void ZoomDragTool::drag(QMouseEvent *e) {
  QPoint delta = e->pos() - m_oldPos;
  m_oldPos     = e->pos();
  double sx = 1, sy = 1;
  // reflect horizontal drag for frame zoom
  double zoomFactor =
      exp(-0.0075 * ((m_zoomType == FrameZoom) ? -delta.x() : delta.y()));
  if (m_zoomType == FrameZoom)
    sx = zoomFactor;
  else
    sy = zoomFactor;
  m_panel->zoom(sx, sy, m_startPos);
}

void ZoomDragTool::release(QMouseEvent *e) {
  if ((e->pos() - m_startPos).manhattanLength() < 2) {
    double frame = m_panel->xToFrame(e->pos().x());
    if (m_panel->getFrameHandle()) m_panel->getFrameHandle()->setFrame(frame);
  }
}

//=============================================================================

void RectSelectTool::click(QMouseEvent *e) {
  m_startPos = e->pos();
  m_rect     = QRect();
}

namespace {

//! Adds every KEYFRAME of \p curve inside the band. Runs alongside the
//! segment pass rather than instead of it: the two together mean the box takes
//! what is inside it, full stop. On its own the segment pass would miss a key
//! whose segments fall outside the box, and skip a one-keyframe curve
//! entirely -- it has no segments to find.
void selectKeyframesUnderBand(FunctionPanel *panel, TDoubleParam *curve,
                              const QRect &rect) {
  if (!curve) return;
  for (int i = 0; i < curve->getKeyframeCount(); i++) {
    const QPointF p = panel->getWinPos(curve, curve->getKeyframe(i));
    if (rect.contains(tround(p.x()), tround(p.y())))
      panel->getSelection()->select(curve, i);
  }
}

//! Adds every SEGMENT the band passes over. The band is walked column of
//! pixels by column of pixels: wherever the curve runs inside it, the segment
//! that stretch belongs to is taken. Segments -- not keyframes -- because that
//! is what the interpolation commands act on; the two keyframes bounding each
//! segment come along on their own, so moving and deleting still have
//! something to work with.
void selectSegmentsUnderBand(FunctionPanel *panel, TDoubleParam *curve,
                             const QRect &rect) {
  if (!curve) return;
  const int kCount = curve->getKeyframeCount();
  if (kCount < 2) return;

  int lastAdded = -1;
  for (int x = rect.left(); x <= rect.right(); ++x) {
    const double frame = panel->xToFrame(x);
    const QPointF p =
        panel->getWinPos(curve, frame, curve->getValue(frame));
    if (!rect.contains(x, tround(p.y()))) continue;

    const int k = curve->getPrevKeyframe(frame);
    // Before the first keyframe, or past the last one, there is no segment.
    if (k < 0 || k >= kCount - 1 || k == lastAdded) continue;
    panel->getSelection()->addSegment(curve, k);
    lastAdded = k;
  }
}

}  // namespace

void RectSelectTool::drag(QMouseEvent *e) {
  m_rect = QRect(m_startPos, e->pos()).normalized();
  m_panel->getSelection()->deselectAllKeyframes();

  // Across every curve the graph is drawing, not just the current one: a band
  // dragged over a spot catches what is visibly under it. Curves you do not
  // want caught are the ones to hide -- which is what the tree's visibility
  // commands are for.
  FunctionTreeModel *model = m_panel->getModel();
  bool anyChannel          = false;
  if (model) {
    for (int c = 0; c < model->getActiveChannelCount(); c++) {
      FunctionTreeModel::Channel *channel = model->getActiveChannel(c);
      TDoubleParam *curve = channel ? channel->getParam() : 0;
      if (!curve) continue;
      anyChannel = true;
      selectSegmentsUnderBand(m_panel, curve, m_rect);
      selectKeyframesUnderBand(m_panel, curve, m_rect);
    }
  }

  // No model or nothing active: fall back to the curve the band was started
  // on, so the band never comes up empty for want of plumbing.
  if (!anyChannel) {
    selectSegmentsUnderBand(m_panel, m_curve, m_rect);
    selectKeyframesUnderBand(m_panel, m_curve, m_rect);
  }

  m_panel->update();
}

void RectSelectTool::release(QMouseEvent *e) { m_panel->update(); }

void RectSelectTool::draw(QPainter &painter) {
  painter.setPen(Qt::white);
  painter.setBrush(QColor(255, 255, 255, 127));
  if (!m_rect.isEmpty()) painter.drawRect(m_rect);
}

//=============================================================================

MovePointDragTool::MovePointDragTool(FunctionPanel *panel, TDoubleParam *curve)
    : m_panel(panel)
    , m_deltaFrame(0)
    , m_speed0Length(0)
    , m_speed0Index(-1)
    , m_speed1Length(0)
    , m_speed1Index(-1)
    , m_groupEnabled(false)
    , m_selection(0) {
    // This undo block is closed in the destructor
  TUndoManager::manager()->beginBlock();

  if (curve) {
    KeyframeSetter *setter =
        new KeyframeSetter(curve, m_panel->getXsheetHandle());
    m_setters.push_back(setter);
  } else {
    m_groupEnabled           = true;
    FunctionTreeModel *model = panel->getModel();
    for (int i = 0; i < model->getActiveChannelCount(); i++) {
      FunctionTreeModel::Channel *channel = model->getActiveChannel(i);
      if (channel && channel->getParam()) {
        TDoubleParam *curve    = channel->getParam();
        KeyframeSetter *setter = new KeyframeSetter(curve);
        m_setters.push_back(setter);
      }
    }
  }
}

//-----------------------------------------------------------------------------

MovePointDragTool::MovePointDragTool(FunctionPanel *panel,
                                     FunctionSelection *selection)
    : m_panel(panel)
    , m_deltaFrame(0)
    , m_speed0Length(0)
    , m_speed0Index(-1)
    , m_speed1Length(0)
    , m_speed1Index(-1)
    , m_groupEnabled(false)
    , m_selection(selection) {
  // This undo block is closed in the destructor
  TUndoManager::manager()->beginBlock();

  if (!selection) return;

  // Only the curves the user actually picked keys on -- NOT every active
  // channel, which is what the group-handle mode does. Dragging a selection
  // must move what is selected and nothing else, even when a dozen other
  // curves are on screen.
  const QList<TDoubleParam *> curves = selection->getSelectedCurves();
  for (TDoubleParam *curve : curves) {
    if (!curve) continue;
    KeyframeSetter *setter =
        new KeyframeSetter(curve, m_panel->getXsheetHandle());
    const QList<int> indices = selection->getSelectedKeyIndices(curve);
    for (int kIndex : indices) setter->selectKeyframe(kIndex);
    m_setters.push_back(setter);
  }
}

//-----------------------------------------------------------------------------

MovePointDragTool::~MovePointDragTool() {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i];
  m_setters.clear();
  m_startFrames.clear();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void MovePointDragTool::addKeyframe2(int kIndex) {
  assert(m_setters.size() == 1);
  if (m_setters.size() == 1) m_setters[0]->selectKeyframe(kIndex);
}

//-----------------------------------------------------------------------------

void MovePointDragTool::createKeyframe(double frame) {
  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    int kIndex             = setter->createKeyframe(tround(frame));
    setter->selectKeyframe(kIndex);
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::selectKeyframes(double frame) {
  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();
    setter->setPixelRatio(m_panel->getPixelRatio(curve));
    int kIndex = curve->getClosestKeyframe(frame);
    if (kIndex >= 0) {
      double kf = curve->keyframeIndexToFrame(kIndex);
      if (fabs(kf - frame) < 0.01) setter->selectKeyframe(kIndex);
    }
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::setSelection(FunctionSelection *selection) {
  if (selection) {
    assert(m_setters.size() == 1);
    if (m_setters.size() == 1) {
      TDoubleParam *curve = m_setters[0]->getCurve();
      assert(curve);
      if (curve) {
        m_selection = selection;
        for (int i = 0; i < curve->getKeyframeCount(); i++)
          if (selection->isSelected(curve, i)) addKeyframe2(i);
      }
    }
  } else
    m_selection = selection;
}

//-----------------------------------------------------------------------------

void MovePointDragTool::click(QMouseEvent *e) {
  m_startPos = m_oldPos = e->pos();
  m_deltaFrame          = 0;
  double frame          = m_panel->xToFrame(e->pos().x());

  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();

    FunctionSheet *sheet = m_panel->getFunctionSheet();
    int col              = sheet->getColumnIndexByCurve(curve);
    TStageObject *stObj  = sheet->getStageObject(col);
    int xcol             = stObj ? stObj->getId().getIndex() : -1;

    std::set<double> frames;
    if (curve->getName() == "W_DrawingNumber" && xcol >= 0 &&
        m_undoDrawings.find(xcol) == m_undoDrawings.end()) {
      setter->getCurve()->getKeyframes(frames);

      int r0, r1;
      TXsheet *xsh        = m_panel->getXsheetHandle()->getXsheet();
      xsh->getCellRange(xcol, r0, r1);
      int n = r1 + 1;
      std::vector<TXshCell> cells(n);
      xsh->getCells(0, xcol, n, &cells[0], false, false);
      for (int i = 0; i < n; i++) {
        if (cells[i].isEmpty()) continue;
        if (stObj->hasDrawingNumberKey(i) ||
            stObj->isChannelInterpolated(TStageObject::T_DrawingNumber, i))
          cells[i] = TXshCell();
      }
      m_undoDrawings.insert(xcol, cells);
    }
    m_startFrames.push_back(frames);

    setter->setPixelRatio(m_panel->getPixelRatio(curve));
    if (!m_groupEnabled) {
      int kIndex = curve->getClosestKeyframe(frame);
      if (kIndex >= 0) {
        double kf = curve->keyframeIndexToFrame(kIndex);
        if (fabs(kf - frame) < 1) setter->selectKeyframe(kIndex);
      }
    }
  }
}

//-----------------------------------------------------------------------------

void MovePointDragTool::drag(QMouseEvent *e) {
  QPoint pos = e->pos();

  // if shift is pressed then apply constraints (only horizontal or vertical
  // moves)
  if (e->modifiers() & Qt::ShiftModifier) {
    QPoint delta = e->pos() - m_startPos;
    if (abs(delta.x()) > abs(delta.y()))
      pos.setY(m_startPos.y());
    else
      pos.setX(m_startPos.x());
  }

  if (m_groupEnabled) pos.setY(m_startPos.y());

  QPoint oldPos = m_oldPos;
  m_oldPos      = pos;

  // compute frame increment. it must be an integer
  int startRow       = m_panel->xToFrame(m_startPos.x());
  int endRow         = m_panel->xToFrame(pos.x());
  double totalDFrame = endRow - startRow;
  double dFrame      = totalDFrame - m_deltaFrame;

  FunctionSheet *sheet = m_panel->getFunctionSheet();
  TXsheet *xsh         = sheet->getXsheetHandle()->getXsheet();

  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();

    // compute value increment
    double dValue = m_panel->yToValue(curve, pos.y()) -
                    m_panel->yToValue(curve, oldPos.y());

    if (curve->getName() == "W_DrawingNumber") {
      // For Drawing nubers, don't let anything go below 0. Find closest
      // selected frames to 0, that will be the max vertical drop allowed
      if (dValue < 0) {
        double v = INT_MAX;
        for (int i = 0; i < curve->getKeyframeCount(); i++) {
          if (setter->isSelected(i)) {
            v = std::min(v, curve->getKeyframe(i).m_value);
          }
        }
        if (v + dValue < 0) dValue = -v;
      }
      if (dFrame < 0) {
        double h = INT_MAX;
        for (int i = 0; i < curve->getKeyframeCount(); i++) {
          if (setter->isSelected(i)) {
            h = std::min(h, curve->getKeyframe(i).m_frame);
          }
        }
        if (h + dFrame < 0) {
          dFrame = -h;
          totalDFrame = dFrame + m_deltaFrame;
        }
      }
    }

    setter->moveKeyframes(dFrame, dValue);

    int c               = sheet->getColumnIndexByCurve(setter->getCurve());
    TStageObject *stObj = sheet->getStageObject(c);
    int colId           = stObj ? stObj->getId().getIndex() : -1;
    // Only the drawing-number curve, and only if click() really recorded its
    // frames. The test used to be on the COLUMN, while click() fills
    // m_startFrames only for W_DrawingNumber -- so every other curve of a
    // column that happened to have drawing keys walked an EMPTY set kCount
    // times and ran off the end of it. Rare before, because only one curve was
    // ever dragged at a time; picking keys on several curves of such a column
    // hits it immediately.
    if (dFrame && colId >= 0 && curve->getName() == "W_DrawingNumber" &&
        i < (int)m_startFrames.size() && !m_startFrames[i].empty() &&
        m_undoDrawings.find(colId) != m_undoDrawings.end()) {
      int kCount   = setter->getCurve()->getKeyframeCount();
      std::set<double>::iterator sit(m_startFrames[i].begin());
      const std::set<double>::iterator sEnd(m_startFrames[i].end());
      for (int j = 0; j < kCount && sit != sEnd; j++, sit++) {
        if (!setter->isSelected(j)) continue;
        int r = setter->getCurve()->getKeyframe(j).m_frame;
        if (dFrame > 0 && (r - dFrame) < *sit) {
          xsh->restoreDrawings(colId, (r - dFrame), (r - 1), xsh,
                               m_undoDrawings);
        } else if (dFrame < 0 && (r - dFrame) > *sit) {
          xsh->restoreDrawings(colId, (r + 1), (r - dFrame), xsh,
                               m_undoDrawings);
        }
      }
    }
  }

  m_deltaFrame = totalDFrame;

  // Keep the selection on the keys as they move. Every setter, not just the
  // first: with a selection spanning several curves, rebuilding from one of
  // them would drop the others the moment the drag started.
  if (m_selection != 0 && !m_setters.empty()) {
    m_selection->deselectAllKeyframes();
    for (int s = 0; s < (int)m_setters.size(); s++) {
      KeyframeSetter *setter = m_setters[s];
      TDoubleParam *curve    = setter->getCurve();
      for (int i = 0; i < curve->getKeyframeCount(); i++)
        if (setter->isSelected(i)) m_selection->select(curve, i);
    }
  }

  m_panel->update();
}

//-----------------------------------------------------------------------------

void MovePointDragTool::release(QMouseEvent *e) {
  FunctionSheet *sheet = m_panel->getFunctionSheet();
  TXsheet *xsh         = m_panel->getXsheetHandle()->getXsheet();
  for (int i = 0; i < (int)m_setters.size(); i++) {
    KeyframeSetter *setter = m_setters[i];
    TDoubleParam *curve    = setter->getCurve();

    if (curve->getName() != "W_DrawingNumber") continue;
    int c = sheet->getColumnIndexByCurve(curve);
    // getStageObject returns null for an fx parameter, and for the -1 that
    // getColumnIndexByCurve gives back when the curve is not among the active
    // channels -- both of which reach here now that a drag can carry curves
    // the user picked rather than the single one under the cursor.
    TStageObject *stObj = sheet->getStageObject(c);
    if (!stObj || i >= (int)m_startFrames.size()) continue;
    int xcol = stObj->getId().getIndex();

    int kCount = curve->getKeyframeCount();
    for (int j = 0; j < kCount; j++) {
      int frame = curve->getKeyframe(j).m_frame;
      xsh->addUndoDrawingNumberChange(frame, xcol, m_startFrames[i],
                                      m_undoDrawings[xcol]);
    }
  }

  if (m_panel->getXsheetHandle())
    m_panel->getXsheetHandle()->notifyXsheetChanged();
}

//=============================================================================

MoveHandleDragTool::MoveHandleDragTool(FunctionPanel *panel,
                                       TDoubleParam *curve, int kIndex,
                                       Handle handle)
    : m_panel(panel)
    , m_curve(curve)
    , m_kIndex(kIndex)
    , m_handle(handle)
    , m_deltaFrame(0)
    , m_setter(curve, kIndex)
    , m_segmentWidth(0)
    , m_channelGroup(0) {}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::click(QMouseEvent *e) {
  m_startPos = e->pos();
  // m_startPos = m_oldPos = e->pos();
  m_deltaFrame       = 0;
  m_keyframe         = m_curve->getKeyframe(m_kIndex);
  m_keyframe.m_value = m_curve->getValue(m_keyframe.m_frame);
  if (m_handle == FunctionPanel::SpeedIn)
    m_keyframe.m_value = m_curve->getValue(m_keyframe.m_frame, true);

  if (m_channelGroup) {
    for (int i = 0; i < m_channelGroup->getChildCount(); i++) {
      TreeModel::Item *child = m_channelGroup->getChild(i);
      FunctionTreeModel::Channel *channel =
          dynamic_cast<FunctionTreeModel::Channel *>(child);
      if (channel && m_curve != channel->getParam()) {
        if (channel->getParam()->isKeyframe(m_keyframe.m_frame)) {
        }
      }
    }
  }

  if (m_handle == FunctionPanel::EaseInPercentage && m_kIndex > 0) {
    double previousFrame = m_curve->keyframeIndexToFrame(m_kIndex - 1);
    m_segmentWidth       = m_keyframe.m_frame - previousFrame;
  }
  if (m_handle == FunctionPanel::EaseOutPercentage &&
      m_kIndex + 1 < m_curve->getKeyframeCount()) {
    double nextFrame = m_curve->keyframeIndexToFrame(m_kIndex + 1);
    m_segmentWidth   = nextFrame - m_keyframe.m_frame;
  }
  TPointD speed;

  if (m_keyframe.m_linkedHandles) {
    if (m_handle == FunctionPanel::SpeedIn &&
        m_kIndex + 1 < m_curve->getKeyframeCount() &&
        (m_keyframe.m_type != TDoubleKeyframe::SpeedInOut &&
         (m_keyframe.m_type != TDoubleKeyframe::Expression ||
          m_keyframe.m_expressionText.find("cycle") == std::string::npos)))
      speed = m_curve->getSpeedIn(m_kIndex);
    else if (m_handle == FunctionPanel::SpeedOut &&
             m_keyframe.m_prevType != TDoubleKeyframe::SpeedInOut &&
             m_kIndex > 0)
      speed = m_curve->getSpeedOut(m_kIndex);
  }
  if (norm2(speed) > 0.001) {
    QPointF a = m_panel->getWinPos(m_curve, speed) -
                m_panel->getWinPos(m_curve, TPointD());
    double aa = 1.0 / sqrt(a.x() * a.x() + a.y() * a.y());
    m_nSpeed  = QPointF(-a.y() * aa, a.x() * aa);
  } else
    m_nSpeed = QPointF();
  m_setter.setPixelRatio(m_panel->getPixelRatio(m_curve));
}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::drag(QMouseEvent *e) {
  if (!m_curve) return;
  QPoint pos = e->pos();

  // if shift is pressed then apply constraints (only horizontal or vertical
  // moves)
  if (e->modifiers() & Qt::ShiftModifier) {
    QPoint delta = e->pos() - m_startPos;
    if (abs(delta.x()) > abs(delta.y()))
      pos.setY(m_startPos.y());
    else
      pos.setX(m_startPos.x());
  }

  // QPoint oldPos = m_oldPos;
  // m_oldPos = pos;

  QPointF p0 = m_panel->getWinPos(m_curve, m_keyframe);
  QPointF posF(pos);

  if (m_nSpeed != QPointF(0, 0)) {
    QPointF delta = posF - p0;
    posF -= m_nSpeed * (delta.x() * m_nSpeed.x() + delta.y() * m_nSpeed.y());
    if ((m_handle == FunctionPanel::SpeedIn && posF.x() > p0.x()) ||
        (m_handle == FunctionPanel::SpeedOut && posF.x() < p0.x()))
      posF = p0;
  } else {
    if ((m_handle == FunctionPanel::SpeedIn && posF.x() > p0.x()) ||
        (m_handle == FunctionPanel::SpeedOut && posF.x() < p0.x()))
      posF.setX(p0.x());
  }

  double frame = m_panel->xToFrame(posF.x());
  double value = m_panel->yToValue(m_curve, posF.y());
  TPointD handlePos(frame - m_keyframe.m_frame, value - m_keyframe.m_value);
  switch (m_handle) {
  case FunctionPanel::SpeedIn:
    if (m_keyframe.m_type != TDoubleKeyframe::SpeedInOut) {
    }
    m_setter.setSpeedIn(handlePos);
    break;
  case FunctionPanel::SpeedOut:
    m_setter.setSpeedOut(handlePos);
    break;
  case FunctionPanel::EaseIn:
    m_setter.setEaseIn(handlePos.x);
    break;
  case FunctionPanel::EaseOut:
    m_setter.setEaseOut(handlePos.x);
    break;
  case FunctionPanel::EaseInPercentage:
    if (m_segmentWidth > 0)
      m_setter.setEaseIn(100.0 * handlePos.x / m_segmentWidth);
    break;
  case FunctionPanel::EaseOutPercentage:
    if (m_segmentWidth > 0)
      m_setter.setEaseOut(100.0 * handlePos.x / m_segmentWidth);
    break;
  case 100:
  case 101:
  case 102:
    break;
  default:
    assert(0);
  }
  m_panel->update();
}

//-----------------------------------------------------------------------------

void MoveHandleDragTool::release(QMouseEvent *e) {}

//=============================================================================

//-----------------------------------------------------------------------------

MoveGroupHandleDragTool::MoveGroupHandleDragTool(FunctionPanel *panel,
                                                 double keyframePosition,
                                                 Handle handle)
    : m_panel(panel), m_keyframePosition(keyframePosition), m_handle(handle) {
  TUndoManager::manager()->beginBlock();
}

//-----------------------------------------------------------------------------

MoveGroupHandleDragTool::~MoveGroupHandleDragTool() {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::click(QMouseEvent *e) {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();

  FunctionTreeModel *model = m_panel->getModel();
  for (int i = 0; i < model->getActiveChannelCount(); i++) {
    FunctionTreeModel::Channel *channel = model->getActiveChannel(i);
    if (channel && channel->getParam()) {
      TDoubleParam *curve    = channel->getParam();
      int kIndex             = curve->getClosestKeyframe(m_keyframePosition);
      KeyframeSetter *setter = new KeyframeSetter(curve, kIndex);
      setter->setPixelRatio(m_panel->getPixelRatio(curve));
      TDoubleKeyframe kf = curve->getKeyframe(kIndex);
      m_setters.push_back(std::make_pair(kf, setter));
    }
  }
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::drag(QMouseEvent *e) {
  //  if(!m_curve) return;
  QPoint pos = e->pos();
  QPointF posF(pos);

  /*
if(m_nSpeed != QPointF(0,0))
{
QPointF delta = posF-p0;
posF -= m_nSpeed*(delta.x()*m_nSpeed.x()+delta.y()*m_nSpeed.y());
if(  m_handle == FunctionPanel::SpeedIn && posF.x()>p0.x()
|| m_handle == FunctionPanel::SpeedOut && posF.x()<p0.x())
posF = p0;
}
else
{
if(  m_handle == FunctionPanel::SpeedIn && posF.x()>p0.x()
|| m_handle == FunctionPanel::SpeedOut && posF.x()<p0.x())
posF.setX(p0.x());
}
*/

  double frame = m_panel->xToFrame(posF.x());

  for (int i = 0; i < (int)m_setters.size(); i++) {
    TDoubleKeyframe kf     = m_setters[i].first;
    KeyframeSetter *setter = m_setters[i].second;

    if (m_handle == 101)  // why the magic numbers... use enums!
    {
      kf.m_speedOut.x = frame - kf.m_frame;

      switch (kf.m_type) {
      case TDoubleKeyframe::SpeedInOut:
        setter->setSpeedOut(kf.m_speedOut);
        break;
      case TDoubleKeyframe::EaseInOut:
        setter->setEaseOut(kf.m_speedOut.x);
        break;
      default:
        assert(false);
      }
    } else if (m_handle == 102)  // aagghhrrr
    {
      kf.m_speedIn.x = frame - kf.m_frame;

      switch (kf.m_prevType) {
      case TDoubleKeyframe::SpeedInOut:
        setter->setSpeedIn(kf.m_speedIn);
        break;
      case TDoubleKeyframe::EaseInOut:
        setter->setEaseIn(kf.m_speedIn.x);
        break;
      default:
        assert(false);
      }
    }
  }

  /*
switch(m_handle)
{


case FunctionPanel::SpeedIn:
if(m_keyframe.m_type != TDoubleKeyframe::SpeedInOut)
{

}
m_setter.setSpeedIn(handlePos);
break;
case FunctionPanel::SpeedOut:m_setter.setSpeedOut(handlePos); break;
case FunctionPanel::EaseIn:m_setter.setEaseIn(handlePos.x); break;
case FunctionPanel::EaseOut:m_setter.setEaseOut(handlePos.x); break;
case FunctionPanel::EaseInPercentage:
if(m_segmentWidth>0)
m_setter.setEaseIn(100.0*handlePos.x/m_segmentWidth);
break;
case FunctionPanel::EaseOutPercentage:
if(m_segmentWidth>0)
m_setter.setEaseOut(100.0*handlePos.x/m_segmentWidth);
break;
case 100:
case 101:
case 102:
break;
default:assert(0);
}
*/
  m_panel->update();
}

//-----------------------------------------------------------------------------

void MoveGroupHandleDragTool::release(QMouseEvent *e) {
  for (int i = 0; i < (int)m_setters.size(); i++) delete m_setters[i].second;
  m_setters.clear();
}

//=============================================================================

StretchPointDragTool::StretchPointDragTool(FunctionPanel *panel,
                                           TDoubleParam *curve, int leftId,
                                           int rightId, bool moveLeft)
    : m_panel(panel), m_curve(curve), m_moveLeft(moveLeft) {
  // This undo block is closed in the destructor
  TUndoManager::manager()->beginBlock();
  for (int k = leftId; k <= rightId; k++) {
    KeyframeSetter *setter = new KeyframeSetter(curve);
    setter->selectKeyframe(k);
    m_keys.append({k, curve->getKeyframe(k).m_frame,
                   curve->getKeyframe(k).m_speedIn,
                   curve->getKeyframe(k).m_speedOut, setter});
  }
  m_previousRange =
      m_keys.value(rightId).orgFramePos - m_keys.value(leftId).orgFramePos;
}

StretchPointDragTool::~StretchPointDragTool() {
  TUndoManager::manager()->endBlock();
}

void StretchPointDragTool::click(QMouseEvent *e) {
  m_clickedFrame = m_panel->xToFrame(e->pos().x());
}
double StretchPointDragTool::maxAllowedRange() const {
  const double noLimit = 1.0e9;
  if (m_moveLeft) {
    if (m_keys.first().kIndex <= 0) return noLimit;
    return m_keys.last().orgFramePos -
           m_curve->getKeyframe(m_keys.first().kIndex - 1).m_frame - 1.;
  }
  if (m_keys.last().kIndex >= m_curve->getKeyframeCount() - 1) return noLimit;
  return m_curve->getKeyframe(m_keys.last().kIndex + 1).m_frame -
         m_keys.first().orgFramePos - 1.;
}

//-----------------------------------------------------------------------------

void StretchPointDragTool::drag(QMouseEvent *e) {
  double currentPosFrame = m_panel->xToFrame(e->pos().x());

  // moved distance
  double dFrame = currentPosFrame - m_clickedFrame;

  double orgRange = m_keys.last().orgFramePos - m_keys.first().orgFramePos;
  // frame range after stretched
  double stretchedRange = (m_moveLeft) ? orgRange - dFrame : orgRange + dFrame;
  // the frame range should not be smaller than [selected key amount] - 1.
  stretchedRange = std::max(stretchedRange, (double)m_keys.size() - 1.);
  // selection should not extend the neighbor unselected key
  stretchedRange = std::min(stretchedRange, maxAllowedRange());

  applyStretch(
      (m_moveLeft) ? m_keys.last().orgFramePos : m_keys.first().orgFramePos,
      orgRange, stretchedRange);
}

//-----------------------------------------------------------------------------

void StretchPointDragTool::applyStretch(double pivot, double orgRange,
                                        double stretchedRange) {
  if (stretchedRange == m_previousRange) return;
  if (orgRange <= 0.) return;

  // compute the key frame positions (int) after stretching
  QMultiMap<int, int> keyPlacement;  // frame(int) - kIndex multimap

  // if the frame range is equal to [selected key amount] - 1, keys will be
  // "packed" in every frames.
  if ((int)std::round(stretchedRange) == m_keys.size() - 1) {
    int f = (m_moveLeft) ? (int)(pivot - stretchedRange) : (int)pivot;
    for (auto keyInfo : m_keys) {
      keyPlacement.insert(f, keyInfo.kIndex);
      f++;
    }
  } else {  // other cases
    // stretch ratio
    double stretchRatio = stretchedRange / orgRange;
    // compute preferable key frame positions (double) after stretching
    QMap<int, double> stretchedKeyPlacement;  // kIndex - frame(double)
    for (auto keyInfo : m_keys) {
      double sf =
          pivot * (1. - stretchRatio) + keyInfo.orgFramePos * stretchRatio;
      stretchedKeyPlacement.insert(keyInfo.kIndex, sf);
    }

    // now, put keys in integer frame positions, minimizing error.
    // first, put the keys at both ends
    int kFrom = m_keys.first().kIndex;
    int kTo   = m_keys.last().kIndex;
    keyPlacement.insert((int)std::round(stretchedKeyPlacement.value(kFrom)),
                        kFrom);
    keyPlacement.insert((int)std::round(stretchedKeyPlacement.value(kTo)), kTo);

    for (int i = 1; i < m_keys.size() - 1;
         i++) {  // put other intermediate keys
      keyInfo info = m_keys.at(i);
      // if the nearest integer position is vacant, put the key and continue
      int tmp_f = (int)std::round(stretchedKeyPlacement.value(info.kIndex));
      if (!keyPlacement.contains(tmp_f)) {
        keyPlacement.insert(tmp_f, info.kIndex);
        continue;
      }

      // evaluate errors with two candidate - insert key at round down or round
      // up positions. inserting the key may push out the existing key.
      int f1 = (int)std::floor(stretchedKeyPlacement.value(info.kIndex));
      int f2 = f1 + 1;
      {
        bool ok1 = true;
        bool ok2 = true;

        // a list after inserting the key at round down position
        QMultiMap<int, int> candidate1 =
            keyPlacement;  // frame(int) - kIndex multimap
        int moveId = candidate1.value(f1);
        candidate1.insert(f1, info.kIndex);
        while (1) {
          // move the key to the previous frame
          candidate1.remove(f1, moveId);
          // if the frame is vacant, put the key and break
          if (!candidate1.contains(f1 - 1)) {
            candidate1.insert(f1 - 1, moveId);
            break;
          }
          // if the frame is occupied, switch the current moving key and "push
          // out" it
          int occupiedId = candidate1.value(f1 - 1);
          candidate1.insert(f1 - 1, moveId);
          moveId = occupiedId;
          f1 -= 1;

          // the moving key should not be the first one
          if (moveId == kFrom) {
            ok1 = false;
            break;
          }
        }

        // a list after inserting the key at round up position
        QMultiMap<int, int> candidate2 =
            keyPlacement;  // frame(int) - kIndex multimap
        moveId = candidate2.value(f2);
        candidate2.insert(f2, info.kIndex);
        while (1) {
          // move the key to the nexy frame
          candidate2.remove(f2, moveId);
          // if the frame is vacant, put the key and break
          if (!candidate2.contains(f2 + 1)) {
            candidate2.insert(f2 + 1, moveId);
            break;
          }
          // if the frame is occupied, switch the current moving key and "push
          // out" it
          int occupiedId = candidate2.value(f2 + 1);
          candidate2.insert(f2 + 1, moveId);
          moveId = occupiedId;
          f2 += 1;

          // the moving key should not be the last one
          if (moveId == kTo) {
            ok2 = false;
            break;
          }
        }

        if (!ok2)
          keyPlacement = candidate1;
        else if (!ok1)
          keyPlacement = candidate2;
        else {
          double error1                         = 0.;
          QMultiMap<int, int>::const_iterator i = candidate1.constBegin();
          while (i != candidate1.constEnd()) {
            error1 += std::abs((double)i.key() -
                               stretchedKeyPlacement.value(i.value()));
            ++i;
          }
          double error2 = 0.;
          i             = candidate2.constBegin();
          while (i != candidate2.constEnd()) {
            error2 += std::abs((double)i.key() -
                               stretchedKeyPlacement.value(i.value()));
            ++i;
          }
          if (error1 <= error2)
            keyPlacement = candidate1;
          else
            keyPlacement = candidate2;
        }
      }
    }
  }
  // each setter will move key frame
  bool extending = stretchedRange > m_previousRange;

  QList<double> segRatio;
  for (int i = 0; i < m_keys.size() - 1; i++) {
    double orgSegLength =
        m_keys.at(i + 1).orgFramePos - m_keys.at(i).orgFramePos;
    double newSegLength = (double)(keyPlacement.key(m_keys.at(i + 1).kIndex) -
                                   keyPlacement.key(m_keys.at(i).kIndex));
    segRatio.append(newSegLength / orgSegLength);
  }

  // dragging left + shrink or dragging right + extend cases
  // move from right key to left key
  int start = (m_moveLeft != extending) ? m_keys.size() - 1 : 0;
  int end   = (m_moveLeft != extending) ? -1 : m_keys.size();
  int dki   = (m_moveLeft != extending) ? -1 : 1;
  for (int ki = start; ki != end; ki += dki) {
    int kId      = m_keys[ki].kIndex;
    int curFrame = (int)std::round(m_curve->getKeyframe(kId).m_frame);
    int dstFrame = keyPlacement.key(kId);
    if (curFrame != dstFrame) {
      m_keys[ki].setter->moveKeyframes(dstFrame - curFrame, 0.);
    }
    m_keys[ki].setter->selectKeyframe(kId);
    if (ki != 0 && segRatio[ki - 1] != 1.) {
      if (m_keys[ki].setter->isSpeedInOut(kId - 1))
        m_keys[ki].setter->setSpeedIn(
            TPointD(m_keys[ki].orgSpeedIn.x * segRatio[ki - 1],
                    m_keys[ki].orgSpeedIn.y));
      else if (m_keys[ki].setter->isEaseInOut(kId - 1))
        m_keys[ki].setter->setEaseIn(m_keys[ki].orgSpeedIn.x *
                                     segRatio[ki - 1]);
    }
    if (ki != m_keys.size() - 1 && segRatio[ki] != 1.) {
      if (m_keys[ki].setter->isSpeedInOut(kId))
        m_keys[ki].setter->setSpeedOut(TPointD(
            m_keys[ki].orgSpeedOut.x * segRatio[ki], m_keys[ki].orgSpeedOut.y));
      else if (m_keys[ki].setter->isEaseInOut(kId))
        m_keys[ki].setter->setEaseOut(m_keys[ki].orgSpeedOut.x * segRatio[ki]);
    }
  }

  m_previousRange = stretchedRange;
  m_panel->update();
}

//=============================================================================

MultiStretchDragTool::MultiStretchDragTool(FunctionPanel *panel,
                                           FunctionSelection *selection,
                                           bool moveLeft)
    : m_panel(panel)
    , m_moveLeft(moveLeft)
    , m_clickedFrame(0)
    , m_pivot(0)
    , m_orgRange(0)
    , m_previousRange(0) {
  if (!selection) return;

  double first = 0, last = 0;
  bool haveBounds = false;

  for (TDoubleParam *curve : selection->getSelectedCurves()) {
    if (!curve) continue;
    QList<int> indices = selection->getSelectedKeyIndices(curve);
    // Each curve needs at least two keys, and they must be consecutive: the
    // stretch redistributes a run of keys, and a run with holes in it is not
    // something the packing below can place.
    if (indices.count() < 2) continue;
    if (indices.last() - indices.first() != indices.count() - 1) continue;

    StretchPointDragTool *tool = new StretchPointDragTool(
        panel, curve, indices.first(), indices.last(), moveLeft);
    m_tools.append(tool);

    if (!haveBounds) {
      first      = tool->firstOrgFrame();
      last       = tool->lastOrgFrame();
      haveBounds = true;
    } else {
      first = std::min(first, tool->firstOrgFrame());
      last  = std::max(last, tool->lastOrgFrame());
    }
  }

  if (!haveBounds) return;

  // ONE pivot and ONE original range for the lot, taken from the outermost
  // keys of the whole selection. Letting each curve use its own ends would
  // scale each about a different point, and curves that started together
  // would come apart.
  m_pivot         = moveLeft ? last : first;
  m_orgRange      = last - first;
  m_previousRange = m_orgRange;
}

//-----------------------------------------------------------------------------

MultiStretchDragTool::~MultiStretchDragTool() {
  for (StretchPointDragTool *tool : m_tools) delete tool;
  m_tools.clear();
}

//-----------------------------------------------------------------------------

void MultiStretchDragTool::click(QMouseEvent *e) {
  m_clickedFrame = m_panel->xToFrame(e->pos().x());
  for (StretchPointDragTool *tool : m_tools) tool->click(e);
}

//-----------------------------------------------------------------------------

void MultiStretchDragTool::drag(QMouseEvent *e) {
  const double dFrame = m_panel->xToFrame(e->pos().x()) - m_clickedFrame;

  double stretchedRange = m_moveLeft ? m_orgRange - dFrame : m_orgRange + dFrame;

  // The floor is the widest curve's key count: pack tighter than that and two
  // of its keys would want the same frame. The ceiling is the tightest of the
  // curves' own neighbour limits, so no curve runs over the key just outside
  // its selection.
  double minRange = 0.0;
  double maxRange = 1.0e9;
  for (StretchPointDragTool *tool : m_tools) {
    minRange = std::max(minRange, (double)tool->keyCount() - 1.);
    maxRange = std::min(maxRange, tool->maxAllowedRange());
  }
  stretchedRange = std::max(stretchedRange, minRange);
  stretchedRange = std::min(stretchedRange, maxRange);

  if (stretchedRange == m_previousRange) return;

  for (StretchPointDragTool *tool : m_tools)
    tool->applyStretch(m_pivot, m_orgRange, stretchedRange);

  m_previousRange = stretchedRange;
  m_panel->update();
}

//-----------------------------------------------------------------------------

void MultiStretchDragTool::release(QMouseEvent *e) {
  for (StretchPointDragTool *tool : m_tools) tool->release(e);
}

//=============================================================================

void StretchPointDragTool::release(QMouseEvent *e) {
  for (int i = 0; i < (int)m_keys.size(); i++) delete m_keys[i].setter;
  m_keys.clear();
}
