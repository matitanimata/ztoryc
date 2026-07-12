

// TnzCore includes
#include "tundo.h"
#include "tgl.h"

// TnzLib includes
#include "toonz/ikccd.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"

#include "plastictool.h"
#include <cstdio>

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
  // Explicit coordinates: a cross-level drag touches several columns, each
  // needing its own undo (TemporaryActivation re-activates m_col to apply it).
  AnimateValuesUndo(int v, int row, int col) : m_row(row), m_col(col), m_v(v) {}

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

//------------------------------------------------------------------------

// Undo for an angle-limit gizmo drag. Covers both cases uniformly: the static
// per-vertex bounds AND the SkVD keyframe snapshot (which carries a keyed
// MINANGLE/MAXANGLE when the bound is animated).
class AngleLimitUndo final : public TUndo {
  int m_row, m_col, m_v;

public:
  double m_oldMin, m_oldMax, m_newMin, m_newMax;
  SkDKey m_oldValues, m_newValues;

  AngleLimitUndo(int v)
      : m_row(::row())
      , m_col(::column())
      , m_v(v)
      , m_oldMin(0)
      , m_oldMax(0)
      , m_newMin(0)
      , m_newMax(0) {}

  int getSize() const override { return sizeof(*this) + (20 << 10); }

  void apply(double sMin, double sMax, const SkDKey &vals) const {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);
    if (m_v >= 0) l_plasticTool.setSkeletonSelection(m_v);

    l_suspendParamsObservation = true;
    if (SkDP sd = l_plasticTool.deformation()) {
      if (PlasticSkeletonP skel = sd->skeleton(::skeletonId())) {
        skel->vertex(m_v).m_minAngle = sMin;
        skel->vertex(m_v).m_maxAngle = sMax;
      }
      sd->deleteKeyframe(m_row - 1);
      sd->setKeyframe(vals);
    }
    l_suspendParamsObservation = false;
    l_plasticTool.onChange();
  }

  void undo() const override { apply(m_oldMin, m_oldMax, m_oldValues); }
  void redo() const override { apply(m_newMin, m_newMax, m_newValues); }

  QString getHistoryString() override {
    return QObject::tr("Plastic: Angle Bounds");
  }
};

//------------------------------------------------------------------------

// Undo for the IK-off bake (bakePinsToFK): the bake rewrites many keyframes
// across many frames and vertices (drops PIN/PINTX/PINTY, writes ANGLE +
// controller translation). Snapshots the full per-frame SkVD keyframe state
// at every affected frame before and after, and restores it wholesale.
class BakeToFKUndo final : public TUndo {
  int m_row, m_col;

public:
  std::vector<double> m_frames;
  std::vector<SkDKey> m_old, m_new;

  BakeToFKUndo() : m_row(::row()), m_col(::column()) {}

  int getSize() const override {
    return sizeof(*this) + (int)m_frames.size() * (4 << 10);
  }

  void apply(const std::vector<SkDKey> &snap, bool ikOn) const {
    PlasticTool::TemporaryActivation tempActivate(m_row, m_col);
    l_suspendParamsObservation = true;
    if (SkDP sd = l_plasticTool.deformation()) {
      for (size_t i = 0; i < m_frames.size(); ++i) {
        sd->deleteKeyframe(m_frames[i]);
        sd->setKeyframe(snap[i]);
      }
    }
    l_suspendParamsObservation = false;
    // Undo brings the pins back → restore IK mode; redo re-commits → IK off
    l_plasticTool.setIkModeState(ikOn);
    l_plasticTool.onChange();
  }

  void undo() const override { apply(m_old, true); }
  void redo() const override { apply(m_new, false); }

  QString getHistoryString() override {
    return QObject::tr("Plastic: Bake IK Pins to FK");
  }
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

    // Controller gizmo hover: handles win over vertices, like the click does
    m_ctrlHighlight = controllerHitTest_animate(pos);
    if (m_ctrlHighlight != CtrlNone) m_svHigh = -1;

    // Angle-limit bound hover (shown for the selected joint)
    m_limitHi = (m_ctrlHighlight == CtrlNone) ? limitHitTest_animate(pos) : 0;
    if (m_limitHi != 0) m_svHigh = -1;

    invalidate();
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  // Track mouse position
  m_pressedPos = m_pos = pos;

  // Fresh drag: drop any cross-level IK state left over from an aborted drag.
  m_ikCrossDragged = false;
  m_ikCrossOld.clear();
  m_ikCrossDefs.clear();
  m_ikCrossPinWorld.clear();

  // Controller gizmo, hit-tested FIRST (it must not touch the vertex
  // selection): a full Animate-tool replica whose pivot follows the deformed
  // root. All controller values at press time are the drag baselines.
  int device = m_sd ? controllerHitTest_animate(pos) : (int)CtrlNone;
  if (device != CtrlNone) {
    SkVD *vd = rootVd_animate();
    TPointD gizmoC;
    if (vd && squashPivot_animate(gizmoC)) {
      auto oldVal = [&](SkVD::Params p, double def) {
        return vd->m_params[p] ? vd->m_params[p]->getValue(::frame()) : def;
      };
      m_ctrlDevice      = device;
      m_scaleDragCenter = gizmoC;
      m_scaleOldX       = oldVal(SkVD::SCALEX, 1.0);
      m_scaleOldY       = oldVal(SkVD::SCALEY, 1.0);
      m_ctrlOldRot      = oldVal(SkVD::ROT, 0.0);
      m_ctrlOldTX       = oldVal(SkVD::TRANSX, 0.0);
      m_ctrlOldTY       = oldVal(SkVD::TRANSY, 0.0);
      m_ctrlOldShX      = oldVal(SkVD::SHEARX, 0.0);
      m_ctrlOldShY      = oldVal(SkVD::SHEARY, 0.0);
      m_sd->getKeyframeAt(frame(), m_pressedSkDF);  // undo baseline
      invalidate();
      return;
    }
  }

  // Angle-limit bound handle, hit-tested before selection changes: grab it to
  // drag the joint's min/max bound without disturbing the vertex selection.
  if (m_sd && m_svSel.hasSingleObject()) {
    int lb = limitHitTest_animate(pos);
    if (lb != 0) {
      m_limitDrag = lb;
      // Snapshot for the undo (static bounds + SkVD keyframe)
      if (PlasticSkeletonP skel = skeleton()) {
        m_limitOldMin = skel->vertex((int)m_svSel).m_minAngle;
        m_limitOldMax = skel->vertex((int)m_svSel).m_maxAngle;
      }
      m_sd->getKeyframeAt(frame(), m_pressedSkDF);
      invalidate();
      return;
    }
  }

  // SuperPlastic multi-level: if nothing on the current column is under the
  // cursor, try the hierarchically connected columns. Clicking a vertex there
  // makes that column active (and selects the vertex), so the whole articulated
  // character is editable without hunting for the right column in the xsheet.
  if (m_sd && m_svHigh < 0 && m_ctrlDevice == CtrlNone && m_limitDrag == 0) {
    const double pickR = getPixelSize() * HIGHLIGHT_DISTANCE;
    double best        = pickR;
    int bestCol = -1, bestV = -1;
    for (const ConnectedSkel &cs : connectedSkeletons_animate()) {
      double dLocal;
      int v = cs.skel.closestVertex(cs.toCur.inv() * pos, &dLocal);
      if (v < 0) continue;
      // Re-measure in the current draw space (toCur may scale distances).
      double d = norm((cs.toCur * cs.skel.vertex(v).P()) - pos);
      if (d < best) { best = d; bestCol = cs.columnIndex; bestV = v; }
    }
    if (bestCol >= 0) {
      redirectChildRootToParent_animate(bestCol, bestV);
      TTool::getApplication()->getCurrentColumn()->setColumnIndex(bestCol);
      updateMatrix();  // columnIndexSwitched re-binds m_sd via onColumnSwitched
      setSkeletonSelection(bestV);
      if (m_svSel.hasSingleObject()) {
        m_pressedVxsPos =
            std::vector<TPointD>(1, deformedSkeleton().vertex(m_svSel).P());
        m_sd->getKeyframeAt(frame(), m_pressedSkDF);
      }
      invalidate();
      return;
    }
  }

  // If the highlighted vertex is this column's stitched root, grab the coincident
  // draggable parent vertex instead (switching to the parent column).
  int selCol = ::column(), selV = m_svHigh;
  redirectChildRootToParent_animate(selCol, selV);
  if (m_svHigh >= 0 && selCol != ::column()) {
    TTool::getApplication()->getCurrentColumn()->setColumnIndex(selCol);
    updateMatrix();
    setSkeletonSelection(selV);
  } else {
    setSkeletonSelection(m_svHigh);
  }

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

  if (m_ctrlDevice != CtrlNone) {
    controllerDrag_animate(pos, me);
    return;
  }
  if (m_limitDrag != 0) {
    limitDrag_animate(pos);
    return;
  }

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

