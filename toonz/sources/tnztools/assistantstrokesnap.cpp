#include "assistantstrokesnap.h"

// TnzTools includes
#include "tools/tool.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"

#include <QAction>

//-----------------------------------------------------------------------------

bool AssistantStrokeSnap::begin(TTool *tool, const TThickPoint &pos) {
  end();
  m_tool          = tool;
  TPointD position = TPointD(pos.x, pos.y);
  TAssistant::scanAssistants(tool, &position, 1, &m_guidelines, false, true,
                             true, false, nullptr);
  if (m_guidelines.empty()) {
    m_tool = nullptr;
    return false;
  }
  m_current = m_guidelines.front();
  m_locked  = m_guidelines.size() <= 1;
  add(pos);
  return true;
}

//-----------------------------------------------------------------------------

bool AssistantStrokeSnap::add(const TThickPoint &raw) {
  if (!isActive()) return false;
  m_raw.push_back(raw);
  if (m_locked) return false;

  if (!m_track) m_track.reset(new TTrack());
  m_track->push_back(TTrackPoint(TPointD(raw.x, raw.y)), true);
  bool longEnough  = false;
  TGuidelineP best =
      TGuideline::findBest(m_guidelines, *m_track, trackToScreen(), longEnough);
  // Once the gesture is long enough the choice is final: a guideline that
  // changed mid-stroke would make the line jump under the pen.
  if (longEnough) m_locked = true;
  if (!best || best == m_current) return false;
  m_current = best;
  return true;
}

//-----------------------------------------------------------------------------

TThickPoint AssistantStrokeSnap::transform(const TThickPoint &raw) const {
  if (!isActive() || !m_current) return raw;
  TTrackPoint p =
      m_current->smoothTransformPoint(TTrackPoint(TPointD(raw.x, raw.y)));
  return TThickPoint(p.position, raw.thick);
}

//-----------------------------------------------------------------------------

void AssistantStrokeSnap::end() {
  m_tool = nullptr;
  m_guidelines.clear();
  m_current.reset();
  m_track.reset();
  m_raw.clear();
  m_locked = false;
}

//-----------------------------------------------------------------------------

bool AssistantStrokeSnap::guidesShown() {
  QAction *action =
      CommandManager::instance()->getAction("MI_ShowPerspectiveGrids");
  return !action || action->isChecked();
}

//-----------------------------------------------------------------------------

bool AssistantStrokeSnap::hoverNeedsFullRepaint(TTool *tool) {
  return guidesShown() &&
         TAssistant::scanAssistants(tool, nullptr, 0, nullptr, false, false,
                                    false, false, nullptr);
}

//-----------------------------------------------------------------------------

void AssistantStrokeSnap::drawGuidelines() const {
  if (!isActive() || !guidesShown()) return;
  for (const TGuidelineP &guideline : m_guidelines)
    if (guideline != m_current) guideline->draw();
  if (m_current) m_current->draw(true);
}

//-----------------------------------------------------------------------------

void AssistantStrokeSnap::drawHoverGuidelines(TTool *tool, const TPointD &pos) {
  if (!guidesShown()) return;
  TAssistant::scanAssistants(tool, &pos, 1, nullptr, false, false, true, true,
                             nullptr);
}

//-----------------------------------------------------------------------------

// Guidelines are chosen on what the gesture looks like on screen, so the
// track (level units, divided by the dpi like the assistants' worldToTrack)
// is brought to viewer pixels.
TAffine AssistantStrokeSnap::trackToScreen() const {
  TTool::Viewer *viewer = m_tool ? m_tool->getViewer() : nullptr;
  if (!viewer) return TAffine();
  TAffine toWorld = m_tool->getMatrix();
  if (m_tool->getToolType() & TTool::LevelTool)
    if (TObjectHandle *objHandle =
            TTool::getApplication()->getCurrentObject())
      if (!objHandle->isSpline())
        toWorld = toWorld * TScale(viewer->getDpiScale().x,
                                   viewer->getDpiScale().y);
  return viewer->getViewMatrix() * toWorld;
}
