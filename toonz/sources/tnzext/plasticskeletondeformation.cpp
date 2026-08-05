

// TnzCore includes
#include "tstream.h"

// TnzBase includes
#include "tdoublekeyframe.h"
#include "tparamchange.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticdeformerstorage.h"

// tcg includes
#include "tcg/tcg_misc.h"

// Qt includes
#include <QDebug>
#include <QString>

// STL includes
#include <atomic>
#include <memory>
#include <set>
#include <map>
#include <vector>
#include <algorithm>

// Boost includes
#include <boost/iterator/transform_iterator.hpp>

#include "ext/plasticskeletondeformation.h"

PERSIST_IDENTIFIER(PlasticSkeletonVertexDeformation,
                   "PlasticSkeletonVertexDeformation")
PERSIST_IDENTIFIER(PlasticSkeletonDeformation, "PlasticSkeletonDeformation")

DEFINE_CLASS_CODE(PlasticSkeletonDeformation, 121)

//**************************************************************************************
//    Local  namespace
//**************************************************************************************

namespace {

static const char *parNames[SkVD::PARAMS_COUNT] = {
    "Angle",  "Distance", "SO",     "Pin",      "PinTX",
    "PinTY",  "PinWX",    "PinWY",  "RootX",    "RootY",
    "ScaleX", "ScaleY",   "PivotX", "PivotY",   "TransX",
    "TransY", "Rotation", "ShearX", "ShearY",   "MinAngle",
    "MaxAngle"};
static const char *parMeasures[SkVD::PARAMS_COUNT] = {
    "angle",    "fxLength", "",         "",         "fxLength",
    "fxLength", "fxLength", "fxLength", "fxLength", "fxLength",
    "scale",    "scale",    "fxLength", "fxLength", "fxLength",
    "fxLength", "angle",    "shear",    "shear",    "angle",
    "angle"};

//------------------------------------------------------------------

// Extract the angle between edges vParent->v and vGrandParent->vParent
double buildAngle(const PlasticSkeleton &skeleton, int v) {
  const PlasticSkeletonVertex &vx = skeleton.vertex(v);
  int vParent                     = vx.parent();

  assert(vx.parent() >= 0);

  const PlasticSkeletonVertex &vxParent = skeleton.vertex(vParent);
  int vGrandParent                      = vxParent.parent();

  // Build reference orientation
  TPointD dir(1.0, 0.0);  // Standard horizontal axis if no grandParent
  if (vGrandParent >= 0) {
    // Relative orientation
    const PlasticSkeletonVertex &vxGrandParent = skeleton.vertex(vGrandParent);
    dir = vxParent.P() - vxGrandParent.P();
  }

  return tcg::point_ops::angle(dir, vx.P() - vxParent.P()) * M_180_PI;
}

//------------------------------------------------------------------

// Effective angular limits at a frame: a keyed MINANGLE/MAXANGLE param
// overrides the vertex's static limit (so joint limits can change over time);
// with no keys the static PlasticSkeletonVertex limit is used (backward
// compatible — old scenes and un-keyed joints behave exactly as before).
void effAngleLimits(const SkVD *vd, const PlasticSkeletonVertex &vx,
                    double frame, double &lo, double &hi) {
  lo = vx.m_minAngle;
  hi = vx.m_maxAngle;
  if (!vd) return;
  if (vd->m_params[SkVD::MINANGLE] &&
      vd->m_params[SkVD::MINANGLE]->getKeyframeCount() > 0)
    lo = vd->m_params[SkVD::MINANGLE]->getValue(frame);
  if (vd->m_params[SkVD::MAXANGLE] &&
      vd->m_params[SkVD::MAXANGLE]->getKeyframeCount() > 0)
    hi = vd->m_params[SkVD::MAXANGLE]->getValue(frame);
}

}  // namespace

//**************************************************************************************
//    PlasticSkeletonVertex  implementation
//**************************************************************************************

// ZtoRig: l'unica definizione di cosa sia una POSA. Dichiarata nell'header
// perche' la leggono in tre — Global Key, registrazione della posa e pose
// blending — e tre copie separate un giorno divergono. Vedi il commento sulla
// dichiarazione per il motivo per cui PIN* e MIN/MAXANGLE restano fuori.
const int SkVD::POSE_PARAMS[] = {
    SkVD::ANGLE,  SkVD::DISTANCE, SkVD::SO,     SkVD::ROOTX,  SkVD::ROOTY,
    SkVD::SCALEX, SkVD::SCALEY,   SkVD::PIVOTX, SkVD::PIVOTY, SkVD::TRANSX,
    SkVD::TRANSY, SkVD::ROT,      SkVD::SHEARX, SkVD::SHEARY};

const int SkVD::POSE_PARAMS_COUNT =
    (int)(sizeof(SkVD::POSE_PARAMS) / sizeof(SkVD::POSE_PARAMS[0]));

int SkVD::poseParamSlot(int param) {
  for (int i = 0; i < POSE_PARAMS_COUNT; ++i)
    if (POSE_PARAMS[i] == param) return i;
  return -1;
}

//------------------------------------------------------------------

SkVD::Keyframe SkVD::getKeyframe(double frame) const {
  Keyframe kf;
  for (int p          = 0; p < PARAMS_COUNT; ++p)
    kf.m_keyframes[p] = m_params[p]->getKeyframeAt(frame);

  return kf;
}

//------------------------------------------------------------------

void SkVD::setKeyframe(double frame) {
  // Only the transform params (ANGLE/DISTANCE/SO) — PIN is an IK anchor toggle
  // and must NOT get a spurious keyframe from a plain Set Key. NOTE: the tool's
  // Set Key does NOT go through here (it uses a TnzLib KeyframeSetter helper in
  // plastictool.cpp); the pin-locking on global key lives there.
  for (int p = 0; p < PIN; ++p)
    m_params[p]->setKeyframe(m_params[p]->getKeyframeAt(frame));
}

//------------------------------------------------------------------

bool SkVD::setKeyframe(const SkVD::Keyframe &values) {
  bool keyWasSet = false;
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (values.m_keyframes[p].m_isKeyframe) {
      m_params[p]->setKeyframe(values.m_keyframes[p]);
      keyWasSet = true;
    }

  return keyWasSet;
}

//------------------------------------------------------------------

bool SkVD::setKeyframe(const SkVD::Keyframe &values, double frame,
                       double easeIn, double easeOut) {
  bool keyWasSet = false;

  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (values.m_keyframes[p].m_isKeyframe) {
      TDoubleKeyframe kf(values.m_keyframes[p]);
      kf.m_frame = frame;

      if (easeIn >= 0.0) kf.m_speedIn   = TPointD(-easeIn, kf.m_speedIn.y);
      if (easeOut >= 0.0) kf.m_speedOut = TPointD(easeOut, kf.m_speedOut.y);

      m_params[p]->setKeyframe(kf);
      keyWasSet = true;
    }

  return keyWasSet;
}

//------------------------------------------------------------------

bool SkVD::isKeyframe(double frame) const {
  for (int p = 0; p < PARAMS_COUNT; ++p)
    if (m_params[p]->isKeyframe(frame)) return true;

  return false;
}

//------------------------------------------------------------------

bool SkVD::isFullKeyframe(double frame) const {
  // PIN excluded: it's an independent IK anchor toggle, not part of the
  // vertex transform, so it must not affect the full/partial key indicator.
  for (int p = 0; p < PIN; ++p)
    if (!m_params[p]->isKeyframe(frame)) return false;

  return true;
}

//------------------------------------------------------------------

void SkVD::deleteKeyframe(double frame) {
  for (int p = 0; p < PARAMS_COUNT; ++p) m_params[p]->deleteKeyframe(frame);
}

//------------------------------------------------------------------

void SkVD::saveData(TOStream &os) {
  for (int p = 0; p < PARAMS_COUNT; ++p) {
    // SCALEX/SCALEY are scale FACTORS with default 1.0 (100%), but
    // TDoubleParam::isDefault() only recognizes zero-defaults: check them
    // explicitly so untouched scales are still not serialized.
    bool isDef = (p == SCALEX || p == SCALEY)
                     ? (m_params[p]->getKeyframeCount() == 0 &&
                        m_params[p]->getDefaultValue() == 1.0)
                     : m_params[p]->isDefault();
    if (!isDef) os.child(::parNames[p]) << *m_params[p];
  }
}

//------------------------------------------------------------------

void SkVD::loadData(TIStream &is) {
  std::string tagName;

  while (is.matchTag(tagName)) {
    int p;
    for (p = 0; p < PARAMS_COUNT; ++p) {
      if (tagName == parNames[p]) {
        is >> *m_params[p], is.matchEndTag();
        break;
      }
    }

    if (p >= PARAMS_COUNT) is.skipCurrentTag();
  }
}

//**************************************************************************************
//    PlasticSkeletonDeformation::Imp  definition
//**************************************************************************************

class PlasticSkeletonDeformation::Imp final : public TParamObserver {
public:
  PlasticSkeletonDeformation *m_back;  //!< Back-pointer to the interface class

  SkeletonSet m_skeletons;  //!< Skeletons owned by the deformation
  SkVDSet m_vds;            //!< Container of vertex deformations

  TDoubleParamP m_skelIdsParam;  //!< Curve of skeleton ids by xsheet frame

  bool m_pinsEnabled = true;  //!< SuperPlastic: whether the IK pin
                              //!< constraints are applied at evaluation
                              //!< (scene data, keys untouched when off)

  // STEP C.2: local-space targets for SECONDARY cross-column pins, pushed down
  // by the stage-level solve (see setSecondaryPinTargets). Transient: valid only
  // for m_secondaryFrame, never serialized.
  std::map<int, TPointD> m_secondaryTargets;
  double m_secondaryFrame = -1.0e30;

  //! ZtoRig: blendable pose actions. Empty on every scene that never used
  //! them, and serialized only when non-empty, so old scenes round-trip
  //! byte-identical.
  std::vector<PoseAction> m_poseActions;

  //! ZtoRig: joint correctives (mesh pose-space deformation). Same empty-by-
  //! default, serialize-only-when-used discipline as the pose actions.
  std::vector<MeshCorrective> m_meshCorrectives;
  std::map<int, std::map<int, QString>> m_soOwners;  //!< SO ownership

  // Base values frozen at the start of an Offset drag, keyed by vertex name and
  // param index. Empty when no drag is running. See beginPoseDrag().
  std::map<std::pair<QString, int>, double> m_poseDragBase;
  int m_poseDragIdx = -1;

  //! ZtoRig: records how much of action \p idx is applied at \p frame in its
  //! guide curve, and clears the other actions when this one is an absolute
  //! Pose (which overwrites the whole skeleton, so nothing else is left).
  void writePoseStrength(int idx, double strength, double frame);

  //! Whether two actions drive any of the same (vertex, param) — which is when
  //! stamping one invalidates the other's record.
  static bool overlaps(const PoseAction &a, const PoseAction &b);

  // STEP C.2b: result of the character-level solve for this column. Transient.
  PlasticSkeleton m_solvedSkeleton;
  double m_solvedFrame = -1.0e30;
  int m_solvedSkelId   = -1;
  bool m_hasSolved     = false;

  std::set<TParamObserver *>
      m_observers;  //!< Set of the deformation's observers

  TSyntax::Grammar
      *m_grammar;  //!< The params' grammar. Weird though - it's a VERY
                   //!< occult requirement to TDoubleParams...

  // NOTE: There \a is a deformation even for a skeleton's root node. This is
  // now required due to the
  // ownership of \a multiple skeletons at once. However, its angle and distance

  // params will be unused.

public:
  Imp(PlasticSkeletonDeformation *back);
  ~Imp();

  Imp(PlasticSkeletonDeformation *back, const Imp &other);
  Imp &operator=(const Imp &other);

  PlasticSkeleton &skeleton(int skelId) const;
  SkVD &vertexDeformation(const QString &name) const;

  void attach(int skeletonId, PlasticSkeleton *skeleton);
  void detach(int skeletonId);

  void attachVertex(const QString &name, int skelId, int v);
  void detachVertex(const QString &name, int skelId, int v);
  void rebindVertex(const QString &name, int skelId, const QString &newName);

  void touchParams(SkVD &vd);

  //! Applies stored vertex deformations to the skeleton branch starting at v
  void updateBranchPositions(const PlasticSkeleton &originalSkeleton,
                             PlasticSkeleton &deformedSkeleton, double frame,
                             int v);

