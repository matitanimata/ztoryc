

// TnzCore includes
#include "tundo.h"

#include <QDebug>
#include "tgl.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/stage.h"  // Stage::inch (column X/Y are in inches)

#include "plastictool.h"
#include "ext/plasticdeformerstorage.h"
#include "tmeshimage.h"
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

// ZtoRig: undo for the STAGE-object transform key that Global-All posing now
// also writes (the plastic key has its own AnimateValuesUndo). Restores the
// frame to exactly what it was — a key with its old values, or no key at all.
class StageTransformKeyUndo final : public TUndo {
  TXsheetHandle *m_xsh;
  TStageObjectId m_id;
  int m_frame;
  bool m_hadKey;
  TStageObject::Keyframe m_oldKey;

  TStageObject *obj() const {
    return (m_xsh && m_xsh->getXsheet())
               ? m_xsh->getXsheet()->getStageObject(m_id)
               : 0;
  }

public:
  StageTransformKeyUndo(TXsheetHandle *xsh, const TStageObjectId &id, int frame,
                        bool hadKey, const TStageObject::Keyframe &oldKey)
      : m_xsh(xsh)
      , m_id(id)
      , m_frame(frame)
      , m_hadKey(hadKey)
      , m_oldKey(oldKey) {}

  int getSize() const override { return sizeof(*this); }

