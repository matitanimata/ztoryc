#pragma once

#ifndef PLASTICTOOL_H
#define PLASTICTOOL_H

// TnzCore includes
#include "tproperty.h"
#include "tmeshimage.h"
#include "tooloptionscontrols.h"

// TnzBase includes
#include "tparamchange.h"
#include "tdoubleparamrelayproperty.h"

// TnzExt includes
#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"
#include "ext/plasticvisualsettings.h"

// TnzQt includes
#include "toonzqt/plasticvertexselection.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/tooloptions.h"

// STD includes
#include <memory>
#include <map>
#include <vector>

// tcg includes
#include "tcg/tcg_base.h"
#include "tcg/tcg_controlled_access.h"

#include "../toonz/menubarcommandids.h"

// Qt includes
#include <QObject>

//****************************************************************************************
//    Metric defines
//****************************************************************************************

// Skeleton primitives

#define HIGHLIGHT_DISTANCE 8  // Pixels distance to highlight
#define HANDLE_SIZE 4         // Size of vertex handles
#define HIGHLIGHTED_HANDLE_SIZE HIGHLIGHT_DISTANCE  // Size of handle highlights
#define SELECTED_HANDLE_SIZE                                                   \
  HIGHLIGHTED_HANDLE_SIZE  // Size of handle selections

// Mesh primitives

#define MESH_HIGHLIGHT_DISTANCE 8
#define MESH_HIGHLIGHTED_HANDLE_SIZE 4
#define MESH_SELECTED_HANDLE_SIZE 2

//****************************************************************************************
//    PlasticTool  declaration
//****************************************************************************************

