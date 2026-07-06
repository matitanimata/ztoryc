

// TnzCore includes
#include "tundo.h"

// TnzLib includes
#include "toonz/ikccd.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"

#include "plastictool.h"

#include <cmath>
#include <limits>

using namespace PlasticToolLocals;

//****************************************************************************************
//    Undo  definitions
//****************************************************************************************

namespace {

class AnimateValuesUndo final : public TUndo {
  int m_row, m_col;  //!< Xsheet coordinates
  int m_v;           //!< Moved vertex

public:
  SkDKey m_oldValues, m_newValues;  //!< Keyframe values

public:
  AnimateValuesUndo(int v) : m_row(::row()), m_col(::column()), m_v(v) {}

  // Again, not accurate. We should get in the details of SkDF... So, let's say
  // around 10 kB - max 10k instances in the standard undos pool.
  int getSize() const override { return 10 << 10; }

  void redo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    if (m_v >= 0) l_plasticTool.setSkeletonSelection(m_v);

    l_suspendParamsObservation =
        true;  // Coalesce params change notifications into one

    l_plasticTool.deformation()->deleteKeyframe(m_row - 1);
    l_plasticTool.deformation()->setKeyframe(m_newValues);

    l_suspendParamsObservation = false;
    l_plasticTool.onChange();
  }

  void undo() const override {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);

    if (m_v >= 0) l_plasticTool.setSkeletonSelection(m_v);

    l_suspendParamsObservation =
        true;  // Coalesce params change notifications into one

    l_plasticTool.deformation()->deleteKeyframe(
        m_row - 1);  // Yep. Typical frame/row shift... xD
    l_plasticTool.deformation()->setKeyframe(m_oldValues);

    l_suspendParamsObservation = false;
    l_plasticTool.onChange();
  }

  QString getHistoryString() override { return "Plastic (animate): Animate Vertex"; }
};

}  // namespace

//****************************************************************************************
//    PlasticTool  functions
//****************************************************************************************

void PlasticTool::mouseMove_animate(const TPointD &pos, const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;  // Needs to be done now - ensures m_pos is valid

  m_svHigh = m_seHigh = -1;  // Reset highlighted primitives

  if (m_sd) {
    double d, highlightRadius = getPixelSize() * HIGHLIGHT_DISTANCE;

    // Look for nearest vertex
    int v = deformedSkeleton().closestVertex(pos, &d);
    if (v >= 0 && d < highlightRadius) m_svHigh = v;

    invalidate();
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  // Track mouse position
  m_pressedPos = m_pos = pos;

  setSkeletonSelection(m_svHigh);

  if (m_svSel.hasSingleObject()) {
    // Store original vertex position and keyframe values
    m_pressedVxsPos =
        std::vector<TPointD>(1, deformedSkeleton().vertex(m_svSel).P());
    m_sd->getKeyframeAt(frame(), m_pressedSkDF);
  }

  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDrag_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;

  if (m_sd && m_svSel.hasSingleObject() &&
      m_svSel > 0)  // Avoid move if vertex is root
  {
    l_suspendParamsObservation = true;  // Automatic params notification happen
    // twice (1 x param) - dealing with it manually

    double frame = ::frame();

    // First, retrieve selected vertex's deformation
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), m_svSel);
    assert(vd);

    // Move selected branch
    if (m_ikDrag.getValue() && deformedSkeleton().vertex(m_svSel).parent() >= 0) {
      moveVertexIK_animate(frame, m_svSel, pos);
    } else if (m_keepDistance.getValue()) {
      ::setKeyframe(vd->m_params[SkVD::ANGLE],
                    frame);  // Set a keyframe for it. It must be done
                             // to set the correct function interpolation
                             // type and other stuff.
      m_sd->updateAngle(*skeleton(), deformedSkeleton(), frame, m_svSel, pos);
    } else {
      ::setKeyframe(vd->m_params[SkVD::ANGLE],
                    frame);  // Same here. NOTE: Not setting a frame on
      ::setKeyframe(vd->m_params[SkVD::DISTANCE],
                    frame);  // vd directly due to SkVD::SO

      m_sd->updatePosition(*skeleton(), deformedSkeleton(), frame, m_svSel,
                           pos);
    }

    l_suspendParamsObservation = false;

    // onChange();                                                     // Due to
    // a nasty Function Editor dependency,
    // it's better to call the following directly
    m_deformedSkeleton.invalidate();
    invalidate();
  }
}