  void onChange(
      const TParamChange &change) override;  // Passes param notifications to
                                             // external observers

private:
  // Not directly copy-constructible
  Imp(const Imp &other);
};

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::Imp(PlasticSkeletonDeformation *back)
    : m_back(back), m_skelIdsParam(1.0), m_grammar() {
  m_skelIdsParam->setName("Skeleton Id");
  m_skelIdsParam->addObserver(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::~Imp() {
  m_skelIdsParam->removeObserver(this);

  SkVDSet::iterator dt, dEnd(m_vds.end());
  for (dt = m_vds.begin(); dt != dEnd; ++dt)
    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
      dt->m_vd.m_params[p]->removeObserver(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp::Imp(PlasticSkeletonDeformation *back,
                                     const Imp &other)
    : m_back(back), m_skelIdsParam(other.m_skelIdsParam->clone()), m_grammar() {
  m_skelIdsParam->setGrammar(m_grammar);
  m_skelIdsParam->addObserver(this);

  // Clone the skeletons
  SkeletonSet::const_iterator st, sEnd(other.m_skeletons.end());
  for (st = other.m_skeletons.begin(); st != sEnd; ++st)
    m_skeletons.insert(SkeletonSet::value_type(
        st->get_left(), new PlasticSkeleton(*st->get_right())));

  // Clone each parameters curve
  SkVD vd;

  SkVDSet::const_iterator dt, dEnd(other.m_vds.end());
  for (dt = other.m_vds.begin(); dt != dEnd; ++dt) {
    VDKey vdKey = {dt->m_name, dt->m_hookNumber, dt->m_vIndices};

    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
      TDoubleParamP &param = vdKey.m_vd.m_params[p];

      param = TDoubleParamP(dt->m_vd.m_params[p]->clone());

      param->setGrammar(m_grammar);
      param->addObserver(this);
    }

    m_vds.insert(vdKey);
  }
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::Imp &PlasticSkeletonDeformation::Imp::operator=(
    const Imp &other) {
  *m_skelIdsParam = *other.m_skelIdsParam;
  m_skelIdsParam->setGrammar(m_grammar);

  // Take in all curves whose name matches one of the stored ones

  // Traverse known curves
  SkVDSet::iterator dt, dEnd(m_vds.end());
  SkVDSet::const_iterator st, sEnd(other.m_vds.end());

  for (dt = m_vds.begin(); dt != dEnd; ++dt) {
    // Search a corresponding curve in the input ones
    st = other.m_vds.find(dt->m_name);
    if (st != sEnd)
      for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
        TDoubleParam &param = *dt->m_vd.m_params[p];

        param = *st->m_vd.m_params[p];
        param.setGrammar(m_grammar);
      }
  }

  return *this;
}

//------------------------------------------------------------------

PlasticSkeleton &PlasticSkeletonDeformation::Imp::skeleton(int skelId) const {
  SkeletonSet::left_map::const_iterator st(m_skeletons.left.find(skelId));
  assert(st != m_skeletons.left.end());

  return *st->second;
}

//------------------------------------------------------------------

SkVD &PlasticSkeletonDeformation::Imp::vertexDeformation(
    const QString &name) const {
  SkVDSet::const_iterator vdt(m_vds.find(name));
  assert(vdt != m_vds.end());

  return vdt->m_vd;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::attach(int skeletonId,
                                             PlasticSkeleton *skeleton) {
  assert(skeleton);

  // Store the skeleton - acquires shared ownership
  assert(m_skeletons.left.find(skeletonId) == m_skeletons.left.end());

  m_skeletons.insert(SkeletonSet::value_type(skeletonId, skeleton));

  // Deal with vertex deformations and parameter defaults
  tcg::list<PlasticSkeleton::vertex_type> &vertices = skeleton->vertices();
  if (!vertices.empty()) {
    tcg::list<PlasticSkeletonVertex>::iterator vt, vEnd(vertices.end());
    for (vt = vertices.begin(); vt != vEnd; ++vt)
      attachVertex(vt->name(), skeletonId, vt.index());
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::detach(int skeletonId) {
  // First, detach all vertices
  tcg::list<PlasticSkeleton::vertex_type> &vertices =
      skeleton(skeletonId).vertices();
  if (!vertices.empty()) {
    tcg::list<PlasticSkeletonVertex>::iterator vt, vEnd(vertices.end());
    for (vt = vertices.begin(); vt != vEnd; ++vt)
      detachVertex(vt->name(), skeletonId, vt.index());
  }

  // Then, release the skeleton itself
  m_skeletons.left.erase(skeletonId);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::attachVertex(const QString &name,
                                                   int skelId, int v) {
  struct locals {
    static int newHookNumber(Imp *imp) {
      int h = 1;

      SkVDByHookNumber::iterator vdt, vdEnd(imp->m_vds.get<int>().end());
      for (vdt = imp->m_vds.get<int>().begin();
           vdt != vdEnd && vdt->m_hookNumber == h; ++vdt, ++h)
        ;

      return h;
    }

  };  // locals

  // Insert a new VD if necessary
  SkVDSet::iterator vdt(m_vds.find(name));
  if (vdt == m_vds.end()) {
    VDKey vdKey = {name, locals::newHookNumber(this)};
    touchParams(vdKey.m_vd);

    vdt = m_vds.insert(vdKey).first;
  }

  // Register (skelId, v) on the vd
  vdt->m_vIndices.insert(std::make_pair(skelId, v));
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::detachVertex(const QString &name,
                                                   int skelId, int v) {
  SkVDSet::iterator vdt = m_vds.find(name);
  assert(vdt != m_vds.end());

  // Unregister skelId
  int count = vdt->m_vIndices.erase(skelId);
  assert(count > 0);

  if (vdt->m_vIndices.empty()) {
    // De-register as vdt's observer, and release it from stored vds
    SkVD &vd = vdt->m_vd;

    for (int p = 0; p < SkVD::PARAMS_COUNT; ++p)
      vd.m_params[p]->removeObserver(this);

    m_vds.erase(vdt);
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::rebindVertex(const QString &name,
                                                   int skelId,
                                                   const QString &newName) {
  if (name == newName) return;

  SkVDSet::iterator oldVdt = m_vds.find(name);
  if (oldVdt == m_vds.end()) return;  // We get here when creating a new vertex

  std::map<int, int>::iterator vit = oldVdt->m_vIndices.find(skelId);
  assert(vit != oldVdt->m_vIndices.end());

  int v = vit->second;

  SkVDSet::iterator newVdt = m_vds.find(newName);
  if (newVdt != m_vds.end()) {
    detachVertex(name, skelId, v);
    attachVertex(newName, skelId, v);
  } else {
    // Creating a new vd entry
    if (oldVdt->m_vIndices.size() == 1) {
      // The old entry should be removed - it is actually *purely* renamed
      VDKey vdKey(*oldVdt);
      vdKey.m_name = newName;

      m_vds.erase(name);
      m_vds.insert(vdKey);
    } else {
      // The old entry remains - and data must be copied from there
      detachVertex(name, skelId, v);
      attachVertex(newName, skelId, v);

      newVdt = m_vds.find(newName);  // Fetch the newly created vd

      // Copy the existing vd into the new one
      SkVD &oldVd = oldVdt->m_vd, &newVd = newVdt->m_vd;

      int p, pCount = SkVD::PARAMS_COUNT;
      for (p = 0; p != pCount; ++p) *newVd.m_params[p] = *oldVd.m_params[p];
    }
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::touchParams(SkVD &vd) {
  for (int p = 0; p < SkVD::PARAMS_COUNT; ++p) {
    if (vd.m_params[p]) continue;

    TDoubleParam *param = new TDoubleParam;
    param->setName(parNames[p]);
    param->setMeasureName(parMeasures[p]);
    param->setGrammar(m_grammar);
    // Scale factors are neutral at 1.0 (100%), not 0
    if (p == SkVD::SCALEX || p == SkVD::SCALEY) param->setDefaultValue(1.0);

    vd.m_params[p] = param;

    param->addObserver(this);
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::onChange(const TParamChange &change) {
  // A character-level solve result describes the pose as it was BEFORE this
  // change, so it is now wrong and must not be served again. Without this the
  // cached answer outlived its inputs: dragging a limb wrote its ANGLEs and then
  // read back the stale solved skeleton, and the limb simply would not move.
  // Note the cache cannot be validated against the params (it is a whole
  // character's worth of them, on several columns) — dropping it on any change
  // and letting the next placement re-solve is both cheaper and safer.
  m_hasSolved    = false;
  m_solvedFrame  = -1.0e30;
  m_solvedSkelId = -1;

  // Since the deformation was changed, any associated deformer
  // must be invalidated (at the animation-deform level only)
  PlasticDeformerStorage::instance()->invalidateDeformation(
      m_back, PlasticDeformerStorage::NONE);

  // Propagate notification to this object's observers
  std::set<TParamObserver *>::iterator ot, oEnd(m_observers.end());
  for (ot = m_observers.begin(); ot != oEnd; ++ot) (*ot)->onChange(change);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::updateBranchPositions(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v) {
  struct locals {
    static void buildParentDirection(const PlasticSkeleton &skel, int v,
                                     TPointD &dir) {
      assert(v >= 0);

      const PlasticSkeletonVertex &vx = skel.vertex(v);

      int vParent = vx.parent();
      if (vParent < 0) return;  // dir remains as passed

      const TPointD &dir_ =
          tcg::point_ops::direction(skel.vertex(vParent).P(), vx.P(), 1e-4);

      if (dir_ != tcg::point_ops::NaP<TPointD>())
        dir = dir_;
      else
        buildParentDirection(skel, vParent, dir);
    }
  };  // locals

  PlasticSkeletonVertex &dvx = deformedSkeleton.vertex(v);
  int vParent                = dvx.parent();
  if (vParent >= 0) {
    // Rebuild vertex position
    const TPointD &ovxPos       = originalSkeleton.vertex(v).P();
    const TPointD &ovxParentPos = originalSkeleton.vertex(vParent).P();

    // Start by getting the polar reference
    TPointD oDir(1.0, 0.0), dDir(1.0, 0.0);

    locals::buildParentDirection(originalSkeleton, vParent, oDir);
    locals::buildParentDirection(deformedSkeleton, vParent, dDir);

    // Now, rebuild vx's position
    const SkVD &vd = m_vds.find(dvx.name())->m_vd;

    double a = tcg::point_ops::angle(oDir, ovxPos - ovxParentPos) * M_180_PI;
    double d = tcg::point_ops::dist(ovxParentPos, ovxPos);

    // ZtoRig: nothing to add here. A pose action is STAMPED into these very
    // params by applyPoseStrength, so by the time evaluation runs the pose is
    // already part of the authored animation.
    double aDelta = vd.m_params[SkVD::ANGLE]->getValue(frame);
    double dDelta = vd.m_params[SkVD::DISTANCE]->getValue(frame);

    dvx.P() = deformedSkeleton.vertex(vParent).P() +
              (d + dDelta) * (TRotation(a + aDelta) * dDir);
  } else {
    // Root: no ANGLE/DISTANCE of its own, but ROOTX/ROOTY (written by the
    // tool's pin-aware write-back when a re-root/pivot solve sweeps the root
    // along as a rigid passenger — see writeBackAnglesFor_animate) gives it a
    // keyframeable offset from rest. Applied here, before children recurse,
    // so it propagates like any other joint's motion; the per-frame pin
    // re-plant in storeDeformedSkeleton still runs on top of this, so an
    // active pin keeps holding exactly regardless of this offset.
    auto vdIt = m_vds.find(dvx.name());
    if (vdIt != m_vds.end()) {
      const SkVD &vd = vdIt->m_vd;
      if (vd.m_params[SkVD::ROOTX] && vd.m_params[SkVD::ROOTY])
        dvx.P() = originalSkeleton.vertex(v).P() +
                  TPointD(vd.m_params[SkVD::ROOTX]->getValue(frame),
                          vd.m_params[SkVD::ROOTY]->getValue(frame));
    }
  }

  // Finally, update children positions
  PlasticSkeleton::vertex_type::edges_iterator et, eEnd(dvx.edgesEnd());
  for (et = dvx.edgesBegin(); et != eEnd; ++et) {
    int vChild = deformedSkeleton.edge(*et).vertex(1);
    if (vChild == v) continue;

    updateBranchPositions(originalSkeleton, deformedSkeleton, frame, vChild);
  }
}

//**************************************************************************************
//    PlasticSkeletonDeformation  implementation
//**************************************************************************************

PlasticSkeletonDeformation::PlasticSkeletonDeformation()
    : m_imp(new Imp(this)) {}

//------------------------------------------------------------------

PlasticSkeletonDeformation::PlasticSkeletonDeformation(
    const PlasticSkeletonDeformation &other)
    : TSmartObject(m_classCode), m_imp(new Imp(this, *other.m_imp)) {
  // Register deformation
  SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
  for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st)
    st->get_right()->addListener(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::~PlasticSkeletonDeformation() {
  // Unregister deformation
  SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
  for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st)
    st->get_right()->removeListener(this);
}

//------------------------------------------------------------------

PlasticSkeletonDeformation &PlasticSkeletonDeformation::operator=(
    const PlasticSkeletonDeformation &other) {
  // The meaning of operator= is DIFFERENT from that implemented in the copy
  // constructor.
  // Skeletons are NOT cloned.

  *m_imp = *other.m_imp;
  return *this;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::empty() const {
  return m_imp->m_skeletons.empty();
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonsCount() const {
  return m_imp->m_skeletons.size();
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::skeletonIds(skelId_iterator &begin,
                                             skelId_iterator &end) const {
  auto const f = [](const SkeletonSet::left_map::value_type &val) {
    return val.first;
  };

  begin = boost::make_transform_iterator(m_imp->m_skeletons.left.begin(), f);
  end   = boost::make_transform_iterator(m_imp->m_skeletons.left.end(), f);
}

//------------------------------------------------------------------

TDoubleParamP PlasticSkeletonDeformation::skeletonIdsParam() const {
  return m_imp->m_skelIdsParam;
}

//------------------------------------------------------------------

PlasticSkeletonP PlasticSkeletonDeformation::skeleton(double frame) const {
  return skeleton(skeletonId(frame));
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonId(double frame) const {
  return m_imp->m_skelIdsParam->getValue(frame);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::attach(int skeletonId,
                                        PlasticSkeleton *skeleton) {
  m_imp->attach(skeletonId, skeleton);

  skeleton->addListener(this);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::detach(int skeletonId) {
  SkeletonSet::left_map::iterator st(m_imp->m_skeletons.left.find(skeletonId));
  if (st != m_imp->m_skeletons.left.end()) {
    st->second->removeListener(this);
    m_imp->detach(skeletonId);
  }
}

//------------------------------------------------------------------

PlasticSkeletonP PlasticSkeletonDeformation::skeleton(int skeletonId) const {
  SkeletonSet::left_map::const_iterator st =
      m_imp->m_skeletons.left.find(skeletonId);
  return (st == m_imp->m_skeletons.left.end()) ? PlasticSkeletonP()
                                               : st->second;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::skeletonId(PlasticSkeleton *skeleton) const {
  SkeletonSet::right_map::const_iterator st =
      m_imp->m_skeletons.right.find(skeleton);
  return (st == m_imp->m_skeletons.right.end())
             ? -(std::numeric_limits<int>::max)()
             : st->second;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::vertexDeformationsCount() const {
  return m_imp->m_vds.size();
}

//------------------------------------------------------------------

SkVD *PlasticSkeletonDeformation::vertexDeformation(
    const QString &vxName) const {
  SkVDSet::const_iterator vdt = m_imp->m_vds.find(vxName);
  return (vdt == m_imp->m_vds.end()) ? (SkVD *)0 : &vdt->m_vd;
}

//------------------------------------------------------------------

SkVD *PlasticSkeletonDeformation::vertexDeformation(int skelId, int v) const {
  const QString &name = skeleton(skelId)->vertex(v).name();
  return vertexDeformation(name);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vertexDeformations(vd_iterator &begin,
                                                    vd_iterator &end) const {
  auto const f = [](const VDKey &vdKey) {
    return std::make_pair(&vdKey.m_name, &vdKey.m_vd);
  };

  begin = boost::make_transform_iterator(m_imp->m_vds.begin(), f);
  end   = boost::make_transform_iterator(m_imp->m_vds.end(), f);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vdSkeletonVertices(const QString &vertexName,
                                                    vx_iterator &begin,
                                                    vx_iterator &end) const {
  auto const f = [](const std::map<int, int>::value_type &val) {
    return std::make_pair(val.first, val.second);
  };

  SkVDSet::const_iterator nt(m_imp->m_vds.find(vertexName));

  if (nt == m_imp->m_vds.end()) {
    begin = vx_iterator();
    end   = vx_iterator();
  } else {
    begin = boost::make_transform_iterator(nt->m_vIndices.begin(), f);
    end   = boost::make_transform_iterator(nt->m_vIndices.end(), f);
  }
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::hookNumber(const QString &name) const {
  SkVDSet::const_iterator nt(m_imp->m_vds.find(name));
  return (nt == m_imp->m_vds.end()) ? -1 : nt->m_hookNumber;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::hookNumber(int skelId, int v) const {
  const QString &name = skeleton(skelId)->vertex(v).name();
  return hookNumber(name);
}

//------------------------------------------------------------------

QString PlasticSkeletonDeformation::vertexName(int hookNumber) const {
  const SkVDByHookNumber &vds = m_imp->m_vds.get<int>();

  SkVDByHookNumber::const_iterator ht(vds.find(hookNumber));
  return (ht == vds.end()) ? QString() : ht->m_name;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::vertexIndex(int hookNumber, int skelId) const {
  const SkVDByHookNumber &vds = m_imp->m_vds.get<int>();

  SkVDByHookNumber::const_iterator ht(vds.find(hookNumber));
  if (ht == vds.end()) return -1;

  std::map<int, int>::const_iterator st(ht->m_vIndices.find(skelId));
  return (st == ht->m_vIndices.end()) ? -1 : st->second;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::getKeyframeAt(double frame,
                                               SkDKey &keyframe) const {
  keyframe.m_skelIdKeyframe = m_imp->m_skelIdsParam->getKeyframeAt(frame);
  keyframe.m_vertexKeyframes.clear();

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    keyframe.m_vertexKeyframes.insert(
        std::make_pair(dt->m_name, dt->m_vd.getKeyframe(frame)));
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setKeyframe(double frame) {
  m_imp->m_skelIdsParam->setKeyframe(frame);

  SkVDSet::iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt) dt->m_vd.setKeyframe(frame);
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::setKeyframe(const SkDKey &keyframe) {
  bool keyWasSet = false;

  if (keyframe.m_skelIdKeyframe.m_isKeyframe) {
    m_imp->m_skelIdsParam->setKeyframe(keyframe.m_skelIdKeyframe);
    keyWasSet = true;
  }

  const std::map<QString, SkVD::Keyframe> &vdKeys = keyframe.m_vertexKeyframes;

  // Iterate the keyframe's vertex deformations
  std::map<QString, SkVD::Keyframe>::const_iterator kt, kEnd(vdKeys.end());
  for (kt = vdKeys.begin(); kt != vdKeys.end(); ++kt) {
    // Search for a corresponding vertex deformation among stored ones
    SkVDSet::iterator vdt = m_imp->m_vds.find(kt->first);
    if (vdt != m_imp->m_vds.end()) {
      // Set the corresponding keyframe
      keyWasSet = vdt->m_vd.setKeyframe(kt->second) || keyWasSet;
    }
  }

  return keyWasSet;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::setKeyframe(const SkDKey &keyframe,
                                             double frame, double easeIn,
                                             double easeOut) {
  bool keyWasSet = false;

  if (keyframe.m_skelIdKeyframe.m_isKeyframe) {
    TDoubleKeyframe kf(keyframe.m_skelIdKeyframe);
    kf.m_frame = frame;

    m_imp->m_skelIdsParam->setKeyframe(kf);
    keyWasSet = true;
  }

  const std::map<QString, SkVD::Keyframe> &vdKeys = keyframe.m_vertexKeyframes;

  // Iterate the keyframe's vertex deformations
  std::map<QString, SkVD::Keyframe>::const_iterator kt, kEnd(vdKeys.end());
  for (kt = vdKeys.begin(); kt != vdKeys.end(); ++kt) {
    // Search for a corresponding vertex deformation among stored ones
    SkVDSet::iterator vdt = m_imp->m_vds.find(kt->first);
    if (vdt != m_imp->m_vds.end()) {
      // Set the corresponding keyframe
      keyWasSet = vdt->m_vd.setKeyframe(kt->second, frame, easeIn, easeOut) ||
                  keyWasSet;
    }
  }

  return keyWasSet;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::isKeyframe(double frame) const {
  if (m_imp->m_skelIdsParam->isKeyframe(frame)) return true;

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    if (dt->m_vd.isKeyframe(frame)) return true;

  return false;
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::isFullKeyframe(double frame) const {
  if (!m_imp->m_skelIdsParam->isKeyframe(frame)) return false;

  SkVDSet::const_iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    if (!dt->m_vd.isFullKeyframe(frame)) return false;

  return true;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::deleteKeyframe(double frame) {
  m_imp->m_skelIdsParam->deleteKeyframe(frame);

  SkVDSet::iterator dt, dEnd(m_imp->m_vds.end());
  for (dt = m_imp->m_vds.begin(); dt != dEnd; ++dt)
    dt->m_vd.deleteKeyframe(frame);
}

//------------------------------------------------------------------

//**************************************************************************************
//    PlasticPinSolver  implementation
//**************************************************************************************

namespace {

// Root-first chain of joint indices from the root down to j.
std::vector<int> solverPathFromRoot(
    const std::vector<PlasticPinSolver::Joint> &joints, int j) {
  std::vector<int> path;
  for (int p = j; p >= 0; p = joints[p].parent) path.push_back(p);
  std::reverse(path.begin(), path.end());
  return path;
}

// j's subtree, j included.
void solverCollectSubtree(const std::vector<std::vector<int>> &children, int j,
                          std::vector<int> &out) {
  out.push_back(j);
  for (int c : children[j]) solverCollectSubtree(children, c, out);
}

// Direction (radians) of the bone arriving at j from its parent, walking up
// degenerate bones; horizontal axis at the root.
double solverBoneDir(const std::vector<PlasticPinSolver::Joint> &joints,
                     const std::vector<TPointD> &pos, int j) {
  int q = j, p = joints[j].parent;
  for (; p >= 0; q = p, p = joints[p].parent) {
    TPointD d = pos[q] - pos[p];
    if (norm2(d) > 1e-8) return atan2(d.y, d.x);
  }
  return 0.0;
}

// Current ANGLE-equivalent delta (degrees) of joint j, measured geometrically
// against the rest pose — the quantity the min/max limits constrain.
double solverRelAngleDeg(const std::vector<PlasticPinSolver::Joint> &joints,
                         const std::vector<TPointD> &pos,
                         const std::vector<TPointD> &rest, int j) {
  const int jp = joints[j].parent;
  if (jp < 0) return 0.0;
  double now  = solverBoneDir(joints, pos, j) - solverBoneDir(joints, pos, jp);
  double base = solverBoneDir(joints, rest, j) -
                solverBoneDir(joints, rest, jp);
  double rel = now - base;
  while (rel > M_PI) rel -= 2.0 * M_PI;
  while (rel < -M_PI) rel += 2.0 * M_PI;
  return rel * M_180_PI;
}

}  // namespace

//-----------------------------------------------------------------------------

// DIAGNOSTIC (2026-07-20) — see the header for the hypothesis being tested.
// Atomic because plant() runs during evaluation, which is not necessarily the
// thread that flips the flag (the tool does, on button down/up).
namespace {
std::atomic<bool> l_solveSuspended{false};

// Opt-in, so the A/B is two launches of the same binary rather than two builds.
bool solveSuspendEnabled() {
  static const bool enabled = ::getenv("ZTORYC_SUSPEND_PLANT") != nullptr;
  return enabled;
}

// DIAGNOSTIC (2026-07-26, opt-in via ZTORYC_PIN_DIAG) — why do pins still shift
// slightly? Two candidates with opposite fixes: a pin genuinely out of reach, so
// the balancing loop spreads the error across every pin BY DESIGN; or the CCD
// simply not converging in the sweeps it is given. This reports which.
bool pinDiagEnabled() {
  static const bool enabled = ::getenv("ZTORYC_PIN_DIAG") != nullptr;
  return enabled;
}

// The drag's feasibility probe runs this same planter several times per mouse
// move, on candidate poses most of which are MEANT to fail. Logging those
// drowned the real evaluation in rejected hypotheses and made the numbers
// uncomparable with earlier runs. Only the real one reports.
thread_local bool l_pinDiagMuted = false;

struct PinDiagMute {
  bool m_prev;
  PinDiagMute() : m_prev(l_pinDiagMuted) { l_pinDiagMuted = true; }
  ~PinDiagMute() { l_pinDiagMuted = m_prev; }
};
}  // namespace

void PlasticPinSolver::setSolveSuspended(bool on) {
  if (!solveSuspendEnabled()) return;
  l_solveSuspended.store(on, std::memory_order_relaxed);
}

bool PlasticPinSolver::isSolveSuspended() {
  return l_solveSuspended.load(std::memory_order_relaxed);
}

//-----------------------------------------------------------------------------

void PlasticPinSolver::plant(const std::vector<Joint> &joints,
                             const std::vector<Pin> &pinsIn,
                             std::vector<TPointD> &pos,
                             const std::vector<int> &preplanted,
                             double maxStepDegrees) {
  if (joints.empty() || pos.size() != joints.size() || pinsIn.empty()) return;

  const double maxStepRad =
      (maxStepDegrees > 0.0) ? maxStepDegrees * (M_PI / 180.0) : 0.0;

  const int n = (int)joints.size();

  std::vector<std::vector<int>> children(n);
  for (int j = 0; j < n; ++j)
    if (joints[j].parent >= 0) children[joints[j].parent].push_back(j);

  std::vector<TPointD> rest(n);
  for (int j = 0; j < n; ++j) rest[j] = joints[j].rest;

  std::vector<Pin> pins = pinsIn;
  std::stable_sort(pins.begin(), pins.end(),
                   [](const Pin &a, const Pin &b) { return a.since < b.since; });

  const bool externallyOwned = !preplanted.empty();

  if (!externallyOwned) {
    // Primary (oldest) pin: rigid whole-structure translation.
    TPointD shift = pins[0].target - pos[pins[0].joint];
    if (norm2(shift) > 1e-12)
      for (int j = 0; j < n; ++j) pos[j] += shift;

    if (pins.size() == 1) return;
  }

  // DIAGNOSTIC (2026-07-20) — return HERE, not at the top: the primary pin's
  // rigid translation above is the anchoring the tool deliberately delegates to
  // us (see writeBackAngles_animate: the drag writes ANGLEs only and leaves the
  // free root to this plant). Suspending that too makes the character drift.
  //
  // What we drop is everything below — the CCD over secondary pins and the
  // balancing loop that re-runs it. That is the half which duplicates FABRIK's
  // job and fights it. Note the single-pin early return just above: with one pin
  // the CCD never runs at all, which is exactly why a lone pin has always felt
  // nailed while two pins do not.
  if (l_solveSuspended.load(std::memory_order_relaxed)) return;

  // Plant every secondary pin: CCD on its own limb, below the point where it
  // diverges from the already-planted chains. Re-runnable — the planted chains
  // are the fixed seed each pass (used by the balancing below).
  auto plantSecondaries = [&]() {
    std::set<int> planted;
    if (externallyOwned) {
      for (int sj : preplanted) {
        std::vector<int> ps = solverPathFromRoot(joints, sj);
        planted.insert(ps.begin(), ps.end());
      }
    } else {
      std::vector<int> p0 = solverPathFromRoot(joints, pins[0].joint);
      planted.insert(p0.begin(), p0.end());
    }

    for (size_t i = externallyOwned ? 0 : 1; i < pins.size(); ++i) {
      const int pinJ        = pins[i].joint;
      const TPointD &target = pins[i].target;
      std::vector<int> path = solverPathFromRoot(joints, pinJ);

      // Anchor = deepest joint of this chain already planted (at worst the
      // root). CCD pivots live strictly below it: rotating their subtrees can
      // never move a previously planted pin (disjoint by construction).
      int aIdx = 0;
      for (int k = (int)path.size() - 1; k >= 0; --k)
        if (planted.count(path[k])) {
          aIdx = k;
          break;
        }

      const int SWEEPS  = 24;
      const double tol2 = 1e-9;

      auto sweepToTarget = [&](bool respectLimits) {
        for (int sweep = 0; sweep < SWEEPS; ++sweep) {
          if (norm2(target - pos[pinJ]) < tol2) break;
          // Nearest-to-pin pivot first: classic CCD sweep order. The anchor
          // itself (k == aIdx) is a valid LAST pivot: rotating only path[k+1]'s
          // subtree about it bends this limb's attachment bone too — without it,
          // a pose that puts the divergence point out of the limb's reach would
          // tear the pin off with no joint able to compensate.
          for (int k = (int)path.size() - 2; k >= aIdx; --k) {
            const TPointD pivot = pos[path[k]];
            TPointD cur         = pos[pinJ] - pivot;
            TPointD tgt         = target - pivot;
            if (norm2(cur) < 1e-8 || norm2(tgt) < 1e-8) continue;
            double ang = atan2(cross(cur, tgt), cur * tgt);

            // This rotation changes exactly the relative angle of joint
            // path[k+1], so clamp it within that joint's limits: a stiff limb
            // bends within its bound rather than hyper-extending.
            const Joint &jvx = joints[path[k + 1]];
            if (respectLimits && (jvx.minAngle > -1e9 || jvx.maxAngle < 1e9)) {
              double curRel = solverRelAngleDeg(joints, pos, rest, path[k + 1]);
              double lo     = (jvx.minAngle - curRel) * (M_PI / 180.0);
              double hi     = (jvx.maxAngle - curRel) * (M_PI / 180.0);
              ang           = std::min(std::max(ang, lo), hi);
            }

            // Damped CCD (see the header): spread the bend along the chain
            // instead of letting the nearest pivot whip toward the target.
            if (maxStepRad > 0.0)
              ang = std::min(std::max(ang, -maxStepRad), maxStepRad);

            double c = cos(ang), s = sin(ang);
            std::vector<int> sub;
            solverCollectSubtree(children, path[k + 1], sub);
            for (int v : sub) {
              TPointD d = pos[v] - pivot;
              pos[v] = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
            }
          }
        }
      };

      // Reach the pin within the joint limits if possible, and let the limits
      // yield only when the pin would otherwise be left behind. MEASURED
      // (2026-07-26): hard limits here put the evaluation into its balancing
      // loop on 81% of frames with a worst miss of 7% of the rig, against 47%
      // and 2.3% when they yield — a pin that cannot be reached is a pin whose
      // error gets spread over ALL of them, primary included.
      //
      // Not the last word: writeBackAngles_animate still clamps hard when it
      // stores the pose, so the two disagree. Making THAT yield as well is the
      // open question, and it needs the drag path actually in use to be
      // identified first.
      sweepToTarget(true);
      if (norm2(target - pos[pinJ]) >= tol2) sweepToTarget(false);

      planted.insert(path.begin(), path.end());
    }
  };

  plantSecondaries();

  // Under external ownership the balancing below would translate the structure
  // and thus fight whoever owns that translation.
  if (externallyOwned) return;

  // Reachability threshold RELATIVE to the rig scale (bbox diagonal): a fixed
  // epsilon fired spuriously on some rigs, nudging the exact primary pin and
  // leaving small shifts. Below this, all pins count as planted.
  double diag2 = 0.0;
  {
    TPointD lo = pos[0], hi = lo;
    for (int j = 0; j < n; ++j) {
      lo.x = std::min(lo.x, pos[j].x), lo.y = std::min(lo.y, pos[j].y);
      hi.x = std::max(hi.x, pos[j].x), hi.y = std::max(hi.y, pos[j].y);
    }
    diag2 = norm2(hi - lo);
  }
  const double reachTol2 = std::max(1e-8, diag2 * 1e-6);  // (~0.1% of diag)²

  // Hard constraint: a pin dragged past the reach of its limb must NOT lift off.
  // Pull the whole structure toward the average pin residual and re-plant, a few
  // passes: the body settles at the feasible middle where every pin stays
  // ~planted and the motion is naturally limited (the support resists), instead
  // of one of them detaching. Damped, so it converges without over-shooting the
  // primary. No-op when all pins reach → the exact primary planting survives.
  int balancePasses = 0;
  for (int pass = 0; pass < 10; ++pass) {
    TPointD resid(0.0, 0.0);
    double maxr2 = 0.0;
    for (const Pin &pin : pins) {
      TPointD r = pin.target - pos[pin.joint];
      resid     = resid + r;
      maxr2     = std::max(maxr2, norm2(r));
    }
    if (maxr2 < reachTol2) break;
    resid = resid * (0.5 / (double)pins.size());
    if (norm2(resid) < reachTol2 * 0.01) break;
    for (int j = 0; j < n; ++j) pos[j] += resid;
    plantSecondaries();
    ++balancePasses;
  }

  if (pinDiagEnabled() && !l_pinDiagMuted) {
    // Per-pin residual as a FRACTION of the rig diagonal, so the numbers mean
    // the same thing on any rig. balancePasses > 0 means the balancing loop
    // ran, i.e. at least one pin could not be reached and the error was shared.
    const double diag = sqrt(std::max(diag2, 1e-12));
    QString msg = QString("[PIN_DIAG] pins=%1 balancePasses=%2 residuals(%% diag):")
                      .arg(pins.size())
                      .arg(balancePasses);
    for (size_t i = 0; i < pins.size(); ++i) {
      const double r = norm(pins[i].target - pos[pins[i].joint]);
      msg += QString(" p%1(since=%2)=%3")
                 .arg(i)
                 .arg(pins[i].since, 0, 'f', 0)
                 .arg(100.0 * r / diag, 0, 'f', 3);
    }
    qDebug().noquote() << msg;
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::storePosedSkeleton(
    int skelId, double frame, PlasticSkeleton &skeleton) const {
  const PlasticSkeletonP &origSkel = this->skeleton(skelId);
  skeleton                         = origSkel ? *origSkel : PlasticSkeleton();

  if (!skeleton.vertices().empty())
    m_imp->updateBranchPositions(*origSkel, skeleton, frame,
                                 skeleton.vertices().begin().m_idx);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::buildSolverJoints(
    int skelId, double frame, std::vector<PlasticPinSolver::Joint> &joints,
    std::vector<int> &vertexIds) const {
  joints.clear();
  vertexIds.clear();

  const PlasticSkeletonP &origSkel = this->skeleton(skelId);
  if (!origSkel) return;

  const tcg::list<PlasticSkeleton::vertex_type> &verts = origSkel->vertices();
  if (verts.empty()) return;

  size_t maxIdx = 0;
  for (auto st = verts.begin(); st != verts.end(); ++st)
    maxIdx = std::max(maxIdx, st.m_idx);

  std::vector<int> denseOf(maxIdx + 1, -1);
  for (auto st = verts.begin(); st != verts.end(); ++st) {
    denseOf[st.m_idx] = (int)vertexIds.size();
    vertexIds.push_back((int)st.m_idx);
  }

  joints.resize(vertexIds.size());
  for (size_t j = 0; j < vertexIds.size(); ++j) {
    const int vi                    = vertexIds[j];
    const PlasticSkeletonVertex &vx = origSkel->vertex(vi);
    const int par                   = vx.parent();
    joints[j].parent                = (par >= 0) ? denseOf[par] : -1;
    joints[j].rest                  = vx.P();

    const SkVD *jvd = 0;
    {
      auto jt = m_imp->m_vds.find(vx.name());
      if (jt != m_imp->m_vds.end()) jvd = &jt->m_vd;
    }
    effAngleLimits(jvd, vx, frame, joints[j].minAngle, joints[j].maxAngle);
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::storeDeformedSkeleton(
    int skelId, double frame, PlasticSkeleton &skeleton) const {
  // A character-level solve has already planted this column as part of one
  // unified joint tree: that answer wins outright. Planting again here is what
  // used to give a stitched rig two solvers pulling in different directions.
  if (m_imp->m_hasSolved && m_imp->m_solvedSkelId == skelId &&
      m_imp->m_solvedFrame == frame) {
    skeleton = m_imp->m_solvedSkeleton;
    return;
  }

  // Copy the un-deformed skeleton to the output one
  const PlasticSkeletonP &origSkel = this->skeleton(skelId);
  skeleton                         = origSkel ? *origSkel : PlasticSkeleton();

  // Update the skeleton to the specified frame
  if (!skeleton.vertices().empty())
    m_imp->updateBranchPositions(*origSkel, skeleton, frame,
                                 skeleton.vertices().begin().m_idx);

  // NOTE: the SuperPlastic squash & stretch (SCALEX/SCALEY) is NOT applied
  // here — it is a controller ON TOP of the skeleton (an affine composed into
  // the drawing/render transforms, see getSquashControllerAffine): keeping it
  // out of the skeleton evaluation means pins, IK and pose manipulation never
  // interact with the scale.

  // IK off means the pins are GONE, not merely hidden: no planting at all, the
  // character sits at the pose its params describe. The tool writes the planted
  // result back into those params, so it stays where it is — which is what
  // switching IK off on a single-level rig has always done.
  //
  // This used to run regardless of the flag, on the theory that skipping it
  // would shift the pose. It does not, and leaving it on had a worse effect on
  // a multi-column character: the stage-level solve and the per-column plant
  // disagreed about who owns the pinned chain, and the rig came apart until the
  // next click forced a re-solve.
  if (!m_imp->m_pinsEnabled) return;

  plantPins(skelId, frame, skeleton, nullptr);
}

//------------------------------------------------------------------

double PlasticSkeletonDeformation::pinResidualForPose(
    int skelId, double frame, const PlasticSkeleton &posed) const {
  // The question the DRAG needs answered: if the character were posed like
  // this, would the pins still hold once evaluation has had its say?
  //
  // It runs the very same plant() the evaluation runs, on the very same inputs,
  // because anything else re-opens the fault this rig keeps falling into — the
  // drag deciding with one solver while another one decides the result. The
  // caller throws the posed skeleton away and keeps only the number.
  PlasticSkeleton scratch = posed;
  double worst2           = 0.0;
  PinDiagMute mute;  // a probe, not a result: keep it out of the log
  plantPins(skelId, frame, scratch, &worst2);
  return sqrt(worst2);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::plantPins(int skelId, double frame,
                                           PlasticSkeleton &skeleton,
                                           double *worstResidual2) const {
  const PlasticSkeletonP &origSkel = this->skeleton(skelId);
  if (!origSkel) return;
  if (worstResidual2) *worstResidual2 = 0.0;

  // SuperPlastic pin constraints (per-frame). The OLDEST active pin is planted
  // by rigidly translating the whole skeleton onto its target (PINTX,PINTY):
  // the root stays free and the animator's pose is untouched. Any FURTHER pin
  // (double support in a walk) is planted by bending ONLY its own limb — CCD
  // confined strictly below the point where its chain diverges from the ones
  // already planted, so the first foot never moves. Enforced at EVERY frame,
  // not just on keyframes, so pins hold through the in-betweens.
  struct locals {
    // Root-first chain of vertex indices from the skeleton root down to v.
    static std::vector<int> pathFromRoot(const PlasticSkeleton &skel, int v) {
      std::vector<int> path;
      for (int p = v; p >= 0; p = skel.vertex(p).parent()) path.push_back(p);
      std::reverse(path.begin(), path.end());
      return path;
    }

    // v's subtree (v included), descending child edges only.
    static void collectSubtree(PlasticSkeleton &skel, int v,
                               std::vector<int> &out) {
      out.push_back(v);
      PlasticSkeletonVertex &vx = skel.vertex(v);
      PlasticSkeleton::vertex_type::edges_iterator et, eEnd(vx.edgesEnd());
      for (et = vx.edgesBegin(); et != eEnd; ++et) {
        int vChild = skel.edge(*et).vertex(1);
        if (vChild == v) continue;
        collectSubtree(skel, vChild, out);
      }
    }

    // Direction (radians) of the bone arriving at v from its parent, walking
    // up degenerate bones; horizontal axis at the root (like buildAngle).
    static double boneDir(const PlasticSkeleton &skel, int v) {
      int q = v, p = skel.vertex(v).parent();
      for (; p >= 0; q = p, p = skel.vertex(p).parent()) {
        TPointD d = skel.vertex(q).P() - skel.vertex(p).P();
        if (norm2(d) > 1e-8) return atan2(d.y, d.x);
      }
      return 0.0;
    }

    // Current ANGLE-equivalent delta (degrees) of joint v, measured
    // geometrically against the original rest pose — the same quantity the
    // min/maxAngle limits constrain in the FK paths.
    static double relAngleDeg(const PlasticSkeleton &def,
                              const PlasticSkeleton &orig, int v) {
      int vp      = def.vertex(v).parent();
      double now  = boneDir(def, v) - boneDir(def, vp);
      double rest = boneDir(orig, v) - boneDir(orig, vp);
      double rel  = now - rest;
      while (rel > M_PI) rel -= 2.0 * M_PI;
      while (rel < -M_PI) rel += 2.0 * M_PI;
      return rel * M_180_PI;
    }

    // First frame of the pin's current ON run (Constant keys): seniority
    // decides which pin owns the translation, so toggling a second pin on
    // never re-targets the first one (that would jump the whole pose).
    static double activationFrame(const TDoubleParam &pin, double frame) {
      double on = frame;
      for (int k = pin.getKeyframeCount() - 1; k >= 0; --k) {
        const TDoubleKeyframe &kf = pin.getKeyframe(k);
        if (kf.m_frame > frame) continue;
        if (kf.m_value < 0.5) break;
        on = kf.m_frame;
      }
      return on;
    }
  };  // locals

  struct ActivePin {
    int idx;
    TPointD target;
    double since;
  };
  std::vector<ActivePin> pins;

  // Pins whose planting authority is the STAGE level, not this skeleton. A pin
  // carrying a scene-space target (PINWX/PINWY keyed) belongs to a multi-column
  // character: TStageObject::computePlasticPinCorrection holds it by translating
  // the WHOLE character, so translating this skeleton too would be a second,
  // competing plant — the two would fight on every support switch of a walk
  // (root-column foot dragged along when a child-column foot takes over).
  // Rigid translation has exactly ONE owner; local CCD bending stays here.
  std::vector<int> stagePinned;
  // Any stage-owned pin at all, primary or secondary. The rigid translation must
  // be suppressed even on a column that holds ONLY secondaries (its primary
  // living on another column of the character): there stagePinned stays empty,
  // and without this flag the oldest local pin would grab the translation and
  // fight the character-level one all over again.
  bool anyStageOwned = false;

  const tcg::list<PlasticSkeleton::vertex_type> &verts = skeleton.vertices();
  for (auto vt = verts.begin(); vt != verts.end(); ++vt) {
    auto it = m_imp->m_vds.find(vt->name());
    if (it == m_imp->m_vds.end()) continue;
    const SkVD &vd = it->m_vd;
    if (!vd.m_params[SkVD::PIN] || !vd.m_params[SkVD::PINTX] ||
        !vd.m_params[SkVD::PINTY])
      continue;
    if (vd.m_params[SkVD::PIN]->getValue(frame) < 0.5) continue;

    // Stage-owned: its target lives in scene space and is meaningless here.
    if (vd.m_params[SkVD::PINWX] && vd.m_params[SkVD::PINWY] &&
        (vd.m_params[SkVD::PINWX]->getKeyframeCount() > 0 ||
         vd.m_params[SkVD::PINWY]->getKeyframeCount() > 0)) {
      anyStageOwned = true;
      // STEP C.2: unless the stage-level solve handed us a LOCAL target for it,
      // which means this is a SECONDARY pin — the character translation is
      // already spoken for by the primary, so this one plants by bending its
      // own limb toward the mapped target, exactly like a same-column secondary.
      auto st = m_imp->m_secondaryTargets.find((int)vt.m_idx);
      if (st != m_imp->m_secondaryTargets.end() &&
          m_imp->m_secondaryFrame == frame) {
        pins.push_back({(int)vt.m_idx, st->second,
                        locals::activationFrame(*vd.m_params[SkVD::PIN], frame)});
        continue;
      }
      // Primary (or no solve yet): held by the character translation. Recorded
      // only so the CCD below treats its chain as already planted.
      stagePinned.push_back((int)vt.m_idx);
      continue;
    }

    // Skip pins that never got a target (avoid snapping to the origin) —
    // also shields against stale PIN flags left over in older scenes.
    if (vd.m_params[SkVD::PINTX]->isDefault() &&
        vd.m_params[SkVD::PINTY]->isDefault())
      continue;
    TPointD target(vd.m_params[SkVD::PINTX]->getValue(frame),
                   vd.m_params[SkVD::PINTY]->getValue(frame));
    pins.push_back(
        {(int)vt.m_idx, target,
         locals::activationFrame(*vd.m_params[SkVD::PIN], frame)});
  }
  if (pins.empty()) return;

  // Delegate to the shared solver. Same algorithm as before — it was lifted out
  // of here verbatim — but now the multi-column character can run the identical
  // code on a unified joint tree instead of growing a second implementation.
  //
  // The skeleton's vertex ids are tcg::list slots and need not be contiguous, so
  // map them onto dense solver indices and back.
  {
    size_t maxIdx = 0;
    for (auto st = verts.begin(); st != verts.end(); ++st)
      maxIdx = std::max(maxIdx, st.m_idx);

    std::vector<int> denseOf(maxIdx + 1, -1);
    std::vector<int> idxOf;
    for (auto st = verts.begin(); st != verts.end(); ++st) {
      denseOf[st.m_idx] = (int)idxOf.size();
      idxOf.push_back((int)st.m_idx);
    }

    const int n = (int)idxOf.size();
    std::vector<PlasticPinSolver::Joint> joints(n);
    std::vector<TPointD> pos(n);
    for (int j = 0; j < n; ++j) {
      const int vi              = idxOf[j];
      const PlasticSkeletonVertex &vx = skeleton.vertex(vi);
      const int par             = vx.parent();
      joints[j].parent          = (par >= 0) ? denseOf[par] : -1;
      joints[j].rest            = origSkel->vertex(vi).P();
      pos[j]                    = vx.P();

      const SkVD *jvd = 0;
      {
        auto jt = m_imp->m_vds.find(origSkel->vertex(vi).name());
        if (jt != m_imp->m_vds.end()) jvd = &jt->m_vd;
      }
      effAngleLimits(jvd, origSkel->vertex(vi), frame, joints[j].minAngle,
                     joints[j].maxAngle);
    }

    std::vector<PlasticPinSolver::Pin> spins;
    for (const ActivePin &ap : pins)
      spins.push_back({denseOf[ap.idx], ap.target, ap.since});

    std::vector<int> preplanted;
    for (int sv : stagePinned) preplanted.push_back(denseOf[sv]);

    PlasticPinSolver::plant(joints, spins, pos, preplanted);

    if (worstResidual2)
      for (const PlasticPinSolver::Pin &sp : spins)
        *worstResidual2 =
            std::max(*worstResidual2, norm2(sp.target - pos[sp.joint]));

    for (int j = 0; j < n; ++j) skeleton.vertex(idxOf[j]).P() = pos[j];
  }
}

//------------------------------------------------------------------

TAffine PlasticSkeletonDeformation::getSquashControllerAffine(
    int skelId, double frame) const {
  const PlasticSkeletonP &skel = skeleton(skelId);
  if (!skel || skel->empty()) return TAffine();

  // The controller params live on the ROOT vertex's deformation
  const tcg::list<PlasticSkeleton::vertex_type> &vs = skel->vertices();
  int rootIdx = -1;
  for (auto vt = vs.begin(); vt != vs.end(); ++vt)
    if (vt->parent() < 0) {
      rootIdx = (int)vt.m_idx;
      break;
    }
  if (rootIdx < 0) return TAffine();

  auto it = m_imp->m_vds.find(skel->vertex(rootIdx).name());
  if (it == m_imp->m_vds.end()) return TAffine();
  const SkVD &vd = it->m_vd;

  // ZtoRig: the controller channels are pose params like any other, so an
  // action that squashes or rotates the whole character has already been
  // stamped into them — read the raw value.
  auto val = [&vd, frame](SkVD::Params p, double def) {
    return vd.m_params[p] ? vd.m_params[p]->getValue(frame) : def;
  };

  double sx  = val(SkVD::SCALEX, 1.0), sy = val(SkVD::SCALEY, 1.0);
  double rot = val(SkVD::ROT, 0.0);
  double shx = val(SkVD::SHEARX, 0.0), shy = val(SkVD::SHEARY, 0.0);
  double tx  = val(SkVD::TRANSX, 0.0), ty = val(SkVD::TRANSY, 0.0);

  if (fabs(sx - 1.0) <= 1e-9 && fabs(sy - 1.0) <= 1e-9 &&
      fabs(rot) <= 1e-9 && fabs(shx) <= 1e-9 && fabs(shy) <= 1e-9 &&
      fabs(tx) <= 1e-9 && fabs(ty) <= 1e-9)
    return TAffine();

  // Degenerate/negative scales would collapse or flip the mesh: clamp
  sx = std::max(sx, 0.01);
  sy = std::max(sy, 0.01);

  // Pivot = DEFORMED root position + keyframeable offset: it follows the
  // character (the stage-object center can't — the advancement lives in
  // mesh-local space via the eval-time pin shifts) and can be animated
  PlasticSkeleton deformed;
  storeDeformedSkeleton(skelId, frame, deformed);
  if (deformed.empty()) return TAffine();

  TPointD C = deformed.vertex(rootIdx).P();
  C.x += val(SkVD::PIVOTX, 0.0);
  C.y += val(SkVD::PIVOTY, 0.0);

  // Same composition order as the Toonz stage placement: translation, then
  // rotation, shear and scale about the pivot
  return TTranslation(tx, ty) * TTranslation(C) * TRotation(rot) *
         TShear(shx, shy) * TScale(sx, sy) * TTranslation(-C);
}

//------------------------------------------------------------------

// STEP C.2 — see the header for why the targets arrive from outside instead of
// being computed here.
void PlasticSkeletonDeformation::setSecondaryPinTargets(
    double frame, const std::map<int, TPointD> &localTargets) {
  m_imp->m_secondaryFrame   = frame;
  m_imp->m_secondaryTargets = localTargets;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::clearSecondaryPinTargets() {
  m_imp->m_secondaryFrame = -1.0e30;
  m_imp->m_secondaryTargets.clear();
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setSolvedSkeleton(
    int skeletonId, double frame, const PlasticSkeleton &skeleton) {
  m_imp->m_solvedSkeleton = skeleton;
  m_imp->m_solvedFrame    = frame;
  m_imp->m_solvedSkelId   = skeletonId;
  m_imp->m_hasSolved      = true;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::clearSolvedSkeleton() {
  m_imp->m_hasSolved    = false;
  m_imp->m_solvedFrame  = -1.0e30;
  m_imp->m_solvedSkelId = -1;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::enablePins(bool on) {
  if (m_imp->m_pinsEnabled == on) return;
  m_imp->m_pinsEnabled = on;

  // Anything solved under the previous pin state is now wrong. Dropping it here
  // is what makes the toggle take effect immediately: leaving it behind is why
  // the rig looked broken until the next click happened to force a re-solve.
  clearSolvedSkeleton();
  clearSecondaryPinTargets();

  // The evaluation result changes: invalidate the associated deformers
  PlasticDeformerStorage::instance()->invalidateDeformation(
      this, PlasticDeformerStorage::NONE);
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::pinsEnabled() const {
  return m_imp->m_pinsEnabled;
}

//------------------------------------------------------------------

namespace {

// Shift a joint's angular limits by the parent BONE's rotation, but only for a
// joint hanging directly off the skeleton's ROOT: those have no parent bone of
// their own, so their limits would be measured against a constant and would not
// follow the body. Every other joint already has a real parent bone here.
inline void shiftLimitsForRootChild(const PlasticSkeleton &skel, int vParent,
                                    double refDeg, double &lo, double &hi) {
  if (refDeg == 0.0) return;
  if (vParent < 0 || skel.vertex(vParent).parent() >= 0) return;
  lo += refDeg;
  hi += refDeg;
}

}  // namespace

void PlasticSkeletonDeformation::updatePosition(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v, const TPointD &pos, double rootChildRefDeg) {
  const PlasticSkeletonVertex &vx = deformedSkeleton.vertex(v);
  int vParent                     = vx.parent();

  const TPointD &vParentPos = deformedSkeleton.vertex(vParent).P();
  const TPointD &vPos       = deformedSkeleton.vertex(v).P();

  SkVD &vd = m_imp->m_vds.find(vx.name())->m_vd;

  // NOTE: The following aDelta calculation should be done as a true difference
  // - this is still ok and spares
  // access to v's grandParent...

  double loLim, hiLim;
  effAngleLimits(&vd, vx, frame, loLim, hiLim);
  shiftLimitsForRootChild(deformedSkeleton, vParent, rootChildRefDeg, loLim, hiLim);

  double aDelta = tcg::point_ops::angle(vPos - vParentPos, pos - vParentPos) *
                  M_180_PI,
         dDelta = tcg::point_ops::dist(vParentPos, pos) -
                  tcg::point_ops::dist(vParentPos, vPos),

         a = tcrop(vd.m_params[SkVD::ANGLE]->getValue(frame) + aDelta, loLim,
                   hiLim),
         d = vd.m_params[SkVD::DISTANCE]->getValue(frame) + dDelta;

  vd.m_params[SkVD::ANGLE]->setValue(frame, a);
  vd.m_params[SkVD::DISTANCE]->setValue(frame, d);

  m_imp->updateBranchPositions(originalSkeleton, deformedSkeleton, frame, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::updateAngle(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v, const TPointD &pos, double rootChildRefDeg) {
  const PlasticSkeletonVertex &vx = deformedSkeleton.vertex(v);
  int vParent                     = vx.parent();

  const TPointD &vParentPos = deformedSkeleton.vertex(vParent).P();

  // No need to access the grandParent, we're making the diff against the old vx
  // position

  SkVD &vd = m_imp->m_vds.find(vx.name())->m_vd;

  double loLim, hiLim;
  effAngleLimits(&vd, vx, frame, loLim, hiLim);
  shiftLimitsForRootChild(deformedSkeleton, vParent, rootChildRefDeg, loLim, hiLim);

  // Continuity-first clamp: the stored angle can sit OUTSIDE [lo, hi] for
  // legitimate reasons — the unpin bake writes the planted pose unclamped, and
  // a wrapped writer can store 185 as -175. A hard tcrop against the static
  // range would then yank the joint to the numerically nearest limit on the
  // FIRST touch (the "suddenly feels bounds it didn't have" snap, or the flip
  // to the other side of the arc past 180). Widening the range to include the
  // current value means: from out of range you can only move back IN — never
  // further out, never teleported — and once inside the full limits apply.
  const double cur = vd.m_params[SkVD::ANGLE]->getValue(frame);
  double aDelta = tcg::point_ops::angle(vx.P() - vParentPos, pos - vParentPos) *
                  M_180_PI,
         a = tcrop(cur + aDelta, std::min(loLim, cur), std::max(hiLim, cur));

  vd.m_params[SkVD::ANGLE]->setValue(frame, a);

  // Probe for the cross-column angle-limit report (2026-07-27). This is the
  // clamp that actually governs a plain (non-IK) joint drag — the twin in
  // writeBackAnglesFor_animate is NOT reached for it, measured. Everything
  // here is in the COLUMN'S OWN local space, so bending the torso (which
  // rotates the whole arm column through its placement) should cancel out and
  // leave the range identical. If `a` stops at the same hi/lo in both torso
  // poses, the geometry is already right and only the drawn wedge is wrong.
  if (::getenv("ZTORYC_LIMIT_DIAG"))
    qDebug().noquote()
        << QString("[UPDANG] v=%1 par=%2 cur=%3 aDelta=%4 a=%5 lo=%6 hi=%7 %8")
               .arg(v).arg(vParent)
               .arg(cur, 0, 'f', 2).arg(aDelta, 0, 'f', 2).arg(a, 0, 'f', 2)
               .arg(loLim, 0, 'f', 2).arg(hiLim, 0, 'f', 2)
               .arg(fabs((cur + aDelta) - a) > 1e-6 ? "CLAMPED" : "free");

  m_imp->updateBranchPositions(originalSkeleton, deformedSkeleton, frame, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::addVertex(PlasticSkeleton *skel, int v) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->attachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::insertVertex(PlasticSkeleton *skel, int v) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->attachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::deleteVertex(PlasticSkeleton *skel, int v) {
  assert(v > 0);  // Root should not be deleted

  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  // Remove the vertex deformation associated with v
  m_imp->detachVertex(skel->vertex(v).name(), skelId, v);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::vertexNameChange(PlasticSkeleton *skel, int v,
                                                  const QString &newName) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->rebindVertex(skel->vertex(v).name(), skelId, newName);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::clear(PlasticSkeleton *skel) {
  int skelId = skeletonId(skel);
  assert(skelId >= 0);

  m_imp->detach(skelId);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::addObserver(TParamObserver *observer) {
  m_imp->m_observers.insert(observer);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::removeObserver(TParamObserver *observer) {
  m_imp->m_observers.erase(observer);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setGrammar(TSyntax::Grammar *grammar) {
  SkVDSet::iterator vdt, vdEnd(m_imp->m_vds.end());
  for (vdt = m_imp->m_vds.begin(); vdt != vdEnd; ++vdt) {
    SkVD &vd = vdt->m_vd;

    for (int c = 0; c != SkVD::PARAMS_COUNT; ++c)
      vd.m_params[c]->setGrammar(grammar);
  }

  m_imp->m_skelIdsParam->setGrammar(grammar);

  m_imp->m_grammar = grammar;
}

//------------------------------------------------------------------

//**************************************************************************************
//    PoseAction  implementation
//**************************************************************************************

double MeshCorrective::weight(double driverAngle) const {
  const double span = m_fullAngle - m_restAngle;
  if (fabs(span) < 1e-9) return 0.0;  // half-built: inert, never NaN
  double w = (driverAngle - m_restAngle) / span;
  if (w < 0.0) w = 0.0;
  if (w > 1.0) w = 1.0;
  return w;
}

//------------------------------------------------------------------

TPointD MeshCorrective::delta(int meshIdx, int v) const {
  std::map<int, std::map<int, TPointD>>::const_iterator mt =
      m_deltas.find(meshIdx);
  if (mt == m_deltas.end()) return TPointD();
  std::map<int, TPointD>::const_iterator vt = mt->second.find(v);
  return vt == mt->second.end() ? TPointD() : vt->second;
}

//------------------------------------------------------------------

void MeshCorrective::setDelta(int meshIdx, int v, const TPointD &d) {
  m_deltas[meshIdx][v] = d;
}

//------------------------------------------------------------------

double PoseAction::delta(const QString &vertexName, int param) const {
  const int slot = SkVD::poseParamSlot(param);
  if (slot < 0) return 0.0;

  std::map<QString, std::vector<double>>::const_iterator it =
      m_deltas.find(vertexName);
  if (it == m_deltas.end()) return 0.0;
  if (slot >= (int)it->second.size()) return 0.0;

  return it->second[slot];
}

//------------------------------------------------------------------

void PoseAction::setDelta(const QString &vertexName, int param, double value) {
  const int slot = SkVD::poseParamSlot(param);
  if (slot < 0) return;  // not a pose param: silently out of scope, by design

  std::vector<double> &v = m_deltas[vertexName];
  if (v.size() != (size_t)SkVD::POSE_PARAMS_COUNT)
    v.resize(SkVD::POSE_PARAMS_COUNT, 0.0);

  v[slot] = value;
}

//**************************************************************************************
//    PlasticSkeletonDeformation  pose actions
//**************************************************************************************

double PlasticSkeletonDeformation::poseParamNeutral(int param) {
  // The scale factors rest at 1 (100%); every other pose channel is already an
  // OFFSET from the original skeleton, so its rest value is 0.
  return (param == SkVD::SCALEX || param == SkVD::SCALEY) ? 1.0 : 0.0;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::recordPoseAction(const QString &name,
                                                 double frame) {
  const int idx   = addPoseAction(name);
  PoseAction &act = m_imp->m_poseActions[idx];

  // Re-recording replaces the shape wholesale: a leftover delta on a vertex
  // the new pose does not touch would keep pulling it.
  act.m_deltas.clear();

  // Below this the delta is numerical noise from the solver, not intent.
  // Keeping it would make every action dense and every save bigger, for a
  // contribution no one can see.
  const double eps = 1e-9;

  SkVDSet::iterator vdt, vdEnd(m_imp->m_vds.end());
  for (vdt = m_imp->m_vds.begin(); vdt != vdEnd; ++vdt) {
    const SkVD &vd = vdt->m_vd;

    for (int i = 0; i < SkVD::POSE_PARAMS_COUNT; ++i) {
      const int p = SkVD::POSE_PARAMS[i];
      if (!vd.m_params[p]) continue;

      // Base value on purpose — see the note on the declaration.
      const double delta =
          vd.m_params[p]->getValue(frame) - poseParamNeutral(p);

      if (fabs(delta) > eps) act.setDelta(vdt->m_name, p, delta);
    }
  }

  // Bind the action to the skeleton it was authored on. The artist can widen
  // it afterwards (see PoseAction::m_skelIds); recording only knows this one.
  act.m_skelIds.clear();
  act.m_skelIds.insert(skeletonId(frame));

  // The character IS in this pose right now, by definition of recording it:
  // strength 1 here, and every other action reads 0 because the shape they were
  // contributing has just been absorbed into this one. A Base action is the
  // zero itself, so it has no strength to record.
  if (!act.m_isBase) m_imp->writePoseStrength(idx, 1.0, frame);

  return idx;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::poseActionsCount() const {
  return (int)m_imp->m_poseActions.size();
}

//------------------------------------------------------------------

const PoseAction *PlasticSkeletonDeformation::poseAction(int idx) const {
  if (idx < 0 || idx >= (int)m_imp->m_poseActions.size()) return nullptr;
  return &m_imp->m_poseActions[idx];
}

//------------------------------------------------------------------

PoseAction *PlasticSkeletonDeformation::poseAction(int idx) {
  if (idx < 0 || idx >= (int)m_imp->m_poseActions.size()) return nullptr;
  return &m_imp->m_poseActions[idx];
}

//------------------------------------------------------------------

PoseAction *PlasticSkeletonDeformation::poseAction(const QString &name) {
  for (size_t i = 0; i < m_imp->m_poseActions.size(); ++i)
    if (m_imp->m_poseActions[i].m_name == name)
      return &m_imp->m_poseActions[i];
  return nullptr;
}

//------------------------------------------------------------------

int PlasticSkeletonDeformation::addPoseAction(const QString &name) {
  for (size_t i = 0; i < m_imp->m_poseActions.size(); ++i)
    if (m_imp->m_poseActions[i].m_name == name) return (int)i;

  m_imp->m_poseActions.push_back(PoseAction(name));
  return (int)m_imp->m_poseActions.size() - 1;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::removePoseAction(int idx) {
  if (idx < 0 || idx >= (int)m_imp->m_poseActions.size()) return;
  m_imp->m_poseActions.erase(m_imp->m_poseActions.begin() + idx);
}

//------------------------------------------------------------------

std::vector<PoseAction> PlasticSkeletonDeformation::getPoseActions() const {
  return m_imp->m_poseActions;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setPoseActions(
    const std::vector<PoseAction> &actions) {
  m_imp->m_poseActions = actions;
}

//------------------------------------------------------------------

PlasticSkeletonDeformation::PoseKeyState
PlasticSkeletonDeformation::getPoseKeyState() const {
  PoseKeyState st;
  SkVDSet::iterator vdt, vdEnd(m_imp->m_vds.end());
  for (vdt = m_imp->m_vds.begin(); vdt != vdEnd; ++vdt) {
    std::map<int, std::vector<TDoubleKeyframe>> byParam;
    for (int i = 0; i < SkVD::POSE_PARAMS_COUNT; ++i) {
      const int p = SkVD::POSE_PARAMS[i];
      TDoubleParam *pp = vdt->m_vd.m_params[p].getPointer();
      if (!pp) continue;
      std::vector<TDoubleKeyframe> keys;
      const int kc = pp->getKeyframeCount();
      for (int k = 0; k < kc; ++k) keys.push_back(pp->getKeyframe(k));
      byParam[p] = keys;
    }
    st.m_paramKeys[vdt->m_name] = byParam;
  }
  for (size_t a = 0; a < m_imp->m_poseActions.size(); ++a) {
    std::vector<TDoubleKeyframe> keys;
    double def = 0.0;
    if (m_imp->m_poseActions[a].m_guide) {
      TDoubleParam *g = m_imp->m_poseActions[a].m_guide.getPointer();
      const int kc    = g->getKeyframeCount();
      for (int k = 0; k < kc; ++k) keys.push_back(g->getKeyframe(k));
      def = g->getDefaultValue();
    }
    st.m_guides.push_back(std::make_pair(keys, def));
  }
  return st;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setPoseKeyState(const PoseKeyState &s) {
  for (std::map<QString, std::map<int, std::vector<TDoubleKeyframe>>>::
           const_iterator vt = s.m_paramKeys.begin();
       vt != s.m_paramKeys.end(); ++vt) {
    SkVDSet::iterator it = m_imp->m_vds.find(vt->first);
    if (it == m_imp->m_vds.end()) continue;
    for (std::map<int, std::vector<TDoubleKeyframe>>::const_iterator pt =
             vt->second.begin();
         pt != vt->second.end(); ++pt) {
      TDoubleParam *pp = it->m_vd.m_params[pt->first].getPointer();
      if (!pp) continue;
      pp->clearKeyframes();
      for (const TDoubleKeyframe &k : pt->second) pp->setKeyframe(k);
    }
  }
  for (size_t a = 0; a < s.m_guides.size() && a < m_imp->m_poseActions.size();
       ++a) {
    if (!m_imp->m_poseActions[a].m_guide) continue;
    TDoubleParam *g = m_imp->m_poseActions[a].m_guide.getPointer();
    g->clearKeyframes();
    for (const TDoubleKeyframe &k : s.m_guides[a].first) g->setKeyframe(k);
    g->setDefaultValue(s.m_guides[a].second);
  }
}

//------------------------------------------------------------------

const PoseAction *PlasticSkeletonDeformation::baseActionOf(int skelId) const {
  for (size_t a = 0; a < m_imp->m_poseActions.size(); ++a) {
    const PoseAction &act = m_imp->m_poseActions[a];
    if (act.m_isBase && act.appliesTo(skelId)) return &act;
  }
  return nullptr;
}

//------------------------------------------------------------------

double PlasticSkeletonDeformation::poseBaseValue(const QString &vertexName,
                                                 int param, int skelId) const {
  const double neutral  = poseParamNeutral(param);
  const PoseAction *base = baseActionOf(skelId);
  return base ? neutral + base->delta(vertexName, param) : neutral;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::setPoseActionAsBase(int idx, bool isBase) {
  PoseAction *act = poseAction(idx);
  if (!act) return;
  act->m_isBase = isBase;
  if (!isBase) return;

  // One zero per skeleton, or "strength 0" would mean two different shapes on
  // the same skeleton. Two bases may coexist as long as they never meet, so
  // what gets cleared is any base whose skeletons OVERLAP with this one's.
  for (size_t a = 0; a < m_imp->m_poseActions.size(); ++a) {
    if ((int)a == idx) continue;
    PoseAction &other = m_imp->m_poseActions[a];
    if (!other.m_isBase) continue;

    bool meet = act->m_skelIds.empty() || other.m_skelIds.empty();
    for (std::set<int>::const_iterator it = act->m_skelIds.begin();
         !meet && it != act->m_skelIds.end(); ++it)
      meet = other.m_skelIds.count(*it) > 0;

    if (meet) other.m_isBase = false;
  }
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::poseActionAppliesAt(int idx,
                                                     double frame) const {
  const PoseAction *act = poseAction(idx);
  return act && act->appliesTo(skeletonId(frame));
}

//------------------------------------------------------------------

double PlasticSkeletonDeformation::poseStrengthAt(int idx, double frame) const {
  const PoseAction *act = poseAction(idx);
  if (!act || !act->m_guide) return 0.0;

  // The strength is RECORDED in the guide curve, never re-derived from the
  // params. Deducing it — (value - neutral) / delta on the most-moved param —
  // only holds while actions are disjoint: two actions sharing a param read
  // each other's work as their own, so dialling one left the others showing a
  // spurious value instead of dropping to 0.
  //
  // Plain getValue(), same extrapolation as the pose params the guide
  // describes: both hold the first key backwards and the last one forwards, so
  // the slider agrees with the character at every frame.
  return act->m_guide->getValue(frame);
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::Imp::writePoseStrength(int idx,
                                                        double strength,
                                                        double frame) {
  if (idx < 0 || idx >= (int)m_poseActions.size()) return;
  PoseAction &act = m_poseActions[idx];
  if (act.m_guide) act.m_guide->setValue(frame, strength);

  // Whose record is now a lie depends on how much of the skeleton was claimed:
  //   ADD    - nothing, it only pushed its own params on top of the others;
  //   POSE   - everything, it rewrote every pose param of the skeleton;
  //   PART   - only the actions it collided with, param by param.
  if (act.m_mode == PoseAction::ADD) return;

  for (size_t a = 0; a < m_poseActions.size(); ++a) {
    if ((int)a == idx) continue;
    // Leave other skeletons' actions alone: they never wrote anything here, and
    // zeroing them would rewrite their curve from a frame they do not own.
    if (!m_back->poseActionAppliesAt((int)a, frame)) continue;
    if (act.isPartial() && !overlaps(act, m_poseActions[a])) continue;

    TDoubleParamP &g = m_poseActions[a].m_guide;
    // Only if it had something to clear: keying 0 on an action that already
    // reads 0 would litter every curve with keys the artist never asked for.
    if (g && fabs(g->getValue(frame)) > 1e-9) g->setValue(frame, 0.0);
  }
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::Imp::overlaps(const PoseAction &a,
                                               const PoseAction &b) {
  // Do the two actions drive any of the same (vertex, param)? Only then does
  // one stamping over the other invalidate it.
  for (std::map<QString, std::vector<double>>::const_iterator at =
           a.m_deltas.begin();
       at != a.m_deltas.end(); ++at) {
    std::map<QString, std::vector<double>>::const_iterator bt =
        b.m_deltas.find(at->first);
    if (bt == b.m_deltas.end()) continue;
    const size_t n = std::min(at->second.size(), bt->second.size());
    for (size_t s = 0; s < n; ++s)
      if (at->second[s] != 0.0 && bt->second[s] != 0.0) return true;
  }
  return false;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::beginPoseDrag(int idx, double frame) {
  m_imp->m_poseDragBase.clear();
  m_imp->m_poseDragIdx = idx;
  PoseAction *act      = poseAction(idx);
  if (!act || act->isAbsolute()) return;  // only an Add reads the live base

  for (std::map<QString, std::vector<double>>::const_iterator it =
           act->m_deltas.begin();
       it != act->m_deltas.end(); ++it) {
    SkVDSet::iterator vt = m_imp->m_vds.find(it->first);
    if (vt == m_imp->m_vds.end()) continue;
    for (int i = 0; i < SkVD::POSE_PARAMS_COUNT && i < (int)it->second.size();
         ++i) {
      const int p = SkVD::POSE_PARAMS[i];
      if (!vt->m_vd.m_params[p]) continue;
      m_imp->m_poseDragBase[std::make_pair(it->first, p)] =
          vt->m_vd.m_params[p]->getValue(frame);
    }
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::endPoseDrag() {
  m_imp->m_poseDragBase.clear();
  m_imp->m_poseDragIdx = -1;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::applyPoseStrength(int idx, double strength,
                                                   double frame) {
  PoseAction *act = poseAction(idx);
  if (!act) return;
  // Belt and braces: the panel disables foreign actions, but the deformation
  // owns the rule — a stray call must not stamp a pose authored for another
  // skeleton onto this one.
  if (!poseActionAppliesAt(idx, frame)) return;

  // Precompute targets before writing (an Offset reads the live base, which a
  // just-written key would perturb).
  struct Target {
    QString m_v;
    int m_param;
    double m_value;
  };
  std::vector<Target> targets;

  if (act->isAbsolute()) {
    // A Pose is EXCLUSIVE and covers the WHOLE skeleton: iterate every vertex
    // param, not just the ones the pose moved. Params it leaves at rest write
    // neutral (delta 0 -> neutral + s*0), so they OVERRIDE whatever another pose
    // left there — otherwise switching from a pose that bent the torso back to
    // one that does not would keep the torso bent (delta absent = untouched).
    // Strength 0 is the skeleton's BASE pose, which is its real rest only when
    // no action was marked Base — on an exploded rig that rest is the scattered
    // layout, and dialling to 0 would take the character apart.
    const int skelId = skeletonId(frame);

    // Both recall modes lerp base -> recorded; they differ only in HOW MUCH of
    // the skeleton they claim. A POSE claims all of it, so params it never
    // moved are driven to the base and no other pose can survive underneath.
    // A PART claims only the params it recorded, so a mouth shape lands exactly
    // as authored while the eyes keep whatever is driving them.
    auto stamp = [&](const QString &vname, int p) {
      const double base = poseBaseValue(vname, p, skelId);
      const double full = poseParamNeutral(p) + act->delta(vname, p);
      targets.push_back({vname, p, base + strength * (full - base)});
    };

    if (act->isPartial()) {
      for (std::map<QString, std::vector<double>>::const_iterator it =
               act->m_deltas.begin();
           it != act->m_deltas.end(); ++it) {
        SkVDSet::iterator vt = m_imp->m_vds.find(it->first);
        if (vt == m_imp->m_vds.end()) continue;
        for (int i = 0; i < SkVD::POSE_PARAMS_COUNT && i < (int)it->second.size();
             ++i) {
          if (it->second[i] == 0.0) continue;  // not one of its params
          const int p = SkVD::POSE_PARAMS[i];
          if (vt->m_vd.m_params[p]) stamp(it->first, p);
        }
      }
    } else {
      SkVDSet::iterator vt, vEnd(m_imp->m_vds.end());
      for (vt = m_imp->m_vds.begin(); vt != vEnd; ++vt)
        for (int i = 0; i < SkVD::POSE_PARAMS_COUNT; ++i) {
          const int p = SkVD::POSE_PARAMS[i];
          if (vt->m_vd.m_params[p]) stamp(vt->m_name, p);
        }
    }
  } else {
    // An Add is partial: only the params it moved, pushed onto the current base,
    // so independent controllers stack without wiping one another.
    for (std::map<QString, std::vector<double>>::const_iterator it =
             act->m_deltas.begin();
         it != act->m_deltas.end(); ++it) {
      SkVDSet::iterator vt = m_imp->m_vds.find(it->first);
      if (vt == m_imp->m_vds.end()) continue;
      for (int i = 0; i < SkVD::POSE_PARAMS_COUNT && i < (int)it->second.size();
           ++i) {
        const double delta = it->second[i];
        if (delta == 0.0) continue;
        const int p = SkVD::POSE_PARAMS[i];
        if (!vt->m_vd.m_params[p]) continue;
        // Base frozen at the start of the gesture when one is running: reading
        // the live value here would add the previous move's own result back in,
        // compounding the offset on every slider step.
        double base;
        std::map<std::pair<QString, int>, double>::const_iterator bt =
            m_imp->m_poseDragBase.find(std::make_pair(it->first, p));
        if (m_imp->m_poseDragIdx == idx && bt != m_imp->m_poseDragBase.end())
          base = bt->second;
        else
          base = vt->m_vd.m_params[p]->getValue(frame);
        targets.push_back({it->first, p, base + strength * delta});
      }
    }
  }

  for (const Target &t : targets) {
    SkVDSet::iterator vt = m_imp->m_vds.find(t.m_v);
    if (vt != m_imp->m_vds.end() && vt->m_vd.m_params[t.m_param])
      vt->m_vd.m_params[t.m_param]->setValue(frame, t.m_value);
  }

  // Record how much of the action is now applied, so the slider can be read
  // back at any frame instead of guessed from the params.
  m_imp->writePoseStrength(idx, strength, frame);
}

//------------------------------------------------------------------
//    Mesh correctives
//------------------------------------------------------------------

int PlasticSkeletonDeformation::meshCorrectivesCount() const {
  return (int)m_imp->m_meshCorrectives.size();
}

const MeshCorrective *PlasticSkeletonDeformation::meshCorrective(int idx) const {
  if (idx < 0 || idx >= (int)m_imp->m_meshCorrectives.size()) return nullptr;
  return &m_imp->m_meshCorrectives[idx];
}

MeshCorrective *PlasticSkeletonDeformation::meshCorrective(int idx) {
  if (idx < 0 || idx >= (int)m_imp->m_meshCorrectives.size()) return nullptr;
  return &m_imp->m_meshCorrectives[idx];
}

MeshCorrective *PlasticSkeletonDeformation::meshCorrective(const QString &name) {
  for (size_t i = 0; i < m_imp->m_meshCorrectives.size(); ++i)
    if (m_imp->m_meshCorrectives[i].m_name == name)
      return &m_imp->m_meshCorrectives[i];
  return nullptr;
}

int PlasticSkeletonDeformation::addMeshCorrective(const QString &name) {
  for (size_t i = 0; i < m_imp->m_meshCorrectives.size(); ++i)
    if (m_imp->m_meshCorrectives[i].m_name == name) return (int)i;
  m_imp->m_meshCorrectives.push_back(MeshCorrective(name));
  return (int)m_imp->m_meshCorrectives.size() - 1;
}

void PlasticSkeletonDeformation::removeMeshCorrective(int idx) {
  if (idx < 0 || idx >= (int)m_imp->m_meshCorrectives.size()) return;
  m_imp->m_meshCorrectives.erase(m_imp->m_meshCorrectives.begin() + idx);
}

bool PlasticSkeletonDeformation::hasSOOwners() const {
  return !m_imp->m_soOwners.empty();
}

void PlasticSkeletonDeformation::setSOOwner(int meshIdx, int v,
                                            const QString &name) {
  if (name.isEmpty())
    clearSOOwner(meshIdx, v);
  else
    m_imp->m_soOwners[meshIdx][v] = name;
}

void PlasticSkeletonDeformation::clearSOOwner(int meshIdx, int v) {
  std::map<int, std::map<int, QString>>::iterator mt =
      m_imp->m_soOwners.find(meshIdx);
  if (mt == m_imp->m_soOwners.end()) return;
  mt->second.erase(v);
  if (mt->second.empty()) m_imp->m_soOwners.erase(mt);
}

void PlasticSkeletonDeformation::clearSOOwners() { m_imp->m_soOwners.clear(); }

bool PlasticSkeletonDeformation::soOwner(int meshIdx, int v,
                                         QString &name) const {
  std::map<int, std::map<int, QString>>::const_iterator mt =
      m_imp->m_soOwners.find(meshIdx);
  if (mt == m_imp->m_soOwners.end()) return false;
  std::map<int, QString>::const_iterator vt = mt->second.find(v);
  if (vt == mt->second.end()) return false;
  name = vt->second;
  return true;
}

std::map<int, std::map<int, QString>>
PlasticSkeletonDeformation::getSOOwners() const {
  return m_imp->m_soOwners;
}

void PlasticSkeletonDeformation::setSOOwners(
    const std::map<int, std::map<int, QString>> &owners) {
  m_imp->m_soOwners = owners;
}

//------------------------------------------------------------------

std::vector<MeshCorrective> PlasticSkeletonDeformation::getMeshCorrectives()
    const {
  return m_imp->m_meshCorrectives;
}

void PlasticSkeletonDeformation::setMeshCorrectives(
    const std::vector<MeshCorrective> &correctives) {
  m_imp->m_meshCorrectives = correctives;
}

TPointD PlasticSkeletonDeformation::meshCorrectiveOffset(int meshIdx, int v,
                                                         double frame) const {
  if (m_imp->m_meshCorrectives.empty()) return TPointD();

  TPointD sum;
  for (size_t i = 0; i < m_imp->m_meshCorrectives.size(); ++i) {
    const MeshCorrective &mc = m_imp->m_meshCorrectives[i];

    const TPointD d = mc.delta(meshIdx, v);
    if (d.x == 0.0 && d.y == 0.0) continue;  // vertex untouched by this one

    // Driver's BASE angle (never the blended result): no guide->effect loop.
    SkVDSet::iterator it = m_imp->m_vds.find(mc.m_driverVertexName);
    if (it == m_imp->m_vds.end()) continue;
    const SkVD &vd = it->m_vd;
    if (!vd.m_params[SkVD::ANGLE]) continue;

    const double a = vd.m_params[SkVD::ANGLE]->getValue(frame);
    sum += mc.weight(a) * d;
  }
  return sum;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::saveData(TOStream &os) {
  // Save skeleton vertex deformations
  os.openChild("VertexDeforms");  // These are saved *before* skeletons
  {                               // ON PURPOSE - in loadData(), we'll
    SkVDSet::iterator vdt,
        vdEnd(m_imp->m_vds.end());  // attach() skeletons to a completely
    for (vdt = m_imp->m_vds.begin(); vdt != vdEnd;
         ++vdt)  // rebuilt set of vertex deformations.
    {
      os.child("Name") << vdt->m_name;
      os.child("Hook") << vdt->m_hookNumber;
      os.child("VD") << vdt->m_vd;
    }
  }
  os.closeChild();

  // Save skeleton ids param
  os.child("SkelIdsParam") << *m_imp->m_skelIdsParam;

  // Save skeletons
  os.openChild("Skeletons");
  {
    SkeletonSet::iterator st, sEnd(m_imp->m_skeletons.end());
    for (st = m_imp->m_skeletons.begin(); st != sEnd; ++st) {
      os.child("SkelId") << st->get_left();
      os.child("Skeleton") << *st->get_right();
    }
  }
  os.closeChild();

  // SuperPlastic: pins put to sleep (tag only when disabled — old scenes and
  // old readers are unaffected)
  if (!m_imp->m_pinsEnabled) os.child("PinsDisabled") << 1;

  // ZtoRig pose actions. Written only when there are any: a scene that never
  // used them serializes exactly as before, and a reader that does not know
  // the tag skips it (loadData falls through to skipCurrentTag), so the format
  // stays compatible in BOTH directions with Tahoma2D and older Ztoryc.
  if (!m_imp->m_poseActions.empty()) {
    os.openChild("PoseActions");
    for (size_t a = 0; a < m_imp->m_poseActions.size(); ++a) {
      PoseAction &act = m_imp->m_poseActions[a];

      os.openChild("Action");
      os.child("Name") << act.m_name;
      // Written only when not the default, so rigs that never left Offset
      // round-trip byte-identical. The legacy "Absolute" tag is still READ.
      if (act.m_mode != PoseAction::ADD) os.child("Mode") << act.m_mode;
      // One tag per skeleton; none at all means "applies everywhere", which is
      // also what scenes written before this keep.
      for (std::set<int>::const_iterator st = act.m_skelIds.begin();
           st != act.m_skelIds.end(); ++st)
        os.child("Skel") << *st;
      if (act.m_isBase) os.child("Base") << 1;  // tag only when true
      os.child("Guide") << *act.m_guide;

      std::map<QString, std::vector<double>>::const_iterator dt;
      for (dt = act.m_deltas.begin(); dt != act.m_deltas.end(); ++dt) {
        os.openChild("Delta");
        os.child("V") << dt->first;
        // Slot order is POSE_PARAMS order; the count is written so a future
        // change to the list is detectable instead of silently misaligning.
        os.child("N") << (int)dt->second.size();
        for (size_t s = 0; s < dt->second.size(); ++s)
          os.child("D") << dt->second[s];
        os.closeChild();
      }
      os.closeChild();
    }
    os.closeChild();
  }

  // ZtoRig mesh correctives — same write-only-when-present, skip-if-unknown
  // discipline as PoseActions: byte-identical for scenes that never used them.
  if (!m_imp->m_meshCorrectives.empty()) {
    os.openChild("MeshCorrectives");
    for (size_t i = 0; i < m_imp->m_meshCorrectives.size(); ++i) {
      const MeshCorrective &mc = m_imp->m_meshCorrectives[i];
      os.openChild("Corrective");
      os.child("Name") << mc.m_name;
      os.child("Driver") << mc.m_driverVertexName;
      os.child("RestAngle") << mc.m_restAngle;
      os.child("FullAngle") << mc.m_fullAngle;
      std::map<int, std::map<int, TPointD>>::const_iterator mt;
      for (mt = mc.m_deltas.begin(); mt != mc.m_deltas.end(); ++mt) {
        std::map<int, TPointD>::const_iterator vt;
        for (vt = mt->second.begin(); vt != mt->second.end(); ++vt) {
          os.openChild("D");
          os.child("m") << mt->first;
          os.child("v") << vt->first;
          os.child("x") << vt->second.x;
          os.child("y") << vt->second.y;
          os.closeChild();
        }
      }
      os.closeChild();
    }
    os.closeChild();
  }

  if (!m_imp->m_soOwners.empty()) {
    os.openChild("SOOwners");
    std::map<int, std::map<int, QString>>::const_iterator mt;
    for (mt = m_imp->m_soOwners.begin(); mt != m_imp->m_soOwners.end(); ++mt) {
      std::map<int, QString>::const_iterator vt;
      for (vt = mt->second.begin(); vt != mt->second.end(); ++vt) {
        os.openChild("S");
        os.child("m") << mt->first;
        os.child("v") << vt->first;
        os.child("n") << vt->second;
        os.closeChild();
      }
    }
    os.closeChild();
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::loadData(TIStream &is) {
  if (is.getVersion() < VersionNumber(1, 21)) {
    loadData_prerelease(is);
    return;
  }

  int skeletonId;
  PlasticSkeleton *skeleton;

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "VertexDeforms") {
      VDKey vKey;

      while (is.openChild(tagName)) {
        if (tagName == "Name")
          is >> vKey.m_name, is.matchEndTag();
        else if (tagName == "Hook")
          is >> vKey.m_hookNumber, is.matchEndTag();
        else if (tagName == "VD") {
          m_imp->touchParams(vKey.m_vd);
          is >> vKey.m_vd, is.matchEndTag();

          m_imp->m_vds.insert(vKey);

          vKey = VDKey();
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else if (tagName == "PinsDisabled") {
      int disabled = 0;
      is >> disabled, is.matchEndTag();
      m_imp->m_pinsEnabled = (disabled == 0);
    } else if (tagName == "PoseActions") {
      while (is.openChild(tagName)) {
        if (tagName == "Action") {
          PoseAction act;
          while (is.openChild(tagName)) {
            if (tagName == "Name")
              is >> act.m_name, is.matchEndTag();
            else if (tagName == "Mode") {
              int m = PoseAction::ADD;
              is >> m, is.matchEndTag();
              if (m >= PoseAction::ADD && m <= PoseAction::PART)
                act.m_mode = m;
            } else if (tagName == "Absolute") {
              // Pre-PART scenes: the flag only ever meant whole-skeleton Pose.
              int abs = 0;
              is >> abs, is.matchEndTag();
              if (abs != 0) act.m_mode = PoseAction::POSE;
            } else if (tagName == "Base") {
              int b = 0;
              is >> b, is.matchEndTag();
              act.m_isBase = (b != 0);
            } else if (tagName == "Skel") {
              int sid = -1;
              is >> sid, is.matchEndTag();
              if (sid >= 0) act.m_skelIds.insert(sid);
            } else if (tagName == "SkelId") {
              // Short-lived single-skeleton form, before sets.
              int sid = -1;
              is >> sid, is.matchEndTag();
              if (sid >= 0) act.m_skelIds.insert(sid);
            } else if (tagName == "Guide")
              is >> *act.m_guide, is.matchEndTag();
            else if (tagName == "Delta") {
              QString vName;
              std::vector<double> values;
              while (is.openChild(tagName)) {
                if (tagName == "V")
                  is >> vName, is.matchEndTag();
                else if (tagName == "N") {
                  int n = 0;
                  is >> n, is.matchEndTag();
                  if (n > 0) values.reserve(n);
                } else if (tagName == "D") {
                  double d = 0.0;
                  is >> d, is.matchEndTag();
                  values.push_back(d);
                } else
                  is.skipCurrentTag();
              }
              is.matchEndTag();

              // Written by an older/newer POSE_PARAMS list: pad or truncate to
              // what this build knows, rather than indexing out of the vector.
              values.resize(SkVD::POSE_PARAMS_COUNT, 0.0);
              if (!vName.isEmpty()) act.m_deltas[vName] = values;
            } else
              is.skipCurrentTag();
          }
          is.matchEndTag();

          if (!act.m_name.isEmpty()) m_imp->m_poseActions.push_back(act);
        } else
          is.skipCurrentTag();
      }
      is.matchEndTag();
    } else if (tagName == "MeshCorrectives") {
      while (is.openChild(tagName)) {
        if (tagName == "Corrective") {
          MeshCorrective mc;
          while (is.openChild(tagName)) {
            if (tagName == "Name")
              is >> mc.m_name, is.matchEndTag();
            else if (tagName == "Driver")
              is >> mc.m_driverVertexName, is.matchEndTag();
            else if (tagName == "RestAngle")
              is >> mc.m_restAngle, is.matchEndTag();
            else if (tagName == "FullAngle")
              is >> mc.m_fullAngle, is.matchEndTag();
            else if (tagName == "D") {
              int m = 0, v = 0;
              double x = 0.0, y = 0.0;
              while (is.openChild(tagName)) {
                if (tagName == "m")
                  is >> m, is.matchEndTag();
                else if (tagName == "v")
                  is >> v, is.matchEndTag();
                else if (tagName == "x")
                  is >> x, is.matchEndTag();
                else if (tagName == "y")
                  is >> y, is.matchEndTag();
                else
                  is.skipCurrentTag();
              }
              is.matchEndTag();
              mc.setDelta(m, v, TPointD(x, y));
            } else
              is.skipCurrentTag();
          }
          is.matchEndTag();
          if (!mc.m_name.isEmpty()) m_imp->m_meshCorrectives.push_back(mc);
        } else
          is.skipCurrentTag();
      }
      is.matchEndTag();
    } else if (tagName == "SOOwners") {
      while (is.openChild(tagName)) {
        if (tagName == "S") {
          int m = 0, v = 0;
          QString n;
          while (is.openChild(tagName)) {
            if (tagName == "m")
              is >> m, is.matchEndTag();
            else if (tagName == "v")
              is >> v, is.matchEndTag();
            else if (tagName == "n")
              is >> n, is.matchEndTag();
            else
              is.skipCurrentTag();
          }
          is.matchEndTag();
          if (!n.isEmpty()) m_imp->m_soOwners[m][v] = n;
        } else
          is.skipCurrentTag();
      }
      is.matchEndTag();
    } else if (tagName == "SkelIdsParam")
      is >> *m_imp->m_skelIdsParam, is.matchEndTag();
    else if (tagName == "Skeletons") {
      while (is.openChild(tagName)) {
        if (tagName == "SkelId")
          is >> skeletonId, is.matchEndTag();
        else if (tagName == "Skeleton") {
          skeleton = new PlasticSkeleton;
          is >> *skeleton, is.matchEndTag();

          attach(skeletonId, skeleton);
          skeletonId = 0, skeleton = 0;
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::loadData_prerelease(TIStream &is) {
  struct locals {
    static void buildParentDirection(const PlasticSkeleton &skel, int v,
                                     TPointD &dir) {
      assert(v >= 0);

      const PlasticSkeletonVertex &vx = skel.vertex(v);

      int vParent = vx.parent();
      if (vParent < 0) return;  // dir remains as passed

      const TPointD &dir_ =
          tcg::point_ops::direction(skel.vertex(vParent).P(), vx.P(), 1e-4);

      if (dir_ != tcg::point_ops::NaP<TPointD>())
        dir = dir_;
      else
        buildParentDirection(skel, vParent, dir);
    }

    //------------------------------------------------------------------------------

    static void adjust(SkD &sd, int v) {
      PlasticSkeleton &skeleton = *sd.skeleton(1);

      PlasticSkeletonVertex &vx = skeleton.vertex(v);
      int vParent               = vx.parent();
      if (vParent >= 0) {
        // Rebuild vertex position
        const TPointD &vxPos       = skeleton.vertex(v).P();
        const TPointD &vxParentPos = skeleton.vertex(vParent).P();

        // Start by getting the polar reference
        TPointD dir(1.0, 0.0);

        buildParentDirection(skeleton, vParent, dir);

        // Now, rebuild vx's position
        SkVD &vd = sd.m_imp->m_vds.find(vx.name())->m_vd;

        double a = tcg::point_ops::angle(dir, vxPos - vxParentPos) * M_180_PI;
        double d = tcg::point_ops::dist(vxParentPos, vxPos);

        {
          TDoubleParamP param(vd.m_params[SkVD::ANGLE]);
          param->setDefaultValue(0.0);

          int k, kCount = param->getKeyframeCount();
          for (k = 0; k != kCount; ++k) {
            TDoubleKeyframe kf(param->getKeyframe(k));
            kf.m_value -= a;
            param->setKeyframe(k, kf);
          }
        }

        {
          TDoubleParamP param(vd.m_params[SkVD::DISTANCE]);
          param->setDefaultValue(0.0);

          int k, kCount = param->getKeyframeCount();
          for (k = 0; k != kCount; ++k) {
            TDoubleKeyframe kf(param->getKeyframe(k));
            kf.m_value -= d;
            param->setKeyframe(k, kf);
          }
        }
      }

      // Finally, update children positions
      PlasticSkeleton::vertex_type::edges_iterator et, eEnd(vx.edgesEnd());
      for (et = vx.edgesBegin(); et != eEnd; ++et) {
        int vChild = skeleton.edge(*et).vertex(1);
        if (vChild == v) continue;

        adjust(sd, vChild);
      }
    }
  };  // locals

  PlasticSkeletonP skeleton(new PlasticSkeleton);

  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "Skeleton")
      is >> *skeleton, is.matchEndTag();
    else if (tagName == "VertexDeforms") {
      while (is.openChild(tagName)) {
        if (tagName == "VD") {
          VDKey vKey;
          m_imp->touchParams(vKey.m_vd);

          is >> vKey.m_name, is >> vKey.m_vd;
          is.closeChild();

          // Rebuild vKey's data from skeleton
          int v, vCount = skeleton->verticesCount();
          for (v = 0; v != vCount; ++v)
            if (skeleton->vertex(v).name() == vKey.m_name) break;

          assert(v < vCount);
          vKey.m_hookNumber = skeleton->vertex(v).number();

          m_imp->m_vds.insert(vKey);
        } else
          is.skipCurrentTag();
      }

      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }

  attach(1, skeleton.getPointer());

  // SkVD params are now intended as deltas. Adjusting...
  locals::adjust(*this, 0);
}