class PlasticTool final : public QObject,
                          public TTool,
                          public TParamObserver,
                          public TSelection::View {
  Q_OBJECT

  friend class PlasticToolOptionsBox;

public:
  class TemporaryActivation {
    bool m_activate;

  public:
    TemporaryActivation(int row, int col);
    ~TemporaryActivation();
  };

  struct MeshIndex final : public tcg::safe_bool<MeshIndex> {
    int m_meshIdx,  //!< Mesh index in a TMeshImage
        m_idx;      //!< Index in the referenced mesh

    explicit MeshIndex(int meshIdx = -1, int idx = -1)
        : m_meshIdx(meshIdx), m_idx(idx) {}

    bool operator_bool() const { return (m_meshIdx >= 0) && (m_idx >= 0); }

    bool operator<(const MeshIndex &other) const {
      return (m_meshIdx == other.m_meshIdx) ? (m_idx < other.m_idx)
                                            : (m_meshIdx < other.m_meshIdx);
    }
  };

  typedef MultipleSelection<MeshIndex> MeshSelection;

private:
  PlasticSkeletonDeformationP m_sd;  //!< Current column's skeleton deformation
  int m_skelId;                      //!< Current m_sd's skeleton id
  tcg::invalidable<PlasticSkeleton>
      m_deformedSkeleton;  //!< The interactively-deformed \a animation-mode
                           //! skeleton

  TMeshImageP m_mi;  //!< Current mesh image

  // Property-related vars  (ie, tool options)

  TPropertyGroup
      *m_propGroup;  //!< Tool properties groups (needed for toolbar-building)

  TEnumProperty m_mode;          //!< Editing mode (BUILD, ANIMATE, etc..)
  TStringProperty m_vertexName;  //!< Vertex name property

  TBoolProperty m_interpolate;  //!< Strict vertex interpolation property
  TBoolProperty m_snapToMesh;   //!< Snap to Mesh vertexes during skeleton build

  TDoubleProperty m_thickness;  //!< Brush radius, from 1 to 100
  TEnumProperty
      m_rigidValue;  //!< Rigidity drawing value (ie draw rigidity/flexibility)
                     //!< put a keyframe at current frame

  //! What a Global Key covers on a rigged column: Stage / Plastic / All.
  //! Mirrors the GlobalKeyScope preference — the same combo appears in the
  //! Animate tool, so both always show (and mean) the same thing.
  TEnumProperty m_globalKeyScope;
  TBoolProperty
      m_globalKey;  //!< Whether animating a vertex will cause EVERY vertex to
  TBoolProperty
      m_keepDistance;  //!< Whether animation editing can alter vertex distances
  TBoolProperty
      m_ikDrag;  //!< Whether animation editing solves the whole chain with IK
                 //!< (SuperPlastic CCD solver) instead of single-vertex FK

  //! Largest rotation, in DEGREES, that one mouse event may apply to a joint.
  //! Low = calm and deliberate, high = free (and, near the root, twitchy).
  //! Bounds the angle rather than the travel of the pinned end: the sweep of
  //! the body hanging off a joint is proportional to the angle alone.
  TDoubleProperty m_ikDamping;

  TEnumProperty m_scaleConstraint;  //!< Squash & stretch constraint, like the
                                    //!< Animate tool: None / Aspect Ratio /
                                    //!< Mass (V = 1/H, area preserved)

  //! ZtoRig joint correctives, authoring (milestone 2). Sculpting happens on
  //! the POSED character, so it lives in Animate: bend the joint until the mesh
  //! pinches, then brush the shape back. m_correctiveSculpt turns the overlay
  //! and the brush on; the radius is in world units, matched to the drawing.
  TBoolProperty   m_correctiveSculpt;
  TDoubleProperty m_correctiveRadius;
  TBoolProperty m_showAngleLimits;  //!< Show the draggable angle-limit gizmo
                                    //!< (off by default to keep the skeleton
                                    //!< clean)

  TBoolProperty m_showController;  //!< Show the squash/stretch controller gizmo
                                   //!< (Animate-tool replica on the skeleton);
                                   //!< toggle off so it doesn't get in the way
                                   //!< of vertex manipulation

  TStringProperty m_minAngle,
      m_maxAngle;  //!< Minimum and maximum angle values allowed

  TPropertyGroup m_relayGroup;  //!< Group for each vertex parameter relay

  TDoubleParamRelayProperty
      m_distanceRelay;  //!< Relay property for vertex distance
  TDoubleParamRelayProperty m_angleRelay;  //!< Relay property for vertex angle
  TDoubleParamRelayProperty m_soRelay;     //!< Relay property for vertex so

  TDoubleParamRelayProperty
      m_scaleXRelay,  //!< SuperPlastic squash & stretch relays — bound to the
      m_scaleYRelay;  //!< SELECTED vertex (= scale pivot), delta from 1

  TDoubleParamRelayProperty
      m_skelIdRelay;  //!< Relay property for m_sd's skeleton id

  // Mouse-related vars

  TPointD m_pos;         //!< Last known mouse position
  TPointD m_pressedPos;  //!< Last mouse press position
  bool m_dragged;        //!< Whether dragging occurred between a press/release

  std::vector<TPointD>
      m_pressedVxsPos;   //!< Position of selected vertices at mouse press
  SkDKey m_pressedSkDF;  //!< Skeleton deformation keyframes at mouse press

  // SuperPlastic controller gizmo: a full Animate-tool replica ON TOP of the
  // skeleton (position/rotation/scale/shear about a keyframeable pivot that
  // follows the deformed root), with per-handle hover highlight and dynamic
  // contrast colors like the Ztoryc Animate tool gizmo.
  enum SquashCtrlDevice {
    CtrlNone = 0,
    CtrlPivot,    //!< double circle at the pivot: moves the pivot (snaps)
    CtrlMove,     //!< bottom handle: TRANSX/TRANSY
    CtrlRot,      //!< top handle: ROT about the pivot
    CtrlScale,    //!< bottom-left square: uniform scale (Mass via combo)
    CtrlScaleXY,  //!< offset square: free per-axis scale
    CtrlShear     //!< bottom-right parallelogram: SHEARX/SHEARY
  };
  int m_ctrlDevice    = CtrlNone;  //!< Handle being dragged
  int m_ctrlHighlight = CtrlNone;  //!< Handle under the mouse

  // Angle-limit gizmo: drag the min/max joint bounds directly in the viewer
  int m_limitDrag = 0;  //!< 0 none, 1 min bound, 2 max bound (being dragged)
  int m_limitHi   = 0;  //!< hovered bound (same codes)
  double m_limitOldMin = 0.0,  //!< static bounds at drag start (for the undo)
      m_limitOldMax    = 0.0;
  TPointD m_scaleDragCenter;       //!< Controller pivot position at press
  double m_scaleOldX = 1.0,        //!< Controller values at press time
      m_scaleOldY = 1.0, m_ctrlOldRot = 0.0, m_ctrlOldTX = 0.0,
         m_ctrlOldTY = 0.0, m_ctrlOldShX = 0.0, m_ctrlOldShY = 0.0;
  // Move handle on a STITCHED CHILD column: the drag steers the column's own
  // X/Y (inches, the same channel the Animate tool writes) instead of the
  // controller's TRANSX/TRANSY. The controller is a render-time affine over the
  // mesh only — it would slide the drawing off its attachment while skeleton,
  // placement and any grand-children stayed behind. Column X/Y is summed on top
  // of the parent's handle position in computeLocalPlacement, so the whole child
  // moves coherently: exactly the shoulder/hip sway of a walk.
  bool m_ctrlChildColumn = false;  //!< current column is a stitched child
  double m_ctrlOldColX = 0.0, m_ctrlOldColY = 0.0;  //!< its X/Y at press
  // Controller Move on the TOP column: active cross-column pins must travel
  // with the whole-character translation (single-level precedent: the
  // in-skeleton plant rides the controller). Without this the stage-level hold
  // (PINW re-plant) would cancel the move outright. Snapshot of each active
  // descendant pin's PINW params + value at press; the drag re-keys them
  // shifted by the world delta.
  struct CtrlPinW {
    TDoubleParamP px, py;
    TPointD oldW;
    double keyFrame;
  };
  std::vector<CtrlPinW> m_ctrlPinWSnapshot;
  // Tool matrix at press time. The controller affine is composed into the live
  // tool matrix (updateMatrix during the drag keeps the overlay glued), so
  // mouse positions arrive in a space that MOVES with the very values being
  // written — using them raw makes the delta cancel itself (handle bouncing
  // back / vibrating). Every controller drag therefore re-projects the mouse
  // into this frozen press-time space before computing deltas.
  TAffine m_ctrlPressMatrix;

  // Selection/Highlighting-related vars

  int m_svHigh,                    //!< Highlighted skeleton vertexes
      m_seHigh;                    //!< Highlighted skeleton edges
  PlasticVertexSelection m_svSel;  //!< Selected skeleton vertexes

  MeshIndex m_mvHigh,     //!< Highlighted mesh vertexes
      m_meHigh;           //!< Highlighted mesh edges
  MeshSelection m_mvSel,  //!< Selected mesh vertexes
      m_meSel;            //!< Selected mesh edges

  // Drawing-related vars

  PlasticVisualSettings m_pvs;  //!< Visual options for plastic painting

  // Editing-related vars

  std::unique_ptr<tcg::polymorphic> m_rigidityPainter;  //!< Delegate class to
                                                        //! deal with (undoable)
  //! rigidity painting
  bool m_showSkeletonOS;  //!< Whether onion-skinned skeletons must be shown

  // Deformation-related vars

  bool m_recompileOnMouseRelease;  //!< Whether skeleton recompilation should
                                   //! happen on mouse release

public:
  enum Modes {
    MESH_IDX = 0,
    RIGIDITY_IDX,
    BUILD_IDX,
    ANIMATE_IDX,
    MODES_COUNT
  };

public:
  PlasticTool();
  ~PlasticTool();

  ToolType getToolType() const override;
  int getCursorId() const override { return ToolCursor::SplineEditorCursor; }

  ToolOptionsBox *createOptionsBox() override;

  TPropertyGroup *getProperties(int idx) override { return &m_propGroup[idx]; }

  void updateTranslation() override;

  void onSetViewer() override;

  void onActivate() override;
  void onDeactivate() override;

  void onEnter() override;
  void onLeave() override;

  void addContextMenuItems(QMenu *menu) override;

  void reset() override;

  bool onPropertyChanged(std::string propertyName) override;

public:
  // Methods reimplemented in each interaction mode
  void mouseMove(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &me) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &me) override;

  bool isDragging() const override { return m_dragged; }

  void draw() override;