  void undo() const override {
    TStageObject *o = obj();
    if (!o) return;
    if (m_hadKey)
      o->setKeyframeWithoutUndo(m_frame, m_oldKey);
    else
      o->removeKeyframeWithoutUndo(m_frame);
    o->updateKeyframes();
    if (m_xsh) m_xsh->notifyXsheetChanged();
  }
  void redo() const override {
    TStageObject *o = obj();
    if (!o) return;
    o->setKeyframeWithoutUndo(m_frame);
    o->updateKeyframes();
    if (m_xsh) m_xsh->notifyXsheetChanged();
  }
};

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

  // Corrective sculpt owns the drag entirely: no skeleton posing while brushing.
  if (m_correctiveSculpt.getValue()) {
    // Ctrl picks a joint without leaving the mode: you need two of them to say
    // "this bone", and going out to Animate and back to do it is absurd.
    if (me.isCtrlPressed() || me.isShiftPressed()) {
      if (m_svHigh >= 0) {
        if (me.isShiftPressed()) {
          std::vector<int> objs = m_svSel.objects();
          if (std::find(objs.begin(), objs.end(), m_svHigh) == objs.end())
            objs.push_back(m_svHigh);
          std::sort(objs.begin(), objs.end());
          PlasticVertexSelection sel;
          sel.setObjects(objs);
          setSkeletonSelection(sel);
        } else
          setSkeletonSelection(m_svHigh);
        invalidate();
      }
      return;
    }
    m_correctiveErase = me.isAltPressed();  // Alt takes ownership away
    beginCorrectiveStroke_animate();
    return;
  }

  // DIAGNOSTIC (2026-07-20, opt-in via ZTORYC_SUSPEND_PLANT): for the duration
  // of the drag, let FABRIK alone own the pose. See plasticskeletondeformation.h.
  PlasticPinSolver::setSolveSuspended(true);

  // Fresh drag: drop any cross-level IK state left over from an aborted drag.
  m_ikCrossDragged = false;
  m_ikCrossOld.clear();
  m_ikCrossDefs.clear();
  m_ikCrossPinWorld.clear();
  m_ikCrossBaseValid = false;
  m_ikCrossBaseGraph = UnifiedGraph();
  m_ikCrossBaseCols.clear();

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
      m_ctrlPressMatrix = getMatrix();
      m_scaleDragCenter = gizmoC;
      m_scaleOldX       = oldVal(SkVD::SCALEX, 1.0);
      m_scaleOldY       = oldVal(SkVD::SCALEY, 1.0);
      m_ctrlOldRot      = oldVal(SkVD::ROT, 0.0);
      m_ctrlOldTX       = oldVal(SkVD::TRANSX, 0.0);
      m_ctrlOldTY       = oldVal(SkVD::TRANSY, 0.0);
      m_ctrlOldShX      = oldVal(SkVD::SHEARX, 0.0);
      m_ctrlOldShY      = oldVal(SkVD::SHEARY, 0.0);

      // Move handle on a stitched child column steers the COLUMN's X/Y (see
      // m_ctrlChildColumn): snapshot those too.
      m_ctrlChildColumn = false;
      m_ctrlOldColX = m_ctrlOldColY = 0.0;
      {
        const int cur = ::column();
        for (const CrossLevelLink &lk : crossLevelLinks_animate())
          if (lk.childColumn == cur) {
            m_ctrlChildColumn = true;
            break;
          }
        if (m_ctrlChildColumn) {
          TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
          TStageObject *obj =
              xsh ? xsh->getStageObject(TStageObjectId::ColumnId(cur)) : nullptr;
          if (obj) {
            m_ctrlOldColX = obj->getParam(TStageObject::T_X, ::frame());
            m_ctrlOldColY = obj->getParam(TStageObject::T_Y, ::frame());
          } else
            m_ctrlChildColumn = false;
        }
      }

      // Snapshot the active cross-column pin targets (PINW): Move on the top
      // column is LOCKED while a pin is planted (the foot wins), and Ctrl+drag
      // moves everything with the pins traveling along — the snapshot is the
      // baseline for that shift.
      m_ctrlPinWSnapshot.clear();
      if (!m_ctrlChildColumn) {
        for (const CrossCol &cc : crossColumns_animate(::frame())) {
          if (!cc.def) continue;
          SkD::vd_iterator vdt, vdEnd;
          cc.def->vertexDeformations(vdt, vdEnd);
          for (; vdt != vdEnd; ++vdt) {
            SkVD *pvd = (*vdt).second;
            if (!pvd->m_params[SkVD::PIN] || !pvd->m_params[SkVD::PINWX] ||
                !pvd->m_params[SkVD::PINWY])
              continue;
            if (pvd->m_params[SkVD::PIN]->getValue(cc.paramFrame) < 0.5)
              continue;
            if (pvd->m_params[SkVD::PINWX]->getKeyframeCount() == 0 &&
                pvd->m_params[SkVD::PINWY]->getKeyframeCount() == 0)
              continue;
            CtrlPinW e;
            e.px       = pvd->m_params[SkVD::PINWX];
            e.py       = pvd->m_params[SkVD::PINWY];
            e.oldW     = TPointD(e.px->getValue(cc.paramFrame),
                                 e.py->getValue(cc.paramFrame));
            e.keyFrame = cc.paramFrame;
            m_ctrlPinWSnapshot.push_back(e);
          }
        }
      }

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
  } else if (me.isShiftPressed() && m_svHigh >= 0) {
    // Shift adds to the selection instead of replacing it — the usual
    // convention. Several joints at once is how you address a whole bone, and
    // how you set their stacking order together instead of one at a time.
    std::vector<int> objs = m_svSel.objects();
    std::vector<int>::iterator it =
        std::find(objs.begin(), objs.end(), m_svHigh);
    if (it != objs.end())
      objs.erase(it);  // clicking a selected joint again drops it
    else
      objs.push_back(m_svHigh);
    std::sort(objs.begin(), objs.end());
    PlasticVertexSelection sel;
    sel.setObjects(objs);
    setSkeletonSelection(sel);
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

// IK damping, driven by the tool's "IK Damping" slider. Lives here as a plain
// value because the CCD helpers below are free functions: they must not reach
// back into the tool, and re-reading a TDoubleProperty per rotation would be
// absurd. Set once per drag event, in leftButtonDrag_animate.
double l_ikDampPercent = 12.0;

//! Largest rotation, in radians, that one step of posing may apply to a joint.
//! The slider reads directly in DEGREES per step.
/*!
  Expressed in degrees, not as a distance budget, after measuring the thing it
  has to control. A cap on how far the pinned END travels grows as the lever
  shortens — right for a CCD step, exactly backwards for the joints near the
  root: there the lever is short and the body hanging off it is long, so a few
  degrees fling everything above. What has to be bounded is the ANGLE, because
  the swing of the far body is proportional to it and to nothing else.

  That is why the hips ran away at every slider value while the other joints
  behaved: their rotation never went through the damped CCD at all, and the cap
  that did exist grew for precisely the geometry that needed it smallest.
*/
static double ikMaxStep() { return l_ikDampPercent * (M_PI / 180.0); }

void PlasticTool::leftButtonDrag_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  if (m_correctiveSculpt.getValue()) {
    const TPointD prev = m_pos;
    m_pos              = pos;
    applyCorrectiveBrush_animate(prev, pos);
    return;
  }

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
  // The pin may be on THIS column (single-level) OR on a connected child column
  // (cross-level): in the latter case the unified solver re-roots at that pin, so
  // this column's root must be draggable too — otherwise the body root stays
  // locked while a foot is pinned on a child leg.
  bool isRoot = m_svSel.hasSingleObject() &&
                deformedSkeleton().vertex(m_svSel).parent() < 0;
  bool ikPin  = m_ikDrag.getValue() && (pinnedVertexAtFrame(frame) >= 0 ||
                                       hasCrossLevelPin_animate(frame));

  // Dragging a PINNED vertex must do nothing — the pin is precisely the
  // statement that this vertex stays put. The cross-level solver declines the
  // drag (the pin is its fixed re-root base) and the FK fallback below would
  // then happily move it off its target, silently breaking the plant with no
  // visible cause.
  if (m_ikDrag.getValue() && m_sd && m_svSel.hasSingleObject()) {
    SkVD *pvd = m_sd->vertexDeformation(::skeletonId(), m_svSel);
    if (pvd && pvd->m_params[SkVD::PIN] &&
        pvd->m_params[SkVD::PIN]->getValue(frame) >= 0.5)
      return;
  }

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
      // Cross-column pin (a foot on a child leg) → unified IK on the combined
      // graph (STEP A): re-root at the pin so the foot holds while the body
      // articulates. If it doesn't apply, FK posing (no pins) also runs on the
      // unified graph; otherwise fall back to the single-level pin path.
      l_ikDampPercent = m_ikDamping.getValue();

      const bool diag = ::getenv("ZTORYC_PIN_DIAG") != nullptr;

      // GAIN probe: how far the dragged joint actually travels compared with
      // how far the mouse asked it to. A joint that "takes off" is a gain
      // above 1 — the pose answering with MORE than it was asked for, which is
      // the signature of a feedback loop, not of a stiff solver.
      TPointD beforeP, askDelta;
      if (diag) {
        beforeP  = deformedSkeleton().vertex(m_svSel).P();
        askDelta = pos - beforeP;
      }

      const char *path = "moveVertexIK";
      if (crossLevelIK_animate(frame, m_svSel, pos))
        path = "crossLevelIK";
      else if (crossLevelFK_animate(frame, m_svSel, pos))
        path = "crossLevelFK";
      else
        moveVertexIK_animate(frame, m_svSel, pos);

      if (diag) {
        m_deformedSkeleton.invalidate();
        const TPointD gotDelta = deformedSkeleton().vertex(m_svSel).P() - beforeP;
        const double ask = norm(askDelta), got = norm(gotDelta);
        qDebug().noquote()
            << QString("[PIN_GAIN] %1 v=%2 asked=%3 got=%4 gain=%5")
                   .arg(path)
                   .arg(m_svSel)
                   .arg(ask, 0, 'f', 3)
                   .arg(got, 0, 'f', 3)
                   .arg(ask > 1e-9 ? got / ask : 0.0, 0, 'f', 2);
      }
    } else if (m_keepDistance.getValue()) {
      ::setKeyframe(vd->m_params[SkVD::ANGLE],
                    frame);  // Set a keyframe for it. It must be done
                             // to set the correct function interpolation
                             // type and other stuff.
      m_sd->updateAngle(*skeleton(), deformedSkeleton(), frame, m_svSel, pos,
                           parentBoneRefDeg_animate());
    } else {
      ::setKeyframe(vd->m_params[SkVD::ANGLE],
                    frame);  // Same here. NOTE: Not setting a frame on
      ::setKeyframe(vd->m_params[SkVD::DISTANCE],
                    frame);  // vd directly due to SkVD::SO

      m_sd->updatePosition(*skeleton(), deformedSkeleton(), frame, m_svSel, pos,
                           parentBoneRefDeg_animate());
    }

    l_suspendParamsObservation = false;

    // onChange();                                                     // Due to
    // a nasty Function Editor dependency,
    // it's better to call the following directly
    m_deformedSkeleton.invalidate();
    invalidate();

    // A child column (following its parent via a hook) needs a full xsheet
    // refresh DURING the drag: otherwise its mesh/placement is re-evaluated
    // only on release, so the skeleton overlay drifts from the mesh mid-drag
    // and snaps back at the end. Guarded so plain single columns stay light.
    // Symmetric case: dragging a PARENT with column children needs the same
    // refresh — a child's attachment point (getHandlePos, which reads the
    // parent's OWN pin-planted deformed skeleton) and getColumnMatrix cache
    // (TStageObject::lazyData().m_time, same-frame) otherwise stay stale
    // through the drag, so a pinned child (e.g. a foot) drifts along with the
    // parent instead of holding, until the next full invalidate on release.
    if (TXsheet *xsh =
            TTool::getApplication()->getCurrentXsheet()->getXsheet()) {
      TStageObject *obj =
          xsh->getStageObject(TStageObjectId::ColumnId(::column()));
      bool hasColumnChild = false;
      if (obj)
        for (TStageObject *child : obj->getChildren())
          if (child->getId().isColumn()) {
            hasColumnChild = true;
            break;
          }
      if (obj && (obj->getParent().isColumn() || hasColumnChild))
        TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    }
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
  updateMatrix();  // keep the overlay on the mesh during the drag (see
                   // controllerDrag_animate)
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
void PlasticTool::controllerDrag_animate(const TPointD &rawPos,
                                         const TMouseEvent &me) {
  // Re-project the mouse into the frozen press-time tool space: the live
  // matrix moves with the values being written (see m_ctrlPressMatrix), and a
  // delta computed in a self-moving space cancels itself.
  const TPointD pos = m_ctrlPressMatrix.inv() * (getMatrix() * rawPos);

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

    // Stitched child column: drive the COLUMN's own X/Y so the whole child
    // (mesh + skeleton + its own children) slides on its attachment, instead of
    // the controller sliding the drawing alone. Convert the tool-space drag into
    // the parent's space, in inches: world = M_linear * d, and column X/Y feed
    // computeLocalPlacement through the parent placement, scaled by Stage::inch.
    if (m_ctrlChildColumn) {
      TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
      TStageObject *obj =
          xsh ? xsh->getStageObject(TStageObjectId::ColumnId(::column()))
              : nullptr;
      if (obj) {
        // d lives in press-time tool space → its world image goes through the
        // press-time matrix too (the live one already moved with the drag).
        const TAffine &M = m_ctrlPressMatrix;
        const TPointD w(M.a11 * d.x + M.a12 * d.y, M.a21 * d.x + M.a22 * d.y);
        TStageObject *par =
            obj->getParent().isColumn()
                ? xsh->getStageObject(obj->getParent())
                : nullptr;
        TPointD local = w;
        if (par) {
          const TAffine pInv = par->getPlacement(frame).inv();
          local = TPointD(pInv.a11 * w.x + pInv.a12 * w.y,
                          pInv.a21 * w.x + pInv.a22 * w.y);
        }
        local = local * (1.0 / Stage::inch);

        l_suspendParamsObservation = true;
        for (int ch : {(int)TStageObject::T_X, (int)TStageObject::T_Y}) {
          TDoubleParam *p = obj->getParam((TStageObject::Channel)ch);
          if (!p) continue;
          TDoubleParamP pp(p);
          ::setKeyframe(pp, frame);
          p->setValue(frame, (ch == TStageObject::T_X)
                                 ? m_ctrlOldColX + local.x
                                 : m_ctrlOldColY + local.y);
        }
        l_suspendParamsObservation = false;

        TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
        updateMatrix();
        invalidate();
        return;
      }
    }

    // Active cross-column pins: a rigid whole-character translation contradicts
    // a world-planted foot, and the stage-level hold would cancel it outright —
    // so plain Move is LOCKED (the foot wins). Cmd/Ctrl+drag moves everything
    // WITH the pins traveling along (single-level precedent: the in-skeleton
    // plant rides the controller): reposition a whole walk cycle and the
    // character keeps walking forward. ONLY the pin targets are written — the
    // controller TRANS would be canceled by the hold anyway and would just
    // pollute the channel for the later unpin/bake. Keys glide IN (Linear
    // prevType, so repositioning at two frames interpolates instead of
    // snapping) and hold OUT (Constant type: the foot stays planted after).
    // NOTE: the PINW shift on the child columns is not covered by the gizmo
    // undo (current column only) — accepted for now.
    if (!m_ctrlPinWSnapshot.empty()) {
      if (!me.isCtrlPressed()) return;  // locked (hover hint explains)
      const TAffine &M = m_ctrlPressMatrix;
      const TPointD dW(M.a11 * d.x + M.a12 * d.y, M.a21 * d.x + M.a22 * d.y);
      for (const CtrlPinW &e : m_ctrlPinWSnapshot) {
        TDoubleKeyframe kx(e.keyFrame, e.oldW.x + dW.x);
        kx.m_type     = TDoubleKeyframe::Constant;
        kx.m_prevType = TDoubleKeyframe::Linear;
        e.px->setKeyframe(kx);
        TDoubleKeyframe ky(e.keyFrame, e.oldW.y + dW.y);
        ky.m_type     = TDoubleKeyframe::Constant;
        ky.m_prevType = TDoubleKeyframe::Linear;
        e.py->setKeyframe(ky);
      }
      TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
      invalidate();
      return;
    }

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

  // The controller affine is composed into the TOOL matrix (updateMatrix
  // override), and the skeleton overlay is drawn in that space: without this
  // refresh the overlay keeps the pre-drag matrix and visually lags behind the
  // mesh for the whole drag, snapping into place only on release.
  updateMatrix();
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
    case CtrlMove: {
      // Context-aware hint: a stitched child slides on its attachment; the top
      // column with a planted cross-column pin is locked unless Ctrl is held
      // (then everything moves and the pins travel along).
      std::string txt = "Move";
      const int cur   = ::column();
      bool child      = false;
      for (const CrossLevelLink &lk : crossLevelLinks_animate())
        if (lk.childColumn == cur) {
          child = true;
          break;
        }
      if (child)
        txt = "Move (slides on the attachment)";
      else {
        bool pinnedW = false;
        for (const CrossCol &cc : crossColumns_animate(::frame())) {
          if (!cc.def) continue;
          SkD::vd_iterator vdt, vdEnd;
          cc.def->vertexDeformations(vdt, vdEnd);
          for (; vdt != vdEnd && !pinnedW; ++vdt) {
            SkVD *pvd = (*vdt).second;
            pinnedW =
                pvd->m_params[SkVD::PIN] && pvd->m_params[SkVD::PINWX] &&
                pvd->m_params[SkVD::PINWY] &&
                pvd->m_params[SkVD::PIN]->getValue(cc.paramFrame) >= 0.5 &&
                (pvd->m_params[SkVD::PINWX]->getKeyframeCount() > 0 ||
                 pvd->m_params[SkVD::PINWY]->getKeyframeCount() > 0);
          }
          if (pinnedW) break;
        }
        // Qt maps the platform's primary modifier to "Ctrl": that's Cmd on
        // macOS — the hint must name the key the user actually presses.
#ifdef MACOSX
        if (pinnedW) txt = "Pinned - Cmd-drag to move all, pins follow";
#else
        if (pinnedW) txt = "Pinned - Ctrl-drag to move all, pins follow";
#endif
      }
      ctrlDrawText(moveP + u * TPointD(0, -12), tu, txt);
      break;
    }
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

        // Damp by LEVER ARM. Rotating about a pivot at distance d to move the
        // end by D costs an angle of roughly D/d: the shorter the lever, the
        // more degrees the same millimetre of mouse travel demands — and those
        // degrees are paid by everything hanging below. That is why the joints
        // near the root feel nervous while a long limb feels calm, and why the
        // measured response swung between 0 and 24x for the same input.
        //
        // The cap is therefore in DEGREES, flat for every joint — see the long
        // note on ikMaxStep(). A distance cap was tried first and is exactly
        // backwards: it grows as the lever shortens, loosening the limit
        // precisely where it had to be tightest. This comment used to describe
        // that old distance cap and survived the fix, contradicting the line
        // right below it.
        //
        // Annealing (a cap that shrinks toward the root, DragonBones' advice)
        // was MEASURED on 2026-08-13 and NOT adopted: on a 5-vertex chain it
        // shifts work from root to tip as advertised (root half 36% -> 26%) but
        // its effect on convergence is not monotonic — better at mid travel,
        // WORSE at large travel, where the root joints are the reach. The
        // existing flat cap already lands within 0.7% of chain length. Don't
        // re-litigate without a case where the current behaviour visibly fails.
        const double maxAng = ikMaxStep();
        ang                 = std::min(std::max(ang, -maxAng), maxAng);

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
        // Same lever-arm damping as the multi-anchor path: this is the branch
        // the single-level rig actually takes, and without it the slider had no
        // effect there at all.
        const double maxAng = ikMaxStep();
        ang                 = std::min(std::max(ang, -maxAng), maxAng);
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
  const bool ikDiag = ::getenv("ZTORYC_PIN_DIAG") != nullptr;
  if (pins.empty() || vIsPin) {
    if (ikDiag)
      qDebug().noquote() << QString("[IK_BRANCH] plainFK v=%1 pins=%2")
                                .arg(v).arg((int)pins.size());
    // No pin (or dragging the pin itself): local single joint, original tree.
    if (orig.vertex(v).parent() < 0) return;  // the root has no ANGLE param
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), v);
    if (!vd) return;
    ::setKeyframe(vd->m_params[SkVD::ANGLE], frame);
    if (m_keepDistance.getValue()) {
      m_sd->updateAngle(*skeleton(), deformedSkeleton(), frame, v, pos,
                        parentBoneRefDeg_animate());
    } else {
      ::setKeyframe(vd->m_params[SkVD::DISTANCE], frame);
      m_sd->updatePosition(*skeleton(), deformedSkeleton(), frame, v, pos,
                           parentBoneRefDeg_animate());
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
    if (ikDiag)
      qDebug().noquote() << QString("[IK_BRANCH] multiAnchor v=%1 pins=%2")
                                .arg(v).arg((int)pins.size());
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

  // Cap the rotation applied per mouse event. This is the one that flings the
  // hips: theta comes straight from the mouse, and the whole re-rooted body
  // turns by it, so a short lever between the joint and its pivot converts a
  // small drag into a large sweep. Nothing downstream limited it — the damping
  // added earlier lived in the CCD that re-plants the OTHER pins, which this
  // path barely touches.
  {
    const double maxTheta = ikMaxStep();
    const double raw      = theta;
    theta = std::min(std::max(theta, -maxTheta), maxTheta);
    if (::getenv("ZTORYC_PIN_DIAG"))
      qDebug().noquote()
          << QString("[IK_BRANCH] rotateAboutPin v=%1 pins=%2 rawDeg=%3 "
                     "capDeg=%4 clamped=%5")
                 .arg(v)
                 .arg((int)pins.size())
                 .arg(raw * 180.0 / M_PI, 0, 'f', 1)
                 .arg(maxTheta * 180.0 / M_PI, 0, 'f', 1)
                 .arg(fabs(raw) > maxTheta ? "YES" : "no");
  }

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

  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote() << "[PIN_PATH] rotateAboutPin pins=" << (int)pins.size();

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

  // Bound how far the dragged joint may travel in ONE event, before anything
  // else touches the target.
  //
  // The slider used to reach only the CCD that re-nails the pins — the SECOND
  // half of solveMultiAnchor. The first half, the FABRIK pass with the
  // stiffness springs, is what repositions the body from the target, and it was
  // completely undamped: no slider value could calm a joint whose motion is
  // decided there. Measured, that is where the hips live (v=1, multiAnchor,
  // 1754 events) while the rotate path that WAS damped saw 107 and never once
  // hit its cap.
  //
  // Bounding the input instead of one internal stage damps every stage at once,
  // and the bound is an arc: the slider's degrees, swung at the joint's own
  // distance from its anchor, so a joint far from its pin may travel further
  // per event than one sitting right on top of it.
  TPointD t = pos;
  {
    const TPointD here = curPos.at(v);
    double leverToPin  = 0.0;
    for (const auto &ap : anchor)
      leverToPin = std::max(leverToPin, norm(here - ap.second));

    const double maxTravel = ikMaxStep() * std::max(leverToPin, 1e-6);
    const TPointD step     = t - here;
    const double stepLen   = norm(step);
    if (stepLen > maxTravel && stepLen > 1e-9)
      t = here + step * (maxTravel / stepLen);
  }

  for (int it = 0; it < 4; ++it)
    for (int p : pins) {
      double L = 0.0;
      for (int u = p; T.parentT.at(u) >= 0; u = T.parentT.at(u))
        L += T.len.at(u);
      TPointD d = t - anchor[p];
      double n  = norm(d);
      if (n > L) t = anchor[p] + d * (L / n);
    }

  // Solve at a candidate target and report how far the WORST pin would end up
  // from where it must stay — as judged by the EVALUATION'S OWN planter, not by
  // the solver that produced the candidate.
  //
  // That distinction is the whole point. Straight-line reach is optimistic (the
  // chains toward the pins share bones, so a target inside every pin's radius
  // can still be one no configuration satisfies at once), but the deeper
  // problem was subtler: asking FABRIK whether FABRIK's own answer holds always
  // says yes. The drag then stopped where FABRIK was happy, plant() re-solved
  // it and was not, and the leftover error got spread over every pin — the
  // primary included, which is the one that had been exact.
  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote() << "[PIN_PATH] moveVertexMultiAnchor pins=" << (int)pins.size();

  PlasticSkeleton probe = deformedSkeleton();
  auto poseAt           = [&](const TPointD &tt) {
    std::map<int, TPointD> P = curPos;
    solveMultiAnchor(orig, T, anchor, tt, P);

    std::map<int, TPointD> full = curPos;
    for (const auto &kv : T.parentT) full[kv.first] = P[kv.first];
    rigidFollowOffTree(orig, curPos, full, T.parentT);

    for (const auto &kv : full) probe.vertex(kv.first).P() = kv.second;
    const double r = m_sd->pinResidualForPose(::skeletonId(), frame, probe);
    return std::make_pair(r * r, P);
  };

  // The body RESISTS. Where the pins cannot follow, the drag stiffens at
  // end-of-range instead of dragging them along: bisect between where the
  // vertex is now (feasible by construction) and where the mouse asks it to go,
  // and keep the farthest point that still holds every pin.
  //
  // Without this the pins were left unreachable, and the evaluation-time
  // balancing loop then shared that error across ALL of them — including the
  // primary, whose exact planting is the one thing that never used to move.
  // That is the slight drift: not a solver defect, the drag simply asked for a
  // pose that does not exist.
  //
  // Same shape as the bisection the rotate-about-a-pin path already uses; the
  // tolerance is a pixel, so "held" means held as far as the eye can tell.
  const double tol  = getPixelSize();
  const double tol2 = tol * tol;

  // Nothing feasible = DO NOT MOVE. Falling back to poseAt(from) looks
  // equivalent and is not: it re-runs the solver, which re-nails the pins and
  // springs the bones toward their rest orientation. Applying that whenever the
  // mouse asked for too much fed a fresh bias into the pose on every gesture —
  // the baseline is recaptured per drag, so it compounded across drags and the
  // character slowly walked away with no way to stand it back up.
  std::map<int, TPointD> P = curPos;
  double accepted          = 1.0;  // frazione del passo richiesto che ha retto
  auto attempt             = poseAt(t);
  if (attempt.first <= tol2)
    P = std::move(attempt.second);
  else {
    const TPointD from = curPos.at(v);
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 8; ++i) {
      const double mid = 0.5 * (lo + hi);
      auto r           = poseAt(from + (t - from) * mid);
      if (r.first <= tol2) {
        lo = mid;
        P  = std::move(r.second);
      } else
        hi = mid;
    }
    accepted = lo;
  }

  // How much of the requested step survived the feasibility bisection. The
  // suspicion this measures (2026-07-27): the nervous snap when posing hips is
  // this fraction jumping between events -- a rig with NO angle limits has
  // nothing to negotiate and drags smoothly, one with limits on most joints
  // re-negotiates on every mouse move.
  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote()
        << QString("[IK_FEASIBLE] v=%1 pins=%2 accepted=%3 askedLen=%4")
               .arg(v)
               .arg((int)pins.size())
               .arg(accepted, 0, 'f', 3)
               .arg(norm(t - curPos.at(v)), 0, 'f', 2);

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
  // Single-level: the free root is handled by the eval-time PINTX/PINTY plant,
  // NOT by a root offset — so writeRootOffset stays false (default).
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
    const std::map<int, TPointD> &desired, bool clampToLimits,
    bool writeRootOffset) {
  if (!def) return;
  static const bool noAngleClamp = ::getenv("ZTORYC_NO_ANGLE_CLAMP") != nullptr;
  // Probe for the cross-column angle-limit bug (2026-07-27), placed HERE and
  // not at the wedge: the symptom Franco judges is the usable RANGE, and the
  // range is decided in this function. Answers the two questions the TODO in
  // limitDisplay_animate left open — does the column resolve, and is the clamp
  // branch even reached for this joint.
  static const bool limDiag = ::getenv("ZTORYC_LIMIT_DIAG") != nullptr;
  const int thisCol = columnOfDeformation_animate(def);
  const int diagCol = limDiag ? thisCol : -1;
  // Shift for joints hanging off THIS column's root, resolved for this column
  // and not the current one: the cross-level write-back walks several columns
  // in a single drag. Must match what the wedge and the long-line overlay draw,
  // or the joint stops somewhere the bound is not.
  const double refDeg = parentBoneRefDegFor_animate(thisCol);

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
    if (op < 0) {
      // The root has no ANGLE. Only the unified IK solver asks to record its
      // motion (writeRootOffset), and only for the ONE effective root column —
      // whose ROOTX/ROOTY carries the free-root translation that keeps the
      // pinned foot planted while the body moves. Every other caller skips the
      // root (single-level relies on the eval PINTX plant; child columns follow
      // their parent's attachment vertex, so their root must NOT be offset).
      if (!writeRootOffset) continue;
      // ROOTX/ROOTY = offset from rest; updateBranchPositions applies it before
      // FK descends into children.
      SkVD *rvd = def->vertexDeformation(skelId, w);
      if (rvd && rvd->m_params[SkVD::ROOTX] && rvd->m_params[SkVD::ROOTY]) {
        TPointD newOff = desired.at(w) - orig.vertex(w).P();
        double oldX    = rvd->m_params[SkVD::ROOTX]->getValue(frame);
        double oldY    = rvd->m_params[SkVD::ROOTY]->getValue(frame);
        if (fabs(newOff.x - oldX) > 1e-4 || fabs(newOff.y - oldY) > 1e-4) {
          ::setKeyframe(rvd->m_params[SkVD::ROOTX], frame);
          ::setKeyframe(rvd->m_params[SkVD::ROOTY], frame);
          rvd->m_params[SkVD::ROOTX]->setValue(frame, newOff.x);
          rvd->m_params[SkVD::ROOTY]->setValue(frame, newOff.y);
        }
      }
      continue;
    }
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
    //
    // ZTORYC_NO_ANGLE_CLAMP disables the clamp entirely, for A/B-ing the
    // nervous snap when posing hips (2026-07-27). Deleting the keyed
    // MIN/MAXANGLE is NOT the same experiment: with no keys the lines below
    // fall back to the vertex's STATIC limit, which on the rigs we measured
    // holds nearly identical values, so the limits would still be enforced.
    const double preClamp = newDelta;
    double loLimDiag = 0.0, hiLimDiag = 0.0;
    bool clampReached = false;
    if (clampToLimits && !noAngleClamp) {
      clampReached = true;
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
      // Continuity-first, like updateAngle: the stored delta can legitimately
      // sit outside [lo, hi] (unpin bake writes unclamped; wrapped writers can
      // store 185 as -175). Never yank past the current value — out of range
      // you can only come back IN; inside, the full limits apply.
      if (orig.vertex(op).parent() < 0) {
        loLim += refDeg;
        hiLim += refDeg;
      }
      loLimDiag = loLim;
      hiLimDiag = hiLim;
      newDelta =
          tcrop(newDelta, std::min(loLim, oldDelta), std::max(hiLim, oldDelta));
    }

    if (limDiag) {
      // parentDir falls back to 0 (world X) when `op` has no parent of its own,
      // which is precisely the cross-column case: the joint's reference is then
      // a CONSTANT instead of the parent bone. Logging it raw shows whether it
      // actually moves when the torso bends.
      const double pdRef = parentDir(desFn, op) * M_180_PI;
      qDebug().noquote()
          << QString("[LIMIT] col=%1 v=%2 par=%3 grand=%4 parentDirDeg=%5 "
                     "rest=%6 pre=%7 post=%8 old=%9 lo=%10 hi=%11 clamp=%12")
                 .arg(diagCol).arg(w).arg(op)
                 .arg(orig.vertex(op).parent())
                 .arg(pdRef, 0, 'f', 2)
                 .arg(rest * M_180_PI, 0, 'f', 2)
                 .arg(preClamp, 0, 'f', 2)
                 .arg(newDelta, 0, 'f', 2)
                 .arg(oldDelta, 0, 'f', 2)
                 .arg(loLimDiag, 0, 'f', 2)
                 .arg(hiLimDiag, 0, 'f', 2)
                 .arg(clampReached ? (fabs(preClamp - newDelta) > 1e-6 ? "BIT" : "reached")
                                   : "SKIPPED");
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
  enablePinsOnCharacter(on);
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

  // 2. Fully un-pin: drop every PIN / PINTX / PINTY / PINWX / PINWY key
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end();
       ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (!vd) continue;
    for (int p : {(int)SkVD::PIN, (int)SkVD::PINTX, (int)SkVD::PINTY,
                  (int)SkVD::PINWX, (int)SkVD::PINWY})
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
  // RESOLVED 2026-07-27 — the old TODO here proposed feeding
  // parentColumnRefDirs_animate() in when vGrand < 0, to stop the reference
  // being the world X axis. Measured, that is the WRONG cure, and the note is
  // kept because the wrong turn is easy to take twice:
  //
  //  - The two questions it asked are answered. The clamp that governs a plain
  //    joint drag is NOT here and NOT in writeBackAnglesFor_animate (probed:
  //    zero calls across 52 drag events) — it is PlasticSkeletonDeformation::
  //    updateAngle. And the effective range measured IDENTICAL with the torso
  //    upright and bent (arm stopped at -95.38 both times), so the range was
  //    never the defect: the drawn wedge was, and so was the arm.
  //  - The real defect was one level down, in the PLACEMENT: parenting to a
  //    hook carried the vertex's position but not its bone's orientation, so
  //    the arm column simply did not turn with the torso. Fixed in
  //    TStageObject::computeLocalPlacement.
  //  - With that in place the local frame rotates with the body, so bone,
  //    bounds and wedge all follow for free. Shifting the limits HERE as well
  //    was tried and reverted the same day: it counts the rotation twice and
  //    the bounds drift out from under the gizmo.
  //
  // So world X stays as the fallback on purpose. If it ever looks wrong again,
  // the thing to check is whether the column is rotating, not this reference.
  TPointD dirFromParent = vx.P() - vxParent.P();
  defAng = atan2(cross(dirFromGrand, dirFromParent),
                 dirFromGrand * dirFromParent) *
           M_180_PI;
  branch = atan2(dirFromDefGrand.y, dirFromDefGrand.x);
  // Same shift the clamp applies, so the wedge can never show a limit where the
  // joint does not actually stop. Only meaningful when there is no grandparent
  // here — that is the case whose reference is otherwise a constant.
  if (vGrand < 0) branch += parentBoneRefDeg_animate() * (M_PI / 180.0);
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

  // Always key. Two reasons it cannot stay "static until someone else animates
  // it": there was no way to create that first key from here, so the animated
  // branch was unreachable and the function editor never showed the bound; and
  // the static value lives on the VERTEX of this skeleton, while the param is
  // shared by vertex name across all of them — so on a multilevel rig a bound
  // set on one view was simply absent on the others.
  if (vd && vd->m_params[p]) {
    ::setKeyframe(vd->m_params[p], ::frame());
    vd->m_params[p]->setValue(::frame(), L);
  }

  // Keep the static limit in step for this skeleton. Evaluation prefers the
  // keyed value once there is one, so this changes nothing on its own — it just
  // stops the two from disagreeing for any reader that still looks at the
  // vertex.
  if (m_limitDrag == 1) {
    m_sd->skeleton(skelId)->vertex(v).m_minAngle = L;
    deformedSkeleton().vertex(v).m_minAngle      = L;
  } else {
    m_sd->skeleton(skelId)->vertex(v).m_maxAngle = L;
    deformedSkeleton().vertex(v).m_maxAngle      = L;
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
  // Pins are keyed in the column's param domain (see togglePinAtCurrentFrame),
  // so read them there. paramsTime is idempotent: callers may pass either the
  // raw xsheet frame or an already-converted one.
  frame = ::sdFrame(frame);
  // Pins off (IK mode off): no diamonds drawn, no pin-aware manipulation. The
  // KEYS stay untouched, so turning IK back on restores them exactly; the
  // evaluation stops planting them too, and the pose stays where it is because
  // leaving IK bakes the planted result into the params first.
  if (!m_sd->pinsEnabled()) return pins;
  PlasticSkeletonP skel = skeleton();
  if (!skel) return pins;
  int skelId = ::skeletonId();

  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end(); ++vt) {
    SkVD *vd = m_sd->vertexDeformation(skelId, vt.m_idx);
    if (!vd || !vd->m_params[SkVD::PIN]) continue;
    if (vd->m_params[SkVD::PIN]->getValue(frame) < 0.5) continue;
    pins.push_back(vt.m_idx);
  }
  return pins;
}

//------------------------------------------------------------------------

void PlasticTool::recapturePinTargets_animate() {
  if (!m_sd) return;

  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  TStageObject *obj =
      xsh ? xsh->getStageObject(TStageObjectId::ColumnId(::column())) : nullptr;
  if (!obj) return;

  const double frame    = ::frame();
  const int rawFrame    = (int)frame;
  const int skelId      = ::skeletonId();
  PlasticSkeletonP skel = m_sd->skeleton(skelId);
  if (!skel) return;

  m_deformedSkeleton.invalidate();

  bool any = false;
  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end(); ++vt) {
    const int v = (int)vt.m_idx;
    SkVD *vd    = m_sd->vertexDeformation(skelId, v);
    if (!vd || !vd->m_params[SkVD::PIN]) continue;
    if (vd->m_params[SkVD::PIN]->getValue(frame) < 0.5) continue;
    if (!vd->m_params[SkVD::PINWX] || !vd->m_params[SkVD::PINWY]) continue;

    // Same capture as pinning does — deformed vertex through the controller,
    // then through the column placement into scene space.
    const TPointD vp =
        m_sd->getSquashControllerAffine(skelId, frame) *
        deformedSkeleton().vertex(v).P();
    const TPointD W = obj->getPlacement(rawFrame) * vp;
    for (int cc : {(int)SkVD::PINWX, (int)SkVD::PINWY}) {
      TDoubleKeyframe tk(frame, cc == SkVD::PINWX ? W.x : W.y);
      tk.m_type = tk.m_prevType = TDoubleKeyframe::Constant;
      vd->m_params[cc]->setKeyframe(tk);
    }
    any = true;
  }

  if (any) {
    m_deformedSkeleton.invalidate();
    invalidate();
  }
}

