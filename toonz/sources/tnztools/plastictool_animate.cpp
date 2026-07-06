

// TnzCore includes
#include "tundo.h"

// TnzLib includes
#include "toonz/ikccd.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"

#include "plastictool.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <vector>

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

namespace {

inline double wrapPi(double a) {
  while (a > M_PI) a -= 2.0 * M_PI;
  while (a < -M_PI) a += 2.0 * M_PI;
  return a;
}

}  // namespace

//------------------------------------------------------------------------

// Solves the serial chain \a chainVx (base = chainVx[0], kept fixed) so its
// last vertex reaches \a target, then writes back the ANGLE deformation params
// of the chain vertices (distances untouched). ANGLE is re-derived from each
// vertex's solved position relative to its ORIGINAL-tree parent, so the result
// stays valid even when the chain traverses tree edges backwards (as in the
// pin-restore stage). Angle bounds are honored only when \a useLimits is set
// (the root->handle stage), where every chain edge runs parent->child.
void PlasticTool::solveChainIK_animate(const std::vector<int> &chainVx,
                                       const TPointD &target, double frame,
                                       bool useLimits) {
  int boneCount = int(chainVx.size()) - 1;
  if (boneCount < 1) return;

  const PlasticSkeleton &orig = *skeleton();
  PlasticSkeleton &defSkel    = deformedSkeleton();

  IKChain chain;
  const double quietNaN = std::numeric_limits<double>::quiet_NaN();
  const double bigBound = 1e9;  // vertex bounds default to +-DBL_MAX
  for (int i = 0; i <= boneCount; ++i)
    chain.currentPositions.push_back(defSkel.vertex(chainVx[i]).P());

  for (int i = 0; i < boneCount; ++i) {
    IKBone bone;
    bone.length      = norm(chain.currentPositions[i + 1] -
                            chain.currentPositions[i]);
    bone.parentIndex = i - 1;
    bone.angleMin = bone.angleMax = quietNaN;
    if (useLimits) {
      const PlasticSkeletonVertex &ovx = orig.vertex(chainVx[i + 1]);
      TPointD od  = orig.vertex(chainVx[i + 1]).P() - orig.vertex(chainVx[i]).P();
      auto pdir   = [&](int vx) {
        for (int p = vx; p >= 0;) {
          int pp = orig.vertex(p).parent();
          if (pp < 0) return 0.0;
          TPointD dd = orig.vertex(p).P() - orig.vertex(pp).P();
          if (norm2(dd) > 1e-8) return atan2(dd.y, dd.x);
          p = pp;
        }
        return 0.0;
      };
      double rest   = wrapPi(atan2(od.y, od.x) - pdir(chainVx[i]));
      bone.angleMin = (ovx.m_minAngle < -bigBound)
                          ? quietNaN
                          : rest + ovx.m_minAngle * M_PI_180;
      bone.angleMax = (ovx.m_maxAngle > bigBound)
                          ? quietNaN
                          : rest + ovx.m_maxAngle * M_PI_180;
    }
    chain.bones.push_back(bone);
  }

  IKTarget ikTarget;
  ikTarget.position = target;
  std::vector<TPointD> solved =
      SolveIK_CCD(chain, ikTarget, 30, 0.5 * getPixelSize());

  // New positions for the chain vertices; everything else keeps its current
  // deformed position (used as parent reference during write-back).
  std::map<int, TPointD> np;
  for (int i = 0; i <= boneCount; ++i) np[chainVx[i]] = solved[i];
  auto newPos = [&](int idx) {
    auto it = np.find(idx);
    return (it != np.end()) ? it->second : defSkel.vertex(idx).P();
  };
  // Absolute direction of a vertex's polar reference, using \a posFn positions.
  auto parentDir = [&](const std::function<TPointD(int)> &posFn, int vx) {
    for (int p = vx; p >= 0;) {
      int pp = orig.vertex(p).parent();
      if (pp < 0) return 0.0;
      TPointD dd = posFn(p) - posFn(pp);
      if (norm2(dd) > 1e-8) return atan2(dd.y, dd.x);
      p = pp;
    }
    return 0.0;
  };
  std::function<TPointD(int)> origFn = [&](int idx) {
    return orig.vertex(idx).P();
  };
  std::function<TPointD(int)> newFn = [&](int idx) { return newPos(idx); };

  for (int i = 1; i <= boneCount; ++i) {
    int w  = chainVx[i];
    int op = orig.vertex(w).parent();
    if (op < 0) continue;  // skeleton root has no ANGLE param
    TPointD od  = orig.vertex(w).P() - orig.vertex(op).P();
    double rest = wrapPi(atan2(od.y, od.x) - parentDir(origFn, op));
    TPointD nd  = newPos(w) - newPos(op);
    double nAbs = (norm2(nd) > 1e-8) ? atan2(nd.y, nd.x) : 0.0;
    double newDelta = wrapPi(nAbs - parentDir(newFn, op) - rest) * M_180_PI;

    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), w);
    if (!vd) continue;
    double oldDelta = vd->m_params[SkVD::ANGLE]->getValue(frame);
    while (newDelta - oldDelta > 180.0) newDelta -= 360.0;
    while (newDelta - oldDelta < -180.0) newDelta += 360.0;

    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    vd->m_params[SkVD::ANGLE]->setValue(frame, newDelta);
  }
}

