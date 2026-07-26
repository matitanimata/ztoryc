#pragma once

#ifndef PLASTICSKELETONDEFORMATION_H
#define PLASTICSKELETONDEFORMATION_H

#include <memory>
#include <functional>
#include <set>

// TnzCore includes
#include "tsmartpointer.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"

// TnzExt includes
#include "ext/plastichandle.h"
#include "ext/plasticskeleton.h"

// Qt includes
#include <QString>

#include <boost/bimap.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/iterator/transform_iterator.hpp>

#undef DVAPI
#undef DVVAR
#ifdef TNZEXT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**************************************************************************************
//    SuperPlastic pin solver
//**************************************************************************************

//! Plants IK pins on an articulated structure, in ONE space.
//!
//! Extracted verbatim from the single-column planting that used to live inside
//! PlasticSkeletonDeformation::storeDeformedSkeleton, so the multi-column case
//! can reuse it instead of growing a second, competing implementation — which is
//! exactly how the cross-column planting went wrong before (two mechanisms, each
//! translating the character, fighting on every support switch).
//!
//! The caller decides what "one space" means: a single column's skeleton space,
//! or a whole stitched character mapped into the scene. Feed it the character as
//! a single joint tree and it behaves the way a single-level rig does — which is
//! the whole point.
namespace PlasticPinSolver {

struct Joint {
  int parent = -1;            //!< parent joint index, -1 for the root
  TPointD rest;               //!< rest position, same space as the positions
  double minAngle = -1.0e10,  //!< joint limits in DEGREES, wide open when unset
      maxAngle    = 1.0e10;
};

struct Pin {
  int joint = -1;
  TPointD target;      //!< where the joint must land, same space as positions
  double since = 0.0;  //!< activation frame — seniority picks the primary
};

//! Plants \p pins by moving \p pos in place.
//!
//! The OLDEST pin is planted by rigidly translating the whole structure onto its
//! target, leaving the root free and the authored pose untouched. Every further
//! pin plants by bending ONLY its own limb (CCD confined strictly below the
//! point where its chain diverges from the ones already planted, so an
//! already-planted pin can never move). Finally, pins that cannot be reached at
//! all pull the whole structure toward their damped average residual, a few
//! passes, so the body settles where every pin stays ~planted instead of one of
//! them detaching.
//!
//! \p preplanted lists joints already held by an EXTERNAL mechanism. When it is
//! non-empty the rigid translation is skipped (someone else owns it), those
//! chains seed the "do not touch" set, and every pin bends its own limb. Keep it
//! empty to get the self-contained behaviour described above.
//!
//! \p maxStepDegrees > 0 enables DAMPED CCD: each joint's rotation is clamped
//! to that many degrees per sweep. Reachability is preserved (sweeps
//! accumulate), but the bend spreads along the chain instead of whipping the
//! pivot nearest to the pin, and the solution varies continuously with the
//! target. Meant for the UNIFIED multi-column tree, whose chains are 2-3x
//! longer than a single skeleton's and whose stitch bonds are synthetic
//! (no authored limits): plain CCD finds wildly different configurations for
//! nearby targets there. 0 (the default) = classic behaviour, and the proven
//! single-column path stays bit-identical.
DVAPI void plant(const std::vector<Joint> &joints, const std::vector<Pin> &pins,
                 std::vector<TPointD> &pos,
                 const std::vector<int> &preplanted = std::vector<int>(),
                 double maxStepDegrees               = 0.0);

//! DIAGNOSTIC (2026-07-20) — suspends the evaluation-time plant.
//!
//! Hypothesis under test: posing a stitched rig fights itself because TWO
//! different IK solvers act on the same pins. The tool runs FABRIK
//! (solveMultiAnchor) while dragging and writes ANGLEs; evaluation then re-runs
//! this CCD plant over those ANGLEs and lands somewhere else; the next mouse
//! move starts from THAT. A closed loop between two algorithms that answer the
//! same question differently — which is what the snapping looks like.
//!
//! With this on during the drag, FABRIK alone owns the pose. If the snapping
//! disappears, the loop is confirmed and the real fix is to make the drag call
//! plant() too (one solver, as in STEP C.2). If it does NOT disappear, the
//! hypothesis is wrong and this comes straight back out.
//!
//! The flag lives here because tnzext cannot see tnztools; the tool sets it on
//! button-down and clears it on button-up.
DVAPI void setSolveSuspended(bool on);
DVAPI bool isSolveSuspended();

}  // namespace PlasticPinSolver

