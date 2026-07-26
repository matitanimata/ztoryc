#include "ztorigpanel.h"

#include "tapp.h"
#include "menubarcommandids.h"

#include "toonz/preferences.h"

#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobjecttree.h"

#include "ext/plasticskeletondeformation.h"
#include "ext/plasticdeformerstorage.h"

#include "tundo.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <queue>
#include <set>

//=============================================================================
// ZtoRigActionRow
//=============================================================================

namespace {
// The dial is a percentage on screen and a 0..1 factor in the data. Values
// beyond the ends are legitimate (over-pose, counter-pose), so the spin box
// goes further than the slider instead of clamping the model to the widget.
const int kSliderMin    = 0;
const int kSliderMax    = 100;
const double kSpinMin   = -2.0;
const double kSpinMax   = 2.0;
}  // namespace

ZtoRigActionRow::ZtoRigActionRow(int index, const QString &name, double value,
                                 bool absolute, QWidget *parent)
    : QWidget(parent), m_index(index), m_absolute(absolute) {
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(6);

  // Mode toggle: "Pose" = absolute (recall exactly), "Offset" = additive (stack
  // on top). Checkable, label reflects the state.
  m_modeButton = new QToolButton(this);
  m_modeButton->setCheckable(true);
  m_modeButton->setChecked(m_absolute);
  m_modeButton->setFixedWidth(52);
  m_modeButton->setText(m_absolute ? tr("Pose") : tr("Offset"));
  m_modeButton->setToolTip(
      tr("Pose = recall this pose exactly (absolute).\n"
         "Offset = add on top of the current pose (additive)."));
  lay->addWidget(m_modeButton);

  auto *label = new QLabel(name, this);
  label->setMinimumWidth(80);
  label->setToolTip(name);
  lay->addWidget(label);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setRange(kSliderMin, kSliderMax);
  m_slider->setValue((int)(value * 100.0 + 0.5));
  m_slider->setToolTip(tr("Pose strength at this frame: 0 = rest, 1 = the "
                          "recorded pose.\nMoving it writes the keyframes "
                          "directly — no separate Key step."));
  lay->addWidget(m_slider, 1);

  m_spin = new QDoubleSpinBox(this);
  m_spin->setRange(kSpinMin, kSpinMax);
  m_spin->setSingleStep(0.05);
  m_spin->setDecimals(2);
  m_spin->setValue(value);
  m_spin->setToolTip(tr("Values outside 0..1 are allowed: over-pose and "
                        "counter-pose."));
  lay->addWidget(m_spin);

  // Rest / Full: jump the dial to 0 or 1 without reaching for Cmd-Z. Rest is
  // the "back to neutral" the user asked for — clearer than undo, which also
  // rolls back whatever else was done.
  auto *restBt = new QPushButton(tr("Rest"), this);
  restBt->setFixedWidth(40);
  restBt->setToolTip(tr("Set this dial to 0 (rest pose)."));
  lay->addWidget(restBt);

  auto *fullBt = new QPushButton(tr("Full"), this);
  fullBt->setFixedWidth(40);
  fullBt->setToolTip(tr("Set this dial to 1 (full pose)."));
  lay->addWidget(fullBt);

  auto *removeBt = new QPushButton(tr("×"), this);
  removeBt->setFixedWidth(24);
  removeBt->setToolTip(tr("Remove this action"));
  lay->addWidget(removeBt);

  // Drive the spin box: its valueChanged updates the slider and emits
  // guideChanged, so the display and the model stay in step.
  connect(restBt, &QPushButton::clicked, this,
          [this]() { m_spin->setValue(0.0); });
  connect(fullBt, &QPushButton::clicked, this,
          [this]() { m_spin->setValue(1.0); });
  connect(m_slider, &QSlider::sliderPressed, this,
          [this]() { emit guideBegin(m_index); });
  connect(m_slider, &QSlider::valueChanged, this, &ZtoRigActionRow::onSlider);
  connect(m_slider, &QSlider::sliderReleased, this,
          [this]() { emit guideCommit(m_index); });
  // The spin box (and the Rest/Full buttons that drive it) is a discrete edit:
  // begin + change + commit in one, so it too gets a single undo.
  connect(m_spin,
          static_cast<void (QDoubleSpinBox::*)(double)>(
              &QDoubleSpinBox::valueChanged),
          this, &ZtoRigActionRow::onSpin);
  connect(removeBt, &QPushButton::clicked, this,
          [this]() { emit removeRequested(m_index); });
  connect(m_modeButton, &QToolButton::toggled, this, [this](bool on) {
    m_absolute = on;
    m_modeButton->setText(on ? tr("Pose") : tr("Offset"));
    emit modeChanged(m_index, on);
  });
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::setValueSilently(double v) {
  m_updating = true;
  m_spin->setValue(v);
  m_slider->setValue((int)(v * 100.0 + 0.5));
  m_updating = false;
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onSlider(int v) {
  if (m_updating) return;
  m_updating = true;
  m_spin->setValue(v / 100.0);
  m_updating = false;
  // Live during the drag (begin/commit come from sliderPressed/Released).
  emit guideChanged(m_index, v / 100.0);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onSpin(double v) {
  if (m_updating) return;
  m_updating = true;
  m_slider->setValue((int)(v * 100.0 + 0.5));
  m_updating = false;
  // A spin edit / Rest / Full is one discrete change: bracket it so it becomes
  // a single undo, like a slider gesture.
  emit guideBegin(m_index);
  emit guideChanged(m_index, v);
  emit guideCommit(m_index);
}

//=============================================================================
// ZtoRigPanel
//=============================================================================

ZtoRigPanel::ZtoRigPanel(QWidget *parent) : TPanel(parent) {
  auto *root = new QWidget(this);
  auto *lay  = new QVBoxLayout(root);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(4);

  m_recordBt = new QPushButton(tr("Record Pose as Action…"), root);
  m_recordBt->setToolTip(
      tr("Store the pose authored at the current frame as a new action.\n"
         "Nothing changes on screen: the new dial starts at 0."));
  lay->addWidget(m_recordBt);

  m_emptyLabel = new QLabel(
      tr("No pose action on this column.\n\n"
         "Pose the character, then Record. Undo the pose and raise the\n"
         "dial: the pose comes back — driven by the action this time."),
      root);
  m_emptyLabel->setWordWrap(true);
  m_emptyLabel->setAlignment(Qt::AlignTop);
  lay->addWidget(m_emptyLabel);

  auto *rowsHost = new QWidget(root);
  m_rowsLay      = new QVBoxLayout(rowsHost);
  m_rowsLay->setContentsMargins(0, 0, 0, 0);
  m_rowsLay->setSpacing(2);
  m_rowsLay->addStretch(1);

  m_scroll = new QScrollArea(root);
  m_scroll->setWidgetResizable(true);
  m_scroll->setWidget(rowsHost);
  lay->addWidget(m_scroll, 1);

  setWidget(root);

  connect(m_recordBt, &QPushButton::clicked, this, &ZtoRigPanel::onRecord);

  TApp *app = TApp::instance();
  connect(app->getCurrentColumn(), SIGNAL(columnIndexSwitched()), this,
          SLOT(rebuild()));
  connect(app->getCurrentScene(), SIGNAL(sceneSwitched()), this,
          SLOT(rebuild()));
  connect(app->getCurrentXsheet(), SIGNAL(xsheetSwitched()), this,
          SLOT(rebuild()));
  // Frame changes only refresh the numbers: rebuilding here would fight the
  // user's drag and reset the scroll position on every frame during playback.
  connect(app->getCurrentFrame(), SIGNAL(frameSwitched()), this,
          SLOT(onFrameSwitched()));
  // Undo/redo of a Record or Remove changes which actions exist without a
  // column/scene switch: refresh (which rebuilds on a count mismatch) so the
  // rows follow the history.
  connect(TUndoManager::manager(), SIGNAL(somethingChanged()), this,
          SLOT(refreshValues()));

  rebuild();
}

//-----------------------------------------------------------------------------

PlasticSkeletonDeformationP ZtoRigPanel::currentDeformation() const {
  TApp *app = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  if (!xsh) return PlasticSkeletonDeformationP();

  const int col = app->getCurrentColumn()->getColumnIndex();
  if (col < 0) return PlasticSkeletonDeformationP();

  TStageObject *obj =
      xsh->getStageObject(TStageObjectId::ColumnId(col));
  if (!obj) return PlasticSkeletonDeformationP();

  return obj->getPlasticSkeletonDeformation();
}

//-----------------------------------------------------------------------------

double ZtoRigPanel::currentFrame() const {
  return (double)TApp::instance()->getCurrentFrame()->getFrame();
}

TStageObject *ZtoRigPanel::currentStageObject() const {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  if (!xsh) return nullptr;
  const int col = app->getCurrentColumn()->getColumnIndex();
  if (col < 0) return nullptr;
  return xsh->getStageObject(TStageObjectId::ColumnId(col));
}

//-----------------------------------------------------------------------------

// Breadth-first over the column tree from `startCol`, invalidating each stage
// object's cached placement. Mirrors PlasticTool's own flush: the pose blend
// can move a parent, and a child column glued to it must refresh. Free so both
// the panel and the undo object can reach it.
static void ztorigFlushPlacements(TXsheet *xsh, int startCol) {
  if (!xsh || startCol < 0) return;
  std::set<int> visited;
  std::queue<int> pending;
  visited.insert(startCol);
  pending.push(startCol);
  while (!pending.empty()) {
    const int c = pending.front();
    pending.pop();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    obj->invalidate();

    const TStageObjectId pid = obj->getParent();
    if (pid.isColumn() && !visited.count(pid.getIndex())) {
      visited.insert(pid.getIndex());
      pending.push(pid.getIndex());
    }
    for (TStageObject *child : obj->getChildren()) {
      const TStageObjectId cid = child->getId();
      if (cid.isColumn() && !visited.count(cid.getIndex())) {
        visited.insert(cid.getIndex());
        pending.push(cid.getIndex());
      }
    }
  }
}

void ZtoRigPanel::flushConnectedPlacements() {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet() ? app->getCurrentXsheet()->getXsheet()
                                         : nullptr;
  ztorigFlushPlacements(xsh, app->getCurrentColumn()->getColumnIndex());

  // The dial GUIDE is a TDoubleParam the deformation does not observe (unlike
  // the real pose params), so moving the slider never fired onChange and the
  // MESH deformer cache stayed stale — the skeleton overlay moved but the
  // drawing did not follow until a click wrote a real param. Invalidate the
  // deformer explicitly here so the mesh re-solves on the next redraw.
  if (const PlasticSkeletonDeformationP sd = currentDeformation())
    PlasticDeformerStorage::instance()->invalidateDeformation(
        sd.getPointer(), PlasticDeformerStorage::NONE);
}

//=============================================================================
// Undo — a Record or a Remove is one step: swap the whole pose-action vector.
// Snapshotting the vector (guides shared by pointer) is simpler and safer than
// tracking per-action edits, and covers re-recording an existing action too.
//=============================================================================

namespace {

class UndoPoseActions final : public TUndo {
  TXsheetHandle *m_xshHandle;
  PlasticSkeletonDeformationP m_sd;
  int m_col;
  std::vector<PoseAction> m_before, m_after;
  QString m_label;

  void apply(const std::vector<PoseAction> &actions) const {
    if (!m_sd) return;
    m_sd->setPoseActions(actions);
    if (m_xshHandle && m_xshHandle->getXsheet())
      ztorigFlushPlacements(m_xshHandle->getXsheet(), m_col);
    if (m_xshHandle) m_xshHandle->notifyXsheetChanged();
  }

public:
  UndoPoseActions(TXsheetHandle *xshHandle,
                  const PlasticSkeletonDeformationP &sd, int col,
                  const std::vector<PoseAction> &before,
                  const std::vector<PoseAction> &after, const QString &label)
      : m_xshHandle(xshHandle)
      , m_sd(sd)
      , m_col(col)
      , m_before(before)
      , m_after(after)
      , m_label(label) {}

  void undo() const override { apply(m_before); }
  void redo() const override { apply(m_after); }
  int getSize() const override {
    return (int)((m_before.size() + m_after.size()) * sizeof(PoseAction)) + 64;
  }
  QString getHistoryString() override { return m_label; }
};

// Undo for stamping a pose into plastic keys: swap the whole pose-key snapshot
// (param keyframes + guide curves). Covers the exclusive zeroing of the other
// dials too, since those live in the same snapshot.
class UndoPoseKeyState final : public TUndo {
  TXsheetHandle *m_xshHandle;
  PlasticSkeletonDeformationP m_sd;
  int m_col;
  PlasticSkeletonDeformation::PoseKeyState m_before, m_after;
  // Optional column TRANSFORM key (Global scope Stage/All). m_xformFrame < 0
  // means the gesture did not touch the transform.
  int m_xformFrame;
  bool m_beforeHad, m_afterHad;
  TStageObject::Keyframe m_beforeKey, m_afterKey;

  void restoreXform(bool had, const TStageObject::Keyframe &key) const {
    if (m_xformFrame < 0 || !m_xshHandle || !m_xshHandle->getXsheet()) return;
    TStageObject *o =
        m_xshHandle->getXsheet()->getStageObject(TStageObjectId::ColumnId(m_col));
    if (!o) return;
    if (had)
      o->setKeyframeWithoutUndo(m_xformFrame, key);
    else
      o->removeKeyframeWithoutUndo(m_xformFrame);
  }

  void apply(const PlasticSkeletonDeformation::PoseKeyState &st, bool xHad,
             const TStageObject::Keyframe &xKey) const {
    if (!m_sd) return;
    m_sd->setPoseKeyState(st);
    restoreXform(xHad, xKey);
    if (m_xshHandle && m_xshHandle->getXsheet()) {
      TXsheet *xsh = m_xshHandle->getXsheet();
      ztorigFlushPlacements(xsh, m_col);
      if (TStageObject *o = xsh->getStageObject(TStageObjectId::ColumnId(m_col)))
        o->updateKeyframes();  // keep the xsheet diamonds in sync on undo/redo
    }
    if (m_xshHandle) m_xshHandle->notifyXsheetChanged();
  }

public:
  UndoPoseKeyState(TXsheetHandle *xshHandle,
                   const PlasticSkeletonDeformationP &sd, int col,
                   const PlasticSkeletonDeformation::PoseKeyState &before,
                   const PlasticSkeletonDeformation::PoseKeyState &after,
                   int xformFrame, bool beforeHad,
                   const TStageObject::Keyframe &beforeKey, bool afterHad,
                   const TStageObject::Keyframe &afterKey)
      : m_xshHandle(xshHandle)
      , m_sd(sd)
      , m_col(col)
      , m_before(before)
      , m_after(after)
      , m_xformFrame(xformFrame)
      , m_beforeHad(beforeHad)
      , m_afterHad(afterHad)
      , m_beforeKey(beforeKey)
      , m_afterKey(afterKey) {}
  void undo() const override { apply(m_before, m_beforeHad, m_beforeKey); }
  void redo() const override { apply(m_after, m_afterHad, m_afterKey); }
  int getSize() const override { return 256; }
  QString getHistoryString() override {
    return QObject::tr("ZtoRig: Pose");
  }
};

}  // namespace

//-----------------------------------------------------------------------------

void ZtoRigPanel::rebuild() {
  for (ZtoRigActionRow *row : m_rows) {
    m_rowsLay->removeWidget(row);
    row->deleteLater();
  }
  m_rows.clear();

  const PlasticSkeletonDeformationP sd = currentDeformation();
  const int count = sd ? sd->poseActionsCount() : 0;

  m_recordBt->setEnabled((bool)sd);
  m_emptyLabel->setVisible(count == 0);
  m_scroll->setVisible(count > 0);

  if (!sd) {
    m_emptyLabel->setText(
        tr("This column has no plastic skeleton.\n\n"
           "Build one with the Plastic tool, then come back here."));
    m_emptyLabel->setVisible(true);
    return;
  }

  m_emptyLabel->setText(
      tr("No pose action on this column.\n\n"
         "Pose the character, then Record. Undo the pose and raise the\n"
         "dial: the pose comes back — driven by the action this time."));

  const double frame = currentFrame();
  for (int i = 0; i < count; ++i) {
    const PoseAction *act = sd->poseAction(i);
    if (!act) continue;

    const double v = sd->poseStrengthAt(i, frame);

    auto *row = new ZtoRigActionRow(i, act->m_name, v, act->m_absolute, this);
    connect(row, &ZtoRigActionRow::guideBegin, this,
            &ZtoRigPanel::onGuideBegin);
    connect(row, &ZtoRigActionRow::guideChanged, this,
            &ZtoRigPanel::onGuideChanged);
    connect(row, &ZtoRigActionRow::guideCommit, this,
            &ZtoRigPanel::onGuideCommit);
    connect(row, &ZtoRigActionRow::removeRequested, this,
            &ZtoRigPanel::onRemove);
    connect(row, &ZtoRigActionRow::modeChanged, this,
            &ZtoRigPanel::onModeChanged);

    m_rowsLay->insertWidget(m_rowsLay->count() - 1, row);
    m_rows.push_back(row);
  }
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::refreshValues() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  // The row count can go stale if the deformation changed behind our back
  // (undo of a Record, say): fall back to a full rebuild instead of reading
  // past the end.
  if (sd->poseActionsCount() != m_rows.size()) {
    rebuild();
    return;
  }

  const double frame = currentFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd->poseStrengthAt(row->index(), frame));
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onFrameSwitched() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  if (sd->poseActionsCount() != m_rows.size()) {
    rebuild();
    return;
  }

  // The slider READS the pose strength off the keys at the new frame: 0 where
  // the character is at rest for this action, 1 where it is fully in the pose.
  // So it always shows where you are and can be dialled in or out.
  const double frame = currentFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd->poseStrengthAt(row->index(), frame));
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onRecord() {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  bool ok = false;
  const QString name = QInputDialog::getText(
      this, tr("Record Pose as Action"), tr("Action name:"), QLineEdit::Normal,
      tr("action%1").arg(sd->poseActionsCount() + 1), &ok);
  if (!ok || name.trimmed().isEmpty()) return;

  const std::vector<PoseAction> before = sd->getPoseActions();
  sd->recordPoseAction(name.trimmed(), currentFrame());
  const std::vector<PoseAction> after = sd->getPoseActions();

  TApp *app = TApp::instance();
  const int col = app->getCurrentColumn()->getColumnIndex();
  TUndoManager::manager()->add(new UndoPoseActions(
      app->getCurrentXsheet(), sd, col, before, after,
      tr("ZtoRig: Record Action")));

  app->getCurrentScene()->setDirtyFlag(true);
  // The new dial is 0, so the render is unchanged; still flush so the viewer is
  // in a known-consistent state and later dial moves start from a clean cache.
  flushConnectedPlacements();
  app->getCurrentXsheet()->notifyXsheetChanged();
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideBegin(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  // Snapshot for one undo per gesture: the pose params + the column transform
  // key at this frame (which the commit may add under Global Stage/All).
  m_dragBefore   = sd->getPoseKeyState();
  m_dragXformFrame   = (int)currentFrame();
  m_dragXformHadKey  = false;
  if (TStageObject *o = currentStageObject()) {
    m_dragXformHadKey = o->isKeyframe(m_dragXformFrame);
    if (m_dragXformHadKey) m_dragXformOldKey = o->getKeyframe(m_dragXformFrame);
  }
  // Freeze the base an Offset adds onto: without this the slider reads back its
  // own previous write on every move and the offset compounds (see beginPoseDrag).
  {
    TStageObject *o2 = currentStageObject();
    const double f   = o2 ? o2->paramsTime(currentFrame()) : currentFrame();
    sd->beginPoseDrag(index, f);
  }
  m_dragActive = true;
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideChanged(int index, double value) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  if (!m_dragActive) onGuideBegin(index);  // safety if begin was missed

  // The slider IS the animation: write the pose straight into plastic keys at
  // the current frame, live. No hidden guide. A full Pose overwrites the shared
  // params (so Poses are mutually exclusive by construction); an Offset only
  // its own, so partial controllers stack.
  TStageObject *obj = currentStageObject();
  const double f    = obj ? obj->paramsTime(currentFrame()) : currentFrame();
  sd->applyPoseStrength(index, value, f);
  if (obj) obj->updateKeyframes();

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  flushConnectedPlacements();  // also invalidates the mesh deformer
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideCommit(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd || !m_dragActive) {
    m_dragActive = false;
    return;
  }
  m_dragActive = false;
  sd->endPoseDrag();

  // Respect the Plastic tool's Global Key scope: with All (or Stage) a pose
  // also drops a TRANSFORM key on the column, exactly like posing by hand.
  const int scope    = Preferences::instance()->getIntValue(GlobalKeyScope);
  bool afterHad      = m_dragXformHadKey;
  TStageObject::Keyframe afterKey = m_dragXformOldKey;
  int xformFrame     = -1;
  TStageObject *obj  = currentStageObject();
  if (obj && scope != 1 /* Stage or All */) {
    xformFrame = (int)currentFrame();
    obj->setKeyframeWithoutUndo(xformFrame);
    obj->updateKeyframes();
    afterHad = true;
    afterKey = obj->getKeyframe(xformFrame);
  }

  PlasticSkeletonDeformation::PoseKeyState after = sd->getPoseKeyState();
  TUndoManager::manager()->add(new UndoPoseKeyState(
      TApp::instance()->getCurrentXsheet(), sd,
      TApp::instance()->getCurrentColumn()->getColumnIndex(), m_dragBefore,
      after, xformFrame, m_dragXformHadKey, m_dragXformOldKey, afterHad,
      afterKey));

  // The move can have changed how much of the OTHER actions reads as applied
  // (overlapping params): refresh every slider from the keys.
  const double frame = currentFrame();
  for (ZtoRigActionRow *row : m_rows)
    row->setValueSilently(sd->poseStrengthAt(row->index(), frame));

  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------

void ZtoRigPanel::onModeChanged(int index, bool absolute) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;
  PoseAction *act = sd->poseAction(index);
  if (!act) return;

  act->m_absolute = absolute;
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  flushConnectedPlacements();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onRemove(int index) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  const PoseAction *act = sd->poseAction(index);
  if (!act) return;

  if (QMessageBox::question(
          this, tr("Remove Action"),
          tr("Remove the action \"%1\"?\n\nUndo brings it back.")
              .arg(act->m_name),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  const std::vector<PoseAction> before = sd->getPoseActions();
  sd->removePoseAction(index);
  const std::vector<PoseAction> after = sd->getPoseActions();

  TApp *app = TApp::instance();
  const int col = app->getCurrentColumn()->getColumnIndex();
  TUndoManager::manager()->add(new UndoPoseActions(
      app->getCurrentXsheet(), sd, col, before, after,
      tr("ZtoRig: Remove Action")));

  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
  rebuild();
}

//=============================================================================
// Factory
//=============================================================================

class ZtoRigPanelFactory final : public TPanelFactory {
public:
  ZtoRigPanelFactory() : TPanelFactory("ZtoRigPanel") {}

  TPanel *createPanel(QWidget *parent) override {
    auto *panel = new ZtoRigPanel(parent);
    panel->setObjectName("ZtoRigPanel");
    panel->setWindowTitle("ZtoRig");
    return panel;
  }

  void initialize(TPanel *) override { assert(0); }

} ztoRigPanelFactory;