//------------------------------------------------------------------------

// SuperPlastic IK adapter (Animate mode). Writes back only per-vertex ANGLE
// params (distances untouched) plus the free-root offset — no pin means a plain
// root -> handle serial solve (angle bounds honored).
//
// With a pin, the PINNED VERTEX BECOMES THE ROOT of the hierarchy for this
// drag: it is the fixed base, the dragged handle reaches the mouse, and every
// other vertex — including the skeleton's own root — follows freely. The result
// is expressed back in the fixed root-down parameterization the deformer uses:
// each vertex's ANGLE from its solved position, and the original root placed via
// its ROOTX/ROOTY offset. That offset is exactly what lets the original root
// move like any other vertex while the pin stays planted.
void PlasticTool::moveVertexIK_animate(double frame, int v,
                                       const TPointD &pos) {
  if (!m_sd || !skeleton()) return;
  const PlasticSkeleton &orig = *skeleton();

  int pin = pinnedVertexAtFrame(frame);
  if (pin < 0 || pin == v) {
    // No pin: root-anchored serial solve.
    PlasticSkeleton &defSkel = deformedSkeleton();
    std::vector<int> chainVx;
    for (int cur = v; cur >= 0; cur = defSkel.vertex(cur).parent())
      chainVx.push_back(cur);
    std::reverse(chainVx.begin(), chainVx.end());  // root ... v
    solveChainIK_animate(chainVx, pos, frame, /*useLimits*/ true);
    return;
  }

  // ---- Re-root at the pin ----
  PlasticSkeleton &defSkel = deformedSkeleton();
  std::map<int, TPointD> curPos;
  int rootIdx = -1;
  for (auto vt = defSkel.vertices().begin(); vt != defSkel.vertices().end();
       ++vt) {
    curPos[vt.m_idx] = vt->P();
    if (vt->parent() < 0) rootIdx = vt.m_idx;
  }

  // Undirected adjacency, then BFS from the pin: re-rooted parent + order.
  std::map<int, std::vector<int>> adj;
  const auto &edges = defSkel.edges();
  for (auto et = edges.begin(); et != edges.end(); ++et) {
    adj[et->vertex(0)].push_back(et->vertex(1));
    adj[et->vertex(1)].push_back(et->vertex(0));
  }
  std::map<int, int> reParent;
  std::vector<int> order;
  {
    std::set<int> visited;
    std::queue<int> q;
    q.push(pin);
    visited.insert(pin);
    reParent[pin] = -1;
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      order.push_back(u);
      for (int w : adj[u])
        if (!visited.count(w)) {
          visited.insert(w);
          reParent[w] = u;
          q.push(w);
        }
    }
    if (!visited.count(v)) return;  // disconnected
  }

  // Path pin -> handle, solved with CCD keeping the pin fixed.
  std::vector<int> path;
  for (int cur = v; cur >= 0; cur = reParent[cur]) path.push_back(cur);
  std::reverse(path.begin(), path.end());  // path[0] == pin
  int boneCount = int(path.size()) - 1;
  if (boneCount < 1) return;

  IKChain chain;
  for (int i = 0; i <= boneCount; ++i)
    chain.currentPositions.push_back(curPos[path[i]]);
  for (int i = 0; i < boneCount; ++i) {
    IKBone bone;
    bone.length      = norm(chain.currentPositions[i + 1] -
                            chain.currentPositions[i]);
    bone.parentIndex = i - 1;
    bone.angleMin = bone.angleMax = std::numeric_limits<double>::quiet_NaN();
    chain.bones.push_back(bone);
  }
  IKTarget ikTarget;
  ikTarget.position = pos;
  std::vector<TPointD> solved =
      SolveIK_CCD(chain, ikTarget, 30, 0.5 * getPixelSize());

  // Desired positions: pin fixed, path from CCD, everything else follows its
  // re-rooted parent rigidly (unmoved branches keep their relative shape).
  std::map<int, TPointD> desired = curPos;
  std::map<int, double> frameRot;
  frameRot[pin] = 0.0;
  for (int i = 1; i <= boneCount; ++i) {
    desired[path[i]] = solved[i];
    TPointD od       = curPos[path[i]] - curPos[path[i - 1]];
    TPointD nd       = solved[i] - solved[i - 1];
    frameRot[path[i]] = wrapPi(atan2(nd.y, nd.x) - atan2(od.y, od.x));
  }
  for (int u : order) {
    if (frameRot.count(u)) continue;  // pin or path
    int rp  = reParent[u];
    double r = frameRot[rp];
    double c = cos(r), s = sin(r);
    TPointD off  = curPos[u] - curPos[rp];
    desired[u]   = desired[rp] + TPointD(c * off.x - s * off.y,
                                         s * off.x + c * off.y);
    frameRot[u]  = r;
  }

  // ---- Write-back into the fixed root-down parameterization ----
  int skelId = ::skeletonId();

  // Original root placed via its offset (moves freely like any other vertex).
  if (rootIdx >= 0) {
    SkVD *rvd = m_sd->vertexDeformation(skelId, rootIdx);
    if (rvd && rvd->m_params[SkVD::ROOTX] && rvd->m_params[SkVD::ROOTY]) {
      TPointD offNew = desired[rootIdx] - orig.vertex(rootIdx).P();
      ::setKeyframe(rvd->m_params[SkVD::ROOTX], frame);
      rvd->m_params[SkVD::ROOTX]->setValue(frame, offNew.x);
      ::setKeyframe(rvd->m_params[SkVD::ROOTY], frame);
      rvd->m_params[SkVD::ROOTY]->setValue(frame, offNew.y);
    }
  }

  // ANGLE of every non-root vertex whose relative angle actually changed.
  auto parentDir = [&](const std::function<TPointD(int)> &posFn, int vx) {
    for (int p = vx; p >= 0;) {
      int pp = orig.vertex(p).parent();
      if (pp < 0) return 0.0;
      TPointD dd = posFn(p) - posFn(pp);
      if (norm2(dd) > 1e-8) return atan2(dd.y, dd.x);
      p = pp;
    }
    return 0.0;
  };
  std::function<TPointD(int)> origFn = [&](int idx) {
    return orig.vertex(idx).P();
  };
  std::function<TPointD(int)> desFn = [&](int idx) { return desired[idx]; };

  for (auto &kv : curPos) {
    int w = kv.first;
    if (w == rootIdx) continue;
    int op = orig.vertex(w).parent();
    if (op < 0) continue;
    TPointD od  = orig.vertex(w).P() - orig.vertex(op).P();
    double rest = wrapPi(atan2(od.y, od.x) - parentDir(origFn, op));
    TPointD nd  = desired[w] - desired[op];
    double nAbs = (norm2(nd) > 1e-8) ? atan2(nd.y, nd.x) : 0.0;
    double newDelta = wrapPi(nAbs - parentDir(desFn, op) - rest) * M_180_PI;

    SkVD *vd = m_sd->vertexDeformation(skelId, w);
    if (!vd) continue;
    double oldDelta = vd->m_params[SkVD::ANGLE]->getValue(frame);
    while (newDelta - oldDelta > 180.0) newDelta -= 360.0;
    while (newDelta - oldDelta < -180.0) newDelta += 360.0;
    if (fabs(newDelta - oldDelta) < 1e-4) continue;  // rigidly-following branch

    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    vd->m_params[SkVD::ANGLE]->setValue(frame, newDelta);
  }
}