//====================================================

//    Forward declarations

class ParamsObserver;
class ParamChange;

//====================================================

//**************************************************************************************
//    PlasticSkeletonVertexDeformation  declaration
//**************************************************************************************

//! The deformation of a plastic skeleton vertex.
typedef struct DVAPI PlasticSkeletonVertexDeformation final : public TPersist {
  PERSIST_DECLARATION(PlasticSkeletonVertexDeformation)

public:
  enum Params {
    ANGLE = 0,  //!< Distance from parent vertex (delta)
    DISTANCE,   //!< Angle with parent edge (delta)
    SO,         //!< Vertex's stacking order
    PIN,        //!< SuperPlastic IK anchor: >=0.5 means this vertex is pinned
                //!< (kept fixed) while solving the chain at the current frame.
                //!< Keyframeable/step-interpolated so the anchor can switch
                //!< over time (e.g. the support foot in a walk cycle).
    PINTX,      //!< SuperPlastic pin TARGET position (local space) for this
    PINTY,      //!< vertex. Meaningful only where PIN>=0.5. At evaluation the
                //!< whole skeleton is translated so the pinned vertex lands on
                //!< (PINTX,PINTY) — a true per-frame constraint, so the pin
                //!< stays planted on in-between frames, not just on keyframes.
    PINWX,      //!< SuperPlastic cross-column pin target, in SCENE (stage
    PINWY,      //!< placement) space. Set only for pins on a stitched CHILD
                //!< column (whose local PINTX plant cannot hold a world spot —
                //!< the column is glued to its parent). Read by the STAGE-level
                //!< per-frame correction (TStageObject::computeLocalPlacement of
                //!< the chain's top column), which pre-translates the whole
                //!< character so this vertex lands on (PINWX,PINWY) — the
                //!< cross-column analogue of the PINTX plant, making the pin
                //!< hold on in-between frames. Ignored by storeDeformedSkeleton.
    ROOTX,      //!< SuperPlastic: keyframeable offset of the skeleton ROOT from
    ROOTY,      //!< its rest position (stored on the root vertex's own
                //!< deformation; default 0 = root stays at rest, old scenes
                //!< unaffected). The root has no ANGLE/DISTANCE of its own, so
                //!< without this a pin-driven rotation of the near-pin segment
                //!< that sweeps the root along as a rigid passenger (e.g.
                //!< dragging a joint with the root one bone further, pivoting
                //!< around a pin beyond it) has nowhere to record the root's
                //!< new position — the drag becomes a silent no-op. Applied in
                //!< updateBranchPositions BEFORE the per-frame PINTX/PINTY
                //!< plant, so the pin still holds exactly regardless.
    SCALEX,     //!< SuperPlastic squash & stretch: scale FACTORS (1.0 = 100%
    SCALEY,     //!< = neutral; custom default handling in save/touchParams
                //!< keeps untouched scales unserialized). Stored on the ROOT
                //!< vertex's deformation. NOT part of the skeleton evaluation:
                //!< the scale is a CONTROLLER on top of it — an affine
                //!< composed into the drawing/render transforms (see
                //!< getSquashControllerAffine) — so pins, IK and pose
                //!< manipulation never interact with it.
    PIVOTX,     //!< Pivot of the squash & stretch controller, stored on the
    PIVOTY,     //!< ROOT vertex's deformation as a keyframeable OFFSET from
                //!< the DEFORMED root position: the pivot follows the
                //!< character and can be moved/animated (default 0 = pivot on
                //!< the root vertex).
    TRANSX,     //!< Controller position offset (composed after the
    TRANSY,     //!< pivot-anchored transform).
    ROT,        //!< Controller rotation (degrees) about the pivot.
    SHEARX,     //!< Controller shear about the pivot.
    SHEARY,
    MINANGLE,   //!< Keyframeable OVERRIDE of the vertex's static min/max angle
    MAXANGLE,   //!< limit: active only where it has keys (so joint limits can
                //!< change during the animation), otherwise the static
                //!< PlasticSkeletonVertex limit is used. Default 0, unused
                //!< until keyed → old scenes and un-keyed joints unaffected.
    PARAMS_COUNT
  };

  struct Keyframe {
    TDoubleKeyframe m_keyframes[PARAMS_COUNT];
  };

  //! ZtoRig: the params that describe a POSE, and only those — the shared
  //! definition behind Global Key, pose recording and pose blending. PIN /
  //! PINT* / PINW* are excluded because their PRESENCE is a semantic switch
  //! (planting authority), and MINANGLE / MAXANGLE because they are joint
  //! limits, not shape: keying or blending either family would silently
  //! re-wire the rig instead of moving it.
  static const int POSE_PARAMS[];
  static const int POSE_PARAMS_COUNT;
  //! Index of \p param inside POSE_PARAMS, or -1 when it is not a pose param.
  static int poseParamSlot(int param);

public:
  TDoubleParamP m_params[PARAMS_COUNT];

public:
  Keyframe getKeyframe(double frame) const;

  void setKeyframe(double frame);
  bool setKeyframe(const Keyframe &values);
  bool setKeyframe(const Keyframe &values, double frame, double easeIn = -1.0,
                   double easeOut = -1.0);

  bool isKeyframe(double frame) const;
  bool isFullKeyframe(double frame) const;
  void deleteKeyframe(double frame);

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

} SkVD;