    // Move selected branch. NOTE: the squash & stretch controller never
    // appears here — it lives ON TOP of the skeleton (composed into the tool
    // matrix), so all manipulation runs in pre-controller space by design.
    // Cross-level IK: a pin lives on a different column than the dragged
    // vertex, so the solve must span the connected columns. Only for a
    // non-root vertex (the current column's root has no ANGLE param of its
    // own — its motion belongs to the parent column).
    if (m_ikDrag.getValue() &&
        (deformedSkeleton().vertex(m_svSel).parent() >= 0 || ikPin)) {
      // FK posing (no pins) on a multi-column character runs on the unified
      // graph so child columns turn as ordinary chain joints; pinned posing
      // falls back to the single-level path (cross-column pins are mirrored onto
      // the parent's attachment vertex, see setAttachmentPin_animate).
      if (!crossLevelFK_animate(frame, m_svSel, pos))
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

// The ROOT vertex's deformation: home of the squash & stretch controller
// params (SCALEX/SCALEY/PIVOTX/PIVOTY).
SkVD *PlasticTool::rootVd_animate(int *rootIdx) {
  if (rootIdx) *rootIdx = -1;
  if (!m_sd) return 0;
  PlasticSkeletonP skel = m_sd->skeleton(::skeletonId());
  if (!skel) return 0;

  const auto &vs = skel->vertices();
  for (auto vt = vs.begin(); vt != vs.end(); ++vt)
    if (vt->parent() < 0) {
      if (rootIdx) *rootIdx = (int)vt.m_idx;
      return m_sd->vertexDeformation(::skeletonId(), (int)vt.m_idx);
    }
  return 0;
}

//------------------------------------------------------------------------

// The squash & stretch controller pivot, in tool-local (pre-controller)
// coordinates: DEFORMED root position + keyframeable PIVOTX/PIVOTY offset.
bool PlasticTool::squashPivot_animate(TPointD &C) {
  int rootIdx = -1;
  SkVD *vd    = rootVd_animate(&rootIdx);
  if (!vd || rootIdx < 0) return false;
  if (!vd->m_params[SkVD::SCALEX] || !vd->m_params[SkVD::SCALEY]) return false;

  double frame = ::frame();
  C            = deformedSkeleton().vertex(rootIdx).P();
  if (vd->m_params[SkVD::PIVOTX])
    C.x += vd->m_params[SkVD::PIVOTX]->getValue(frame);
  if (vd->m_params[SkVD::PIVOTY])
    C.y += vd->m_params[SkVD::PIVOTY]->getValue(frame);
  return true;
}

//------------------------------------------------------------------------

// Replica of the Animate tool's DragScaleTool math, with the pivot on the
// selected vertex: sqrt-damped axis factors, Shift = aspect ratio, Alt =
// precise (1/10), "Maintain:" combo = A/R or Mass (V = 1/H) constraint.
// Writes keyframed SCALEX/SCALEY on the pivot's deformation; the undo is the
// standard AnimateValuesUndo added on button release.
void PlasticTool::scaleDrag_animate(const TPointD &pos, const TMouseEvent &me) {
  if (!m_sd) return;
  SkVD *vd = rootVd_animate();
  if (!vd || !vd->m_params[SkVD::SCALEX] || !vd->m_params[SkVD::SCALEY])
    return;

  const double eps = 1e-8;
  TPointD a        = m_pressedPos - m_scaleDragCenter;
  TPointD b        = pos - m_scaleDragCenter;
  if (norm2(a) < eps || norm2(b) < eps) return;
  if (fabs(a.x) < eps || fabs(a.y) < eps) return;

  double fx            = b.x / a.x;
  if (fabs(fx) > 1) fx = (fx < 0 ? -1 : 1) * sqrt(fabs(fx));
  double fy            = b.y / a.y;
  if (fabs(fy) > 1) fy = (fy < 0 ? -1 : 1) * sqrt(fabs(fy));

  int constraint = m_scaleConstraint.getIndex();  // 0 none, 1 A/R, 2 mass
  if (constraint == 0 && me.isShiftPressed()) constraint = 1;
  // The main scale handle is UNIFORM (like the Animate tool's "Scale"): free
  // per-axis scaling belongs to the offset square (CtrlScaleXY)
  if (m_ctrlDevice == CtrlScale && constraint == 0) constraint = 1;

  if (constraint == 1) {
    TPointD c = pos - m_pressedPos;
    if (fabs(c.x) > fabs(c.y))
      fy = fx;
    else
      fx = fy;
  } else if (constraint == 2) {
    double bxay = b.x * a.y, byax = b.y * a.x;
    if (fabs(bxay) < eps || fabs(byax) < eps) return;
    fx = bxay / byax;
    fy = byax / bxay;
  }
  if (fabs(fx) < eps || fabs(fy) < eps) return;

  // Precise control while pressing Alt, like the Animate tool
  if (me.isAltPressed()) {
    fx = 1.0 + (fx - 1.0) * 0.1;
    fy = 1.0 + (fy - 1.0) * 0.1;
  }

  double oldX                   = m_scaleOldX, oldY = m_scaleOldY;
  if (fabs(oldX) < 0.001) oldX  = 0.001;
  if (fabs(oldY) < 0.001) oldY  = 0.001;

  double frame               = ::frame();
  l_suspendParamsObservation = true;
  ::setKeyframe(vd->m_params[SkVD::SCALEX], frame);
  ::setKeyframe(vd->m_params[SkVD::SCALEY], frame);
  vd->m_params[SkVD::SCALEX]->setValue(frame, oldX * fx);
  vd->m_params[SkVD::SCALEY]->setValue(frame, oldY * fy);
  l_suspendParamsObservation = false;

  m_scaleXRelay.notifyListeners();  // Keep the toolbar fields live
  m_scaleYRelay.notifyListeners();

  m_deformedSkeleton.invalidate();
  invalidate();
}

//------------------------------------------------------------------------

// Moves the squash & stretch controller pivot (Ctrl+drag): the position is
// stored as a keyframeable OFFSET from the DEFORMED root, so the pivot
// follows the character. Snaps to skeleton vertices within a small radius.
void PlasticTool::pivotDrag_animate(const TPointD &pos) {
  int rootIdx = -1;
  SkVD *vd    = rootVd_animate(&rootIdx);
  if (!vd || rootIdx < 0 || !vd->m_params[SkVD::PIVOTX] ||
      !vd->m_params[SkVD::PIVOTY])
    return;

  // Snap to the nearest skeleton vertex (the typical pivot targets)
  TPointD target = pos;
  {
    double d;
    int v = deformedSkeleton().closestVertex(pos, &d);
    if (v >= 0 && d < 10.0 * getPixelSize())
      target = deformedSkeleton().vertex(v).P();
  }

  double frame         = ::frame();
  const TPointD offset = target - deformedSkeleton().vertex(rootIdx).P();

  l_suspendParamsObservation = true;
  ::setKeyframe(vd->m_params[SkVD::PIVOTX], frame);
  ::setKeyframe(vd->m_params[SkVD::PIVOTY], frame);
  vd->m_params[SkVD::PIVOTX]->setValue(frame, offset.x);
  vd->m_params[SkVD::PIVOTY]->setValue(frame, offset.y);
  l_suspendParamsObservation = false;

  invalidate();
}

//------------------------------------------------------------------------

// Full controller drag dispatch: position/rotation/shear here, pivot and
// scale in their dedicated handlers. Math replicated from the Animate tool's
// DragPositionTool / DragRotationTool / DragShearTool (Shift = axis lock,
// Alt = precise 1/10).
void PlasticTool::controllerDrag_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  switch (m_ctrlDevice) {
  case CtrlPivot:
    pivotDrag_animate(pos);
    return;
  case CtrlScale:
  case CtrlScaleXY:
    scaleDrag_animate(pos, me);
    return;
  case CtrlMove:
  case CtrlRot:
  case CtrlShear:
    break;
  default:
    return;
  }

  SkVD *vd = rootVd_animate();
  if (!vd) return;
  double frame = ::frame();

  int pA = -1, pB = -1;
  double vA = 0.0, vB = 0.0;

  if (m_ctrlDevice == CtrlMove) {
    TPointD d = pos - m_pressedPos;
    if (me.isShiftPressed()) {
      if (fabs(d.x) > fabs(d.y))
        d.y = 0.0;
      else
        d.x = 0.0;
    }
    if (me.isAltPressed()) d = 0.1 * d;
    pA = SkVD::TRANSX, vA = m_ctrlOldTX + d.x;
    pB = SkVD::TRANSY, vB = m_ctrlOldTY + d.y;
  } else if (m_ctrlDevice == CtrlRot) {
    TPointD a = m_pressedPos - m_scaleDragCenter;
    TPointD b = pos - m_scaleDragCenter;
    if (norm2(a) < 1e-8 || norm2(b) < 1e-8) return;
    double ang = atan2(cross(a, b), a * b) * (180.0 / M_PI);
    if (me.isAltPressed()) ang *= 0.1;
    pA = SkVD::ROT, vA = m_ctrlOldRot + ang;
  } else {  // CtrlShear
    TPointD a = m_pressedPos - m_scaleDragCenter;
    TPointD b = pos - m_scaleDragCenter;
    double fx = a.x - b.x, fy = b.y - a.y;
    if (me.isShiftPressed()) {
      if (fabs(fx) > fabs(fy))
        fy = 0.0;
      else
        fx = 0.0;
    }
    if (me.isAltPressed()) fx *= 0.1, fy *= 0.1;
    pA = SkVD::SHEARX, vA = m_ctrlOldShX + 0.01 * fx;
    pB = SkVD::SHEARY, vB = m_ctrlOldShY + 0.01 * fy;
  }

  l_suspendParamsObservation = true;
  if (pA >= 0 && vd->m_params[pA]) {
    ::setKeyframe(vd->m_params[pA], frame);
    vd->m_params[pA]->setValue(frame, vA);
  }
  if (pB >= 0 && vd->m_params[pB]) {
    ::setKeyframe(vd->m_params[pB], frame);
    vd->m_params[pB]->setValue(frame, vB);
  }
  l_suspendParamsObservation = false;

  invalidate();
}

//------------------------------------------------------------------------

// Hit-test of the controller gizmo handles, in tool-local coordinates.
// Same layout as the Animate tool: rotation on top, scale bottom-left (plus
// the free-axis square), shear bottom-right, move at the bottom, pivot ring
// around the center (the inner area is left to vertex picking: the pivot
// usually sits on the root vertex).
int PlasticTool::controllerHitTest_animate(const TPointD &pos) {
  if (!m_sd || !m_showController.getValue()) return CtrlNone;
  TPointD C;
  if (!squashPivot_animate(C)) return CtrlNone;

  double u           = getPixelSize();
  const double delta = 30.0;

  TPointD rotP   = C + u * TPointD(0, delta);
  TPointD scaleP = C + u * TPointD(-delta, -delta);
  TPointD sxyP   = scaleP + u * TPointD(10, 10);
  TPointD shearP = C + u * TPointD(delta, -delta);
  TPointD moveP  = C + u * TPointD(0, -delta);

  if (norm(pos - rotP) < 8.0 * u) return CtrlRot;
  if (norm(pos - sxyP) < 5.0 * u) return CtrlScaleXY;
  if (norm(pos - scaleP) < 5.0 * u) return CtrlScale;
  if (norm(pos - shearP) < 8.0 * u) return CtrlShear;
  if (norm(pos - moveP) < 8.0 * u) return CtrlMove;

  double dc = norm(pos - C);
  if (dc > 5.0 * u && dc < 12.0 * u) return CtrlPivot;

  return CtrlNone;
}

//------------------------------------------------------------------------

namespace {

// Same dynamic-contrast scheme as the Ztoryc Animate tool gizmo (edittool):
// complementary hue with luminance forced opposite to the sampled background;
// the highlighted variant is a vivid distinct hue at mid lightness.
TPixel32 ctrlContrastColor(const TPixel32 &bg, bool highlighted) {
  double r = bg.r / 255.0, g = bg.g / 255.0, b = bg.b / 255.0;
  double mx = std::max({r, g, b}), mn = std::min({r, g, b}), d = mx - mn;
  double h = 0.0, s = 0.0, l = (mx + mn) / 2.0;
  if (d > 1e-6) {
    s = (l > 0.5) ? d / (2.0 - mx - mn) : d / (mx + mn);
    if (mx == r)
      h = (g - b) / d + (g < b ? 6.0 : 0.0);
    else if (mx == g)
      h = (b - r) / d + 2.0;
    else
      h = (r - g) / d + 4.0;
    h /= 6.0;
  }
  double L = 0.299 * r + 0.587 * g + 0.114 * b;
  double nh, ns, nl;
  if (highlighted) {
    nh = h + 0.5 + 0.42;
    ns = 0.95;
    nl = (L > 0.5) ? 0.42 : 0.66;
  } else {
    nh = h + 0.5;
    ns = (s < 0.18) ? 0.0 : 0.85;
    nl = (L > 0.5) ? 0.14 : 0.94;
  }
  nh -= std::floor(nh);
  auto hue = [](double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6.0) return p + (q - p) * 6 * t;
    if (t < 1 / 2.0) return q;
    if (t < 2 / 3.0) return p + (q - p) * (2 / 3.0 - t) * 6;
    return p;
  };
  double q  = nl < 0.5 ? nl * (1 + ns) : nl + ns - nl * ns;
  double p  = 2 * nl - q;
  double rr = hue(p, q, nh + 1 / 3.0), gg = hue(p, q, nh),
         bb = hue(p, q, nh - 1 / 3.0);
  return TPixel32(int(rr * 255), int(gg * 255), int(bb * 255), 255);
}

// Hover hint next to a handle, same look as the Animate tool's labels
void ctrlDrawText(const TPointD &p, double unit, const std::string &text) {
  glPushMatrix();
  glTranslated(p.x, p.y, 0.0);
  double sc = unit * 1.6;
  glScaled(sc, sc, 1);
  tglDrawText(TPointD(8, -3), text);
  glPopMatrix();
}

}  // namespace

// Draws the full controller gizmo with the Animate-tool dynamic colors: the
// framebuffer is sampled under the pivot BEFORE drawing, so the gizmo
// contrasts with the artwork behind it; the hovered/dragged handle uses the
// highlighted variant.
void PlasticTool::drawController_animate(double pixelSize) {
  if (!m_showController.getValue()) return;
  TPointD C;
  if (!squashPivot_animate(C)) return;

  double u           = pixelSize;
  const double delta = 30.0;

  // Sample the background under the pivot (viewer window coordinates)
  TPixel32 bg(128, 128, 128, 255);
  if (m_viewer) {
    TPointD wc  = getMatrix() * C;
    TPointD p   = m_viewer->worldToPos(wc);
    int dpr     = m_viewer->getDevPixRatio();
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    const int S = 7;
    int x       = vp[0] + (int)(p.x * dpr);
    int y       = vp[1] + vp[3] - (int)(p.y * dpr);
    x           = std::max(vp[0], std::min(x, vp[0] + vp[2] - S));
    y           = std::max(vp[1], std::min(y, vp[1] + vp[3] - S));
    unsigned char buf[S * S * 4];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, S, S, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    long r = 0, g = 0, b = 0;
    const int n = S * S;
    for (int i = 0; i < n; ++i) {
      r += buf[i * 4];
      g += buf[i * 4 + 1];
      b += buf[i * 4 + 2];
    }
    bg = TPixel32(int(r / n), int(g / n), int(b / n), 255);
  }
  const TPixel32 normal = ctrlContrastColor(bg, false);
  const TPixel32 hi     = ctrlContrastColor(bg, true);

  // While dragging, only the active device is marked; otherwise the hover
  int mark      = (m_ctrlDevice != CtrlNone) ? m_ctrlDevice : m_ctrlHighlight;
  auto colorFor = [&](int device) {
    const TPixel32 &c = (mark == device) ? hi : normal;
    glColor4ub(c.r, c.g, c.b, 255);
  };
  auto normalColor = [&]() { glColor4ub(normal.r, normal.g, normal.b, 255); };

  glLineWidth(1.5f * m_viewer->getDevPixRatio());

  TPointD rotP   = C + u * TPointD(0, delta);
  TPointD scaleP = C + u * TPointD(-delta, -delta);
  TPointD sxyP   = scaleP + u * TPointD(10, 10);
  TPointD shearP = C + u * TPointD(delta, -delta);
  TPointD moveP  = C + u * TPointD(0, -delta);

  // Connecting spokes first: DASHED, part of the visual identity that tells
  // this controller apart from the column Animate tool (solid spokes there)
  normalColor();
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(1, 0x0F0F);
  tglDrawSegment(rotP, C);
  tglDrawSegment(scaleP, C);
  tglDrawSegment(shearP, C);
  tglDrawSegment(moveP, C);
  glDisable(GL_LINE_STIPPLE);

  // Pivot: double HEXAGON (vs the Animate tool's double circle — the
  // silhouette is the first thing the eye reads). Drag the ring to move the
  // pivot, with vertex snapping.
  colorFor(CtrlPivot);
  auto drawHex = [&](double r) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 6; ++i) {
      double a = (30.0 + i * 60.0) * (M_PI / 180.0);
      glVertex2d(C.x + r * cos(a), C.y + r * sin(a));
    }
    glEnd();
  };
  drawHex(10.0 * u);
  drawHex(8.0 * u);

  // Rotation: disk on top
  colorFor(CtrlRot);
  tglDrawDisk(rotP, 5.0 * u);

  // Scale: uniform square + free-axis square, bottom-left
  double r3 = 3.0 * u;
  colorFor(CtrlScale);
  tglDrawRect(scaleP.x - r3, scaleP.y - r3, scaleP.x + r3, scaleP.y + r3);
  colorFor(CtrlScaleXY);
  tglDrawRect(sxyP.x - r3, sxyP.y - r3, sxyP.x + r3, sxyP.y + r3);

  // Shear: parallelogram, bottom-right
  colorFor(CtrlShear);
  glBegin(GL_LINE_LOOP);
  glVertex2d(shearP.x - u * 6, shearP.y - u * 3);
  glVertex2d(shearP.x - u * 3, shearP.y - u * 3);
  glVertex2d(shearP.x + u * 6, shearP.y + u * 3);
  glVertex2d(shearP.x + u * 3, shearP.y + u * 3);
  glEnd();

  // Move: filled diamond at the bottom
  colorFor(CtrlMove);
  double r4 = 4.0 * u;
  glBegin(GL_QUADS);
  glVertex2d(moveP.x, moveP.y - r4);
  glVertex2d(moveP.x + r4, moveP.y);
  glVertex2d(moveP.x, moveP.y + r4);
  glVertex2d(moveP.x - r4, moveP.y);
  glEnd();

  // Hover hints, like the Animate tool's labels (hidden while dragging).
  // The GLUT stroke text must be scaled by the device pixel ratio, exactly
  // like edittool's `unit` — without it the hints are near-invisible on
  // retina displays.
  if (m_ctrlDevice == CtrlNone && m_ctrlHighlight != CtrlNone) {
    double tu = u * m_viewer->getDevPixRatio();
    glColor4ub(hi.r, hi.g, hi.b, 255);
    switch (m_ctrlHighlight) {
    case CtrlPivot:
      ctrlDrawText(C + u * TPointD(14, 0), tu, "Move pivot");
      break;
    case CtrlRot:
      ctrlDrawText(rotP, tu, "Rotate");
      break;
    case CtrlScale:
      ctrlDrawText(scaleP + u * TPointD(-16, -16), tu, "Scale");
      break;
    case CtrlScaleXY:
      ctrlDrawText(sxyP + u * TPointD(6, 6), tu, "Horizontal/Vertical scale");
      break;
    case CtrlShear:
      ctrlDrawText(shearP + u * TPointD(0, -10), tu, "Shear");
      break;
    case CtrlMove:
      ctrlDrawText(moveP + u * TPointD(0, -12), tu, "Move");
      break;
    }
  }

  glLineWidth(1.0f);
}

