

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

// STL includes
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
  // onwership of \a multiple skeletons at once. However, its angle and distance
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

void PlasticPinSolver::plant(const std::vector<Joint> &joints,
                             const std::vector<Pin> &pinsIn,
                             std::vector<TPointD> &pos,
                             const std::vector<int> &preplanted,
                             double maxStepDegrees) {
  if (joints.empty() || pos.size() != joints.size() || pinsIn.empty()) return;

  const double maxStepRad =
      (maxStepDegrees > 0.0) ? maxStepDegrees * (M_PI / 180.0) : 0.0;

  // Softening length of the lever-arm damping, as a fraction of the longest
  // lever on the limb being solved. Smaller = only the pivots really on top of
  // the pin are held back; larger = the bend is pushed further up the limb.
  // The single knob to turn if the multi-pin pose feels too loose or too stiff.
  const double kLeverSoftenFrac = 0.25;

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
      for (int sweep = 0; sweep < SWEEPS; ++sweep) {
        if (norm2(target - pos[pinJ]) < tol2) break;

        // Lever-arm reference for the damping below: the longest lever any
        // pivot on this limb has THIS sweep. Making the softening length
        // relative to it keeps the damping scale-free — the pivot with the
        // best leverage always gets the full step, whether the limb is a
        // finger or a whole stitched body.
        double maxLever = 0.0;
        if (maxStepRad > 0.0)
          for (int k = (int)path.size() - 2; k >= aIdx; --k)
            maxLever =
                std::max(maxLever, sqrt(norm2(pos[pinJ] - pos[path[k]])));
        const double soften = kLeverSoftenFrac * maxLever;

        // Nearest-to-pin pivot first: classic CCD sweep order. The anchor itself
        // (k == aIdx) is a valid LAST pivot: rotating only path[k+1]'s subtree
        // about it bends this limb's attachment bone too — without it, a pose
        // that puts the divergence point out of the limb's reach would tear the
        // pin off with no joint able to compensate.
        for (int k = (int)path.size() - 2; k >= aIdx; --k) {
          const TPointD pivot = pos[path[k]];
          TPointD cur         = pos[pinJ] - pivot;
          TPointD tgt         = target - pivot;
          if (norm2(cur) < 1e-8 || norm2(tgt) < 1e-8) continue;
          double ang = atan2(cross(cur, tgt), cur * tgt);

          // This rotation changes exactly the relative angle of joint path[k+1],
          // so clamp it within that joint's limits: a stiff limb stops at its
          // bound instead of hyper-extending, even when it is the pin PLANTING
          // that bends it. The pin may then simply not reach on this limb.
          const Joint &jvx = joints[path[k + 1]];
          if (jvx.minAngle > -1e9 || jvx.maxAngle < 1e9) {
            double curRel =
                solverRelAngleDeg(joints, pos, rest, path[k + 1]);
            double lo = (jvx.minAngle - curRel) * (M_PI / 180.0);
            double hi = (jvx.maxAngle - curRel) * (M_PI / 180.0);
            ang       = std::min(std::max(ang, lo), hi);
          }

          // Damped CCD (see the header), scaled by the LEVER ARM.
          //
          // A uniform clamp is not enough. The angle CCD wants here is
          // atan2 over `cur` — the vector from this pivot to the pin — so the
          // shorter that vector, the larger the angle for the same target
          // displacement, up to ~180deg for a pivot sitting on the pin. Giving
          // every pivot the same budget therefore lets the ones nearest the
          // pin fold the limb onto itself (24 sweeps x the full step = 360deg)
          // while the pin barely moves: the hold looks kept, the pose explodes.
          //
          // So the budget follows the leverage: lever/(lever + soften),
          // renormalised so the best-placed pivot gets the whole step. Pivots
          // close to the pin are nearly frozen, the bend migrates up the limb
          // to the joints that can actually produce the motion, and the pose
          // moves the way a limb does — from the shoulder, not from the wrist.
          if (maxStepRad > 0.0) {
            double limit = maxStepRad;
            if (soften > 1e-12) {
              const double lever = sqrt(norm2(cur));
              limit *= (lever / (lever + soften)) * (1.0 + kLeverSoftenFrac);
            }
            ang = std::min(std::max(ang, -limit), limit);
          }

          double c = cos(ang), s = sin(ang);
          std::vector<int> sub;
          solverCollectSubtree(children, path[k + 1], sub);
          for (int v : sub) {
            TPointD d = pos[v] - pivot;
            pos[v] = pivot + TPointD(c * d.x - s * d.y, s * d.x + c * d.y);
          }
        }
      }

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

  // NOTE: the planting below is NOT gated by pinsEnabled — the flag only
  // puts the pins to sleep in the TOOL UI (no diamonds, no pin manipulation).
  // Removing the planting from the evaluation would shift the whole pose the
  // moment IK mode is toggled, and viewer/render would diverge from the
  // animation as authored.

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

  // The evaluation result changes: invalidate the associated deformers
  PlasticDeformerStorage::instance()->invalidateDeformation(
      this, PlasticDeformerStorage::NONE);
}

//------------------------------------------------------------------

bool PlasticSkeletonDeformation::pinsEnabled() const {
  return m_imp->m_pinsEnabled;
}

//------------------------------------------------------------------

void PlasticSkeletonDeformation::updatePosition(
    const PlasticSkeleton &originalSkeleton, PlasticSkeleton &deformedSkeleton,
    double frame, int v, const TPointD &pos) {
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
    double frame, int v, const TPointD &pos) {
  const PlasticSkeletonVertex &vx = deformedSkeleton.vertex(v);
  int vParent                     = vx.parent();

  const TPointD &vParentPos = deformedSkeleton.vertex(vParent).P();

  // No need to access the grandParent, we're making the diff against the old vx
  // position

  SkVD &vd = m_imp->m_vds.find(vx.name())->m_vd;

  double loLim, hiLim;
  effAngleLimits(&vd, vx, frame, loLim, hiLim);

  double aDelta = tcg::point_ops::angle(vx.P() - vParentPos, pos - vParentPos) *
                  M_180_PI,
         a = tcrop(vd.m_params[SkVD::ANGLE]->getValue(frame) + aDelta, loLim,
                   hiLim);

  vd.m_params[SkVD::ANGLE]->setValue(frame, a);

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