struct VDKey {
  QString m_name;
  int m_hookNumber;

  //!< Skeleton index to Vertex index map
  mutable std::map<int, int> m_vIndices;
  mutable SkVD m_vd;
};

using SkVDSet = boost::multi_index_container<
  VDKey,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<QString>,
      boost::multi_index::member<VDKey, QString, &VDKey::m_name>
    >,
    boost::multi_index::ordered_unique<
      boost::multi_index::tag<int>,
      boost::multi_index::member<VDKey, int, &VDKey::m_hookNumber>
    >
  >
>;

using SkVDByHookNumber = SkVDSet::index<int>::type;

//**************************************************************************************
//    PlasticSkeletonDeformationKeyframe  declaration
//**************************************************************************************

//! The keyframe of a plastic skeleton vertex deformation.
/*!
  \note A deformation keyframe stores vertex deformation keyframes by vertex \a
  names.
  This is the approach we use to deal with keyframe pasting to different
  skeletons.
*/
typedef struct PlasticSkeletonDeformationKeyframe {
  std::map<QString, SkVD::Keyframe>
      m_vertexKeyframes;             //!< Keyframes by vertex \a name
  TDoubleKeyframe m_skelIdKeyframe;  //!< Skeleton id keyframe

} SkDKey;

//**************************************************************************************
//    PoseAction  declaration
//**************************************************************************************

//! ZtoRig: one recallable action — a named pose delta, plus the curve that
//! records how much of it is applied at each frame.
/*!
  An action is STAMPED, not blended: applyPoseStrength() writes the pose
  straight into the plastic params as real keys, so the slider IS the
  animation and evaluation never has to know actions exist.

  \code
    param(frame) := neutral + strength * delta[param]     // absolute Pose
    param(frame) := base    + strength * delta[param]     // additive Offset
  \endcode

  \note Deltas are stored BY VERTEX NAME, like keyframes are (see SkDKey), and
  never by vertex index: that is what lets an action be copied to a skeleton
  whose internal vertex numbering differs.
*/
struct PoseAction {
  QString m_name;  //!< Action name, unique within the deformation

  //! How the action is stamped. Three modes, and the difference that matters is
  //! RECALL (land on the recorded value, wherever you started) vs PUSH (add to
  //! wherever you are), crossed with how much of the skeleton is claimed:
  //!
  //! - ADD — push, on its own params only. Stacks with anything, but the
  //!   result depends on the starting pose: the same strength gives a different
  //!   shape depending on what came before.
  //! - POSE — recall, over the WHOLE skeleton: params the action never moved go
  //!   to the base, which is what makes two Poses mutually exclusive. Right for
  //!   a full-body pose, wrong for a per-element controller (it would wipe it).
  //! - PART — recall, over its OWN params only. A mouth shape lands exactly as
  //!   recorded no matter which shape preceded it, and leaves the eyes alone.
  //!   This is what lip-sync phonemes and per-limb poses need: an ADD "A"
  //!   after an "O" gives "O+A", never the "A" that was recorded.
  enum Mode { ADD = 0, POSE = 1, PART = 2 };
  int m_mode = ADD;

  //! True when the action RECALLS its recorded values (POSE and PART) rather
  //! than pushing from wherever the character is (ADD).
  bool isAbsolute() const { return m_mode != ADD; }
  //! True when the action claims only the params it recorded, leaving the rest
  //! of the skeleton to whoever else is driving it.
  bool isPartial() const { return m_mode == PART; }