//------------------------------------------------------------------------

void PlasticTool::togglePinAtCurrentFrame() {
  if (!m_sd || !m_svSel.hasSingleObject()) return;
  // Pins are read by the evaluation at paramsTime (identity, except past the
  // last stage key with Cycle on), so their keys must be WRITTEN there too — a
  // pin keyed at the raw frame past the cycle point would never be read back.
  // The raw frame survives only for placement evaluation, which takes xsheet
  // time (paramsTime is applied internally, per object).
  const double rawFrame = ::frame();
  double frame          = ::sdFrame();
  int v        = m_svSel;
  SkVD *vd     = m_sd->vertexDeformation(::skeletonId(), v);
  if (!vd) return;

  // Does the current column belong to a STITCHED CHARACTER (several columns
  // hooked together — a body with legs/arms parented via handles)? Then its pins
  // are a character-level concern: they get a SCENE-space target (PINWX/PINWY)
  // and are planted once, at the stage level, by
  // TStageObject::computePlasticPinCorrection.
  //
  // The test is deliberately on the CHARACTER, not on the column's role. Gating
  // on "is a child column" gave the two pin kinds two different owners — a pin
  // on the root column planted itself in-skeleton (PINTX) while a pin on a child
  // planted at stage level — and in a walk cycle both were active at once, each
  // translating the character independently: on every support switch the plant
  // transferred, moved the child's attachment, and the stage correction
  // counter-translated the whole character, dragging the previously planted foot
  // along. One character, one planting authority.
  //
  // We still DON'T capture PINTX/PINTY and DON'T mirror onto the parent: the
  // per-column eval plant lives in a space glued to the parent, so it cannot
  // hold a WORLD spot, and the mirror over-constrained the chain (heel and hip
  // both nailed). The pose is carried by the ANGLE/ROOTX-ROOTY write-back.
  bool isStitchedCharacter = false;
  {
    const int cur = ::column();
    for (const CrossLevelLink &lk : crossLevelLinks_animate())
      if (lk.childColumn == cur || lk.parentColumn == cur) {
        isStitchedCharacter = true;
        break;
      }
  }
  if (isStitchedCharacter) {
    TUndoManager::manager()->beginBlock();
    // Explicit row = the PARAM frame + 1: the undo deletes at m_row - 1, and
    // the keys just written live in the param domain, not at the raw frame.
    AnimateValuesUndo *undo =
        new AnimateValuesUndo(v, (int)frame + 1, ::column());
    m_sd->getKeyframeAt(frame, undo->m_oldValues);

    bool pinned = vd->m_params[SkVD::PIN]->getValue(frame) >= 0.5;

    // Capture the pin's SCENE-space target BEFORE flagging the pin: the
    // placement chain is still free of this pin's stage-level correction, so
    // this is the vertex's current visual spot. The per-frame hold
    // (TStageObject::computePlasticPinCorrection on the chain's top column)
    // re-plants the vertex here on every frame, in-betweens included — the
    // cross-column analogue of the single-level PINTX plant.
    bool captureW = !pinned && vd->m_params[SkVD::PINWX] &&
                    vd->m_params[SkVD::PINWY];
    if (captureW) {
      TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
      TStageObject *childObj =
          xsh ? xsh->getStageObject(TStageObjectId::ColumnId(::column()))
              : nullptr;
      if (childObj) {
        m_deformedSkeleton.invalidate();
        const TPointD vp =
            m_sd->getSquashControllerAffine(::skeletonId(), frame) *
            deformedSkeleton().vertex(v).P();
        const TPointD W = childObj->getPlacement(rawFrame) * vp;
        for (int c : {(int)SkVD::PINWX, (int)SkVD::PINWY}) {
          TDoubleKeyframe tk(frame, c == SkVD::PINWX ? W.x : W.y);
          tk.m_type = tk.m_prevType = TDoubleKeyframe::Constant;
          vd->m_params[c]->setKeyframe(tk);
        }
      }
    }

    if (!pinned && frame > 1.0 &&
        vd->m_params[SkVD::PIN]->getKeyframeCount() == 0) {
      TDoubleKeyframe base(1.0, 0.0);
      base.m_type = base.m_prevType = TDoubleKeyframe::Constant;
      vd->m_params[SkVD::PIN]->setKeyframe(base);
    }
    TDoubleKeyframe kf(frame, pinned ? 0.0 : 1.0);
    kf.m_type = kf.m_prevType = TDoubleKeyframe::Constant;
    vd->m_params[SkVD::PIN]->setKeyframe(kf);

    // Un-pinning: the stage-level correction vanishes with the PIN flag and the
    // pose would snap by the residual in-between drift. At the frame being
    // unpinned the drift is what it is — bake it into the character's pose by
    // NOT correcting here; keyframes stay exact (the tool keeps them planted),
    // so the snap is bounded by the current in-between error. Refinement (bake
    // the residual into ROOTX/ROOTY of the top column) deferred.

    m_sd->getKeyframeAt(frame, undo->m_newValues);
    TUndoManager::manager()->add(undo);
    TUndoManager::manager()->endBlock();

    m_deformedSkeleton.invalidate();
    invalidate();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    return;
  }

  // One block: this pin plus the mirrored parent-attachment pin undo together.
  TUndoManager::manager()->beginBlock();

  // Snapshot for undo (captures the PIN param, part of the SkVD keyframe).
  // Explicit row: param frame + 1, same reasoning as the stitched branch above.
  AnimateValuesUndo *undo =
      new AnimateValuesUndo(v, (int)frame + 1, ::column());
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
  // RAW frame: setAttachmentPin converts through the PARENT's own paramsTime
  // (each column wraps its cycle independently).
  setAttachmentPin_animate(!pinned, rawFrame);

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
  if (m_correctiveSculpt.getValue()) {
    endCorrectiveStroke_animate();
    return;
  }

  // Track mouse position
  m_pos = pos;

  // Drag over: hand the pose back to the evaluation-time solver. Cleared here,
  // ahead of every early return below, so no exit path can leave it stuck on.
  PlasticPinSolver::setSolveSuspended(false);

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
    // Global key: complete the pose on EVERY connected column, not just the
    // ANGLEs the solver wrote — a drag that keys only the touched chain leaves
    // the pose partial (the xsheet diamond showed white-over-gold instead of
    // solid gold, reading as "transform keyed too"). Keyed BEFORE the undo
    // snapshot in finishCrossLevelUndo_animate, so undo removes these keys as
    // well. Per-column param time, same rule as the undo loop.
    if (m_globalKey.getValue()) {
      TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
      for (auto &kv : m_ikCrossDefs) {
        const int col   = kv.first;
        const SkDP &def = kv.second;
        if (!def) continue;
        double pf = ::frame();
        if (col != ::column() && xsh)
          if (TStageObject *obj =
                  xsh->getStageObject(TStageObjectId::ColumnId(col)))
            pf = obj->paramsTime(pf);
        ::setKeyframe(def, pf, def->skeletonId(pf));
      }
    }
    finishCrossLevelUndo_animate(::frame());
    m_dragged = false;
    updateMatrix();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    TTool::getApplication()->getCurrentObject()->notifyObjectIdChanged(false);
    invalidate();
    return;
  }

  if (m_svSel.hasSingleObject() && m_dragged) {
    // Global Key scope: 0 Stage, 1 Plastic, 2 All. Posing always writes the
    // moved plastic params; the global key adds the OTHER channels per scope.
    // Ztoryc: with scope All (or Stage) a pose must also drop a TRANSFORM key
    // on the column — otherwise the column stays un-keyed and drifts relative
    // to the plastic keys ("in global all, posing must also key the transform").
    const int scope       = m_globalKeyScope.getIndex();
    const bool globalKey  = m_globalKey.getValue();
    const bool doTransform = globalKey && scope != 1;  // Stage or All
    const bool doFullPlastic = globalKey && scope != 0;  // Plastic or All

    TUndoManager::manager()->beginBlock();

    // Transform key first, with its own undo (AnimateValuesUndo below covers
    // only the plastic side).
    if (doTransform) {
      TStageObject *o = stageObject();
      if (o) {
        const int f = (int)::frame();
        const bool had = o->isKeyframe(f);
        TStageObject::Keyframe oldKey;
        if (had) oldKey = o->getKeyframe(f);
        o->setKeyframeWithoutUndo(f);
        TUndoManager::manager()->add(new StageTransformKeyUndo(
            TTool::getApplication()->getCurrentXsheet(), o->getId(), f, had,
            oldKey));
      }
    }

    // Plastic side: full pose key when the scope includes Plastic; otherwise
    // just surface the params the drag already keyed.
    if (doFullPlastic)
      ::setKeyframe(m_sd, ::frame());  // Already invokes keyframes rebuild
    else
      ::updateStageObjectKeyframes();  // Otherwise, must be explicit

    // Add a corresponding undo
    AnimateValuesUndo *undo = new AnimateValuesUndo(m_svSel);

    undo->m_oldValues = m_pressedSkDF;
    m_sd->getKeyframeAt(frame(), undo->m_newValues);

    TUndoManager::manager()->add(undo);

    TUndoManager::manager()->endBlock();

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

// One action = one support switch (walk cycle): at the CURRENT frame, pin the
// selected vertex and release every other active pin of the character. Same
// frame, deliberately — swapping the support on one key is what the animation
// wants, and it is what keeps this simple: no frame handle to move, every undo
// on the same row, one key per pin channel.
//
// It used to pin here and release ONE FRAME LATER, to leave a double-support
// key behind. That gave a messier curve, and because the frame handle moved
// inside the undo block the undo did not come back cleanly. Identical for
// single-level and stitched rigs — the behaviour must not fork on rig shape.
void PlasticTool::switchPinAtCurrentFrame() {
  if (!m_sd || !m_svSel.hasSingleObject()) return;
  // Param domain for everything touching SkVD params (see
  // togglePinAtCurrentFrame); the raw frame survives for the other columns,
  // which each wrap their cycle independently.
  const double rawFrame = ::frame();
  const double frame    = ::sdFrame();
  const int v           = m_svSel;

  std::vector<int> pins = pinnedVerticesAtFrame(frame);
  const bool vPinned = std::find(pins.begin(), pins.end(), v) != pins.end();

  TUndoManager::manager()->beginBlock();

  // New support first: its scene target is captured while the PREVIOUS pin is
  // still planting the character, i.e. at the foot's true visual spot.
  if (!vPinned) togglePinAtCurrentFrame();

  // Release the others on this column, through the proven path.
  for (int p : pins) {
    if (p == v) continue;
    SkVD *vd = m_sd->vertexDeformation(::skeletonId(), p);
    if (!vd || !vd->m_params[SkVD::PIN]) continue;
    if (vd->m_params[SkVD::PIN]->getValue(frame) < 0.5) continue;
    setSkeletonSelection(p);
    togglePinAtCurrentFrame();
  }
  setSkeletonSelection(v);

  // The previous support foot usually lives on ANOTHER column of the character
  // (the body, or the other leg), and pinnedVerticesAtFrame only ever sees the
  // CURRENT one — so its release key was never written and the old foot stayed
  // planted, which is why a walk cycle would not switch support.
  //
  // Write straight into the other column's deformation, with explicit (row,
  // column) coordinates for the undo. Deliberately NOT by activating those
  // columns: a first attempt drove them through TemporaryActivation +
  // togglePinAtCurrentFrame and made things worse — no keys at all, the previous
  // foot un-pinned instead of released, and the selection stranded on an
  // unrelated column. Moving the current column or selection under an undo block
  // is the thing to avoid.
  {
    TXsheet *xsh   = TTool::getApplication()->getCurrentXsheet()->getXsheet();
    const int col0 = ::column();
    for (int c : characterColumns()) {
      if (c == col0 || !xsh) continue;
      TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
      if (!obj) continue;
      const PlasticSkeletonDeformationP &sd =
          obj->getPlasticSkeletonDeformation();
      if (!sd) continue;

      // This column's OWN param time, from the raw xsheet frame: each column
      // wraps its cycle independently. Now that togglePinAtCurrentFrame also
      // keys pins in the param domain, writes and reads finally agree on every
      // column. (An earlier "fix" used the raw frame here to match togglePin's
      // then-raw writes — same domain, aligned at the wrong end.)
      const double sdFr     = obj->paramsTime(rawFrame);
      const int skelId      = sd->skeletonId(sdFr);
      PlasticSkeletonP skel = sd->skeleton(skelId);
      if (!skel) continue;

      for (auto vt = skel->vertices().begin(); vt != skel->vertices().end();
           ++vt) {
        SkVD *vd = sd->vertexDeformation(skelId, vt.m_idx);
        if (!vd || !vd->m_params[SkVD::PIN]) continue;
        if (vd->m_params[SkVD::PIN]->getValue(sdFr) < 0.5) continue;

        // ROW = the frame the key is WRITTEN at + 1: the undo deletes at
        // m_row - 1, so it must point at sdFr, this column's param time.
        AnimateValuesUndo *undo =
            new AnimateValuesUndo((int)vt.m_idx, (int)sdFr + 1, c);
        sd->getKeyframeAt(sdFr, undo->m_oldValues);

        TDoubleKeyframe kf(sdFr, 0.0);
        kf.m_type = kf.m_prevType = TDoubleKeyframe::Constant;
        vd->m_params[SkVD::PIN]->setKeyframe(kf);

        sd->getKeyframeAt(sdFr, undo->m_newValues);
        TUndoManager::manager()->add(undo);
      }
    }
  }

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

int PlasticTool::columnOfDeformation_animate(const SkDP &def) const {
  if (!def) return -1;
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh) return -1;
  for (int c = 0; c < xsh->getColumnCount(); ++c) {
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (obj && obj->getPlasticSkeletonDeformation() == def) return c;
  }
  return -1;
}

//------------------------------------------------------------------------

// See the declaration. ZTORYC_BOUND_REF picks the sign (or 0/unset = disabled)
// because which way the shift goes was never settled by measurement: the limb
// ALREADY turns with its parent through the IK evaluation, so the limits have
// to move with it — but a shift the wrong way puts the joint outside its own
// bound by twice the angle, which is exactly what the first attempt looked like.
double PlasticTool::parentBoneRefDegFor_animate(int column) const {
  // ON by default since 2026-07-27 (verified by Franco on a real multi-column
  // rig, with the kinematics on and off). ZTORYC_BOUND_REF=0 restores the old
  // behaviour, -1 flips the direction — kept because the sign convention was
  // settled by experiment, not derived.
  static const int sign = ::getenv("ZTORYC_BOUND_REF")
                              ? atoi(::getenv("ZTORYC_BOUND_REF"))
                              : 1;
  if (sign == 0 || column < 0) return 0.0;
  TPointD restDir, defDir;
  if (!parentColumnRefDirs_animate(column, restDir, defDir)) return 0.0;
  return sign * wrapPi(atan2(defDir.y, defDir.x) - atan2(restDir.y, restDir.x)) *
         M_180_PI;
}

//------------------------------------------------------------------------

double PlasticTool::parentBoneRefDeg_animate() const {
  return parentBoneRefDegFor_animate(::column());
}

//------------------------------------------------------------------------

bool PlasticTool::parentColumnRefDirs_animate(int column, TPointD &restDir,
                                              TPointD &defDir) const {
  TXsheet *xsh = TTool::getApplication()->getCurrentXsheet()->getXsheet();
  if (!xsh || column < 0) return false;

  int parentColumn = -1, parentVertex = -1;
  for (const CrossLevelLink &lk :
       const_cast<PlasticTool *>(this)->crossLevelLinks_animate())
    if (lk.childColumn == column) {
      parentColumn = lk.parentColumn;
      parentVertex = lk.parentVertex;
      break;
    }
  if (parentColumn < 0 || parentVertex < 0) return false;

  TStageObject *pobj =
      xsh->getStageObject(TStageObjectId::ColumnId(parentColumn));
  if (!pobj) return false;
  const SkDP &pdef = pobj->getPlasticSkeletonDeformation();
  if (!pdef) return false;

  const double pFrame = pobj->paramsTime((double)::frame());
  const int pSkelId   = pdef->skeletonId(pFrame);
  PlasticSkeletonP pRest = pdef->skeleton(pSkelId);
  if (!pRest || parentVertex >= (int)pRest->vertices().size()) return false;

  const int gp = pRest->vertex(parentVertex).parent();
  if (gp < 0) return false;  // the attachment is the parent's own root

  PlasticSkeleton pDeformed;
  pdef->storeDeformedSkeleton(pSkelId, pFrame, pDeformed);

  TPointD r = pRest->vertex(parentVertex).P() - pRest->vertex(gp).P();
  TPointD d = pDeformed.vertex(parentVertex).P() - pDeformed.vertex(gp).P();
  if (norm2(r) < 1e-8 || norm2(d) < 1e-8) return false;

  // Into this column's space. Only the LINEAR part: a direction has no origin,
  // and the two spaces differ by the column parenting, which is exactly the
  // rotation that has to be carried over.
  TAffine toCur;
  bool found = false;
  for (const ConnectedSkel &cs : connectedSkeletons_animate())
    if (cs.columnIndex == parentColumn) {
      toCur = cs.toCur;
      found = true;
      break;
    }
  if (found) {
    const TAffine lin(toCur.a11, toCur.a12, 0.0, toCur.a21, toCur.a22, 0.0);
    r = lin * r;
    d = lin * d;
  }

  restDir = r;
  defDir  = d;
  return true;
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
    // Param-time, like the connected columns below get: paramFrame is used to
    // READ and WRITE SkVD params, and those live in the column's own param
    // domain (identity, except past the last stage key with Cycle on).
    cc.paramFrame  = ::sdFrame(frame);
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

// Unified multi-anchor: dragging a vertex that lies BETWEEN two or more pins
// (the neck base with both feet and a hand planted). The re-root rotation
// below is the wrong tool there: whichever single pin it picks as base, the
// rigid rotation displaces ALL the others — including the one the eval-time
// plant translates the whole structure onto — so the plant cancels most of
// the intended motion and the vertex barely responds to the mouse. This is
// the same problem the single-level path already solved with
// moveVertexMultiAnchor_animate ("posing a vertex BETWEEN the pins"): FABRIK
// over the tree spanned by the chains dragged -> each pin, every pin nailed.
// Reuse that machinery unchanged by mirroring the unified graph onto a
// SYNTHETIC single skeleton (dense indices, unified parenting, rest = the
// graph's world-frame rest), then dispatch per column off the same baseline
// snapshots the rotation path uses.
bool PlasticTool::crossLevelMultiAnchor_animate(const UNode &dragged,
                                                const TPointD &mousePos) {
  UnifiedGraph &g = m_ikCrossBaseGraph;
  if ((int)g.pins.size() < 2) return false;

  auto pathFromRootU = [&](const UNode &n) {
    std::vector<UNode> path;
    UNode u = n;
    path.push_back(u);
    auto it = g.parent.find(u);
    while (it != g.parent.end() && it->second.col >= 0) {
      u = it->second;
      path.push_back(u);
      it = g.parent.find(u);
    }
    std::reverse(path.begin(), path.end());
    return path;
  };

  // Engage only when the dragged vertex sits INSIDE the minimal subtree
  // spanning the pins (the unified analogue of spanningOfPins): outside it,
  // the vertex hangs off a single chain and the rotation path serves better.
  std::set<UNode> span;
  {
    std::vector<std::vector<UNode>> pinPaths;
    for (const UNode &p : g.pins) pinPaths.push_back(pathFromRootU(p));
    for (size_t i = 0; i < pinPaths.size(); ++i)
      for (size_t j = i + 1; j < pinPaths.size(); ++j) {
        const std::vector<UNode> &pa = pinPaths[i], &pb = pinPaths[j];
        size_t k = 0;
        while (k < pa.size() && k < pb.size() && pa[k] == pb[k]) ++k;
        for (size_t q = (k ? k - 1 : 0); q < pa.size(); ++q) span.insert(pa[q]);
        for (size_t q = (k ? k - 1 : 0); q < pb.size(); ++q) span.insert(pb[q]);
      }
  }
  if (!span.count(dragged)) return false;

  // Synthetic skeleton mirroring the unified graph. BFS from the effective
  // root so every parent is added before its children; rest positions feed
  // solveMultiAnchor's stiffness pass (it springs toward rest directions).
  PlasticSkeleton synth;
  std::map<UNode, int> denseOf;
  std::vector<UNode> idxOf;
  {
    std::queue<UNode> q;
    q.push(g.root);
    while (!q.empty()) {
      UNode u = q.front();
      q.pop();
      int par  = -1;
      auto pit = g.parent.find(u);
      if (pit != g.parent.end() && pit->second.col >= 0)
        par = denseOf.at(pit->second);
      int d      = synth.addVertex(PlasticSkeletonVertex(g.rest.at(u)), par);
      denseOf[u] = d;
      if (d >= (int)idxOf.size()) idxOf.resize(d + 1);
      idxOf[d] = u;
      auto ct  = g.children.find(u);
      if (ct != g.children.end())
        for (const UNode &w : ct->second) q.push(w);
    }
    // A disconnected rig can't be mirrored onto one skeleton — let the
    // rotation path (which walks reachability explicitly) deal with it.
    if (denseOf.size() != g.nodes.size()) return false;
  }

  std::map<int, TPointD> curPos;
  for (const auto &kv : denseOf) curPos[kv.second] = g.world.at(kv.first);
  std::vector<int> pinsD;
  std::map<int, TPointD> anchor;
  for (const UNode &p : g.pins) {
    const int pd = denseOf.at(p);
    pinsD.push_back(pd);
    auto ta    = g.pinTarget.find(p);
    anchor[pd] = (ta != g.pinTarget.end()) ? ta->second : g.world.at(p);
  }
  const int vD = denseOf.at(dragged);

  PinTree T = buildPinTree(synth, curPos, vD, pinsD);

  // Clamp the mouse target inside every pin's reach (alternating projection),
  // exactly as the single-level path does.
  TPointD t = getMatrix() * mousePos;
  for (int it = 0; it < 4; ++it)
    for (int p : pinsD) {
      double L = 0.0;
      for (int u = p; T.parentT.at(u) >= 0; u = T.parentT.at(u))
        L += T.len.at(u);
      TPointD d = t - anchor[p];
      double n  = norm(d);
      if (n > L && n > 1e-9) t = anchor[p] + d * (L / n);
    }

  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote() << "[PIN_PATH] crossLevelMultiAnchor pins="
                       << (int)pinsD.size();

  // Solve at a candidate target and report how far the WORST pin lands from
  // where it must stay. The straight-line clamp above is optimistic: here the
  // chains toward the pins share whole COLUMNS, so a target inside every pin's
  // radius can still be one no configuration satisfies at once — and an
  // unreached pin is not a pin that merely misses, it is an error the
  // evaluation then spreads over every pin, the primary included.
  auto poseAt = [&](const TPointD &tt) {
    std::map<int, TPointD> Pc = curPos;
    solveMultiAnchor(synth, T, anchor, tt, Pc);
    double worst2 = 0.0;
    for (const auto &ap : anchor) {
      auto it = Pc.find(ap.first);
      if (it != Pc.end())
        worst2 = std::max(worst2, norm2(ap.second - it->second));
    }
    return std::make_pair(worst2, Pc);
  };

  // The body RESISTS: bisect between where the dragged joint stands (feasible
  // by construction) and where the mouse asks it to go, keeping the farthest
  // point that still holds every pin.
  //
  // If NOTHING is feasible the answer is curPos — do not move at all. Handing
  // back a re-solved "safe" pose instead is what walked the character away:
  // that pose has the stiffness pass in it, springing bones toward rest, and
  // since the baseline is recaptured per gesture the bias compounded drag after
  // drag until the character could not be stood back up.
  //
  // NOTE this judges with solveMultiAnchor while the CHARACTER-level solve in
  // TStageObject has the last word, so it is an approximation — the exact
  // version needs that solve exposed as a query, the way plantPins now is for a
  // single column. It still rejects the plainly impossible targets.
  // Tolerance in PIXELS. Not one pixel: solveMultiAnchor re-nails the pins with
  // a fixed number of sweeps, so it leaves a small residue even on a pose that
  // is perfectly fine. Judging at one pixel makes this test stricter than the
  // solver it is testing, and the drag then stiffens on poses nobody would call
  // impossible — the root barely moved at all.
  const double tolC  = 4.0 * getPixelSize();
  const double tolC2 = tolC * tolC;

  // The test is RELATIVE: "does this move make the pins worse than standing
  // still?" — not "are the pins perfect?". They often are not perfect to begin
  // with (the solver leaves a residue, and a pose can be inherited slightly
  // off), and an absolute bar then rejects every candidate including a
  // one-pixel nudge: the bisection returned nothing, the character froze, and
  // the drag alternated between moving freely and not moving at all. That is
  // the jerkiness — measured: sequences of kept=0% with the standing residual
  // already above the bar.
  // ON by default; ZTORYC_PIN_NORESIST turns it off for comparison.
  //
  // Tried the other way round (2026-07-26) on the theory that the single-level
  // path is fluid precisely BECAUSE it does not resist — pose freely, write the
  // angles, let the evaluation hold the pins. Verified with Franco on the rig:
  // without the resistance it is clearly worse. Holding the pins is not
  // something the evaluation can be left to sort out alone on a stitched
  // character.
  static const bool resist = ::getenv("ZTORYC_PIN_NORESIST") == nullptr;

  const TPointD from   = curPos.at(denseOf.at(dragged));
  const double stand2  = resist ? poseAt(from).first : 0.0;
  const double accept2 = std::max(tolC2, stand2 * 1.21);  // +10% in distance

  std::map<int, TPointD> P = curPos;
  auto firstTry            = poseAt(t);
  double acceptedAt        = 1.0;
  if (!resist || firstTry.first <= accept2)
    P = std::move(firstTry.second);
  else {
    // Scan from far to near and keep the FIRST fraction that holds — do not
    // bisect. Bisection assumes feasibility is monotone along the segment, and
    // this solver does not offer that: "plain CCD finds wildly different
    // configurations for nearby targets" (see plant()'s header). A single
    // unlucky midpoint sent the search into a false infeasible half and it came
    // back with a few percent of the drag even when most of it was fine —
    // measured as partials bunched at 4-8%, which is the stutter.
    static const double kSteps[] = {0.85, 0.70, 0.55, 0.40, 0.28, 0.18, 0.10,
                                    0.05};
    acceptedAt                   = 0.0;
    for (double f : kSteps) {
      auto r = poseAt(from + (t - from) * f);
      if (r.first <= accept2) {
        acceptedAt = f;
        P          = std::move(r.second);
        break;
      }
    }
  }

  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote()
        << QString("[PIN_REACH] asked=%1px standing=%2px bar=%3px kept=%4%%")
               .arg(sqrt(firstTry.first) / getPixelSize(), 0, 'f', 2)
               .arg(sqrt(stand2) / getPixelSize(), 0, 'f', 2)
               .arg(sqrt(accept2) / getPixelSize(), 0, 'f', 2)
               .arg(100.0 * acceptedAt, 0, 'f', 0);

  std::map<int, TPointD> desiredD = curPos;
  for (const auto &kv : T.parentT) desiredD[kv.first] = P[kv.first];
  rigidFollowOffTree(synth, curPos, desiredD, T.parentT);

  std::map<UNode, TPointD> desired;
  for (const auto &kv : denseOf) desired[kv.first] = desiredD.at(kv.second);

  // Per-column dispatch off the BASELINE snapshots — same reasons as the
  // rotation path (write-back is absolute; a mid-drag re-fetch would map
  // through drifted child-column affines).
  for (const CrossCol &cc : m_ikCrossBaseCols) {
    const TAffine wInv = cc.world.inv();
    std::map<int, TPointD> curLocal, desLocal;
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      curLocal[(int)vt.m_idx] = vt->P();
      auto dit = desired.find(UNode{cc.columnIndex, (int)vt.m_idx});
      desLocal[(int)vt.m_idx] = (dit != desired.end()) ? wInv * dit->second
                                                       : vt->P();
    }
    writeBackAnglesFor_animate(cc.rest, cc.def, cc.skelId, cc.paramFrame,
                               curLocal, desLocal, true,
                               /*writeRootOffset=*/cc.columnIndex == g.root.col);
  }

  invalidateConnectedPlacements_animate();
  return true;
}

//------------------------------------------------------------------------

// STEP A — unified IK pin drag. When a pin sits on a CHILD column (a foot on a
// leg parented to the body), the whole articulated character is treated as ONE
// skeleton: re-root the unified graph at that pin, rotate the dragged vertex's
// re-rooted subtree about its new parent (single joint, constant bone lengths),
// and dispatch the result back per column as ANGLE deltas — plus ROOTX/ROOTY on
// the ONE effective root column, which carries the free-root translation. The
// pinned foot stays put (it is the re-root base) while the body articulates.
// Returns false (caller falls back to FK / single-level) when there is no
// cross-column pin, it's a single column, or the dragged vertex is the pin.
bool PlasticTool::crossLevelIK_animate(double frame, int vDragged,
                                       const TPointD &mousePos) {
  // Solve against the PRESS-TIME baseline, captured on the first move of the
  // drag. Rebuilding from the deformed skeletons here would read back the
  // eval-time plant's answer to our own previous write — the closed loop
  // between two solvers that oscillates joints inside pinned chains (see the
  // m_ikCrossBase* members). writeBackAnglesFor_animate writes ABSOLUTE
  // angles from `desired` alone, so a fixed baseline is exactly as valid a
  // reference as the live pose. theta below thus becomes the TOTAL rotation
  // since the press, not a per-move increment.
  if (!m_ikCrossBaseValid) {
    // First move of the drag. ensureCrossLevelBaselines_animate flushes the
    // per-frame placement caches (or the photograph below would inherit the
    // PREVIOUS drag's mid-frame state and the whole drag would solve against
    // a skewed baseline) and captures the per-column undo baselines, setting
    // m_ikCrossDragged so the button-up routes through
    // finishCrossLevelUndo_animate — the grouped multi-column undo block.
    // Before this call was wired in, undoing a cross-column pose restored
    // ONLY the current column and left every other column's ANGLEs dirty.
    ensureCrossLevelBaselines_animate(frame);
    m_ikCrossBaseGraph = buildUnifiedGraph_animate(frame);
    m_ikCrossBaseCols  = crossColumns_animate(frame);
    m_ikCrossBaseValid = true;
  }
  UnifiedGraph &g = m_ikCrossBaseGraph;
  if (g.nodes.empty() || g.pins.empty() || g.root.col < 0) return false;

  // Only engage for a pin on a CHILD column (col != root). Pins on the root
  // column keep the proven single-level path (its free root is handled by the
  // eval PINTX plant, which works for a top-level column).
  bool crossPin = false;
  for (const UNode &p : g.pins)
    if (p.col != g.root.col) {
      crossPin = true;
      break;
    }
  if (!crossPin) return false;

  // Multi-column only.
  const int curCol = ::column();
  bool multi = false;
  for (const UNode &n : g.nodes)
    if (n.col != curCol) {
      multi = true;
      break;
    }
  if (!multi) return false;

  const UNode dragged{curCol, vDragged};
  if (!g.world.count(dragged)) return false;
  for (const UNode &p : g.pins)
    if (p == dragged) return false;  // dragging the pin itself

  // A vertex BETWEEN >= 2 pins gets the multi-anchor FABRIK; the single-pivot
  // rotation below would fight the eval plant's primary translation there.
  if (crossLevelMultiAnchor_animate(dragged, mousePos)) return true;

  // Undirected adjacency over the unified parent map.
  std::map<UNode, std::vector<UNode>> adj;
  for (const auto &kv : g.parent) {
    adj[kv.first].push_back(kv.second);
    adj[kv.second].push_back(kv.first);
  }

  // Nearest pin to the dragged node (BFS): posing near a planted foot behaves
  // the same whichever foot was pinned first.
  UNode pin{-1, -1};
  {
    std::set<UNode> pinSet(g.pins.begin(), g.pins.end());
    std::set<UNode> vis;
    std::queue<UNode> q;
    q.push(dragged);
    vis.insert(dragged);
    while (!q.empty()) {
      UNode u = q.front();
      q.pop();
      if (pinSet.count(u)) {
        pin = u;
        break;
      }
      for (const UNode &w : adj[u])
        if (!vis.count(w)) {
          vis.insert(w);
          q.push(w);
        }
    }
    if (pin.col < 0) return false;  // no pin reachable
  }

  // Re-root the unified tree at the pin: it becomes the fixed base, so the
  // pinned foot stays put for free (no explicit primary translation needed).
  std::map<UNode, UNode> reParent;
  {
    std::set<UNode> vis;
    std::queue<UNode> q;
    q.push(pin);
    vis.insert(pin);
    reParent[pin] = UNode{-1, -1};
    while (!q.empty()) {
      UNode u = q.front();
      q.pop();
      for (const UNode &w : adj[u])
        if (!vis.count(w)) {
          vis.insert(w);
          reParent[w] = u;
          q.push(w);
        }
    }
    if (!vis.count(dragged)) return false;  // disconnected
  }

  UNode rp = reParent[dragged];
  if (rp.col < 0) return false;  // dragged is the pin (guarded above, defensive)

  // Glued cross-link: a child column's root sits EXACTLY on its parent's
  // attachment vertex (the hip), a zero-length bone. If the dragged node
  // coincides with its re-rooted parent, rotating about it is undefined — walk
  // up to the first ancestor at a real distance (the next real joint toward the
  // pin, e.g. the knee) and use THAT as the pivot. The skipped coincident nodes
  // are the glued partners: they must rotate together with the dragged node so
  // the joint stays stitched, so collect them to seed the moved set.
  std::vector<UNode> gluedExtra;
  while (rp.col >= 0 && norm2(g.world[dragged] - g.world[rp]) < 1e-8) {
    gluedExtra.push_back(rp);
    rp = reParent.count(rp) ? reParent[rp] : UNode{-1, -1};
  }
  if (rp.col < 0) return false;  // no non-coincident pivot found

  const TPointD pivot = g.world[rp];
  const TPointD oldD  = g.world[dragged] - pivot;
  const TPointD newD  = (getMatrix() * mousePos) - pivot;
  if (norm2(oldD) < 1e-12 || norm2(newD) < 1e-12) return false;
  const double theta = wrapPi(atan2(newD.y, newD.x) - atan2(oldD.y, oldD.x));

  // Dragged node's re-rooted subtree (+ its glued partners), rotated rigidly
  // about the pivot (constant bone lengths → a valid FK configuration the
  // per-column write-back can reproduce). Single joint: only this set moves; the
  // pin side stays put.
  std::set<UNode> moved;
  {
    std::queue<UNode> q;
    q.push(dragged);
    moved.insert(dragged);
    for (const UNode &e : gluedExtra) {
      if (moved.insert(e).second) q.push(e);
    }
    while (!q.empty()) {
      UNode u = q.front();
      q.pop();
      for (const UNode &w : adj[u])
        if (reParent.count(w) && reParent[w] == u && !moved.count(w)) {
          moved.insert(w);
          q.push(w);
        }
    }
  }

  // Probe for the reported difference against a single-level rig (2026-07-27):
  // there, posing stops at the next joint toward the pin. Here the glued
  // cross-link walk-up above can push the pivot one real joint further — split
  // the leg at the knee and the knee's two coincident nodes travel with the
  // dragged set, so the rotation looks like it pivots at the calf. `glued` > 0
  // with a pivot that is not the joint under the hand is that case.
  if (::getenv("ZTORYC_PIN_DIAG"))
    qDebug().noquote()
        << QString("[XLEVEL] dragged=(%1,%2) pivot=(%3,%4) glued=%5 moved=%6 "
                   "pin=(%7,%8) thetaDeg=%9 lever=%10")
               .arg(dragged.col).arg(dragged.vtx)
               .arg(rp.col).arg(rp.vtx)
               .arg((int)gluedExtra.size())
               .arg((int)moved.size())
               .arg(pin.col).arg(pin.vtx)
               .arg(theta * M_180_PI, 0, 'f', 1)
               .arg(norm(oldD), 0, 'f', 2);

  // The whole re-rooted chain from the dragged node down to the pin, with the
  // segment lengths: that is what says which joint the pivot actually landed
  // on, and whether one segment too many travels with the drag. '<' marks the
  // pivot, '*' a node that moves.
  if (::getenv("ZTORYC_PIN_DIAG")) {
    QString chain;
    for (UNode u = dragged;;) {
      chain += QString("(%1,%2)%3%4")
                   .arg(u.col).arg(u.vtx)
                   .arg(u == rp ? "<" : "")
                   .arg(moved.count(u) ? "*" : "");
      auto it = reParent.find(u);
      if (it == reParent.end() || it->second.col < 0) break;
      chain += QString(" --%1-- ")
                   .arg(norm(g.world[u] - g.world[it->second]), 0, 'f', 1);
      u = it->second;
    }
    qDebug().noquote() << "[XCHAIN] " << chain;
  }

  std::map<UNode, TPointD> desired = g.world;
  const double c = cos(theta), s = sin(theta);
  for (const UNode &u : moved) {
    const TPointD d = g.world[u] - pivot;
    desired[u] = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
  }

  // Dispatch per touched column: ANGLE deltas everywhere, plus ROOTX/ROOTY on
  // the one effective root column (its free-root translation). Always visit the
  // root column so its offset is refreshed even if only its root node moved.
  std::set<int> touchedCols;
  for (const UNode &u : moved) touchedCols.insert(u.col);
  touchedCols.insert(g.root.col);
  // Baseline snapshots, NOT crossColumns_animate(frame): cc.world of a child
  // column follows its parent's attachment vertex, so re-fetching mid-drag
  // would map `desired` (baseline world space) through a drifted affine.
  // curLocal only enumerates joints in writeBackAnglesFor_animate — the
  // baseline deformed pose serves that equally well.
  for (const CrossCol &cc : m_ikCrossBaseCols) {
    if (!touchedCols.count(cc.columnIndex)) continue;
    const TAffine wInv = cc.world.inv();
    std::map<int, TPointD> curLocal, desLocal;
    for (auto vt = cc.deformed.vertices().begin();
         vt != cc.deformed.vertices().end(); ++vt) {
      curLocal[(int)vt.m_idx] = vt->P();
      desLocal[(int)vt.m_idx] =
          wInv * desired[UNode{cc.columnIndex, (int)vt.m_idx}];
    }
    writeBackAnglesFor_animate(cc.rest, cc.def, cc.skelId, cc.paramFrame,
                               curLocal, desLocal, true,
                               /*writeRootOffset=*/cc.columnIndex == g.root.col);
  }

  // The ANGLEs just written move attachment vertices, so the connected
  // columns' same-frame placement caches now lie: flush them, or the overlay
  // (connectedSkeletons_animate -> getColumnMatrix) keeps drawing a limb's
  // skeleton where the column WAS until some unrelated click refreshes it.
  invalidateConnectedPlacements_animate();
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
  m_ikCrossBaseValid = false;
  m_ikCrossBaseGraph = UnifiedGraph();
  m_ikCrossBaseCols.clear();
}

//------------------------------------------------------------------------


//------------------------------------------------------------------------

// The mesh AFTER the ARAP solve — the space a MeshCorrective's offsets live in,
// and the only positions the corrective brush can sensibly hit-test against.
// The identity affine is deliberate: in Build mode the tool draws raw mesh
// vertices in its own space, so mesh space and tool space are the same thing.
// If the overlay ever lands off the drawing, THIS is the assumption to revisit.
std::vector<std::pair<PlasticTool::MeshIndex, TPointD>>
PlasticTool::deformedMeshVertices_animate() {
  std::vector<std::pair<MeshIndex, TPointD>> out;
  if (!m_sd) return out;

  TMeshImageP mi = TImageP(getImage(false));
  if (!mi) return out;

  TStageObject *obj = ::stageObject();
  if (!obj) return out;

  const double sdFrame = obj->paramsTime(::frame());
  const int skelId     = m_sd->skeletonId(sdFrame);

  const PlasticDeformerDataGroup *dg =
      PlasticDeformerStorage::instance()->process(
          sdFrame, mi.getPointer(), m_sd.getPointer(), skelId, TAffine(),
          PlasticDeformerStorage::MESH);
  if (!dg) return out;

  const int meshCount = (int)mi->meshes().size();
  for (int m = 0; m < meshCount; ++m) {
    const double *o = dg->m_datas[m].m_output.get();
    if (!o) continue;
    const int vCount = (int)mi->meshes()[m]->verticesCount();
    for (int v = 0; v < vCount; ++v)
      out.push_back(std::make_pair(MeshIndex(m, v), TPointD(o[2 * v], o[2 * v + 1])));
  }
  return out;
}

//------------------------------------------------------------------------

// Verification overlay for the corrective brush (milestone 2, step 1). Draws
// nothing but the deformed mesh vertices and the brush circle: if the dots sit
// on the drawing, the whole coordinate chain from cursor to post-solve mesh is
// right, and everything else is arithmetic.

//------------------------------------------------------------------------

std::vector<std::pair<PlasticTool::MeshIndex, TPointD>>
PlasticTool::brushedVertices_animate(const TPointD &c, double radius) {
  std::vector<std::pair<MeshIndex, TPointD>> out;
  if (radius <= 0.0) return out;

  const std::vector<std::pair<MeshIndex, TPointD>> all =
      deformedMeshVertices_animate();
  if (all.empty()) return out;

  TMeshImageP mi = TImageP(getImage(false));
  if (!mi) return out;

  // Seed: the deformed vertex nearest the brush centre. Its REST position is
  // what the mesh BFS can start from — buildDistances walks the mesh's own
  // coordinates, while the brush lives in deformed space.
  int seedMesh = -1, seedVertex = -1;
  double best  = 0.0;
  for (size_t i = 0; i < all.size(); ++i) {
    const double d2 = norm2(all[i].second - c);
    if (seedVertex < 0 || d2 < best) {
      best       = d2;
      seedMesh   = all[i].first.m_meshIdx;
      seedVertex = all[i].first.m_idx;
    }
  }
  if (seedVertex < 0 || seedMesh < 0) return out;
  if (best > radius * radius) return out;  // brush is off the mesh entirely

  const TTextureMesh &mesh = *mi->meshes()[seedMesh];
  // NOT reached must mean far, not near. buildDistances only writes the
  // vertices its BFS visits, so a mesh island that is disconnected from the
  // seed — an arm, a leg — keeps whatever was in the array. Left at zero it
  // reads as "on top of the brush" and passes every test, which is exactly how
  // painting the head assigned the limbs too.
  std::vector<float> dist(mesh.verticesCount(),
                          (std::numeric_limits<float>::max)());
  int faceHint = -1;
  if (!::buildDistances(&dist.front(), mesh, mesh.vertex(seedVertex).P(),
                        &faceHint)) {
    // No containing face: fall back to the plain screen-space brush rather than
    // silently doing nothing.
    for (size_t i = 0; i < all.size(); ++i)
      if (norm(all[i].second - c) < radius) out.push_back(all[i]);
    return out;
  }

  int inCircle = 0;
  for (size_t i = 0; i < all.size(); ++i) {
    const MeshIndex &m = all[i].first;
    if (m.m_meshIdx != seedMesh) continue;
    if (norm(all[i].second - c) >= radius) continue;
    ++inCircle;
    if (m.m_idx < (int)dist.size() && dist[m.m_idx] > radius) continue;
    out.push_back(all[i]);
  }

  if (::getenv("ZTORYC_BRUSH_DIAG")) {
    float dmin = 0.0f, dmax = 0.0f;
    for (size_t i = 0; i < dist.size(); ++i) {
      if (i == 0 || dist[i] < dmin) dmin = dist[i];
      if (i == 0 || dist[i] > dmax) dmax = dist[i];
    }
    qDebug().noquote()
        << QString("[BRUSH] mesh=%1 verts=%2 R=%3 inCircle=%4 kept=%5 "
                   "meshDist min=%6 max=%7 seedV=%8")
               .arg(seedMesh).arg((int)dist.size())
               .arg(radius, 0, 'f', 1).arg(inCircle).arg((int)out.size())
               .arg(dmin, 0, 'f', 2).arg(dmax, 0, 'f', 2).arg(seedVertex);
  }
  return out;
}


//------------------------------------------------------------------------

std::map<int, int> PlasticTool::nearestJointPerVertex_animate(int meshIdx) {
  std::map<int, int> out;
  if (!m_sd || meshIdx < 0) return out;

  PlasticSkeletonP skel = m_sd->skeleton(::skeletonId());
  if (!skel) return out;

  TMeshImageP mi = TImageP(getImage(false));
  if (!mi || meshIdx >= (int)mi->meshes().size()) return out;

  const TTextureMesh &mesh = *mi->meshes()[meshIdx];
  const int vCount         = mesh.verticesCount();
  if (vCount <= 0) return out;

  const float kFar = (std::numeric_limits<float>::max)();
  std::vector<float> best(vCount, kFar);
  std::vector<int> bestJoint(vCount, -1);
  std::vector<float> dist(vCount, kFar);

  for (auto vt = skel->vertices().begin(); vt != skel->vertices().end(); ++vt) {
    // Re-fill every pass: unvisited entries are left untouched by the BFS, and
    // a stale value from the previous joint would read as this joint's distance.
    std::fill(dist.begin(), dist.end(), kFar);
    int faceHint = -1;
    if (!::buildDistances(&dist.front(), mesh, vt->P(), &faceHint)) continue;
    for (int v = 0; v < vCount; ++v) {
      if (dist[v] >= kFar) continue;  // this joint cannot reach v at all
      if (bestJoint[v] < 0 || dist[v] < best[v]) {
        best[v]      = dist[v];
        bestJoint[v] = (int)vt.m_idx;
      }
    }
  }

  for (int v = 0; v < vCount; ++v)
    if (bestJoint[v] >= 0) out[v] = bestJoint[v];
  return out;
}

void PlasticTool::drawCorrectiveSculpt_animate(double pixelSize) {
  if (!m_correctiveSculpt.getValue()) return;

  const std::vector<std::pair<MeshIndex, TPointD>> verts =
      deformedMeshVertices_animate();

  const double r = 1.6 * pixelSize;
  // Owned vertices are drawn apart so the cut you made is visible instead of
  // remembered — and so you can see where it actually landed.
  for (int pass = 0; pass < 2; ++pass) {
    if (pass == 0)
      glColor4ub(60, 130, 255, 200);
    else
      glColor4ub(255, 90, 160, 230);
    glBegin(GL_QUADS);
    for (size_t i = 0; i < verts.size(); ++i) {
      QString owner;
      const MeshIndex &mi = verts[i].first;
      const bool owned =
          m_sd && m_sd->soOwner(mi.m_meshIdx, mi.m_idx, owner);
      if (owned != (pass == 1)) continue;
      const TPointD &p = verts[i].second;
      const double s   = owned ? r * 1.6 : r;
      glVertex2d(p.x - s, p.y - s);
      glVertex2d(p.x + s, p.y - s);
      glVertex2d(p.x + s, p.y + s);
      glVertex2d(p.x - s, p.y + s);
    }
    glEnd();
  }

  const double br = m_correctiveRadius.getValue();
  glColor4ub(255, 160, 0, 220);
  glLineWidth(1.5f);
  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < 48; ++i) {
    const double a = i * (2.0 * M_PI / 48);
    glVertex2d(m_pos.x + br * cos(a), m_pos.y + br * sin(a));
  }
  glEnd();
  glLineWidth(1.0f);
}