//------------------------------------------------------------------------

namespace {

inline double wrapPi(double a) {
  while (a > M_PI) a -= 2.0 * M_PI;
  while (a < -M_PI) a += 2.0 * M_PI;
  return a;
}

// Root-first chain of vertex indices from the skeleton root down to v.
std::vector<int> pathFromRoot(const PlasticSkeleton &skel, int v) {
  std::vector<int> path;
  for (int p = v; p >= 0; p = skel.vertex(p).parent()) path.push_back(p);
  std::reverse(path.begin(), path.end());
  return path;
}

// Whether u belongs to a's subtree in the original parenting (a included).
bool isInSubtree(const PlasticSkeleton &skel, int u, int a) {
  for (int p = u; p >= 0; p = skel.vertex(p).parent())
    if (p == a) return true;
  return false;
}

// Tree path a .. LCA .. b (both included).
std::vector<int> pathBetween(const PlasticSkeleton &skel, int a, int b) {
  std::vector<int> pa = pathFromRoot(skel, a), pb = pathFromRoot(skel, b);
  size_t i = 0;
  while (i < pa.size() && i < pb.size() && pa[i] == pb[i]) ++i;
  std::vector<int> res;
  for (size_t j = pa.size(); j-- > i;) res.push_back(pa[j]);
  res.push_back(pa[i - 1]);  // the LCA (i >= 1: both paths share the root)
  for (size_t j = i; j < pb.size(); ++j) res.push_back(pb[j]);
  return res;
}

// Minimal subtree spanning all the pins (union of their pairwise paths).
std::set<int> spanningOfPins(const PlasticSkeleton &skel,
                             const std::vector<int> &pins) {
  std::set<int> S;
  for (size_t i = 0; i < pins.size(); ++i)
    for (size_t j = i + 1; j < pins.size(); ++j) {
      std::vector<int> path = pathBetween(skel, pins[i], pins[j]);
      S.insert(path.begin(), path.end());
    }
  return S;
}

// Union of the chains v -> each pin, re-parented toward v (v is the T root).
struct PinTree {
  std::map<int, int> parentT;               // toward v; v -> -1
  std::map<int, std::vector<int>> childT;   // away from v
  std::map<int, double> len;                // bone length to parentT
  std::map<int, int> depth;                 // hops from v
  std::vector<int> order;                   // BFS from v outward
};

PinTree buildPinTree(const PlasticSkeleton &skel,
                     const std::map<int, TPointD> &curPos, int v,
                     const std::vector<int> &pins) {
  PinTree T;
  T.parentT[v] = -1;
  for (int p : pins) {
    std::vector<int> path = pathBetween(skel, v, p);
    for (size_t k = 1; k < path.size(); ++k)
      if (!T.parentT.count(path[k])) T.parentT[path[k]] = path[k - 1];
  }
  for (const auto &kv : T.parentT)
    if (kv.second >= 0) {
      T.childT[kv.second].push_back(kv.first);
      T.len[kv.first] = norm(curPos.at(kv.first) - curPos.at(kv.second));
    }
  std::queue<int> q;
  q.push(v);
  T.depth[v] = 0;
  while (!q.empty()) {
    int u = q.front();
    q.pop();
    T.order.push_back(u);
    auto ct = T.childT.find(u);
    if (ct != T.childT.end())
      for (int c : ct->second) {
        T.depth[c] = T.depth[u] + 1;
        q.push(c);
      }
  }
  return T;
}

// FABRIK with multiple fixed anchors (the pins) and a dragged root (v): phase
// A puts v on the mouse target pushing the chains outward; phase B nails every
// pin back on its anchor pulling toward v, averaging at the junctions; a
// stiffness pass then pulls proximal bones back toward their pre-drag
// direction — FABRIK concentrates all the bending at the junction (clavicles
// fold, elbows barely move), while an animator wants the DISTAL joints to
// absorb first. Finally, exact bone lengths are restored (junction averaging
// violates them, and the ANGLE-only write-back would turn that violation into
// unplanted pins) and each pin is re-nailed rotating ONLY its exclusive
// branch, which cannot disturb the other chains.
void solveMultiAnchor(const PlasticSkeleton &orig, const PinTree &T,
                      const std::map<int, TPointD> &anchor, const TPointD &t,
                      std::map<int, TPointD> &P) {
  auto dir = [](const TPointD &d) {
    double n = norm(d);
    return (n < 1e-9) ? TPointD(1.0, 0.0) : d * (1.0 / n);
  };
  auto rot = [](const TPointD &d, double ang) {
    double c = cos(ang), s = sin(ang);
    return TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
  };

  // Per-joint stiffness (1 hop from v = stiff, fully flexible at 4+ hops)
  // pulling toward the bone's REST orientation relative to its reference bone
  // — not toward its direction at drag start: that acted as a ratchet (a
  // clavicle stretched out of necessity stayed stretched when slack returned;
  // with the rest reference it springs back to the natural pose).
  struct StiffRef {
    double stiff, rel;  // rest angle relative to the reference bone
    int refA, refB;     // live reference bone (positions in P)...
    TPointD constDir;   // ...or a fixed direction when the bone is off-tree
  };
  // Bones on the stretch from v toward the tree top are v's MOUNT to the
  // torso: articulating them is exactly what dragging v means (dragging a
  // shoulder pivots ITS clavicle), so they are exempt from stiffness. When v
  // is the top itself (dragging the sternum), nothing is exempt and the
  // clavicles keep their anatomical resistance.
  std::set<int> mount;
  {
    int top = -1;
    for (const auto &kv : T.parentT) {
      int op = orig.vertex(kv.first).parent();
      if (op < 0 || !T.parentT.count(op)) {
        top = kv.first;
        break;
      }
    }
    for (int u = top; u >= 0 && u != T.order[0]; u = T.parentT.at(u))
      mount.insert(u);
  }

  std::map<int, StiffRef> sref;
  for (size_t k = 1; k < T.order.size(); ++k) {
    int u = T.order[k], pu = T.parentT.at(u);
    if (mount.count(u)) continue;  // v's articulation: fully flexible
    StiffRef r;
    r.stiff = std::max(0.0, 0.8 - 0.3 * (T.depth.at(u) - 1));
    if (r.stiff <= 0.0) continue;
    int ppu = T.parentT.at(pu);
    TPointD refOrig;
    if (ppu >= 0) {
      r.refA = ppu, r.refB = pu;
      refOrig = orig.vertex(pu).P() - orig.vertex(ppu).P();
    } else {
      int op = orig.vertex(pu).parent();  // pu == v: reach outside the tree
      if (op >= 0 && T.parentT.count(op)) {
        r.refA = op, r.refB = pu;
        refOrig = orig.vertex(pu).P() - orig.vertex(op).P();
      } else if (op >= 0) {
        r.refA = r.refB = -1;
        r.constDir = dir(P.at(pu) - P.at(op));  // body follows v rigidly
        refOrig    = orig.vertex(pu).P() - orig.vertex(op).P();
      } else {
        r.refA = r.refB = -1;
        r.constDir = TPointD(1.0, 0.0);
        refOrig    = TPointD(1.0, 0.0);
      }
    }
    TPointD boneOrig = orig.vertex(u).P() - orig.vertex(pu).P();
    r.rel   = atan2(cross(refOrig, boneOrig), refOrig * boneOrig);
    sref[u] = r;
  }

  const int ITERS = 10;
  for (int it = 0; it < ITERS; ++it) {
    P[T.order[0]] = t;
    for (size_t k = 1; k < T.order.size(); ++k) {
      int u = T.order[k], pu = T.parentT.at(u);
      P[u] = P[pu] + T.len.at(u) * dir(P[u] - P[pu]);
    }
    for (size_t k = T.order.size(); k-- > 0;) {
      int u   = T.order[k];
      auto at = anchor.find(u);
      if (at != anchor.end()) {
        P[u] = at->second;  // a pin: nailed (even mid-chain)
        continue;
      }
      auto ct = T.childT.find(u);
      if (ct == T.childT.end() || ct->second.empty()) continue;  // v alone
      TPointD acc(0.0, 0.0);
      for (int c : ct->second)
        acc = acc + P[c] + T.len.at(c) * dir(P[u] - P[c]);
      P[u] = acc * (1.0 / (double)ct->second.size());
    }
    for (size_t k = 1; k < T.order.size(); ++k) {
      int u   = T.order[k];
      auto st = sref.find(u);
      if (anchor.count(u) || st == sref.end()) continue;
      const StiffRef &r = st->second;
      int pu            = T.parentT.at(u);
      TPointD refNow =
          (r.refA >= 0) ? dir(P[r.refB] - P[r.refA]) : r.constDir;
      TPointD restDir = rot(refNow, r.rel);
      TPointD dNow    = dir(P[u] - P[pu]);
      P[u] = P[pu] + T.len.at(u) * dir(dNow * (1.0 - r.stiff) +
                                       restDir * r.stiff);
    }
  }

  // Restore exact bone lengths from the junction outward.
  for (size_t k = 1; k < T.order.size(); ++k) {
    int u = T.order[k], pu = T.parentT.at(u);
    P[u] = P[pu] + T.len.at(u) * dir(P[u] - P[pu]);
  }

  // Re-nail each pin with a confined CCD on its exclusive branch: pure
  // rotations (lengths stay true) below the last vertex shared with other
  // chains (rotating a shared vertex would move the other pins).
  std::map<int, int> useCount;
  for (const auto &ap : anchor)
    for (int u = ap.first; u >= 0; u = T.parentT.at(u)) ++useCount[u];

  for (const auto &ap : anchor) {
    int p = ap.first;
    std::vector<int> chain;
    for (int u = p; u >= 0; u = T.parentT.at(u)) chain.push_back(u);
    std::reverse(chain.begin(), chain.end());  // v .. p
    int firstExcl = (int)chain.size();
    for (int i = 0; i < (int)chain.size(); ++i)
      if (useCount[chain[i]] < 2) {
        firstExcl = i;
        break;
      }
    // The last shared vertex (i == firstExcl-1) is a valid last pivot too,
    // rotating ONLY this branch's subtree about it: the attachment bone can
    // yield when the pin would otherwise be out of reach (the stiffness above
    // is a preference, planting is a hard constraint) — and the other chains
    // are never touched.
    for (int sweep = 0; sweep < 8; ++sweep) {
      if (norm2(ap.second - P[p]) < 1e-9) break;
      for (int i = (int)chain.size() - 2; i >= firstExcl - 1; --i) {
        const TPointD pivot = P[chain[i]];
        TPointD cur = P[p] - pivot, tgt = ap.second - pivot;
        if (norm2(cur) < 1e-8 || norm2(tgt) < 1e-8) continue;
        double ang = atan2(cross(cur, tgt), cur * tgt);
        double c = cos(ang), s = sin(ang);
        std::queue<int> q;
        q.push(chain[i + 1]);
        while (!q.empty()) {
          int u = q.front();
          q.pop();
          auto ct = T.childT.find(u);
          if (ct != T.childT.end())
            for (int w : ct->second) q.push(w);
          TPointD d = P[u] - pivot;
          P[u]      = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
        }
      }
    }
  }
}

// Completes a solved T with every off-tree vertex: whatever hangs BELOW a T
// vertex rotates rigidly with its attachment bone; the body ABOVE the tree
// (and all its limbs) translates with the tree's topmost vertex — nothing else
// is nailed. Keeps all bone lengths, so the ANGLE write-back is exact.
void rigidFollowOffTree(const PlasticSkeleton &skel,
                        const std::map<int, TPointD> &curPos,
                        std::map<int, TPointD> &desired,
                        const std::map<int, int> &parentT) {
  int top = -1;
  for (const auto &kv : parentT) {
    int op = skel.vertex(kv.first).parent();
    if (op < 0 || !parentT.count(op)) {
      top = kv.first;
      break;
    }
  }
  TPointD topShift = desired.at(top) - curPos.at(top);

  for (const auto &kv : curPos) {
    int w = kv.first;
    if (parentT.count(w)) continue;
    int a = -1;
    for (int p = skel.vertex(w).parent(); p >= 0; p = skel.vertex(p).parent())
      if (parentT.count(p)) {
        a = p;
        break;
      }
    if (a < 0) {  // body side, above the tree
      desired[w] = curPos.at(w) + topShift;
      continue;
    }
    int ap      = skel.vertex(a).parent();
    double dphi = 0.0;
    if (ap >= 0 && parentT.count(ap)) {
      TPointD od = curPos.at(a) - curPos.at(ap);
      TPointD nd = desired.at(a) - desired.at(ap);
      if (norm2(od) > 1e-12 && norm2(nd) > 1e-12)
        dphi = atan2(nd.y, nd.x) - atan2(od.y, od.x);
    }
    double c = cos(dphi), s = sin(dphi);
    TPointD d  = curPos.at(w) - curPos.at(a);
    desired[w] = desired.at(a) + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
  }
}

// Re-plant every pin except keepPin inside the `desired` position snapshot:
// each one gets a confined CCD that bends ONLY its own limb — pivots strictly
// below the point where its chain diverges from the chains already planted —
// so the body stays exactly where the drag put it (nothing else is nailed)
// and previously planted pins can never be disturbed. Returns the worst
// squared miss, so the caller can refuse drags that tear a pin off.
double replantOtherPins(const PlasticSkeleton &orig,
                        const std::map<int, TPointD> &targets,
                        std::map<int, TPointD> &desired,
                        const std::vector<int> &pins, int keepPin) {
  double worst2 = 0.0;
  std::set<int> planted;
  {
    std::vector<int> p0 = pathFromRoot(orig, keepPin);
    planted.insert(p0.begin(), p0.end());
  }

  for (int p : pins) {
    if (p == keepPin) continue;
    const TPointD target  = targets.at(p);
    std::vector<int> path = pathFromRoot(orig, p);

    int aIdx = 0;
    for (int j = (int)path.size() - 1; j >= 0; --j)
      if (planted.count(path[j])) {
        aIdx = j;
        break;
      }

    const int SWEEPS  = 10;
    const double tol2 = 1e-6;
    for (int sweep = 0; sweep < SWEEPS; ++sweep) {
      if (norm2(target - desired.at(p)) < tol2) break;
      // The anchor itself (j == aIdx) is a valid last pivot: rotating only
      // path[j+1]'s subtree bends the limb's attachment bone too — without
      // it, a body pose that puts the anchor out of the limb's reach tears
      // the pin off with no joint able to compensate. That subtree is
      // disjoint from the planted chains, so they can't be disturbed.
      for (int j = (int)path.size() - 2; j >= aIdx; --j) {
        const TPointD pivot = desired.at(path[j]);
        TPointD cur = desired.at(p) - pivot;
        TPointD tgt = target - pivot;
        if (norm2(cur) < 1e-8 || norm2(tgt) < 1e-8) continue;
        double ang = atan2(cross(cur, tgt), cur * tgt);
        double c = cos(ang), s = sin(ang);
        for (auto &kv : desired) {
          if (!isInSubtree(orig, kv.first, path[j + 1])) continue;
          TPointD d = kv.second - pivot;
          kv.second = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
        }
      }
    }

    worst2 = std::max(worst2, norm2(target - desired.at(p)));
    planted.insert(path.begin(), path.end());
  }
  return worst2;
}

// Absolute planting target (PINTX,PINTY) of each pin at the given frame —
// anchoring to these instead of the current eval positions keeps small
// per-event solver misses from accumulating into a visible drift.
std::map<int, TPointD> pinTargetsAtFrame(
    const SkDP &sd, int skelId, double frame, const std::vector<int> &pins,
    const std::map<int, TPointD> &fallback) {
  std::map<int, TPointD> targets;
  for (int p : pins) {
    SkVD *vd = sd->vertexDeformation(skelId, p);
    if (vd && vd->m_params[SkVD::PINTX] && vd->m_params[SkVD::PINTY] &&
        !(vd->m_params[SkVD::PINTX]->isDefault() &&
          vd->m_params[SkVD::PINTY]->isDefault()))
      targets[p] = TPointD(vd->m_params[SkVD::PINTX]->getValue(frame),
                           vd->m_params[SkVD::PINTY]->getValue(frame));
    else
      targets[p] = fallback.at(p);
  }
  return targets;
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
// With pins: THE PIN NEAREST TO THE DRAGGED VERTEX IS A TEMPORARY ROOT. The
// hierarchy is re-rooted at it (BFS), and v is manipulated as a single joint of
// THAT tree: the bone (reParent(v), v) rotates about reParent(v) — which stays
// fixed — carrying v and its re-rooted subtree rigidly, while everything on the
// pin side (the pin included) stays put. Rooting at the NEAREST pin keeps the
// region around the pin being posed stable under the mouse. Any OTHER active
// pin was carried along by the rigid rotation: it is re-planted right here, in
// the manipulated snapshot, by bending only its own limb (replantOtherPins) —
// the body stays free, but the written FK already satisfies every pin, so the
// eval-time constraint has nothing left to correct against the drag. The
// result is written back into the deformer's fixed root-down ANGLE params; the
// pins' absolute planting is kept per-frame at EVALUATION via their PINTX/PINTY
// targets, so pinned feet never drift on the in-betweens.
void PlasticTool::moveVertexIK_animate(double frame, int v,
                                       const TPointD &pos) {
  if (!m_sd || !skeleton()) return;
  const PlasticSkeleton &orig = *skeleton();

  std::vector<int> pins = pinnedVerticesAtFrame(frame);
  bool vIsPin = std::find(pins.begin(), pins.end(), v) != pins.end();
  if (pins.empty() || vIsPin) {
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

  // ---- Pins active: re-root at the pin nearest to v ----
  PlasticSkeleton &defSkel = deformedSkeleton();
  std::map<int, TPointD> curPos;
  for (auto vt = defSkel.vertices().begin(); vt != defSkel.vertices().end();
       ++vt)
    curPos[vt.m_idx] = vt->P();

  // v BETWEEN the pins (on their spanning subtree): a single rigid pivot
  // would nail one whole pin-side chain — hanging from a bar by both hands,
  // one shoulder couldn't lift. Solve as a multi-anchor chain instead: every
  // pin stays nailed, every chain toward v bends symmetrically, and the mouse
  // target is clamped to what the chains can reach (the drag stiffens at
  // end-of-range instead of tearing a pin off).
  if (pins.size() >= 2 && spanningOfPins(orig, pins).count(v)) {
    moveVertexMultiAnchor_animate(frame, v, pos, pins, curPos);
    return;
  }

  std::map<int, std::vector<int>> adj;
  const auto &edges = defSkel.edges();
  for (auto et = edges.begin(); et != edges.end(); ++et) {
    adj[et->vertex(0)].push_back(et->vertex(1));
    adj[et->vertex(1)].push_back(et->vertex(0));
  }

  // Nearest pin by tree distance: posing near a planted foot must behave the
  // same whichever foot got pinned first.
  int pin = -1;
  {
    std::set<int> pinSet(pins.begin(), pins.end());
    std::set<int> visited;
    std::queue<int> q;
    q.push(v);
    visited.insert(v);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      if (pinSet.count(u)) {
        pin = u;
        break;
      }
      for (int w : adj[u])
        if (!visited.count(w)) {
          visited.insert(w);
          q.push(w);
        }
    }
    if (pin < 0) return;  // no pin reachable from v
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

  // Pose at a given rotation and re-plant the other pins (bending only their
  // own limb); the returned residual tells whether some pin got out of reach.
  std::map<int, TPointD> targets =
      pinTargetsAtFrame(m_sd, ::skeletonId(), frame, pins, curPos);
  auto poseWithTheta = [&](double th) {
    double c = cos(th), s = sin(th);
    std::map<int, TPointD> desired = curPos;
    for (int u : moved) {
      TPointD d  = curPos[u] - pivot;
      desired[u] = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
    }
    double resid2 = (pins.size() > 1)
                        ? replantOtherPins(orig, targets, desired, pins, pin)
                        : 0.0;
    return std::make_pair(resid2, desired);
  };

  // Hard pins: if the full rotation tears some pin off (its limb can't reach
  // the target anymore), bisect down to the widest rotation that still keeps
  // every pin planted — the drag stiffens at end-of-range instead of escaping.
  double tol   = getPixelSize();
  double tol2  = tol * tol;
  auto attempt = poseWithTheta(theta);
  std::map<int, TPointD> desired;
  if (attempt.first <= tol2)
    desired = std::move(attempt.second);
  else {
    double lo = 0.0, hi = theta;
    desired = curPos;
    for (int i = 0; i < 8; ++i) {
      double mid = 0.5 * (lo + hi);
      auto r     = poseWithTheta(mid);
      if (r.first <= tol2) {
        lo      = mid;
        desired = std::move(r.second);
      } else
        hi = mid;
    }
  }

  writeBackAngles_animate(frame, curPos, desired);
}

//------------------------------------------------------------------------

// Symmetric posing of a vertex lying BETWEEN the pins: FABRIK over the tree
// spanned by the chains v -> each pin (all pins nailed, target clamped to
// reach), then the rest of the skeleton follows rigidly — the body above the
// tree just translates, nothing besides the pins is anchored.
void PlasticTool::moveVertexMultiAnchor_animate(
    double frame, int v, const TPointD &pos, const std::vector<int> &pins,
    const std::map<int, TPointD> &curPos) {
  const PlasticSkeleton &orig = *skeleton();
  PinTree T                   = buildPinTree(orig, curPos, v, pins);

  std::map<int, TPointD> anchor =
      pinTargetsAtFrame(m_sd, ::skeletonId(), frame, pins, curPos);

  // Clamp the mouse target inside every pin's reach (alternating projection).
  TPointD t = pos;
  for (int it = 0; it < 4; ++it)
    for (int p : pins) {
      double L = 0.0;
      for (int u = p; T.parentT.at(u) >= 0; u = T.parentT.at(u))
        L += T.len.at(u);
      TPointD d = t - anchor[p];
      double n  = norm(d);
      if (n > L) t = anchor[p] + d * (L / n);
    }

  std::map<int, TPointD> P = curPos;
  solveMultiAnchor(orig, T, anchor, t, P);

  std::map<int, TPointD> desired = curPos;
  for (const auto &kv : T.parentT) desired[kv.first] = P[kv.first];
  rigidFollowOffTree(orig, curPos, desired, T.parentT);

  writeBackAngles_animate(frame, curPos, desired);
}

//------------------------------------------------------------------------

// Converts a full desired-positions snapshot into the deformer's root-down
// ANGLE parameterization, keyframing only the joints that actually changed.
// Valid as long as `desired` preserves every bone length (rigid moves only):
// DISTANCE params are never written here.
void PlasticTool::writeBackAngles_animate(
    double frame, const std::map<int, TPointD> &curPos,
    const std::map<int, TPointD> &desired, bool clampToLimits) {
  writeBackAnglesFor_animate(*skeleton(), m_sd, ::skeletonId(), frame, curPos,
                             desired, clampToLimits);
}

//------------------------------------------------------------------------

// Column-agnostic core: `orig`/`def`/`skelId` and the position snapshots all
// belong to ONE column's own local space. Reused as-is for the current column
// (single-level paths) and once per touched column by the cross-level solver.
void PlasticTool::writeBackAnglesFor_animate(
    const PlasticSkeleton &orig, const SkDP &def, int skelId, double frame,
    const std::map<int, TPointD> &curPos,
    const std::map<int, TPointD> &desired, bool clampToLimits) {
  if (!def) return;

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
  std::function<TPointD(int)> desFn = [&](int idx) { return desired.at(idx); };

  for (const auto &kv : curPos) {
    int w  = kv.first;
    int op = orig.vertex(w).parent();
    if (op < 0) continue;  // skeleton root has no ANGLE param
    TPointD od  = orig.vertex(w).P() - orig.vertex(op).P();
    double rest = wrapPi(atan2(od.y, od.x) - parentDir(origFn, op));
    TPointD nd  = desired.at(w) - desired.at(op);
    double nAbs = (norm2(nd) > 1e-8) ? atan2(nd.y, nd.x) : 0.0;
    double newDelta = wrapPi(nAbs - parentDir(desFn, op) - rest) * M_180_PI;

    SkVD *vd = def->vertexDeformation(skelId, w);
    if (!vd) continue;
    double oldDelta = vd->m_params[SkVD::ANGLE]->getValue(frame);
    while (newDelta - oldDelta > 180.0) newDelta -= 360.0;
    while (newDelta - oldDelta < -180.0) newDelta += 360.0;

    // Angular limits (min/maxAngle), same clamp as the FK updateAngle path.
    // Children hold RELATIVE deltas, so a clamped joint rotates its whole
    // subtree rigidly: the limb stiffens at the limit instead of tearing.
    // The unpin bake opts out — it must reproduce the planted pose exactly.
    if (clampToLimits) {
      // Keyed MINANGLE/MAXANGLE override the vertex's static limit (limits can
      // change over time); no keys → the static limit.
      double loLim = orig.vertex(w).m_minAngle;
      double hiLim = orig.vertex(w).m_maxAngle;
      if (vd->m_params[SkVD::MINANGLE] &&
          vd->m_params[SkVD::MINANGLE]->getKeyframeCount() > 0)
        loLim = vd->m_params[SkVD::MINANGLE]->getValue(frame);
      if (vd->m_params[SkVD::MAXANGLE] &&
          vd->m_params[SkVD::MAXANGLE]->getKeyframeCount() > 0)
        hiLim = vd->m_params[SkVD::MAXANGLE]->getValue(frame);
      newDelta = tcrop(newDelta, loLim, hiLim);
    }

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

void PlasticTool::setIkModeState(bool on) {
  // notifyListeners only refreshes the checkbox widget — it does NOT re-enter
  // onPropertyChanged (which would re-run the bake); and even if it did, the
  // handler is a no-op here (ik-on skips the bake, ik-off with no pins returns
  // early), so this is safe either way.
  m_ikDrag.setValue(on);
  m_ikDrag.notifyListeners();
  if (m_sd) m_sd->enablePins(on);
  m_deformedSkeleton.invalidate();
  invalidate();
}

//------------------------------------------------------------------------

// Leaving IK mode = commit the pinned animation. Every keyframe is baked into
// FK (angles) + controller (rigid translation) so the pose is IDENTICAL at
// every key and nothing shifts; then all pins are dropped, freeing the rig for
// plain FK editing. Uses storeDeformedSkeleton at explicit frames (no frame
// handle juggling) and writeBackAngles (purely geometric). Undoable via a
// full per-frame keyframe snapshot (BakeToFKUndo).
void PlasticTool::bakePinsToFK_animate() {
  if (!m_sd) return;
  int skelId            = ::skeletonId();
  PlasticSkeletonP skel = m_sd->skeleton(skelId);
  if (!skel) return;

  int rootIdx = -1;
  SkVD *rvd   = rootVd_animate(&rootIdx);

  // All keyframe times in the animation + whether any pin exists at all
  std::set<double> times;
  bool anyPin = false;
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end();
       ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (!vd) continue;
    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
      if (vd->m_params[p]) vd->m_params[p]->getKeyframes(times);
    if (vd->m_params[SkVD::PIN] &&
        vd->m_params[SkVD::PIN]->getKeyframeCount() > 0)
      anyPin = true;
  }
  if (!anyPin) return;

  // Keyframe times where at least one pin is actually planting
  std::vector<double> bakeTimes;
  for (double t : times)
    if (!pinnedVerticesAtFrame(t).empty()) bakeTimes.push_back(t);
  if (bakeTimes.empty()) return;
  std::sort(bakeTimes.begin(), bakeTimes.end());

  // 1. Capture the planted pose + current controller translation at each bake
  //    time, while the pins are still active
  std::map<double, std::map<int, TPointD>> plantedByT;
  std::map<double, TPointD> origTransByT;
  for (double t : bakeTimes) {
    PlasticSkeleton ds;
    m_sd->storeDeformedSkeleton(skelId, t, ds);
    std::map<int, TPointD> pose;
    for (auto vt = ds.vertices().begin(); vt != ds.vertices().end(); ++vt)
      pose[vt.m_idx] = vt->P();
    plantedByT[t] = pose;
    if (rvd && rvd->m_params[SkVD::TRANSX] && rvd->m_params[SkVD::TRANSY])
      origTransByT[t] = TPointD(rvd->m_params[SkVD::TRANSX]->getValue(t),
                                rvd->m_params[SkVD::TRANSY]->getValue(t));
  }

  // Snapshot the full keyframe state at every affected frame for undo (BEFORE
  // any mutation). All new keys land on bakeTimes ⊂ times, and the dropped
  // pin keys live at frames in times too, so `times` is the complete set.
  std::unique_ptr<BakeToFKUndo> undo(new BakeToFKUndo);
  undo->m_frames.assign(times.begin(), times.end());
  undo->m_old.resize(undo->m_frames.size());
  for (size_t i = 0; i < undo->m_frames.size(); ++i)
    m_sd->getKeyframeAt(undo->m_frames[i], undo->m_old[i]);

  l_suspendParamsObservation = true;

  // 2. Fully un-pin: drop every PIN / PINTX / PINTY key
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end();
       ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (!vd) continue;
    for (int p : {(int)SkVD::PIN, (int)SkVD::PINTX, (int)SkVD::PINTY})
      if (vd->m_params[p]) vd->m_params[p]->clearKeyframes();
  }

  // 3. Bake the captured pose SHAPE into the ANGLE params at each bake time
  for (double t : bakeTimes)
    writeBackAngles_animate(t, plantedByT[t], plantedByT[t], false);

  // 4. Bake the rigid translation the planting added into the controller's
  //    TransX/TransY (mapped through the controller's linear part → exact
  //    even under an active squash/rotation)
  if (rvd && rootIdx >= 0 && rvd->m_params[SkVD::TRANSX] &&
      rvd->m_params[SkVD::TRANSY]) {
    for (double t : bakeTimes) {
      PlasticSkeleton fk;
      m_sd->storeDeformedSkeleton(skelId, t, fk);  // unpinned + angle-baked
      TPointD delta = plantedByT[t][rootIdx] - fk.vertex(rootIdx).P();
      if (norm2(delta) < 1e-12) continue;
      TAffine ctrl = m_sd->getSquashControllerAffine(skelId, t);
      TPointD d(ctrl.a11 * delta.x + ctrl.a12 * delta.y,
                ctrl.a21 * delta.x + ctrl.a22 * delta.y);
      TPointD base =
          origTransByT.count(t) ? origTransByT[t] : TPointD();
      ::setKeyframe(rvd->m_params[SkVD::TRANSX], t);
      rvd->m_params[SkVD::TRANSX]->setValue(t, base.x + d.x);
      ::setKeyframe(rvd->m_params[SkVD::TRANSY], t);
      rvd->m_params[SkVD::TRANSY]->setValue(t, base.y + d.y);
    }
  }

  l_suspendParamsObservation = false;

  // Snapshot the resulting state and register the undo
  undo->m_new.resize(undo->m_frames.size());
  for (size_t i = 0; i < undo->m_frames.size(); ++i)
    m_sd->getKeyframeAt(undo->m_frames[i], undo->m_new[i]);
  TUndoManager::manager()->add(undo.release());

  m_deformedSkeleton.invalidate();
  onChange();
  invalidate();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
}

//------------------------------------------------------------------------

// Geometry + display values for the angle-limit gizmo of vertex v: the
// deformed parent position, the deformed parent-bone direction (branch), the
// rest angle offset, a handle radius, and the min/max limit values to show
// (effective = keyed override or static; unset limits fall back to a spread
// around the joint's current bend so the handles are always grabbable).
bool PlasticTool::limitDisplay_animate(int v, TPointD &pp, double &branch,
                                       double &defAng, double &radius,
                                       double &minDisp, double &maxDisp) {
  if (!m_sd || v < 0) return false;
  PlasticSkeletonP skelP = skeleton();  // rest
  if (!skelP) return false;
  const PlasticSkeleton &skel    = *skelP;
  const PlasticSkeleton &defSkel = deformedSkeleton();
  const PlasticSkeletonVertex &vx = skel.vertex(v);
  int vParent = vx.parent();
  if (vParent < 0) return false;

  const PlasticSkeletonVertex &vxParent = skel.vertex(vParent);
  int vGrand = vxParent.parent();
  TPointD dirFromGrand(1, 0), dirFromDefGrand(1, 0);
  if (vGrand >= 0) {
    dirFromGrand    = vxParent.P() - skel.vertex(vGrand).P();
    dirFromDefGrand = defSkel.vertex(vParent).P() - defSkel.vertex(vGrand).P();
  }
  TPointD dirFromParent = vx.P() - vxParent.P();
  defAng = atan2(cross(dirFromGrand, dirFromParent),
                 dirFromGrand * dirFromParent) *
           M_180_PI;
  branch = atan2(dirFromDefGrand.y, dirFromDefGrand.x);
  pp     = defSkel.vertex(vParent).P();

  double bone = norm(defSkel.vertex(v).P() - pp);
  radius      = std::max(bone * 0.55, 36.0 * getPixelSize());

  SkVD *vd = m_sd->vertexDeformation(::skeletonId(), v);
  double curDelta =
      (vd && vd->m_params[SkVD::ANGLE]) ? vd->m_params[SkVD::ANGLE]->getValue(::frame()) : 0.0;
  auto eff = [&](int p, double stat) {
    return (vd && vd->m_params[p] && vd->m_params[p]->getKeyframeCount() > 0)
               ? vd->m_params[p]->getValue(::frame())
               : stat;
  };
  double rawMin = skel.vertex(v).m_minAngle;
  double rawMax = skel.vertex(v).m_maxAngle;
  double minL   = eff(SkVD::MINANGLE, rawMin);
  double maxL   = eff(SkVD::MAXANGLE, rawMax);
  minDisp = (minL > -1e9) ? minL : curDelta - 60.0;
  maxDisp = (maxL < 1e9) ? maxL : curDelta + 60.0;
  return true;
}

//------------------------------------------------------------------------

int PlasticTool::limitHitTest_animate(const TPointD &pos) {
  if (!m_showAngleLimits.getValue()) return 0;
  if (!m_svSel.hasSingleObject()) return 0;
  TPointD pp;
  double branch, defAng, r, minD, maxD;
  if (!limitDisplay_animate((int)m_svSel, pp, branch, defAng, r, minD, maxD))
    return 0;
  const double d2r = M_PI / 180.0;
  auto H = [&](double L) {
    double a = branch + (L + defAng) * d2r;
    return pp + r * TPointD(cos(a), sin(a));
  };
  double u = getPixelSize();
  if (norm(pos - H(minD)) < 9.0 * u) return 1;
  if (norm(pos - H(maxD)) < 9.0 * u) return 2;
  return 0;
}

//------------------------------------------------------------------------

void PlasticTool::limitDrag_animate(const TPointD &pos) {
  if (!m_sd || m_limitDrag == 0 || !m_svSel.hasSingleObject()) return;
  int v = (int)m_svSel;
  TPointD pp;
  double branch, defAng, r, minD, maxD;
  if (!limitDisplay_animate(v, pp, branch, defAng, r, minD, maxD)) return;

  double dir = atan2(pos.y - pp.y, pos.x - pp.x);
  double L   = (dir - branch) * M_180_PI - defAng;
  while (L > 180.0) L -= 360.0;
  while (L < -180.0) L += 360.0;

  int skelId = ::skeletonId();
  SkVD *vd   = m_sd->vertexDeformation(skelId, v);
  int p      = (m_limitDrag == 1) ? SkVD::MINANGLE : SkVD::MAXANGLE;
  bool keyed =
      vd && vd->m_params[p] && vd->m_params[p]->getKeyframeCount() > 0;

  if (keyed) {
    // Already animated → set a key at the current frame
    ::setKeyframe(vd->m_params[p], ::frame());
    vd->m_params[p]->setValue(::frame(), L);
  } else {
    // Constant limit → the static vertex value (rest + deformed), like the
    // toolbar field
    if (m_limitDrag == 1) {
      m_sd->skeleton(skelId)->vertex(v).m_minAngle = L;
      deformedSkeleton().vertex(v).m_minAngle      = L;
    } else {
      m_sd->skeleton(skelId)->vertex(v).m_maxAngle = L;
      deformedSkeleton().vertex(v).m_maxAngle      = L;
    }
  }

  // Live-update the toolbar Angle Bounds field (notifyListeners refreshes the
  // widget only, it does not re-enter onPropertyChanged)
  TStringProperty &field = (m_limitDrag == 1) ? m_minAngle : m_maxAngle;
  field.setValue(QString::number(L, 'f', 1).toStdWString());
  field.notifyListeners();

  m_deformedSkeleton.invalidate();
  invalidate();
}

//------------------------------------------------------------------------

void PlasticTool::drawAngleLimitGizmo_animate(double u) {
  if (!m_showAngleLimits.getValue()) return;
  if (!m_svSel.hasSingleObject()) return;
  TPointD pp;
  double branch, defAng, r, minD, maxD;
  if (!limitDisplay_animate((int)m_svSel, pp, branch, defAng, r, minD, maxD))
    return;

  const double d2r = M_PI / 180.0;
  auto dirOf       = [&](double L) { return branch + (L + defAng) * d2r; };
  auto H           = [&](double L) {
    double a = dirOf(L);
    return pp + r * TPointD(cos(a), sin(a));
  };

  glLineWidth(1.5f * m_viewer->getDevPixRatio());

  // Faint filled wedge of the allowed range
  glColor4ub(60, 130, 255, 40);
  glBegin(GL_TRIANGLE_FAN);
  glVertex2d(pp.x, pp.y);
  int steps = 32;
  for (int i = 0; i <= steps; ++i) {
    double L = minD + (maxD - minD) * (i / (double)steps);
    double a = dirOf(L);
    glVertex2d(pp.x + r * cos(a), pp.y + r * sin(a));
  }
  glEnd();

  // The two bound handles + their radial lines
  for (int b = 1; b <= 2; ++b) {
    double L    = (b == 1) ? minD : maxD;
    TPointD h   = H(L);
    bool active = (m_limitDrag == b) || (m_limitDrag == 0 && m_limitHi == b);
    if (active)
      glColor4ub(255, 160, 0, 255);
    else
      glColor4ub(60, 130, 255, 220);
    glBegin(GL_LINES);
    glVertex2d(pp.x, pp.y);
    glVertex2d(h.x, h.y);
    glEnd();
    double hr = 4.0 * u;
    glBegin(GL_QUADS);
    glVertex2d(h.x - hr, h.y - hr);
    glVertex2d(h.x + hr, h.y - hr);
    glVertex2d(h.x + hr, h.y + hr);
    glVertex2d(h.x - hr, h.y + hr);
    glEnd();
  }
  glLineWidth(1.0f);
}

//------------------------------------------------------------------------

double PlasticTool::nextPinActivationAfter_animate(double frame) const {
  if (!m_sd) return -1.0;
  PlasticSkeleton *skel = skeleton().getPointer();
  if (!skel) return -1.0;
  int skelId  = ::skeletonId();
  double best = -1.0;
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end();
       ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (!vd || !vd->m_params[SkVD::PIN]) continue;
    const TDoubleParamP &pin = vd->m_params[SkVD::PIN];
    for (int k = 0; k < pin->getKeyframeCount(); ++k) {
      const TDoubleKeyframe &kf = pin->getKeyframe(k);
      if (kf.m_frame > frame && kf.m_value >= 0.5 &&
          (best < 0.0 || kf.m_frame < best))
        best = kf.m_frame;
    }
  }
  return best;
}

//------------------------------------------------------------------------

std::vector<int> PlasticTool::pinnedVerticesAtFrame(double frame) const {
  std::vector<int> pins;
  if (!m_sd) return pins;
  // Pins asleep (IK mode off): single gating point — no diamonds drawn, no
  // pin-aware manipulation. Keys AND evaluation planting stay untouched, so
  // toggling IK never moves the pose.
  if (!m_sd->pinsEnabled()) return pins;
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

  // One block: this pin plus the mirrored parent-attachment pin undo together.
  TUndoManager::manager()->beginBlock();

  // Snapshot for undo (captures the PIN param, part of the SkVD keyframe).
  AnimateValuesUndo *undo = new AnimateValuesUndo(v);
  m_sd->getKeyframeAt(frame, undo->m_oldValues);

  bool pinned = vd->m_params[SkVD::PIN]->getValue(frame) >= 0.5;
  // Un-pinning the LAST active pin also drops the rigid translation that was
  // planting the pose (not representable in ANGLE params): captured before
  // writing the PIN=0 key, transferred to the controller below.
  bool wasLastPin = pinned && pinnedVerticesAtFrame(frame).size() == 1;

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

  // When unpinning, the eval-time correction (shift + limb CCD) that was
  // planting this vertex vanishes with the key, and the pose would snap back
  // to raw FK: snapshot the planted pose now, to bake it into the ANGLE
  // params right after the key is written.
  std::map<int, TPointD> planted;
  if (pinned) {
    m_deformedSkeleton.invalidate();
    PlasticSkeleton &ds = deformedSkeleton();
    for (auto vt = ds.vertices().begin(); vt != ds.vertices().end(); ++vt)
      planted[vt.m_idx] = vt->P();
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

  // Bake the previously planted pose into the ANGLE params (compare against
  // the fresh, unpinned FK): the vertex stays visually where it was. NO limit
  // clamping here — the eval-time planting ignores limits, so clamping the
  // bake could snap the pose at unpin.
  if (pinned && !planted.empty()) {
    m_deformedSkeleton.invalidate();
    PlasticSkeleton &ds = deformedSkeleton();
    std::map<int, TPointD> cur;
    for (auto vt = ds.vertices().begin(); vt != ds.vertices().end(); ++vt)
      cur[vt.m_idx] = vt->P();
    writeBackAngles_animate(frame, cur, planted, false);
  }

  // Last pin released: the planted pose's rigid translation vanished with it
  // (angles are translation-invariant), which used to shift the whole
  // character. Transfer it to the controller's TransX/TransY — keyed with a
  // confinement key one frame earlier so the still-pinned frames before this
  // one are untouched — mapped through the controller's linear part so the
  // DISPLAYED pose stays identical even under an active squash/rotation.
  if (pinned && wasLastPin && !planted.empty()) {
    m_deformedSkeleton.invalidate();
    PlasticSkeleton &ds = deformedSkeleton();  // fresh: un-pinned, angle-baked
    auto pt             = planted.find(v);
    SkVD *rvd           = rootVd_animate();
    if (pt != planted.end() && rvd) {
      TPointD t = pt->second - ds.vertex(v).P();
      if (norm2(t) > 1e-12 && rvd->m_params[SkVD::TRANSX] &&
          rvd->m_params[SkVD::TRANSY]) {
        const TAffine ctrl =
            m_sd->getSquashControllerAffine(::skeletonId(), frame);
        TPointD d(ctrl.a11 * t.x + ctrl.a12 * t.y,
                  ctrl.a21 * t.x + ctrl.a22 * t.y);
        // Confine the transfer to the un-pinned gap: it holds until the NEXT
        // pin activation, where planting takes over and the controller must
        // be back to its pre-transfer value (otherwise the advancement would
        // double up onto the following pinned range — a walk with alternating
        // feet loses its subsequent animation). No later activation → the
        // advancement persists forward (the character stays where it walked).
        double nextActive = nextPinActivationAfter_animate(frame);
        for (int p : {(int)SkVD::TRANSX, (int)SkVD::TRANSY}) {
          TDoubleParamP par = rvd->m_params[p];
          double add        = (p == SkVD::TRANSX) ? d.x : d.y;
          double closeVal   = (nextActive > 0.0) ? par->getValue(nextActive)
                                                 : 0.0;
          if (frame > 1.0) ::setKeyframe(par, frame - 1.0);
          ::setKeyframe(par, frame);
          par->setValue(frame, par->getValue(frame) + add);
          if (nextActive > 0.0) {
            ::setKeyframe(par, nextActive);
            par->setValue(nextActive, closeVal);
          }
        }
      }
    }
  }

  m_sd->getKeyframeAt(frame, undo->m_newValues);
  TUndoManager::manager()->add(undo);

  // A pin on a child column also plants the parent's attachment vertex, so the
  // proven single-level primary-pin machinery handles cross-column posing (rigid
  // rig translation, free root) with no separate cross-level solver.
  setAttachmentPin_animate(!pinned, frame);

  TUndoManager::manager()->endBlock();

  m_deformedSkeleton.invalidate();
  invalidate();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
}

//------------------------------------------------------------------------

// A pin on a CHILD column (a hand parented to the body's wrist) is mirrored onto
// the parent's attachment vertex: the child follows that vertex rigidly, so
// pinning the wrist plants the whole child in world, and single-level's
// primary-pin translation (free root, exact per-frame plant) then handles body
// posing. `on` = the child pin's NEW state. No-op when the current column is not
// a stitched child. Its own undo (parent column) rides in togglePin's block.
void PlasticTool::setAttachmentPin_animate(bool on, double frame) {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return;
  const int cur = ::column();

  // The current column's stitching parent + attachment vertex, if any.
  int pCol = -1, pV = -1;
  for (const CrossLevelLink &lk : crossLevelLinks_animate())
    if (lk.childColumn == cur) {
      pCol = lk.parentColumn;
      pV   = lk.parentVertex;
      break;
    }
  if (pCol < 0) return;  // not a stitched child: top of the chain

  TStageObject *pObj = xsh->getStageObject(TStageObjectId::ColumnId(pCol));
  if (!pObj) return;
  const PlasticSkeletonDeformationP &pDef = pObj->getPlasticSkeletonDeformation();
  if (!pDef) return;
  const int pSkel = pDef->skeletonId(pObj->paramsTime(frame));
  SkVD *vd        = pDef->vertexDeformation(pSkel, pV);
  if (!vd || !vd->m_params[SkVD::PIN]) return;

  // Already in the wanted state (e.g. another finger already planted the wrist):
  // nothing to do — and this stops the recursion from ping-ponging.
  const bool parentPinned =
      vd->m_params[SkVD::PIN]->getValue(pObj->paramsTime(frame)) >= 0.5;
  if (parentPinned == on) return;

  // Route the parent's pin through the SAME togglePinAtCurrentFrame: it captures
  // the plant target, bakes the planted pose + transfers the rigid translation
  // to the controller on release (so the character does NOT jump), and — since
  // it calls us again — recurses up the whole hierarchy (hand -> forearm ->
  // body). Save/restore the cell and selection around the temporary activation.
  const int sRow = ::row(), sCol = ::column();
  const PlasticVertexSelection sSel = m_svSel;
  {
    TemporaryActivation act(sRow, pCol);
    setSkeletonSelection(pV);
    togglePinAtCurrentFrame();
  }
  ::setCell(sRow, sCol);
  setSkeletonSelection(sSel);
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonUp_animate(const TPointD &pos,
                                       const TMouseEvent &me) {
  // Track mouse position
  m_pos = pos;

  // End of an angle-limit bound drag: the value was written live during the
  // drag (no undo, like the toolbar field). The toolbar text field refreshes
  // on the next selection change.
  if (m_limitDrag != 0) {
    m_limitDrag = 0;
    if (m_sd && m_svSel.hasSingleObject()) {
      int v             = (int)m_svSel;
      AngleLimitUndo *u = new AngleLimitUndo(v);
      u->m_oldMin       = m_limitOldMin;
      u->m_oldMax       = m_limitOldMax;
      u->m_oldValues    = m_pressedSkDF;
      if (PlasticSkeletonP skel = skeleton()) {
        u->m_newMin = skel->vertex(v).m_minAngle;
        u->m_newMax = skel->vertex(v).m_maxAngle;
      }
      m_sd->getKeyframeAt(frame(), u->m_newValues);
      TUndoManager::manager()->add(u);
    }
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    invalidate();
    return;
  }

  // End of a controller-gizmo drag: own undo path, independent of the vertex
  // selection; the global-key logic is skipped on purpose (the controller is
  // not part of the vertex transform params).
  bool gizmoDrag = (m_ctrlDevice != CtrlNone);
  m_ctrlDevice   = CtrlNone;

  if (gizmoDrag && m_dragged && m_sd) {
    // Global key: one key on every channel at this frame, controller included
    // (keyed BEFORE the undo snapshot, so undo/redo capture them too)
    if (m_globalKey.getValue()) ::setKeyframe(m_sd, ::frame());

    AnimateValuesUndo *undo =
        new AnimateValuesUndo(m_svSel.hasSingleObject() ? (int)m_svSel : -1);
    undo->m_oldValues = m_pressedSkDF;
    m_sd->getKeyframeAt(frame(), undo->m_newValues);
    TUndoManager::manager()->add(undo);

    m_dragged = false;

    updateMatrix();  // refresh the controller affine composed in the matrix
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    invalidate();
    return;
  }

  // A cross-level IK drag wrote ANGLE (and pin) params on several columns:
  // group them into one undo block, its own path independent of the single
  // column logic below.
  if (m_ikCrossDragged && m_dragged) {
    finishCrossLevelUndo_animate(::frame());
    m_dragged = false;
    updateMatrix();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
    invalidate();
    return;
  }

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

// One action = one support switch (walk cycle): pin the selected vertex at
// the CURRENT frame and release every other active pin ONE FRAME LATER — the
// double support lives on exactly this key, with no second manual unpin. The
// release goes through togglePinAtCurrentFrame at f+1, so it inherits the
// planted-pose bake and the undo handling; everything is grouped in a single
// undo block.
void PlasticTool::switchPinAtCurrentFrame() {
  if (!m_sd || !m_svSel.hasSingleObject()) return;
  double frame = ::frame();
  int v        = m_svSel;

  std::vector<int> pins = pinnedVerticesAtFrame(frame);
  bool vPinned = std::find(pins.begin(), pins.end(), v) != pins.end();

  TUndoManager::manager()->beginBlock();

  if (!vPinned) togglePinAtCurrentFrame();

  TFrameHandle *fh = TTool::getApplication()->getCurrentFrame();
  int row0         = fh->getFrame();
  fh->setFrame(row0 + 1);
  for (int p : pins) {
    if (p == v) continue;
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), p);
    if (!vd || !vd->m_params[SkVD::PIN]) continue;
    if (vd->m_params[SkVD::PIN]->getValue(::frame()) < 0.5) continue;
    setSkeletonSelection(p);
    togglePinAtCurrentFrame();
  }
  fh->setFrame(row0);
  setSkeletonSelection(v);

  TUndoManager::manager()->endBlock();

  m_deformedSkeleton.invalidate();
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

  if (m_svSel.hasSingleObject()) {
    menu->addSeparator();
    QAction *switchPin =
        menu->addAction(QObject::tr("Switch Support Pin Here"));
    ret = QObject::connect(switchPin, &QAction::triggered,
                           [this] { switchPinAtCurrentFrame(); }) &&
          ret;
  }

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

//****************************************************************************
//    SuperPlastic multi-level : connected-skeleton discovery
//****************************************************************************

std::vector<PlasticTool::ConnectedSkel>
PlasticTool::connectedSkeletons_animate() const {
  std::vector<ConnectedSkel> result;
  if (!m_sd) return result;

  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return result;
  const int curCol = ::column();
  const int fr     = ::frame();

  // BFS over the TStageObject parenting graph (parent + children), collecting
  // every column connected to the current one — the articulated character,
  // however it's spread across drawing levels.
  std::set<int> visited;
  std::queue<int> queue;
  std::vector<int> others;
  visited.insert(curCol);
  queue.push(curCol);
  while (!queue.empty()) {
    const int c       = queue.front();
    queue.pop();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    if (c != curCol) others.push_back(c);

    const TStageObjectId pid = obj->getParent();
    if (pid.isColumn() && !visited.count(pid.getIndex())) {
      visited.insert(pid.getIndex());
      queue.push(pid.getIndex());
    }
    for (TStageObject *child : obj->getChildren()) {
      const TStageObjectId cid = child->getId();
      if (cid.isColumn() && !visited.count(cid.getIndex())) {
        visited.insert(cid.getIndex());
        queue.push(cid.getIndex());
      }
    }
  }

  // For each connected Plastic column, grab its deformed skeleton and the affine
  // that brings its own draw space into the current tool's draw space:
  //   A_C = getMatrix()^-1 * getColumnMatrix(C) * controller_C
  // (getMatrix() already = getColumnMatrix(current) * controller_current).
  const TAffine curInv = getMatrix().inv();
  for (int c : others) {
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    const PlasticSkeletonDeformationP &def =
        obj->getPlasticSkeletonDeformation();
    if (!def) continue;  // rigid (non-meshed) level — no skeleton to draw yet
    const double sdFr = obj->paramsTime((double)fr);
    const int skelId  = def->skeletonId(sdFr);

    ConnectedSkel cs;
    cs.columnIndex = c;
    def->storeDeformedSkeleton(skelId, sdFr, cs.skel);
    if (cs.skel.vertices().size() == 0) continue;
    const TAffine ctrl = def->getSquashControllerAffine(skelId, sdFr);
    cs.toCur           = curInv * getColumnMatrix(c, fr) * ctrl;
    result.push_back(std::move(cs));
  }
  return result;
}

//------------------------------------------------------------------------

std::vector<PlasticTool::CrossLevelLink>
PlasticTool::crossLevelLinks_animate() {
  std::vector<CrossLevelLink> links;
  if (!m_sd) return links;
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return links;
  const int fr = ::frame();

  // All character columns (connected + the current one, which draws at identity).
  std::vector<ConnectedSkel> cols = connectedSkeletons_animate();
  {
    ConnectedSkel cur;
    cur.columnIndex = ::column();
    cur.skel        = deformedSkeleton();  // copy of the live deformed skeleton
    cur.toCur       = TAffine();
    cols.push_back(std::move(cur));
  }
  auto findCol = [&](int c) -> const ConnectedSkel * {
    for (const ConnectedSkel &cs : cols)
      if (cs.columnIndex == c) return &cs;
    return nullptr;
  };
  auto rootOf = [](const PlasticSkeleton &s) -> int {
    const tcg::list<PlasticSkeleton::vertex_type> &vs = s.vertices();
    for (auto vt = vs.begin(); vt != vs.end(); ++vt)
      if (vt->parent() < 0) return (int)vt.m_idx;
    return -1;
  };

  for (const ConnectedSkel &child : cols) {
    TStageObject *obj =
        xsh->getStageObject(TStageObjectId::ColumnId(child.columnIndex));
    if (!obj) continue;
    const TStageObjectId pid = obj->getParent();
    if (!pid.isColumn()) continue;
    const ConnectedSkel *parent = findCol(pid.getIndex());
    if (!parent) continue;  // parent isn't part of the drawn character

    // The parent handle "H<n>" names vertex n of the parent mesh.
    const std::string &h = obj->getParentHandle();
    if (h.size() < 2 || h[0] != 'H') continue;
    const int hookIndex = atoi(h.c_str() + 1);

    TStageObject *pObj = xsh->getStageObject(pid);
    const PlasticSkeletonDeformationP &pDef =
        pObj->getPlasticSkeletonDeformation();
    if (!pDef) continue;
    const int pSkelId = pDef->skeletonId(pObj->paramsTime((double)fr));
    const int pv      = pDef->vertexIndex(hookIndex, pSkelId);
    if (pv < 0) continue;
    const int cr = rootOf(child.skel);
    if (cr < 0) continue;

    CrossLevelLink lk;
    lk.childColumn     = child.columnIndex;
    lk.childRootVertex = cr;
    lk.parentColumn    = pid.getIndex();
    lk.parentVertex    = pv;
    lk.childPos        = child.toCur * child.skel.vertex(cr).P();
    lk.parentPos       = parent->toCur * parent->skel.vertex(pv).P();
    links.push_back(lk);
  }
  return links;
}

//------------------------------------------------------------------------

void PlasticTool::drawCrossLevelLinks_animate(double pixelSize) {
  const std::vector<CrossLevelLink> links = crossLevelLinks_animate();
  if (links.empty()) return;

  const int devPixRatio = m_viewer->getDevPixRatio();

  // Connecting segment (child root -> parent attachment vertex).
  glColor4ub(0, 220, 255, 255);
  glLineWidth(3.0f * devPixRatio);
  glEnable(GL_LINE_STIPPLE);
  glLineStipple(2, 0x0F0F);
  glBegin(GL_LINES);
  for (const CrossLevelLink &lk : links) {
    glVertex2d(lk.childPos.x, lk.childPos.y);
    glVertex2d(lk.parentPos.x, lk.parentPos.y);
  }
  glEnd();
  glDisable(GL_LINE_STIPPLE);

  // A big cyan cross at the parent attachment vertex — unmistakable, and still
  // visible when the child root sits exactly on it (a well-glued connection).
  const double r = 12.0 * pixelSize;
  glLineWidth(2.5f * devPixRatio);
  glBegin(GL_LINES);
  for (const CrossLevelLink &lk : links) {
    glVertex2d(lk.parentPos.x - r, lk.parentPos.y);
    glVertex2d(lk.parentPos.x + r, lk.parentPos.y);
    glVertex2d(lk.parentPos.x, lk.parentPos.y - r);
    glVertex2d(lk.parentPos.x, lk.parentPos.y + r);
  }
  glEnd();
  glLineWidth(1.0f);
}

//------------------------------------------------------------------------

std::vector<PlasticTool::CrossCol>
PlasticTool::crossColumns_animate(double frame) {
  std::vector<CrossCol> cols;
  if (!m_sd) return cols;
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return cols;
  const int fr = (int)frame;

  // Current column first: the tool evaluates its params at the tool frame and
  // getMatrix() already folds in its squash controller — so its world matrix
  // is exactly getMatrix() and its deformed skeleton is the live one.
  {
    CrossCol cc;
    cc.columnIndex = ::column();
    cc.def         = m_sd;
    cc.skelId      = ::skeletonId();
    cc.paramFrame  = frame;
    if (PlasticSkeletonP rs = skeleton()) cc.rest = *rs;
    cc.deformed = deformedSkeleton();
    cc.world    = getMatrix();
    cols.push_back(std::move(cc));
  }

  // Every hierarchically connected column, resolved into the same world frame
  // as connectedSkeletons_animate (world = column placement * squash affine).
  for (const ConnectedSkel &cs : connectedSkeletons_animate()) {
    TStageObject *obj =
        xsh->getStageObject(TStageObjectId::ColumnId(cs.columnIndex));
    if (!obj) continue;
    const PlasticSkeletonDeformationP &def =
        obj->getPlasticSkeletonDeformation();
    if (!def) continue;
    const double sdFr = obj->paramsTime((double)fr);
    const int skId    = def->skeletonId(sdFr);

    CrossCol cc;
    cc.columnIndex = cs.columnIndex;
    cc.def         = def;
    cc.skelId      = skId;
    cc.paramFrame  = sdFr;
    if (PlasticSkeletonP rs = def->skeleton(skId)) cc.rest = *rs;
    cc.deformed = cs.skel;
    cc.world =
        getColumnMatrix(cs.columnIndex, fr) * def->getSquashControllerAffine(skId, sdFr);
    cols.push_back(std::move(cc));
  }
  return cols;
}

//------------------------------------------------------------------------

PlasticTool::UnifiedGraph
PlasticTool::buildUnifiedGraph_animate(double frame) {
  UnifiedGraph g;
  std::vector<CrossCol> cols = crossColumns_animate(frame);
  if (cols.empty()) return g;

  // Nodes + positions (world) + intra-column parenting.
  for (const CrossCol &cc : cols)
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      UNode n{cc.columnIndex, (int)vt.m_idx};
      g.nodes.push_back(n);
      g.world[n] = cc.world * vt->P();
      // Rest position in the same world frame (bone lengths for reach/IK). The
      // rest skeleton shares the deformed topology/indices.
      g.rest[n]   = cc.world * cc.rest.vertex(vt.m_idx).P();
      const int p = vt->parent();
      if (p >= 0) g.parent[n] = UNode{cc.columnIndex, p};
    }

  // Cross-links turn each child column's ROOT into an ordinary joint parented to
  // the parent column's attachment vertex; the column with no cross-link parent
  // holds the single effective root.
  std::map<int, std::pair<int, int>> linkOf;
  for (const CrossLevelLink &lk : crossLevelLinks_animate())
    linkOf[lk.childColumn] = {lk.parentColumn, lk.parentVertex};
  for (const CrossCol &cc : cols)
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      if (vt->parent() >= 0) continue;  // only column roots
      UNode rootN{cc.columnIndex, (int)vt.m_idx};
      auto it = linkOf.find(cc.columnIndex);
      if (it != linkOf.end() &&
          g.world.count(UNode{it->second.first, it->second.second}))
        g.parent[rootN] = UNode{it->second.first, it->second.second};
      else
        g.root = rootN;  // no stitching parent -> the one effective root
    }

  // Children = inverse of parent.
  for (const auto &kv : g.parent) g.children[kv.second].push_back(kv.first);

  // Pins + their world targets (PINTX/PINTY when set, else current position).
  for (const CrossCol &cc : cols)
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      SkVD *vd = cc.def->vertexDeformation(cc.skelId, vt.m_idx);
      if (!vd || !vd->m_params[SkVD::PIN] ||
          vd->m_params[SkVD::PIN]->getValue(cc.paramFrame) < 0.5)
        continue;
      UNode n{cc.columnIndex, (int)vt.m_idx};
      g.pins.push_back(n);
      TPointD tLocal = vt->P();
      if (vd->m_params[SkVD::PINTX] && vd->m_params[SkVD::PINTY] &&
          !vd->m_params[SkVD::PINTX]->isDefault())
        tLocal = TPointD(vd->m_params[SkVD::PINTX]->getValue(cc.paramFrame),
                         vd->m_params[SkVD::PINTY]->getValue(cc.paramFrame));
      g.pinTarget[n] = cc.world * tLocal;
    }

  return g;
}

//------------------------------------------------------------------------

// A child column's root sits ON its parent's attachment vertex (they are glued
// by the cross-link). The root is NOT draggable (roots have no ANGLE), so a
// click there should grab the coincident PARENT vertex, which is. Redirect the
// pick in place; no-op when (col,v) is not such a stitched root.
void PlasticTool::redirectChildRootToParent_animate(int &col, int &v) {
  for (const CrossLevelLink &lk : crossLevelLinks_animate())
    if (lk.childColumn == col && lk.childRootVertex == v) {
      col = lk.parentColumn;
      v   = lk.parentVertex;
      return;
    }
}

//------------------------------------------------------------------------

// Unified FK: a single-joint rotation on the COMBINED graph, so dragging a
// parent joint turns its child columns too (their roots are ordinary chain
// joints here). Constant bone length, then the ANGLE write-back per column
// reproduces it. Returns false if it can't run (single column, root, etc.) so
// the caller falls back to the normal per-column path.
bool PlasticTool::crossLevelFK_animate(double frame, int vDragged,
                                       const TPointD &mousePos) {
  UnifiedGraph g = buildUnifiedGraph_animate(frame);
  if (g.nodes.empty()) return false;
  if (!g.pins.empty()) return false;  // pinned posing handled by the pin path

  const int curCol = ::column();
  const UNode dragged{curCol, vDragged};
  auto pit = g.parent.find(dragged);
  if (pit == g.parent.end() || pit->second.col < 0) return false;  // root/leaf

  // Multi-column only: single-column FK already works via the normal path.
  bool multi = false;
  for (const UNode &n : g.nodes)
    if (n.col != curCol) {
      multi = true;
      break;
    }
  if (!multi) return false;

  const TPointD pivot = g.world[pit->second];
  const TPointD oldD  = g.world[dragged] - pivot;
  const TPointD newD  = (getMatrix() * mousePos) - pivot;
  if (norm2(oldD) < 1e-12 || norm2(newD) < 1e-12) return false;
  const double theta = wrapPi(atan2(newD.y, newD.x) - atan2(oldD.y, oldD.x));

  // Dragged node's unified subtree (children map), rotated rigidly about pivot.
  std::set<UNode> moved;
  std::queue<UNode> q;
  q.push(dragged);
  moved.insert(dragged);
  while (!q.empty()) {
    UNode u = q.front();
    q.pop();
    auto ct = g.children.find(u);
    if (ct != g.children.end())
      for (const UNode &w : ct->second)
        if (!moved.count(w)) {
          moved.insert(w);
          q.push(w);
        }
  }

  std::map<UNode, TPointD> desired = g.world;
  const double c = cos(theta), s = sin(theta);
  for (const UNode &u : moved) {
    const TPointD d = g.world[u] - pivot;
    desired[u] = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
  }

  // Dispatch per touched column. Build the column's rest skeleton + world matrix
  // from crossColumns (buildUnifiedGraph already used them).
  std::set<int> touchedCols;
  for (const UNode &u : moved) touchedCols.insert(u.col);
  std::vector<CrossCol> cols = crossColumns_animate(frame);
  for (const CrossCol &cc : cols) {
    if (!touchedCols.count(cc.columnIndex)) continue;
    const TAffine wInv = cc.world.inv();
    std::map<int, TPointD> curLocal, desLocal;
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      curLocal[(int)vt.m_idx] = vt->P();
      desLocal[(int)vt.m_idx] = wInv * desired[UNode{cc.columnIndex, (int)vt.m_idx}];
    }
    writeBackAnglesFor_animate(cc.rest, cc.def, cc.skelId, cc.paramFrame,
                               curLocal, desLocal, true);
  }
  return true;
}

