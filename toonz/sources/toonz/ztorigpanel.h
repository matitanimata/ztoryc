#pragma once

// ZtoRigPanel — ZtoRig pose dials.
//
// One row per pose action of the CURRENT column's plastic deformation: the
// action name, a slider for how much of it applies, and a button to drop it.
// Plus a button that records the pose authored at the current frame as a new
// action.
//
// The slider IS the animation: moving it stamps the pose into real plastic
// keyframes at the current frame (PlasticSkeletonDeformation::applyPoseStrength)
// and records the strength, which is what the slider reads back on every frame
// change. Nothing is blended at evaluation time.

#include "pane.h"  // TPanel, TPanelFactory

#include <QWidget>
#include <QString>
#include <QVector>

#include <set>
#include <vector>

#include "toonz/tstageobject.h"

class QVBoxLayout;
class QScrollArea;
class QLabel;
class QPushButton;
class QSlider;
class QDoubleSpinBox;
class QToolButton;
class QCheckBox;
class QAction;

//! One row: name + dial + remove.
class ZtoRigActionRow final : public QWidget {
  Q_OBJECT

public:
  ZtoRigActionRow(int index, const QString &name, double value, int mode,
                  bool isBase, const std::vector<int> &allSkelIds,
                  const std::set<int> &onSkelIds, QWidget *parent = nullptr);

  int index() const { return m_index; }
  //! Refresh the shown value without re-emitting (guard against feedback when
  //! the frame changes under a dial the user is not touching).
  void setValueSilently(double v);
  //! Grey the row out when its action belongs to a skeleton other than the one
  //! active at the current frame. Remove stays live: you must be able to delete
  //! an action of another view without going to look for that view first.
  void setApplicable(bool on);

private:
  //! Rebuild the skeleton menu's ticks from m_skelIds.
  void updateSkelButton();
  //! Refresh the mode button's label from m_mode.
  void updateModeButton();
  //! The base pose is the zero, so its own dial is meaningless: greyed out.
  void setBaseAppearance();

public:

signals:
  //! Interaction started (slider pressed / spin about to change): snapshot for
  //! one undo per gesture.
  void guideBegin(int index);
  //! Live value while dragging: write the pose into keys at this strength.
  void guideChanged(int index, double value);
  //! Interaction ended (slider released / spin committed): finalize the undo.
  void guideCommit(int index);
  void removeRequested(int index);
  //! Stamping mode cycled for this action (PoseAction::Mode).
  void modeChanged(int index, int mode);
  //! This action was made (or unmade) the zero of its skeleton.
  void baseToggled(int index, bool isBase);
  //! The set of skeletons the action may be applied to was edited.
  void skeletonsChanged(int index, const std::set<int> &skelIds);

private slots:
  void onSlider(int v);
  //! Cycle Offset -> Pose -> Part. A three-state control, so a checkable
  //! button will not do.
  void onModeClicked();
  void onSpin(double v);

private:
  int m_index;
  QSlider *m_slider         = nullptr;
  QDoubleSpinBox *m_spin    = nullptr;
  QToolButton *m_modeButton = nullptr;  // Pose (absolute) / Offset (additive)
  QToolButton *m_baseButton = nullptr;  // this action is the skeleton's zero
  QToolButton *m_skelButton = nullptr;  // which skeletons it applies to
  QVector<QAction *> m_skelActions;     // its menu entries, "All" first
  QPushButton *m_restBt     = nullptr;
  QPushButton *m_fullBt     = nullptr;
  QLabel *m_label           = nullptr;
  int m_mode                = 0;  // PoseAction::Mode
  bool m_isBase             = false;
  std::set<int> m_skelIds;  // empty = every skeleton
  bool m_updating           = false;  // slider <-> spin feedback guard
};

//----------------------------------------------------------------------------

