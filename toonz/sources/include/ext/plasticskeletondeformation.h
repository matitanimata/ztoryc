#pragma once

#ifndef PLASTICSKELETONDEFORMATION_H
#define PLASTICSKELETONDEFORMATION_H

#include <memory>
#include <functional>

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
DVAPI void plant(const std::vector<Joint> &joints, const std::vector<Pin> &pins,
                 std::vector<TPointD> &pos,
                 const std::vector<int> &preplanted = std::vector<int>());

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

protected:
  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

private:
  friend class PlasticSkeleton;

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