public:
  // Skeleton methods

  void setSkeletonSelection(const PlasticVertexSelection &vSel);
  void toggleSkeletonSelection(const PlasticVertexSelection &vSel);
  void clearSkeletonSelections();

  const PlasticVertexSelection &skeletonVertexSelection() const {
    return m_svSel;
  }
  PlasticVertexSelection branchSelection(int vIdx) const;

  void moveVertex_build(const std::vector<TPointD> &originalVxsPos,
                        const TPointD &posShift);
  void addVertex(const PlasticSkeletonVertex &vx);
  void insertVertex(const PlasticSkeletonVertex &vx, int e);
  void insertVertex(const PlasticSkeletonVertex &vx, int parent,
                    const std::vector<int> &children);
  void removeVertex();
  void setVertexName(QString &name);

  int addSkeleton(const PlasticSkeletonP &skeleton);
  void addSkeleton(int skelId, const PlasticSkeletonP &skeleton);
  void removeSkeleton(int skelId);

  PlasticSkeletonP skeleton() const;
  void touchSkeleton();

  PlasticSkeletonDeformationP deformation() const { return m_sd; }
  void touchDeformation();

  void storeDeformation();  //!< Stores deformation of current column (copying
                            //! its reference)
  void storeSkeletonId();  //!< Stores current skeleton id associated to current
                           //! deformation

  void onChange();  //!< Updates the tool after a deformation parameter change.
                    //!< It can be used to refresh the tool in ANIMATION mode.