  //! How much of the action IS applied, per frame — a record of what the
  //! stamping wrote, not a driver of the deformation. A keyframeable param on
  //! purpose: it inherits interpolation, Constant segments, undo and the
  //! function editor without a line of new code. 0 = action not applied.
  //! Named "guide" for the serialized tag's sake: it used to drive a runtime
  //! blend, superseded by stamping the pose into the params themselves.
  TDoubleParamP m_guide;

  //! Skeletons this action may be applied to. Vertex names are shared across a
  //! turnaround's views, so a pose is TECHNICALLY replayable on any of them —
  //! but DISTANCE is a length in skeleton space, not a ratio, so replaying a
  //! front pose on the side view lands somewhere nobody authored.
  //! Recording fills this with the skeleton it was authored on; the artist can
  //! widen it, because a pose that only moves matching parts (a blink) really
  //! does work on several views.
  //! EMPTY = no restriction, applies everywhere. That is both the deliberate
  //! "works on all of them" and what actions recorded before this existed get.
  std::set<int> m_skelIds;

  //! Whether the action may be applied on skeleton \p skelId.
  bool appliesTo(int skelId) const {
    return m_skelIds.empty() || m_skelIds.count(skelId) > 0;
  }

  //! This action is the ZERO of its skeleton: an absolute Pose interpolates
  //! from HERE instead of from the skeleton's real rest pose. That rest is the
  //! EXPLODED layout on an exploded rig — limbs scattered — so dialling a pose
  //! to 0 would take the character apart. Record the assembled pose once, mark
  //! it Base, and 0 means assembled. At most one per skeleton.
  bool m_isBase = false;

  //! Deltas from the REST pose, by vertex name. Each entry is
  //! SkVD::POSE_PARAMS_COUNT long and is indexed by SkVD::poseParamSlot() —
  //! not by the raw param enum, which is why poseParamSlot() exists.
  std::map<QString, std::vector<double>> m_deltas;

  PoseAction() : m_guide(new TDoubleParam(0.0)) {}
  explicit PoseAction(const QString &name)
      : m_name(name), m_guide(new TDoubleParam(0.0)) {}

  //! Delta of \p param on \p vertexName at rest-relative scale, 0 when the
  //! action does not touch it.
  double delta(const QString &vertexName, int param) const;
  //! Sets the delta, creating the vertex entry on demand.
  void setDelta(const QString &vertexName, int param, double value);
};

//**************************************************************************************
//    MeshCorrective  declaration
//**************************************************************************************

//! ZtoRig joint corrective (pose-space deformation / SmartSkin): a per-mesh-
//! vertex shape fix that fades in with a driving joint's bend angle.
/*!
  Where a PoseAction drives SKELETON pose params, a MeshCorrective drives the
  MESH itself, applied AFTER the ARAP solve. The ARAP deformer alone pinches a
  sharp bend (one handle at the joint, no volume term); this stores the shape
  the artist sculpted at a given bend and blends it back in by angle:

  \code
    mesh_out[v] += SUM over correctives of  delta[v] * weight(driverAngle)
    weight(a)    = clamp01((a - restAngle) / (fullAngle - restAngle))
  \endcode

  
ote The offsets live in mesh output space (post-deform), keyed by mesh index
  and vertex INDEX — a corrective is sculpted for THIS drawing's mesh and does
  not transfer, unlike pose deltas which key by vertex name.

  
ote No self-loop: the delta touches only the mesh, never the joint ANGLE
  that drives it, and weight() reads the joint's BASE angle. Same rule that
  keeps pins from oscillating.
*/
struct MeshCorrective {
  QString m_name;              //!< Unique within the deformation
  QString m_driverVertexName;  //!< Skeleton vertex whose ANGLE drives the fade
  double m_restAngle = 0.0;    //!< Angle at which the corrective is off (0)
  double m_fullAngle = 0.0;    //!< Angle at which it is fully applied (1)

  //! Per-vertex offsets, [meshIndex][vertexIndex] -> (dx, dy), in mesh output
  //! space. Sparse: only sculpted vertices are stored.
  std::map<int, std::map<int, TPointD>> m_deltas;

  MeshCorrective() {}
  explicit MeshCorrective(const QString &name) : m_name(name) {}

