#pragma once

// ZtoRigPanel — ZtoRig pose dials.
//
// One row per pose action of the CURRENT column's plastic deformation: the
// action name, a dial that drives its guide, and a button to drop it. Plus a
// button that records the pose authored at the current frame as a new action.
//
// The dial writes the guide TDoubleParam, nothing else: the blend is summed at
// evaluation time (PlasticSkeletonDeformation::poseBlendOffset) and never
// written into the pose params, so the authored animation stays separable from
// the actions driving it.

#include "pane.h"  // TPanel, TPanelFactory

#include <QWidget>
#include <QString>
#include <QVector>

#include "toonz/tstageobject.h"

class QVBoxLayout;
class QScrollArea;
class QLabel;
class QPushButton;
class QSlider;
class QDoubleSpinBox;
class QToolButton;

//! One row: name + dial + remove.
class ZtoRigActionRow final : public QWidget {
  Q_OBJECT

public:
  ZtoRigActionRow(int index, const QString &name, double value, bool absolute,
                  QWidget *parent = nullptr);

  int index() const { return m_index; }
  //! Refresh the shown value without re-emitting (guard against feedback when
  //! the frame changes under a dial the user is not touching).
  void setValueSilently(double v);

signals:
  //! Interaction started (slider pressed / spin about to change): snapshot for
  //! one undo per gesture.
  void guideBegin(int index);
  //! Live value while dragging: write the pose into keys at this strength.
  void guideChanged(int index, double value);
  //! Interaction ended (slider released / spin committed): finalize the undo.
  void guideCommit(int index);
  void removeRequested(int index);
  //! Absolute (pose) <-> additive (offset) toggled for this action.
  void modeChanged(int index, bool absolute);

private slots:
  void onSlider(int v);
  void onSpin(double v);

private:
  int m_index;
  QSlider *m_slider         = nullptr;
  QDoubleSpinBox *m_spin    = nullptr;
  QToolButton *m_modeButton = nullptr;  // Pose (absolute) / Offset (additive)
  bool m_absolute           = false;
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
  void onModeChanged(int index, bool absolute);
  void onRemove(int index);
  //! Frame changed: the slider reads the current pose STRENGTH off the keys, so
  //! it shows where you are (0 rest, 1 pose) and can be dialled in and out.
  void onFrameSwitched();
  //! Rebuild the rows from the current column (column/scene changed).
  void rebuild();
  //! Only refresh the shown values (frame changed): rebuilding on every frame
  //! would fight the user's drag and reset scroll position during playback.
  void refreshValues();

private:
  //! Deformation of the current column, or null when the column has no plastic
  //! skeleton — which is the normal case, not an error.
  PlasticSkeletonDeformationP currentDeformation() const;
  TStageObject *currentStageObject() const;
  double currentFrame() const;
  //! After a dial moves, the connected columns' cached placements don't know
  //! they depend on plastic params, so the drawing follows the skeleton only
  //! at the next click. Invalidate them here — same fix as PlasticTool::
  //! invalidateConnectedPlacements_animate, walking the parent/child columns.
  void flushConnectedPlacements();

  // One undo per slider gesture: snapshot on begin, finalize on commit.
  PlasticSkeletonDeformation::PoseKeyState m_dragBefore;
  bool m_dragActive          = false;
  bool m_dragXformHadKey     = false;
  TStageObject::Keyframe m_dragXformOldKey;
  int m_dragXformFrame       = -1;

  QVBoxLayout *m_rowsLay  = nullptr;
  QScrollArea *m_scroll   = nullptr;
  QLabel *m_emptyLabel    = nullptr;
  QPushButton *m_recordBt = nullptr;
  QVector<ZtoRigActionRow *> m_rows;
};