//------------------------------------------------------------------------

namespace {

// Undo for one brush stroke: the corrective list is small and sculpting is not
// a per-event operation, so a whole-list snapshot is both simplest and exact.
class CorrectiveStrokeUndo final : public TUndo {
  SkDP m_sd;
  std::vector<MeshCorrective> m_before, m_after;

public:
  CorrectiveStrokeUndo(const SkDP &sd, const std::vector<MeshCorrective> &before,
                       const std::vector<MeshCorrective> &after)
      : m_sd(sd), m_before(before), m_after(after) {}

  void undo() const override {
    if (m_sd) m_sd->setMeshCorrectives(m_before);
    PlasticDeformerStorage::instance()->invalidateDeformation(m_sd.getPointer());
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    if (m_sd) m_sd->setMeshCorrectives(m_after);
    PlasticDeformerStorage::instance()->invalidateDeformation(m_sd.getPointer());
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return "ZtoRig: sculpt joint corrective"; }
};

// Undo for one ownership stroke. Same whole-map snapshot as the shape stroke:
// the map is sparse and a stroke is not a per-event operation.
class SOOwnerStrokeUndo final : public TUndo {
  SkDP m_sd;
  std::map<int, std::map<int, QString>> m_before, m_after;

public:
  SOOwnerStrokeUndo(const SkDP &sd,
                    const std::map<int, std::map<int, QString>> &before,
                    const std::map<int, std::map<int, QString>> &after)
      : m_sd(sd), m_before(before), m_after(after) {}