  //! Fade weight at \p driverAngle, clamped to [0,1]. 0 when the range is
  //! degenerate (rest == full), so a half-built corrective stays inert.
  double weight(double driverAngle) const;
  //! Offset of vertex \p v in mesh \p meshIdx, or (0,0) when untouched.
  TPointD delta(int meshIdx, int v) const;
  void setDelta(int meshIdx, int v, const TPointD &d);
};

//**************************************************************************************
//    PlasticSkeletonDeformation  declaration
//**************************************************************************************

/*!
  PlasticSkeletonDeformation models the deformation of a group of
PlasticSkeleton instances.

\par Description

  A PlasticSkeleton instance is typically used to act as a deformable object -
in other
  words, it defines the 'original' form of a hierarchy of vertices that can be
manipulated
  interactively to obtain a 'deformed' configuration.
  \n\n
  PlasticSkeletonDeformation represents a deformation of PlasticSkeleton
objects,
  therefore acting primarily as a collection of PlasticSkeletonVertexDeformation
  instances - one per skeleton vertex. The collection is an associative
container mapping a
  <I> vertex name <\I> to its deformation; using names as keys is a useful
abstraction
  that allows vertex deformations data (eg keyframes) to be copied to skeleton
deformations
  whose skeletons have a different internal configuration (ie vertex indices and
such).
  \n\n
  Each vertex deformation also stores a unique <I> hook number <\I> that can be
used during
  xsheet animation to link stage objects to a skeleton vertex.

\par The Skeletons group

  The PlasticSkeletonDeformation implementation has been extended to work on
multiple
  skeletons for one class instance. This class stores a map associating skeleton
indices
  to the skeleton instances, that can be used to select a skeleton to be
deformed with
  the deformation's data.
  \n\n
  Vertices in different skeleton instances share the same animation if their
name is the same.
  \n\n
  This class provides an animatable parameter that is intended to choose the \a
active
  skeleton along an xsheet timeline. It is retrievable through the
skeletonIdsParam() method.

\par Notable implementation details

  In current implementation, a PlasticSkeletonDeformation keeps shared ownership
of the
  skeletons it is attached to. It is therefore intended to be a \a container of
said skeletons.
*/
class DVAPI PlasticSkeletonDeformation final : public TSmartObject,
                                               public TPersist {
  DECLARE_CLASS_CODE
  PERSIST_DECLARATION(PlasticSkeletonDeformation)

private:
  class Imp;
  std::unique_ptr<Imp> m_imp;

public:
  using SkeletonSet = boost::bimap<int, PlasticSkeletonP>;
  using skelId_iterator = boost::iterators::transform_iterator<
    std::function<int (const SkeletonSet::left_map::value_type &)>,
    SkeletonSet::left_map::iterator
  >;
  using vd_iterator = boost::iterators::transform_iterator<
    std::function<std::pair<const QString *, SkVD *> (const VDKey &)>,
    SkVDSet::iterator
  >;
  using vx_iterator = boost::iterators::transform_iterator<
    std::function<std::pair<int, int> (const std::pair<int, int> &)>,
    std::map<int, int>::iterator
  >;

public:
  PlasticSkeletonDeformation();  //!< Constructs an empty deformation
  PlasticSkeletonDeformation(
      const PlasticSkeletonDeformation
          &other);  //!< Constructs a deformation \a cloning other's skeletons
  ~PlasticSkeletonDeformation();

  PlasticSkeletonDeformation &operator=(
      const PlasticSkeletonDeformation &other);

  // Skeleton-related methods

  bool empty() const;
  int skeletonsCount() const;

  //! Acquires <I> shared ownership <\I> of the specified skeleton, under given
  //! skeletonId
  void attach(int skeletonId, PlasticSkeleton *skeleton);

  //! Releases the skeleton associated to specified skeletonId
  void detach(int skeletonId);

  PlasticSkeletonP skeleton(int skeletonId) const;
  int skeletonId(PlasticSkeleton *skeleton) const;

  //! Returns the ordered range containing the skeleton ids
  void skeletonIds(skelId_iterator &begin, skelId_iterator &end) const;

  TDoubleParamP skeletonIdsParam()
      const;  //!< Returns the skeleton id by frame animatable parameter

  PlasticSkeletonP skeleton(
      double frame) const;  //!< Returns the \a active skeleton by xsheet frame
  int skeletonId(double frame)
      const;  //!< Returns the \a active skeleton id by xsheet frame

  // Vertex deformations-related methods

  int vertexDeformationsCount() const;

  SkVD *vertexDeformation(const QString &vertexName)
      const;  //!< Returns the vertex deformation associated to given
              //!< vertex name. The returned pointer is <I> owned by
              //!< the deformation - it must \b not be deleted <\I>
  SkVD *vertexDeformation(int skelId, int v) const;

  void vertexDeformations(vd_iterator &begin, vd_iterator &end)
      const;  //!< Returns the ordered range of vertex deformations

  void vdSkeletonVertices(const QString &vertexName, vx_iterator &begin,
                          vx_iterator &end)
      const;  //!< Returns the ordered range of skeleton vertices
              //!< (at max one per skeleton id) associated to a
              //!< vertex name
  // Hook number-related methods

  int hookNumber(const QString &name) const;
  int hookNumber(int skelId, int v) const;

  QString vertexName(int hookNumber) const;
  int vertexIndex(int hookNumber, int skelId) const;

  // Parameters-related methods

  void addObserver(TParamObserver *observer);
  void removeObserver(TParamObserver *observer);

  void setGrammar(TSyntax::Grammar *grammar);

  // Keyframes-related methods

  void getKeyframeAt(double frame, SkDKey &keysMap)
      const;  //!< \note keysMap returned by argument to avoid map
              //!< copies in case move semantics is not available
  void setKeyframe(double frame);
  bool setKeyframe(const SkDKey &keyframe);
  bool setKeyframe(const SkDKey &keyframe, double frame, double easeIn = -1.0,
                   double easeOut = -1.0);

  bool isKeyframe(double frame) const;
  bool isFullKeyframe(double frame) const;
  void deleteKeyframe(double frame);

  // SuperPlastic STEP C.2 — secondary cross-column pins.
  //
  // A pin carrying a SCENE target (PINW) is planted by the character-level
  // correction, which is a pure translation and therefore satisfies exactly ONE
  // pin. Every further pin has to plant by BENDING its own limb, and CCD needs
  // its target in this skeleton's LOCAL space — which requires the column's
  // local->scene affine, something this class (in tnzext) cannot and must not
  // know about: TStageObject lives in the layer ABOVE.
  //
  // So the stage side pushes the data down. TStageObject::
  // computePlasticPinCorrection already composes each column's local->scene
  // affine while looking for the primary pin, so it maps the secondary targets
  // into local space and hands them over here, keyed by vertex index. Purely
  // transient: never serialized, never observed, dropped on any frame change.
  void setSecondaryPinTargets(double frame,
                              const std::map<int, TPointD> &localTargets);
  void clearSecondaryPinTargets();

  //! The posed skeleton WITHOUT any pin planting — storeDeformedSkeleton minus
  //! its final plant. The multi-column solve needs the raw pose: it plants the
  //! whole character itself, on the unified joint tree.
  void storePosedSkeleton(int skeletonId, double frame,
                          PlasticSkeleton &skeleton) const;

  //! One solver Joint per vertex of this skeleton, in DENSE order (the tcg slot
  //! of each is returned in \p vertexIds). Parent indices are dense and local to
  //! this skeleton — the caller re-parents the root when stitching several
  //! columns into one character. Rest positions and the effective angle limits
  //! at \p frame come from here so the limit rules stay in one place.
  void buildSolverJoints(int skeletonId, double frame,
                         std::vector<PlasticPinSolver::Joint> &joints,
                         std::vector<int> &vertexIds) const;

  //! Result of a character-level solve for this column, in this column's own
  //! skeleton space. While one is set for the requested (skeletonId, frame),
  //! storeDeformedSkeleton returns it verbatim instead of planting locally:
  //! a stitched character has ONE solver, and this is where its answer lands.
  void setSolvedSkeleton(int skeletonId, double frame,
                         const PlasticSkeleton &skeleton);
  void clearSolvedSkeleton();

  // Interface methods using a deformed copy of the original skeleton (which is
  // owned by this class)

  //! How far the worst pin would end up from its target if the skeleton were
  //! posed as \p posed at \p frame — measured by running the EVALUATION's own
  //! plant on it.
  /*!
    This exists so the drag can ask the question it actually needs answered:
    "if I move the body here, do the pins still hold?" — and get the answer from
    the solver that will really decide, not from a second one that agrees only
    approximately. Every time those two have differed, the pins have slipped.
  */
  double pinResidualForPose(int skeletonId, double frame,
                            const PlasticSkeleton &posed) const;

  void storeDeformedSkeleton(int skeletonId, double frame,
                             PlasticSkeleton &skeleton) const;

  //! SuperPlastic squash & stretch controller: the affine (in skeleton/world
  //! mesh coordinates) to be composed ON TOP of the deformed result —
  //! T(C)·S·T(-C), where S = the root deformation's SCALEX/SCALEY factors and
  //! C = the DEFORMED root position plus the keyframeable PIVOTX/PIVOTY
  //! offset (so the pivot follows the character). Identity when the scale is
  //! neutral. The skeleton evaluation itself is never affected.
  TAffine getSquashControllerAffine(int skeletonId, double frame) const;

  //! SuperPlastic: whether the pin UI (diamonds, pin-aware manipulation) is
  //! active in the tool. Scene data (serialized; default enabled). NOTE: the
  //! evaluation planting is NEVER gated by this — leaving IK mode releases
  //! the active pins with a full bake instead (tool side), so the authored
  //! animation and the render never change under a UI toggle.
  void enablePins(bool on);
  bool pinsEnabled() const;

  void updatePosition(const PlasticSkeleton &originalSkeleton,
                      PlasticSkeleton &deformedSkeleton, double frame, int v,
                      const TPointD &pos);
  void updateAngle(const PlasticSkeleton &originalSkeleton,
                   PlasticSkeleton &deformedSkeleton, double frame, int v,
                   const TPointD &pos);

  //! \name ZtoRig pose actions
  //@{

  int poseActionsCount() const;
  //! Action by index, or nullptr when out of range.
  const PoseAction *poseAction(int idx) const;
  PoseAction *poseAction(int idx);
  //! Action by name, or nullptr when there is none.
  PoseAction *poseAction(const QString &name);
  //! Adds an action, or returns the index of the existing one with that name.
  int addPoseAction(const QString &name);
  void removePoseAction(int idx);

  //! Whole-vector snapshot / restore, for undo. Copies share each action's
  //! guide TDoubleParam by pointer — intended: the guide is a live object the
  //! function editor may reference, and restoring the SAME object keeps those
  //! references valid.
  std::vector<PoseAction> getPoseActions() const;
  void setPoseActions(const std::vector<PoseAction> &actions);

  //! Snapshot of everything a stamp touches: the pose-param KEYFRAMES of every
  //! vertex, plus every action's guide curve. One blob so applying a pose (and
  //! the exclusive zeroing of the other pose dials) is a single undo step.
  struct PoseKeyState {
    // vertexName -> pose param enum -> its keyframes
    std::map<QString, std::map<int, std::vector<TDoubleKeyframe>>> m_paramKeys;
    // per action (same order as getPoseActions): guide keyframes + default
    std::vector<std::pair<std::vector<TDoubleKeyframe>, double>> m_guides;
  };
  PoseKeyState getPoseKeyState() const;
  void setPoseKeyState(const PoseKeyState &s);

  //! The action acting as the zero of skeleton \p skelId, or null when that
  //! skeleton has none and the real rest pose is the zero. (The base is looked
  //! up per skeleton, so each view of a turnaround can have its own assembled
  //! pose while sharing the actions that work on both.)
  const PoseAction *baseActionOf(int skelId) const;

  //! Value param \p param of \p vertexName has at strength 0: the base action's
  //! if the skeleton has one, the real neutral otherwise.
  double poseBaseValue(const QString &vertexName, int param, int skelId) const;

  //! Makes action \p idx the zero of its skeleton (or clears the flag). Only
  //! one action per skeleton can be the base, so this clears the others.
  void setPoseActionAsBase(int idx, bool isBase);

  //! Whether action \p idx can be applied at \p frame, i.e. whether it was
  //! recorded on the skeleton that is active there. Unbound (legacy) actions
  //! apply everywhere.
  bool poseActionAppliesAt(int idx, double frame) const;

  //! Strength of the action at \p frame, 0 at rest and 1 in the recorded pose,
  //! read back from the RECORD written by applyPoseStrength — never re-derived
  //! from the params, which cannot tell two actions sharing a param apart. Lets
  //! the slider show where you are so you can dial the pose in AND out.
  double poseStrengthAt(int idx, double frame) const;

  //! Write the action at \p strength straight into keyframes at \p frame:
  //! Pose (absolute) -> neutral + strength*delta (0 = rest, 1 = recorded);
  //! Add (additive) -> base + strength*delta. This is the auto-stamp behind
  //! the slider: the keys ARE the animation. Also records \p strength so
  //! poseStrengthAt() can read it back, and — for an absolute Pose, which
  //! overwrites the whole skeleton — zeroes the other actions' records.
  void applyPoseStrength(int idx, double strength, double frame);

  //! Open/close an Add drag. An Add is <TT>base + strength*delta</TT>, and
  //! the slider calls applyPoseStrength on EVERY move: without a base frozen at
  //! the start of the gesture each move would read back its own previous write
  //! and add to it, so the offset compounds and the character shoots away after
  //! a few pixels of slider travel. beginPoseDrag() snapshots that base once;
  //! endPoseDrag() drops it. Harmless for an absolute Pose, which never reads
  //! the live value.
  void beginPoseDrag(int idx, double frame);
  void endPoseDrag();

  //! \name ZtoRig mesh correctives (joint pose-space deformation)
  //@{

  int meshCorrectivesCount() const;
  const MeshCorrective *meshCorrective(int idx) const;
  MeshCorrective *meshCorrective(int idx);
  MeshCorrective *meshCorrective(const QString &name);
  int addMeshCorrective(const QString &name);
  void removeMeshCorrective(int idx);

  std::vector<MeshCorrective> getMeshCorrectives() const;
  void setMeshCorrectives(const std::vector<MeshCorrective> &correctives);

  //! Total mesh-space offset for vertex \p v of mesh \p meshIdx at \p frame:
  //! the sum over correctives of delta * weight(driver base angle). Added to
  //! the ARAP result by the deformer. (0,0) when no corrective touches it.
  TPointD meshCorrectiveOffset(int meshIdx, int v, double frame) const;

  //@}

  //! Records the pose authored at \p frame as an action named \p name,
  //! returning its index; an existing action with that name is re-recorded.
  /*!
    Deltas are taken from the BASE params only, never from the blended result:
    recording while other dials are up captures what the animator authored,
    not the other actions' contribution — otherwise actions would accumulate
    each other and re-recording would drift.

    The base animation is left untouched, and the new action's guide starts at
    0, so recording changes nothing on screen. To check an action: pose, record,
    undo the pose, then raise the guide — the pose must come back.
  */
  int recordPoseAction(const QString &name, double frame);

  //! Value a pose param has at rest: 1 for the scale factors, 0 for every
  //! other channel. Deltas are stored relative to this.
  static double poseParamNeutral(int param);

  //@}

protected:
  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

private:
  friend class PlasticSkeleton;

  //! Collects this column's active pins and hands them to the solver, moving
  //! \p skeleton in place. The single place that knows how a pin becomes a
  //! solver constraint — storeDeformedSkeleton and pinResidualForPose both go
  //! through here, so the drag and the evaluation cannot drift apart.
  //! \p worstResidual2, when given, receives the worst squared miss.
  void plantPins(int skelId, double frame, PlasticSkeleton &skeleton,
                 double *worstResidual2) const;

  void addVertex(
      PlasticSkeleton *sk,
      int v);  //!< Deals with vertex deformations when v has been added
  void insertVertex(PlasticSkeleton *sk, int v);  //!< Deals with vertex
                                                  //! deformations when v has
  //! been inserted in an edge
  void deleteVertex(
      PlasticSkeleton *sk,
      int v);  //!< Removes vertex deformation for v, \a before it is deleted
  void vertexNameChange(
      PlasticSkeleton *sk, int v,
      const QString &newName);      //!< Rebinds a vertex deformation name
  void clear(PlasticSkeleton *sk);  //!< Clears all vertex deformations

  void loadData_prerelease(
      TIStream &is);  // Toonz 7.0 pre-release loading function. Will be deleted
                      // in the next minor release.
};

typedef PlasticSkeletonDeformation SkD;

//===============================================================================

#ifdef _WIN32
#ifndef TFX_EXPORTS
template class DVAPI TSmartPointerT<PlasticSkeletonDeformation>;
#endif
#endif

typedef TSmartPointerT<PlasticSkeletonDeformation> PlasticSkeletonDeformationP;
typedef PlasticSkeletonDeformationP SkDP;

#endif  // PLASTICSKELETONDEFORMATION_H