public:
  // Mesh methods

  const MeshSelection &meshVertexesSelection() const { return m_mvSel; }
  const MeshSelection &meshEdgesSelection() const { return m_meSel; }

  void setMeshVertexesSelection(const MeshSelection &vSel);
  void toggleMeshVertexesSelection(const MeshSelection &vSel);

  void setMeshEdgesSelection(const MeshSelection &eSel);
  void toggleMeshEdgesSelection(const MeshSelection &eSel);

  void clearMeshSelections();

  void storeMeshImage();

  void moveVertex_mesh(const std::vector<TPointD> &originalVxsPos,
                       const TPointD &posShift);

public:
  //! Restores the IK checkbox + pin UI state without re-triggering the
  //! clean-release logic (used by the IK-release undo/redo)
  void setIkPinsUiEnabled(bool on);

  //! Column indices of the stitched character the current column belongs to
  //! (itself included), restricted to those carrying a plastic deformation.
  //! Column parenting only — no skeleton evaluation, so it is safe/cheap
  //! outside animate mode.
  std::vector<int> characterColumns() const;

  //! characterColumns(), resolved to their plastic deformations.
  std::vector<PlasticSkeletonDeformationP> characterDeformations() const;

  //! Enable/disable IK pins on the WHOLE character: the flag is a property of
  //! the character, not of one column (see the definition for why).
  void enablePinsOnCharacter(bool on);

  // Actions with associated undo
  int addSkeleton_undo(const PlasticSkeletonP &skeleton);
  void addSkeleton_undo(int skelId, const PlasticSkeletonP &skeleton);

  void removeSkeleton_undo(int skelId);
  void removeSkeleton_withKeyframes_undo(int skelId);

  void editSkelId_undo(int skelId);

public slots:

  void swapEdge_mesh_undo();
  void collapseEdge_mesh_undo();
  void splitEdge_mesh_undo();
  void cutEdges_mesh_undo();

  void deleteSelectedVertex_undo();

  void setKey_undo();
  void setGlobalKey_undo();
  void setRestKey_undo();
  void setGlobalRestKey_undo();

  void copySkeleton();
  void pasteSkeleton_undo();
  void copyDeformation();
  void pasteDeformation_undo();

signals:  // privates

  void skelIdsListChanged();
  void skelIdChanged();