  void undo() const override {
    if (m_sd) m_sd->setSOOwners(m_before);
    PlasticDeformerStorage::instance()->invalidateDeformation(m_sd.getPointer());
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    if (m_sd) m_sd->setSOOwners(m_after);
    PlasticDeformerStorage::instance()->invalidateDeformation(m_sd.getPointer());
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return "ZtoRig: assign stacking order"; }
};

}  // namespace

// Pick (or create) the corrective this stroke writes into, and snapshot for undo.
//
// The name carries the angle — "elbow_dx_95" — so sculpting the same joint at a
// second bend makes a SECOND corrective instead of overwriting the first, which
// is what a joint needs: one shape at 45 and another at 95 read very differently.
// The new one starts fading in where the previous one finished (restAngle), and
// since the brush records the delta against WHAT IS ON SCREEN — which already
// includes the lower correctives — the layers add up instead of double-counting.
bool PlasticTool::beginCorrectiveStroke_animate() {
  if (!m_sd) return false;
  if (m_svSel.isEmpty()) return false;  // nothing to work from

  PlasticSkeletonP skel = m_sd->skeleton(::skeletonId());
  if (!skel) return false;
  // The shape brush needs ONE driver joint (whose angle fades the corrective);
  // the order brush wants SEVERAL — two of them are how you say "this bone".
  const bool orderMode = m_correctiveOrder.getValue();
  if (!orderMode && !m_svSel.hasSingleObject()) return false;
  const int v = orderMode ? m_svSel.objects().front() : (int)m_svSel;
  const QString driver = skel->vertex(v).name();

  // Ownership stroke: no corrective involved, and no angle required — declaring
  // that a vertex belongs to a joint is a fact about the rig, not about a pose.
  m_correctiveOrderStroke = m_correctiveOrder.getValue();
  if (m_correctiveOrderStroke) {
    m_soOwnersBefore = m_sd->getSOOwners();
    m_correctiveName = driver;  // the joint the brush hands vertices to

    // Confine the stroke to the mesh the selected joint sits on, found by the
    // vertex nearest to it. A limb drawn over the body puts both sets of points
    // under the same brush, and there is no way to aim between them by eye.
    m_correctiveMeshIdx = -1;
    m_correctiveOwnerJoint.clear();
    m_correctiveOwnerMesh = -1;
    {
      const TPointD jp = deformedSkeleton().vertex(v).P();
      double best      = 0.0;
      const std::vector<std::pair<MeshIndex, TPointD>> verts =
          deformedMeshVertices_animate();
      for (size_t i = 0; i < verts.size(); ++i) {
        const double d2 = norm2(verts[i].second - jp);
        if (m_correctiveMeshIdx < 0 || d2 < best) {
          best                = d2;
          m_correctiveMeshIdx = verts[i].first.m_meshIdx;
        }
      }
      if (m_correctiveMeshIdx >= 0) {
        m_correctiveOwnerJoint =
            nearestJointPerVertex_animate(m_correctiveMeshIdx);
        m_correctiveOwnerMesh = m_correctiveMeshIdx;
      }
    }
    return true;
  }

  SkVD *vd = m_sd->vertexDeformation(::skeletonId(), v);
  if (!vd || !vd->m_params[SkVD::ANGLE]) return false;
  const double angle = vd->m_params[SkVD::ANGLE]->getValue(::frame());
  if (fabs(angle) < 1.0) return false;  // sculpting at rest would fade to nothing

  m_correctiveUndoBefore = m_sd->getMeshCorrectives();

  const QString name = driver + "_" + QString::number((int)qRound(fabs(angle)));
  if (!m_sd->meshCorrective(name)) {
    const int idx = m_sd->addMeshCorrective(name);
    MeshCorrective *mc = m_sd->meshCorrective(idx);
    if (!mc) return false;
    mc->m_driverVertexName = driver;
    mc->m_fullAngle        = angle;
    // Start where the closest lower corrective on this same joint finished.
    double rest = 0.0;
    for (int i = 0; i < m_sd->meshCorrectivesCount(); ++i) {
      const MeshCorrective *o = m_sd->meshCorrective(i);
      if (!o || o == mc || o->m_driverVertexName != driver) continue;
      if (fabs(o->m_fullAngle) < fabs(angle) && fabs(o->m_fullAngle) > fabs(rest))
        rest = o->m_fullAngle;
    }
    mc->m_restAngle = rest;
  }
  m_correctiveName = name;
  return true;
}