//------------------------------------------------------------------------

int PlasticTool::pinnedVertexAtFrame(double frame) const {
  if (!m_sd) return -1;
  PlasticSkeletonP skel = skeleton();
  if (!skel) return -1;
  int skelId = ::skeletonId();
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end(); ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (vd && vd->m_params[SkVD::PIN]->getValue(frame) >= 0.5) return vt.m_idx;
  }
  return -1;
}

//------------------------------------------------------------------------

void PlasticTool::togglePinAtCurrentFrame() {
  if (!m_sd || !m_svSel.hasSingleObject()) return;
  double frame = ::frame();
  int v        = m_svSel;
  SkVD *vd     = m_sd->vertexDeformation(::skeletonId(), v);
  if (!vd) return;

  // Snapshot for undo (captures the PIN param, part of the SkVD keyframe).
  AnimateValuesUndo *undo = new AnimateValuesUndo(v);
  m_sd->getKeyframeAt(frame, undo->m_oldValues);

  bool pinned = vd->m_params[SkVD::PIN]->getValue(frame) >= 0.5;
  // Constant (step) interpolation so the anchor holds between keyframes.
  TDoubleKeyframe kf(frame, pinned ? 0.0 : 1.0);
  kf.m_type = kf.m_prevType = TDoubleKeyframe::Constant;
  vd->m_params[SkVD::PIN]->setKeyframe(kf);

  m_sd->getKeyframeAt(frame, undo->m_newValues);
  TUndoManager::manager()->add(undo);

  m_deformedSkeleton.invalidate();
  invalidate();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
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

    // SuperPlastic: mark the IK anchor (pinned vertex) at the current frame
    // with a cyan diamond so the animator sees what is planted.
    int pin = pinnedVertexAtFrame(::frame());
    if (pin >= 0) {
      TPointD p  = deformedSkeleton.vertex(pin).P();
      double r   = 9.0 * pixelSize;
      glColor4ub(0, 220, 255, 255);
      glLineWidth(2.0f * m_viewer->getDevPixRatio());
      glBegin(GL_LINE_LOOP);
      glVertex2d(p.x, p.y - r);
      glVertex2d(p.x + r, p.y);
      glVertex2d(p.x, p.y + r);
      glVertex2d(p.x - r, p.y);
      glEnd();
      glLineWidth(1.0f);
    }
  }

  drawHighlights(m_sd, &deformedSkeleton, pixelSize);
}