class ZtoRigPanel final : public TPanel {
  Q_OBJECT

public:
  explicit ZtoRigPanel(QWidget *parent = nullptr);

private slots:
  void onRecord();
  void onGuideBegin(int index);
  void onGuideChanged(int index, double value);
  void onGuideCommit(int index);
  void onModeChanged(int index, int mode);
  void onBaseToggled(int index, bool isBase);
  void onSkeletonsChanged(int index, const std::set<int> &skelIds);
  void onRemove(int index);
  //! Frame changed: the slider reads the current pose STRENGTH off the keys, so
  //! it shows where you are (0 rest, 1 pose) and can be dialled in and out.
  void onFrameSwitched();
  //! Rebuild the rows from the current column (column/scene changed).
  void rebuild();
  //! Only refresh the shown values (frame changed): rebuilding on every frame
  //! would fight the user's drag and reset scroll position during playback.
  void refreshValues();
  //! Record dialog: action name plus which skeletons it may be used on.
  //! Returns false when cancelled. The skeleton section is omitted on a rig
  //! with a single skeleton, where there is nothing to choose.
  bool askRecordDetails(const std::vector<int> &allSkelIds, int activeSkelId,
                        QString &name, std::set<int> &skelIds);
  //! Every skeleton id in the deformation, ascending.
  std::vector<int> allSkeletonIds(const PlasticSkeletonDeformationP &sd) const;
  //! Inserts a bold, ruled header before a skeleton's block of actions.
  //! Returns it so rebuild() can tear it down with the rows.
  QWidget *addGroupHeader(int skelId);
  //! Strength of the current column's action \p index at \p frame; the parts
  //! are written in lockstep, so any one of them answers for the action.
  double sd_poseStrength(int index, double frame) const;
  //! Enable/disable each row against the skeleton active at the current frame.
  //! Cheap enough to run on every frame change, unlike a rebuild.
  void updateApplicability();

private:
  //! One column of the character, resolved for a fan-out.
  struct CharPart {
    PlasticSkeletonDeformationP m_sd;
    int m_col      = -1;
    double m_frame = 0.0;  //!< that column's params time for the current frame
  };

  //! Every column of the character the current one belongs to — climbing to the
  //! top through column parenting, then back down. A pose action is a property
  //! of the CHARACTER: on an exploded rig the body, the arm and the leg are
  //! separate columns, and a pose that only reached one of them would leave the
  //! others behind and vanish the moment you selected a different column.
  std::vector<CharPart> characterParts() const;

  //! Index of the action named \p name inside \p sd, or -1. The parts of one
  //! action are tied together by NAME: each column stores its own deltas, and
  //! their indices need not line up.
  static int actionIndexByName(const PlasticSkeletonDeformationP &sd,
                               const QString &name);
  //! Name of the action a row refers to, empty if the row is stale.
  QString actionNameAt(int index) const;

  //! Deformation of the current column, or null when the column has no plastic
  //! skeleton — which is the normal case, not an error.
  PlasticSkeletonDeformationP currentDeformation() const;
  TStageObject *currentStageObject() const;
  //! The XSHEET frame — for xsheet-level things only (the column transform
  //! key). Never for the deformation: use paramsFrame().
  double currentFrame() const;
  //! The frame in the stage object's parameter time, which is what the
  //! deformation's params and skeleton-id curve are keyed against.
  double paramsFrame() const;
  //! After a dial moves, the connected columns' cached placements don't know
  //! they depend on plastic params, so the drawing follows the skeleton only
  //! at the next click. Invalidate them here — same fix as PlasticTool::
  //! invalidateConnectedPlacements_animate, walking the parent/child columns.
  void flushConnectedPlacements();

  // One undo per slider gesture: snapshot on begin, finalize on commit. One
  // entry per column of the character — the gesture writes them all.
  std::vector<std::pair<PlasticSkeletonDeformationP,
                        PlasticSkeletonDeformation::PoseKeyState>>
      m_dragBefore;
  bool m_dragActive          = false;
  bool m_dragXformHadKey     = false;
  TStageObject::Keyframe m_dragXformOldKey;
  int m_dragXformFrame       = -1;

  QVBoxLayout *m_rowsLay  = nullptr;
  QScrollArea *m_scroll   = nullptr;
  QLabel *m_emptyLabel    = nullptr;
  QPushButton *m_recordBt = nullptr;
  QVector<ZtoRigActionRow *> m_rows;
  QCheckBox *m_showAllBt = nullptr;
  //! Skeleton the rows were built for, so a frame change only rebuilds when it
  //! actually switches skeleton — not on every frame during playback.
  int m_builtSkelId = -1;
  //! Actions at the last rebuild. NOT m_rows.size(): an action applying to
  //! several skeletons has a row under each of them.
  int m_builtActionCount = 0;
  //! Per-skeleton group headers, owned by the same layout as the rows and torn
  //! down with them on rebuild.
  QVector<QWidget *> m_groupHeaders;
};