// One brush step. Offsets are stored in mesh OUTPUT space, the same space the
// vertices come back in, so the drag vector needs no conversion.
void PlasticTool::applyCorrectiveBrush_animate(const TPointD &from,
                                               const TPointD &to) {
  if (m_correctiveName.isEmpty() || !m_sd) return;

  const double R = m_correctiveRadius.getValue();
  if (R <= 0.0) return;

  const std::vector<std::pair<MeshIndex, TPointD>> verts =
      brushedVertices_animate(to, R);

  // Ownership: a hard stamp, deliberately without falloff. A soft edge here
  // would blend the two joints' SO all over again, which is the whole problem.
  if (m_correctiveOrderStroke) {
    bool touched = false;
    for (size_t i = 0; i < verts.size(); ++i) {
      const MeshIndex &mi = verts[i].first;
      // Only points that BELONG to a selected joint. Select the elbow and the
      // wrist and the forearm becomes paintable while the upper arm cannot be
      // touched at all — the overlap stops being something to avoid by hand.
      if (mi.m_meshIdx == m_correctiveOwnerMesh &&
          !m_correctiveOwnerJoint.empty()) {
        std::map<int, int>::const_iterator jt =
            m_correctiveOwnerJoint.find(mi.m_idx);
        if (jt == m_correctiveOwnerJoint.end()) continue;
        const std::vector<int> &sel = m_svSel.objects();
        if (!sel.empty() &&
            std::find(sel.begin(), sel.end(), jt->second) == sel.end())
          continue;
      }
      if (m_correctiveErase)
        m_sd->clearSOOwner(mi.m_meshIdx, mi.m_idx);
      else {
        // Each point goes to ITS OWN nearest selected joint, not to a single
        // one for the whole stroke: that is what "belongs to" already means
        // everywhere else here. Give both joints of a bone the same SO and the
        // region reads as one; give them different ones and it splits, which is
        // occasionally what you want.
        QString owner = m_correctiveName;
        if (mi.m_meshIdx == m_correctiveOwnerMesh) {
          std::map<int, int>::const_iterator jt =
              m_correctiveOwnerJoint.find(mi.m_idx);
          PlasticSkeletonP sk = m_sd->skeleton(::skeletonId());
          if (jt != m_correctiveOwnerJoint.end() && sk)
            owner = sk->vertex(jt->second).name();
        }
        m_sd->setSOOwner(mi.m_meshIdx, mi.m_idx, owner);
      }
      touched = true;
    }
    if (touched) {
      PlasticDeformerStorage::instance()->invalidateDeformation(
          m_sd.getPointer(), PlasticDeformerStorage::SO);
      invalidate();
    }
    return;
  }

  MeshCorrective *mc = m_sd->meshCorrective(m_correctiveName);
  if (!mc) return;

  const TPointD d = to - from;
  if (norm2(d) < 1e-12) return;

  for (size_t i = 0; i < verts.size(); ++i) {
    const double dist = norm(verts[i].second - to);
    // Smoothstep falloff: full at the centre, zero at the rim, and flat at both
    // ends so repeated strokes do not leave a ridge at the brush edge.
    const double t = dist / R;
    const double w = 1.0 - t * t * (3.0 - 2.0 * t);
    const MeshIndex &mi = verts[i].first;
    TPointD &off = mc->m_deltas[mi.m_meshIdx][mi.m_idx];
    off = off + d * w;
  }

  PlasticDeformerStorage::instance()->invalidateDeformation(m_sd.getPointer());
  invalidate();
}