//------------------------------------------------------------------------

// SuperPlastic adapter: solves the whole root -> v chain against the mouse
// position with the shared CCD solver (toonz/ikccd.h), then writes back the
// per-vertex ANGLE deformation params only — distances are never altered, so
// the rig keeps its proportions. Children hanging off the chain follow via
// the ordinary FK rebuild of the deformed skeleton.
void PlasticTool::moveVertexIK_animate(double frame, int v,
                                       const TPointD &pos) {
  const PlasticSkeleton &orig    = *skeleton();
  PlasticSkeleton &defSkel       = deformedSkeleton();
  const double quietNaN          = std::numeric_limits<double>::quiet_NaN();
  const double noBoundThreshold  = 1e9;  // vertex bounds default to +-DBL_MAX

  // Serial chain from the skeleton root down to the dragged vertex
  std::vector<int> chainVx;
  for (int cur = v; cur >= 0; cur = defSkel.vertex(cur).parent())
    chainVx.push_back(cur);
  std::reverse(chainVx.begin(), chainVx.end());

  int boneCount = int(chainVx.size()) - 1;
  if (boneCount < 1) return;

  // Absolute direction angle of the polar reference for a vertex, replicating
  // updateBranchPositions(): the parent bone direction, walking further up on
  // degenerate (zero-length) bones, world +x at the root.
  auto parentDirAngle = [](const PlasticSkeleton &skel, int vx) -> double {
    for (int p = vx; p >= 0;) {
      int pp = skel.vertex(p).parent();
      if (pp < 0) return 0.0;
      TPointD d = skel.vertex(p).P() - skel.vertex(pp).P();
      if (norm2(d) > 1e-8) return atan2(d.y, d.x);
      p = pp;
    }
    return 0.0;
  };
  auto wrapAngle = [](double a) {
    while (a > M_PI) a -= 2.0 * M_PI;
    while (a < -M_PI) a += 2.0 * M_PI;
    return a;
  };

  // Rest angles come from the ORIGINAL skeleton: the deformed bone direction
  // is Rot(rest + ANGLE param) * parentDir, so solver-space bone angles map
  // to ANGLE params as (solved relative angle - rest angle).
  IKChain chain;
  std::vector<double> restAngle(boneCount);
  chain.currentPositions.reserve(boneCount + 1);
  for (int i = 0; i <= boneCount; ++i)
    chain.currentPositions.push_back(defSkel.vertex(chainVx[i]).P());

  for (int i = 0; i < boneCount; ++i) {
    const PlasticSkeletonVertex &ovx = orig.vertex(chainVx[i + 1]);
    TPointD od   = ovx.P() - orig.vertex(chainVx[i]).P();
    double rest  = wrapAngle(atan2(od.y, od.x) - parentDirAngle(orig, chainVx[i]));
    restAngle[i] = rest;

    IKBone bone;
    bone.length      = norm(chain.currentPositions[i + 1] -
                            chain.currentPositions[i]);
    bone.parentIndex = i - 1;
    bone.angleMin    = (ovx.m_minAngle < -noBoundThreshold)
                           ? quietNaN
                           : rest + ovx.m_minAngle * M_PI_180;
    bone.angleMax    = (ovx.m_maxAngle > noBoundThreshold)
                           ? quietNaN
                           : rest + ovx.m_maxAngle * M_PI_180;
    chain.bones.push_back(bone);
  }

  IKTarget target;
  target.position = pos;

  std::vector<TPointD> solved =
      SolveIK_CCD(chain, target, 30, 0.5 * getPixelSize());

  // Write back the ANGLE params along the chain (root vertex excluded: it has
  // no ANGLE). Values are kept within 180 degrees of the previous ones so the
  // Function Editor curves don't jump by full turns.
  double prevAbs = 0.0;
  for (int i = 0; i < boneCount; ++i) {
    TPointD d      = solved[i + 1] - solved[i];
    double absAng  = (norm2(d) > 1e-8) ? atan2(d.y, d.x) : prevAbs;
    double newDelta = wrapAngle((absAng - prevAbs) - restAngle[i]) * M_180_PI;
    prevAbs        = absAng;

    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), chainVx[i + 1]);
    assert(vd);
    double oldDelta = vd->m_params[SkVD::ANGLE]->getValue(frame);
    while (newDelta - oldDelta > 180.0) newDelta -= 360.0;
    while (newDelta - oldDelta < -180.0) newDelta += 360.0;

    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    vd->m_params[SkVD::ANGLE]->setValue(frame, newDelta);
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp_animate(const TPointD &pos,
                                       const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;

  if (m_svSel.hasSingleObject() && m_dragged) {
    // Set a keyframe to each skeleton vertex, if that was requested
    if (m_globalKey.getValue())
      ::setKeyframe(m_sd, ::frame());  // Already invokes keyframes rebuild
    else
      stageObject()->updateKeyframes();  // Otherwise, must be explicit

    // Add a corresponding undo
    AnimateValuesUndo *undo = new AnimateValuesUndo(m_svSel);

    undo->m_oldValues = m_pressedSkDF;
    m_sd->getKeyframeAt(frame(), undo->m_newValues);

    TUndoManager::manager()->add(undo);

    m_dragged = false; // Turn this off now so toolbar updates

    // This is needed to refresh the xsheet (there may be new keyframes)
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
  }

  // In case one of the vertices is attached (as a hook) to an external
  // position,
  // we need to update the whole skeleton according to the updated vertex.
  updateMatrix();
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::addContextMenuActions_animate(QMenu *menu) {
  bool ret = true;

  if (m_sd.getPointer() == nullptr) return;

  QAction *action;
  if (!m_svSel.isEmpty()) {
    action = CommandManager::instance()->getAction(MI_SetKeyframes);
    menu->addAction(action);

  action = CommandManager::instance()->getAction(MI_SetRestKeyframes);
    menu->addAction(action);
  }

  action = CommandManager::instance()->getAction(MI_SetGlobalKeyframes);
  menu->addAction(action);

  action = CommandManager::instance()->getAction(MI_SetGlobalRestKeyframes);
  menu->addAction(action);

  menu->addSeparator();

  assert(ret);
}

//------------------------------------------------------------------------

void PlasticTool::keyFunc_undo(void (PlasticTool::*keyFunc)()) {
  assert(m_svSel.objects().size() <= 1);

  // Guard: the Set Key / Set Rest Key actions (added upstream for the Plastic
  // Tool) are reachable via shortcut even when there is no active deformation
  // (m_sd == nullptr, e.g. the tool is active but no skeleton/mesh is current),
  // which crashed in m_sd->getKeyframeAt(). Bail out instead of dereferencing.
  if (!m_sd) return;

  double frame = ::frame();

  AnimateValuesUndo *undo = new AnimateValuesUndo(m_svSel);
  m_sd->getKeyframeAt(frame, undo->m_oldValues);

  (this->*keyFunc)();

  m_sd->getKeyframeAt(frame, undo->m_newValues);

  TUndoManager::manager()->add(undo);
}

//------------------------------------------------------------------------

void PlasticTool::draw_animate() {
  double pixelSize = getPixelSize();

  PlasticSkeleton &deformedSkeleton = this->deformedSkeleton();

  // Draw deformed skeleton
  if (m_sd) {
    drawOnionSkinSkeletons_animate(pixelSize);
    drawSkeleton(deformedSkeleton, pixelSize);
    drawSelections(m_sd, deformedSkeleton, pixelSize);
    drawAngleLimits(m_sd, m_skelId, m_svSel, pixelSize);
  }

  drawHighlights(m_sd, &deformedSkeleton, pixelSize);
}