//------------------------------------------------------------------------

bool PlasticTool::hasCrossLevelPin_animate(double frame) {
  // Gated by the tool's IK mode (current deformation's flag). Other columns'
  // own pinsEnabled flag isn't managed by the tool, so we read their PIN param
  // directly — exactly as the evaluation (storeDeformedSkeleton) does.
  if (!m_sd || !m_sd->pinsEnabled()) return false;
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return false;
  const int fr = (int)frame;
  for (const ConnectedSkel &cs : connectedSkeletons_animate()) {
    TStageObject *obj =
        xsh->getStageObject(TStageObjectId::ColumnId(cs.columnIndex));
    if (!obj) continue;
    const PlasticSkeletonDeformationP &def =
        obj->getPlasticSkeletonDeformation();
    if (!def) continue;
    const double sdFr = obj->paramsTime((double)fr);
    const int skId    = def->skeletonId(sdFr);
    PlasticSkeletonP rs = def->skeleton(skId);
    if (!rs) continue;
    for (auto vt = rs->vertices().begin(); vt != rs->vertices().end(); ++vt) {
      SkVD *vd = def->vertexDeformation(skId, vt.m_idx);
      if (vd && vd->m_params[SkVD::PIN] &&
          vd->m_params[SkVD::PIN]->getValue(sdFr) >= 0.5)
        return true;
    }
  }
  return false;
}

