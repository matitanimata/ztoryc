#include "ztorymonitorpanel.h"

#include "ztorymodel.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/tframehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tcolumnhandle.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "toonzqt/gutil.h"
#include "xsheetdragtool.h"   // XsheetGUI::setPlayRange

#include <QApplication>
#include <QShowEvent>
#include <QTimer>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QToolButton>

//=============================================================================
// Helpers
//=============================================================================

static ZtoryAnimaticPanel *findAnimaticPanel() {
  for (QWidget *w : QApplication::allWidgets())
    if (auto *p = qobject_cast<ZtoryAnimaticPanel *>(w)) return p;
  return nullptr;
}

//=============================================================================
// ZtoryMonitorPanel
//=============================================================================

ZtoryMonitorPanel::ZtoryMonitorPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle(tr("Ztoryc Monitor"));
  setObjectName("ZtoryMonitorPanel");

  // Debounce timer: coalesces rapid model-changed signals into one refresh.
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(16);
  connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    m_ruler->setCurrentFrame(frame);
    m_track->setCurrentFrame(frame);
    for (auto *at : m_audioTracks)
      at->setCurrentFrame(frame);
    m_track->refreshFromScene();
    refreshAudioTracks();
  });

  // ── Viewer ────────────────────────────────────────────────────────────────
  m_viewer = new ZtoryAnimaticViewer(this);
  m_viewer->setMinimumHeight(80);
  m_viewer->initializeAnimaticTitleBar(getTitleBar());

  // ── Ruler + video track + audio tracks (timeline area) ───────────────────
  m_ruler = new ZtoryAnimaticRuler(this);
  m_ruler->setFixedHeight(24);
  m_ruler->setPixelsPerFrame(m_ppf);

  m_track = new ZtoryAnimaticTrack(this);
  m_track->setFixedHeight(52);
  m_track->setPixelsPerFrame(m_ppf);

  // ── Toolbar (between viewer and ruler, above the video track) ─────────────
  QWidget *toolbar = new QWidget(this);
  toolbar->setFixedHeight(28);
  toolbar->setStyleSheet("background:#2a2a2a;");
  QHBoxLayout *tbLay = new QHBoxLayout(toolbar);
  tbLay->setContentsMargins(4, 2, 4, 2);
  tbLay->setSpacing(2);

  // ── Context chip: "MONITOR" — visual orientation badge ───────────────────
  {
    QLabel *chip = new QLabel("MONITOR", toolbar);
    chip->setStyleSheet(
        "QLabel{"
        "  background:#3a1a5c;"        // deep purple
        "  color:#c07af0;"
        "  font-size:9px;"
        "  font-weight:bold;"
        "  letter-spacing:1px;"
        "  border-radius:3px;"
        "  padding:1px 6px;"
        "}");
    chip->setFixedHeight(18);
    chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    tbLay->addWidget(chip);
    tbLay->addSpacing(6);
  }
  // ─────────────────────────────────────────────────────────────────────────

  QLabel *zoomLabel = new QLabel("Zoom:", toolbar);
  zoomLabel->setStyleSheet("color:#aaa;font-size:11px;");
  tbLay->addWidget(zoomLabel);

  m_zoomSlider = new QSlider(Qt::Horizontal, toolbar);
  m_zoomSlider->setRange(2, 20000);
  m_zoomSlider->setValue((int)(m_ppf * 100));
  m_zoomSlider->setMaximumWidth(120);
  m_zoomSlider->setToolTip("Zoom (pixels per frame)");
  tbLay->addWidget(m_zoomSlider);

  auto makeTbBtn = [&](const char *icon, const QString &tip, bool checkable = false) {
    QToolButton *btn = new QToolButton(toolbar);
    btn->setIcon(createQIcon(icon));
    btn->setIconSize(QSize(20, 20));
    btn->setFixedSize(28, 28);
    btn->setToolTip(tip);
    btn->setCheckable(checkable);
    btn->setStyleSheet(
        "QToolButton{background:transparent;border:none;border-radius:4px;}"
        "QToolButton:hover{background:#555;}"
        "QToolButton:checked{background:#666;}");
    return btn;
  };

  QToolButton *fitAllBtn = new QToolButton(toolbar);
  fitAllBtn->setText("[]");
  fitAllBtn->setFixedSize(26, 22);
  fitAllBtn->setToolTip(tr("Fit All (Ctrl+0)"));
  fitAllBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:3px;"
      "color:#aaa;font-size:10px;}"
      "QToolButton:hover{background:#555;}");
  tbLay->addWidget(fitAllBtn);
  tbLay->addSpacing(4);

  m_selectBtn = makeTbBtn("ztoryc_select",
      tr("Select  (S)\nClick to select shots, drag right edge to resize"), true);
  m_trimBtn   = makeTbBtn("ztoryc_trim",
      tr("Trim / Roll  (T)\nDrag seam between shots to roll the edit point"), true);
  m_razorBtn  = makeTbBtn("ztoryc_razor",
      tr("Razor  (C)\nClick to cut a shot at the cursor position"), true);
  m_selectBtn->setChecked(true);
  tbLay->addWidget(m_selectBtn);
  tbLay->addWidget(m_trimBtn);
  tbLay->addWidget(m_razorBtn);
  tbLay->addSpacing(8);

  QToolButton *addBtn    = makeTbBtn("ztoryc_add_shot",    tr("Add Shot after selection"));
  QToolButton *deleteBtn = makeTbBtn("ztoryc_delete_shot", tr("Delete selected shots  (Del)"));
  tbLay->addWidget(addBtn);
  tbLay->addWidget(deleteBtn);
  tbLay->addSpacing(4);

  QToolButton *mergeBtn = makeTbBtn("ztoryc_merge", tr("Merge selected shots"));
  tbLay->addWidget(mergeBtn);
  tbLay->addSpacing(8);

  QToolButton *copyBtn  = makeTbBtn("ztoryc_copy",  tr("Copy selected shots  (Ctrl+C)"));
  QToolButton *cloneBtn = makeTbBtn("ztoryc_clone", tr("Clone selected shots  (Ctrl+D)"));
  QToolButton *pasteBtn = makeTbBtn("ztoryc_paste", tr("Paste shots  (Ctrl+V)"));
  tbLay->addWidget(copyBtn);
  tbLay->addWidget(cloneBtn);
  tbLay->addWidget(pasteBtn);
  tbLay->addSpacing(8);

  m_timecodeBtn = new QToolButton(toolbar);
  m_timecodeBtn->setText("TC");
  m_timecodeBtn->setCheckable(true);
  m_timecodeBtn->setFixedSize(32, 22);
  m_timecodeBtn->setToolTip(tr("Toggle between frame numbers and timecode (MM:SS:FF)"));
  m_timecodeBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:3px;"
      "color:#aaa;font-size:10px;font-weight:bold;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#3a6a9a;color:#fff;border-color:#5599cc;}");
  tbLay->addWidget(m_timecodeBtn);
  tbLay->addSpacing(8);

  // Return to main xsheet button
  QToolButton *returnBtn = new QToolButton(toolbar);
  returnBtn->setText("←");
  returnBtn->setFixedSize(26, 22);
  returnBtn->setToolTip(tr("Return to main xsheet (close open sub-scene)"));
  returnBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:3px;"
      "color:#aaa;font-size:12px;font-weight:bold;}"
      "QToolButton:hover{background:#555;}");
  tbLay->addWidget(returnBtn);
  tbLay->addStretch(1);

  // ── Bottom area: toolbar + ruler + track + audio ──────────────────────────
  // The viewer lives in the top half of the splitter; the bottom half contains
  // the toolbar above the timeline (matching what the Animatic room shows but
  // without a scroll area — the monitor uses a simple fixed-height layout).
  QWidget *timelineWidget = new QWidget(this);
  m_timelineLay = new QVBoxLayout(timelineWidget);
  m_timelineLay->setContentsMargins(0, 0, 0, 0);
  m_timelineLay->setSpacing(0);
  m_timelineLay->addWidget(toolbar);
  m_timelineLay->addWidget(m_ruler);
  m_timelineLay->addWidget(m_track);
  // Audio tracks are inserted here dynamically by refreshAudioTracks().

  // ── Splitter: viewer (resizable) on top, timeline+toolbar at bottom ───────
  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->setChildrenCollapsible(false);
  splitter->addWidget(m_viewer);
  splitter->addWidget(timelineWidget);
  splitter->setStretchFactor(0, 1);  // viewer gets extra space
  splitter->setStretchFactor(1, 0);  // bottom stays at natural size

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  lay->addWidget(splitter, 1);
  setWidget(container);

  // ── Toolbar connections ───────────────────────────────────────────────────

  connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v) {
    onZoomChanged(v / 100.0);
  });
  connect(fitAllBtn, &QToolButton::clicked, this, [this]() {
    int maxFrame = 0;
    for (const auto &b : m_track->blocks())
      maxFrame = qMax(maxFrame, b.startFrameInMain + (b.f1 - b.f0 + 1));
    if (maxFrame <= 0) return;
    onZoomChanged(qBound(0.02, (double)qMax(m_track->width(), 80) / maxFrame, 200.0));
  });

  // Tool buttons — mutually exclusive, wired directly to our own track
  connect(m_selectBtn, &QToolButton::clicked, this, [this]() {
    m_track->setTool(ZtoryAnimaticTrack::SelectTool);
    m_selectBtn->setChecked(true);
    m_trimBtn->setChecked(false);
    m_razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(m_trimBtn, &QToolButton::clicked, this, [this]() {
    m_track->setTool(ZtoryAnimaticTrack::TrimTool);
    m_trimBtn->setChecked(true);
    m_selectBtn->setChecked(false);
    m_razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(m_razorBtn, &QToolButton::clicked, this, [this]() {
    m_track->setTool(ZtoryAnimaticTrack::RazorTool);
    m_razorBtn->setChecked(true);
    m_selectBtn->setChecked(false);
    m_trimBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(true);
  });

  connect(m_timecodeBtn, &QToolButton::toggled, this, [this](bool on) {
    m_ruler->setShowTimecode(on);
  });
  connect(returnBtn, &QToolButton::clicked,
          this, &ZtoryMonitorPanel::onReturnToMain);

  // Shot edit operations — the Monitor's track selection is already mirrored
  // to ZtoryModel::sharedSelection() (see selectionChanged connection below).
  // ZtoryAnimaticPanel methods fall back to sharedSelection when their own
  // track has nothing selected, so they operate on the Monitor's selection.
  connect(addBtn,    &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onAddShot(); });
  connect(deleteBtn, &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onDeleteShots(); });
  connect(mergeBtn,  &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onMergeShots(); });
  connect(copyBtn,   &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onCopyShots(); });
  connect(cloneBtn,  &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onCloneShots(); });
  connect(pasteBtn,  &QToolButton::clicked, this, [](){ if (auto *ap = findAnimaticPanel()) ap->onPasteShots(); });

  // ── Timeline connections ──────────────────────────────────────────────────

  // Monitor track selection → ZtoryModel::sharedSelection so the Animatic
  // panel operations (delete, add, copy, clone, cut, merge) can read it.
  connect(m_track, &ZtoryAnimaticTrack::selectionChanged,
          [](std::set<int> sel) {
            ZtoryModel::instance()->setSharedSelection(std::move(sel));
          });

  // Ruler ↔ track zoom sync
  connect(m_ruler, &ZtoryAnimaticRuler::zoomChanged,
          this, &ZtoryMonitorPanel::onZoomChanged);
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryMonitorPanel::onZoomChanged);

  // Shot navigation
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryMonitorPanel::onShotClicked);
  connect(m_track, &ZtoryAnimaticTrack::shotDoubleClicked,
          this, &ZtoryMonitorPanel::onShotDoubleClicked);
  connect(m_track, &ZtoryAnimaticTrack::returnToMainRequested,
          this, &ZtoryMonitorPanel::onReturnToMain);

  // Ruler seek → controller
  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryMonitorPanel::onFrameChanged);

  // Controller frame → ruler + track
  auto *ctrl = ZtoryAnimaticController::instance();
  connect(ctrl->frameHandle(), SIGNAL(frameSwitched()),
          this, SLOT(onModelChanged()));

  // Scene / model changes → refresh
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &ZtoryMonitorPanel::onModelChanged);
  connect(TApp::instance()->getCurrentXsheet(),
          &TXsheetHandle::xsheetChanged,
          this, &ZtoryMonitorPanel::onModelChanged);

  // Trim / edit operations — forward to ZtoryAnimaticPanel
  connect(m_track, &ZtoryAnimaticTrack::shotDurationChanged,
          this, [](int col, int newF1) {
    if (auto *ap = findAnimaticPanel()) ap->onShotDurationChanged(col, newF1);
  });
  connect(m_track, &ZtoryAnimaticTrack::rollEdit,
          this, [](int colA, int newDurA, int colB, int newDurB) {
    if (auto *ap = findAnimaticPanel()) ap->onRollEdit(colA, newDurA, colB, newDurB);
  });
  connect(m_track, &ZtoryAnimaticTrack::razorRequested,
          this, [](int col, int frame) {
    if (auto *ap = findAnimaticPanel()) ap->onRazorRequested(col, frame);
  });
  connect(m_track, &ZtoryAnimaticTrack::mergeWithNextRequested,
          this, [](int col) {
    if (auto *ap = findAnimaticPanel()) ap->onMergeWithNext(col);
  });
  connect(m_track, &ZtoryAnimaticTrack::shotMoved,
          this, [](int col, int newStartFrame) {
    if (auto *ap = findAnimaticPanel()) ap->onShotMoved(col, newStartFrame);
  });
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  m_track->refreshFromScene();
  m_ruler->initPlayRangeIfNeeded();
  m_audioFP = 0;
  refreshAudioTracks();
  onModelChanged();
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::refreshAudioTracks() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Fingerprint: avoid full rebuild when nothing changed.
  size_t fp = 1469598103934665603ULL;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
    if (!sc || sc->isEmpty()) continue;
    fp = (fp ^ reinterpret_cast<size_t>(sc)) * 1099511628211ULL;
  }
  if (fp == m_audioFP) return;
  m_audioFP = fp;

  for (auto *at : m_audioTracks) {
    m_timelineLay->removeWidget(at);
    at->hide();
    at->deleteLater();
  }
  m_audioTracks.clear();

  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc || sc->isEmpty()) continue;

    QString name = QString::fromStdString(
        xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (name.isEmpty()) name = tr("Audio %1").arg(col + 1);

    ZtoryAudioTrack *at = new ZtoryAudioTrack(col, name, this);
    at->setPixelsPerFrame(m_ppf);
    at->setCurrentFrame(ZtoryAnimaticController::instance()->currentFrame());
    connect(at, &ZtoryAudioTrack::zoomChanged,
            this, &ZtoryMonitorPanel::onZoomChanged);
    m_timelineLay->addWidget(at);
    m_audioTracks.append(at);
  }
}

