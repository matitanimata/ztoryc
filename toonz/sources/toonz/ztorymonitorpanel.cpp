#include "ztorymonitorpanel.h"

#include "ztorymodel.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"

#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

//=============================================================================
// ZtoryMonitorPanel
//=============================================================================

ZtoryMonitorPanel::ZtoryMonitorPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle(tr("Ztoryc Monitor"));
  setObjectName("ZtoryMonitorPanel");

  // ── Viewer ────────────────────────────────────────────────────────────────
  // Create the animatic viewer as a plain child widget.  Its own TPanel title
  // bar is replaced with an empty widget so it stays invisible; the outer
  // panel's title bar is used for the viewer's control buttons.
  m_viewer = new ZtoryAnimaticViewer(this);
  m_viewer->setMinimumHeight(80);
  // Populate the outer panel's title bar with the viewer's control buttons.
  m_viewer->initializeAnimaticTitleBar(getTitleBar());

  // ── Timeline area ─────────────────────────────────────────────────────────
  m_ruler = new ZtoryAnimaticRuler(this);
  m_ruler->setFixedHeight(24);
  m_ruler->setPixelsPerFrame(m_ppf);

  m_track = new ZtoryAnimaticTrack(this);
  m_track->setFixedHeight(52);
  m_track->setPixelsPerFrame(m_ppf);

  QWidget *timelineWidget = new QWidget(this);
  QVBoxLayout *tlay = new QVBoxLayout(timelineWidget);
  tlay->setContentsMargins(0, 0, 0, 0);
  tlay->setSpacing(0);
  tlay->addWidget(m_ruler);
  tlay->addWidget(m_track);
  timelineWidget->setFixedHeight(m_ruler->height() + m_track->height());

  // ── Splitter ─────────────────────────────────────────────────────────────
  // Vertical splitter: viewer (resizable) on top, timeline (fixed) at bottom.
  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->setChildrenCollapsible(false);
  splitter->addWidget(m_viewer);
  splitter->addWidget(timelineWidget);
  // Viewer gets all the stretch; timeline stays compact.
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  lay->addWidget(splitter, 1);
  setWidget(container);

  // ── Connections ──────────────────────────────────────────────────────────

  // Ruler ↔ track zoom sync
  connect(m_ruler, &ZtoryAnimaticRuler::zoomChanged,
          this, &ZtoryMonitorPanel::onZoomChanged);
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryMonitorPanel::onZoomChanged);

  // Shot navigation — seek only, never open sub-scene
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryMonitorPanel::onShotClicked);
  connect(m_track, &ZtoryAnimaticTrack::shotDoubleClicked,
          this, &ZtoryMonitorPanel::onShotDoubleClicked);

  // Ruler seek → controller
  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryMonitorPanel::onFrameChanged);

  // Controller frame → ruler + track
  auto *ctrl = ZtoryAnimaticController::instance();
  connect(ctrl->frameHandle(), SIGNAL(frameSwitched()),
          this, SLOT(onModelChanged()));

  // Scene / model changes → refresh track
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &ZtoryMonitorPanel::onModelChanged);
  connect(TApp::instance()->getCurrentXsheet(),
          &TXsheetHandle::xsheetChanged,
          this, &ZtoryMonitorPanel::onModelChanged);
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  m_track->refreshFromScene();
  m_ruler->initPlayRangeIfNeeded();
  onModelChanged();
}

//-----------------------------------------------------------------------------
// Seek to the first frame of the clicked shot — no sub-scene opened.

void ZtoryMonitorPanel::onShotClicked(int col) {
  auto *ctrl = ZtoryAnimaticController::instance();
  for (const auto &block : m_track->blocks()) {
    if (block.col == col) {
      ctrl->setCurrentFrame(block.startFrameInMain);
      break;
    }
  }
}

//-----------------------------------------------------------------------------
// Double-click: same as single click — seek, do NOT open sub-scene.

void ZtoryMonitorPanel::onShotDoubleClicked(int col) {
  onShotClicked(col);
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::onFrameChanged(int frame) {
  ZtoryAnimaticController::instance()->setCurrentFrame(frame);
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::onZoomChanged(double ppf) {
  if (qAbs(ppf - m_ppf) < 0.001) return;
  m_ppf = ppf;
  m_ruler->setPixelsPerFrame(ppf);
  m_track->setPixelsPerFrame(ppf);
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::onModelChanged() {
  int frame = ZtoryAnimaticController::instance()->currentFrame();
  m_ruler->setCurrentFrame(frame);
  m_track->setCurrentFrame(frame);
  m_track->refreshFromScene();
}

//=============================================================================
// Factory
//=============================================================================

class ZtoryMonitorPanelFactory final : public TPanelFactory {
public:
  ZtoryMonitorPanelFactory() : TPanelFactory("ZtoryMonitorPanel") {}

  TPanel *createPanel(QWidget *parent) override {
    auto *panel = new ZtoryMonitorPanel(parent);
    panel->setObjectName("ZtoryMonitorPanel");
    panel->setWindowTitle("Ztoryc Monitor");
    return panel;
  }

  void initialize(TPanel *) override { assert(0); }

} ztoryMonitorPanelFactory;