//------------------------------------------------------------------------

// Drop the cached per-frame placements of every column connected to the
// current one. getPlacement/computeLocalPlacement cache by frame time
// (lazyData().m_time), so WITHIN one frame they never refresh on their own:
// after skeleton angles are written, a column parented to a mesh vertex would
// keep reading a stale attachment position without this.
void PlasticTool::invalidateConnectedPlacements_animate() {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return;
  std::set<int> visited;
  std::queue<int> queue;
  visited.insert(::column());
  queue.push(::column());
  while (!queue.empty()) {
    const int c = queue.front();
    queue.pop();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    obj->invalidate();
    const TStageObjectId pid = obj->getParent();
    if (pid.isColumn() && !visited.count(pid.getIndex())) {
      visited.insert(pid.getIndex());
      queue.push(pid.getIndex());
    }
    for (TStageObject *child : obj->getChildren()) {
      const TStageObjectId cid = child->getId();
      if (cid.isColumn() && !visited.count(cid.getIndex())) {
        visited.insert(cid.getIndex());
        queue.push(cid.getIndex());
      }
    }
  }
}

//------------------------------------------------------------------------

// First move of a cross-level drag: per-column undo baselines, plus the WORLD
// anchor of every pin sitting on a column other than the current one. Captured
// from the PRE-drag pose, the anchors are the stable targets the CCD pass
// chases for the whole drag -- chasing the pins' live evaluated positions
// instead (which move with their column) was the earlier feedback loop.
void PlasticTool::ensureCrossLevelBaselines_animate(double frame) {
  if (m_ikCrossDragged) return;

  invalidateConnectedPlacements_animate();
  m_deformedSkeleton.invalidate();
  std::vector<CrossCol> cols = crossColumns_animate(frame);
  const int curCol           = ::column();

  for (const CrossCol &cc : cols) {
    SkDKey base;
    cc.def->getKeyframeAt(cc.paramFrame, base);
    m_ikCrossOld[cc.columnIndex]  = base;
    m_ikCrossDefs[cc.columnIndex] = cc.def;

    if (cc.columnIndex == curCol) continue;  // local pins: single-level machinery
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      SkVD *vd = cc.def->vertexDeformation(cc.skelId, vt.m_idx);
      if (vd && vd->m_params[SkVD::PIN] &&
          vd->m_params[SkVD::PIN]->getValue(cc.paramFrame) >= 0.5)
        m_ikCrossPinWorld[{cc.columnIndex, (int)vt.m_idx}] = cc.world * vt->P();
    }
  }
  m_ikCrossDragged = true;
}

