

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

  double frame = ::frame();

  // The native root normally can't be moved (it has no ANGLE/DISTANCE params).
  // But under IK with an active pin the pin is the temporary root, so the native
  // root becomes an ordinary manipulable vertex: its rotation about its pin-ward
  // neighbour is stored in that neighbour's ANGLE and preserves bone lengths.
  bool isRoot = m_svSel.hasSingleObject() &&
                deformedSkeleton().vertex(m_svSel).parent() < 0;
  bool ikPin  = m_ikDrag.getValue() && pinnedVertexAtFrame(frame) >= 0;

  if (m_sd && m_svSel.hasSingleObject() && (!isRoot || ikPin)) {
    l_suspendParamsObservation = true;  // Automatic params notification happen
    // twice (1 x param) - dealing with it manually

    // First, retrieve selected vertex's deformation
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), m_svSel);
    assert(vd);

    // Move selected branch
    if (m_ikDrag.getValue() &&
        (deformedSkeleton().vertex(m_svSel).parent() >= 0 || ikPin)) {
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

// SuperPlastic IK adapter (Animate mode). Posing is always a SINGLE-JOINT,
// local operation — never a chain solve (distributing a drag over the whole
// root->handle chain, as the earlier CCD did, reconfigured the body
// unpredictably).
//
// With NO pin: plain local handling in the ORIGINAL hierarchy — dragging a
// vertex rotates only its own joint, descendants follow rigidly, nothing toward
// the root moves.
//
// With a pin: THE PIN IS A TEMPORARY ROOT. The hierarchy is re-rooted at the
// pin (BFS), and v is manipulated as a single joint of THAT tree: the bone
// (reParent(v), v) rotates about reParent(v) — which stays fixed — carrying v
// and its re-rooted subtree rigidly, while everything on the pin side (the pin
// included) stays put. This is exactly how one manipulates a normal skeleton
// whose root is the pinned vertex. The resulting shape is written back into the
// deformer's fixed root-down ANGLE params; the pin's absolute planting is kept
// per-frame at EVALUATION via its PINTX/PINTY target (applyPinConstraint), so a
// pinned foot never drifts on the in-betweens.
void PlasticTool::moveVertexIK_animate(double frame, int v,
                                       const TPointD &pos) {
  if (!m_sd || !skeleton()) return;
  const PlasticSkeleton &orig = *skeleton();

  int pin = pinnedVertexAtFrame(frame);
  if (pin < 0 || pin == v) {
    // No pin (or dragging the pin itself): local single joint, original tree.
    if (orig.vertex(v).parent() < 0) return;  // the root has no ANGLE param
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), v);
    if (!vd) return;
    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    if (m_keepDistance.getValue()) {
      m_sd->updateAngle(*skeleton(), deformedSkeleton(), frame, v, pos);
    } else {
      ::setKeyframe(vd->m_params[SkVD::DISTANCE], frame);
      m_sd->updatePosition(*skeleton(), deformedSkeleton(), frame, v, pos);
    }
    return;
  }

  // ---- Pin active: re-root at the pin ----
  PlasticSkeleton &defSkel = deformedSkeleton();
  std::map<int, TPointD> curPos;
  int rootIdx = -1;
  for (auto vt = defSkel.vertices().begin(); vt != defSkel.vertices().end();
       ++vt) {
    curPos[vt.m_idx] = vt->P();
    if (vt->parent() < 0) rootIdx = vt.m_idx;
  }

  std::map<int, std::vector<int>> adj;
  const auto &edges = defSkel.edges();
  for (auto et = edges.begin(); et != edges.end(); ++et) {
    adj[et->vertex(0)].push_back(et->vertex(1));
    adj[et->vertex(1)].push_back(et->vertex(0));
  }
  std::map<int, int> reParent;
  {
    std::set<int> visited;
    std::queue<int> q;
    q.push(pin);
    visited.insert(pin);
    reParent[pin] = -1;
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int w : adj[u])
        if (!visited.count(w)) {
          visited.insert(w);
          reParent[w] = u;
          q.push(w);
        }
    }
    if (!visited.count(v)) return;  // disconnected
  }

  int rp = reParent[v];
  if (rp < 0) return;  // v is the pin itself
  TPointD pivot = curPos[rp];

  // Single-joint rotation about the pivot (the re-rooted parent), at CONSTANT
  // bone length — v travels on a circle to follow the mouse angle. This is a
  // pure rigid rotation of v's subtree, so it stays a valid FK configuration
  // (all bone lengths preserved) that the deformer reproduces exactly. We do
  // NOT translate to reach the mouse precisely: that would change one bone's
  // length without writing its DISTANCE param, corrupting the FK rebuild (a
  // non-uniform error the pin's translation-only constraint cannot correct,
  // which made the whole chain toward the native root explode).
  TPointD oldD = curPos[v] - pivot;
  TPointD newD = pos - pivot;
  if (norm2(oldD) < 1e-12 || norm2(newD) < 1e-12) return;
  double theta = wrapPi(atan2(newD.y, newD.x) - atan2(oldD.y, oldD.x));
  double c = cos(theta), s = sin(theta);
  auto rotAbout = [&](const TPointD &p) {
    TPointD d = p - pivot;
    return pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
  };

  // v's re-rooted subtree = vertices reached from v descending re-rooted edges.
  std::set<int> moved;
  {
    std::queue<int> q;
    q.push(v);
    moved.insert(v);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int w : adj[u])
        if (reParent.count(w) && reParent[w] == u && !moved.count(w)) {
          moved.insert(w);
          q.push(w);
        }
    }
  }

  std::map<int, TPointD> desired = curPos;
  for (int u : moved) desired[u] = rotAbout(curPos[u]);

  // ---- Write-back into the fixed root-down parameterization ----
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

  int skelId = ::skeletonId();
  for (auto &kv : curPos) {
    int w  = kv.first;
    int op = orig.vertex(w).parent();
    if (op < 0) continue;  // skeleton root has no ANGLE param
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
    if (fabs(newDelta - oldDelta) < 1e-4) continue;  // unchanged joint

    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    vd->m_params[SkVD::ANGLE]->setValue(frame, newDelta);
  }
}

