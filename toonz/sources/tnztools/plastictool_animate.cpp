

// TnzCore includes
#include "tundo.h"

// TnzLib includes
#include "toonz/ikccd.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"

#include "plastictool.h"

#include <algorithm>
#include <cmath>
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

// Absolute direction of a vertex's polar reference (its parent bone direction,
// walking further up on degenerate bones, world +x at the root), computed from
// an arbitrary position lookup keyed on the ORIGINAL parent topology.
double parentDirAngle(const PlasticSkeleton &topo,
                      const std::map<int, TPointD> &P, int vx) {
  for (int p = vx; p >= 0;) {
    int pp = topo.vertex(p).parent();
    if (pp < 0) return 0.0;
    TPointD d = P.at(p) - P.at(pp);
    if (norm2(d) > 1e-8) return atan2(d.y, d.x);
    p = pp;
  }
  return 0.0;
}

}  // namespace

//------------------------------------------------------------------------

// SuperPlastic IK adapter. Two modes, both writing back only the per-vertex
// ANGLE deformation params (distances untouched, proportions preserved):
//
//  * no pin  -> solves the serial chain skeleton-root -> dragged vertex with
//    the shared CCD solver (toonz/ikccd.h), honoring per-vertex angle bounds.
//  * a pin   -> re-roots the chain at the pinned vertex, keeping it fixed while
//    the dragged vertex reaches the mouse; branches hanging off the solved path
//    follow rigidly. (Angle bounds are not enforced in this mode yet — the
//    re-rooted angle space would need per-edge sign handling.)
//
// Positions are computed for every affected vertex, then a single write-back
// re-derives each vertex's ANGLE from its solved position relative to its
// ORIGINAL-tree parent — so the result is valid whatever the solve did.
void PlasticTool::moveVertexIK_animate(double frame, int v,
                                       const TPointD &pos) {
  if (!m_sd || !skeleton()) return;
  const PlasticSkeleton &orig = *skeleton();
  PlasticSkeleton &defSkel    = deformedSkeleton();

  // Current deformed and rest positions, keyed by vertex index.
  std::map<int, TPointD> curPos, origPos;
  for (auto vt = defSkel.vertices().begin(); vt != defSkel.vertices().end();
       ++vt) {
    curPos[vt.m_idx]  = vt->P();
    origPos[vt.m_idx] = orig.vertex(vt.m_idx).P();
  }

  std::map<int, TPointD> solvedPos = curPos;  // untouched vertices stay put
  std::set<int> moved;                        // vertices to write back

  int pin = pinnedVertexAtFrame(frame);

  if (pin < 0 || pin == v) {
    // ---- root-anchored serial solve ----
    std::vector<int> chainVx;
    for (int cur = v; cur >= 0; cur = defSkel.vertex(cur).parent())
      chainVx.push_back(cur);
    std::reverse(chainVx.begin(), chainVx.end());
    int boneCount = int(chainVx.size()) - 1;
    if (boneCount < 1) return;

    IKChain chain;
    const double quietNaN = std::numeric_limits<double>::quiet_NaN();
    const double bigBound  = 1e9;  // bounds default to +-DBL_MAX
    for (int i = 0; i <= boneCount; ++i)
      chain.currentPositions.push_back(curPos[chainVx[i]]);
    for (int i = 0; i < boneCount; ++i) {
      const PlasticSkeletonVertex &ovx = orig.vertex(chainVx[i + 1]);
      TPointD od  = origPos[chainVx[i + 1]] - origPos[chainVx[i]];
      double rest = wrapPi(atan2(od.y, od.x) -
                           parentDirAngle(orig, origPos, chainVx[i]));
      IKBone bone;
      bone.length      = norm(chain.currentPositions[i + 1] -
                              chain.currentPositions[i]);
      bone.parentIndex = i - 1;
      bone.angleMin    = (ovx.m_minAngle < -bigBound)
                             ? quietNaN
                             : rest + ovx.m_minAngle * M_PI_180;
      bone.angleMax    = (ovx.m_maxAngle > bigBound)
                             ? quietNaN
                             : rest + ovx.m_maxAngle * M_PI_180;
      chain.bones.push_back(bone);
    }
    IKTarget target;
    target.position = pos;
    std::vector<TPointD> solved =
        SolveIK_CCD(chain, target, 30, 0.5 * getPixelSize());
    for (int i = 1; i <= boneCount; ++i) {
      solvedPos[chainVx[i]] = solved[i];
      moved.insert(chainVx[i]);
    }
  } else {
    // ---- pin-anchored re-rooted solve ----
    // Undirected adjacency of the skeleton.
    std::map<int, std::vector<int>> adj;
    const auto &edges = defSkel.edges();
    for (auto et = edges.begin(); et != edges.end(); ++et) {
      int a = et->vertex(0), b = et->vertex(1);
      adj[a].push_back(b);
      adj[b].push_back(a);
    }
    // BFS from the pin: re-rooted parent + discovery order.
    std::map<int, int> reParent;
    std::vector<int> order;
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
    if (!visited.count(v)) return;  // disconnected — nothing to solve

    // Path pin -> v (following the re-rooted parents).
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
    IKTarget target;
    target.position = pos;
    std::vector<TPointD> solved =
        SolveIK_CCD(chain, target, 30, 0.5 * getPixelSize());

    // Per-vertex rigid rotation of the local frame (approximated by the
    // incoming re-rooted bone's rotation); the pin's frame is the identity.
    std::map<int, double> frameRot;
    std::map<int, bool> didMove;
    frameRot[pin] = 0.0;
    didMove[pin]  = false;
    for (int i = 1; i <= boneCount; ++i) {
      int p = path[i], pp = path[i - 1];
      solvedPos[p]  = solved[i];
      TPointD oldD  = curPos[p] - curPos[pp];
      TPointD newD  = solved[i] - solved[i - 1];
      frameRot[p]   = wrapPi(atan2(newD.y, newD.x) - atan2(oldD.y, oldD.x));
      didMove[p]    = true;
      moved.insert(p);
    }
    // Propagate to off-path vertices in BFS order: a branch rides rigidly on
    // the frame of the first moved ancestor, unmoved branches stay put.
    for (int u : order) {
      if (frameRot.count(u)) continue;  // pin or path: already handled
      int rp = reParent[u];
      if (didMove[rp]) {
        double r     = frameRot[rp];
        double c = cos(r), s = sin(r);
        TPointD off  = curPos[u] - curPos[rp];
        solvedPos[u] = solvedPos[rp] +
                       TPointD(c * off.x - s * off.y, s * off.x + c * off.y);
        frameRot[u]  = r;
        didMove[u]   = true;
        moved.insert(u);
      } else {
        frameRot[u] = 0.0;
        didMove[u]  = false;
      }
    }
  }

  // ---- unified write-back: recompute ANGLE from solved positions ----
  for (int w : moved) {
    int op = orig.vertex(w).parent();
    if (op < 0) continue;  // skeleton root has no ANGLE param
    TPointD od  = origPos[w] - origPos[op];
    double rest = wrapPi(atan2(od.y, od.x) - parentDirAngle(orig, origPos, op));
    TPointD nd  = solvedPos[w] - solvedPos[op];
    double nAbs = (norm2(nd) > 1e-8) ? atan2(nd.y, nd.x) : 0.0;
    double newDelta =
        wrapPi(nAbs - parentDirAngle(orig, solvedPos, op) - rest) * M_180_PI;

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