//------------------------------------------------------------------------

// End-effector pass (cross-level pins), run AFTER the normal local
// manipulation of each move. The pose the user is authoring stays untouched;
// for every pin living on ANOTHER column (a pinned hand column, say) the chain
// of ancestor joints between the drag and that column's attachment is bent
// with CCD so the attachment carries the pinned column back onto its captured
// world anchor.
//
// The rig fact this leans on (tstageobject.cpp:1532): a column parented to a
// mesh vertex inherits TRANSLATION only -- the pinned column rigidly follows
// its attachment vertex, never rotating with it. So (pin - attachment) is a
// constant world offset for the whole drag, holding the pin at W is exactly
// holding the attachment at W - offset, and the pinned column's own params are
// never touched. A rotation at a pivot propagates to the columns hanging below
// it as a pure translation; likewise the eval plant of the pinned column works
// in LOCAL space only (forcing the world anchor into PINTX/PINTY would shear
// the hand off its wrist), so the world constraint lives entirely here.
// Cross-level IK reach. The body is posed FREELY by moveVertexIK (the current
// column carries no pin, so it runs the normal root-free single-joint drag);
// this pass then bends the ancestor joint chain of every pin that lives on
// another column so those pins return to the world anchors captured at drag
// start. Keeping a pinned hand still while the body moves REQUIRES the arm to
// bend (the wrist is rigidly part of the body) -- re-rooting instead nails the
// body to the pin, which is not what an end-effector pin should feel like.
void PlasticTool::crossLevelSolve_animate(double frame, int vDragged,
                                          const TPointD &mousePos) {
  typedef std::pair<int, int> Node;
  (void)mousePos;
  if (m_ikCrossPinWorld.empty()) return;

  invalidateConnectedPlacements_animate();
  m_deformedSkeleton.invalidate();
  std::vector<CrossCol> cols = crossColumns_animate(frame);
  if (cols.empty()) return;
  auto findCol = [&cols](int col) -> CrossCol * {
    for (CrossCol &cc : cols)
      if (cc.columnIndex == col) return &cc;
    return nullptr;
  };
  const int curCol = ::column();

  // World node positions + per-column children (original intra-column parenting).
  std::map<Node, TPointD> nodePos;
  std::map<int, std::map<int, std::vector<int>>> childrenOf;
  for (const CrossCol &cc : cols)
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      nodePos[{cc.columnIndex, (int)vt.m_idx}] = cc.world * vt->P();
      const int p = vt->parent();
      if (p >= 0) childrenOf[cc.columnIndex][p].push_back((int)vt.m_idx);
    }

  // Column-stitching topology.
  std::map<int, std::pair<int, int>> linkOf;              // childCol->(pCol,pV)
  std::map<int, std::vector<std::pair<int, int>>> linksUnder;  // pCol->[(cCol,pV)]
  for (const CrossLevelLink &lk : crossLevelLinks_animate()) {
    linkOf[lk.childColumn] = {lk.parentColumn, lk.parentVertex};
    linksUnder[lk.parentColumn].push_back({lk.childColumn, lk.parentVertex});
  }

  std::map<int, TPointD> colOffset;  // accumulated rigid translation per column
  std::set<int> touched;

  std::function<void(int, const TPointD &)> translateColumn;
  translateColumn = [&](int col, const TPointD &d) {
    CrossCol *cc = findCol(col);
    if (!cc) return;
    for (auto vt = cc->deformed.vertices().begin();
         vt != cc->deformed.vertices().end(); ++vt)
      nodePos[{col, (int)vt.m_idx}] += d;
    colOffset[col] += d;
    auto lu = linksUnder.find(col);
    if (lu != linksUnder.end())
      for (const auto &l : lu->second) translateColumn(l.first, d);
  };

  // Rotate childV's intra-column subtree about a pivot; child columns hanging
  // off any rotated vertex follow by pure translation.
  auto rotateSubtree = [&](const Node &pivotN, int childV, double theta) {
    const int X     = pivotN.first;
    const TPointD C = nodePos[pivotN];
    const double co = cos(theta), si = sin(theta);
    std::vector<int> stack(1, childV), sub;
    while (!stack.empty()) {
      const int u = stack.back();
      stack.pop_back();
      sub.push_back(u);
      for (int w : childrenOf[X][u]) stack.push_back(w);
    }
    std::set<int> rot(sub.begin(), sub.end());
    std::map<int, TPointD> attachOld;
    auto lu = linksUnder.find(X);
    if (lu != linksUnder.end())
      for (const auto &l : lu->second)
        if (rot.count(l.second)) attachOld[l.second] = nodePos[{X, l.second}];
    for (int u : sub) {
      TPointD &P      = nodePos[{X, u}];
      const TPointD d = P - C;
      P = C + TPointD(co * d.x - si * d.y, si * d.x + co * d.y);
    }
    touched.insert(X);
    if (lu != linksUnder.end())
      for (const auto &l : lu->second)
        if (rot.count(l.second))
          translateColumn(l.first, nodePos[{X, l.second}] - attachOld[l.second]);
  };

  // Node parent: intra-column skeleton parent; a column root climbs into its
  // parent column at the attachment vertex.
  auto parentOfNode = [&](const Node &n, Node &out) -> bool {
    CrossCol *cc = findCol(n.first);
    if (!cc) return false;
    const int p = cc->deformed.vertex(n.second).parent();
    if (p >= 0) {
      out = {n.first, p};
      return true;
    }
    auto it = linkOf.find(n.first);
    if (it == linkOf.end() || !findCol(it->second.first)) return false;
    out = {it->second.first, it->second.second};
    return true;
  };

  const Node draggedNode{curCol, vDragged};
  const double tol2 = 0.25 * getPixelSize() * getPixelSize();

  for (const auto &pw : m_ikCrossPinWorld) {
    const Node pinNode{pw.first.first, pw.first.second};
    const TPointD W = pw.second;
    if (!nodePos.count(pinNode)) continue;
    auto lit = linkOf.find(pinNode.first);
    if (lit == linkOf.end()) continue;  // pinned column is the hierarchy root
    const Node aNode{lit->second.first, lit->second.second};
    if (!nodePos.count(aNode)) continue;

    // Attachment chain upward.
    std::vector<Node> chain(1, aNode);
    {
      std::set<Node> seen{pinNode, aNode};
      Node cur = aNode, up;
      while ((int)chain.size() < 256 && parentOfNode(cur, up)) {
        if (seen.count(up)) break;
        chain.push_back(up);
        seen.insert(up);
        cur = up;
      }
    }

    // Pivots: every joint from the attachment up to (not incl) the chain root,
    // skipping the user's dragged joint (its pose is what they author). The
    // chain root stays the fixed IK base.
    std::vector<int> pivots;
    for (int i = 1; i + 1 < (int)chain.size(); ++i) {
      if (chain[i] == draggedNode) continue;
      if (chain[i].first == chain[i - 1].first) pivots.push_back(i);
    }
    if (pivots.empty()) continue;

    std::map<int, int> entryOfCol;
    for (int i = 0; i < (int)chain.size(); ++i)
      if (!entryOfCol.count(chain[i].first)) entryOfCol[chain[i].first] = i;

    const int SWEEPS = 24;
    for (int sweep = 0; sweep < SWEEPS; ++sweep) {
      if (norm2(W - nodePos[pinNode]) < tol2) break;
      for (int pi = (int)pivots.size() - 1; pi >= 0; --pi) {
        const int i        = pivots[pi];
        const Node &pivotN = chain[i];
        const Node &childN = chain[i - 1];
        // Steer the whole pin toward W by rotating this pivot: use the pin as
        // the effector directly (it translates rigidly with the pivot's turn).
        const TPointD C   = nodePos[pivotN];
        const TPointD cur = nodePos[pinNode] - C;
        const TPointD tgt = W - C;
        if (norm2(cur) < 1e-8 || norm2(tgt) < 1e-8) continue;
        double th = atan2(cross(cur, tgt), cur * tgt);
        if (fabs(th) < 1e-7) continue;
        rotateSubtree(pivotN, childN.second, th);
      }
    }
  }

  for (int colIdx : touched) {
    CrossCol *cc = findCol(colIdx);
    if (!cc) continue;
    const TPointD off  = colOffset.count(colIdx) ? colOffset[colIdx] : TPointD();
    const TAffine wInv = cc->world.inv();
    std::map<int, TPointD> curLocal, desLocal;
    for (auto vt = cc->deformed.vertices().begin();
         vt != cc->deformed.vertices().end(); ++vt) {
      curLocal[(int)vt.m_idx] = vt->P();
      desLocal[(int)vt.m_idx] =
          wInv * (nodePos[{colIdx, (int)vt.m_idx}] - off);
    }
    writeBackAnglesFor_animate(cc->rest, cc->def, cc->skelId, cc->paramFrame,
                               curLocal, desLocal, true);
  }
}