//-----------------------------------------------------------------------------

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
// Double-click: open the shot sub-scene in the main application context.

void ZtoryMonitorPanel::onShotDoubleClicked(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  app->getCurrentColumn()->setColumnIndex(col);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1, /*ignoreLastStop=*/true);
  TXshCell cell = xsh->getCell(r0, col);
  if (cell.m_level && cell.m_level->getChildLevel()) {
    app->getCurrentFrame()->setFrame(r0);
    CommandManager::instance()->execute("MI_OpenChild");
    int durInAnimatic = r1 - r0 + 1;
    int outF = durInAnimatic - 1;
    if (outF >= 0)
      XsheetGUI::setPlayRange(0, outF, 1, false);
    ZtoryAnimaticController::instance()->setCurrentFrame(r0);
    ZtoryModel::instance()->activateShotForViewing(col);
  }
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::onReturnToMain() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  ZtoryModel::instance()->requestReturnToViewer();
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
  for (auto *at : m_audioTracks)
    at->setPixelsPerFrame(ppf);
  int sliderVal = (int)(ppf * 100);
  if (m_zoomSlider->value() != sliderVal) {
    QSignalBlocker blocker(m_zoomSlider);
    m_zoomSlider->setValue(qBound(m_zoomSlider->minimum(), sliderVal,
                                  m_zoomSlider->maximum()));
  }
}

//-----------------------------------------------------------------------------

void ZtoryMonitorPanel::onModelChanged() {
  m_refreshTimer->start();
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