//------------------------------------------------------------------------

int PlasticTool::pinnedVertexAtFrame(double frame) const {
  std::vector<int> pins = pinnedVerticesAtFrame(frame);
  return pins.empty() ? -1 : pins.front();
}

//------------------------------------------------------------------------

std::vector<int> PlasticTool::pinnedVerticesAtFrame(double frame) const {
  std::vector<int> pins;
  if (!m_sd) return pins;
  PlasticSkeletonP skel = skeleton();
  if (!skel) return pins;
  int skelId = ::skeletonId();
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end(); ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (vd && vd->m_params[SkVD::PIN]->getValue(frame) >= 0.5)
      pins.push_back(vt.m_idx);
  }
  return pins;
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

  // Capture the planting target BEFORE flagging the pin: read where the vertex
  // is right now (still unpinned at this frame), from a fresh evaluation. If we
  // set PIN first, the eval would re-plant this vertex on its OLD target (a
  // leftover Constant key from an earlier pin range) and we'd capture that stale
  // spot instead of the current one — which is exactly what shifted everything.
  bool capture =
      !pinned && vd->m_params[SkVD::PINTX] && vd->m_params[SkVD::PINTY];
  TPointD target;
  if (capture) {
    m_deformedSkeleton.invalidate();
    target = deformedSkeleton().vertex(v).P();
  }

  // First-ever pin key on this vertex, past frame 1: anchor a 0 at frame 1 so
  // the pin is confined to [frame, ...). Without it a lone "on" key extrapolates
  // constant backwards and the foot reads as pinned on every earlier frame too.
  if (!pinned && frame > 1.0 &&
      vd->m_params[SkVD::PIN]->getKeyframeCount() == 0) {
    TDoubleKeyframe base(1.0, 0.0);
    base.m_type = base.m_prevType = TDoubleKeyframe::Constant;
    vd->m_params[SkVD::PIN]->setKeyframe(base);
  }
  // Constant (step) interpolation so the anchor holds between keyframes.
  TDoubleKeyframe kf(frame, pinned ? 0.0 : 1.0);
  kf.m_type = kf.m_prevType = TDoubleKeyframe::Constant;
  vd->m_params[SkVD::PIN]->setKeyframe(kf);

  // Write the captured target (only this pin — re-anchoring others would shift
  // them). Constant so it holds until re-planted.
  if (capture) {
    for (int c : {SkVD::PINTX, SkVD::PINTY}) {
      TDoubleKeyframe tk(frame, c == SkVD::PINTX ? target.x : target.y);
      tk.m_type = tk.m_prevType = TDoubleKeyframe::Constant;
      vd->m_params[c]->setKeyframe(tk);
    }
  }

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

    // SuperPlastic: mark every IK anchor (pinned vertex) at the current frame
    // with a cyan diamond so the animator sees what is planted — during the
    // double-support frame of a walk there can be two at once.
    double r = 9.0 * pixelSize;
    glColor4ub(0, 220, 255, 255);
    glLineWidth(2.0f * m_viewer->getDevPixRatio());
    for (int pin : pinnedVerticesAtFrame(::frame())) {
      TPointD p = deformedSkeleton.vertex(pin).P();
      glBegin(GL_LINE_LOOP);
      glVertex2d(p.x, p.y - r);
      glVertex2d(p.x + r, p.y);
      glVertex2d(p.x, p.y + r);
      glVertex2d(p.x - r, p.y);
      glEnd();
    }
    glLineWidth(1.0f);
  }

  drawHighlights(m_sd, &deformedSkeleton, pixelSize);
}
