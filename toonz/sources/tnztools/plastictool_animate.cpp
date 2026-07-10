

// TnzCore includes
#include "tundo.h"
#include "tgl.h"

// TnzLib includes
#include "toonz/ikccd.h"
#include "toonz/tframehandle.h"
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

    // Controller gizmo hover: handles win over vertices, like the click does
    m_ctrlHighlight = controllerHitTest_animate(pos);
    if (m_ctrlHighlight != CtrlNone) m_svHigh = -1;

    invalidate();
  }
}

//------------------------------------------------------------------------

void PlasticTool::leftButtonDown_animate(const TPointD &pos,
                                         const TMouseEvent &me) {
  // Track mouse position
  m_pressedPos = m_pos = pos;

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

  if (m_ctrlDevice != CtrlNone) {
    controllerDrag_animate(pos, me);
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
  if (!m_sd) return CtrlNone;
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
    const std::map<int, TPointD> &desired) {
  const PlasticSkeleton &orig = *skeleton();

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

  int skelId = ::skeletonId();
  for (const auto &kv : curPos) {
    int w  = kv.first;
    int op = orig.vertex(w).parent();
    if (op < 0) continue;  // skeleton root has no ANGLE param
    TPointD od  = orig.vertex(w).P() - orig.vertex(op).P();
    double rest = wrapPi(atan2(od.y, od.x) - parentDir(origFn, op));
    TPointD nd  = desired.at(w) - desired.at(op);
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
  // the fresh, unpinned FK): the vertex stays visually where it was.
  if (pinned && !planted.empty()) {
    m_deformedSkeleton.invalidate();
    PlasticSkeleton &ds = deformedSkeleton();
    std::map<int, TPointD> cur;
    for (auto vt = ds.vertices().begin(); vt != ds.vertices().end(); ++vt)
      cur[vt.m_idx] = vt->P();
    writeBackAngles_animate(frame, cur, planted);
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
  }

  drawHighlights(m_sd, &deformedSkeleton, pixelSize);
}