protected:
  void mouseMove_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_mesh(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_mesh(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_mesh(QMenu *menu);

  void mouseMove_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_build(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_build(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_build(QMenu *menu);

  void mouseMove_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_rigidity(const TPointD &pos, const TMouseEvent &me);
  void leftButtonUp_rigidity(const TPointD &pos, const TMouseEvent &me);
  void addContextMenuActions_rigidity(QMenu *menu);

  void mouseMove_animate(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDown_animate(const TPointD &pos, const TMouseEvent &me);
  void leftButtonDrag_animate(const TPointD &pos, const TMouseEvent &me);
  void moveVertexIK_animate(double frame, int v, const TPointD &pos);
  void moveVertexMultiAnchor_animate(double frame, int v, const TPointD &pos,
                                     const std::vector<int> &pins,
                                     const std::map<int, TPointD> &curPos);
  void writeBackAngles_animate(double frame,
                               const std::map<int, TPointD> &curPos,
                               const std::map<int, TPointD> &desired,
                               bool clampToLimits = true);
  // Column-agnostic core of writeBackAngles_animate: convert a rigid
  // desired-positions snapshot (in `def`'s own local space) into that column's
  // root-down ANGLE params. The single-column wrapper above passes the current
  // column; the cross-level solver calls it once per touched column.
  // writeRootOffset: when true, the skeleton root's new position is recorded as
  // its ROOTX/ROOTY offset (used only by the unified IK solver, for the one
  // effective root column that carries the free-root translation). Default false
  // keeps every other caller — single-level and cross-level FK — unchanged: the
  // root has no representable motion there and is simply skipped.
  void writeBackAnglesFor_animate(const PlasticSkeleton &orig, const SkDP &def,
                                  int skelId, double frame,
                                  const std::map<int, TPointD> &curLocal,
                                  const std::map<int, TPointD> &desiredLocal,
                                  bool clampToLimits, bool writeRootOffset = false);
  void leftButtonUp_animate(const TPointD &pos, const TMouseEvent &me);
  void controllerDrag_animate(const TPointD &pos, const TMouseEvent &me);
  void scaleDrag_animate(const TPointD &pos, const TMouseEvent &me);
  void pivotDrag_animate(const TPointD &pos);
  int controllerHitTest_animate(const TPointD &pos);  //!< SquashCtrlDevice
  void drawController_animate(double pixelSize);

  // Angle-limit gizmo helpers
  bool limitDisplay_animate(int v, TPointD &parentPosDef, double &branchAngleRad,
                            double &defaultAngleDeg, double &radius,
                            double &minDisp, double &maxDisp);
  int limitHitTest_animate(const TPointD &pos);  //!< 0 none / 1 min / 2 max
  void limitDrag_animate(const TPointD &pos);
  void drawAngleLimitGizmo_animate(double pixelSize);
  SkVD *rootVd_animate(int *rootIdx = 0);  //!< ROOT vertex's deformation
  bool squashPivot_animate(TPointD &C);    //!< controller pivot (local coords)

  void updateMatrix() override;  //!< composes the squash controller affine

  // SuperPlastic IK anchor (pin). The pinned vertex is kept fixed while IK
  // solving at a given frame; the anchor is keyframeable so it can switch over
  // time (e.g. the support foot in a walk cycle).
  int pinnedVertexAtFrame(double frame) const;  //!< first pin, -1 if none
  std::vector<int> pinnedVerticesAtFrame(double frame) const;  //!< all pins
  //! Smallest frame > `frame` where any vertex's pin turns ON, or -1 if none
  double nextPinActivationAfter_animate(double frame) const;
  //! Leaving IK mode: bake the whole pinned animation into FK + controller so
  //! every keyframe stays exactly in place, then drop the pins (not undoable)
  void bakePinsToFK_animate();
  void togglePinAtCurrentFrame();               //!< pin/unpin selected vertex
  void switchPinAtCurrentFrame();  //!< pin selected, release others at f+1

public:
  //! Restore the IK checkbox + pin visibility (used by the bake undo). Sets
  //! the property with notifyListeners (widget refresh only, no re-trigger).
  void setIkModeState(bool on);

protected:
  void addContextMenuActions_animate(QMenu *menu);

  void draw_mesh();
  void draw_build();
  void draw_rigidity();
  void draw_animate();

private:
  // Skeleton methods

  PlasticSkeleton &deformedSkeleton();
  void updateDeformedSkeleton(PlasticSkeleton &deformedSkeleton);

  // Keyframe methods

  void keyFunc_undo(void (PlasticTool::*keyFunc)());

  void setKey();
  void setGlobalKey();
  void setRestKey();
  void setGlobalRestKey();

  // Rigidity methods
  static std::unique_ptr<tcg::polymorphic> createRigidityPainter();

  // Drawing methods

  void drawSkeleton(const PlasticSkeleton &skel, double pixelSize,
                    UCHAR alpha = 255);
  void drawOnionSkinSkeletons_build(double pixelSize);
  void drawOnionSkinSkeletons_animate(double pixelSize);

  void drawHighlights(const SkDP &sd, const PlasticSkeleton *skel,
                      double pixelSize);
  void drawSelections(const SkDP &sd, const PlasticSkeleton &skel,
                      double pixelSize);

  void drawAngleLimits(const SkDP &sd, int skeId, int v, double pixelSize);

  // SuperPlastic multi-level: the skeletons of the columns hierarchically
  // connected to the current one (via TStageObject parenting) — i.e. the whole
  // articulated character spread across several drawing levels. Each entry
  // carries the column's deformed skeleton plus the affine that maps its own
  // draw space into the current tool's draw space, so the whole character can
  // be drawn/picked as one.
  struct ConnectedSkel {
    int            columnIndex;
    PlasticSkeleton skel;   //!< deformed, in that column's own draw space
    TAffine        toCur;   //!< that column's draw space -> current tool space
  };
  std::vector<ConnectedSkel> connectedSkeletons_animate() const;

  // One cross-level joint: the child column's root vertex is attached (via the
  // column parent handle "H<n>") to vertex `parentVertex` of the parent column.
  // This is the extra edge that stitches the per-level skeletons into one graph
  // for cross-level IK. Positions resolved in the current tool's draw space.
  struct CrossLevelLink {
    int     childColumn, childRootVertex;
    int     parentColumn, parentVertex;
    TPointD childPos, parentPos;  // in current tool draw space
  };
  std::vector<CrossLevelLink> crossLevelLinks_animate();

  //! Reference direction for a joint whose parent is its column's ROOT: the
  //! bone of the PARENT COLUMN it hangs from, at rest and deformed, expressed
  //! in \p column's own space.
  /*!
    A joint angle is meaningful only against its parent bone. Inside one column
    that bone is always there; a column ROOT has no parent bone in its own
    skeleton, and the code fell back to the world X axis. That is a constant, so
    the joint limits of an arm on its own column stopped following the body:
    bend the torso and the arm's usable range slid by exactly the torso's
    rotation — the wedge stayed put against the SCREEN.

    Returns false when there is no parent column, where the caller's old
    behaviour (world X) is the right answer.
  */
  //! How far the parent COLUMN's anchoring bone has turned from rest, in
  //! degrees, for the current column — what the angular limits of a joint
  //! hanging off this skeleton's root must follow so a shoulder's usable range
  //! bends with the torso. Returns 0 unless ZTORYC_BOUND_REF is set: the sign
  //! convention was never established experimentally, so the variable selects
  //! it (1 or -1) and 0/unset keeps the limits as they are today.
  double parentBoneRefDeg_animate() const;
  //! Same, for an explicit column: the cross-level write-back walks several
  //! columns in one go, so it cannot use the current one.
  double parentBoneRefDegFor_animate(int column) const;
  //! Deformed mesh vertices of the current column at the current frame, in the
  //! space the tool draws in. Empty when there is no mesh or no deformation.
  //! This is the data the corrective brush acts on: the positions AFTER the
  //! ARAP solve, which is where a MeshCorrective's offsets live too.
  std::vector<std::pair<MeshIndex, TPointD>> deformedMeshVertices_animate();
  //! Begin/continue/end one brush stroke on the active joint corrective.
  bool beginCorrectiveStroke_animate();
  void applyCorrectiveBrush_animate(const TPointD &from, const TPointD &to);
  void endCorrectiveStroke_animate();

  QString m_correctiveName;   //!< corrective being sculpted, empty = no stroke
  std::vector<MeshCorrective> m_correctiveUndoBefore;  //!< snapshot per stroke
  //! Overlay for the corrective sculpt: the mesh vertices plus the brush.
  void drawCorrectiveSculpt_animate(double pixelSize);

  bool parentColumnRefDirs_animate(int column, TPointD &restDir,
                                   TPointD &defDir) const;
  //! The column whose plastic deformation is \p def, or -1.
  int columnOfDeformation_animate(const SkDP &def) const;
  void drawCrossLevelLinks_animate(double pixelSize);

  // ---- SuperPlastic cross-level IK (pins spanning several columns) ----
  // One column of the articulated character, resolved for a cross-level solve:
  // its deformation, rest + deformed skeletons, and the affine that maps its
  // own skeleton-local space into WORLD space (the common inertial frame — it
  // already folds in the column parenting, so an ancestor's motion is absorbed
  // there and pins can be re-planted against it per frame).
  struct CrossCol {
    int             columnIndex;
    SkDP            def;
    int             skelId;
    double          paramFrame;  //!< frame to read/write this column's params
    PlasticSkeleton rest;        //!< un-deformed
    PlasticSkeleton deformed;    //!< at the current frame, local space
    TAffine         world;       //!< skeleton-local -> world
  };
  //! Every connected column (current first), resolved for a cross-level solve.
  std::vector<CrossCol> crossColumns_animate(double frame);
  //! True when an active pin sits on a column OTHER than the current one: the
  //! trigger that routes the drag through the cross-level solver.
  bool hasCrossLevelPin_animate(double frame);
  //! Drop the cached per-frame placements of every connected column (they
  //! cache by frame time and never refresh within one frame on their own).
  void invalidateConnectedPlacements_animate();
  //! First move of a cross-level drag: per-column undo baselines + the world
  //! anchor of every pin sitting on a non-current column.
  void ensureCrossLevelBaselines_animate(double frame);
  // Unified skeleton graph (Franco's model): all connected columns merged into
  // ONE skeleton in a common world frame. Column roots stop being special — a
  // child column's root is an ordinary joint whose parent is the parent column's
  // attachment vertex (the cross-link). There is a single effective root (the
  // root column's root). The single-level FK/IK algorithm runs on THIS, then the
  // result is dispatched back per column via writeBackAnglesFor_animate.
  struct UNode {  // one graph node = (columnIndex, vertexIndex)
    int col, vtx;
    bool operator<(const UNode &o) const {
      return col != o.col ? col < o.col : vtx < o.vtx;
    }
    bool operator==(const UNode &o) const {
      return col == o.col && vtx == o.vtx;
    }
  };
  struct UnifiedGraph {
    std::vector<UNode> nodes;
    std::map<UNode, TPointD> world;    //!< current deformed position (world)
    std::map<UNode, TPointD> rest;     //!< rest position (same world frame)
    std::map<UNode, UNode> parent;     //!< unified parent; root -> {-1,-1}
    std::map<UNode, std::vector<UNode>> children;
    std::vector<UNode> pins;           //!< active pins across all columns
    std::map<UNode, TPointD> pinTarget;  //!< PINTX/PINTY world target per pin
    UNode root{-1, -1};                //!< the one effective root
  };
  //! Build the unified skeleton graph from the columns connected to the current
  //! one (foundation for the unified cross-level FK/IK solver). No side effects.
  UnifiedGraph buildUnifiedGraph_animate(double frame);
  //! Multi-anchor posing of a vertex lying BETWEEN >= 2 pins on the unified
  //! graph (the cross-level analogue of moveVertexMultiAnchor_animate).
  //! Returns false when it doesn't apply and the re-root rotation should run.
  bool crossLevelMultiAnchor_animate(const UNode &dragged,
                                     const TPointD &mousePos);
  //! Unified FK single-joint drag on the combined graph (child columns turn as
  //! ordinary chain joints). Returns false (no-op) when it doesn't apply —
  //! single column, pins present, dragging the root — so the caller falls back.
  bool crossLevelFK_animate(double frame, int vDragged, const TPointD &mousePos);
  //! Unified IK pin drag on the combined graph (STEP A): when a pin lives on a
  //! CHILD column, re-root the whole cross-column graph at that pin and pose the
  //! dragged vertex about it, so the pinned foot stays put while the body
  //! articulates — the free-root translation lands on the one root column's
  //! ROOTX/ROOTY. Returns false (caller falls back) when there is no cross-column
  //! pin, it's single-column, or the dragged vertex is the pin itself.
  bool crossLevelIK_animate(double frame, int vDragged, const TPointD &mousePos);
  //! Redirect a pick on a child column's (non-draggable) root to the coincident
  //! parent attachment vertex, which is draggable. In-place; no-op otherwise.
  void redirectChildRootToParent_animate(int &col, int &v);

  //! Mirror a child-column pin onto the parent's attachment vertex, so the
  //! single-level primary-pin machinery plants the whole rig (free root). `on`
  //! is the child pin's new state.
  void setAttachmentPin_animate(bool on, double frame);
  //! (legacy) unified multi-column solver — superseded by the attachment-pin
  //! approach; kept for reference, no longer routed.
  void crossLevelSolve_animate(double frame, int vDragged,
                               const TPointD &mousePos);
  //! Build the multi-column undo block for a finished cross-level drag.
  void finishCrossLevelUndo_animate(double frame);

  bool                   m_ikCrossDragged = false;  //!< drag spanned columns
  std::map<int, SkDKey>  m_ikCrossOld;   //!< per-column undo baseline (col->key)
  std::map<int, SkDP>    m_ikCrossDefs;  //!< per-column deformation (col->def)
  std::map<std::pair<int, int>, TPointD>
      m_ikCrossPinWorld;  //!< world plant target per pin, key (col,vertex)

  // Press-time pose baseline for the cross-level IK drag. Rebuilding the
  // unified graph from the DEFORMED skeletons on every mouse move feeds the
  // eval-time plant's CCD output back into the next drag step: drag writes
  // ANGLEs -> plant re-solves elsewhere -> next move measures against THAT.
  // Two solvers answering the same question in a closed loop is the
  // snapping/oscillation on joints inside pinned chains. Capturing the graph
  // once per drag makes each move a pure function of (baseline, mouse):
  // plant still refines what is DISPLAYED, but never re-enters the solve.
  bool                  m_ikCrossBaseValid = false;
  UnifiedGraph          m_ikCrossBaseGraph;  //!< unified graph at press time
  std::vector<CrossCol> m_ikCrossBaseCols;   //!< column snapshots at press time

  // Selection methods

  void setMeshSelection(MeshSelection &target, const MeshSelection &newSel);
  void toggleMeshSelection(MeshSelection &target,
                           const MeshSelection &addedSel);

  void onSelectionChanged() override;
  void enableCommands() override;

  // Parameter Observation methods

  void onChange(const TParamChange &) override;

private slots:

  void onFrameSwitched() override;
  void onColumnSwitched();
  void onXsheetChanged();

  void onShowMeshToggled(bool on);
  void onShowSOToggled(bool on);
  void onShowRigidityToggled(bool on);
  void onShowSkelOSToggled(bool on);
};

//****************************************************************************************
//    PlasticToolOptionsBox  declaration
//****************************************************************************************

class PlasticToolOptionsBox final : public GenericToolOptionsBox,
                                    public TProperty::Listener {
  Q_OBJECT

public:
  PlasticToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                        ToolHandle *toolHandle, TFrameHandle *frameHandle,
                        TObjectHandle *objHandle, TXsheetHandle *xshHandle);

private:
  class SkelIdsComboBox;

private:
  TTool *m_tool;
  TFrameHandle *m_frameHandle; 
  TObjectHandle *m_objHandle;
  TXsheetHandle *m_xshHandle;
  GenericToolOptionsBox **m_subToolbars;

  SkelIdsComboBox *m_skelIdComboBox;
  QPushButton *m_addSkelButton, *m_removeSkelButton;

  // Animate
  ToolOptionParamRelayField *m_distanceField, *m_angleField, *m_soField;
  ToolOptionParamRelayField *m_scaleXField,
      *m_scaleYField;  //!< SuperPlastic squash & stretch (root-anchored)
  QComboBox *m_interpolationCombo;
  QPushButton *m_setKeyButton, *m_setRestKeyButton;
  QPushButton *m_pinButton;  //!< SuperPlastic IK anchor toggle
  bool m_updateControls;

  void updateControls();
  void updateStatus();
  void onStageObjectChange(bool isDragging = false);

  int getKeysStatus(int frame, SkVD *vd);
  bool canSetInterpolation(int frame, TStageObject *stageObj);

  bool isSDChannelInterpolated(SkVD *vd, SkVD::Params param, int frame);

private:
  void showEvent(QShowEvent *se) override;
  void hideEvent(QHideEvent *he) override;

  void onPropertyChanged() override;

private slots:

  void onSkelIdsListChanged();
  void onSkelIdChanged();
  void onSkelIdEdited();

  void onAddSkeleton();
  void onRemoveSkeleton();

  void onSetKey();
  void onSetRestKey();
  void onPinButton();
  void onInterpolationComboActivated(int index);
  void onFrameSwitched();
  void onPlayingStatusChanged();
};

//****************************************************************************************
//    PlasticTool  local functions
//****************************************************************************************

namespace PlasticToolLocals {

extern PlasticTool l_plasticTool;        //!< Tool instance.
extern bool l_suspendParamsObservation;  //!< Used to join multiple param change
                                         //! notifications.

//------------------------------------------------------------------------------

// Generic functions

TPointD projection(
    const PlasticSkeleton &skeleton, int e,
    const TPointD &pos);  //!< Projects specified position an a skeleton edge.

// Global getters

double frame();  //!< Returns current global xsheet frame.
int row();       //!< Returns current global xsheet row.

int column();                 //!< Returns current global xsheet column index.
TXshColumn *xshColumn();      //!< Returns current xsheet column object.
TStageObject *stageObject();  //!< Returns current stage object.

const TXshCell &xshCell();  //!< Returns current xsheet cell.
void setCell(
    int row,
    int col);  //!< Moves current xsheet cell to the specified position.

int skeletonId();  //!< Returns current skeleton id.
double sdFrame();  //!< Returns current stage object's <I>parameters time</I>
//!  (ie the frame value to be used with function editor curves,
//!  which takes cyclicity into consideration).

// Keyframe functions

void setKeyframe(
    TDoubleParamP &param,
    double frame);  //!< Sets a keyframe to the specified parameter curve.
void setKeyframe(
    SkVD *vd,
    double frame);  //!< Sets a keyframe to the specified vertex deformation.
void setKeyframe(
    const PlasticSkeletonDeformationP &sd,
    double frame);  //!< Sets a keyframe to an entire skeleton deformation.
void setKeyframe(const PlasticSkeletonDeformationP &sd, double frame,
                 int skelId);  //!< Same, but with an explicit skeleton id —
                               //!< needed for connected columns other than the
                               //!< current one (::skeletonId() is current-only).

void invalidateXsheet();  //!< Refreshes xsheet content.

// Draw functions

void drawSquare(const TPointD &pos,
                double radius);  //!< Draws the outline of a square
void drawFullSquare(const TPointD &pos,
                    double radius);  //!< Draws a filled square

// Mesh functions

std::pair<double, PlasticTool::MeshIndex> closestVertex(const TMeshImage &mi,
                                                        const TPointD &pos);
std::pair<double, PlasticTool::MeshIndex> closestEdge(const TMeshImage &mi,
                                                      const TPointD &pos);

}  // namespace PlasticToolLocals

#endif  // PLASTICTOOL_H
