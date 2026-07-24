#include "ztorigpanel.h"

#include "tapp.h"
#include "menubarcommandids.h"

#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobjecttree.h"

#include "ext/plasticskeletondeformation.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

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
                                 QWidget *parent)
    : QWidget(parent), m_index(index) {
  auto *lay = new QHBoxLayout(this);
  lay->setContentsMargins(4, 2, 4, 2);
  lay->setSpacing(6);

  auto *label = new QLabel(name, this);
  label->setMinimumWidth(90);
  label->setToolTip(name);
  lay->addWidget(label);

  m_slider = new QSlider(Qt::Horizontal, this);
  m_slider->setRange(kSliderMin, kSliderMax);
  m_slider->setValue((int)(value * 100.0 + 0.5));
  m_slider->setToolTip(tr("How much of this action applies at the current "
                          "frame. The dial is keyframeable."));
  lay->addWidget(m_slider, 1);

  m_spin = new QDoubleSpinBox(this);
  m_spin->setRange(kSpinMin, kSpinMax);
  m_spin->setSingleStep(0.05);
  m_spin->setDecimals(2);
  m_spin->setValue(value);
  m_spin->setToolTip(tr("Values outside 0..1 are allowed: over-pose and "
                        "counter-pose."));
  lay->addWidget(m_spin);

  auto *removeBt = new QPushButton(tr("×"), this);
  removeBt->setFixedWidth(24);
  removeBt->setToolTip(tr("Remove this action"));
  lay->addWidget(removeBt);

  connect(m_slider, &QSlider::valueChanged, this, &ZtoRigActionRow::onSlider);
  connect(m_spin,
          static_cast<void (QDoubleSpinBox::*)(double)>(
              &QDoubleSpinBox::valueChanged),
          this, &ZtoRigActionRow::onSpin);
  connect(removeBt, &QPushButton::clicked, this,
          [this]() { emit removeRequested(m_index); });
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
  emit guideChanged(m_index, v / 100.0);
}

//-----------------------------------------------------------------------------

void ZtoRigActionRow::onSpin(double v) {
  if (m_updating) return;
  m_updating = true;
  m_slider->setValue((int)(v * 100.0 + 0.5));
  m_updating = false;
  emit guideChanged(m_index, v);
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

    const double v = act->m_guide ? act->m_guide->getValue(frame) : 0.0;

    auto *row = new ZtoRigActionRow(i, act->m_name, v, this);
    connect(row, &ZtoRigActionRow::guideChanged, this,
            &ZtoRigPanel::onGuideChanged);
    connect(row, &ZtoRigActionRow::removeRequested, this,
            &ZtoRigPanel::onRemove);

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
  for (ZtoRigActionRow *row : m_rows) {
    const PoseAction *act = sd->poseAction(row->index());
    if (!act || !act->m_guide) continue;
    row->setValueSilently(act->m_guide->getValue(frame));
  }
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

  sd->recordPoseAction(name.trimmed(), currentFrame());

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  rebuild();
}

//-----------------------------------------------------------------------------

void ZtoRigPanel::onGuideChanged(int index, double value) {
  const PlasticSkeletonDeformationP sd = currentDeformation();
  if (!sd) return;

  PoseAction *act = sd->poseAction(index);
  if (!act || !act->m_guide) return;

  // Keyed dial -> write a key at this frame; un-keyed -> move the constant
  // value. Same idiom the rest of the app uses, so a dial can be animated
  // from the function editor without this panel knowing about it.
  if (act->m_guide->getKeyframeCount() > 0)
    act->m_guide->setValue(currentFrame(), value);
  else
    act->m_guide->setDefaultValue(value);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
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
          tr("Remove the action \"%1\"?\n\nThe pose it drives is lost.")
              .arg(act->m_name),
          QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    return;

  sd->removePoseAction(index);

  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
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