void PlasticTool::endCorrectiveStroke_animate() {
  if (m_correctiveName.isEmpty() || !m_sd) return;
  if (m_correctiveOrderStroke) {
    TUndoManager::manager()->add(
        new SOOwnerStrokeUndo(m_sd, m_soOwnersBefore, m_sd->getSOOwners()));
    m_correctiveOrderStroke = false;
    m_correctiveErase       = false;
    m_correctiveMeshIdx     = -1;
    m_soOwnersBefore.clear();
    m_correctiveName.clear();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    return;
  }
  TUndoManager::manager()->add(new CorrectiveStrokeUndo(
      m_sd, m_correctiveUndoBefore, m_sd->getMeshCorrectives()));
  m_correctiveName.clear();
  m_correctiveUndoBefore.clear();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
}

void PlasticTool::draw_animate() {
  double pixelSize = getPixelSize();

  PlasticSkeleton &deformedSkeleton = this->deformedSkeleton();

  // Draw deformed skeleton
  if (m_sd) {
    // SuperPlastic multi-level: draw the skeletons of the hierarchically
    // connected columns as dimmed context, so the whole articulated character
    // (spread across several drawing levels) is visible at once. Each is placed
    // via its own affine into the current tool's draw space.
    const std::vector<ConnectedSkel> connSkels = connectedSkeletons_animate();
    for (const ConnectedSkel &cs : connSkels) {
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
    drawCorrectiveSculpt_animate(pixelSize);
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

    // Pins on the CONNECTED columns too: a planted hand must stay visible
    // while posing from the body column. Before, the diamond existed only
    // while its own column was current — so placing a pin on another level's
    // vertex gave no visual feedback at all, and already-planted limbs looked
    // free the moment you switched column. Read each column's PIN params
    // directly (same convention as hasCrossLevelPin_animate / the eval) and
    // map the deformed position through that column's toCur affine.
    // Same gate as the current column's diamonds above: this loop reads the PIN
    // params directly instead of going through pinnedVerticesAtFrame, so it was
    // the one path that kept drawing pins with IK switched off.
    if (TXsheet *xsh = (m_sd && m_sd->pinsEnabled())
                           ? TTool::getApplication()->getCurrentXsheet()->getXsheet()
                           : nullptr)
      for (const ConnectedSkel &cs : connSkels) {
        TStageObject *obj =
            xsh->getStageObject(TStageObjectId::ColumnId(cs.columnIndex));
        if (!obj) continue;
        const PlasticSkeletonDeformationP &def =
            obj->getPlasticSkeletonDeformation();
        if (!def) continue;
        const double sdFr = obj->paramsTime((double)::frame());
        const int skId    = def->skeletonId(sdFr);
        glColor4ub(0, 220, 255, 255);
        for (auto vt = cs.skel.vertices().begin();
             vt != cs.skel.vertices().end(); ++vt) {
          SkVD *vd = def->vertexDeformation(skId, (int)vt.m_idx);
          if (!vd || !vd->m_params[SkVD::PIN] ||
              vd->m_params[SkVD::PIN]->getValue(sdFr) < 0.5)
            continue;
          const TPointD p = cs.toCur * vt->P();
          glBegin(GL_LINE_LOOP);
          glVertex2d(p.x, p.y - r);
          glVertex2d(p.x + r, p.y);
          glVertex2d(p.x, p.y + r);
          glVertex2d(p.x - r, p.y);
          glEnd();
        }
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