//------------------------------------------------------------------------

void PlasticTool::finishCrossLevelUndo_animate(double frame) {
  if (!m_ikCrossDragged) return;
  const int row = ::row();

  TUndoManager *um = TUndoManager::manager();
  um->beginBlock();
  for (auto &kv : m_ikCrossDefs) {
    const int col = kv.first;
    const SkDP &def = kv.second;
    if (!def) continue;
    // paramFrame: current column at the tool frame, others at their params time.
    double pf = frame;
    if (col != ::column()) {
      TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
      if (TStageObject *obj =
              xsh ? xsh->getStageObject(TStageObjectId::ColumnId(col)) : 0)
        pf = obj->paramsTime(frame);
    }
    AnimateValuesUndo *undo = new AnimateValuesUndo(
        col == ::column() && m_svSel.hasSingleObject() ? (int)m_svSel : -1, row,
        col);
    undo->m_oldValues = m_ikCrossOld[col];
    def->getKeyframeAt(pf, undo->m_newValues);
    um->add(undo);
  }
  um->endBlock();

  m_ikCrossDragged = false;
  m_ikCrossOld.clear();
  m_ikCrossDefs.clear();
  m_ikCrossPinWorld.clear();
}

//------------------------------------------------------------------------

void PlasticTool::draw_animate() {
  double pixelSize = getPixelSize();

  PlasticSkeleton &deformedSkeleton = this->deformedSkeleton();

  // Draw deformed skeleton
  if (m_sd) {
    // SuperPlastic multi-level: draw the skeletons of the hierarchically
    // connected columns as dimmed context, so the whole articulated character
    // (spread across several drawing levels) is visible at once. Each is placed
    // via its own affine into the current tool's draw space.
    for (const ConnectedSkel &cs : connectedSkeletons_animate()) {
      glPushMatrix();
      tglMultMatrix(cs.toCur);
      drawSkeleton(cs.skel, pixelSize, 90);
      glPopMatrix();
    }

    drawOnionSkinSkeletons_animate(pixelSize);
    drawSkeleton(deformedSkeleton, pixelSize);

    // SuperPlastic multi-level: the cross-level joints (child root ↔ parent
    // attachment vertex) that stitch the per-level skeletons into one graph.
    // Drawn ON TOP of the skeletons so the thick bone edges don't cover it.
    drawCrossLevelLinks_animate(pixelSize);
    drawSelections(m_sd, deformedSkeleton, pixelSize);
    drawAngleLimits(m_sd, m_skelId, m_svSel, pixelSize);

    // SuperPlastic: mark every IK anchor (pinned vertex) at the current frame
    // with a cyan diamond so the animator sees what is planted — during the
    // double-support frame of a walk there can be two at once. The SELECTED
    // pin is drawn filled, so toggling/untoggling always has a visible target.
    double r = 9.0 * pixelSize;
    int selV = m_svSel.hasSingleObject() ? (int)m_svSel : -1;
    glLineWidth(2.0f * m_viewer->getDevPixRatio());
    for (int pin : pinnedVerticesAtFrame(::frame())) {
      TPointD p = deformedSkeleton.vertex(pin).P();
      if (pin == selV) {
        glColor4ub(0, 220, 255, 160);
        glBegin(GL_QUADS);
        glVertex2d(p.x, p.y - r);
        glVertex2d(p.x + r, p.y);
        glVertex2d(p.x, p.y + r);
        glVertex2d(p.x - r, p.y);
        glEnd();
      }
      glColor4ub(0, 220, 255, 255);
      glBegin(GL_LINE_LOOP);
      glVertex2d(p.x, p.y - r);
      glVertex2d(p.x + r, p.y);
      glVertex2d(p.x, p.y + r);
      glVertex2d(p.x - r, p.y);
      glEnd();
    }
    glLineWidth(1.0f);

    // SuperPlastic controller gizmo: full Animate-tool replica ON TOP of the
    // skeleton (pivot / move / rotate / scale / shear), with the dynamic
    // background-contrast colors of the Ztoryc Animate tool.
    drawController_animate(pixelSize);

    // Angle-limit gizmo: draggable min/max bound handles for the selected
    // joint, with the allowed-range wedge.
    drawAngleLimitGizmo_animate(pixelSize);
  }

  drawHighlights(m_sd, &deformedSkeleton, pixelSize);
}
