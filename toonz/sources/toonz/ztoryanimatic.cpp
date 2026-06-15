
#include "ztoryanimatic.h"
#include "ztorylightgizmo.h"
#include "viewerpane.h"
#include "comboviewerpane.h"
#include "ztorymodel.h"
#include "ztoryshotops.h"
#include "toonz/tcolumnhandle.h"
#include "subscenecommand.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "previewer.h"
#include "columncommand.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/navigationtags.h"
#include "navtageditorpopup.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "iocommand.h"
#include "xsheetdragtool.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "orientation.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QMenu>
#include <QInputDialog>
#include <QLabel>
#include <QWindow>
#include <QSettings>
#include <QColorDialog>
#include <cmath>
#include <QFileDialog>
#include <QContextMenuEvent>
#include "tsound.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectid.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"
#include "toonzqt/stageobjectsdata.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/fxdag.h"
#include <thread>
extern ToggleCommandHandler mainAudioToggle;
#include "toonzqt/selectioncommandids.h"
#include "pane.h"
#include "mainwindow.h"
#include "storyboardpanel.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "../tnztools/symmetrytool.h"
#include "../tnztools/perspectivetool.h"
#include "tundo.h"
#include "tpanels.h"
#include "ztoryscriptpanel.h"
#include "filebrowser.h"
#include "toonz/stage.h"
#include <QSplitter>
#include <QApplication>

// Shared label column width — must match ZtoryAudioTrack::labelW (80px).
// Used by ZtoryAnimaticRuler and ZtoryAnimaticTrack to align with audio tracks.
static constexpr int    kLabelW  = 80;
// Zoom limits (pixels per frame).
// kMinPpf = 0.02 supports a 26-minute episode (37 440 frames) in a ~750 px
// viewport without scrolling.  kMaxPpf = 200 gives single-frame editing.
static constexpr double kMinPpf  = 0.02;
static constexpr double kMaxPpf  = 200.0;

// ── Cross-dissolve note columns ──────────────────────────────────────────────
// The XD note columns (SoundText) physically persist inside each sub-scene in
// the .tnz, so they are the single source of truth for transitions: the .ztoryc
// transitionFrames value is only a convenience mirror.  All display and mark-out
// logic derives the transition length by inspecting these columns.
static const char *kXDOutName = "XD-out"; // tail extra frames on outgoing shot A
static const char *kXDInName  = "XD-in";  // head extra frames on incoming shot B

// Returns the number of rows in the named XD note column of a sub-scene (= T/2),
// or 0 if no such column exists.
static int xdNoteHalfCount(TXsheet *subXsh, const char *name) {
  if (!subXsh) return 0;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    TXshColumn *col = subXsh->getColumn(c);
    if (!col || !col->getSoundTextColumn()) continue;
    std::string colName =
        subXsh->getStageObject(subXsh->getColumnObjectId(c))->getName();
    if (colName == name) {
      int r0 = 0, r1 = 0;
      col->getRange(r0, r1);
      return (r1 >= r0) ? (r1 - r0 + 1) : 0;
    }
  }
  return 0;
}

// ── Snap ──────────────────────────────────────────────────────────────────────
// Snap a frame to the nearest target within kSnapPx screen pixels.  Used by the
// audio tracks and the video track when the magnet toggle is on.  Targets are
// shot boundaries, the playhead and other audio-segment edges (computed by the
// panel and pushed to each widget).
static constexpr int kSnapPx = 8;
static int snapToTargets(int frame, double ppf, const QVector<int> &targets,
                         bool enabled) {
  if (!enabled || targets.isEmpty() || ppf <= 0.0) return frame;
  int best = frame;
  double bestDistPx = kSnapPx + 1.0;
  for (int t : targets) {
    double distPx = std::abs((double)(frame - t) * ppf);
    if (distPx < bestDistPx) { bestDistPx = distPx; best = t; }
  }
  return (bestDistPx <= kSnapPx) ? best : frame;
}

// Find the live StoryboardPanel instance for undo/redo from the Animatic.
// Searches the whole widget tree since the Board can be embedded or floating.
static StoryboardPanel *findBoardPanel() {
  for (QWidget *w : QApplication::allWidgets()) {
    if (auto *b = qobject_cast<StoryboardPanel *>(w)) return b;
  }
  return nullptr;
}


// ---- ZtoryAnimaticController ----

ZtoryAnimaticController::ZtoryAnimaticController() : QObject() {
  m_frameHandle = new TFrameHandle();
  m_frameHandle->setFrame(0);
  // Monitor native-viewer play state so we can stream main-xsheet audio
  // when the user is editing a shot (sub-scene open) and presses play.
  // The signal fires on the main thread, so this is safe to call from here.
  connect(TApp::instance()->getCurrentFrame(),
          &TFrameHandle::isPlayingStatusChanged,
          this, &ZtoryAnimaticController::onNativePlayingStatusChanged);
  connect(TApp::instance()->getCurrentFrame(),
          &TFrameHandle::frameSwitched,
          this, &ZtoryAnimaticController::onNativeFrameSwitched);
}

ZtoryAnimaticController::~ZtoryAnimaticController() {
  delete m_scrubDevice;
  m_scrubDevice = nullptr;
}

ZtoryAnimaticController *ZtoryAnimaticController::instance() {
  static ZtoryAnimaticController ctrl;
  return &ctrl;
}

TSoundOutputDevice *ZtoryAnimaticController::scrubDevice() {
  if (!m_scrubDevice) m_scrubDevice = new TSoundOutputDevice();
  return m_scrubDevice;
}

TXsheet *ZtoryAnimaticController::mainXsheet() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return nullptr;
  return scene->getChildStack()->getTopXsheet();
}

// Returns true when the animatic viewer is alive AND we are at the main level
// (not inside a sub-scene).  In that case the animatic owns audio and the
// native ComboViewer must yield — otherwise they both call TXsheet::play()
// on the same TSoundOutputDevice and corrupt each other's buffer.
bool ZtoryAnimaticController::ownsAudioAtMainLevel() const {
  if (!ZtoryModel::instance()->isStoryboardWorkflow()) return false;
  if (!m_viewer) return false;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return false;
  return scene->getChildStack()->getAncestorCount() == 0;
}

// Returns true when the user is inside a sub-scene (ancestorCount==1) AND
// the main xsheet has at least one non-empty audio column. In that case
// onNativePlayingStatusChanged() streams main-xsheet audio at the correct
// time offset; the native ComboViewer's playAudioFrame() must yield, or the
// user hears two overlapping audio streams (one from sample 0 because the
// native code reads from the current — sub — xsheet at frame 0, plus the
// controller's main-xsheet stream from mainFrame*spf).
bool ZtoryAnimaticController::ownsSubSceneAudio() const {
  // Desired behaviour (mirrors the video toggle):
  //   toggle ON  → hear the MAIN xsheet soundtrack only
  //   toggle OFF → hear the CURRENT sub-scene's own soundtrack
  //
  // OFF → the controller does NOT stream main audio; the native ComboViewer
  // plays the sub-scene's own audio.
  if (!TXsheet::isMainAudioEnabled()) return false;
  // ON, inside a sub-scene at ANY nesting depth → the controller overlays the
  // main soundtrack; the native viewer must yield so the two don't overlap.
  // Returns true even when the main xsheet has no audio — ON means "main
  // only", a silent main included.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return false;
  return scene->getChildStack()->getAncestorCount() > 0;
}

// ---- ZtoryAnimaticController::setAnimaticPlayRangeAndSync ----
// Sets the animatic-owned play range AND mirrors it to XsheetGUI (native
// viewer) so both are in sync while at main level.  Ruler and marker code
// should call this instead of XsheetGUI::setPlayRange() directly.
void ZtoryAnimaticController::setAnimaticPlayRange(int r0, int r1) {
  m_animaticR0 = r0;
  m_animaticR1 = r1;
  // Do NOT call XsheetGUI::setPlayRange() here — that would propagate the
  // animatic marker changes to the ComboViewer and the XsheetViewer, which
  // must keep their own independent play range (sub-scene or main xsheet).
  // The animatic FlipConsole gets its markers directly via
  // updateAnimaticFrameMarkers() → m_flipConsole->setMarkers().
  emit playRangeChanged();
}

// Helper: frame count from VIDEO columns only (ignores sound columns).
// mainXsh->getFrameCount() includes audio columns; after a razor cut the
// trailing ColumnLevel keeps endOffset=0 (= full raw file length), inflating
// the count far beyond the actual video range.
static int videoFrameCount(TXsheet *xsh) {
  if (!xsh) return 1;
  int maxFrame = 0;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *col = xsh->getColumn(c);
    if (!col || col->getSoundColumn()) continue;
    int r0, r1;
    // ignoreLastStop=true: skip the trailing SFH placed by
    // ZtoryModel::resequenceXsheet() so total video frame count reflects
    // the animatic's actual play length, not +1 per shot.
    if (col->getRange(r0, r1, /*ignoreLastStop=*/true))
      maxFrame = std::max(maxFrame, r1 + 1);
  }
  return maxFrame > 0 ? maxFrame : 1;
}

void ZtoryAnimaticController::setCurrentFrame(int frame) {
  if (frame < 0) frame = 0;
  m_frameHandle->setFrame(frame);
}

int ZtoryAnimaticController::currentFrame() const {
  return m_frameHandle->getFrame();
}

void ZtoryAnimaticController::startPerColumnAudio(int startMainFrame) {
  validateSoundCache();  // drop stale per-column cache if tracks changed
  TXsheet *xsh = mainXsheet();
  if (!xsh) return;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  if (!TXsheet::isMainAudioEnabled()) return;
  if (!TSoundOutputDevice::installed()) return;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *column = xsh->getColumn(c);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc || sc->isEmpty()) continue;
    TSoundTrackP colTrack = requireColumnSoundTrack(c);
    if (!colTrack) continue;
    double colSpf = colTrack->getSampleRate() / fps;
    TINT32 ts0 = (TINT32)(startMainFrame * colSpf);
    TINT32 colSamples = (TINT32)colTrack->getSampleCount();
    if (ts0 >= colSamples) continue;
    TINT32 ts1 = colSamples - 1;
    if (ts1 <= ts0) continue;
    sc->play(colTrack, ts0, ts1, false);
  }
}

void ZtoryAnimaticController::stopPerColumnAudio() {
  TXsheet *xsh = mainXsheet();
  if (!xsh) return;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *column = xsh->getColumn(c);
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (sc) sc->stop();
  }
}

qint64 ZtoryAnimaticController::getMasterAudioUsecs() const {
  TXsheet *xsh = mainXsheet();
  if (!xsh) return 0;
  // First non-empty audio column with an active player.
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *column = xsh->getColumn(c);
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (!sc || sc->isEmpty()) continue;
    qint64 us = sc->getProcessedUsecs();
    if (us > 0) return us;
  }
  return 0;
}

TSoundTrackP ZtoryAnimaticController::requireColumnSoundTrack(int col) {
  TXsheet *xsh = mainXsheet();
  if (!xsh) return TSoundTrackP();
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return TSoundTrackP();
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc || sc->isEmpty()) return TSoundTrackP();
  // Cache is keyed by the sound-column pointer, resolved AFTER we have a
  // valid column — index keying aliased to the wrong column after a
  // delete/reorder.
  auto it = m_columnSoundTracks.find(sc);
  if (it != m_columnSoundTracks.end() && it->second) return it->second;
  TSoundTrackP track;
  try {
    // Pass fromFrame=0 explicitly so the resulting track is indexed by
    // absolute main-xsheet frame: sample 0 == main frame 0.  Without this,
    // getOverallSoundTrack defaults to fromFrame = getFirstRow() (the column's
    // first non-empty row), making sample 0 == that row's frame.  Then the
    // per-frame mapping `mainFrame * spf` reads the wrong portion of the
    // track — for two columns whose first rows differ from 0 we'd hear them
    // both from sample 0 ("frame 1") regardless of clip placement.
    //
    // CRITICAL: cap toFrame at videoFrameCount() — NOT sc->getMaxFrame().
    // sc->getMaxFrame() returns the end frame of the audio FILE (can be hours).
    // If an audio file is 2 hours long but the video is 1700 frames,
    // getOverallSoundTrack(0, 172800) allocates ~1.27 GB per track.
    // 18 such tracks = 23 GB.  We only need audio up to the last video frame.
    int audioEnd = std::max(sc->getMaxFrame(), 0);
    int vidEnd   = videoFrameCount(xsh) - 1;
    int toFrame  = std::min(audioEnd, vidEnd);
    if (toFrame < 0) toFrame = 0;
    track = sc->getOverallSoundTrack(0, toFrame);
  } catch (...) {
    track = TSoundTrackP();
  }
  m_columnSoundTracks[sc] = track;
  return track;
}

// Collect all non-empty sound columns from |xsh| into |out|.
static void collectSoundColumns(TXsheet *xsh,
                                std::vector<TXshSoundColumn *> &out) {
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh ? xsh->getColumn(col) : nullptr;
    TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
    if (sc && !sc->isEmpty()) out.push_back(sc);
  }
}

// Cheap fingerprint of the sound columns: order + pointer + length.  Changes
// whenever a track is added, removed, reordered or edited.
static size_t soundColumnsFingerprint(TXsheet *xsh) {
  size_t h = 1469598103934665603ULL;  // FNV-ish seed
  if (!xsh) return h;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
    if (!sc || sc->isEmpty()) continue;
    h = (h ^ reinterpret_cast<size_t>(sc)) * 1099511628211ULL;
    h = (h ^ (size_t)std::max(sc->getMaxFrame(), 0)) * 1099511628211ULL;
  }
  return h;
}

// Self-invalidate the audio caches if the live sound columns no longer match
// what was cached.  Catch-all: any add/remove/reorder/edit path is covered
// without having to hook each call site.
void ZtoryAnimaticController::validateSoundCache() {
  size_t fp = soundColumnsFingerprint(mainXsheet());
  if (fp != m_soundFingerprint) {
    invalidateSoundTrack();
    m_soundFingerprint = fp;
  }
}

TSoundTrackP ZtoryAnimaticController::requireSoundTrack() {
  validateSoundCache();  // drop stale cache if tracks changed
  if (m_soundTrack) return m_soundTrack;
  TXsheet *xsh = mainXsheet();
  if (!xsh) return TSoundTrackP();
  try {
    // CRITICAL: call mixingTogether() directly instead of xsh->makeSound().
    // makeSound() writes to xsh->m_imp->m_mixedSound — a shared non-atomic
    // pointer that preBuildSoundTrackAsync() also writes from a background
    // thread.  Concurrent writes cause reference-count corruption → unbounded
    // memory leak (observed: 40 GB+ on long scenes).
    // mixingTogether() is read-only on the ColumnLevel list and returns a
    // fresh TSoundTrackP that we own privately — no shared state touched.
    std::vector<TXshSoundColumn *> sounds;
    collectSoundColumns(xsh, sounds);
    if (sounds.empty()) return TSoundTrackP();
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    double fps = scene
        ? scene->getProperties()->getOutputProperties()->getFrameRate()
        : 24.0;
    int toFrame = videoFrameCount(xsh) - 1;
    if (toFrame < 0) return TSoundTrackP();
    m_soundTrack = sounds[0]->mixingTogether(sounds, 0, toFrame, fps);
  } catch (...) {
    m_soundTrack = TSoundTrackP();
  }
  return m_soundTrack;
}

// ---- ZtoryAnimaticController::preBuildSoundTrackAsync ----
// Starts a detached std::thread that calls makeSound() in the background.
// The result is delivered to the main thread via QMetaObject::invokeMethod
// with Qt::QueuedConnection — no main-thread blocking, no mutex needed.
// Thread safety note: makeSound() is read-only on the xsheet's ColumnLevel list.
// We accept the small race window; a stale or null result is harmless because
// requireSoundTrack() will rebuild synchronously on first play if needed.
void ZtoryAnimaticController::preBuildSoundTrackAsync() {
  validateSoundCache();  // drop stale cache before deciding to skip
  if (m_soundTrack || m_soundBuildPending) return;
  TXsheet *xsh = mainXsheet();
  if (!xsh) return;
  int toFrame = videoFrameCount(xsh) - 1;
  if (toFrame < 0) return;

  // Collect sound columns on the main thread before spawning (safe, xsh stable).
  std::vector<TXshSoundColumn *> sounds;
  collectSoundColumns(xsh, sounds);
  if (sounds.empty()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  double fps = scene
      ? scene->getProperties()->getOutputProperties()->getFrameRate()
      : 24.0;

  m_soundBuildPending = true;

  // Capture the current generation.  invalidateSoundTrack() bumps it; if it
  // changes before this build finishes (scene switched, track deleted, …) the
  // result belongs to a stale scene and must be discarded — otherwise audio
  // from a previously open scene leaks into the current one.
  unsigned gen = m_soundGen;

  // CRITICAL: call mixingTogether() directly — never call xsh->makeSound()
  // from a background thread.  makeSound() writes to xsh->m_imp->m_mixedSound
  // which the main thread also writes; concurrent non-atomic writes on the
  // TSoundTrackP reference count → memory leak → 40 GB+ RAM on long scenes.
  // mixingTogether() only reads the ColumnLevel list and audio sample data —
  // safe to call concurrently as long as the main thread isn't editing the
  // same columns simultaneously (invariant: this thread is spawned at rest).
  ZtoryAnimaticController *ctrl = this;
  std::thread([ctrl, sounds, toFrame, fps, gen]() {
    TSoundTrackP st;
    try {
      st = sounds[0]->mixingTogether(sounds, 0, toFrame, fps);
    } catch (...) {}
    // Post result to main thread via QueuedConnection (thread-safe).
    QMetaObject::invokeMethod(ctrl, [ctrl, st, gen]() {
      ctrl->m_soundBuildPending = false;
      // Discard if a newer invalidation happened while we were building.
      if (gen != ctrl->m_soundGen) return;
      if (!ctrl->m_soundTrack) ctrl->m_soundTrack = st;
    }, Qt::QueuedConnection);
  }).detach();
}

// ---- ZtoryAnimaticController::onNativePlayingStatusChanged ----
// Called whenever the NATIVE viewer (ComboViewer / FlipConsole) starts or
// stops playback via TApp::getCurrentFrame()->isPlayingStatusChanged.
//
// Goal: when the user is editing a shot (sub-scene open, ancestorCount == 1)
// and presses play, stream the main-xsheet audio at the correct offset so
// they can hear the soundtrack while animating to picture.
//
// Guard: if the animatic viewer is already playing it handles its own audio
// (continuous-play mode) — we must not start a second concurrent stream.
void ZtoryAnimaticController::onNativePlayingStatusChanged() {
  TFrameHandle *fh     = TApp::instance()->getCurrentFrame();
  TXsheet      *mainXsh = mainXsheet();

  if (!fh->isPlaying()) {
    // Playback stopped — stop any audio we started.
    if (mainXsh) mainXsh->stopScrub();
    stopPerColumnAudio();
    m_nativeAudioPlaying  = false;
    m_lastNativePlayFrame = -1;
    return;
  }
  // Play start: no work here.  onNativeFrameSwitched plays each frame's
  // audio per-frame, so audio always follows the current frame mapping
  // (like the video display).  This avoids the "permanent mute after a
  // silent stretch" caused by buffering the entire range up-front.
}

void ZtoryAnimaticController::restartNativeAudioIfPlaying() {
  // Re-enter the same logic as onNativePlayingStatusChanged — it already guards
  // against isMainAudioEnabled() == false, animatic playing, wrong depth, etc.
  onNativePlayingStatusChanged();
}

// ---- ZtoryAnimaticController::onNativeFrameSwitched ----
// Provides per-frame scrub audio from the main xsheet when the user drags
// the playhead inside a shot sub-scene.  Mirrors the per-frame scrub logic
// in ZtoryAnimaticViewer::playAnimaticAudioFrame but uses the native frame
// handle and maps the sub-scene frame through ChildStack::getAncestor().
void ZtoryAnimaticController::onNativeFrameSwitched() {
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  // The animatic owning audio always wins.
  if (m_viewer && m_viewer->isContinuousPlaying()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  ChildStack *cs = scene->getChildStack();
  // Inside a sub-scene at ANY nesting depth.  At main level the FlipConsole's
  // normal audio path handles things.  ChildStack::getAncestor() already
  // chains the row mapping through every stack level, so depth 2+ works.
  if (!cs || cs->getAncestorCount() < 1) return;

  TSoundTrackP st = requireSoundTrack();
  TXsheet *mainXsh = mainXsheet();
  if (!st || !mainXsh) return;

  // Map the current sub-frame all the way up to its main-xsheet row.  If the
  // mapping does not reach the main xsheet, the sub-frame has no main audio
  // source — silent.
  int subFrame = fh->getFrame();
  std::pair<TXsheet *, int> ancestor = cs->getAncestor(subFrame);
  if (ancestor.first != mainXsh) return;
  int mainFrame = ancestor.second;

  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf = st->getSampleRate() / fps;

  TINT32 s0 = (TINT32)(mainFrame * spf);
  // Minimum audible scrub chunk — one frame (~41 ms at 24 fps) is too short.
  double scrubLen = std::max(spf, st->getSampleRate() * 0.15);
  TINT32 totalSamples = (TINT32)st->getSampleCount();
  if (s0 >= totalSamples) return;

  if (!TXsheet::isMainAudioEnabled()) return;
  if (!TSoundOutputDevice::installed()) return;

  // Per-frame audio: each sub-frame plays one frame of its mapped main-
  // xsheet audio.  Mirrors the video frame mapping — when the sub-scene
  // shows main frame N, we hear main frame N's audio.
  if (fh->isPlaying()) {
    // During play, push 1 frame of audio per audio column on its OWN
    // TSoundOutputDevice (sc->m_player).  This gives each track an
    // independent QAudioOutput whose volume can be changed in real time
    // via TXshSoundColumn::setVolume() → m_player->setVolume().  CoreAudio
    // mixes the per-track outputs in hardware, so multiple tracks play in
    // sync without us having to rebuild a baked mix on every volume change.
    for (int c = 0; c < mainXsh->getColumnCount(); c++) {
      TXshColumn *column = mainXsh->getColumn(c);
      if (!column) continue;
      TXshSoundColumn *sc = column->getSoundColumn();
      if (!sc || sc->isEmpty()) continue;
      TSoundTrackP colTrack = requireColumnSoundTrack(c);
      if (!colTrack) continue;
      double colSpf = colTrack->getSampleRate() / fps;
      TINT32 ts0 = (TINT32)(mainFrame * colSpf);
      TINT32 ts1 = (TINT32)(ts0 + colSpf);
      TINT32 colSamples = (TINT32)colTrack->getSampleCount();
      if (ts0 >= colSamples) continue;
      if (ts1 >= colSamples) ts1 = colSamples - 1;
      sc->play(colTrack, ts0, ts1, false);
    }
    m_nativeAudioPlaying = true;
  } else {
    // Scrub: play a short (~150 ms) audible chunk of the merged main soundtrack
    // on the MAIN xsheet's OWN sound device — exactly the path the animatic
    // ruler scrub uses (ZtoryAnimaticRuler::mouseMoveEvent), which works.
    //
    // The previous implementation used scrubDevice() — a bare
    // TSoundOutputDevice that is never opened/configured with the audio format,
    // so inside a shot it produced NO sound (play branch works because it uses
    // each column's own managed device).  Routing through mainXsh->play()
    // reuses the xsheet's properly-initialised player.
    TINT32 sg0 = (TINT32)(mainFrame * spf);
    TINT32 sg1 = (TINT32)(sg0 + scrubLen);
    if (sg0 >= totalSamples) return;
    if (sg1 > totalSamples) sg1 = totalSamples;
    if (sg0 < sg1) mainXsh->play(st, sg0, sg1, false);
  }
}

// ---- ZtoryAnimaticRuler ----

ZtoryAnimaticRuler::ZtoryAnimaticRuler(QWidget *parent) : QWidget(parent) {
  setFixedHeight(18);
  setMouseTracking(true);
}

void ZtoryAnimaticRuler::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const int h = height();
  const int rulerY = 0;
  const int rulerH = h;

  // Background
  p.fillRect(rect(), QColor(40, 40, 40));
  p.fillRect(0, 0, kLabelW, h, QColor(25, 25, 25));
  p.setPen(QColor(60, 60, 60));
  p.drawLine(kLabelW, 0, kLabelW, h);

  // ---- In/Out range highlight ----
  // Read from the animatic-owned range — independent from XsheetGUI which
  // gets overwritten by the native viewer when entering/leaving sub-scenes.
  auto *ctrl = ZtoryAnimaticController::instance();
  int r0, r1;
  ctrl->getAnimaticPlayRange(r0, r1);
  bool rangeEnabled = (r0 <= r1 && r1 >= 0);
  if (!rangeEnabled) {
    TXsheet *mainXsh = ctrl->mainXsheet();
    r0 = 0;
    r1 = mainXsh ? std::max(0, videoFrameCount(mainXsh) - 1) : 0;
  }
  if (r1 >= r0) {
    int x0 = kLabelW + (int)(r0 * m_ppf);
    int x1 = kLabelW + (int)((r1 + 1) * m_ppf);
    p.fillRect(x0, rulerY, x1 - x0, rulerH,
               QColor(255, 165, 0, rangeEnabled ? 45 : 20));
  }

  // ---- Tick marks ----
  // Choose label interval dynamically: smallest "nice" interval (in frames) such
  // that adjacent labels are at least kMinLabelPx apart.
  // Intervals follow animation-friendly units: frames → seconds (×24) → minutes.
  p.setFont(QFont("", 8));
  p.setPen(QColor(180, 180, 180));
  int w = width() - kLabelW;
  // Fully adaptive tick spacing — fps-agnostic, works in both directions:
  //   zoom in  → labels every 1/2/5/10 frames (frame-accurate at high zoom)
  //   zoom out → labels every 25/50/100/250/... frames (no crowding)
  // Series: 1 2 5 10 25 50 100 250 500 1000 ... (base-10/5, universally readable)
  p.setFont(QFont());  // use application default font (same as original)
  static const int kNice[] = {
      1, 2, 5, 10, 25, 50, 100, 250, 500,
      1000, 2500, 5000, 10000, 25000, 50000
  };
  const int kMinLabelPx = 40;  // minimum pixels between two label centres
  int labelEvery = kNice[0];
  for (int iv : kNice) {
    if ((int)(iv * m_ppf) >= kMinLabelPx) { labelEvery = iv; break; }
  }
  // Minor ticks: next finer level, at least 4 px apart (0 = no minor ticks)
  int tickEvery = 0;
  for (int iv : kNice) {
    if (iv >= labelEvery) break;
    if ((int)(iv * m_ppf) >= 4) tickEvery = iv;
  }

  // Label formatter: frame number or timecode (HH:MM:SS:FF)
  auto fmtFrame = [&](int f) -> QString {
    if (!m_showTimecode || m_fps <= 0) return QString::number(f);
    int fps   = qMax(1, (int)qRound(m_fps));
    int fr    = f % fps;
    int total = f / fps;
    int ss    = total % 60;
    int mm    = (total / 60) % 60;
    int hh    = total / 3600;
    if (hh > 0)
      return QString("%1:%2:%3:%4")
          .arg(hh).arg(mm,2,10,QChar('0')).arg(ss,2,10,QChar('0')).arg(fr,2,10,QChar('0'));
    return QString("%1:%2:%3")
        .arg(mm,2,10,QChar('0')).arg(ss,2,10,QChar('0')).arg(fr,2,10,QChar('0'));
  };

  for (int f = 0; (int)(f * m_ppf) < w; f++) {
    int x = kLabelW + (int)(f * m_ppf);
    if (f % labelEvery == 0) {
      p.drawLine(x, rulerY, x, rulerY + 12);
      p.drawText(x + 2, rulerY + 11, fmtFrame(f));
    } else if (tickEvery > 0 && f % tickEvery == 0) {
      p.drawLine(x, rulerY + 6, x, rulerY + 12);
    }
  }

  // ---- In/Out triangular markers ----
  static const int kM = 8;
  if (r1 >= r0) {
    int x0 = kLabelW + (int)(r0 * m_ppf);
    int x1 = kLabelW + (int)((r1 + 1) * m_ppf);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 200, 0));
    QPolygon inTri;
    inTri << QPoint(x0, rulerY) << QPoint(x0 + kM, rulerY) << QPoint(x0, rulerY + kM);
    p.drawConvexPolygon(inTri);
    QPolygon outTri;
    outTri << QPoint(x1, rulerY) << QPoint(x1 - kM, rulerY) << QPoint(x1, rulerY + kM);
    p.drawConvexPolygon(outTri);
  }

  // ---- Navigation tag markers ----
  // Drawn from the MAIN xsheet's navigation tags (Tahoma2D's native tag system),
  // so markers persist with the scene and are shared with the native xsheet.
  {
    TXsheet *tagXsh = ctrl->mainXsheet();
    NavigationTags *nav = tagXsh ? tagXsh->getNavigationTags() : nullptr;
    if (nav) {
      QFont mf("", 8, QFont::Bold);
      p.setFont(mf);
      QFontMetrics fm(mf);
      for (auto &tag : nav->getTags()) {
        int tx = kLabelW + (int)(tag.m_frame * m_ppf) + (int)(m_ppf / 2);
        if (tx < kLabelW || (tx - kLabelW) > w) continue;
        QColor c = tag.m_color.isValid() ? tag.m_color
                                         : QColor(0xE0, 0x24, 0x9B);
        // Faint guide line from the pin tip down to the bottom of the ruler.
        p.setPen(QPen(c, 1));
        p.drawLine(tx, rulerY + 13, tx, h);
        // Downward pin — same shape as Tahoma2D's native navigation tag
        // (PredefinedPath::NAVIGATION_TAG, horizontal-timeline variant).
        QPainterPath pin(QPointF(tx - 3, rulerY));
        pin.lineTo(QPointF(tx + 3, rulerY));
        pin.lineTo(QPointF(tx + 3, rulerY + 9));
        pin.lineTo(QPointF(tx,     rulerY + 13));
        pin.lineTo(QPointF(tx - 3, rulerY + 9));
        pin.closeSubpath();
        p.setPen(QPen(Qt::black, 1));
        p.setBrush(c);
        p.drawPath(pin);
        // Label shown only while hovering this marker — drawn on a translucent
        // dark chip for readability so it never clutters the ruler otherwise.
        if (!tag.m_label.isEmpty() && tag.m_frame == m_hoverTagFrame) {
          int tw = fm.horizontalAdvance(tag.m_label);
          QRect chip(tx + 6, rulerY, tw + 6, h);
          p.setBrush(QColor(0, 0, 0, 180));
          p.setPen(Qt::NoPen);
          p.drawRoundedRect(chip, 2, 2);
          p.setPen(c.lighter(140));
          p.drawText(chip.adjusted(3, 0, -3, 0),
                     Qt::AlignVCenter | Qt::AlignLeft, tag.m_label);
        }
      }
    }
  }

  // ---- Playhead — downward triangle + line ----
  static const int kPH = 8;
  int px = kLabelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 100, 0));
  QPolygon tri;
  tri << QPoint(px - 5, rulerY) << QPoint(px + 5, rulerY) << QPoint(px, rulerY + kPH);
  p.drawConvexPolygon(tri);
  p.setPen(QPen(QColor(255, 100, 0), 1));
  p.drawLine(px, rulerY + kPH, px, h);
}

void ZtoryAnimaticRuler::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) return;

  int mx    = qMax(0, e->x() - kLabelW);
  int frame = (int)(mx / m_ppf);

  auto *ctrlR = ZtoryAnimaticController::instance();

  // Shift+click = set In, Alt+click = set Out
  if (e->modifiers() & Qt::ShiftModifier) {
    int r0, r1;
    ctrlR->getAnimaticPlayRange(r0, r1);
    if (r0 > r1) { r0 = frame; r1 = frame; }
    ctrlR->setAnimaticPlayRange(frame, std::max(frame, r1));
    ctrlR->notifyPlayRangeChanged();
    update();
    return;
  }
  if (e->modifiers() & Qt::AltModifier) {
    int r0, r1;
    ctrlR->getAnimaticPlayRange(r0, r1);
    if (r0 > r1) { r0 = frame; r1 = frame; }
    ctrlR->setAnimaticPlayRange(std::min(r0, frame), frame);
    ctrlR->notifyPlayRangeChanged();
    update();
    return;
  }

  // Hit-test In/Out markers for drag (8px tolerance)
  static const int kM = 8;
  m_dragMode = None;
  if (ctrlR->isAnimaticPlayRangeEnabled()) {
    int r0, r1;
    ctrlR->getAnimaticPlayRange(r0, r1);
    int x0 = (int)(r0 * m_ppf);
    int x1 = (int)((r1 + 1) * m_ppf);
    if (std::abs(mx - x0) <= kM) { m_dragMode = DragIn;  return; }
    if (std::abs(mx - x1) <= kM) { m_dragMode = DragOut; return; }
  }

  // Plain click: move playhead
  if (e->modifiers() == Qt::NoModifier) {
    m_currentFrame = frame;
    update();
    emit frameChanged(m_currentFrame);
    // Audio scrub — requireSoundTrack() builds the track lazily if not cached
    auto *ctrl = ZtoryAnimaticController::instance();
    TSoundTrackP st = ctrl->requireSoundTrack();
    TXsheet *xsh = ctrl->mainXsheet();
    if (st && xsh) {
      ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
      double fps = (sc && m_fps <= 0)
                   ? sc->getProperties()->getOutputProperties()->getFrameRate()
                   : (m_fps > 0 ? m_fps : 24.0);
      TINT32 sr = st->getSampleRate();
      TINT32 s0 = qBound((TINT32)0, (TINT32)(m_currentFrame * sr / fps),
                         st->getSampleCount() - 1);
      TINT32 s1 = qBound(s0 + 1, (TINT32)((m_currentFrame + 1) * sr / fps),
                         st->getSampleCount());
      if (s0 < s1) xsh->play(st, s0, s1, false);
    }
  }
}

void ZtoryAnimaticRuler::mouseMoveEvent(QMouseEvent *e) {
  int mx = qMax(0, e->x() - kLabelW);

  // Hover detection for marker labels — runs with no button pressed too
  // (the ruler has mouse tracking enabled).
  {
    TXsheet *tagXsh = ZtoryAnimaticController::instance()->mainXsheet();
    NavigationTags *nav = tagXsh ? tagXsh->getNavigationTags() : nullptr;
    int hover = -1;
    if (nav) {
      int best = 9;
      for (auto &t : nav->getTags()) {
        int tx = (int)(t.m_frame * m_ppf) + (int)(m_ppf / 2);
        int d  = std::abs(tx - mx);
        if (d < best) { best = d; hover = t.m_frame; }
      }
    }
    if (hover != m_hoverTagFrame) { m_hoverTagFrame = hover; update(); }
    setCursor(hover >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
  }

  if (!(e->buttons() & Qt::LeftButton)) return;
  int frame = (int)(mx / m_ppf);

  if (m_dragMode == DragIn) {
    auto *ctrlD = ZtoryAnimaticController::instance();
    int r0, r1;
    ctrlD->getAnimaticPlayRange(r0, r1);
    ctrlD->setAnimaticPlayRange(std::min(frame, r1), r1);
    ctrlD->notifyPlayRangeChanged();
    update();
    return;
  }
  if (m_dragMode == DragOut) {
    auto *ctrlD = ZtoryAnimaticController::instance();
    int r0, r1;
    ctrlD->getAnimaticPlayRange(r0, r1);
    ctrlD->setAnimaticPlayRange(r0, std::max(frame, r0));
    ctrlD->notifyPlayRangeChanged();
    update();
    return;
  }

  m_currentFrame = frame;
  update();
  emit frameChanged(m_currentFrame);
  // Audio scrub (12b) — requireSoundTrack() builds lazily if not cached yet
  auto *ctrl = ZtoryAnimaticController::instance();
  TSoundTrackP st = ctrl->requireSoundTrack();
  TXsheet *xsh = ctrl->mainXsheet();
  if (st && xsh) {
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    double fps = (sc) ? sc->getProperties()->getOutputProperties()->getFrameRate() : (m_fps > 0 ? m_fps : 24.0);
    TINT32 sr = st->getSampleRate();
    TINT32 s0 = qBound((TINT32)0, (TINT32)(m_currentFrame * sr / fps), st->getSampleCount()-1);
    TINT32 s1 = qBound(s0+1, (TINT32)((m_currentFrame+1) * sr / fps), st->getSampleCount());
    if (s0 < s1) xsh->play(st, s0, s1, false);
  }
}

void ZtoryAnimaticRuler::mouseReleaseEvent(QMouseEvent *) {
  m_dragMode = None;
  ZtoryAnimaticController::instance()->frameHandle()->stopScrubbing();
}

void ZtoryAnimaticRuler::leaveEvent(QEvent *) {
  setCursor(Qt::ArrowCursor);
  m_hoverTagFrame = -1;  // hide the hovered marker label
  update();
}

void ZtoryAnimaticRuler::resetPlayRangeToFull() {
  auto *ctrl = ZtoryAnimaticController::instance();
  TXsheet *xsh = ctrl->mainXsheet();
  if (!xsh) return;
  int lastFrame = std::max(0, videoFrameCount(xsh) - 1);
  ctrl->setAnimaticPlayRange(0, lastFrame);
  ctrl->notifyPlayRangeChanged();
  update();
}

void ZtoryAnimaticRuler::clampPlayRangeToTimeline() {
  // After shots are deleted/trimmed, the mark-out may be beyond the new end.
  // Clamp it without touching a valid mark-out that is still within range.
  auto *ctrl = ZtoryAnimaticController::instance();
  TXsheet *xsh = ctrl->mainXsheet();
  if (!xsh) return;
  int lastFrame = std::max(0, videoFrameCount(xsh) - 1);
  int r0, r1;
  ctrl->getAnimaticPlayRange(r0, r1);
  if (r1 > lastFrame) {
    ctrl->setAnimaticPlayRange(std::min(r0, lastFrame), lastFrame);
    ctrl->notifyPlayRangeChanged();
    update();
  }
}

void ZtoryAnimaticRuler::initPlayRangeIfNeeded() {
  // Initialise In/Out markers to full range on first show, if not yet set.
  auto *ctrl = ZtoryAnimaticController::instance();
  int r0, r1;
  ctrl->getAnimaticPlayRange(r0, r1);
  if (r0 <= r1) return;  // already set
  TXsheet *xsh = ctrl->mainXsheet();
  if (!xsh) return;
  int lastFrame = std::max(0, videoFrameCount(xsh) - 1);
  ctrl->setAnimaticPlayRange(0, lastFrame);
  ctrl->notifyPlayRangeChanged();
  update();
}

void ZtoryAnimaticRuler::editMarker(int frame) {
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  NavigationTags *nav = xsh ? xsh->getNavigationTags() : nullptr;
  if (!nav) return;
  QString label = nav->getTagLabel(frame);
  QColor color  = nav->getTagColor(frame);
  NavTagEditorPopup popup(frame, label, color);
  if (popup.exec() != QDialog::Accepted) return;
  nav->setTagLabel(frame, popup.getLabel());
  nav->setTagColor(frame, popup.getColor());
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  update();
}

void ZtoryAnimaticRuler::contextMenuEvent(QContextMenuEvent *e) {
  int mx = qMax(0, e->x() - kLabelW);
  int frame = (int)(mx / m_ppf);  // frame under cursor, NOT playhead

  auto *ctrlM = ZtoryAnimaticController::instance();

  // Locate a navigation tag near the click (within ~7 px), for rename/remove.
  TXsheet *tagXsh = ctrlM->mainXsheet();
  NavigationTags *nav = tagXsh ? tagXsh->getNavigationTags() : nullptr;
  int hitFrame = -1;
  if (nav) {
    int best = 8;
    for (auto &t : nav->getTags()) {
      int tx = (int)(t.m_frame * m_ppf) + (int)(m_ppf / 2);
      int d  = std::abs(tx - mx);
      if (d < best) { best = d; hitFrame = t.m_frame; }
    }
  }

  QMenu menu(this);
  QAction *inAct    = menu.addAction(tr("Mark IN here"));
  QAction *outAct   = menu.addAction(tr("Mark OUT here"));
  menu.addSeparator();
  QAction *autoAct  = menu.addAction(tr("Set OUT to last frame"));
  QAction *resetAct = menu.addAction(tr("Reset IN/OUT to full range"));
  menu.addSeparator();
  QAction *addMarkerAct = nullptr, *editMarkerAct = nullptr,
          *removeMarkerAct = nullptr;
  if (nav) {
    if (hitFrame >= 0) {
      editMarkerAct   = menu.addAction(tr("Edit Marker…"));
      removeMarkerAct = menu.addAction(tr("Remove Marker"));
    } else {
      addMarkerAct = menu.addAction(tr("Add Marker here"));
    }
  }

  QAction *chosen = menu.exec(e->globalPos());
  // ---- Navigation tag actions ----
  if (nav && chosen && (chosen == addMarkerAct ||
                        chosen == editMarkerAct ||
                        chosen == removeMarkerAct)) {
    if (chosen == addMarkerAct) {
      nav->addTag(frame);    // create, then open the editor for label + color
      editMarker(frame);
    } else if (chosen == editMarkerAct) {
      editMarker(hitFrame);  // editMarker() handles dirty flag + repaint
    } else {                 // removeMarkerAct
      nav->removeTag(hitFrame);
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      update();
    }
    return;
  }

  if (chosen == inAct) {
    int r0, r1;
    ctrlM->getAnimaticPlayRange(r0, r1);
    if (r0 > r1) { r0 = 0; r1 = frame; }
    ctrlM->setAnimaticPlayRange(frame, std::max(frame, r1));
  } else if (chosen == outAct) {
    int r0, r1;
    ctrlM->getAnimaticPlayRange(r0, r1);
    if (r0 > r1) { r0 = 0; r1 = frame; }
    ctrlM->setAnimaticPlayRange(std::min(r0, frame), frame);
  } else if (chosen == autoAct) {
    TXsheet *xsh = ctrlM->mainXsheet();
    int last = std::max(0, videoFrameCount(xsh) - 1);
    int r0, r1;
    ctrlM->getAnimaticPlayRange(r0, r1);
    if (r0 > r1) r0 = 0;
    ctrlM->setAnimaticPlayRange(r0, last);
  } else if (chosen == resetAct) {
    TXsheet *xsh = ctrlM->mainXsheet();
    int last = std::max(0, videoFrameCount(xsh) - 1);
    ctrlM->setAnimaticPlayRange(0, last);
  } else {
    return;
  }
  ctrlM->notifyPlayRangeChanged();
  update();
}

void ZtoryAnimaticRuler::wheelEvent(QWheelEvent *e) {
  int delta = e->angleDelta().y();
  double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
  double newPpf = qBound(kMinPpf, m_ppf * factor, kMaxPpf);
  emit zoomChanged(newPpf);
  e->accept();
}

// ---- ZtoryAudioTrack ----

ZtoryAudioTrack::ZtoryAudioTrack(int col, const QString &name, QWidget *parent)
    : QWidget(parent), m_col(col), m_name(name) {
  setFixedHeight(m_trackHeight);
  setMinimumWidth(100);
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(true);
  setAttribute(Qt::WA_Hover);

  // Read initial volume from the underlying audio column (saved in the .tnz).
  // Also cache the TXshSoundColumn* — this pointer is stable across column
  // index shifts (insertColumn / deleteColumn renumber indices but don't
  // recreate the column object), so it can be used as a stable identity key.
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (xsh) {
    TXshColumn *column = xsh->getColumn(m_col);
    m_soundCol = column ? column->getSoundColumn() : nullptr;
    if (m_soundCol) m_volume = m_soundCol->getVolume();
  }

  // L/M/S state is driven purely via paintEvent + mousePressEvent hit-test.
  // No child QToolButton widgets — they don't render reliably inside custom-
  // painted QWidgets on macOS.
}

int ZtoryAudioTrack::frameAtX(int x) const {
  return std::max(0, (int)((x - kLabelW) / m_ppf));
}

void ZtoryAudioTrack::setLocked(bool on) {
  if (m_locked == on) return;
  m_locked = on;
  update();
  emit lockedChanged(m_col, on);
}

void ZtoryAudioTrack::setMuted(bool on) {
  if (m_muted == on) return;
  m_muted = on;
  update();
}

void ZtoryAudioTrack::setSolo(bool on) {
  if (m_solo == on) return;
  m_solo = on;
  update();
}

// ── Audio undo ────────────────────────────────────────────────────────────────
// Stores before/after clones of the sound column for a single edit operation.
// assignLevels() reconstructs the column from the clone's internal level list.
class UndoAudioEdit final : public TUndo {
  int               m_col;
  TXshSoundColumn  *m_before;
  TXshSoundColumn  *m_after;
  QString           m_label;

  void restore(TXshSoundColumn *src) const {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (!xsh) return;
    TXshColumn *col = xsh->getColumn(m_col);
    TXshSoundColumn *sc = col ? col->getSoundColumn() : nullptr;
    if (!sc) return;
    sc->assignLevels(src);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
public:
  UndoAudioEdit(int col, TXshSoundColumn *before, TXshSoundColumn *after,
                const QString &label)
      : m_col(col), m_before(before), m_after(after), m_label(label) {}
  ~UndoAudioEdit() { delete m_before; delete m_after; }
  void    undo() const override { restore(m_before); }
  void    redo() const override { restore(m_after); }
  int     getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return m_label; }
};

class UndoAddAudioTrack final : public TUndo {
  int m_col;  // column index that was inserted
public:
  explicit UndoAddAudioTrack(int col) : m_col(col) {}
  void undo() const override {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (!xsh) return;
    xsh->removeColumn(m_col);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (!xsh) return;
    xsh->insertColumn(m_col, TXshColumn::eSoundType);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int     getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return QObject::tr("Add Audio Track"); }
};

// ── Audio clipboard ───────────────────────────────────────────────────────────
// Stores cloned ColumnLevel objects from the copied/cut selection.
struct AudioClipboard {
  int originFrame = 0;          // visibleStartFrame of the first clipped level
  std::vector<ColumnLevel *> levels;  // owned clones
  bool empty() const { return levels.empty(); }
  void clear() { for (auto *l : levels) delete l; levels.clear(); }
  ~AudioClipboard() { clear(); }
};
static AudioClipboard s_audioClip;

std::vector<ZtoryAudioTrack::Segment> ZtoryAudioTrack::findSegments() const {
  std::vector<Segment> segs;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return segs;
  TXshColumn *column = xsh->getColumn(m_col);
  if (!column) return segs;
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc) return segs;
  // Iterate ColumnLevels directly so that razor splits created by
  // splitLevelAtFrame (which share audio data but have distinct visible
  // ranges) are each reported as a separate segment — without losing the
  // frame at the cut boundary the way clearCells() would.
  int n = sc->getColumnLevelCount();
  for (int i = 0; i < n; i++) {
    ColumnLevel *cl = sc->getColumnLevel(i);
    if (!cl) continue;
    segs.push_back({cl->getVisibleStartFrame(), cl->getVisibleEndFrame()});
  }
  return segs;
}

void ZtoryAudioTrack::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  const int labelW = kLabelW;
  const int trackW = width() - labelW;
  const int trackH = height();
  const int center = trackH / 2;

  // Sfondo
  p.fillRect(rect(), QColor(25, 25, 25));

  // ---- Label area ----
  p.fillRect(0, 0, labelW, trackH, QColor(35, 35, 35));

  // L/M/S buttons — horizontal row at top of label area
  // Layout: 3 buttons each 22px wide, 2px gap, starting at x=2, y=2
  // Coordinates also used in mousePressEvent hit-test — keep in sync!
  static const int kBtnY  = 2;
  static const int kBtnH  = 16;
  static const int kBtnW  = 22;
  static const int kBtnGap = 3;
  struct BtnInfo { const char *txt; QColor offColor; QColor onColor; bool active; };
  BtnInfo btns[3] = {
    {"L", QColor(100, 65, 20), QColor(220, 140, 50), m_locked},
    {"M", QColor(100, 30, 30), QColor(210, 64, 64),  m_muted},
    {"S", QColor(20, 90, 95),  QColor(50, 190, 200), m_solo},
  };
  p.setFont(QFont("Arial", 8, QFont::Bold));
  for (int i = 0; i < 3; i++) {
    QRect r(2 + i * (kBtnW + kBtnGap), kBtnY, kBtnW, kBtnH);
    p.fillRect(r, btns[i].active ? btns[i].onColor : btns[i].offColor);
    p.setPen(btns[i].active ? QColor(255, 255, 255) : btns[i].onColor);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.drawText(r, Qt::AlignCenter, btns[i].txt);
  }

  // Volume slider — thin horizontal bar at the bottom of the label area.
  // Click sets volume to position; drag adjusts continuously.  Range [0,1].
  // Coordinates also used in mousePressEvent hit-test — keep in sync!
  static const int kVolBarH    = 6;
  static const int kVolBarBotMargin = 4;
  const int volBarY  = trackH - kVolBarH - kVolBarBotMargin;
  const int volBarX  = 4;
  const int volBarW  = labelW - 8;
  QRect volBar(volBarX, volBarY, volBarW, kVolBarH);
  // Background track
  p.fillRect(volBar, QColor(28, 28, 28));
  // Filled portion
  int filledW = (int)(m_volume * (volBarW - 2));
  p.fillRect(volBar.x() + 1, volBar.y() + 1, filledW, kVolBarH - 2,
             m_muted || m_effectiveMuted ? QColor(70, 70, 70)
                                         : QColor(80, 140, 200));
  // Knob position marker
  int knobX = volBar.x() + 1 + filledW;
  p.setPen(QPen(QColor(220, 230, 245), 1));
  p.drawLine(knobX, volBar.y() - 1, knobX, volBar.y() + kVolBarH);

  // Track name — between the buttons (top) and the volume slider (bottom)
  p.setPen(QColor(210, 210, 210));
  p.setFont(QFont("Arial", 8));
  int nameY = kBtnY + kBtnH + 2;
  int nameH = volBarY - nameY - 2;
  if (nameH > 0)
    p.drawText(2, nameY, labelW - 4, nameH, Qt::AlignVCenter | Qt::AlignLeft, m_name);

  p.setPen(QColor(65, 65, 65));
  p.drawLine(labelW, 0, labelW, trackH);

  // Viewport-aware waveform cache.
  // We only allocate and render pixels within the VISIBLE area + a small
  // overscan, so a 10,000-frame scene never creates a 80,000px-wide QPixmap.
  // Cache miss = visible area scrolled outside the cached band, or data changed.
  const int kOverscan = 600;  // extra pixels rendered beyond clip on each side
  const QRect clip = e->rect();
  // Visible range in track coordinates (relative to the label-right edge)
  const int visX0 = qMax(0, clip.left() - labelW);
  const int visX1 = qMin(trackW, clip.right() - labelW + 1);

  // Desired cache band: clamp to actual track width
  const int bandX0 = qMax(0, visX0 - kOverscan);
  const int bandX1 = qMin(trackW, visX1 + kOverscan);
  const int bandW  = qMax(1, bandX1 - bandX0);

  // Cache is stale if: data changed, height changed, or visible area
  // scrolled outside the rendered band.
  bool needRebuild = m_waveformDirty
      || m_waveformCache.isNull()
      || m_waveformCache.height() != trackH
      || visX0 < m_cacheOffsetX
      || visX1 > m_cacheOffsetX + m_waveformCache.width();

  if (needRebuild) {
    m_cacheOffsetX  = bandX0;
    m_waveformCache = QPixmap(bandW, trackH);
    m_waveformCache.fill(QColor(25, 25, 25));
    m_waveformDirty = false;

    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;

    // Draw per-ColumnLevel waveform. Each CL references the same audio file
    // but may have different startOffset/endOffset (from splits/trims).
    // Reading sample positions directly from each CL avoids any merging
    // artefacts that getOverallSoundTrack() could introduce.
    QPainter cp(&m_waveformCache);
    cp.setPen(QColor(60, 60, 60));
    cp.drawLine(0, center, bandW, center);

    if (sc && sc->getColumnLevelCount() > 0) {
      double fps = 24.0;
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene)
        fps = scene->getProperties()->getOutputProperties()->getFrameRate();

      // Collect one TSoundTrackP per CL (may be null if file not loaded).
      int nCL = sc->getColumnLevelCount();
      struct CLData {
        ColumnLevel *cl;
        TSoundTrackP track;
        TINT32       sampleRate;
      };
      std::vector<CLData> cldata;
      cldata.reserve(nCL);
      for (int i = 0; i < nCL; i++) {
        ColumnLevel *cl = sc->getColumnLevel(i);
        if (!cl) continue;
        TSoundTrackP t;
        try { t = cl->getSoundLevel()->getSoundTrack(); } catch (...) {}
        if (!t) continue;
        cldata.push_back({cl, t, (TINT32)t->getSampleRate()});
      }

      if (!cldata.empty()) {
        // Global peak normalisation across all CLs
        double peak = 1.0;
        for (auto &d : cldata) {
          TINT32 s0 = (TINT32)(d.cl->getStartOffset() * d.sampleRate / fps);
          TINT32 visFrames = d.cl->getVisibleFrameCount();
          TINT32 s1 = s0 + (TINT32)(visFrames * d.sampleRate / fps);
          s0 = qBound((TINT32)0, s0, d.track->getSampleCount() - 1);
          s1 = qBound((TINT32)0, s1, d.track->getSampleCount() - 1);
          if (s1 > s0) {
            double mn = 0, mx = 0;
            d.track->getMinMaxPressure(s0, s1, TSound::MONO, mn, mx);
            peak = std::max(peak, std::max(std::fabs(mn), std::fabs(mx)));
          }
        }

        const int halfH = (trackH - 4) / 2;
        cp.setPen(QColor(80, 200, 120));

        for (auto &d : cldata) {
          double samplesPerPixel = d.sampleRate / fps / m_ppf;
          int vsf = d.cl->getVisibleStartFrame();
          // Clamp CL pixel range to the cache band
          int pxL = qBound(bandX0, (int)(vsf * m_ppf), bandX1);
          int pxR = qBound(bandX0, (int)((d.cl->getVisibleEndFrame() + 1) * m_ppf), bandX1);

          for (int px = pxL; px < pxR; px++) {
            // px is in track coords; cp paints in cache coords (px - bandX0)
            double relFrame = (double)px / m_ppf - vsf;
            TINT32 s0 = (TINT32)((d.cl->getStartOffset() + relFrame)
                                  * d.sampleRate / fps);
            TINT32 s1 = s0 + (TINT32)(samplesPerPixel) + 1;
            TINT32 sc2 = d.track->getSampleCount();
            s0 = qBound((TINT32)0, s0, sc2 - 1);
            s1 = qBound((TINT32)0, s1, sc2 - 1);
            if (s0 > s1) continue;

            double minV = 0, maxV = 0;
            d.track->getMinMaxPressure(s0, s1, TSound::MONO, minV, maxV);

            int yMax = center - (int)(maxV / peak * halfH);
            int yMin = center - (int)(minV / peak * halfH);
            yMax = qBound(2, yMax, trackH - 2);
            yMin = qBound(2, yMin, trackH - 2);
            if (yMax > yMin) std::swap(yMax, yMin);
            // Draw into cache: x offset by -bandX0
            cp.drawLine(px - bandX0, yMax, px - bandX0, yMin);
          }
        }
      }
    }
  }

  // Draw segments and gaps:
  // - Gaps (empty areas between segments) → gray background, no waveform
  // - Segments → blit only the corresponding slice of the waveform cache
  // - Segment edges → 1px dark border
  {
    auto segs = findSegments();

    if (segs.empty()) {
      // No segments at all — fill entire track area with gap color
      p.fillRect(labelW, 0, trackW, trackH, QColor(42, 42, 42));
    } else {
      // 1. Fill entire area with gap color first (covers gaps before first
      //    segment, between segments, and after last segment)
      p.fillRect(labelW, 0, trackW, trackH, QColor(42, 42, 42));

      // 2. For each segment, blit the corresponding slice from the waveform
      //    cache (cache covers [m_cacheOffsetX .. m_cacheOffsetX+cache.width()))
      for (auto &s : segs) {
        int x0 = (int)(s.r0 * m_ppf);          // offset in track coords
        int x1 = (int)((s.r1 + 1) * m_ppf);
        int w  = x1 - x0;
        if (w <= 0) continue;
        // Clamp to the cached band; fall back to gap color outside it
        int cx0 = qMax(x0, m_cacheOffsetX);
        int cx1 = qMin(x1, m_cacheOffsetX + m_waveformCache.width());
        if (cx0 < cx1) {
          p.drawPixmap(labelW + cx0, 0, m_waveformCache,
                       cx0 - m_cacheOffsetX, 0, cx1 - cx0, trackH);
        }
      }

      // 3. Draw 1px borders at each segment edge
      p.setPen(QColor(70, 70, 70));
      for (auto &s : segs) {
        int x0 = labelW + (int)(s.r0 * m_ppf);
        int x1 = labelW + (int)((s.r1 + 1) * m_ppf);
        p.drawLine(x0, 0, x0, trackH - 1);
        p.drawLine(x1 - 1, 0, x1 - 1, trackH - 1);
      }
    }
  }

  // Dim overlay whenever the track is silent: either user-muted (M) or
  // silenced by another track's solo (effectiveMuted).
  if (m_muted || m_effectiveMuted)
    p.fillRect(labelW, 0, trackW, trackH, QColor(0, 0, 0, 130));

  // Highlight selected segment
  if (m_selSeg.r0 >= 0 && m_selSeg.r1 >= m_selSeg.r0) {
    int x0 = labelW + (int)(m_selSeg.r0 * m_ppf);
    int x1 = labelW + (int)((m_selSeg.r1 + 1) * m_ppf);
    p.fillRect(x0, 0, x1 - x0, trackH, QColor(100, 180, 255, 50));
    p.setPen(QColor(100, 180, 255, 150));
    p.drawRect(x0, 0, x1 - x0 - 1, trackH - 1);
  }

  // Preview bar (12c) — thin strip at bottom, orange selection
  static const int kScrubBarH = 6;
  p.fillRect(labelW, trackH - kScrubBarH, trackW, kScrubBarH, QColor(55, 55, 55));
  if (m_previewR0 >= 0 && m_previewR1 >= m_previewR0) {
    int x0 = labelW + (int)(m_previewR0 * m_ppf);
    int x1 = labelW + (int)((m_previewR1 + 1) * m_ppf);
    p.fillRect(x0, trackH - kScrubBarH, x1 - x0, kScrubBarH, QColor(255, 165, 0));
  }

  // NOTE: m_cutFrames (video shot boundaries) are intentionally NOT drawn here.
  // Audio segment edges are already visible as the 1px borders around each block.
  // Drawing shot boundaries on the audio track caused ghost lines when segments
  // were moved away from those positions.

  // Razor hover preview line — shown when razor is active and cursor hovers
  if (m_razorHoverFrame >= 0) {
    int rx = labelW + (int)(m_razorHoverFrame * m_ppf);
    p.setPen(QPen(QColor(255, 255, 80, 200), 1, Qt::DashLine));
    p.drawLine(rx, 0, rx, trackH);
  }

  // Playhead (always on top, not cached) — centered on frame like the ruler
  int phx = labelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(phx, 0, phx, trackH);

  // Focus indicator: bright border around the whole track when keyboard-focused
  if (m_hasFocus) {
    p.setPen(QPen(QColor(80, 160, 255), 2));
    p.drawRect(1, 1, width() - 2, trackH - 2);
  }
}

void ZtoryAudioTrack::setRazorActive(bool on) {
  m_razorActive = on;
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ZtoryAudioTrack::clearSelection() {
  if (m_selSeg.r0 >= 0) { m_selSeg = {-1, -1}; update(); }
}

// ── Group move (multi-track) ──────────────────────────────────────────────────
// beginGroupDrag: called by the panel on every selected track when a group drag
// starts.  Captures the selection's origin and takes an undo snapshot.
void ZtoryAudioTrack::beginGroupDrag() {
  if (m_selSeg.r0 < 0) { m_groupOrigR0 = m_groupOrigR1 = -1; return; }
  m_groupOrigR0 = m_selSeg.r0;
  m_groupOrigR1 = m_selSeg.r1;
  delete m_undoBefore; m_undoBefore = nullptr;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  TXshColumn *c = xsh ? xsh->getColumn(m_col) : nullptr;
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (sc) m_undoBefore = dynamic_cast<TXshSoundColumn *>(sc->clone());
}

// previewGroupMove: visual-only move of this track's selection by deltaFrames
// from its captured origin (clamped to >= 0).  Used for sibling tracks.
void ZtoryAudioTrack::previewGroupMove(int deltaFrames) {
  if (m_groupOrigR0 < 0) return;
  int len  = m_groupOrigR1 - m_groupOrigR0;
  int newR0 = qMax(0, m_groupOrigR0 + deltaFrames);
  m_selSeg.r0 = newR0;
  m_selSeg.r1 = newR0 + len;
  update();
}

// commitGroupMove: apply the shift to the audio data and push an undo step.
void ZtoryAudioTrack::commitGroupMove(int deltaFrames) {
  if (m_groupOrigR0 < 0) { m_groupOrigR0 = m_groupOrigR1 = -1; return; }
  int len   = m_groupOrigR1 - m_groupOrigR0;
  int newR0 = qMax(0, m_groupOrigR0 + deltaFrames);
  int applied = newR0 - m_groupOrigR0;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  TXshColumn *c = xsh ? xsh->getColumn(m_col) : nullptr;
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (sc && applied != 0) {
    sc->shiftLevelInRange(m_groupOrigR0, m_groupOrigR1, applied);
    TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
    TUndoManager::manager()->add(
        new UndoAudioEdit(m_col, m_undoBefore, after, tr("Move Audio")));
    m_undoBefore = nullptr;  // ownership transferred
    invalidateWaveform();
    m_selSeg = {newR0, newR0 + len};
  } else {
    delete m_undoBefore; m_undoBefore = nullptr;
  }
  m_groupOrigR0 = m_groupOrigR1 = -1;
  update();
}

void ZtoryAudioTrack::focusInEvent(QFocusEvent *e) {
  m_hasFocus = true;
  update();
  QWidget::focusInEvent(e);
}

void ZtoryAudioTrack::focusOutEvent(QFocusEvent *e) {
  m_hasFocus = false;
  update();
  QWidget::focusOutEvent(e);
}

void ZtoryAudioTrack::setCutFrames(const QVector<int> &frames) {
  m_cutFrames = frames;
  update();
}

void ZtoryAudioTrack::setRazorHoverFrame(int frame) {
  if (m_razorHoverFrame == frame) return;
  m_razorHoverFrame = frame;
  update();
}

// ---- ZtoryAudioTrack mouse events (12c: preview bar, razor) ----

static bool nearSegmentEdge(int mx, double ppf, const std::vector<ZtoryAudioTrack::Segment> &segs, int widgetW) {
  for (auto &s : segs) {
    int xLeft    = kLabelW + (int)(s.r0 * ppf);
    int xRightRaw = kLabelW + (int)((s.r1 + 1) * ppf);
    if (mx >= xLeft && mx < xLeft + 10) return true;
    if (xRightRaw < widgetW && mx > xRightRaw - 10 && mx <= xRightRaw + 1) return true;
  }
  return false;
}

bool ZtoryAudioTrack::event(QEvent *e) {
  // Claim keyboard shortcuts before Qt/CommandManager steals them on macOS.
  // Without this, ShortcutOverride propagates and Delete never reaches
  // keyPressEvent because a global action absorbs it first.
  if (e->type() == QEvent::ShortcutOverride && !m_locked) {
    auto *ke = static_cast<QKeyEvent *>(e);
    bool ctrl = ke->modifiers() & Qt::ControlModifier;
    if (ctrl && (ke->key() == Qt::Key_X || ke->key() == Qt::Key_C || ke->key() == Qt::Key_V))
      { ke->accept(); return true; }
    if (!ctrl && (ke->key() == Qt::Key_Delete || ke->key() == Qt::Key_Backspace))
      { ke->accept(); return true; }
  }
  // WA_Hover delivers HoverMove/HoverLeave even without mouse button — more
  // reliable than setMouseTracking inside QScrollArea on macOS.
  if (e->type() == QEvent::HoverMove && m_dragMode == NoDrag && !m_razorActive) {
    auto *he = static_cast<QHoverEvent *>(e);
    int mx = he->pos().x();
    // Avoid identifier name 'near' — it's a macro in Windows windef.h.
    bool nearEdge = nearSegmentEdge(mx, m_ppf, findSegments(), width());
    setCursor(nearEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
    return true;
  }
  if (e->type() == QEvent::HoverLeave) {
    unsetCursor();
    return true;
  }
  return QWidget::event(e);
}

void ZtoryAudioTrack::wheelEvent(QWheelEvent *e) {
  if (e->modifiers() & Qt::ControlModifier) {
    int delta = e->angleDelta().y();
    double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
    double newPpf = qBound(kMinPpf, m_ppf * factor, kMaxPpf);
    emit zoomChanged(newPpf);
    e->accept();
  } else {
    e->ignore();  // plain scroll → scroll area
  }
}

void ZtoryAudioTrack::mousePressEvent(QMouseEvent *e) {
  // L/M/S button hit-test — must match coordinates in paintEvent exactly
  // Buttons: horizontal row, each 22px wide, 3px gap, starting at x=2, y=2, h=16
  if (e->button() == Qt::LeftButton && e->y() >= 2 && e->y() < 18) {
    static const int kBtnW = 22, kBtnGap = 3;
    for (int i = 0; i < 3; i++) {
      int bx = 2 + i * (kBtnW + kBtnGap);
      if (e->x() >= bx && e->x() < bx + kBtnW) {
        if (i == 0) {        // Lock
          m_locked = !m_locked;
          update();
          emit lockedChanged(m_col, m_locked);
        } else if (i == 1) { // Mute
          m_muted = !m_muted;
          update();
          // Delegate volume/cache/restart to the panel via applyMuteSolo(),
          // so solo state and live-play restart are always considered.
          emit muteToggleRequested(m_col);
        } else {             // Solo
          m_solo = !m_solo;
          update();
          emit soloToggleRequested(m_col);
        }
        return;
      }
    }
  }
  // Volume slider hit-test — must match coordinates in paintEvent exactly
  if (e->button() == Qt::LeftButton) {
    static const int kVolBarH = 6, kVolBarBotMargin = 4;
    const int volBarY = m_trackHeight - kVolBarH - kVolBarBotMargin;
    const int volBarX = 4;
    const int volBarW = kLabelW - 8;
    if (e->y() >= volBarY - 2 && e->y() <= volBarY + kVolBarH + 2 &&
        e->x() >= volBarX && e->x() <= volBarX + volBarW) {
      m_draggingVolume = true;
      double v = double(e->x() - volBarX - 1) / double(volBarW - 2);
      m_volume = qBound(0.0, v, 1.0);
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      if (xsh) {
        TXshColumn *column = xsh->getColumn(m_col);
        TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
        if (sc) sc->setVolume(m_volume);
      }
      update();
      return;
    }
  }
  // Ignore other label area clicks
  if (e->x() < kLabelW) return;

  static const int kScrubBarH = 6;
  // Razor tool: click anywhere on the waveform area (not in the scrub bar)
  if (m_razorActive && !m_locked && e->y() < height() - kScrubBarH) {
    int frame = frameAtX(e->x());
    // Clear selection so the highlight doesn't persist after the cut
    if (m_selSeg.r0 >= 0) { m_selSeg = {-1, -1}; update(); }
    emit razorRequested(m_col, frame);
    return;
  }
  // Scrub bar drag — allowed even when locked (read-only preview)
  if (e->y() >= height() - kScrubBarH) {
    m_draggingPreview  = true;
    int frame = frameAtX(e->x());
    m_previewDragStart = frame;
    m_previewR0 = m_previewR1 = frame;
    update();
    return;
  }
  // Select / drag / trim segment — blocked when locked
  if (e->button() == Qt::LeftButton && !m_razorActive && !m_locked) {
    setFocus(Qt::MouseFocusReason);
    // Ctrl/Cmd adds to the cross-track selection instead of replacing it.
    bool additive = (e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier));
    int frame = frameAtX(e->x());
    int mx = e->x();
    auto segs = findSegments();
    Segment prevSel = m_selSeg;  // remember selection state before this click
    m_selSeg = {-1, -1};
    m_dragMode = NoDrag;
    delete m_undoBefore; m_undoBefore = nullptr;
    bool wasSelected = false;
    for (auto &s : segs) {
      if (frame < s.r0 || frame > s.r1) continue;
      m_selSeg = s;
      wasSelected = (prevSel.r0 == s.r0 && prevSel.r1 == s.r1);
      m_dragOrigR0 = s.r0;
      m_dragOrigR1 = s.r1;
      m_dragStartFrame = frame;
      // Snapshot for undo — taken once at drag start
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      TXshColumn *c = xsh ? xsh->getColumn(m_col) : nullptr;
      TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
      if (sc) m_undoBefore = dynamic_cast<TXshSoundColumn *>(sc->clone());
      // Edge detection: 6px zone at each border
      int xLeft  = kLabelW + (int)(s.r0 * m_ppf);
      int xRight = kLabelW + (int)((s.r1 + 1) * m_ppf);
      if (mx - xLeft < 6 && mx - xLeft >= 0) {
        m_dragMode = TrimLeft;
        setCursor(Qt::SizeHorCursor);
      } else if (xRight - mx < 6 && xRight - mx >= 0) {
        m_dragMode = TrimRight;
        setCursor(Qt::SizeHorCursor);
      } else {
        m_dragMode = SegmentDrag;
        setCursor(Qt::ClosedHandCursor);
      }
      break;
    }
    if (m_selSeg.r0 >= 0) {
      // Plain click on an UNselected segment selects it exclusively (panel
      // clears the other tracks).  Ctrl/Cmd adds to the selection, and a plain
      // click on an ALREADY-selected segment keeps the multi-selection so it can
      // be dragged as a group.
      if (!additive && !wasSelected) emit exclusiveSelectRequested();
      // Tell the panel to arm a group move across all selected tracks so a
      // SegmentDrag here moves every selected segment by the same delta.
      if (m_dragMode == SegmentDrag) { m_isGroupLeader = true; emit groupDragStarted(); }
    } else {
      // Clicked empty area — clear this track and notify siblings.
      emit selectionCleared();
    }
    update();
  }
}

void ZtoryAudioTrack::mouseMoveEvent(QMouseEvent *e) {
  // Volume slider drag — must precede other drag handlers.
  // sc->setVolume() updates the column's m_volume AND, if its m_player is
  // currently playing, calls m_player->setVolume() which is applied by
  // QAudioOutput in real time (zero-latency volume change during playback).
  if (m_draggingVolume) {
    const int volBarX = 4;
    const int volBarW = kLabelW - 8;
    double v = double(e->x() - volBarX - 1) / double(volBarW - 2);
    double newVol = qBound(0.0, v, 1.0);
    if (newVol != m_volume) {
      m_volume = newVol;
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      if (xsh) {
        TXshColumn *column = xsh->getColumn(m_col);
        TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
        if (sc) sc->setVolume(m_volume);
      }
      update();
    }
    return;
  }

  if (m_draggingPreview) {
    int frame = frameAtX(e->x());
    m_previewR0 = std::min(m_previewDragStart, frame);
    m_previewR1 = std::max(m_previewDragStart, frame);
    update();
    return;
  }

  // Segment drag with overlap clamping
  if (m_dragMode == SegmentDrag && m_selSeg.r0 >= 0) {
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    int segLen = m_dragOrigR1 - m_dragOrigR0;
    int newR0 = m_dragOrigR0 + delta;
    if (newR0 < 0) newR0 = 0;
    // Clamp against neighboring segments to prevent overlap
    auto segs = findSegments();
    for (auto &s : segs) {
      if (s.r0 == m_dragOrigR0 && s.r1 == m_dragOrigR1) continue;
      // Neighbor on the left: newR0 must be > s.r1
      if (s.r1 < m_dragOrigR0 && newR0 <= s.r1)
        newR0 = s.r1 + 1;
      // Neighbor on the right: newR0 + segLen must be < s.r0
      if (s.r0 > m_dragOrigR1 && newR0 + segLen >= s.r0)
        newR0 = s.r0 - segLen - 1;
    }
    if (newR0 < 0) newR0 = 0;
    // Snap: align the start or the end edge (whichever is closer) to a target.
    if (m_snapEnabled && !m_snapFrames.isEmpty()) {
      int endR = newR0 + segLen + 1;  // one-past-end
      int sStart = snapToTargets(newR0, m_ppf, m_snapFrames, true);
      int sEnd   = snapToTargets(endR,  m_ppf, m_snapFrames, true);
      int corrStart = sStart - newR0;
      int corrEnd   = sEnd - endR;
      int corr = 0;
      if (corrStart != 0 && (corrEnd == 0 || std::abs(corrStart) <= std::abs(corrEnd)))
        corr = corrStart;
      else if (corrEnd != 0)
        corr = corrEnd;
      newR0 = qMax(0, newR0 + corr);
    }
    m_selSeg.r0 = newR0;
    m_selSeg.r1 = newR0 + segLen;
    // Broadcast the applied delta so the panel moves every other selected
    // track's segment by the same amount (group move).
    emit groupDragDelta(newR0 - m_dragOrigR0);
    update();
    return;
  }

  // Trim left/right edge: visual feedback
  if ((m_dragMode == TrimLeft || m_dragMode == TrimRight) && m_selSeg.r0 >= 0) {
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    if (m_dragMode == TrimLeft) {
      int newR0 = m_dragOrigR0 + delta;
      newR0 = snapToTargets(newR0, m_ppf, m_snapFrames, m_snapEnabled);
      if (newR0 < 0) newR0 = 0;
      if (newR0 > m_dragOrigR1 - 1) newR0 = m_dragOrigR1 - 1;
      m_selSeg.r0 = newR0;
      m_selSeg.r1 = m_dragOrigR1;
    } else {
      int newR1 = m_dragOrigR1 + delta;
      newR1 = snapToTargets(newR1 + 1, m_ppf, m_snapFrames, m_snapEnabled) - 1;
      if (newR1 < m_dragOrigR0 + 1) newR1 = m_dragOrigR0 + 1;
      m_selSeg.r0 = m_dragOrigR0;
      m_selSeg.r1 = newR1;
    }
    update();
    return;
  }

  // Razor hover: track cursor position to show the cut preview line
  if (m_razorActive) {
    int frame = frameAtX(e->x());
    if (frame != m_razorHoverFrame) {
      m_razorHoverFrame = frame;
      update();
    }
    return;
  }

  // Hover cursor: show SizeHorCursor near segment edges
  {
    int mx = e->x();
    auto segs = findSegments();
    bool nearEdge = false;
    for (auto &s : segs) {
      int xLeft    = kLabelW + (int)(s.r0 * m_ppf);
      int xRightRaw = kLabelW + (int)((s.r1 + 1) * m_ppf);
      // Cap to widget width — getVisibleEndFrame() returns full file duration
      // for uncut audio (endOffset=0), making xRightRaw far off-screen.
      int xRight = qMin(xRightRaw, width() - 1);
      bool nearLeft  = (mx >= xLeft && mx < xLeft + 8);
      // Only show right-edge cursor when the actual audio end is on-screen.
      bool nearRight = (xRightRaw < width()) &&
                       (mx > xRight - 8 && mx <= xRight + 1);
      if (nearLeft || nearRight) {
        nearEdge = true;
        break;
      }
    }
    setCursor(nearEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
  }
}

void ZtoryAudioTrack::leaveEvent(QEvent *) {
  if (m_razorHoverFrame >= 0) {
    m_razorHoverFrame = -1;
    update();
  }
}

void ZtoryAudioTrack::contextMenuEvent(QContextMenuEvent *e) {
  // Only show the menu when right-clicking in the label area (left of the
  // waveform), where the L/M/S buttons live — consistent with other DAW UX.
  if (e->x() >= kLabelW) { e->ignore(); return; }

  QMenu menu(this);
  QAction *delAct = menu.addAction(tr("Delete Audio Track"));
  delAct->setIcon(QIcon::fromTheme("edit-delete"));
  if (menu.exec(e->globalPos()) == delAct)
    emit deleteRequested(m_col);
}

void ZtoryAudioTrack::mouseReleaseEvent(QMouseEvent *e) {
  if (m_draggingVolume) {
    m_draggingVolume = false;
    // Invalidate the merged sound track cache so the next scrub uses the
    // new volume.  During play this isn't needed (per-column players are
    // used), but scrub still goes through the merged track on scrubDevice.
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (xsh) xsh->invalidateSound();
    ZtoryAnimaticController::instance()->invalidateSoundTrack();
    return;
  }

  DragMode finishedMode = m_dragMode;
  m_dragMode = NoDrag;
  setCursor(m_razorActive ? Qt::CrossCursor : Qt::ArrowCursor);

  // Finish segment drag — commit via shiftLevelInRange on the ColumnLevel
  if (finishedMode == SegmentDrag) {
    int segLen = m_dragOrigR1 - m_dragOrigR0;

    // Cross-track drop: if mouse is outside this widget vertically
    QPoint localPos = e->pos();
    if (localPos.y() < 0 || localPos.y() >= height()) {
      m_isGroupLeader = false;  // a vertical drop is a cross-track move, not a group move
      int dragOffset = m_dragStartFrame - m_dragOrigR0;
      emit segmentDroppedOutside(m_col, m_dragOrigR0, m_dragOrigR1,
                                 dragOffset, e->globalPos());
      update();
      return;
    }

    int finalDelta = m_selSeg.r0 - m_dragOrigR0;
    m_isGroupLeader = false;
    // Group the leader's move and every sibling's move (committed synchronously
    // via the groupDragCommitted signal below) into one undo step.
    TUndoScopedBlock undoBlock;
    if (finalDelta != 0 && m_dragOrigR0 >= 0) {
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
      TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
      if (sc) {
        sc->shiftLevelInRange(m_dragOrigR0, m_dragOrigR1, finalDelta);
        TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
        TUndoManager::manager()->add(
            new UndoAudioEdit(m_col, m_undoBefore, after, tr("Move Audio")));
        m_undoBefore = nullptr;  // ownership transferred
        invalidateWaveform();
        ZtoryAnimaticController::instance()->invalidateSoundTrack();
        emit segmentMoved();
      } else {
        m_selSeg = {m_dragOrigR0, m_dragOrigR0 + segLen};
        delete m_undoBefore; m_undoBefore = nullptr;
      }
    } else {
      delete m_undoBefore; m_undoBefore = nullptr;
    }
    // Commit the same delta on every other selected (sibling) track.
    emit groupDragCommitted(finalDelta);
    update();
    return;
  }

  // Finish edge trim — commit via modifyCellRange
  if (finishedMode == TrimLeft || finishedMode == TrimRight) {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (sc) {
      bool changed = false;
      if (finishedMode == TrimLeft) {
        int delta = m_selSeg.r0 - m_dragOrigR0;
        if (delta != 0) { sc->modifyCellRange(m_dragOrigR0, delta, true); changed = true; }
      } else {
        int delta = m_selSeg.r1 - m_dragOrigR1;
        if (delta != 0) { sc->modifyCellRange(m_dragOrigR1, delta, false); changed = true; }
      }
      if (changed) {
        TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
        TUndoManager::manager()->add(
            new UndoAudioEdit(m_col, m_undoBefore, after, tr("Trim Audio")));
        m_undoBefore = nullptr;  // ownership transferred
      } else {
        delete m_undoBefore; m_undoBefore = nullptr;
      }
      invalidateWaveform();
      ZtoryAnimaticController::instance()->invalidateSoundTrack();
      emit segmentMoved();
    } else {
      delete m_undoBefore; m_undoBefore = nullptr;
    }
    update();
    return;
  }

  // Finish preview bar drag
  if (m_draggingPreview) {
    m_draggingPreview = false;
    if (m_previewR0 < 0 || m_previewR1 < m_previewR0) return;

    auto *ctrl = ZtoryAnimaticController::instance();
    TXsheet *xsh = ctrl->mainXsheet();
    if (!xsh) return;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    double fps = scene ? scene->getProperties()->getOutputProperties()->getFrameRate() : 24.0;

    TSoundTrackP st = ctrl->requireSoundTrack();
    if (!st) return;

    TINT32 sr = st->getSampleRate();
    TINT32 s0 = (TINT32)(m_previewR0 * sr / fps);
    TINT32 s1 = (TINT32)((m_previewR1 + 1) * sr / fps);
    s0 = qBound((TINT32)0, s0, st->getSampleCount() - 1);
    s1 = qBound((TINT32)0, s1, st->getSampleCount() - 1);
    if (s0 >= s1) return;
    xsh->play(st, s0, s1, false);
  }
}

// ---- Clipboard operations ----

void ZtoryAudioTrack::clipboardCopy(ZtoryAudioTrack *src) {
  if (!src || src->m_selSeg.r0 < 0) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  TXshColumn *c = xsh->getColumn(src->m_col);
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (!sc) return;

  s_audioClip.clear();
  s_audioClip.originFrame = src->m_selSeg.r0;

  int r0 = src->m_selSeg.r0, r1 = src->m_selSeg.r1;
  for (int i = 0; i < sc->getColumnLevelCount(); i++) {
    ColumnLevel *cl = sc->getColumnLevel(i);
    if (cl->getVisibleEndFrame() < r0 || cl->getVisibleStartFrame() > r1) continue;
    s_audioClip.levels.push_back(cl->clone());
  }
}

void ZtoryAudioTrack::clipboardCut(ZtoryAudioTrack *src) {
  if (!src || src->m_selSeg.r0 < 0) return;
  clipboardCopy(src);
  // Delete the selection with undo.
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  TXshColumn *c = xsh->getColumn(src->m_col);
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (!sc) return;
  TXshSoundColumn *before = dynamic_cast<TXshSoundColumn *>(sc->clone());
  sc->clearCells(src->m_selSeg.r0, src->m_selSeg.r1 - src->m_selSeg.r0 + 1);
  TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
  TUndoManager::manager()->add(
      new UndoAudioEdit(src->m_col, before, after, QObject::tr("Cut Audio")));
  src->m_selSeg = {-1, -1};
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAudioTrack::clipboardPaste(ZtoryAudioTrack *dst, int targetFrame) {
  if (s_audioClip.empty() || !dst) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  TXshColumn *c = xsh->getColumn(dst->m_col);
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (!sc) return;
  TXshSoundColumn *before = dynamic_cast<TXshSoundColumn *>(sc->clone());
  int offset = targetFrame - s_audioClip.originFrame;
  for (ColumnLevel *cl : s_audioClip.levels) {
    ColumnLevel *copy = cl->clone();
    int newVsf = cl->getVisibleStartFrame() + offset;
    sc->adoptLevel(copy, newVsf);
  }
  TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
  TUndoManager::manager()->add(
      new UndoAudioEdit(dst->m_col, before, after, QObject::tr("Paste Audio")));
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAudioTrack::keyPressEvent(QKeyEvent *e) {
  if (m_locked) { QWidget::keyPressEvent(e); return; }
  bool ctrl = (e->modifiers() & Qt::ControlModifier);
  if (ctrl && e->key() == Qt::Key_X) { clipboardCut(this); e->accept(); return; }
  if (ctrl && e->key() == Qt::Key_C) { clipboardCopy(this); e->accept(); return; }
  if (ctrl && e->key() == Qt::Key_V) {
    // Paste at the playhead (cursor), not at the lingering copy selection —
    // standard NLE behaviour: position the playhead, then paste there.
    clipboardPaste(this, m_currentFrame);
    e->accept();
    return;
  }
  if (!ctrl && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
    if (m_selSeg.r0 < 0) { QWidget::keyPressEvent(e); return; }
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (!xsh) { QWidget::keyPressEvent(e); return; }
    TXshColumn *c = xsh->getColumn(m_col);
    TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
    if (!sc) { QWidget::keyPressEvent(e); return; }
    TXshSoundColumn *before = dynamic_cast<TXshSoundColumn *>(sc->clone());
    sc->clearCells(m_selSeg.r0, m_selSeg.r1 - m_selSeg.r0 + 1);
    TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
    TUndoManager::manager()->add(
        new UndoAudioEdit(m_col, before, after, QObject::tr("Delete Audio")));
    m_selSeg = {-1, -1};
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    e->accept();
    return;
  }
  QWidget::keyPressEvent(e);
}

// ---- ZtoryAnimaticTrack ----

ZtoryAnimaticTrack::ZtoryAnimaticTrack(QWidget *parent) : QWidget(parent) {
  setFixedHeight(80);
  setMouseTracking(true);
  // ClickFocus: clicking a shot block focuses the track, so the eventFilter on
  // ZtoryAnimaticPanel can verify focus is inside its subtree and fire shortcuts.
  setFocusPolicy(Qt::ClickFocus);

  // Clear thumbnail cache on model reset (shots added/deleted/reordered) so
  // new shots get fresh renders on the next refreshFromScene().
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &ZtoryAnimaticTrack::clearThumbCache);

  // Lock button is painted in paintEvent and activated via mousePressEvent
  // hit-test — no child QToolButton needed.
}

void ZtoryAnimaticTrack::setLocked(bool on) {
  if (m_locked == on) return;
  m_locked = on;
  update();
  emit lockedChanged(on);
}

void ZtoryAnimaticTrack::updateCursor() {
  if (m_tool == RazorTool)
    setCursor(Qt::CrossCursor);
  else
    unsetCursor();
}

void ZtoryAnimaticTrack::setRazorHoverFrame(int frame) {
  if (m_razorHoverFrame == frame) return;
  m_razorHoverFrame = frame;
  update();
}

void ZtoryAnimaticTrack::refreshFromScene() {
  m_blocks.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  int numCols = mainXsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    // Usa getMinFrame/getMaxFrame per avere durata reale incluse celle vuote
    TXshColumn *column = mainXsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    // ignoreLastStop=true: the SFH placed by ZtoryModel::resequenceXsheet()
    // at row r1+1 must not inflate the shot block's duration in the animatic
    // track (otherwise every shot would render 1 frame too long).
    column->getRange(r0, r1, /*ignoreLastStop=*/true);
    if (r1 < r0) continue;
    // Cerca child level per identificare lo shot
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    int startFrame = r0;
    int duration = r1 - r0 + 1;

    ShotBlock b;
    b.col = col;
    b.startFrameInMain = startFrame;
    b.f0 = 0;
    b.f1 = duration - 1;
    // Legge il numero shot dal nome della colonna (impostato da StoryboardPanel)
    QString colName = QString::fromStdString(
        mainXsh->getStageObject(mainXsh->getColumnObjectId(col))->getName());
    b.shotNumber = colName.isEmpty() ? QString("%1").arg(col + 1, 2, 10, QChar('0')) : colName;

    // Transition length derived from the XD-out note column in this shot's
    // sub-scene — the physical, persisted source of truth (survives reload).
    b.transitionFrames = xdNoteHalfCount(cl->getXsheet(), kXDOutName) * 2;

    // Thumbnail: render the composed sub-xsheet frame (all layers) at a small
    // fixed size.  Use a per-column cache so refreshFromScene() called on every
    // xsheetChanged does not re-render unless the shot set actually changed.
    // Cache is cleared by clearThumbCache() which is called on model reset.
    if (m_thumbCache.contains(col)) {
      b.thumbnail = m_thumbCache.value(col);
    } else if (cl) {
      TXsheet *subXsh = cl->getXsheet();
      if (subXsh) {
        QPixmap px = IconGenerator::renderXsheetFrame(
            subXsh, 0, TDimension(160, 90));
        if (!px.isNull()) {
          m_thumbCache.insert(col, px);
          b.thumbnail = px;
        }
      }
    }
    m_blocks.push_back(b);
  }
  // Ordina per startFrameInMain per garantire ordine corretto nel ripple
  std::sort(m_blocks.begin(), m_blocks.end(),
    [](const ShotBlock &a, const ShotBlock &b) {
      return a.startFrameInMain < b.startFrameInMain;
    });

  // Calcola larghezza totale
  int totalFrames = 0;
  for (auto &b : m_blocks)
    totalFrames = qMax(totalFrames, b.startFrameInMain + (b.f1 - b.f0 + 1));
  setMinimumWidth(kLabelW + (int)(totalFrames * m_ppf) + 100);
  update();
}

void ZtoryAnimaticTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);

  p.fillRect(rect(), QColor(30, 30, 30));

  // Label column — aligned with audio track label and ruler
  p.fillRect(0, 0, kLabelW, height(), QColor(40, 40, 40));
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString sceneName = scene
      ? QString::fromStdWString(scene->getSceneName())
      : tr("Animatic");
  // Track name (left of lock button)
  p.setPen(QColor(210, 210, 210));
  p.setFont(QFont("Arial", 9));
  p.drawText(4, 0, kLabelW - 30, height(),
             Qt::AlignVCenter | Qt::AlignLeft, sceneName);

  // Painted lock button (right of label area)
  {
    const int bw = 22, bh = 20;
    QRect r(kLabelW - bw - 4, (height() - bh) / 2, bw, bh);
    p.fillRect(r, m_locked ? QColor(220, 140, 50) : QColor(45, 45, 45));
    p.setPen(m_locked ? QColor(255, 255, 255) : QColor(220, 140, 50));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(r, Qt::AlignCenter, "L");
  }

  p.setPen(QColor(60, 60, 60));
  p.drawLine(kLabelW, 0, kLabelW, height());

  // Active shot column: when the user is editing a shot (sub-scene open), the
  // first ancestor node's column is the active shot's column in the main
  // xsheet. -1 when at the animatic top level (no shot being edited).
  int activeShotCol = -1;
  if (scene) {
    ChildStack *cs = scene->getChildStack();
    if (cs && cs->getAncestorCount() > 0) {
      AncestorNode *node = cs->getAncestorInfo(0);
      if (node) activeShotCol = node->m_col;
    }
  }

  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = kLabelW + (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    int h = height() - 4;
    bool selected = m_selectedCols.count(b.col) > 0;
    bool active   = (b.col == activeShotCol);

    // Sfondo shot
    QColor bg = selected ? QColor(80, 110, 160) : QColor(60, 90, 130);
    p.fillRect(x + 1, 2, w - 2, h, bg);
    p.setPen(selected ? QColor(255, 160, 0) : QColor(100, 140, 200));
    p.drawRect(x + 1, 2, w - 2, h);
    if (selected) {
      p.setPen(QPen(QColor(255, 160, 0), 2));
      p.drawRect(x + 2, 3, w - 4, h - 2);
    }
    // Active shot (being edited) — magenta glow + 2px border so the user always
    // knows which shot they are inside. Drawn on top of the selection border.
    if (active) {
      p.setOpacity(0.18);
      p.fillRect(x + 1, 2, w - 2, h, QColor(0xE0, 0x24, 0x9B));
      p.setOpacity(1.0);
      p.setPen(QPen(QColor(0xE0, 0x24, 0x9B), 2));
      p.drawRect(x + 2, 3, w - 4, h - 2);
    }

    // Thumbnail — visible when block is wide enough to show a useful preview
    int thumbW = 0;
    if (!b.thumbnail.isNull() && w > 40) {
      double aspect = b.thumbnail.width() / (double)qMax(b.thumbnail.height(), 1);
      int thumbH = h - 4;
      thumbW = (int)(thumbH * aspect);
      thumbW = qMin(thumbW, w - 6);
      p.setOpacity(0.85);
      p.drawPixmap(x + 2, 4, thumbW, thumbH, b.thumbnail);
      p.setOpacity(1.0);
    }

    // Numero shot — to the right of the thumbnail if present
    p.setPen(Qt::white);
    int textX = x + 4 + (thumbW > 0 ? thumbW + 2 : 0);
    int textW = w - 8 - (thumbW > 0 ? thumbW + 2 : 0);
    if (textW > 10)
      p.drawText(textX, 2, textW, h, Qt::AlignBottom | Qt::AlignLeft, b.shotNumber);
    else
      p.drawText(x + 4, 2, w - 8, h, Qt::AlignBottom | Qt::AlignLeft, b.shotNumber);

    // Cross-dissolve transition indicator: overlapping triangles on the right edge
    if (b.transitionFrames > 0) {
      int transW = qMax(4, (int)(b.transitionFrames * m_ppf / 2)); // T/2 belongs to this block
      // Trapezoid fill (alpha blend) on the tail of block A
      QColor transColor(255, 160, 60, 160);
      QPolygon tri;
      tri << QPoint(x + w - transW, 2)
          << QPoint(x + w, 2)
          << QPoint(x + w, h + 2);
      p.save();
      p.setRenderHint(QPainter::Antialiasing, true);
      QPainterPath path;
      path.addPolygon(tri);
      p.fillPath(path, transColor);
      // Right-edge tick marking the transition boundary
      p.setPen(QPen(QColor(255, 160, 60), 2));
      p.drawLine(x + w - transW, 2, x + w - transW, h + 2);
      // Frame count label above the triangle
      if (transW > 18) {
        p.setPen(QColor(255, 220, 100));
        p.setFont(QFont("Arial", 7));
        p.drawText(x + w - transW + 2, 2, transW - 2, 12,
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1f").arg(b.transitionFrames));
      }
      p.restore();
    }

    // Handle resize (bordo destro) — drawn on top of transition indicator
    p.fillRect(x + w - 4, 2, 4, h, QColor(180, 180, 80));
  }

  // Draw incoming-side (head) triangles for blocks that have an outgoing transition on the previous block
  for (int bi = 1; bi < (int)m_blocks.size(); bi++) {
    auto &prev = m_blocks[bi - 1];
    if (prev.transitionFrames <= 0) continue;
    auto &cur = m_blocks[bi];
    int x  = kLabelW + (int)(cur.startFrameInMain * m_ppf);
    int h  = height() - 4;
    int transW = qMax(4, (int)(prev.transitionFrames * m_ppf / 2));
    QColor transColor(255, 160, 60, 120);
    QPolygon tri;
    tri << QPoint(x, 2)
        << QPoint(x + transW, 2)
        << QPoint(x, h + 2);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath path;
    path.addPolygon(tri);
    p.fillPath(path, transColor);
    p.restore();
  }

  // Playhead — centered on frame like the ruler
  int px = kLabelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());

  // Razor hover preview — bright vertical line snapped to the frame boundary
  if (m_tool == RazorTool && m_razorHoverFrame >= 0) {
    int rx = kLabelW + (int)(m_razorHoverFrame * m_ppf);
    p.setPen(QPen(QColor(255, 255, 80, 200), 1, Qt::DashLine));
    p.drawLine(rx, 0, rx, height());
  }
}

void ZtoryAnimaticTrack::mousePressEvent(QMouseEvent *e) {
  setFocus(Qt::MouseFocusReason);  // guarantee focus for keyboard shortcuts
  int mx = e->x() - kLabelW;
  // Lock button hit-test in label area
  if (mx < 0) {
    if (e->button() == Qt::LeftButton) {
      const int bw = 22, bh = 20;
      QRect lockRect(kLabelW - bw - 4, (height() - bh) / 2, bw, bh);
      if (lockRect.contains(e->pos()))
        setLocked(!m_locked);
    }
    return;
  }
  if (m_locked) return; // track is locked — block all editing

  // Razor tool: split shot at click position
  if (m_tool == RazorTool && e->button() == Qt::LeftButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        int splitFrame = (int)((mx - x) / m_ppf); // relativo all'inizio del blocco
        if (splitFrame > 0 && splitFrame < duration - 1)
          emit razorRequested(b.col, b.startFrameInMain + splitFrame);
        return;
      }
    }
    return;
  }

  // Tasto destro — context menu
  if (e->button() == Qt::RightButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        QMenu menu(this);
        QAction *matchAct   = menu.addAction(tr("Match Subscene Duration"));
        QAction *mergeAct   = menu.addAction(tr("Merge with Next Shot"));
        QAction *chosen = menu.exec(e->globalPos());
        if (chosen == matchAct)
          emit matchSubsceneDuration(b.col);
        else if (chosen == mergeAct)
          emit mergeWithNextRequested(b.col);
        return;
      }
    }
    return;
  }

  for (int bi = 0; bi < (int)m_blocks.size(); bi++) {
    auto &b = m_blocks[bi];
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);

    // ── TrimTool: Roll on seam, RippleTrim on last edge ──────────────────
    if (m_tool == TrimTool && e->button() == Qt::LeftButton) {
      if (mx >= x + w - 6 && mx <= x + w + 6) {
        bool hasNext = (bi + 1 < (int)m_blocks.size());
        if (hasNext) {
          // Roll: seam between b and the next block
          auto &nb = m_blocks[bi + 1];
          int nextDur = nb.f1 - nb.f0 + 1;
          m_dragMode       = Roll;
          m_dragStartX     = mx;
          m_dragColA       = b.col;
          m_dragColB       = nb.col;
          m_dragOrigDurA   = duration;
          m_dragOrigDurB   = nextDur;
          m_dragOrigStartB = nb.startFrameInMain;
          setCursor(Qt::SplitHCursor);
        } else {
          // RippleTrim: last shot — same as SelectTool drag
          m_dragMode  = RippleTrim;
          m_dragColA  = b.col;
          m_dragStartX = mx;
          m_dragOrigDurA = duration;
          refreshFromScene();
          m_origStarts.clear();
          m_origDurations.clear();
          for (auto &sb : m_blocks) {
            m_origStarts[sb.col] = sb.startFrameInMain;
            m_origDurations[sb.col] = sb.f1 - sb.f0 + 1;
          }
          setCursor(Qt::SizeHorCursor);
        }
        return;
      }
    }

    // ── SelectTool: Alt+drag on seam → TransitionTrim ────────────────────
    if (m_tool == SelectTool && e->button() == Qt::LeftButton &&
        (e->modifiers() & Qt::AltModifier)) {
      bool hasNext = (bi + 1 < (int)m_blocks.size());
      if (hasNext && mx >= x + w - 12 && mx <= x + w + 6) {
        m_dragMode           = TransitionTrim;
        m_dragStartX         = mx;
        m_dragColA           = b.col;
        m_dragColB           = m_blocks[bi + 1].col;
        m_dragOrigTransition = b.transitionFrames;
        setCursor(Qt::SplitHCursor);
        return;
      }
    }

    // ── SelectTool: resize (RippleTrim) on right edge ────────────────────
    if (m_tool == SelectTool && e->button() == Qt::LeftButton) {
      if (mx >= x + w - 6 && mx <= x + w + 2) {
        m_dragMode   = RippleTrim;
        m_dragColA   = b.col;
        m_dragStartX = mx;
        m_dragOrigDurA = duration;
        // Refresh prima di salvare originals per avere dati aggiornati
        refreshFromScene();
        m_origStarts.clear();
        m_origDurations.clear();
        for (auto &sb : m_blocks) {
          m_origStarts[sb.col] = sb.startFrameInMain;
          m_origDurations[sb.col] = sb.f1 - sb.f0 + 1;
        }
        return;
      }
    }
    // Click su shot
    if (mx >= x && mx < x + w) {
      Qt::KeyboardModifiers mods = e->modifiers();
      if (mods & Qt::ShiftModifier && m_lastClickedCol >= 0) {
        // Selezione a range: seleziona tutti i blocchi tra m_lastClickedCol e b.col
        bool inRange = false;
        for (auto &sb : m_blocks) {
          if (sb.col == m_lastClickedCol || sb.col == b.col) {
            inRange = !inRange;
            m_selectedCols.insert(sb.col);
          } else if (inRange) {
            m_selectedCols.insert(sb.col);
          }
        }
        // Garantisce che anche il punto finale sia incluso
        m_selectedCols.insert(b.col);
      } else if (mods & Qt::ControlModifier) {
        // Toggle selezione
        if (m_selectedCols.count(b.col)) m_selectedCols.erase(b.col);
        else m_selectedCols.insert(b.col);
      } else {
        // Click semplice: seleziona solo questo
        m_selectedCols.clear();
        m_selectedCols.insert(b.col);
      }
      m_lastClickedCol = b.col;
      update();
      emit selectionChanged(m_selectedCols);
      emit shotClicked(b.col);
      return;
    }
  }
  // Click su area vuota: deseleziona tutto
  if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
    m_selectedCols.clear();
    m_lastClickedCol = -1;
    update();
    emit selectionChanged(m_selectedCols);
  }
}

void ZtoryAnimaticTrack::mouseMoveEvent(QMouseEvent *e) {
  int mx = e->x() - kLabelW;

  // ── Active drag update ─────────────────────────────────────────────────
  if (m_dragMode == RippleTrim) {
    int dx = mx - m_dragStartX;
    int delta = (int)(dx / m_ppf);
    int origDur = m_dragOrigDurA;
    int newDuration = qMax(origDur + delta, 1);
    // Snap the dragged (right) edge to a target (playhead / audio edge / boundary).
    if (m_snapEnabled && !m_snapFrames.isEmpty()) {
      int startA  = m_origStarts.value(m_dragColA, 0);
      int snapped = snapToTargets(startA + newDuration, m_ppf, m_snapFrames, true);
      newDuration = qMax(1, snapped - startA);
    }
    int newF1 = newDuration - 1;  // f0 is always 0 for RippleTrim

    // Aggiorna durata shot draggato e ricalcola posizioni successive
    int nextStart = -1;
    for (int i = 0; i < (int)m_blocks.size(); i++) {
      if (m_blocks[i].col == m_dragColA) {
        m_blocks[i].f1 = m_blocks[i].f0 + newF1;
        nextStart = m_origStarts[m_blocks[i].col] + newDuration;
      } else if (nextStart >= 0) {
        int origBlockDur = m_origDurations.value(m_blocks[i].col, m_blocks[i].f1 - m_blocks[i].f0 + 1);
        m_blocks[i].startFrameInMain = nextStart;
        nextStart += origBlockDur;
      }
    }
    update();
    return;
  }

  if (m_dragMode == Roll) {
    int dx = mx - m_dragStartX;
    int delta = (int)(dx / m_ppf);
    // Snap the seam (boundary between colA and colB) to a target.
    if (m_snapEnabled && !m_snapFrames.isEmpty()) {
      int seam    = m_dragOrigStartB + delta;
      int snapped = snapToTargets(seam, m_ppf, m_snapFrames, true);
      delta = snapped - m_dragOrigStartB;
    }
    // Clamp: each side must keep at least 1 frame
    delta = qMax(delta, -(m_dragOrigDurA - 1));
    delta = qMin(delta,   m_dragOrigDurB - 1);
    int newDurA = m_dragOrigDurA + delta;
    int newDurB = m_dragOrigDurB - delta;

    for (auto &b : m_blocks) {
      if (b.col == m_dragColA) {
        b.f1 = b.f0 + newDurA - 1;
      } else if (b.col == m_dragColB) {
        b.startFrameInMain = m_dragOrigStartB + delta;
        b.f1 = b.f0 + newDurB - 1;
      }
    }
    update();
    return;
  }

  if (m_dragMode == TransitionTrim) {
    int dx = mx - m_dragStartX;
    int delta = (int)(dx / m_ppf) * 2; // drag 1 frame pixel → 2 total (T/2 each side)
    int newTrans = qMax(0, m_dragOrigTransition + delta);
    newTrans = (newTrans / 2) * 2; // keep even so T/2 is integer
    // Clamp to min duration of the two shots minus 1
    for (int bi = 0; bi < (int)m_blocks.size(); bi++) {
      if (m_blocks[bi].col == m_dragColA) {
        int maxA = m_blocks[bi].f1 - m_blocks[bi].f0; // durA - 1
        int maxB = (bi + 1 < (int)m_blocks.size()) ? (m_blocks[bi+1].f1 - m_blocks[bi+1].f0) : maxA;
        newTrans = qMin(newTrans, qMin(maxA, maxB));
        m_blocks[bi].transitionFrames = newTrans;
        break;
      }
    }
    update();
    return;
  }

  // ── Hover cursor (no active drag) ─────────────────────────────────────
  if (m_tool == SelectTool) {
    bool nearSeam = false;
    if (e->modifiers() & Qt::AltModifier) {
      for (int bi = 0; bi + 1 < (int)m_blocks.size(); bi++) {
        int duration = m_blocks[bi].f1 - m_blocks[bi].f0 + 1;
        int bx1 = (int)((m_blocks[bi].startFrameInMain + duration) * m_ppf);
        if (mx >= bx1 - 12 && mx <= bx1 + 6) { nearSeam = true; break; }
      }
    }
    if (nearSeam) {
      setCursor(Qt::SplitHCursor);
      TApp::instance()->showZtoryHint(
          tr("Alt-drag the seam to add or remove a transition between the two shots"));
    } else {
      bool nearEdge = false;
      for (auto &b : m_blocks) {
        int duration = b.f1 - b.f0 + 1;
        int bx1 = (int)((b.startFrameInMain + duration) * m_ppf);
        if (mx >= bx1 - 6 && mx <= bx1 + 2) { nearEdge = true; break; }
      }
      setCursor(nearEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
      if (nearEdge)
        TApp::instance()->showZtoryHint(
            tr("Drag the right edge to ripple-trim the shot"));
      else
        TApp::instance()->showZtoryHint(
            tr("Double-click to enter the shot and draw  --  "
               "Timing and audio are edited here in the Animatic"));
    }
  } else if (m_tool == TrimTool) {
    Qt::CursorShape cur = Qt::ArrowCursor;
    QString trimHint;
    for (int bi = 0; bi < (int)m_blocks.size(); bi++) {
      int duration = m_blocks[bi].f1 - m_blocks[bi].f0 + 1;
      int bx1 = (int)((m_blocks[bi].startFrameInMain + duration) * m_ppf);
      if (mx >= bx1 - 6 && mx <= bx1 + 6) {
        bool hasNext = (bi + 1 < (int)m_blocks.size());
        cur = hasNext ? Qt::SplitHCursor : Qt::SizeHorCursor;
        trimHint = hasNext
            ? tr("Roll edit: extend one side, shorten the other -- total duration unchanged")
            : tr("Ripple trim: resize the shot, the rest of the sequence shifts accordingly");
        break;
      }
    }
    setCursor(cur);
    if (!trimHint.isEmpty())
      TApp::instance()->showZtoryHint(trimHint);
    else
      TApp::instance()->clearZtoryHint();
  }

  // Razor hover: snap the indicator to the nearest frame boundary
  if (m_tool == RazorTool) {
    TApp::instance()->showZtoryHint(
        tr("Click to split the shot into two panels  --  Total duration stays the same"));
    int frame = (mx >= 0) ? (int)(mx / m_ppf) : -1;
    // Only show hover if cursor is over a valid cut position inside a block
    int hoverFrame = -1;
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int bx0 = (int)(b.startFrameInMain * m_ppf);
      int bx1 = (int)((b.startFrameInMain + duration) * m_ppf);
      if (mx >= bx0 && mx < bx1) {
        int rel = (int)((mx - bx0) / m_ppf);
        if (rel > 0 && rel < duration - 1)
          hoverFrame = b.startFrameInMain + rel;
        break;
      }
    }
    if (hoverFrame != m_razorHoverFrame) {
      m_razorHoverFrame = hoverFrame;
      update();
      emit razorHoverFrameChanged(hoverFrame);
    }
  }
}

void ZtoryAnimaticTrack::leaveEvent(QEvent *) {
  unsetCursor();
  TApp::instance()->clearZtoryHint();
  if (m_tool == RazorTool && m_razorHoverFrame >= 0) {
    m_razorHoverFrame = -1;
    update();
    emit razorHoverFrameChanged(-1);
  }
}

void ZtoryAnimaticTrack::mouseReleaseEvent(QMouseEvent *) {
  DragMode finished = m_dragMode;
  m_dragMode = NoDrag;
  unsetCursor();

  if (finished == RippleTrim) {
    bool found = false;
    for (auto &b : m_blocks) {
      if (b.col == m_dragColA) {
        emit shotDurationChanged(m_dragColA, b.f1 - b.f0);  // newF1 relative to f0
        found = true;
      } else if (found) {
        emit shotMoved(b.col, b.startFrameInMain);
      }
    }
    m_dragColA = -1;
    return;
  }

  if (finished == Roll) {
    int newDurA = -1, newDurB = -1;
    for (auto &b : m_blocks) {
      if (b.col == m_dragColA) newDurA = b.f1 - b.f0 + 1;
      if (b.col == m_dragColB) newDurB = b.f1 - b.f0 + 1;
    }
    if (newDurA > 0 && newDurB > 0 &&
        (newDurA != m_dragOrigDurA || newDurB != m_dragOrigDurB))
      emit rollEdit(m_dragColA, newDurA, m_dragColB, newDurB);
    m_dragColA = m_dragColB = -1;
    return;
  }

  if (finished == TransitionTrim) {
    int newTrans = m_dragOrigTransition;
    for (auto &b : m_blocks) {
      if (b.col == m_dragColA) { newTrans = b.transitionFrames; break; }
    }
    emit transitionChanged(m_dragColA, m_dragColB, newTrans);
    m_dragColA = m_dragColB = -1;
    return;
  }

}

void ZtoryAnimaticTrack::mouseDoubleClickEvent(QMouseEvent *e) {
  int mx = e->x() - kLabelW;
  if (mx < 0) return;
  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    if (mx >= x && mx < x + w) {
      emit shotDoubleClicked(b.col);
      return;
    }
  }
  // Double-click on empty space → return to main xsheet (close any open sub-scene)
  emit returnToMainRequested();
}

void ZtoryAnimaticTrack::wheelEvent(QWheelEvent *e) {
  if (e->modifiers() & Qt::ControlModifier) {
    // Ctrl+Scroll: zoom in/out, keeping the frame under the cursor fixed.
    int delta = e->angleDelta().y();
    double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
    double newPpf = qBound(kMinPpf, m_ppf * factor, kMaxPpf);
    emit zoomChanged(newPpf);
    e->accept();
  } else {
    e->ignore();  // plain scroll → horizontal pan via scroll area
  }
}

// ---- ZtoryStoryStrip ----

ZtoryStoryStrip::ZtoryStoryStrip(QWidget *parent) : QWidget(parent) {
  setFixedHeight(kThumbH + 8);
  setMinimumWidth(100);
  setStyleSheet("background:#1a1a1a;");
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, [this]() { m_thumbCache.clear(); });
}

void ZtoryStoryStrip::refreshFromScene() {
  m_entries.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { update(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { update(); return; }

  int numCols = xsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    ThumbEntry e;
    e.col = col;
    e.shotNumber = ZtoryModel::instance()->shotCount() > col
        ? QString::fromStdString("")  // will use column name below
        : QString("%1").arg(col + 1, 2, 10, QChar('0'));
    QString colName = QString::fromStdString(
        xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (!colName.isEmpty()) e.shotNumber = colName;
    // Render composed frame — cache per-column so we don't re-render every refresh
    if (m_thumbCache.contains(col)) {
      e.thumb = m_thumbCache.value(col);
    } else {
      QPixmap px = IconGenerator::renderXsheetFrame(
          cl->getXsheet(), 0, TDimension(160, 90));
      if (!px.isNull()) {
        m_thumbCache.insert(col, px);
        e.thumb = px;
      }
    }
    m_entries.push_back(e);
  }
  update();
}

void ZtoryStoryStrip::setCurrentCol(int col) {
  m_currentCol = col;
  // Ensure the current shot is visible (auto-scroll)
  int idx = -1;
  for (int i = 0; i < (int)m_entries.size(); i++)
    if (m_entries[i].col == col) { idx = i; break; }
  if (idx >= 0) {
    int x = idx * (kThumbW + kSpacing);
    if (x < m_scrollOffset) m_scrollOffset = x;
    else if (x + kThumbW > m_scrollOffset + width()) m_scrollOffset = x + kThumbW - width();
    m_scrollOffset = qMax(0, m_scrollOffset);
  }
  update();
}

void ZtoryStoryStrip::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(25, 25, 25));

  int x = 4 - m_scrollOffset;
  for (auto &e : m_entries) {
    bool current = (e.col == m_currentCol);
    QRect r(x, 4, kThumbW, kThumbH);

    // Background
    p.fillRect(r, current ? QColor(70, 100, 150) : QColor(45, 45, 45));

    // Thumbnail
    if (!e.thumb.isNull())
      p.drawPixmap(r, e.thumb.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Border
    p.setPen(current ? QColor(255, 160, 0) : QColor(80, 80, 80));
    p.drawRect(r);

    // Shot number overlay
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.drawText(r.adjusted(2, 2, -2, -2), Qt::AlignBottom | Qt::AlignLeft, e.shotNumber);

    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::mousePressEvent(QMouseEvent *e) {
  int mx = e->x();
  int x = 4 - m_scrollOffset;
  for (auto &entry : m_entries) {
    if (mx >= x && mx < x + kThumbW) {
      setCurrentCol(entry.col);
      emit shotClicked(entry.col);
      return;
    }
    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::wheelEvent(QWheelEvent *e) {
  m_scrollOffset -= e->angleDelta().y() / 2;
  int maxOffset = qMax(0, (int)m_entries.size() * (kThumbW + kSpacing) - width());
  m_scrollOffset = qBound(0, m_scrollOffset, maxOffset);
  update();
  e->accept();
}

// ---- ZtoryAnimaticViewer (standalone) ----

ZtoryAnimaticViewer::~ZtoryAnimaticViewer() {
  // Stop FlipConsole's internal play timer/thread before Qt destroys child
  // widgets. QThread::~QThread calls abort() if the thread is still running,
  // which crashes the app on room switch or panel close while playing.
  stopAudio();
  if (m_flipConsole && ZtoryAnimaticController::instance()->frameHandle()->isPlaying())
    m_flipConsole->pressButton(FlipConsole::ePause);
}

ZtoryAnimaticViewer::ZtoryAnimaticViewer(QWidget *parent)
    : BaseViewerPanel(parent) {
  m_sceneViewer->setAlwaysMainXsheet(true);
  m_sceneViewer->setReferenceMode(SceneViewer::CAMERA_REFERENCE);

  // --- Dedicated frame handle from the animatic controller ---
  auto *ctrl = ZtoryAnimaticController::instance();
  ctrl->setViewer(this);  // allow panel to call restartAudioIfPlaying()
  // Override FlipConsole so playback uses the animatic frame, not the global
  m_flipConsole->setFrameHandle(ctrl->frameHandle());
  // Override SceneViewer so rendering reads the animatic frame
  m_sceneViewer->setCustomFrameHandle(ctrl->frameHandle());
  // Disconnect playback from global frame handle, connect to ours
  disconnect(m_flipConsole, SIGNAL(playStateChanged(bool)),
             TApp::instance()->getCurrentFrame(), SLOT(setPlaying(bool)));
  connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
          ctrl->frameHandle(), SLOT(setPlaying(bool)));

  // Don't let this viewer become the global active viewer
  disconnect(TApp::instance(), SIGNAL(activeViewerChanged()),
             this, SLOT(onActiveViewerChanged()));

  // Repaint when the animatic frame changes
  connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched, this, [this]() {
    if (m_sceneViewer) m_sceneViewer->update();
  });

  // Set up layout: scene viewer + flip console (like SceneViewerPanel)
  m_mainLayout->insertWidget(0, m_fsWidget, 1);
  setLayout(m_mainLayout);
  m_visiblePartsFlag = VPPARTS_ALL;
}

// ---- initializeAnimaticTitleBar ----
// Creates a focused set of title-bar buttons for the animatic viewer:
//   • Camera Stand View  (NORMAL_REFERENCE)
//   • Camera View        (CAMERA_REFERENCE) — default
//   • Preview            (full render preview toggle)
// We deliberately skip 3D view, symmetry, perspective, safe area, grids,
// freeze, and sub-camera preview — not meaningful for a timeline viewer.
// IMPORTANT: m_subcameraPreviewButton must be created even if not shown,
// because enableFullPreview() and onSceneSwitched() call setPressed() on it.
void ZtoryAnimaticViewer::initializeAnimaticTitleBar(TPanelTitleBar *titleBar) {
  // Hidden dummy widgets — base-class slots dereference these without null guards.
  m_symmetryButton = new TPanelTitleBarButton(this, getIconPath("pane_symmetry"));
  m_symmetryButton->hide();
  m_perspectiveButton =
      new TPanelTitleBarButton(this, getIconPath("pane_perspective"));
  m_perspectiveButton->hide();

  int x         = -156;
  int iconWidth = 20;

  // ---- Always-visible overlay toggles ----

  // Safe area
  auto *safeBtn =
      new TPanelTitleBarButtonForSafeArea(titleBar, getIconPath("pane_safe"));
  safeBtn->setToolTip(tr("Safe Area (Right Click to Select)"));
  titleBar->add(QPoint(x, 0), safeBtn);
  connect(safeBtn, SIGNAL(toggled(bool)),
          CommandManager::instance()->getAction(MI_SafeArea), SLOT(trigger()));
  connect(CommandManager::instance()->getAction(MI_SafeArea),
          SIGNAL(triggered(bool)), safeBtn, SLOT(setPressed(bool)));
  safeBtn->setPressed(
      CommandManager::instance()->getAction(MI_SafeArea)->isChecked());

  // Field guide
  x += 1 + iconWidth;
  auto *gridBtn = new TPanelTitleBarButton(titleBar, getIconPath("pane_grid"));
  gridBtn->setToolTip(tr("Field Guide"));
  titleBar->add(QPoint(x, 0), gridBtn);
  connect(gridBtn, SIGNAL(toggled(bool)),
          CommandManager::instance()->getAction(MI_FieldGuide), SLOT(trigger()));
  connect(CommandManager::instance()->getAction(MI_FieldGuide),
          SIGNAL(triggered(bool)), gridBtn, SLOT(setPressed(bool)));
  gridBtn->setPressed(
      CommandManager::instance()->getAction(MI_FieldGuide)->isChecked());

  // ---- View mode button set: Camera Stand View + Camera View ----
  TPanelTitleBarButtonSet *viewModeButtonSet = new TPanelTitleBarButtonSet();
  m_referenceModeBs                         = viewModeButtonSet;

  // 10px gap before the camera view group
  x = -105;
  TPanelTitleBarButton *button =
      new TPanelTitleBarButton(titleBar, getIconPath("pane_table"));
  button->setToolTip(tr("Camera Stand View"));
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::NORMAL_REFERENCE);

  x += 1 + iconWidth;
  button = new TPanelTitleBarButton(titleBar, getIconPath("pane_cam"));
  button->setToolTip(tr("Camera View"));
  titleBar->add(QPoint(x, 0), button);
  button->setButtonSet(viewModeButtonSet, SceneViewer::CAMERA_REFERENCE);
  button->setPressed(true);

  connect(viewModeButtonSet, SIGNAL(selected(int)), m_sceneViewer,
          SLOT(setReferenceMode(int)));

  // Preview button
  x += 10 + iconWidth;
  m_previewButton = new TPanelTitleBarButtonForPreview(
      titleBar, getIconPath("pane_preview"));
  titleBar->add(QPoint(x, 0), m_previewButton);
  m_previewButton->setToolTip(tr("Preview"));
  connect(m_previewButton, SIGNAL(toggled(bool)), this,
          SLOT(enableFullPreview(bool)));

  // Sub-camera preview: hidden, kept non-null for base-class safety.
  m_subcameraPreviewButton = new TPanelTitleBarButtonForPreview(
      this, getIconPath("pane_subpreview"));
  m_subcameraPreviewButton->hide();
}

// ---- onDrawFrame override ----
// Called by FlipConsole's internal play timer once per frame.
// The base implementation writes the frame to TApp::getCurrentFrame() (global).
// When a sub-scene is open, that means the sub-scene's frame advances during
// animatic play — wrong behaviour.  We redirect to the dedicated controller
// handle so the animatic stays isolated from the native timeline state.
void ZtoryAnimaticViewer::onDrawFrame(
    int frame, const ImagePainter::VisualSettings &settings,
    QElapsedTimer * /*timer*/, qint64 /*targetInstant*/) {
  m_sceneViewer->setVisual(settings);
  auto *ctrl = ZtoryAnimaticController::instance();

  // When render preview is active, trigger rendering for this frame —
  // the base onDrawFrame does this but we override it entirely.
  // drawPreview() already calls getRaster() from paintGL(), but only
  // after setVisual(); calling it here as well ensures the frame is
  // scheduled even on the very first tick (before paintGL runs).
  if (m_sceneViewer->isPreviewEnabled()) {
    Previewer *pr = Previewer::instance(
        m_sceneViewer->getPreviewMode() == SceneViewer::SUBCAMERA_PREVIEW);
    pr->getRaster(frame - 1, settings.m_recomputeIfNeeded);
  }

  // Handle deferred audio restart (from mute/solo changes during playback).
  // onDrawFrame is called by a Qt timer — between CoreAudio XPC callbacks —
  // making it the safe place to call stopScrub()/play().
  if (m_pendingAudioRestart) {
    m_pendingAudioRestart = false;
    restartAudioIfPlaying();
    // restartAudioIfPlaying() resets m_playStartFrame/m_first so the audio
    // master clock will re-sync on the next iteration — no extra work needed.
  }

  if (!settings.m_drawBlankFrame) {
    int targetFrame;

    if (m_continuousPlay && m_fps > 0) {
      // Detect loop-back: FlipConsole wrapped its internal counter from
      // mark-out back to mark-in.  When frame < m_prevFlipFrame the loop
      // fired — restart the audio stream from the new start position.
      if (m_prevFlipFrame > 0 && frame < m_prevFlipFrame) {
        TXsheet *mainXsh2 = ctrl->mainXsheet();
        if (mainXsh2) mainXsh2->stopScrub();
        ctrl->stopPerColumnAudio();
        m_continuousPlay = false;
        // Restart audio from FlipConsole's new start frame (0-based).
        int newStart = frame - 1;
        m_playStartFrame = newStart;
        if (m_sound && m_hasSoundtrack && mainXsh2) {
          ToonzScene *sc3 = TApp::instance()->getCurrentScene()->getScene();
          double fps3 = sc3
              ? sc3->getProperties()->getOutputProperties()->getFrameRate()
              : 24.0;
          double spf3 = m_sound->getSampleRate() / fps3;
          TINT32 startSmp = (TINT32)(newStart * spf3);
          TINT32 totalSmp = (TINT32)m_sound->getSampleCount();
          int stopFr = videoFrameCount(mainXsh2);
          // Use animatic's own play range — NOT XsheetGUI::getPlayRange which
          // can hold a stale mark-out from a previous native-xsheet session and
          // would cut audio short even after resequenceXsheet() updated the
          // total duration.
          {
            int ar0, ar1;
            ctrl->getAnimaticPlayRange(ar0, ar1);
            if (ar1 >= 0) stopFr = ar1 + 1;
          }
          TINT32 endSmp = std::min((TINT32)(stopFr * spf3), totalSmp - 1);
          if (startSmp <= endSmp && TXsheet::isMainAudioEnabled()) {
            ctrl->startPerColumnAudio(newStart);
            m_continuousPlay = true;
          }
        }
        targetFrame = newStart;
      } else {
        // Audio-master clock: use the first audio column's QAudioOutput
        // processedUSecs() as the authoritative time source.  We can't use
        // mainXsh->m_player anymore — when its volume is 0 (muted because
        // per-column players carry the audible audio), CoreAudio on macOS
        // doesn't advance processedUSecs reliably.  Any column player works
        // since they all start at the same time.
        TXsheet *mainXsh = ctrl->mainXsheet();
        qint64 audioUsecs = ctrl->getMasterAudioUsecs();
        if (audioUsecs > 0) {
          targetFrame = m_playStartFrame + (int)(audioUsecs * m_fps / 1000000.0);
          int totalFrames = videoFrameCount(mainXsh);
          // Also clamp to mark-out so that loop respects it.
          // Only apply the mark-out when at the top (main xsheet) level;
          // inside a sub-scene, updateAnimaticFrameMarkers() cleared markers.
          int markOut = totalFrames - 1;
          // Use animatic's own play range for the video mark-out clamp —
          // NOT XsheetGUI::getPlayRange which can be stale (e.g. set while
          // inside a sub-scene or from a previous session) and stop playback
          // prematurely even after resequenceXsheet() updated the duration.
          {
            int ar0, ar1;
            ctrl->getAnimaticPlayRange(ar0, ar1);
            if (ar1 >= 0) markOut = ar1;
          }
          targetFrame = std::max(0, std::min(targetFrame, markOut));
        } else {
          // DAC not yet started — use FlipConsole frame as fallback.
          targetFrame = frame - 1;
        }
      }
    } else {
      // Not playing (scrubbing) — use FlipConsole frame directly.
      targetFrame = frame - 1;  // 1-based → 0-based
    }
    m_prevFlipFrame = frame;

    int oldFrame = ctrl->currentFrame();
    ctrl->setCurrentFrame(targetFrame);

    // Audio scrub when scrubbing (not during play) — mirrors native onDrawFrame.
    if (!ctrl->frameHandle()->isPlaying() && oldFrame != targetFrame) {
      TXsheet *xsh = ctrl->mainXsheet();
      if (xsh) {
        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        double fps = scene
            ? scene->getProperties()->getOutputProperties()->getFrameRate()
            : 24.0;
        ctrl->frameHandle()->scrubXsheet(targetFrame, targetFrame, xsh, fps);
      }
    }
  } else if (settings.m_blankColor != TPixel::Transparent) {
    if (m_sceneViewer) m_sceneViewer->update();
  }
}

// ---- updateAnimaticFrameRange ----
// Sets the FlipConsole play range from the MAIN xsheet total frame count.
// The base updateFrameRange() uses TApp::getCurrentFrame()->getMaxFrameIndex()
// which, when inside a sub-scene, returns the sub-scene's (shorter) frame
// count — causing play to stop too early.
void ZtoryAnimaticViewer::updateAnimaticFrameRange() {
  auto *ctrl       = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl->mainXsheet();
  if (!mainXsh) return;

  // Compute frame count from VIDEO columns only — skip sound columns.
  // mainXsh->getFrameCount() includes audio columns; after a razor cut the
  // trailing ColumnLevel keeps endOffset=0 (= full raw file length, potentially
  // hours), inflating the count and making the ruler/cursor jump to the right.
  int totalFrames = 0;
  for (int c = 0; c < mainXsh->getColumnCount(); c++) {
    TXshColumn *col = mainXsh->getColumn(c);
    if (!col || col->getSoundColumn()) continue; // skip audio
    int r0, r1;
    // ignoreLastStop=true: keep total length aligned with videoFrameCount()
    // (which also skips the trailing SFH).  Mismatched counts would let the
    // playhead overshoot mark-out by 1 frame per shot.
    if (col->getRange(r0, r1, /*ignoreLastStop=*/true))
      totalFrames = std::max(totalFrames, r1 + 1);
  }
  if (totalFrames <= 0) totalFrames = mainXsh->getFrameCount();
  if (totalFrames <= 0) totalFrames = 1;

  int currentFrame = ctrl->currentFrame();  // 0-based
  // Clamp: currentFrame can exceed totalFrames if audio was longer than video
  // during the last play session (onDrawFrame advances it via audio clock).
  currentFrame = qBound(0, currentFrame, totalFrames - 1);
  // FlipConsole expects 1-based from/to/current values.
  m_flipConsole->setFrameRange(1, totalFrames, 1, currentFrame + 1);
}

// ---- updateAnimaticFrameMarkers ----
// Sets the FlipConsole in/out markers from the MAIN xsheet play range.
// When inside a sub-scene, ToonzScene::getPreviewProperties() has been swapped
// by subscenecommand.cpp to the sub-scene's play range.  The animatic must
// always use the main xsheet markers (or no markers if none are set on main).
// Override of BaseViewerPanel::updateFrameMarkers() — called by the base's
// onSceneChanged() which fires on xsheetChanged (sub-scene open/close).
// Delegates to updateAnimaticFrameMarkers() so markers always come from main.
void ZtoryAnimaticViewer::updateFrameMarkers() {
  updateAnimaticFrameMarkers();
}

void ZtoryAnimaticViewer::updateAnimaticFrameMarkers() {
  auto *ctrl = ZtoryAnimaticController::instance();
  int r0, r1;
  ctrl->getAnimaticPlayRange(r0, r1);
  m_flipConsole->setMarkers(r0, r1);
}

// ---- refreshAnimaticSound ----
// Sets m_sound / m_hasSoundtrack from the controller's CACHED sound track.
//
// IMPORTANT: do NOT call makeSound() here.
// makeSound() for a long audio file can take 1–3 seconds. It is called from
// onAnimaticPlayingStatusChanged() which fires AFTER PlaybackExecutor has
// already started. Because the executor uses BlockingQueuedConnection, it
// blocks waiting for the main thread, accumulating the makeSound() delay.
// When it finally unblocks it skips frames to catch up — but audio starts
// from the original frame, creating an audio-vs-video desync of several
// seconds.  Using the pre-built cache (requireSoundTrack) is instantaneous
// and eliminates the skip entirely.
void ZtoryAnimaticViewer::refreshAnimaticSound() {
  m_sound         = nullptr;
  m_hasSoundtrack = false;
  m_first         = true;
  // requireSoundTrack() builds lazily on first call and caches the result.
  // The cache is invalidated (set to null) whenever audio ColumnLevels change.
  auto *ctrl       = ZtoryAnimaticController::instance();
  m_soundTrackRef  = ctrl->requireSoundTrack(); // keep our own ref alive
  m_sound          = m_soundTrackRef.getPointer(); // safe: ref keeps object alive
  m_hasSoundtrack  = (m_sound != nullptr);
}

// ---- playAnimaticAudioFrame ----
// Used ONLY for per-frame scrub audio (ruler drag, preview bar drag).
// During continuous play the audio streams on its own — this function is a
// no-op so it does not interrupt or replace the streaming buffer.
void ZtoryAnimaticViewer::playAnimaticAudioFrame(int frame) {
  if (m_continuousPlay) return;   // audio already streaming — do nothing
  if (!TXsheet::isMainAudioEnabled()) return;  // audio toggle OFF
  if (!m_sound) return;
  if (m_first) {
    m_first           = false;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    m_fps             = scene->getProperties()->getOutputProperties()
                            ->getFrameRate();
    m_samplesPerFrame = m_sound->getSampleRate() / std::abs(m_fps);
  }
  auto *ctrl = ZtoryAnimaticController::instance();
  if (!ctrl->mainXsheet()) return;
  m_viewerFps = m_flipConsole->getCurrentFps();
  double s0   = frame * m_samplesPerFrame;
  double s1   = s0 + m_samplesPerFrame;
  // Use the dedicated scrub device so ruler scrub doesn't overwrite the
  // continuous-play buffer on mainXsh->m_player.
  if (!TSoundOutputDevice::installed()) return;
  ctrl->scrubDevice()->play(m_sound, s0, s1, false);
}

// ---- onAnimaticPlayingStatusChanged ----
// Runs AFTER base's onPlayingStatusChanged (both connected to playStateChanged).
// Base calls hasSoundtrack() using the sub-scene xsheet → m_sound = null.
// We immediately override with the correct main-xsheet sound, then start a
// single continuous-play call from the current frame to end-of-track.
// Continuous play avoids per-frame buffer replacement (which causes glitches).
void ZtoryAnimaticViewer::onAnimaticPlayingStatusChanged(bool playing) {
  auto *ctrl = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl->mainXsheet();
  if (!playing) {
    // Stop the continuous audio stream.
    m_continuousPlay = false;
    m_prevFlipFrame  = 0;   // reset so next play starts fresh
    if (mainXsh) {
      mainXsh->stopScrub();
      // Restore the master volume — was muted during play because per-column
      // players carried the audible signal.  Future plays start at full mix.
      mainXsh->setMasterVolume(1.0);
    }
    ctrl->stopPerColumnAudio();
    // Clamp ctrl frame to valid video range — audio clock can advance it past
    // the video end if audio is longer than video.
    if (mainXsh) {
      auto *ctrl2 = ZtoryAnimaticController::instance();
      int videoFrames = 0;
      for (int c = 0; c < mainXsh->getColumnCount(); c++) {
        TXshColumn *col = mainXsh->getColumn(c);
        if (!col || col->getSoundColumn()) continue;
        int r0, r1;
        // ignoreLastStop=true: align with videoFrameCount() / totalFrames so
        // the post-stop clamp doesn't leave the playhead past mark-out.
        if (col->getRange(r0, r1, /*ignoreLastStop=*/true))
          videoFrames = std::max(videoFrames, r1 + 1);
      }
      if (videoFrames > 0) {
        int cur = ctrl2->currentFrame();
        if (cur >= videoFrames) ctrl2->setCurrentFrame(videoFrames - 1);
      }
    }
    return;
  }

  // Rebuild m_sound from the main xsheet (base's hasSoundtrack() used the
  // wrong sub-scene xsheet, giving null m_sound and m_hasSoundtrack=false).
  refreshAnimaticSound();

  if (!m_sound || !m_hasSoundtrack || !mainXsh) return;

  // Compute the sample position that corresponds to the current animatic frame.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf           = m_sound->getSampleRate() / fps;
  int startFrame       = ZtoryAnimaticController::instance()->currentFrame();
  TINT32 startSample  = (TINT32)(startFrame * spf);
  TINT32 totalSamples = (TINT32)m_sound->getSampleCount();
  if (startSample >= totalSamples) return;

  // Cap endSample at the animatic timeline length, NOT the raw audio file
  // length.  After a razor cut the trailing ColumnLevel keeps endOffset=0, so
  // its visibleEndFrame equals the raw file length (potentially hours).
  // makeSound() then produces a track with getSampleCount() = raw file size.
  // play(st, s0, rawEnd) calls m_buffer.resize(rawBytes) + memcpy which can
  // allocate/copy hundreds of MB — causing 1-3 s startup delay while the
  // PlaybackExecutor has already advanced video frames, causing A/V desync.
  int    animFrames      = videoFrameCount(mainXsh);
  // Cap audio at the animatic's own mark-out (NOT XsheetGUI::getPlayRange
  // which can be stale and cut audio at the wrong frame).
  int stopFrame = animFrames;
  {
    int ar0, ar1;
    ZtoryAnimaticController::instance()->getAnimaticPlayRange(ar0, ar1);
    if (ar1 >= 0) stopFrame = ar1 + 1;  // exclusive end
  }
  TINT32 animEndSample   = (TINT32)(stopFrame * spf);
  TINT32 endSample       = std::min(animEndSample, totalSamples - 1);
  if (startSample > endSample) return;

  // Also cache fps/samplesPerFrame for any scrub calls that happen before
  // the next refreshAnimaticSound (e.g., if play is cancelled mid-frame).
  m_fps             = fps;
  m_samplesPerFrame = spf;
  m_first           = false;

  // Record the 0-based animatic frame at play-start so that onDrawFrame can
  // compute the audio-master target frame: targetFrame = m_playStartFrame +
  // (int)(audioUsecs * fps / 1e6).  Captured BEFORE play() so that
  // processedUsecs() = 0 at the moment the first onDrawFrame fires.
  m_playStartFrame = startFrame;

  // Start streaming audio from current frame to animatic end in one shot.
  // TSoundOutputDeviceImp uses QAudioOutput with a 100 ms hardware buffer,
  // refilled every 50 ms via QAudioOutput::notify.  One call avoids the
  // per-frame m_buffer replacement that caused glitches in the old approach.
  if (!TXsheet::isMainAudioEnabled()) return;  // audio toggle OFF
  // Per-column audio playback only — no mainXsh mix.  Each column's
  // m_player runs at its column's m_volume (real-time updates via
  // column->setVolume).  The audio-master clock for onDrawFrame comes from
  // ctrl->getMasterAudioUsecs() which reads the first column's processedUsecs.
  ZtoryAnimaticController::instance()->startPerColumnAudio(startFrame);
  m_continuousPlay = true;
}

// ---- restartAudioIfPlaying ----
// Called by the panel after mute/solo changes.  If continuous play is active,
// rebuilds the merged sound track with updated volumes and calls play()
// directly.  m_soundTrackRef keeps the OLD TSoundTrack alive (refcount > 0)
// until refreshAnimaticSound() atomically replaces it — so m_sound is always
// valid and we never hand the audio device a dangling pointer.
void ZtoryAnimaticViewer::restartAudioIfPlaying() {
  if (!m_continuousPlay) return;
  auto *ctrl = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl->mainXsheet();
  if (!mainXsh) return;

  // Ensure QAudioOutput is fully idle before rebuilding.
  // stopScrub() clears m_buffer so that any pending sendBuffer() callbacks
  // return immediately without writing stale data.
  mainXsh->stopScrub();
  ctrl->stopPerColumnAudio();

  int startFrame = ctrl->currentFrame();

  // Rebuild sound with updated volumes (caches already invalidated by caller).
  // refreshAnimaticSound() swaps m_soundTrackRef atomically: old ref kept alive
  // until the assignment, so m_sound never points to freed memory.
  refreshAnimaticSound();
  if (!m_sound) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf = m_sound->getSampleRate() / fps;

  TINT32 startSample  = (TINT32)(startFrame * spf);
  TINT32 totalSamples = (TINT32)m_sound->getSampleCount();
  if (startSample >= totalSamples) return;

  int stopFrame = videoFrameCount(mainXsh);
  // Use animatic play range, not XsheetGUI mark-out (same reasoning as
  // refreshAnimaticSound — avoids stale mark-out cutting audio short).
  {
    int ar0, ar1;
    ctrl->getAnimaticPlayRange(ar0, ar1);
    if (ar1 >= 0) stopFrame = ar1 + 1;
  }
  TINT32 endSample = std::min((TINT32)(stopFrame * spf), totalSamples - 1);
  if (startSample > endSample) return;

  m_fps             = fps;
  m_samplesPerFrame = spf;
  m_first           = false;
  m_playStartFrame  = startFrame;

  if (!TXsheet::isMainAudioEnabled()) return;
  // Per-column audio playback only.  Each column's player carries audible
  // audio at its own volume; clock comes from getMasterAudioUsecs().
  ctrl->startPerColumnAudio(startFrame);
}

void ZtoryAnimaticViewer::stopAudio() {
  ZtoryAnimaticController *ctrl = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl ? ctrl->mainXsheet() : nullptr;
  if (mainXsh) mainXsh->stopScrub();
  m_continuousPlay = false;
  // Also stop controller-managed native audio (used inside sub-scenes)
  if (ctrl) ctrl->stopNativeAudio();
}

void ZtoryAnimaticViewer::onAudioToggleChanged() {
  if (!TXsheet::isMainAudioEnabled()) return;  // audio already OFF — nothing to stop
  // Audio toggled OFF — stop any active playback immediately
  ZtoryAnimaticController *ctrl = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl ? ctrl->mainXsheet() : nullptr;
  if (mainXsh) mainXsh->stopScrub();
  m_continuousPlay = false;
}

void ZtoryAnimaticViewer::showEvent(QShowEvent *e) {
  // BaseViewerPanel::showEvent connects TApp::getCurrentFrame() signals to
  // slots such as updateFrameRange() and changeWindowTitle().  Those would
  // set the FlipConsole range from the sub-scene's frame count (wrong).
  BaseViewerPanel::showEvent(e);

  TApp *app  = TApp::instance();
  auto *ctrl = ZtoryAnimaticController::instance();

  // Remove EVERY connection from the global frame handle to this viewer:
  // - frameSwitched → onFrameSwitched (would advance our FlipConsole in sync
  //   with ComboViewer play, making both appear to play together)
  // - frameSwitched → updateFrameRange (would reset range to sub-scene count)
  // - frameTypeChanged → onFrameTypeChanged (wrong frame type handling)
  // - frameSwitched → changeWindowTitle (cosmetic but wrong)
  disconnect(app->getCurrentFrame(), nullptr, this, nullptr);

  // After base's onSceneChanged() (triggered by sceneSwitched/sceneChanged)
  // calls updateFrameRange() and updateFrameMarkers() with wrong values
  // (sub-scene data), run our corrections right after.
  // hideEvent() (from base) disconnects all sceneHandle→this signals, so
  // these connections are cleaned up automatically on hide.
  connect(app->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);
  connect(app->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);
  connect(app->getCurrentScene(), &TSceneHandle::sceneChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);
  connect(app->getCurrentScene(), &TSceneHandle::sceneChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);
  connect(app->getCurrentScene(), &TSceneHandle::sceneChanged,
          this, &ZtoryAnimaticViewer::onAudioToggleChanged);

  // ctrl frame handle: re-add the range-update connection, removing any
  // previous one first to avoid accumulation across show/hide cycles.
  if (m_frameRangeConn) disconnect(m_frameRangeConn);
  m_frameRangeConn = connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched,
                             this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);

  // Ruler In/Out marker changes: keep FlipConsole markers in sync.
  // hideEvent() from base disconnects all sceneHandle→this signals but NOT ctrl→this,
  // so disconnect first to avoid accumulating connections across show/hide cycles.
  disconnect(ctrl, &ZtoryAnimaticController::playRangeChanged,
             this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);
  connect(ctrl, &ZtoryAnimaticController::playRangeChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);

  // Camera/drawing changes inside a sub-scene: xsheetChanged fires on the
  // current (sub) xsheet but the animatic SceneViewer won't repaint unless we
  // connect it explicitly.  Use xsheetHandle so the connection always targets
  // the currently-active xsheet (native or sub-scene).
  // xsheetChanged: covers structural changes (add/remove keyframe, etc.)
  disconnect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
             m_sceneViewer, static_cast<void(QWidget::*)()>(&QWidget::update));
  connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          m_sceneViewer, static_cast<void(QWidget::*)()>(&QWidget::update));
  // objectChanged: covers camera/peg drag in real-time (SceneViewer connects
  // this in its own showEvent, but re-adding here ensures it survives any
  // disconnect cycle that happens when entering/leaving sub-scenes).
  disconnect(app->getCurrentObject(), SIGNAL(objectChanged(bool)),
             m_sceneViewer, SLOT(update()));
  connect(app->getCurrentObject(), SIGNAL(objectChanged(bool)),
          m_sceneViewer, SLOT(update()));

  // Audio: reconnect our handler (disconnect first to avoid accumulation).
  disconnect(m_flipConsole, SIGNAL(playStateChanged(bool)),
             this, SLOT(onAnimaticPlayingStatusChanged(bool)));
  connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
          this, SLOT(onAnimaticPlayingStatusChanged(bool)));

  // Audio: play sound from main xsheet on each animatic frame change during play.
  if (m_audioConn) disconnect(m_audioConn);
  m_audioConn = connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched,
                        this, [this]() {
    if (!m_playing || !m_playSound || !m_hasSoundtrack) return;
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    playAnimaticAudioFrame(frame);
  });

  // Set correct range and markers immediately (overrides what base just set).
  updateAnimaticFrameRange();
  updateAnimaticFrameMarkers();

  // Pre-build the merged sound track in a background thread so the main
  // thread is never blocked by makeSound() (1–3 s for long audio files).
  // preBuildSoundTrackAsync() detaches a std::thread; the result arrives via
  // QueuedConnection after makeSound() completes — zero UI blocking.
  // If the user presses play before the async build finishes, requireSoundTrack()
  // in refreshAnimaticSound() will build synchronously as a fallback.
  ZtoryAnimaticController::instance()->preBuildSoundTrackAsync();

  if (m_sceneViewer) m_sceneViewer->update();
}


// ---- ZtoryPanelNavigator ----
// Shot panel navigator for ZTORYC T drawing room.
// Shows the active panel of the current shot as a large preview with
// prev/next navigation, editable Dialog/Action/Notes fields, and a
// Sync-to-timeline toggle.

ZtoryPanelNavigator::ZtoryPanelNavigator(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle(tr("Shot Board"));
  setPanelType("ZtoryPanelNavigator");

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay   = new QVBoxLayout(container);
  lay->setContentsMargins(4, 4, 4, 4);
  lay->setSpacing(4);

  // --- Header: shot label ---
  m_headerLabel = new QLabel(tr("No shot"), container);
  m_headerLabel->setAlignment(Qt::AlignCenter);
  m_headerLabel->setStyleSheet("font-weight: bold; color: #d0d0d0;");
  lay->addWidget(m_headerLabel);

  // --- Large single-panel preview ---
  m_previewLabel = new QLabel(container);
  m_previewLabel->setAlignment(Qt::AlignCenter);
  m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  m_previewLabel->setMinimumHeight(120);
  m_previewLabel->setStyleSheet("QLabel{background:#1e1e1e;border:1px solid #444;}");
  lay->addWidget(m_previewLabel, 1);

  // --- Nav row: [<]  P 2/5  [>] ---
  QWidget *navRow     = new QWidget(container);
  QHBoxLayout *navLay = new QHBoxLayout(navRow);
  navLay->setContentsMargins(0, 0, 0, 0);
  navLay->setSpacing(4);
  m_prevBtn = new QToolButton(navRow);
  m_prevBtn->setText(tr("<"));
  m_prevBtn->setFixedWidth(36);
  m_prevBtn->setFixedHeight(28);
  m_panelCountLabel = new QLabel(tr("-"), navRow);
  m_panelCountLabel->setAlignment(Qt::AlignCenter);
  m_panelCountLabel->setStyleSheet("color: #aaa; font-size: 11px;");
  m_nextBtn = new QToolButton(navRow);
  m_nextBtn->setText(tr(">"));
  m_nextBtn->setFixedWidth(36);
  m_nextBtn->setFixedHeight(28);
  navLay->addWidget(m_prevBtn);
  navLay->addWidget(m_panelCountLabel, 1);
  navLay->addWidget(m_nextBtn);
  lay->addWidget(navRow);

  // --- Panel info row (label + duration) and shot total ---
  m_panelInfoLabel = new QLabel(tr("-"), container);
  m_panelInfoLabel->setAlignment(Qt::AlignCenter);
  m_panelInfoLabel->setStyleSheet("color: #c0c0c0; font-size: 11px; font-weight: bold;");
  lay->addWidget(m_panelInfoLabel);
  m_shotInfoLabel = new QLabel(tr("-"), container);
  m_shotInfoLabel->setAlignment(Qt::AlignCenter);
  m_shotInfoLabel->setStyleSheet("color: #909090; font-size: 10px;");
  lay->addWidget(m_shotInfoLabel);

  // --- Text fields ---
  auto makeField = [&](const QString &placeholder) -> QTextEdit * {
    auto *te = new QTextEdit(container);
    te->setPlaceholderText(placeholder);
    te->setFixedHeight(52);
    te->setAcceptRichText(false);
    te->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return te;
  };
  auto makeLbl = [&](const QString &text) -> QLabel * {
    QLabel *l = new QLabel(text, container);
    l->setStyleSheet("color: #a0a0a0; font-size: 10px;");
    return l;
  };
  lay->addWidget(makeLbl(tr("Dialog:")));
  m_dialogField = makeField(tr("Enter dialogue..."));
  lay->addWidget(m_dialogField);
  lay->addWidget(makeLbl(tr("Action:")));
  m_actionField = makeField(tr("Enter action notes..."));
  lay->addWidget(m_actionField);
  lay->addWidget(makeLbl(tr("Notes:")));
  m_notesField = makeField(tr("Enter notes..."));
  lay->addWidget(m_notesField);

  // --- Bottom row: Sync + Auto-match toggles ---
  QWidget *toggleRow     = new QWidget(container);
  QHBoxLayout *toggleLay = new QHBoxLayout(toggleRow);
  toggleLay->setContentsMargins(0, 0, 0, 0);
  toggleLay->setSpacing(4);

  m_syncBtn = new QToolButton(toggleRow);
  m_syncBtn->setText(tr("Sync timeline"));
  m_syncBtn->setCheckable(true);
  m_syncBtn->setChecked(true);
  m_syncBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  toggleLay->addWidget(m_syncBtn);

  // Auto-match mirror button — same state as the animatic toolbar button
  auto *autoMatchBtn = new QToolButton(toggleRow);
  autoMatchBtn->setIcon(createQIcon("ztoryc_automatch"));
  autoMatchBtn->setIconSize(QSize(16, 16));
  autoMatchBtn->setFixedSize(28, 28);
  autoMatchBtn->setCheckable(true);
  autoMatchBtn->setChecked(ZtoryModel::instance()->autoMatch());
  autoMatchBtn->setToolTip(tr("Auto Match Duration\nAutomatically resize the animatic slot\nto match the shot drawing content."));
  autoMatchBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:4px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#c8703a;border-color:#c8703a;}");
  // New Shot After Current — same as Shift+N: from inside a shot it adds the
  // new shot right after this one and enters it directly.
  auto *newShotBtn = new QToolButton(toggleRow);
  newShotBtn->setText("+");
  newShotBtn->setFixedSize(28, 28);
  newShotBtn->setToolTip(
      tr("New shot after this one (Shift+N).\nCreated right after the shot "
         "you are editing; you enter it directly and keep drawing."));
  newShotBtn->setStyleSheet(
      "QToolButton{background:transparent;color:#ccc;border:1px solid #555;border-radius:4px;font-size:15px;}"
      "QToolButton:hover{background:#555;}");
  toggleLay->addWidget(newShotBtn);
  connect(newShotBtn, &QToolButton::clicked, this, [](){
    CommandManager::instance()->execute(MI_ZtoryNewShotAfter);
  });

  // Light-direction placement (same gizmo as the Board, bigger canvas here)
  m_lightEditBtn = new QToolButton(toggleRow);
  m_lightEditBtn->setText(QString::fromUtf8("☀"));
  m_lightEditBtn->setFixedSize(28, 28);
  m_lightEditBtn->setCheckable(true);
  m_lightEditBtn->setToolTip(
      tr("Place light-direction arrow: drag on the preview, mouse wheel tilts "
         "toward camera/background, Shift+wheel sets the beam spread "
         "(right-click removes)"));
  m_lightEditBtn->setStyleSheet(
      "QToolButton{background:transparent;color:#ccc;border:1px solid #555;border-radius:4px;font-size:12px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#5c4a1e;color:#ffd27d;border-color:#5c4a1e;}");
  toggleLay->addWidget(m_lightEditBtn);
  connect(m_lightEditBtn, &QToolButton::toggled, this, [this](bool on) {
    m_previewLabel->setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    if (on && m_lightShowBtn && !m_lightShowBtn->isChecked())
      m_lightShowBtn->setChecked(true);  // arrows must be visible to place them
  });
  m_previewLabel->installEventFilter(this);

  // Colour swatch — same QSettings key as the Board, kept in lockstep.
  auto lightSwatchStyle = [](const QString &c) {
    return QString("QToolButton{background:%1;border:1px solid #555;"
                   "border-radius:4px;margin:6px;}"
                   "QToolButton:hover{border:1px solid #aaa;}").arg(c);
  };
  QString lightColor =
      QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
  m_lightColorBtn = new QToolButton(toggleRow);
  m_lightColorBtn->setFixedSize(28, 28);
  m_lightColorBtn->setToolTip(tr("Light colour (temperature)"));
  m_lightColorBtn->setStyleSheet(lightSwatchStyle(lightColor));
  toggleLay->addWidget(m_lightColorBtn);
  connect(m_lightColorBtn, &QToolButton::clicked, this, [this, lightSwatchStyle]() {
    QColor cur(QSettings().value("Ztoryc/LightColor", "#FFC34D").toString());
    QColor c = QColorDialog::getColor(cur, this, tr("Light colour"));
    if (!c.isValid()) return;
    QSettings().setValue("Ztoryc/LightColor", c.name());
    m_lightColorBtn->setStyleSheet(lightSwatchStyle(c.name()));
    emit ZtoryModel::instance()->overlayDisplayChanged();
  });

  // Visibility toggle — mirrors the Board's "L" button bidirectionally.
  m_lightShowBtn = new QToolButton(toggleRow);
  m_lightShowBtn->setText("L");
  m_lightShowBtn->setFixedSize(28, 28);
  m_lightShowBtn->setCheckable(true);
  m_lightShowBtn->setChecked(
      QSettings().value("Ztoryc/ShowLightDirection", true).toBool());
  m_lightShowBtn->setToolTip(tr("Show light-direction arrows"));
  m_lightShowBtn->setStyleSheet(
      "QToolButton{background:transparent;color:#ccc;border:1px solid #555;border-radius:4px;font-size:12px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#5c4a1e;color:#ffd27d;border-color:#5c4a1e;}");
  toggleLay->addWidget(m_lightShowBtn);
  connect(m_lightShowBtn, &QToolButton::toggled, this, [this](bool on) {
    QSettings().setValue("Ztoryc/ShowLightDirection", on);
    if (!on && m_lightEditBtn->isChecked()) m_lightEditBtn->setChecked(false);
    refreshPreview();
    emit ZtoryModel::instance()->overlayDisplayChanged();
  });

  // Mirror changes made from the Board (or another Shot Board instance).
  connect(ZtoryModel::instance(), &ZtoryModel::overlayDisplayChanged, this,
          [this, lightSwatchStyle]() {
    QString color = QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
    m_lightColorBtn->setStyleSheet(lightSwatchStyle(color));
    bool show = QSettings().value("Ztoryc/ShowLightDirection", true).toBool();
    if (show != m_lightShowBtn->isChecked()) {
      m_lightShowBtn->blockSignals(true);
      m_lightShowBtn->setChecked(show);
      m_lightShowBtn->blockSignals(false);
      if (!show && m_lightEditBtn->isChecked()) m_lightEditBtn->setChecked(false);
    }
    if (m_shotIdx >= 0) refreshPreview();
  });

  toggleLay->addWidget(autoMatchBtn);
  lay->addWidget(toggleRow);

  // Keep in sync with ZtoryModel (shared with the animatic toolbar button)
  connect(autoMatchBtn, &QToolButton::toggled,
          ZtoryModel::instance(), &ZtoryModel::setAutoMatch);
  connect(ZtoryModel::instance(), &ZtoryModel::autoMatchChanged,
          autoMatchBtn, &QToolButton::setChecked);

  setWidget(container);

  // Debounce timer for xsheet changes (draws, erases inside sub-scene)
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setSingleShot(true);
  m_refreshTimer->setInterval(800);
  connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
    if (m_shotIdx >= 0) refreshPreview();
  });

  // --- Connections ---
  connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
          this, &ZtoryPanelNavigator::onShotActivated);
  connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
          this, &ZtoryPanelNavigator::onReturnToMain);
  connect(ZtoryModel::instance(), &ZtoryModel::shotDataChanged,
          this, &ZtoryPanelNavigator::onShotDataChanged);
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &ZtoryPanelNavigator::onModelReset);
  connect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched,
          this, &ZtoryPanelNavigator::onFrameSwitched);

  // Live thumbnail update when drawing/erasing inside the sub-scene
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &ZtoryPanelNavigator::onXsheetChanged);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this]() {
    disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
               this, nullptr);
    connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
            this, &ZtoryPanelNavigator::onXsheetChanged);
    // Sync mode on sub-scene enter/exit
    syncFromScene();
  });

  connect(m_prevBtn, &QToolButton::clicked, this, [this]() {
    if (m_shotIdx < 0 || m_panelIdx <= 0) return;
    setActivePanel(m_panelIdx - 1, m_syncEnabled);
  });
  connect(m_nextBtn, &QToolButton::clicked, this, [this]() {
    if (m_shotIdx < 0) return;
    const auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
    if (m_panelIdx >= (int)panels.size() - 1) return;
    setActivePanel(m_panelIdx + 1, m_syncEnabled);
  });
  connect(m_syncBtn, &QToolButton::toggled, this, [this](bool on) {
    m_syncEnabled = on;
  });

  auto onTextChanged = [this]() {
    if (m_blockSignals || m_shotIdx < 0) return;
    const auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
    if (m_panelIdx < 0 || m_panelIdx >= (int)panels.size()) return;
    PanelData &pd = ZtoryModel::instance()->shot(m_shotIdx).panels[m_panelIdx];
    pd.dialog = m_dialogField->toPlainText();
    pd.action = m_actionField->toPlainText();
    pd.notes  = m_notesField->toPlainText();
    emit ZtoryModel::instance()->shotDataChanged(m_shotIdx);
  };
  connect(m_dialogField, &QTextEdit::textChanged, this, onTextChanged);
  connect(m_actionField, &QTextEdit::textChanged, this, onTextChanged);
  connect(m_notesField,  &QTextEdit::textChanged, this, onTextChanged);
}

void ZtoryPanelNavigator::onShotActivated(int col) {
  int si = ZtoryModel::instance()->shotIndexForCol(col);
  if (si < 0) return;
  m_shotIdx  = si;
  m_panelIdx = 0;
  const auto &shot = ZtoryModel::instance()->shot(si);
  int n = (int)shot.panels.size();
  m_prevBtn->setEnabled(false);
  m_nextBtn->setEnabled(n > 1);
  m_panelCountLabel->setText(n > 0 ? tr("P 1 / %1").arg(n) : tr("-"));
  refreshInfoLabels();
  refreshPreview();
  refreshTextFields();
}

void ZtoryPanelNavigator::onReturnToMain() {
  m_shotIdx  = -1;
  m_panelIdx = 0;
  m_headerLabel->setText(tr("No shot"));
  m_panelCountLabel->setText(tr("-"));
  m_panelInfoLabel->setText(tr("-"));
  m_shotInfoLabel->setText(tr("-"));
  m_previewLabel->clear();
  m_dialogField->clear();
  m_actionField->clear();
  m_notesField->clear();
}

void ZtoryPanelNavigator::onShotDataChanged(int shotIdx) {
  if (shotIdx != m_shotIdx) return;
  refreshInfoLabels();
  refreshPreview();
  // Skip text refresh if the user is currently typing in one of our fields.
  // setPlainText() resets the cursor to position 0, so calling it on every
  // keystroke makes new characters appear at the start instead of after the
  // caret (the "letters going left" bug).
  QWidget *fw = QApplication::focusWidget();
  if (fw == m_dialogField || fw == m_actionField || fw == m_notesField)
    return;
  refreshTextFields();
}

void ZtoryPanelNavigator::onModelReset() {
  if (m_shotIdx < 0) return;
  refreshPreview();
  refreshTextFields();
}

void ZtoryPanelNavigator::onFrameSwitched() {
  if (!m_syncEnabled || m_shotIdx < 0) return;
  refreshActivePanelFromFrame();
}

void ZtoryPanelNavigator::onXsheetChanged() {
  // Re-sync first so we always reflect the current sub-scene, even when
  // the user jumps from one shot to another without xsheetSwitched firing
  // (e.g. via timeline navigation or direct cell double-click).
  syncFromScene();
  if (m_shotIdx >= 0) m_refreshTimer->start();
}

void ZtoryPanelNavigator::syncFromScene() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app && app->getCurrentScene() ? app->getCurrentScene()->getScene() : nullptr;
  if (!scene) return;
  auto *cs = scene->getChildStack();
  if (!cs) return;
  if (cs->getAncestorCount() == 0) {
    if (m_shotIdx >= 0) onReturnToMain();
    return;
  }
  AncestorNode *node = cs->getAncestorInfo(0);
  if (!node) return;
  int newSi = ZtoryModel::instance()->shotIndexForCol(node->m_col);
  if (newSi < 0) return;
  if (newSi != m_shotIdx) onShotActivated(node->m_col);
}

void ZtoryPanelNavigator::setActivePanel(int panelIdx, bool updateFrame) {
  if (m_shotIdx < 0) return;
  const auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
  if (panelIdx < 0 || panelIdx >= (int)panels.size()) return;

  m_panelIdx = panelIdx;
  int n = (int)panels.size();
  m_prevBtn->setEnabled(panelIdx > 0);
  m_nextBtn->setEnabled(panelIdx < n - 1);
  m_panelCountLabel->setText(tr("P %1 / %2").arg(panelIdx + 1).arg(n));

  refreshInfoLabels();
  refreshPreview();
  refreshTextFields();

  if (updateFrame) {
    TApp::instance()->getCurrentFrame()->setFrame(panels[panelIdx].startFrame);
  }
}

// Maps a position inside m_previewLabel to normalized 0-1 coords over the
// DISPLAYED pixmap (which is centered in the label and aspect-correct).
QPointF ZtoryPanelNavigator::normalizedPreviewPos(const QPoint &labelPos) const {
  // Do NOT use QLabel::pixmap(): its signature differs across the Qt builds
  // we target (pointer vs by-value — broke the Windows CI twice). The label
  // always shows m_cachedPreview scaled into the label keeping aspect ratio,
  // so the displayed logical size can be derived from the cache directly.
  if (m_cachedPreview.isNull()) return QPointF(0.5, 0.5);
  QSizeF logical(m_cachedPreview.size());
  logical.scale(QSizeF(m_previewLabel->size()), Qt::KeepAspectRatio);
  QPointF origin((m_previewLabel->width() - logical.width()) / 2.0,
                 (m_previewLabel->height() - logical.height()) / 2.0);
  if (logical.width() <= 0 || logical.height() <= 0) return QPointF(0.5, 0.5);
  return QPointF(
      qBound(0.0, (labelPos.x() - origin.x()) / logical.width(), 1.0),
      qBound(0.0, (labelPos.y() - origin.y()) / logical.height(), 1.0));
}

// Live drag feedback: redraw the cached preview with the in-progress gizmo
// (editing=true → sun glyph + beam + readout, like the Board rubber-band).
void ZtoryPanelNavigator::drawLightRubberBand() {
  if (m_cachedPreview.isNull()) return;
  qreal dpr = 1.0;
  if (QWindow *win = window()->windowHandle()) dpr = win->devicePixelRatio();
  QPixmap scaled = m_cachedPreview;
  scaled.setDevicePixelRatio(1.0);
  scaled = scaled.scaled(m_previewLabel->size() * dpr, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
  scaled.setDevicePixelRatio(dpr);
  QPainter p(&scaled);
  QString color = QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
  ztoryDrawLightGizmo(p, scaled.width() / dpr, scaled.height() / dpr,
                      m_lightDragTail.x(), m_lightDragTail.y(),
                      m_lightDragTip.x(), m_lightDragTip.y(),
                      m_lightDragDepth, m_lightDragSpread, QColor(color),
                      /*editing=*/true);
  p.end();
  m_previewLabel->setPixmap(scaled);
}

// Writes the placed/removed gizmo into ZtoryModel and notifies — the Board
// mirrors the change (thumbnail re-bake + .ztoryc save) via shotDataChanged.
void ZtoryPanelNavigator::commitLightEdit(bool remove) {
  if (m_shotIdx < 0) return;
  auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
  if (m_panelIdx < 0 || m_panelIdx >= (int)panels.size()) return;
  PanelData &pd = panels[m_panelIdx];
  if (remove) {
    if (!pd.hasLight) return;
    pd.hasLight = false;
  } else {
    pd.hasLight    = true;
    pd.lightTailX  = m_lightDragTail.x();
    pd.lightTailY  = m_lightDragTail.y();
    pd.lightTipX   = m_lightDragTip.x();
    pd.lightTipY   = m_lightDragTip.y();
    pd.lightDepth  = m_lightDragDepth;
    pd.lightSpread = m_lightDragSpread;
    pd.lightColor  = QSettings().value("Ztoryc/LightColor", "#FFC34D").toString();
  }
  emit ZtoryModel::instance()->shotDataChanged(m_shotIdx);
  refreshPreview();
}

bool ZtoryPanelNavigator::eventFilter(QObject *obj, QEvent *e) {
  if (obj == m_previewLabel && m_lightEditBtn && m_lightEditBtn->isChecked()) {
    switch (e->type()) {
    case QEvent::MouseButtonPress: {
      auto *me = static_cast<QMouseEvent *>(e);
      if (me->button() == Qt::LeftButton) {
        m_lightDragging  = true;
        m_lightDragDepth = 0.0;
        m_lightDragTail = m_lightDragTip = normalizedPreviewPos(me->pos());
        return true;
      }
      if (me->button() == Qt::RightButton) {
        commitLightEdit(/*remove=*/true);
        return true;
      }
      break;
    }
    case QEvent::MouseMove:
      if (m_lightDragging) {
        m_lightDragTip = normalizedPreviewPos(
            static_cast<QMouseEvent *>(e)->pos());
        drawLightRubberBand();
        return true;
      }
      break;
    case QEvent::MouseButtonRelease:
      if (m_lightDragging &&
          static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton) {
        m_lightDragging = false;
        QPointF d = m_lightDragTip - m_lightDragTail;
        if (std::hypot(d.x() * m_previewLabel->width(),
                       d.y() * m_previewLabel->height()) >= 12.0)
          commitLightEdit(/*remove=*/false);
        else
          refreshPreview();  // discard the rubber-band
        return true;
      }
      break;
    case QEvent::Wheel:
      if (m_lightDragging) {
        auto *we = static_cast<QWheelEvent *>(e);
        if (we->modifiers() & Qt::ShiftModifier)
          m_lightDragSpread = qBound(12.0,
              m_lightDragSpread + (we->angleDelta().y() > 0 ? 3.0 : -3.0), 90.0);
        else
          m_lightDragDepth = qBound(-1.0,
              m_lightDragDepth + (we->angleDelta().y() > 0 ? 0.1 : -0.1), 1.0);
        drawLightRubberBand();
        return true;
      }
      break;
    default:
      break;
    }
  }
  return TPanel::eventFilter(obj, e);
}

void ZtoryPanelNavigator::refreshPreview() {
  if (m_shotIdx < 0) return;
  const auto &shot = ZtoryModel::instance()->shot(m_shotIdx);
  if (m_panelIdx >= (int)shot.panels.size()) return;

  // Logical size of the preview label (may be 0 before first layout pass).
  QSize avail = m_previewLabel->size();
  if (avail.isEmpty()) avail = QSize(320, 180);

  // Device pixel ratio — use the top-level window's handle; windowHandle() on
  // a child widget returns nullptr, so we walk up to window().
  qreal dpr = 1.0;
  if (QWindow *win = window()->windowHandle())
    dpr = win->devicePixelRatio();

  // Fit the render size to the CAMERA aspect inside the available label area.
  // Rendering at the raw label size stretched the frame to the panel's
  // arbitrary proportions (image "too big" / wrong aspect on resize); the
  // Board never had the issue because its labels are hard-locked to 16:9.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  double aspect = 16.0 / 9.0;
  if (scene && scene->getCurrentCamera()) {
    TDimension res = scene->getCurrentCamera()->getRes();
    if (res.lx > 0 && res.ly > 0) aspect = (double)res.lx / res.ly;
  }
  QSize fit(avail.width(), qMax(1, qRound(avail.width() / aspect)));
  if (fit.height() > avail.height())
    fit = QSize(qMax(1, qRound(avail.height() * aspect)), avail.height());
  // Physical pixel size, capped at 1280 wide (proportionally) so very large
  // panels don't trigger a slow full-resolution render.
  QSize physSize = fit * dpr;
  if (physSize.width() > 1280)
    physSize = QSize(1280, qMax(1, qRound(1280.0 / aspect)));
  QPixmap px;
  if (scene) {
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    int col = shot.xsheetColumn;
    TXshChildLevel *cl = nullptr;
    if (xsh) {
      for (int r = 0; r <= xsh->getFrameCount(); r++) {
        TXshCell cell = xsh->getCell(r, col);
        if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
          cl = cell.m_level->getChildLevel();
          break;
        }
      }
    }
    if (cl) {
      // Same render path as the Board thumbnails: backed-out camera-move
      // framing + rectangles/letters overlay, then the light gizmo.
      int moveOrdinal = 0;
      for (int k = 0; k < m_panelIdx && k < (int)shot.panels.size(); k++)
        if (shot.panels[k].cameraMoveType != PanelData::CamNone) moveOrdinal++;
      bool showCamLabel =
          QSettings().value("Ztoryc/ShowCamMoveType", true).toBool();
      px = ztoryRenderPanelPreview(cl->getXsheet(), shot.panels[m_panelIdx],
                                   physSize.width(), physSize.height(),
                                   moveOrdinal, showCamLabel);
      // Tag with DPR so Qt maps the physical pixels back to logical coordinates.
      px.setDevicePixelRatio(dpr);
      if (QSettings().value("Ztoryc/ShowLightDirection", true).toBool())
        ztoryApplyLightOverlay(px, shot.panels[m_panelIdx]);
    }
  }

  // Helper: fit a HiDPI-tagged pixmap into the label without upscaling.
  // The pixmap's logical size (physSize / dpr ≈ avail) fills the label exactly.
  auto display = [&](const QPixmap &src) {
    // If the cached pixmap has the wrong physical size (e.g. panel was resized),
    // scale it smoothly rather than showing a blurry upscale.
    QSize srcPhys = src.size();  // physical pixels, ignoring DPR tag
    if (srcPhys != physSize) {
      QPixmap rescaled = src;
      rescaled.setDevicePixelRatio(1.0);
      rescaled = rescaled.scaled(physSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      rescaled.setDevicePixelRatio(dpr);
      m_previewLabel->setPixmap(rescaled);
    } else {
      m_previewLabel->setPixmap(src);
    }
  };

  if (!px.isNull()) {
    m_cachedPreview = px;
    display(px);
  } else if (!m_cachedPreview.isNull()) {
    display(m_cachedPreview);
  } else {
    QString lbl = shot.panels[m_panelIdx].panelLabel;
    m_previewLabel->setText(lbl.isEmpty() ? tr("(no preview)") : lbl);
  }
}

void ZtoryPanelNavigator::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  syncFromScene();
}

void ZtoryPanelNavigator::resizeEvent(QResizeEvent *e) {
  TPanel::resizeEvent(e);
  if (m_shotIdx < 0) return;
  // Cheap immediate feedback: rescale the cached pixmap (keeps aspect), then
  // re-render at the new resolution only after the resize burst settles —
  // refreshPreview() does a full sub-scene render and lagged on every tick.
  if (!m_cachedPreview.isNull()) {
    qreal dpr = 1.0;
    if (QWindow *win = window()->windowHandle()) dpr = win->devicePixelRatio();
    QPixmap scaled = m_cachedPreview;
    scaled.setDevicePixelRatio(1.0);
    scaled = scaled.scaled(m_previewLabel->size() * dpr, Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    m_previewLabel->setPixmap(scaled);
  }
  if (!m_resizeRenderTimer) {
    m_resizeRenderTimer = new QTimer(this);
    m_resizeRenderTimer->setSingleShot(true);
    m_resizeRenderTimer->setInterval(250);
    connect(m_resizeRenderTimer, &QTimer::timeout, this, [this]() {
      if (m_shotIdx >= 0) refreshPreview();
    });
  }
  m_resizeRenderTimer->start();
}

void ZtoryPanelNavigator::refreshTextFields() {
  if (m_shotIdx < 0) return;
  const auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
  if (m_panelIdx >= (int)panels.size()) return;

  m_blockSignals = true;
  const PanelData &pd = panels[m_panelIdx];
  m_dialogField->setPlainText(pd.dialog);
  m_actionField->setPlainText(pd.action);
  m_notesField->setPlainText(pd.notes);
  m_blockSignals = false;
}

void ZtoryPanelNavigator::refreshInfoLabels() {
  if (m_shotIdx < 0) return;
  ZtoryModel *m = ZtoryModel::instance();
  if (m_shotIdx >= m->shotCount()) return;
  const ShotData &shot = m->shot(m_shotIdx);
  int n   = (int)shot.panels.size();
  int fps = m->fps() > 0 ? m->fps() : 24;

  // Header: full label (SQxxx_SHxxx if sequenced) + panel count
  m_headerLabel->setText(tr("%1  -  %2 panels")
    .arg(m->fullLabel(m_shotIdx))
    .arg(n));

  // Panel info: "P003 - 24f / 1.00s"
  if (m_panelIdx >= 0 && m_panelIdx < n) {
    const PanelData &pd = shot.panels[m_panelIdx];
    QString plabel = pd.panelLabel.isEmpty()
                       ? QString("P%1").arg(m_panelIdx + 1, 3, 10, QChar('0'))
                       : pd.panelLabel;
    double sec = (double)pd.duration / (double)fps;
    m_panelInfoLabel->setText(tr("%1  -  %2f / %3s")
                                .arg(plabel)
                                .arg(pd.duration)
                                .arg(sec, 0, 'f', 2));
  } else {
    m_panelInfoLabel->setText(tr("-"));
  }

  // Shot info: "Total: 96f / 4.00s"
  int total = shot.totalDuration();
  double secTot = (double)total / (double)fps;
  m_shotInfoLabel->setText(tr("Total: %1f / %2s")
                             .arg(total)
                             .arg(secTot, 0, 'f', 2));
}

void ZtoryPanelNavigator::refreshActivePanelFromFrame() {
  if (m_shotIdx < 0) return;
  const auto &panels = ZtoryModel::instance()->shot(m_shotIdx).panels;
  int frame = TApp::instance()->getCurrentFrame()->getFrame();
  for (int i = 0; i < (int)panels.size(); i++) {
    if (frame >= panels[i].startFrame &&
        frame < panels[i].startFrame + panels[i].duration) {
      if (i != m_panelIdx) setActivePanel(i, false);
      return;
    }
  }
}


// ---- ZtoryRightPanel ----

ZtoryRightPanel::ZtoryRightPanel(QWidget *parent)
    : TPanel(parent) {
  // No window title: the switcher row (SCRIPT | PALETTE) already labels this
  // composite panel — a title bar caption would just duplicate it. The title
  // bar strip itself is kept (drag handle for docking in custom rooms).
  setWindowTitle("");
  setPanelType("ZtoryRightPanel");

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay   = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // Toggle bar
  QWidget *bar      = new QWidget(container);
  QHBoxLayout *barL = new QHBoxLayout(bar);
  barL->setContentsMargins(2, 2, 2, 2);
  barL->setSpacing(2);

  m_toggleBtn = new QToolButton(bar);
  m_toggleBtn->setText(tr("SCRIPT | PALETTE"));
  m_toggleBtn->setCheckable(true);
  m_toggleBtn->setChecked(false);
  m_toggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_toggleBtn->setFixedHeight(22);
  barL->addWidget(m_toggleBtn);

  m_linkBtn = new QToolButton(bar);
  m_linkBtn->setText(tr("🔗"));
  m_linkBtn->setCheckable(true);
  m_linkBtn->setChecked(ZtoryModel::instance()->sidePanelsLinked());
  m_linkBtn->setFixedSize(28, 22);
  barL->addWidget(m_linkBtn);
  lay->addWidget(bar);

  m_stack = new QStackedWidget(container);

  // ── Page 0: animatic mode ─────────────────────────────────────────────────
  QSplitter *animaticSplit = new QSplitter(Qt::Vertical, m_stack);

  // Script viewer
  ZtoryScriptView *scriptView = new ZtoryScriptView(animaticSplit);
  animaticSplit->addWidget(scriptView);

  // Record audio mic button bar
  QWidget *micBar     = new QWidget(animaticSplit);
  QHBoxLayout *micLay = new QHBoxLayout(micBar);
  micLay->setContentsMargins(4, 2, 4, 2);
  QToolButton *micBtn = new QToolButton(micBar);
  micBtn->setText(tr("🎙 Record Audio"));
  micBtn->setToolTip(tr("Open Audio Recording panel"));
  micBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  micBtn->setFixedHeight(26);
  micLay->addWidget(micBtn);
  animaticSplit->addWidget(micBar);

  // Script gets all space, mic bar stays compact
  animaticSplit->setStretchFactor(0, 1);
  animaticSplit->setStretchFactor(1, 0);

  m_stack->addWidget(animaticSplit);  // page 0

  lay->addWidget(m_stack, 1);
  setWidget(container);

  // Mic button → open AudioRecordingPopup (floating dialog, existing Tahoma2D command)
  connect(micBtn, &QToolButton::clicked, []() {
    CommandManager::instance()->execute(MI_AudioRecording);
  });

  // Manual toggle
  connect(m_toggleBtn, &QToolButton::toggled, this, [this](bool shotMode) {
    if (shotMode) showShotMode();
    else          showAnimaticMode();
  });

  connect(m_linkBtn, &QToolButton::toggled, this, [](bool on) {
    ZtoryModel::instance()->setSidePanelsLinked(on);
  });

  // Viewer-driven auto-switch
  connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
          this, &ZtoryRightPanel::showShotMode);
  connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
          this, &ZtoryRightPanel::showAnimaticMode);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this]() {
    if (m_stack->currentIndex() != 1) return;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      showAnimaticMode();
  });
}

void ZtoryRightPanel::showAnimaticMode() {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  m_stack->setCurrentIndex(0);
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(false);
  m_toggleBtn->setText(tr("SCRIPT | PALETTE"));
  m_toggleBtn->blockSignals(false);
}

void ZtoryRightPanel::showShotMode(int /*col*/) {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  // Lazy-create shot-mode panels on first activation
  if (!m_styleEditor) {
    QSplitter *shotSplit = new QSplitter(Qt::Vertical, m_stack);

    m_studioPalette = new StudioPaletteViewerPanel(shotSplit);
    m_studioPalette->getTitleBar()->hide();
    m_studioPalette->setEmbedded();
    shotSplit->addWidget(m_studioPalette);

    m_styleEditor = new StyleEditorPanel(shotSplit);
    m_styleEditor->getTitleBar()->hide();
    m_styleEditor->setEmbedded();
    shotSplit->addWidget(m_styleEditor);

    m_levelPalette = new PaletteViewerPanel(shotSplit);
    m_levelPalette->getTitleBar()->hide();
    m_levelPalette->setEmbedded();
    shotSplit->addWidget(m_levelPalette);

    shotSplit->setStretchFactor(0, 3);
    shotSplit->setStretchFactor(1, 4);
    shotSplit->setStretchFactor(2, 3);

    m_stack->addWidget(shotSplit);  // page 1
  }
  // Defer the stack switch so the sub-scene palette is fully loaded before
  // PaletteViewer::showEvent → onPaletteSwitched → updateTabBar runs.
  // Showing the palette viewer before the palette handle is populated causes
  // a null-deref crash inside updateTabBar.
  QTimer::singleShot(0, this, [this]() {
    m_stack->setCurrentIndex(1);
    m_toggleBtn->blockSignals(true);
    m_toggleBtn->setChecked(true);
    m_toggleBtn->setText(tr("PALETTE | SCRIPT"));
    m_toggleBtn->blockSignals(false);
  });
}

// ---- ZtoryLeftPanel ----

ZtoryLeftPanel::ZtoryLeftPanel(QWidget *parent)
    : TPanel(parent) {
  // No window title: the switcher row (BOARD | XSHEET) already labels this
  // composite panel — see ZtoryRightPanel for rationale.
  setWindowTitle("");
  setPanelType("ZtoryLeftPanel");

  QWidget *container  = new QWidget(this);
  QVBoxLayout *lay    = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // Toggle bar: [BOARD | XSHEET (checkable)] [🔗 link]
  QWidget *bar      = new QWidget(container);
  QHBoxLayout *barL = new QHBoxLayout(bar);
  barL->setContentsMargins(2, 2, 2, 2);
  barL->setSpacing(2);

  m_toggleBtn = new QToolButton(bar);
  m_toggleBtn->setText(tr("BOARD | XSHEET"));
  m_toggleBtn->setToolTip(tr("Toggle between Board (storyboard) and XSheet views"));
  m_toggleBtn->setCheckable(true);
  m_toggleBtn->setChecked(false);  // false = Board, true = XSheet
  m_toggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_toggleBtn->setFixedHeight(22);
  barL->addWidget(m_toggleBtn);

  m_linkBtn = new QToolButton(bar);
  m_linkBtn->setText(tr("🔗"));
  m_linkBtn->setToolTip(tr("Link to viewer toggle\n"
                            "When active, switches with the Animatic/Shot viewer"));
  m_linkBtn->setCheckable(true);
  m_linkBtn->setChecked(ZtoryModel::instance()->sidePanelsLinked());
  m_linkBtn->setFixedSize(28, 22);
  barL->addWidget(m_linkBtn);

  lay->addWidget(bar);

  // Stack: page 0 = Board, page 1 = XSheet (lazy)
  m_stack = new QStackedWidget(container);

  m_boardPanel = new StoryboardPanel(m_stack);
  m_boardPanel->getTitleBar()->hide();
  m_boardPanel->setEmbedded();  // prevents drag-to-float when inside ZtoryLeftPanel
  m_stack->addWidget(m_boardPanel);  // page 0

  lay->addWidget(m_stack, 1);
  setWidget(container);

  // Manual toggle button
  connect(m_toggleBtn, &QToolButton::toggled, this, [this](bool xsheetMode) {
    if (xsheetMode) {
      // Lazy-create XSheet panel on first switch
      if (!m_xsheetPanel) {
        m_xsheetPanel = new XsheetViewerPanel(m_stack);
        m_xsheetPanel->getTitleBar()->hide();
        m_xsheetPanel->setEmbedded();
        m_xsheetPanel->reset();
        m_stack->addWidget(m_xsheetPanel);  // page 1
      }
      m_stack->setCurrentIndex(1);
    } else {
      m_stack->setCurrentIndex(0);
    }
    m_toggleBtn->setText(xsheetMode ? tr("XSHEET | BOARD") : tr("BOARD | XSHEET"));
  });

  // Link button syncs state to ZtoryModel
  connect(m_linkBtn, &QToolButton::toggled, this, [](bool on) {
    ZtoryModel::instance()->setSidePanelsLinked(on);
  });

  // Viewer mode signals — only react when linked
  connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
          this, &ZtoryLeftPanel::showShotMode);
  connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
          this, &ZtoryLeftPanel::showBoardMode);
  // Edge case: xsheet navigated back to main by any path
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this]() {
    if (m_stack->currentIndex() != 1) return;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      showBoardMode();
  });
}

void ZtoryLeftPanel::showBoardMode() {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  m_stack->setCurrentIndex(0);
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(false);
  m_toggleBtn->setText(tr("BOARD | XSHEET"));
  m_toggleBtn->blockSignals(false);
}

void ZtoryLeftPanel::showShotMode(int /*col*/) {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  if (!m_xsheetPanel) {
    m_xsheetPanel = new XsheetViewerPanel(m_stack);
    m_xsheetPanel->getTitleBar()->hide();
    m_xsheetPanel->setEmbedded();
    m_xsheetPanel->reset();
    m_stack->addWidget(m_xsheetPanel);  // page 1
  }
  m_stack->setCurrentIndex(1);
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(true);
  m_toggleBtn->setText(tr("XSHEET | BOARD"));
  m_toggleBtn->blockSignals(false);
}

// ---- ZtoryAnimaticViewerPanel ----

ZtoryAnimaticViewerPanel::ZtoryAnimaticViewerPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Viewer");

  // Container: back button (top, hidden in animatic mode) + stacked viewer (below)
  QWidget *container = new QWidget(this);
  QVBoxLayout *lay   = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // Top bar: [← Back to Animatic (expanding)] [🔗 link side panels (fixed)]
  // The whole bar is hidden in animatic mode and shown in shot mode.
  QWidget *topBar   = new QWidget(container);
  QHBoxLayout *barLay = new QHBoxLayout(topBar);
  barLay->setContentsMargins(0, 0, 0, 0);
  barLay->setSpacing(2);

  m_backBtn = new QToolButton(topBar);
  m_backBtn->setText(tr("← Back to Animatic"));
  m_backBtn->setToolTip(tr("Return to Animatic viewer and close sub-scene"));
  m_backBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_backBtn->setFixedHeight(22);
  barLay->addWidget(m_backBtn);

  m_linkBtn = new QToolButton(topBar);
  m_linkBtn->setText(tr("🔗"));
  m_linkBtn->setToolTip(tr("Link side panels to viewer toggle\n"
                            "When active, Board/Xsheet and other panels\n"
                            "switch automatically with the viewer"));
  m_linkBtn->setCheckable(true);
  m_linkBtn->setChecked(ZtoryModel::instance()->sidePanelsLinked());
  m_linkBtn->setFixedSize(28, 22);
  barLay->addWidget(m_linkBtn);

  m_topBar = topBar;
  m_topBar->hide();  // hidden until user enters a shot
  lay->addWidget(m_topBar);

  // QStackedWidget: page 0 = animatic viewer (always present),
  // page 1 = shot viewer (created lazily on first enterShotMode call).
  m_stack = new QStackedWidget(container);

  m_viewer = new ZtoryAnimaticViewer(m_stack);
  m_viewer->setMinimumHeight(120);
  m_stack->addWidget(m_viewer);  // index 0

  lay->addWidget(m_stack, 1);
  setWidget(container);

  // Title bar buttons for the animatic viewer.
  m_viewer->initializeAnimaticTitleBar(getTitleBar());

  // Track the shared view-mode selection (Camera Stand / Camera View) so it can
  // be re-applied to whichever viewer becomes active on an animatic↔shot switch.
  if (auto *bs = m_viewer->referenceModeButtonSet())
    connect(bs, &TPanelTitleBarButtonSet::selected, this,
            [this](int id) { m_currentRefMode = id; });

  // Symmetry and perspective grid buttons — only meaningful in shot edit mode.
  // Created here (hidden) so they share the panel title bar; shown/hidden by
  // enterShotMode() / restoreAnimaticButtons().
  auto makeOverlayBtn = [&](int x, const QString &icon, const QString &tip,
                             const char *actionId) -> TPanelTitleBarButton * {
    auto *btn = new TPanelTitleBarButton(getTitleBar(), getIconPath(icon));
    btn->setToolTip(tr(tip.toUtf8().constData()));
    getTitleBar()->add(QPoint(x, 0), btn);
    connect(btn, SIGNAL(toggled(bool)),
            CommandManager::instance()->getAction(actionId), SLOT(trigger()));
    connect(CommandManager::instance()->getAction(actionId),
            SIGNAL(triggered(bool)), btn, SLOT(setPressed(bool)));
    btn->hide();
    return btn;
  };
  m_overlayButtons.append(makeOverlayBtn(-198, "pane_symmetry",    "Show Symmetry Guide",    MI_ShowSymmetryGuide));
  m_overlayButtons.append(makeOverlayBtn(-177, "pane_perspective", "Show Perspective Grids", MI_ShowPerspectiveGrids));

  // The CommandManager action toggling alone is not enough — the tool object must
  // also be notified via setGuideEnabled() (normally done in onSymmetryGuideToggled,
  // which is only wired in viewers that call initializeTitleBar()).  Mirror that
  // call here so symmetry/perspective drawing works in shot edit mode.
  connect(CommandManager::instance()->getAction(MI_ShowSymmetryGuide),
          &QAction::triggered, this, [](bool on) {
    SymmetryTool *t = dynamic_cast<SymmetryTool *>(
        TTool::getTool(T_Symmetry, TTool::RasterImage));
    if (t) t->setGuideEnabled(on);
  });
  connect(CommandManager::instance()->getAction(MI_ShowPerspectiveGrids),
          &QAction::triggered, this, [](bool on) {
    PerspectiveTool *t = dynamic_cast<PerspectiveTool *>(
        TTool::getTool(T_PerspectiveGrid, TTool::RasterImage));
    if (t) t->setGuideEnabled(on);
  });

  // Back button → return to animatic mode.
  connect(m_backBtn, &QToolButton::clicked,
          this, &ZtoryAnimaticViewerPanel::returnToAnimaticMode);

  // Link button → persist the linked state in ZtoryModel.
  connect(m_linkBtn, &QToolButton::toggled, this, [](bool on) {
    ZtoryModel::instance()->setSidePanelsLinked(on);
  });

  // Model signals: double-click anywhere (board or timeline) triggers enterShotMode;
  // any "back to main" request triggers returnToAnimaticMode.
  connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
          this, &ZtoryAnimaticViewerPanel::enterShotMode);
  connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
          this, &ZtoryAnimaticViewerPanel::returnToAnimaticMode);

  // Edge case: user navigates back to main xsheet via any path other than the
  // back button (e.g., double-click on empty area in the Board panel, or
  // MI_CloseChild from keyboard/menu).  When we detect the xsheet switched back
  // to main level (ancestorCount == 0) while we are in shot view, auto-return.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this]() {
    if (m_stack->currentIndex() == 1) {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene && scene->getChildStack()->getAncestorCount() == 0) {
        // Xsheet returned to main level via any path — restore button routing
        // before hiding the shot viewer (same cleanup as returnToAnimaticMode).
        restoreAnimaticButtons();
        m_stack->setCurrentIndex(0);
        m_topBar->hide();
      }
    }
    updateTitle();
  });

  // Keep the title in sync with shot-data updates (e.g. shot/sequence relabel).
  connect(ZtoryModel::instance(), &ZtoryModel::shotDataChanged,
          this, [this](int){ updateTitle(); });
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &ZtoryAnimaticViewerPanel::updateTitle);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticViewerPanel::updateTitle);

  // Deferred: the factory calls setWindowTitle("Ztoryc Viewer") right after
  // constructing us, which would reset our title. Running updateTitle() on the
  // next event-loop tick guarantees it wins over the factory's hardcoded string.
  QTimer::singleShot(0, this, &ZtoryAnimaticViewerPanel::updateTitle);
}

void ZtoryAnimaticViewerPanel::updateTitle() {
  TApp *app = TApp::instance();
  ToonzScene *scene = (app && app->getCurrentScene()) ? app->getCurrentScene()->getScene() : nullptr;
  QString title;
  if (!scene) {
    title = tr("Animatic");
  } else {
    auto *cs = scene->getChildStack();
    if (!cs || cs->getAncestorCount() == 0) {
      // Animatic (main xsheet) mode — show "Animatic - <scene name>"
      QString sceneName = QString::fromStdWString(scene->getSceneName());
      if (sceneName.isEmpty()) sceneName = tr("Untitled");
      title = tr("Animatic - %1").arg(sceneName);
    } else {
      // Shot editing — show full label (sequence + shot)
      AncestorNode *node = cs->getAncestorInfo(0);
      int si = node ? ZtoryModel::instance()->shotIndexForCol(node->m_col) : -1;
      if (si >= 0)
        title = ZtoryModel::instance()->fullLabel(si);
      else
        title = tr("Shot");
    }
  }
  setWindowTitle(title);
  // TPanel propagates window title to its title bar widget; force a refresh.
  if (getTitleBar()) getTitleBar()->update();
}

void ZtoryAnimaticViewerPanel::enterShotMode(int /*col*/) {
  // Caller (onShotDoubleClicked / onEditShot) has already opened the sub-scene.
  // Here we only switch the viewer page, show the top bar, and (if linked)
  // switch the side panels to shot mode.
  if (!m_shotViewer) {
    m_shotViewer = new ComboViewerPanel(m_stack);
    m_shotViewer->setMinimumHeight(120);
    m_stack->addWidget(m_shotViewer);  // index 1
    // Double-click on the shot viewer returns to animatic mode,
    // mirroring the double-click-to-enter gesture.
    m_shotViewer->installEventFilter(this);
  }

  // Redirect title bar buttons (view mode + preview) from the animatic viewer
  // to the shot viewer so they control the visible panel.
  if (auto *bs = m_viewer->referenceModeButtonSet()) {
    disconnect(bs, SIGNAL(selected(int)), m_viewer->sceneViewer(), SLOT(setReferenceMode(int)));
    connect(bs, SIGNAL(selected(int)), m_shotViewer->sceneViewer(), SLOT(setReferenceMode(int)));
  }

  // Align the now-active shot viewer with the shared button selection so the
  // visible view always matches the pressed button (no toggling needed).
  m_shotViewer->sceneViewer()->setReferenceMode(m_currentRefMode);
  if (auto *pb = m_viewer->previewButton()) {
    disconnect(pb, SIGNAL(toggled(bool)), m_viewer, SLOT(enableFullPreview(bool)));
    connect(pb, SIGNAL(toggled(bool)), m_shotViewer, SLOT(enableFullPreview(bool)));
  }

  for (auto *btn : m_overlayButtons) btn->show();

  m_stack->setCurrentIndex(1);
  m_topBar->show();
}

bool ZtoryAnimaticViewerPanel::eventFilter(QObject *obj, QEvent *e) {
  // Double-click anywhere on the shot viewer exits shot mode —
  // mirrors the double-click-to-enter gesture on the board/timeline.
  if (m_shotViewer &&
      obj == m_shotViewer &&
      e->type() == QEvent::MouseButtonDblClick) {
    returnToAnimaticMode();
    return true;  // consume event
  }
  return TPanel::eventFilter(obj, e);
}

void ZtoryAnimaticViewerPanel::restoreAnimaticButtons() {
  // Re-route title bar buttons (view mode + preview) from shot viewer back to
  // the animatic viewer. Safe to call even if buttons were never rerouted —
  // disconnect() is a no-op when the connection doesn't exist.
  for (auto *btn : m_overlayButtons) btn->hide();
  if (!m_shotViewer) return;
  if (auto *bs = m_viewer->referenceModeButtonSet()) {
    disconnect(bs, SIGNAL(selected(int)), m_shotViewer->sceneViewer(), SLOT(setReferenceMode(int)));
    connect(bs, SIGNAL(selected(int)), m_viewer->sceneViewer(), SLOT(setReferenceMode(int)));
  }
  // Align the animatic viewer with the shared button selection (see enterShotMode).
  m_viewer->sceneViewer()->setReferenceMode(m_currentRefMode);
  if (auto *pb = m_viewer->previewButton()) {
    if (pb->isChecked()) pb->setPressed(false);
    disconnect(pb, SIGNAL(toggled(bool)), m_shotViewer, SLOT(enableFullPreview(bool)));
    connect(pb, SIGNAL(toggled(bool)), m_viewer, SLOT(enableFullPreview(bool)));
  }
}

void ZtoryAnimaticViewerPanel::returnToAnimaticMode() {
  restoreAnimaticButtons();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene) {
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  }
  m_stack->setCurrentIndex(0);
  m_topBar->hide();
}

void ZtoryAnimaticViewerPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  // QStackedWidget manages visibility of the current page; no manual show() needed.
  updateTitle();
}

// ---- ZtoryStoryStripPanel ----

ZtoryStoryStripPanel::ZtoryStoryStripPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Story Strip");
  m_strip = new ZtoryStoryStrip(this);
  setWidget(m_strip);

  // Refresh when scene or xsheet changes
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryStoryStripPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this]() {
    // Defer: xsheetChanged fires mid-import (xsheet not yet stable).
    // Calling IconGenerator::getIcon() synchronously here can trigger
    // PlasticDeformerStorage::process() with an uninitialized GL context → crash.
    QTimer::singleShot(0, this, [this]() {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene && scene->getChildStack()->getAncestorCount() == 0)
        refreshFromScene();
    });
  });
  // Highlight current column when column selection changes
  connect(TApp::instance()->getCurrentColumn(), &TColumnHandle::columnIndexSwitched,
          this, [this]() {
    m_strip->setCurrentCol(TApp::instance()->getCurrentColumn()->getColumnIndex());
  });
  // Clicking a shot in the strip selects its column
  connect(m_strip, &ZtoryStoryStrip::shotClicked,
          this, [](int col) {
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  });
}

void ZtoryStoryStripPanel::refreshFromScene() {
  m_strip->refreshFromScene();
}

void ZtoryStoryStripPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
}

// ---- ZtoryAnimaticPanel (timeline only) ----

ZtoryAnimaticPanel::ZtoryAnimaticPanel(QWidget *parent, bool switchEnabled)
    : TPanel(parent), m_switchEnabled(switchEnabled) {
  setWindowTitle("Ztory Animatic");
  setFocusPolicy(Qt::StrongFocus);

  // Keyboard shortcuts via qApp event filter (same approach as StoryboardPanel).
  // QShortcut + WidgetWithChildrenShortcut was unreliable because CommandManager's
  // ApplicationShortcut QActions consume Cmd+C/X/V/D before KeyPress arrives.
  // The event filter intercepts both ShortcutOverride (to claim the key) and
  // KeyPress (to dispatch the action).
  qApp->installEventFilter(this);

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  m_ruler = new ZtoryAnimaticRuler(container);
  m_track = new ZtoryAnimaticTrack(container);

  // Toolbar
  QWidget *toolbar = new QWidget(container);
  toolbar->setFixedHeight(28);
  toolbar->setStyleSheet("background:#2a2a2a;");
  QHBoxLayout *tbLay = new QHBoxLayout(toolbar);
  tbLay->setContentsMargins(6, 2, 6, 2);
  tbLay->setSpacing(4);
  QLabel *zoomLabel = new QLabel("Zoom:", toolbar);
  zoomLabel->setStyleSheet("color:#ccc; font-size:11px;");
  m_zoomSlider = new QSlider(Qt::Horizontal, toolbar);
  m_zoomSlider->setRange(2, 20000);  // 0.02–200 ppf (×100 scale)
  m_zoomSlider->setValue((int)(m_ppf * 100));
  m_zoomSlider->setMaximumWidth(160);
  m_zoomSlider->setToolTip("Zoom (pixels per frame)");
  tbLay->addWidget(zoomLabel);
  tbLay->addWidget(m_zoomSlider);

  // Fit All: zoom out to show the entire animatic in one view
  m_fitAllBtn = new QToolButton(toolbar);
  m_fitAllBtn->setText("[]");
  m_fitAllBtn->setFixedSize(26, 22);
  m_fitAllBtn->setToolTip(tr("Fit All — zoom to show the entire animatic (Ctrl+0)"));
  m_fitAllBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:3px;"
      "color:#ccc;font-size:10px;font-weight:bold;}"
      "QToolButton:hover{background:#555;}");
  tbLay->addWidget(m_fitAllBtn);
  tbLay->addSpacing(12);

  QToolButton *selectBtn = new QToolButton(toolbar);
  selectBtn->setIcon(createQIcon("ztoryc_select"));
  selectBtn->setIconSize(QSize(20, 20));
  selectBtn->setFixedSize(28, 28);
  selectBtn->setToolTip(tr("Select  (S)\nClick to select shots, drag right edge to resize"));
  selectBtn->setCheckable(true);
  selectBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  selectBtn->setChecked(true);

  QToolButton *trimBtn = new QToolButton(toolbar);
  trimBtn->setIcon(createQIcon("ztoryc_trim"));
  trimBtn->setIconSize(QSize(20, 20));
  trimBtn->setFixedSize(28, 28);
  trimBtn->setToolTip(tr("Trim / Roll  (T)\nDrag seam between two shots to roll the edit point\nDrag last shot edge to ripple trim"));
  trimBtn->setCheckable(true);
  trimBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");

  QToolButton *razorBtn = new QToolButton(toolbar);
  razorBtn->setIcon(createQIcon("ztoryc_razor"));
  razorBtn->setIconSize(QSize(20, 20));
  razorBtn->setFixedSize(28, 28);
  razorBtn->setToolTip(tr("Razor  (C)\nClick to cut a shot at the cursor position"));
  razorBtn->setCheckable(true);
  razorBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");

  QToolButton *linkBtn = new QToolButton(toolbar);
  linkBtn->setIcon(createQIcon("ztoryc_av_link"));
  linkBtn->setIconSize(QSize(20, 20));
  linkBtn->setFixedSize(28, 28);
  linkBtn->setToolTip(tr("A/V Link"));
  linkBtn->setCheckable(true);
  linkBtn->setChecked(true);
  linkBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");

  QToolButton *mergeBtn = new QToolButton(toolbar);
  mergeBtn->setIcon(createQIcon("ztoryc_merge"));
  mergeBtn->setIconSize(QSize(20, 20));
  mergeBtn->setFixedSize(28, 28);
  mergeBtn->setToolTip(tr("Merge Shots"));
  mergeBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  mergeBtn->setToolTip("Merge selected shots into the first selected shot");

  QToolButton *addShotBtn = new QToolButton(toolbar);
  addShotBtn->setIcon(createQIcon("ztoryc_add_shot"));
  addShotBtn->setIconSize(QSize(20, 20));
  addShotBtn->setFixedSize(28, 28);
  addShotBtn->setToolTip(tr("Add Shot"));
  addShotBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");

  tbLay->addWidget(selectBtn);
  tbLay->addWidget(trimBtn);
  tbLay->addWidget(razorBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(linkBtn);
  tbLay->addSpacing(8);
  connect(addShotBtn, &QToolButton::clicked,
          this, &ZtoryAnimaticPanel::onAddShot);
  tbLay->addWidget(addShotBtn);

  // ── Delete — right next to Add (+ -), matching Board toolbar layout ──────
  auto makeAnimaticBtn = [&](const char *icon, const QString &tip) {
    QToolButton *btn = new QToolButton(toolbar);
    btn->setIcon(createQIcon(icon));
    btn->setIconSize(QSize(20, 20));
    btn->setFixedSize(28, 28);
    btn->setToolTip(tip);
    btn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}"
                       "QToolButton:hover{background:#555;}");
    return btn;
  };
  QToolButton *deleteBtn = makeAnimaticBtn("ztoryc_delete_shot", tr("Delete selected shots  (Del)"));
  connect(deleteBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onDeleteShots);
  tbLay->addWidget(deleteBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(mergeBtn);
  tbLay->addSpacing(8);

  // ── Copy / Clone / Paste ─────────────────────────────────────────────────
  auto makeAnimaticBtn2 = [&](const char *icon, const QString &tip) {
    QToolButton *btn = new QToolButton(toolbar);
    btn->setIcon(createQIcon(icon));
    btn->setIconSize(QSize(20, 20));
    btn->setFixedSize(28, 28);
    btn->setToolTip(tip);
    btn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}"
                       "QToolButton:hover{background:#555;}");
    return btn;
  };
  QToolButton *copyBtn  = makeAnimaticBtn2("ztoryc_copy",  tr("Copy selected shots  (Ctrl+C)"));
  QToolButton *cloneBtn = makeAnimaticBtn2("ztoryc_clone", tr("Clone selected shots  (Ctrl+D)"));
  QToolButton *pasteBtn = makeAnimaticBtn2("ztoryc_paste", tr("Paste shots  (Ctrl+V)"));
  connect(copyBtn,  &QToolButton::clicked, this, &ZtoryAnimaticPanel::onCopyShots);
  connect(cloneBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onCloneShots);
  connect(pasteBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onPasteShots);
  tbLay->addWidget(copyBtn);
  tbLay->addWidget(cloneBtn);
  tbLay->addWidget(pasteBtn);
  tbLay->addSpacing(8);

  // Frame / Timecode toggle
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
  connect(m_timecodeBtn, &QToolButton::toggled, this, [this](bool on) {
    m_ruler->setShowTimecode(on);
  });
  tbLay->addWidget(m_timecodeBtn);
  tbLay->addSpacing(8);

  // Snap (magnet) toggle: snap dragged shot/audio edges to shot boundaries,
  // the playhead and other audio-segment edges.
  m_snapBtn = new QToolButton(toolbar);
  m_snapBtn->setText("U");  // simple magnet glyph stand-in
  m_snapBtn->setCheckable(true);
  m_snapBtn->setChecked(m_snapEnabled);
  m_snapBtn->setFixedSize(26, 22);
  m_snapBtn->setToolTip(tr("Snap (magnet) — snap dragged edges to shot "
                           "boundaries, the playhead and audio clip edges"));
  m_snapBtn->setStyleSheet(
      "QToolButton{background:transparent;border:1px solid #555;border-radius:3px;"
      "color:#aaa;font-size:11px;font-weight:bold;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#3a6a9a;color:#fff;border-color:#5599cc;}");
  connect(m_snapBtn, &QToolButton::toggled, this, [this](bool on) {
    m_snapEnabled = on;
    updateSnapFrames();
  });
  tbLay->addWidget(m_snapBtn);
  tbLay->addSpacing(8);

  // Auto-match toggle: when ON, automatically syncs the animatic slot
  // duration to the sub-scene's drawing content whenever it changes.
  m_autoMatchBtn = new QToolButton(toolbar);
  m_autoMatchBtn->setIcon(createQIcon("ztoryc_automatch"));
  m_autoMatchBtn->setIconSize(QSize(20, 20));
  m_autoMatchBtn->setFixedSize(28, 28);
  m_autoMatchBtn->setCheckable(true);
  m_autoMatchBtn->setToolTip(tr("Auto Match Duration\nWhen enabled, the animatic slot automatically\nresizes to match the shot drawing content."));
  m_autoMatchBtn->setStyleSheet(
      "QToolButton{background:transparent;border:none;border-radius:4px;}"
      "QToolButton:hover{background:#555;}"
      "QToolButton:checked{background:#c8703a;}");
  tbLay->addWidget(m_autoMatchBtn);
  // Sync button ↔ ZtoryModel (shared state with ZtoryPanelNavigator)
  connect(m_autoMatchBtn, &QToolButton::toggled,
          ZtoryModel::instance(), &ZtoryModel::setAutoMatch);
  connect(ZtoryModel::instance(), &ZtoryModel::autoMatchChanged,
          m_autoMatchBtn, &QToolButton::setChecked);
  tbLay->addSpacing(12);
  tbLay->addStretch(1);

  // Debounce timer: fires 300ms after the last xsheetChanged while inside
  // a sub-scene with auto-match ON.
  m_autoMatchTimer = new QTimer(this);
  m_autoMatchTimer->setSingleShot(true);
  connect(m_autoMatchTimer, &QTimer::timeout, this, [this]() {
    if (m_autoMatchCol < 0 || m_autoMatchBusy) return;
    m_autoMatchBusy = true;
    onMatchSubsceneDuration(m_autoMatchCol);
    m_autoMatchBusy = false;
    m_autoMatchCol  = -1;
  });

  connect(selectBtn, &QToolButton::clicked, this, [this, selectBtn, trimBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::SelectTool);
    selectBtn->setChecked(true);
    trimBtn->setChecked(false);
    razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(trimBtn, &QToolButton::clicked, this, [this, selectBtn, trimBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::TrimTool);
    trimBtn->setChecked(true);
    selectBtn->setChecked(false);
    razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(razorBtn, &QToolButton::clicked, this, [this, selectBtn, trimBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::RazorTool);
    razorBtn->setChecked(true);
    selectBtn->setChecked(false);
    trimBtn->setChecked(false);
    // Always activate razor on audio tracks so the user can cut them directly.
    // m_audioLinked only controls whether a video cut also cuts audio.
    for (auto *at : m_audioTracks) at->setRazorActive(true);
  });
  connect(linkBtn, &QToolButton::toggled, this, [this](bool checked){
    m_audioLinked = checked;
  });
  connect(mergeBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onMergeShots);

  // Scroll area with ruler + track + audio
  QScrollArea *scroll = new QScrollArea(container);
  m_scroll = scroll;
  m_scrollContent = new QWidget();
  m_scrollLay = new QVBoxLayout(m_scrollContent);
  m_scrollLay->setContentsMargins(0, 0, 0, 0);
  m_scrollLay->setSpacing(0);
  m_scrollLay->addWidget(m_ruler);
  m_scrollLay->addWidget(m_track);
  m_scrollLay->addStretch(1);
  scroll->setWidget(m_scrollContent);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  // On macOS, mouse-move events without button press only reach child widgets
  // if every ancestor up to the scroll viewport also has mouse tracking.
  scroll->viewport()->setMouseTracking(true);
  m_scrollContent->setMouseTracking(true);
  // Pan event filter: intercepts middle-mouse and space+left on ruler/track.
  m_ruler->installEventFilter(this);
  m_track->installEventFilter(this);

  // Layout: toolbar on top, scroll below
  lay->addWidget(toolbar);
  lay->addWidget(scroll, 1);

  // Outer stacked widget: page 0 = animatic, page 1 = native timeline
  m_outerStack = new QStackedWidget(this);
  m_outerStack->addWidget(container);  // page 0
  setWidget(m_outerStack);

  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryAnimaticPanel::onFrameChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryAnimaticPanel::onShotClicked);
  // Clear audio track selection when the video track is clicked
  connect(m_track, &ZtoryAnimaticTrack::shotClicked, this, [this]() {
    for (auto *at : m_audioTracks) at->clearSelection();
  });
  connect(m_track, &ZtoryAnimaticTrack::shotDoubleClicked,
          this, &ZtoryAnimaticPanel::onShotDoubleClicked);
  // Sync selection to ZtoryModel so Board's merge button can use it.
  connect(m_track, &ZtoryAnimaticTrack::selectionChanged,
          [](std::set<int> sel){
              ZtoryModel::instance()->setSharedSelection(std::move(sel));
          });
  // Mirror selection changes coming from the Board (or elsewhere): highlight
  // the matching clip(s) in the timeline. The no-op guard in
  // setSelectedColsFromShared / setSharedSelection prevents an update loop.
  connect(ZtoryModel::instance(), &ZtoryModel::sharedSelectionChanged,
          this, [this]() {
            m_track->setSelectedColsFromShared(
                ZtoryModel::instance()->sharedSelection());
          });
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryAnimaticPanel::onZoomChanged);
  connect(m_ruler, &ZtoryAnimaticRuler::zoomChanged,
          this, &ZtoryAnimaticPanel::onZoomChanged);
  connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v){
    onZoomChanged(v / 100.0);
  });
  connect(m_fitAllBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onFitAll);
  connect(m_track, &ZtoryAnimaticTrack::matchSubsceneDuration,
          this, &ZtoryAnimaticPanel::onMatchSubsceneDuration);
  connect(m_track, &ZtoryAnimaticTrack::razorRequested,
          this, &ZtoryAnimaticPanel::onRazorRequested);
  connect(m_track, &ZtoryAnimaticTrack::mergeWithNextRequested,
          this, &ZtoryAnimaticPanel::onMergeWithNext);
  connect(m_track, &ZtoryAnimaticTrack::returnToMainRequested,
          this, &ZtoryAnimaticPanel::onReturnToMain);
  connect(m_track, &ZtoryAnimaticTrack::shotDurationChanged,
          this, &ZtoryAnimaticPanel::onShotDurationChanged);
  connect(m_track, &ZtoryAnimaticTrack::rollEdit,
          this, &ZtoryAnimaticPanel::onRollEdit);
  connect(m_track, &ZtoryAnimaticTrack::shotMoved,
          this, &ZtoryAnimaticPanel::onShotMoved);
  connect(m_track, &ZtoryAnimaticTrack::transitionChanged,
          this, &ZtoryAnimaticPanel::onTransitionChanged);
  // Video lock state — no persistence map needed (single track)
  // The lock state lives directly on m_track and survives refreshFromScene()
  // because m_track itself is not rebuilt (only its blocks are refreshed).

  // BUG-02 fix: sceneSwitched fires when entering a sub-scene too.
  // Without this guard, refreshFromScene() would call getTopXsheet() which
  // returns the sub-scene, causing the timeline to show its columns instead
  // of the main shot-list.  Same guard already used for xsheetChanged below.
  //
  // Also invalidate the controller's sound track cache here: requireSoundTrack()
  // caches the mixed TSoundTrackP lazily and never resets it on scene switch,
  // causing the play-on-selection bar to play audio from the previous scene.
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    // Invalidate regardless of level — a new scene always needs fresh audio.
    ZtoryAnimaticController::instance()->invalidateSoundTrack();
    if (scene->getChildStack()->getAncestorCount() != 0) return;
    // A new scene starts at frame 0 — reset the scroll BEFORE refreshFromScene
    // so its scroll-preserve logic pins 0 instead of the previous scene's offset.
    if (m_scroll) m_scroll->horizontalScrollBar()->setValue(0);
    refreshFromScene();
    m_ruler->resetPlayRangeToFull();
  });
  // Animatic panel listens to the controller's dedicated frame handle,
  // NOT the global TApp frame.  This decouples the animatic playhead from
  // the native timeline cursor (BUG-03 fix).
  auto *animCtrl = ZtoryAnimaticController::instance();
  connect(animCtrl->frameHandle(), &TFrameHandle::frameSwitched,
          this, [this](){
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    m_ruler->setCurrentFrame(frame);
    m_track->setCurrentFrame(frame);
    for (auto *at : m_audioTracks)
      at->setCurrentFrame(frame);
    updateSnapFrames();  // keep the playhead snap target current
  });
  // Repaint the timeline whenever the current xsheet switches (entering or
  // leaving a shot). The active-shot highlight depends on the child-stack
  // depth, which only changes here — not on frameSwitched/xsheetChanged. A bare
  // update() is enough: it does not rebuild blocks, just redraws them.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this](){ m_track->update(); });
  // Rebuild the blocks on every model resequence, even while a sub-scene is
  // open (the xsheetChanged refresh below is guarded by ancestorCount == 0):
  // covers New Shot After Current invoked from inside a shot, which would
  // otherwise leave the timeline stale. refreshFromScene() reads the TOP
  // xsheet, so the open sub-scene is irrelevant. Deferred: modelReset can
  // fire mid-operation while the xsheet is not yet stable.
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, [this](){
    QTimer::singleShot(0, this, [this](){ refreshFromScene(); });
  });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this](){
    // Defer: xsheetChanged fires mid-import (xsheet not yet stable).
    // Calling IconGenerator::getIcon() synchronously here can trigger
    // PlasticDeformerStorage::process() with an uninitialized GL context → crash.
    QTimer::singleShot(0, this, [this](){
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene && scene->getChildStack()->getAncestorCount() == 0)
        refreshFromScene();
    });

    // Auto-match: if the toggle is ON and we're inside a sub-scene, find
    // the corresponding main-xsheet column and schedule a debounced resize.
    if (!ZtoryModel::instance()->autoMatch()) return;
    if (m_autoMatchBusy) return;  // re-entrancy guard: our own resize fired this
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene || scene->getChildStack()->getAncestorCount() == 0) return;
    // Find which main-xsheet column owns the currently open sub-scene
    TXsheet *curXsh  = scene->getChildStack()->getXsheet();
    TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
    if (!curXsh || !mainXsh) return;
    // Optimization: all cells in a shot column point to the same child level,
    // so checking only the first non-empty cell is sufficient — O(cols) not O(cols×rows).
    int foundCol = -1;
    for (int col = 0; col < mainXsh->getColumnCount() && foundCol < 0; col++) {
      TXshColumn *c = mainXsh->getColumn(col);
      if (!c) continue;
      int r0 = 0, r1 = 0;
      c->getRange(r0, r1);
      TXshCell cell = mainXsh->getCell(r0, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel() &&
          cell.m_level->getChildLevel()->getXsheet() == curXsh)
        foundCol = col;
    }
    if (foundCol < 0) return;
    m_autoMatchCol = foundCol;
    m_autoMatchTimer->start(300);  // debounce: restart on every change burst
  });

  // If this is the T-variant (m_switchEnabled), connect timeline switch
  if (m_switchEnabled) {
    connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
            this, [this](int) { showShotTimeline(); });
    connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
            this, [this]() { showAnimaticTimeline(); });
    connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
            this, [this]() {
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (!scene) return;
      if (scene->getChildStack()->getAncestorCount() == 0)
        showAnimaticTimeline();
      else
        showShotTimeline();
    });
  }
}

void ZtoryAnimaticPanel::refreshFromScene() {
  if (m_refreshing) return;
  m_refreshing = true;
  // Preserve the horizontal scroll across the rebuild: refreshing the tracks
  // never changes absolute frame positions, so the view must stay put.  This
  // covers every edit that fires notifyXsheetChanged() (razor, audio paste/cut/
  // delete/drag).  An explicit pin (m_restoreScrollX, set before a deferred
  // cascade) takes precedence; otherwise we re-pin the current value.  Scene
  // switches reset the scroll to 0 separately in the sceneSwitched handler.
  int keepScrollX = (m_restoreScrollX >= 0)
      ? m_restoreScrollX
      : (m_scroll ? m_scroll->horizontalScrollBar()->value() : -1);
  m_track->refreshFromScene();
  refreshAudioTracks();
  updateTrackWidths();
  // Keep ruler FPS in sync so the TC toggle shows correct timecode after
  // the user changes frame rate in Scene Settings (fires sceneChanged).
  if (m_ruler) {
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    if (sc) {
      double fps = sc->getProperties()->getOutputProperties()->getFrameRate();
      m_ruler->setFps(fps);
    }
  }
  if (keepScrollX >= 0 && m_scroll) {
    if (m_scrollContent && m_scrollContent->layout())
      m_scrollContent->resize(m_scrollContent->layout()->sizeHint().width(),
                              m_scrollContent->height());
    m_scroll->horizontalScrollBar()->setValue(keepScrollX);
  }
  m_refreshing = false;
}

void ZtoryAnimaticPanel::updateTrackWidths() {
  // Use video block extents only.  Audio ColumnLevel extents are NOT used here:
  // after a razor cut the right segment's endOffset=0 makes getVisibleEndFrame()
  // return the raw file duration (potentially minutes/hours), inflating totalW
  // and making the ruler/cursor appear to jump to that frame.
  // Animatic length is driven by video shots, not audio placement.
  int maxFrame = 0;
  for (auto &b : m_track->blocks())
    maxFrame = qMax(maxFrame, b.startFrameInMain + (b.f1 - b.f0 + 1));
  if (maxFrame <= 0) maxFrame = 1;
  int totalW = kLabelW + (int)(maxFrame * m_ppf) + 200;
  m_ruler->setMinimumWidth(totalW);
  m_track->setMinimumWidth(totalW);
  for (auto *at : m_audioTracks)
    at->setMinimumWidth(totalW);
}

void ZtoryAnimaticPanel::updateCutFrames() {
  QVector<int> cutFrames;
  for (auto &b : m_track->blocks()) {
    if (b.startFrameInMain > 0)
      cutFrames.append(b.startFrameInMain);
  }
  for (auto *at : m_audioTracks)
    at->setCutFrames(cutFrames);
  updateSnapFrames();
}

void ZtoryAnimaticPanel::updateSnapFrames() {
  // Build the snap-target list: shot boundaries (start + end of every block),
  // the playhead, and every audio segment edge (start + one-past-end).
  QVector<int> targets;
  for (auto &b : m_track->blocks()) {
    int dur = b.f1 - b.f0 + 1;
    targets.append(b.startFrameInMain);
    targets.append(b.startFrameInMain + dur);
  }
  targets.append(ZtoryAnimaticController::instance()->currentFrame());
  for (auto *at : m_audioTracks) {
    for (const auto &s : at->findSegments()) {
      targets.append(s.r0);
      targets.append(s.r1 + 1);
    }
  }
  // Push to every track.
  m_track->setSnapEnabled(m_snapEnabled);
  m_track->setSnapFrames(targets);
  for (auto *at : m_audioTracks) {
    at->setSnapEnabled(m_snapEnabled);
    at->setSnapFrames(targets);
  }
}

void ZtoryAnimaticPanel::applyMuteSolo() {
  // Visual state is already set on the widgets by the click handler.
  // Apply effective volume to each xsheet sound column.
  // Solo logic: if any track is solo'd, only solo tracks are audible.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
  if (!xsh) return;

  // Check if any track is solo'd
  bool hasSolo = false;
  for (auto *at : m_audioTracks)
    if (at->isSolo()) { hasSolo = true; break; }

  for (auto *at : m_audioTracks) {
    int col = at->columnIndex();
    // M wins over S: a muted track is silent even if solo'd.
    // A non-solo track is silenced when any other track is solo'd.
    bool effectiveMute = at->isMuted() || (hasSolo && !at->isSolo());
    // Dim waveform for solo-silenced tracks (M already shown via button color)
    at->setEffectiveMuted(hasSolo && !at->isSolo());
    TXshColumn *xshCol = xsh->getColumn(col);
    TXshSoundColumn *sc = xshCol ? xshCol->getSoundColumn() : nullptr;
    if (sc) sc->setVolume(effectiveMute ? 0.0 : 1.0);
  }
  // Invalidate BOTH caches: TXsheet's internal m_mixedSound AND our controller
  // cache. Without xsh->invalidateSound(), makeSound() returns stale data.
  xsh->invalidateSound();
  auto *ctrl = ZtoryAnimaticController::instance();
  ctrl->invalidateSoundTrack();

  // Schedule an audio restart on the next onDrawFrame tick.
  // We CANNOT call stopScrub()/play() here directly: this slot runs during a
  // button-click event, and a CoreAudio XPC callback may be in-flight
  // concurrently.  onDrawFrame is called from a Qt timer, which fires between
  // XPC callbacks on the main thread — it is the safe place to replace the
  // audio stream.
  if (ctrl->viewer()) ctrl->viewer()->requestAudioRestart();
}

void ZtoryAnimaticPanel::restoreTrackStates() {
  // Called after refreshAudioTracks() rebuilds widgets — re-apply persisted
  // UI state (button checked/unchecked) only.  Do NOT call applyMuteSolo()
  // here: it triggers invalidateSound() + restartAudioIfPlaying() which can
  // destroy the sound track while the audio device thread is still playing,
  // causing heap corruption / SIGSEGV.  The column volumes were already set
  // before the rebuild and remain valid.
  for (auto *at : m_audioTracks) {
    int col = at->columnIndex();
    if (m_colLocked.value(col, false)) at->setLocked(true);
    if (m_colMuted.value(col, false))  at->setMuted(true);
    if (m_colSolo.contains(col))       at->setSolo(true);
  }
}

void ZtoryAnimaticPanel::refreshAudioTracks() {
  if (m_refreshingAudio) return;

  // Check preconditions BEFORE setting the re-entrancy guard so that an early
  // return never leaves m_refreshingAudio stuck at true.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  m_refreshingAudio = true;

  // ── Fast path: match by TXshSoundColumn* pointer ─────────────────────────
  // Column indices shift whenever shots are added/removed, but the SC pointer
  // is stable (insert/delete renumbers indices, does not recreate objects).
  // Build SC→newCol map, then update existing tracks in-place if all SCs match.
  // This eliminates flicker for any operation that doesn't add/remove audio tracks.
  {
    QMap<TXshSoundColumn*, int> scToCol;
    for (int col = 0; col < xsh->getColumnCount(); col++) {
      TXshColumn *c = xsh->getColumn(col);
      if (!c) continue;
      TXshSoundColumn *sc = c->getSoundColumn();
      if (sc) scToCol[sc] = col;
    }
    // Check that every existing track's SC is still present (no add/remove)
    bool canReuse = (scToCol.size() == m_audioTracks.size());
    if (canReuse) {
      for (auto *at : m_audioTracks)
        if (!scToCol.contains(at->soundColumn())) { canReuse = false; break; }
    }
    if (canReuse) {
      // Update column indices (may have shifted) and repaint in-place — no flicker
      for (auto *at : m_audioTracks) {
        at->setColumnIndex(scToCol[at->soundColumn()]);
        at->invalidateWaveform();
      }
      updateTrackWidths();
      restoreTrackStates();
      m_refreshingAudio = false;
      return;
    }
  }

  // ── Full rebuild: column structure changed ────────────────────────────────
  // Rimuovi tracce audio esistenti.
  // cancelDrag() prevents spurious shiftLevelInRange commits on stale widgets.
  // deleteLater() (not delete) defers destruction until after the current
  // event finishes — avoids SIGABRT when refreshAudioTracks() is called
  // from inside a ZtoryAudioTrack event handler (e.g. razorRequested).
  for (auto *at : m_audioTracks) {
    at->cancelDrag();
    m_scrollLay->removeWidget(at);
    at->hide();       // hide immediately — deleteLater defers destruction but
    at->deleteLater(); // the widget stays visible until the event loop ticks
  }
  m_audioTracks.clear();

  // Trova tutte le colonne audio nel main xsheet
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc) continue;

    QString name = QString::fromStdString(xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (name.isEmpty()) name = tr("Audio %1").arg(col + 1);

    ZtoryAudioTrack *at = new ZtoryAudioTrack(col, name, m_scrollContent);
    at->setPixelsPerFrame(m_ppf);
    at->setCurrentFrame(m_track->property("currentFrame").toInt());
    // Propagate current razor state to newly created audio track.
    // m_audioLinked only controls whether a video cut also cuts audio;
    // audio tracks are always razerable when the razor tool is active.
    at->setRazorActive(m_track->tool() == ZtoryAnimaticTrack::RazorTool);

    // Cut frames will be set by updateCutFrames() after all tracks are created

    // Sync razor hover across video ↔ audio tracks
    connect(m_track, &ZtoryAnimaticTrack::razorHoverFrameChanged,
            at, &ZtoryAudioTrack::setRazorHoverFrame);

    connect(at, &ZtoryAudioTrack::razorRequested,
            this, &ZtoryAnimaticPanel::onAudioRazorRequested);
    // Ctrl+Scroll on audio track: zoom the whole animatic (same as ruler/video track)
    connect(at, &ZtoryAudioTrack::zoomChanged,
            this, &ZtoryAnimaticPanel::onZoomChanged);
    // segmentMoved: audio segment was dragged. Notify the xsheet via a queued
    // (deferred) call so we are NOT inside the widget's event handler when
    // notifyXsheetChanged fires — that would trigger refreshAudioTracks() and
    // delete the widget mid-event.
    connect(at, &ZtoryAudioTrack::segmentMoved, this, [this]() {
      // NOTE: do NOT call xsh->updateFrameCount() here. Audio moves do not
      // change the animatic length (determined by video shots). Calling it
      // would include the long trailing ColumnLevel extent after a razor cut,
      // making getFrameCount() huge and causing the ruler/cursor to jump right.
      TApp *app = TApp::instance();
      app->getCurrentScene()->notifyCastChange();
      updateCutFrames();
      updateTrackWidths();
    }, Qt::QueuedConnection);

    connect(at, &ZtoryAudioTrack::segmentDroppedOutside,
            this, &ZtoryAnimaticPanel::onSegmentDroppedOutside,
            Qt::QueuedConnection);

    // Lock / mute / solo signals
    connect(at, &ZtoryAudioTrack::lockedChanged,
            this, [this](int col, bool on){ m_colLocked[col] = on; });
    connect(at, &ZtoryAudioTrack::muteToggleRequested, this, [this, at](int col){
      // Persist so restoreTrackStates() can re-apply after refreshAudioTracks()
      m_colMuted[col] = at->isMuted();
      applyMuteSolo();
    });
    connect(at, &ZtoryAudioTrack::soloToggleRequested, this, [this, at](int col){
      // Persist so restoreTrackStates() can re-apply after refreshAudioTracks()
      if (at->isSolo()) m_colSolo.insert(col);
      else              m_colSolo.remove(col);
      applyMuteSolo();
    });
    connect(at, &ZtoryAudioTrack::deleteRequested, this, [this](int col){
      // Audio tracks always reference MAIN xsheet columns, but
      // ColumnCmd::deleteColumns() operates on the CURRENT xsheet. Inside a
      // shot the current xsheet is the sub-scene, so the delete would hit the
      // wrong column (or nothing). Require main level.
      TApp *app = TApp::instance();
      ToonzScene *scene = app->getCurrentScene()->getScene();
      if (!scene) return;
      if (scene->getChildStack()->getAncestorCount() != 0) {
        DVGui::warning(tr("Exit the shot (Back to Animatic) to delete an "
                          "audio track."));
        return;
      }
      std::set<int> cs; cs.insert(col);
      // onlyColumns=false (also clears cells), withoutUndo=false (native undo).
      ColumnCmd::deleteColumns(cs, false, false);
      app->getCurrentXsheet()->notifyXsheetChanged();
      app->getCurrentXsheet()->notifyXsheetSoundChanged();
      // Rebuild the audio strip immediately so the deleted track disappears
      // even if the deferred xsheetChanged refresh path is skipped.
      refreshAudioTracks();
      updateTrackWidths();
    }, Qt::QueuedConnection);  // Queued: we are inside the track widget's event

    // Inserisci prima dello stretch finale
    int insertIdx = m_scrollLay->count() - 1;
    if (insertIdx < 0) insertIdx = 0;
    m_scrollLay->insertWidget(insertIdx, at);
    m_audioTracks.append(at);
  }

  // Cross-connect: when one track clears its selection, clear all others.
  // Also clear audio selection when the video track receives a click.
  for (auto *src : m_audioTracks) {
    connect(src, &ZtoryAudioTrack::selectionCleared, this, [this, src]() {
      for (auto *other : m_audioTracks)
        if (other != src) other->clearSelection();
    });
    // Plain click on a segment selects it exclusively — clear the other tracks.
    connect(src, &ZtoryAudioTrack::exclusiveSelectRequested, this, [this, src]() {
      for (auto *other : m_audioTracks)
        if (other != src) other->clearSelection();
    });
    // Group move: a SegmentDrag on one track moves every other selected track's
    // segment by the same delta.  src is the leader; the siblings are armed,
    // previewed and committed here.
    connect(src, &ZtoryAudioTrack::groupDragStarted, this, [this, src]() {
      for (auto *other : m_audioTracks)
        if (other != src && other->hasSelection()) other->beginGroupDrag();
    });
    connect(src, &ZtoryAudioTrack::groupDragDelta, this, [this, src](int delta) {
      for (auto *other : m_audioTracks)
        if (other != src && other->hasSelection()) other->previewGroupMove(delta);
    });
    connect(src, &ZtoryAudioTrack::groupDragCommitted, this, [this, src](int delta) {
      for (auto *other : m_audioTracks)
        if (other != src && other->hasSelection()) other->commitGroupMove(delta);
      ZtoryAnimaticController::instance()->invalidateSoundTrack();
    });
  }

  updateCutFrames();
  restoreTrackStates();
  m_refreshingAudio = false;
}

void ZtoryAnimaticPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (m_switchEnabled && scene && scene->getChildStack()->getAncestorCount() > 0) {
    showShotTimeline();
  } else {
    showAnimaticTimeline();
    refreshFromScene();
    m_ruler->initPlayRangeIfNeeded();
  }
}

void ZtoryAnimaticPanel::showShotTimeline() {
  if (!m_timelinePanel) {
    m_timelinePanel = new TimelineViewerPanel(m_outerStack);
    m_timelinePanel->getTitleBar()->hide();
    m_timelinePanel->setEmbedded();
    m_timelinePanel->reset();
    m_outerStack->addWidget(m_timelinePanel);  // page 1
  }
  m_outerStack->setCurrentIndex(1);
}

void ZtoryAnimaticPanel::showAnimaticTimeline() {
  m_outerStack->setCurrentIndex(0);
}

// Helper: returns shot index in ZtoryModel for a given xsheet column, or -1.





// ── Cmd+C/X/V slot implementations ───────────────────────────────────────────
// All operations work directly on xsheet columns — no ZtoryModel::shot() lookup
// needed, which avoids stale xsheetColumn values.


void ZtoryAnimaticPanel::onCopyShots() {
  if (!ZtoryModel::assertMainXsheet(false)) return;
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->empty() && !shared.empty()) selPtr = &shared;
  const std::set<int> &sel = *selPtr;
  if (sel.empty()) return;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::vector<ZtoryClipEntry> clip;
  for (int col : sel) {
    ZtoryClipEntry ce;
    ce.srcCol = col; ce.duration = ZtoryShotOps::colDuration(xsh, col);
    ce.isCut = false; ce.isClone = false;
    clip.push_back(ce);
  }
  std::sort(clip.begin(), clip.end(),
            [](const ZtoryClipEntry &a, const ZtoryClipEntry &b){ return a.srcCol < b.srcCol; });
  ZtoryModel::instance()->setSharedClip(std::move(clip));
}

// Cut = immediate: shot disappears right away; TXshLevel kept alive in cutLevel
// so paste can restore sub-scene content without loss.
void ZtoryAnimaticPanel::onCutShots() {
  if (!ZtoryModel::assertMainXsheet(false)) return;
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->empty() && !shared.empty()) selPtr = &shared;
  const std::set<int> &sel = *selPtr;
  if (sel.empty()) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::vector<int> cols(sel.begin(), sel.end());
  std::sort(cols.begin(), cols.end());
  std::vector<ZtoryClipEntry> clip;
  for (int col : cols) {
    ZtoryClipEntry ce;
    ce.srcCol = -1; ce.duration = ZtoryShotOps::colDuration(xsh, col);
    ce.isCut = true; ce.isClone = false;
    TXshColumn      *xshCol = xsh->getColumn(col);
    TXshLevelColumn *lc     = xshCol ? xshCol->getLevelColumn() : nullptr;
    if (lc) {
      int r0=0, r1=0; lc->getRange(r0, r1);
      TXshCell cell = lc->getCell(r0);
      if (!cell.isEmpty()) ce.cutLevel = cell.m_level;
    }
    clip.push_back(ce);
  }
  ZtoryModel::instance()->setSharedClip(std::move(clip));
  for (int i = (int)cols.size()-1; i >= 0; i--) {
    std::set<int> cs; cs.insert(cols[i]);
    ColumnCmd::deleteColumns(cs, false, true);  // withoutUndo=true
  }
  xsh->updateFrameCount();
  ZtoryModel::instance()->resequenceXsheet();
  refreshFromScene();
  m_track->setFocus(Qt::OtherFocusReason);

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Cut Shot"), std::move(before), std::move(after)));
  }
}


void ZtoryAnimaticPanel::onPasteShots() {
  const auto &clip = ZtoryModel::instance()->sharedClip();
  if (clip.empty()) return;
  if (!ZtoryModel::assertMainXsheet(false)) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  const std::set<int> &sel = m_track->selectedCols();
  int insertCol = sel.empty() ? xsh->getColumnCount() : *sel.rbegin() + 1;
  ZtoryShotOps::pasteSharedClip(clip, insertCol, xsh, scene);
  xsh->updateFrameCount();
  ZtoryModel::instance()->resequenceXsheet();
  refreshFromScene();
  auto newClip = clip;
  newClip.erase(std::remove_if(newClip.begin(), newClip.end(),
                [](const ZtoryClipEntry &e){ return e.isCut || e.isClone; }),
                newClip.end());
  ZtoryModel::instance()->setSharedClip(std::move(newClip));
  m_track->setFocus(Qt::OtherFocusReason);

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Paste Shot"), std::move(before), std::move(after)));
  }
}

void ZtoryAnimaticPanel::onDeleteShots() {
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->empty() && !shared.empty()) selPtr = &shared;
  const std::set<int> &sel = *selPtr;
  if (sel.empty()) return;
  if (!ZtoryModel::assertMainXsheet(false)) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::vector<int> cols(sel.begin(), sel.end());
  std::sort(cols.rbegin(), cols.rend());
  for (int col : cols) {
    std::set<int> cs; cs.insert(col);
    ColumnCmd::deleteColumns(cs, false, true);  // withoutUndo=true
  }
  xsh->updateFrameCount();
  ZtoryModel::instance()->resequenceXsheet();
  refreshFromScene();
  m_track->setFocus(Qt::OtherFocusReason);

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Delete Shot"), std::move(before), std::move(after)));
  }
}

// Cmd+D: puts selected shots in clipboard as clones; Cmd+V inserts them.
void ZtoryAnimaticPanel::onCloneShots() {
  if (!ZtoryModel::assertMainXsheet(false)) return;
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->empty() && !shared.empty()) selPtr = &shared;
  const std::set<int> &sel = *selPtr;
  if (sel.empty()) return;
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  std::vector<ZtoryClipEntry> clip;
  for (int col : sel) {
    ZtoryClipEntry ce;
    ce.srcCol = col; ce.duration = ZtoryShotOps::colDuration(xsh, col);
    ce.isCut = false; ce.isClone = true;
    clip.push_back(ce);
  }
  std::sort(clip.begin(), clip.end(),
            [](const ZtoryClipEntry &a, const ZtoryClipEntry &b){ return a.srcCol < b.srcCol; });
  ZtoryModel::instance()->setSharedClip(std::move(clip));
}

bool ZtoryAnimaticPanel::eventFilter(QObject *obj, QEvent *e) {
  const QEvent::Type t = e->type();

  // ── Middle-mouse / Space+drag panning on ruler or track ──────────────────
  if (m_scroll && (obj == m_ruler || obj == m_track)) {
    if (t == QEvent::MouseButtonPress) {
      auto *me = static_cast<QMouseEvent *>(e);
      bool wantPan = me->button() == Qt::MiddleButton ||
                     (me->button() == Qt::LeftButton && m_spaceDown);
      if (wantPan) {
        m_panning         = true;
        m_panAnchorGlobal = me->globalPos();
        m_panAnchorScrollX = m_scroll->horizontalScrollBar()->value();
        static_cast<QWidget *>(obj)->setCursor(Qt::ClosedHandCursor);
        return true;
      }
    }
    if (t == QEvent::MouseMove && m_panning) {
      auto *me = static_cast<QMouseEvent *>(e);
      int dx = m_panAnchorGlobal.x() - me->globalPos().x();
      m_scroll->horizontalScrollBar()->setValue(qMax(0, m_panAnchorScrollX + dx));
      return true;
    }
    if (t == QEvent::MouseButtonRelease && m_panning) {
      auto *me = static_cast<QMouseEvent *>(e);
      if (me->button() == Qt::MiddleButton || me->button() == Qt::LeftButton) {
        m_panning = false;
        auto *w = static_cast<QWidget *>(obj);
        if (m_spaceDown) w->setCursor(Qt::OpenHandCursor);
        else             w->unsetCursor();
        return true;
      }
    }
  }

  // ── Key events ────────────────────────────────────────────────────────────
  if (t != QEvent::ShortcutOverride && t != QEvent::KeyPress &&
      t != QEvent::KeyRelease)
    return false;

  // Check focus is inside this panel's subtree
  QWidget *fw = QApplication::focusWidget();
  bool inPanel = false;
  for (QWidget *w = fw; w; w = w->parentWidget())
    if (w == this) { inPanel = true; break; }

  if (!inPanel) return false;

  // Audio tracks handle their own shortcuts (Delete, Ctrl+C/X/V) — don't intercept.
  if (qobject_cast<ZtoryAudioTrack *>(fw)) return false;

  QKeyEvent *ke = static_cast<QKeyEvent *>(e);

  // Space key: track pressed state for pan mode; do NOT consume so play still works.
  if (ke->key() == Qt::Key_Space && !ke->isAutoRepeat()) {
    if (t == QEvent::KeyPress && !m_spaceDown) {
      m_spaceDown = true;
      m_ruler->setCursor(Qt::OpenHandCursor);
      m_track->setCursor(Qt::OpenHandCursor);
    } else if (t == QEvent::KeyRelease) {
      m_spaceDown = false;
      m_panning   = false;
      m_ruler->unsetCursor();
      m_track->unsetCursor();
    }
    return false;
  }

  if (t == QEvent::KeyRelease) return false;

  const bool cmd   = ke->modifiers() & Qt::ControlModifier;
  const bool shift = ke->modifiers() & Qt::ShiftModifier;
  const bool noMod = ke->modifiers() == Qt::NoModifier;

  const bool isDelete = noMod && (ke->key() == Qt::Key_Delete ||
                                  ke->key() == Qt::Key_Backspace);
  const bool isCopy   = cmd && !shift && ke->key() == Qt::Key_C;
  const bool isCut    = cmd && !shift && ke->key() == Qt::Key_X;
  const bool isPaste  = cmd && !shift && ke->key() == Qt::Key_V;
  const bool isClone  = cmd && !shift && ke->key() == Qt::Key_D;

  if (!isDelete && !isCopy && !isCut && !isPaste && !isClone) return false;

  // Paste works even with empty selection (inserts at end).
  // All other operations require at least one shot selected.
  if (!isPaste && m_track->selectedCols().empty()) return false;

  // Phase 1: claim the key to prevent CommandManager from stealing it
  if (t == QEvent::ShortcutOverride) { ke->accept(); return true; }

  // Phase 2: dispatch the shot operation
  if (isCopy)        onCopyShots();
  else if (isCut)    onCutShots();
  else if (isPaste)  onPasteShots();
  else if (isClone)  onCloneShots();
  else if (isDelete) onDeleteShots();

  ke->accept();
  return true;
}

void ZtoryAnimaticPanel::keyPressEvent(QKeyEvent *e) {
  const Qt::KeyboardModifiers mod = e->modifiers();
  const bool cmd  = mod & Qt::ControlModifier;
  const bool shift = mod & Qt::ShiftModifier;

  // ── Cmd+C / Cmd+X / Cmd+V — delegated to named slots ────────────────────
  if (cmd && !shift && e->key() == Qt::Key_C) { onCopyShots(); e->accept(); return; }
  if (cmd && !shift && e->key() == Qt::Key_X) { onCutShots();  e->accept(); return; }
  if (cmd && !shift && e->key() == Qt::Key_V) { onPasteShots(); e->accept(); return; }

  // Cmd+D — duplicate (clone) selected shots
  if (cmd && !shift && e->key() == Qt::Key_D) { onCloneShots(); e->accept(); return; }

  // Delete / Backspace — delete selected shots
  if (!cmd && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
    onDeleteShots(); e->accept(); return;
  }

  // Cmd+N — add new shot after selection
  if (cmd && e->key() == Qt::Key_N) {
    onAddShot();
    e->accept();
    return;
  }

  // Ctrl+0 — Fit All: zoom to show the entire animatic
  if (cmd && e->key() == Qt::Key_0) {
    onFitAll();
    e->accept();
    return;
  }

  TPanel::keyPressEvent(e);
}

void ZtoryAnimaticPanel::keyReleaseEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
    m_spaceDown = false;
    m_panning   = false;
    m_ruler->unsetCursor();
    m_track->unsetCursor();
    e->accept();
    return;
  }
  TPanel::keyReleaseEvent(e);
}

void ZtoryAnimaticPanel::onShotClicked(int col) {
  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
}

void ZtoryAnimaticPanel::onShotDoubleClicked(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Close any open sub-scene first
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  app->getCurrentColumn()->setColumnIndex(col);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  // BUG-01 fix: shots 2+ start at row r0 > 0; getCell(0,col) returned an
  // empty cell for them, so MI_OpenChild never fired.
  int r0 = 0, r1 = 0;
  // ignoreLastStop=true: skip the trailing SFH so the sub-scene's play
  // range mark-out matches the shot's actual animatic length, not +1.
  column->getRange(r0, r1, /*ignoreLastStop=*/true);
  TXshCell cell = xsh->getCell(r0, col);
  if (cell.m_level && cell.m_level->getChildLevel()) {
    app->getCurrentFrame()->setFrame(r0);
    CommandManager::instance()->execute("MI_OpenChild");
    // Set the native play range to the full animatic duration of this shot.
    // Do NOT clamp to subXsh->getFrameCount(): in Ztoryc the sub-scene
    // typically has fewer drawings than the animatic slot (implicit hold
    // fills the rest), so clamping would set mark-out to the drawing count
    // instead of the shot's timing.
    //
    // Cross-dissolve: extend the mark-out by the extra dissolve frames (XD-out
    // tail / XD-in head) so the animator sees and works on them.  Derived from
    // the persisted XD note columns, so it survives reload.  Mark-in stays at 0.
    {
      int durInAnimatic = r1 - r0 + 1;
      TXsheet *subXsh = cell.m_level->getChildLevel()->getXsheet();
      int xdExtra = xdNoteHalfCount(subXsh, kXDOutName) +
                    xdNoteHalfCount(subXsh, kXDInName);
      int outF = durInAnimatic - 1 + xdExtra;
      if (outF >= 0)
        XsheetGUI::setPlayRange(0, outF, 1, false);
    }
    // Keep the animatic controller's frame at the shot's main-xsheet row
    // so the animatic viewer renders at the correct position.
    ZtoryAnimaticController::instance()->setCurrentFrame(r0);
    // Switch the viewer panel to shot view (ZtoryAnimaticViewerPanel listens).
    ZtoryModel::instance()->activateShotForViewing(col);
  }
}

void ZtoryAnimaticPanel::onReturnToMain() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  // returnToAnimaticMode() in the viewer panel will also close the sub-scene,
  // but since we already closed it above it becomes a no-op there.
  ZtoryModel::instance()->requestReturnToViewer();
}

void ZtoryAnimaticPanel::onShotMoved(int col, int newStartFrame) {
  // NOTE: this signal fires after resequenceXsheet() has already run, so
  // delta computation from getRange() would always be 0.  Audio shifting
  // on ripple is handled directly inside onShotDurationChanged().
  Q_UNUSED(col); Q_UNUSED(newStartFrame);
}

// Split the ColumnLevel covering |splitFrame| into two parts using offset
// trimming — no audio frames are lost at the cut boundary.
// findSegments() iterates ColumnLevels directly, so both halves are
// independently selectable and draggable after the split.
static void splitAudioColumn(TXsheet *xsh, int audioCol, int splitFrame) {
  TXshColumn *ac = xsh ? xsh->getColumn(audioCol) : nullptr;
  TXshSoundColumn *sc = ac ? ac->getSoundColumn() : nullptr;
  if (!sc) return;
  sc->splitLevelAtFrame(splitFrame);
}

// Helper: insert a TStageObject keyframe (without undo) on every column
// and the first camera of the given child xsheet at 0-based |frame|.
static void addRazorKeyframes(TXshChildLevel *cl, int frame) {
  if (!cl) return;
  TXsheet *childXsh = cl->getXsheet();
  if (!childXsh) return;
  TStageObjectTree *tree = childXsh->getStageObjectTree();
  if (!tree) return;
  int numCols = childXsh->getColumnCount();
  for (int c = 0; c < numCols; c++) {
    TStageObject *obj =
        tree->getStageObject(TStageObjectId::ColumnId(c), false);
    if (obj) obj->setKeyframeWithoutUndo(frame);
  }
  TStageObject *cam =
      tree->getStageObject(TStageObjectId::CameraId(0), false);
  if (cam) cam->setKeyframeWithoutUndo(frame);
}

// Helper: materialize "hold" cells in a child xsheet.
// In Tahoma2D, drawing cells are stored only at transition points; intermediate
// frames are "held" (empty in storage but rendered as the previous drawing).
// Before trimming/shifting we must make held frames explicit so that neither
// shot ends up with empty columns at the cut boundary.
// |duration|   = total number of frames that the child xsheet must cover.
// |fillToEnd|  = when true, fill implicit holds all the way to duration-1
//                (needed for split so shiftChildXsheetBy has real cells to
//                shift even if the last drawing is before the cut point).
//                When false (default, merge), stop at lastContent to avoid
//                spurious held frames in merged sub-scenes where columns have
//                different time ranges.
void materializeCells(TXshChildLevel *cl, int duration, bool fillToEnd = false) {
  if (!cl) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int nCols = xsh->getColumnCount();
  for (int c = 0; c < nCols; c++) {
    // Find the last row that actually has content in this column.
    int lastContent = -1;
    for (int r = std::min(duration, xsh->getFrameCount()) - 1; r >= 0; r--) {
      if (!xsh->getCell(r, c).isEmpty()) { lastContent = r; break; }
    }
    if (lastContent < 0) continue;  // column entirely empty up to duration

    // fillToEnd=true: fill holds all the way to duration-1 so that
    // shiftChildXsheetBy can find real cells even past lastContent.
    // fillToEnd=false (merge): stop at lastContent to avoid filling old
    // columns past their range in merged sub-scenes.
    int fillEnd = fillToEnd ? (duration - 1) : lastContent;

    TXshCell last;
    for (int r = 0; r <= fillEnd; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty()) {
        if (cell.getFrameId().isStopFrame()) {
          // SFH terminates the implicit hold chain — do NOT propagate it into
          // subsequent empty frames; reset the carry cell.
          last = TXshCell();
        } else {
          last = cell;
        }
      } else if (!last.isEmpty()) {
        // Held frame: write the last explicit cell so it becomes explicit.
        xsh->setCell(r, c, last);
      }
    }
  }
}

// Helper: trim a child xsheet to |keepFrames| frames.
// Removes cells >= keepFrames and removes stage-object keyframes >= keepFrames.
void trimChildXsheetTo(TXshChildLevel *cl, int keepFrames) {
  if (!cl) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int maxF  = xsh->getFrameCount();
  int nCols = xsh->getColumnCount();
  for (int c = 0; c < nCols; c++)
    for (int r = keepFrames; r < maxF; r++)
      xsh->clearCells(r, c);
  TStageObjectTree *tree = xsh->getStageObjectTree();
  if (tree) {
    auto trim = [&](TStageObject *obj) {
      if (!obj) return;
      TStageObject::KeyframeMap kfs;
      obj->getKeyframes(kfs);
      for (auto &kv : kfs)
        if (kv.first >= keepFrames)
          obj->removeKeyframeWithoutUndo(kv.first);
    };
    for (int c = 0; c < nCols; c++)
      trim(tree->getStageObject(TStageObjectId::ColumnId(c), false));
    trim(tree->getStageObject(TStageObjectId::CameraId(0), false));
  }
  xsh->updateFrameCount();
}

// Helper: shift a child xsheet left by |offset| frames.
// Cells at [offset .. end] become [0 .. end-offset].
// Stage-object keyframes are shifted by -offset; keyframes < 0 are removed.
static void shiftChildXsheetBy(TXshChildLevel *cl, int offset) {
  if (!cl || offset <= 0) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int maxF  = xsh->getFrameCount();
  int nCols = xsh->getColumnCount();
  int keep  = maxF - offset;
  if (keep <= 0) { /* nothing would remain */ return; }
  for (int c = 0; c < nCols; c++) {
    std::vector<TXshCell> buf(keep);
    for (int r = 0; r < keep; r++)
      buf[r] = (offset + r < maxF) ? xsh->getCell(offset + r, c) : TXshCell();
    for (int r = 0; r < maxF; r++) xsh->clearCells(r, c);
    for (int r = 0; r < keep; r++)
      if (!buf[r].isEmpty()) xsh->setCell(r, c, buf[r]);
  }
  TStageObjectTree *tree = xsh->getStageObjectTree();
  if (tree) {
    auto shift = [&](TStageObject *obj) {
      if (!obj) return;
      TStageObject::KeyframeMap kfs;
      obj->getKeyframes(kfs);
      // Remove all existing keyframes first (collect positions, then remove).
      for (auto &kv : kfs) obj->removeKeyframeWithoutUndo(kv.first);
      // Re-add with offset applied; discard those that fall before frame 0.
      for (auto &kv : kfs) {
        int newF = kv.first - offset;
        if (newF >= 0) obj->setKeyframeWithoutUndo(newF, kv.second);
      }
    };
    for (int c = 0; c < nCols; c++)
      shift(tree->getStageObject(TStageObjectId::ColumnId(c), false));
    shift(tree->getStageObject(TStageObjectId::CameraId(0), false));
  }
  xsh->updateFrameCount();
}

// Helper: merge srcCl's content into dstCl starting at |dstOffset|.
//
// Design rules (matching the cut behaviour):
//  - Each drawing column of srcCl is appended as a NEW column in dstCl so
//    that the two shots never share column indices (no cell/keyframe mixing).
//  - The camera track IS shared: srcCl's camera keyframes are copied (with
//    offset) into dstCl's existing camera object.
//  - Boundary keyframes are inserted at the junction to freeze animation:
//      dstOffset-1 : added BEFORE inserting new cols (only existing cols)
//      dstOffset   : added AFTER  inserting new cols (all cols incl. new)
//      dstOffset+srcDuration-1 : added AFTER (end of the new segment, needed
//                                for 3+-shot merges where it becomes a middle).
//  - srcCl's cells are materialized (held cells → explicit) before copying.
void mergeChildXsheetContent(TXshChildLevel *dstCl,
                                    TXshChildLevel *srcCl,
                                    int dstOffset, int srcDuration) {
  if (!dstCl || !srcCl) return;
  TXsheet *dstXsh = dstCl->getXsheet();
  TXsheet *srcXsh = srcCl->getXsheet();
  if (!dstXsh || !srcXsh) return;

  // Step 1: materialize srcCl's held cells for its full duration.
  materializeCells(srcCl, srcDuration);

  // Step 2: keyframe at end of previous content — BEFORE inserting new cols.
  // This pins only the already-existing dst columns at that boundary.
  addRazorKeyframes(dstCl, dstOffset - 1);

  // Step 3: append new columns in dstXsh for each drawing column of srcCl.
  int srcNCols    = srcXsh->getColumnCount();
  int dstColBase  = dstXsh->getColumnCount(); // first new column index
  for (int c = 0; c < srcNCols; c++)
    dstXsh->insertColumn(dstColBase + c);

  // Step 4: copy cells into the new columns at dstOffset.
  for (int c = 0; c < srcNCols; c++) {
    for (int r = 0; r < srcDuration; r++) {
      TXshCell cell = srcXsh->getCell(r, c);
      if (!cell.isEmpty())
        dstXsh->setCell(dstOffset + r, dstColBase + c, cell);
    }
  }

  // Step 5: copy stage-object keyframes for drawing columns (shifted).
  TStageObjectTree *srcTree = srcXsh->getStageObjectTree();
  TStageObjectTree *dstTree = dstXsh->getStageObjectTree();
  if (srcTree && dstTree) {
    for (int c = 0; c < srcNCols; c++) {
      TStageObject *srcObj =
          srcTree->getStageObject(TStageObjectId::ColumnId(c), false);
      TStageObject *dstObj =
          dstTree->getStageObject(TStageObjectId::ColumnId(dstColBase + c), false);
      if (!srcObj || !dstObj) continue;
      TStageObject::KeyframeMap kfs;
      srcObj->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstObj->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
    }
  }

  // Step 7a: boundary keyframe at start of new segment — pins all columns
  // (including the new drawing columns) at their current interpolated values.
  // Must run BEFORE copying the src camera keyframes: the single-arg
  // setKeyframeWithoutUndo() materialises the interpolated value at that frame,
  // so it would overwrite the correct second-shot camera value if called after.
  addRazorKeyframes(dstCl, dstOffset);

  // Step 6: merge camera keyframes (shared camera track).
  // Runs AFTER addRazorKeyframes so the correct srcCam value at dstOffset
  // overwrites the interpolated first-shot value written by step 7a.
  if (srcTree && dstTree) {
    TStageObject *srcCam =
        srcTree->getStageObject(TStageObjectId::CameraId(0), false);
    TStageObject *dstCam =
        dstTree->getStageObject(TStageObjectId::CameraId(0), false);
    if (srcCam && dstCam) {
      TStageObject::KeyframeMap kfs;
      srcCam->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstCam->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
      // The junction keyframe at dstOffset was set by addRazorKeyframes (step 7a)
      // to the first-shot's interpolated value. Override it with the second shot's
      // first camera keyframe value so the junction is clean.
      // If srcCam has no explicit keyframe at frame 0, we use its first keyframe;
      // if it has no keyframes at all, addRazorKeyframes already set the right hold.
      if (!kfs.empty()) {
        dstCam->setKeyframeWithoutUndo(dstOffset, kfs.begin()->second);
      }
    }
  }

  // Step 7b: boundary keyframe at end of new segment.
  addRazorKeyframes(dstCl, dstOffset + srcDuration - 1);

  dstXsh->updateFrameCount();
}

void ZtoryAnimaticPanel::onMergeShots() {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  // Use own selection if >= 2; otherwise fall back to shared selection
  // (set by Board when the user selected shots there).
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->size() < 2 && shared.size() >= 2) selPtr = &shared;
  const std::set<int> &sel = *selPtr;
  if (sel.size() < 2) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Sort selected cols by their start frame in main xsheet
  std::vector<int> sortedCols(sel.begin(), sel.end());
  std::sort(sortedCols.begin(), sortedCols.end(), [&](int a, int b){
    int r0a = 0, r1a = 0, r0b = 0, r1b = 0;
    if (xsh->getColumn(a)) xsh->getColumn(a)->getRange(r0a, r1a);
    if (xsh->getColumn(b)) xsh->getColumn(b)->getRange(r0b, r1b);
    return r0a < r0b;
  });

  int dstCol = sortedCols[0];
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  // ignoreLastStop=true: skip trailing SFH so dstDuration and appendAt are exact.
  // appendAt = dstR1+1 = SFH position → new cells overwrite it cleanly.
  dstColumn->getRange(dstR0, dstR1, /*ignoreLastStop=*/true);

  // Find child level of destination
  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, dstCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  int appendAt    = dstR1 + 1;
  int dstDuration = dstR1 - dstR0 + 1;
  int lastFrameNum = dstDuration; // 1-based frame index for continuation

  // Materialize held cells in the first shot before merging, then trim to the
  // timeline duration. Without the trim, any frames in the sub-scene beyond
  // dstDuration (hidden from the main xsheet) would overlap with the incoming
  // src content that is inserted starting at row dstDuration.
  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    // ignoreLastStop=true: exclude the trailing SFH from the merged shot's
    // duration so the merge keeps the source's real frame count.
    srcColumn->getRange(r0, r1, /*ignoreLastStop=*/true);
    int duration = r1 - r0 + 1;

    // Find src child level
    TXshChildLevel *srcCl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, srcCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        srcCl = cell.m_level->getChildLevel();
        break;
      }
    }
    // Merge srcCl into dstCl: new columns per shot, shared camera,
    // boundary keyframes at junction and end of segment.
    mergeChildXsheetContent(dstCl, srcCl, lastFrameNum, duration);

    // Extend main xsheet column to cover the merged duration.
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  for (int i = (int)sortedCols.size() - 1; i >= 1; i--) {
    std::set<int> cs; cs.insert(sortedCols[i]);
    ColumnCmd::deleteColumns(cs, false, true);  // withoutUndo=true
  }

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Merge Shots"), std::move(before), std::move(after)));
  }
}

// ── Board sync contract ───────────────────────────────────────────────────────
// REGOLA: NON emettere shotAdded/shotRemovedAt dopo resequenceXsheet() in
// nessuna delle funzioni qui sotto. Il Board si sincronizza esclusivamente via:
//   resequenceXsheet() → emit modelReset()
//   → StoryboardPanel::onModelResequenced() → refreshFromScene() (se count ≠)
// Emettere shotAdded/shotRemovedAt DOPO causa double-update:
// onModelResequenced() ha già ricostruito il Board da xsheet ground-truth,
// poi il segnale ridondante inserisce/rimuove un shot in più → Board errato.
// ─────────────────────────────────────────────────────────────────────────────

// ---- Add Shot ----
void ZtoryAnimaticPanel::onAddShot() {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/false)) {
    // Inside a sub-scene: instead of refusing with a warning, behave like the
    // global New Shot After Current — add after the shot being edited and
    // re-enter it so the artist keeps drawing.
    if (StoryboardPanel *board = findBoardPanel())
      board->newShotAfterCurrent();
    return;
  }

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  // Insert after the rightmost selected shot, or append at the end
  int insertAt = xsh->getColumnCount();
  const std::set<int> *selPtr = &m_track->selectedCols();
  const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
  if (selPtr->empty() && !shared.empty()) selPtr = &shared;
  if (!selPtr->empty())
    insertAt = *std::max_element(selPtr->begin(), selPtr->end()) + 1;

  static const int kDefaultDuration = 24;

  // Create a new sub-scene (child level)
  TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
  if (!xl || !xl->getChildLevel()) return;
  TXshChildLevel *cl = xl->getChildLevel();

  xsh->insertColumn(insertAt);
  for (int r = 0; r < kDefaultDuration; r++)
    xsh->setCell(r, insertAt, TXshCell(cl, TFrameId(r + 1)));
  xsh->updateFrameCount();

  // Copy camera resolution/size from parent to sub-scene
  ZtoryShotOps::syncChildCameraToMain(xsh, cl);

  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();
  // Board syncs via resequenceXsheet() → modelReset() → onModelResequenced()
  // (xsheet count check). No shotAdded() needed — it would cause double-insert.

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Add Shot"), std::move(before), std::move(after)));
  }
}

void ZtoryAnimaticPanel::onMergeWithNext(int col) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Find the next non-empty column after 'col'
  TXshColumn *dstColumn = xsh->getColumn(col);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  // ignoreLastStop=true: skip trailing SFH so dstDuration and appendAt are exact.
  dstColumn->getRange(dstR0, dstR1, /*ignoreLastStop=*/true);

  // Find next shot column
  int nextCol = -1;
  for (int c = col + 1; c < xsh->getColumnCount(); c++) {
    TXshColumn *nc = xsh->getColumn(c);
    if (!nc || nc->isEmpty()) continue;
    // Check it's a child level (shot), not audio
    int nr0 = 0, nr1 = 0;
    nc->getRange(nr0, nr1);
    for (int r = nr0; r <= nr1; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        nextCol = c;
        break;
      }
    }
    if (nextCol >= 0) break;
  }
  if (nextCol < 0) return;

  // Find child level of destination
  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  // Find child level of source (next) shot
  TXshChildLevel *srcCl = nullptr;
  TXshColumn *srcColumn = xsh->getColumn(nextCol);
  int srcR0 = 0, srcR1 = 0;
  // ignoreLastStop=true: skip trailing SFH so srcDuration is the real frame count.
  srcColumn->getRange(srcR0, srcR1, /*ignoreLastStop=*/true);
  for (int r = srcR0; r <= srcR1; r++) {
    TXshCell cell = xsh->getCell(r, nextCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      srcCl = cell.m_level->getChildLevel();
      break;
    }
  }

  int dstDuration = dstR1 - dstR0 + 1;
  int srcDuration = srcR1 - srcR0 + 1;

  // Materialize held cells then trim to timeline duration — same reasoning as
  // onMergeShots: hidden frames beyond dstDuration must be removed before
  // appending src content, or they overlap with the incoming material.
  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  // Merge srcCl into dstCl: new columns per shot, shared camera,
  // boundary keyframes at junction and end of segment.
  mergeChildXsheetContent(dstCl, srcCl, dstDuration, srcDuration);

  // Extend dst column in main xsheet to cover the merged duration.
  int appendAt     = dstR1 + 1;
  int lastFrameNum = dstDuration;
  int duration     = srcDuration;
  for (int r = 0; r < duration; r++)
    xsh->setCell(appendAt + r, col, TXshCell(dstCl, TFrameId(++lastFrameNum)));

  { std::set<int> cs; cs.insert(nextCol); ColumnCmd::deleteColumns(cs, false, true); }

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Merge with Next"), std::move(before), std::move(after)));
  }
}

// Helper: log column range to debug file

void ZtoryAnimaticPanel::onRazorRequested(int col, int splitFrame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;

  // A razor cut does not move any absolute frame position (shot 1 keeps its
  // frames, shot 2 takes the tail in a new column at the same frames), so the
  // view should stay put.  notifyXsheetChanged() schedules a cascade of deferred
  // refreshFromScene() calls; set m_restoreScrollX so each of them re-pins the
  // scroll, then clear it once the cascade has drained.
  if (m_scroll) m_restoreScrollX = m_scroll->horizontalScrollBar()->value();

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *srcColumn = mainXsh->getColumn(col);
  if (!srcColumn) return;
  int r0 = 0, r1 = 0;
  // ignoreLastStop=true: exclude the trailing SFH placed by resequenceXsheet()
  // so that totalDuration and secondHalf are the real frame counts.
  srcColumn->getRange(r0, r1, /*ignoreLastStop=*/true);
  // splitRel = # frames that stay in the original shot (rows r0..splitFrame-1)
  int splitRel = splitFrame - r0;
  if (splitRel <= 0 || splitRel >= r1 - r0 + 1) return;

  int totalDuration = r1 - r0 + 1;
  int secondHalf    = totalDuration - splitRel;  // frames for the new shot

  // Grab original child level before cloning.
  TXshChildLevel *origCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      origCL = cell.m_level->getChildLevel();
      break;
    }
  }

  // ── Step 1: materialize held cells for the full shot duration ──────────────
  // Tahoma2D stores drawing cells only at transition points; intermediate
  // frames are empty in storage ("held"). We must make them explicit so that
  // after trim/shift neither shot has gaps at the cut boundary.
  // fillToEnd=true: fill holds all the way to totalDuration-1 so that
  // shiftChildXsheetBy can find real cells even when splitRel > lastContent.
  materializeCells(origCL, totalDuration, /*fillToEnd=*/true);

  // ── Step 2: bake boundary keyframes BEFORE cloning ────────────────────────
  // Adding keyframes to origCL now means the clone inherits them automatically.
  // splitRel-1 = last frame of shot 1; splitRel = first frame of shot 2.
  addRazorKeyframes(origCL, splitRel - 1);
  addRazorKeyframes(origCL, splitRel);

  // ── Step 3: Clone ──────────────────────────────────────────────────────────
  ColumnCmd::cloneChild(col);
  TUndoManager::manager()->popUndo(1);  // strip CloneChildUndo: our UndoBoardState covers this
  int newCol = col + 1;

  // Grab clone's child level.
  TXshChildLevel *cloneCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, newCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cloneCL = cell.m_level->getChildLevel();
      break;
    }
  }

  // ── Step 4: Trim original child xsheet to splitRel frames ─────────────────
  // Also removes stage-object keyframes beyond the cut point.
  trimChildXsheetTo(origCL, splitRel);

  // ── Step 4b: Place SFH at frame splitRel in Shot 1's sub-scene columns ─────
  // After materialization + trim, every frame 0..splitRel-1 that had content
  // is a real cell. Placing STOP_FRAME at splitRel prevents the last drawing
  // from bleeding as an implicit hold if the sub-scene is opened for editing.
  // onMatchSubsceneDuration correctly skips it via ignoreLastStop=true.
  {
    TXsheet *origXsh = origCL->getXsheet();
    int origNCols = origXsh->getColumnCount();
    for (int c = 0; c < origNCols; c++) {
      TXshColumn *oc = origXsh->getColumn(c);
      if (!oc || oc->isEmpty()) continue;
      int cr0 = 0, cr1 = 0;
      oc->getRange(cr0, cr1);
      if (cr1 < 0) continue;
      // Find any real drawing cell to get the level pointer for the SFH.
      TXshLevelP sfhLevel;
      for (int r = cr0; r <= cr1; r++) {
        TXshCell cell = origXsh->getCell(r, c);
        if (!cell.isEmpty() && !cell.getFrameId().isStopFrame() && cell.m_level) {
          sfhLevel = cell.m_level;
          break;
        }
      }
      if (sfhLevel)
        origXsh->setCell(splitRel, c,
                         TXshCell(sfhLevel, TFrameId(TFrameId::STOP_FRAME)));
    }
    origXsh->updateFrameCount();
  }

  // ── Step 5: Shift clone child xsheet left by splitRel ─────────────────────
  // Cells and keyframes at [splitRel..end] become [0..secondHalf-1].
  shiftChildXsheetBy(cloneCL, splitRel);

  // ── Step 6: Fix main-xsheet columns ───────────────────────────────────────
  // Trim original column: remove the tail (secondHalf cells).
  mainXsh->removeCells(r0 + splitRel, col, secondHalf);

  // Rebuild clone column: wipe then repopulate with TFrameId(1..secondHalf).
  mainXsh->clearCells(r0, newCol, totalDuration + 2);
  if (cloneCL) {
    for (int r = 0; r < secondHalf; r++)
      mainXsh->setCell(r0 + r, newCol, TXshCell(cloneCL, TFrameId(r + 1)));
  }

  mainXsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();

  // Linked razor: also split audio tracks at the same absolute frame.
  // cloneChild(col) inserted a new column at newCol=col+1, shifting every
  // column that was at index >= newCol by +1.  m_audioTracks still carry
  // pre-insert indices, so we must correct them before accessing the xsheet.
  struct AudioRazorEntry { int oldIdx; int newIdx; TXshSoundColumn *before = nullptr; };
  QList<AudioRazorEntry> audioEntries;
  if (m_audioLinked) {
    for (auto *at : m_audioTracks) {
      int oldIdx = at->columnIndex();
      int newIdx = (oldIdx >= newCol) ? oldIdx + 1 : oldIdx;
      TXshColumn *c = mainXsh->getColumn(newIdx);
      TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
      if (!sc) continue;  // shouldn't happen, but skip non-sound columns
      AudioRazorEntry e;
      e.oldIdx = oldIdx;
      e.newIdx = newIdx;
      e.before = dynamic_cast<TXshSoundColumn *>(sc->clone());
      audioEntries.append(e);
    }
    for (auto &e : audioEntries) splitAudioColumn(mainXsh, e.newIdx, splitFrame);
  }

  m_track->refreshFromScene();
  refreshAudioTracks();

  // Group board state + audio edits into a single undoable step.
  {
    TUndoScopedBlock undoBlock;
    if (board) {
      auto after = board->captureSnapshot();
      TUndoManager::manager()->add(
          new UndoBoardState(board, tr("Razor"), std::move(before), std::move(after)));
    }
    for (auto &e : audioEntries) {
      TXshColumn *c = mainXsh->getColumn(e.newIdx);
      TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
      TXshSoundColumn *aft = sc ? dynamic_cast<TXshSoundColumn *>(sc->clone()) : nullptr;
      TUndoManager::manager()->add(
          new UndoAudioEdit(e.newIdx, e.before, aft, tr("Razor Audio")));
    }
  }

  // Pin the scroll now (synchronous refreshes above) and again after the whole
  // deferred-refresh cascade drains, then stop pinning so normal scrolling works.
  if (m_restoreScrollX >= 0 && m_scroll)
    m_scroll->horizontalScrollBar()->setValue(m_restoreScrollX);
  QTimer::singleShot(0, this, [this]() { m_restoreScrollX = -1; });
}

void ZtoryAnimaticPanel::onAudioRazorRequested(int col, int frame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/false)) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  TXshColumn *c = xsh->getColumn(col);
  TXshSoundColumn *sc = c ? c->getSoundColumn() : nullptr;
  if (!sc) return;
  // Pin the scroll across the deferred refresh cascade (notifyXsheetChanged
  // schedules refreshFromScene) so the view stays anchored on the cut point.
  if (m_scroll) m_restoreScrollX = m_scroll->horizontalScrollBar()->value();
  TXshSoundColumn *before = dynamic_cast<TXshSoundColumn *>(sc->clone());
  splitAudioColumn(xsh, col, frame);
  TXshSoundColumn *after = dynamic_cast<TXshSoundColumn *>(sc->clone());
  TUndoManager::manager()->add(
      new UndoAudioEdit(col, before, after, tr("Razor Audio")));
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  refreshAudioTracks();
  updateTrackWidths();
  if (m_restoreScrollX >= 0 && m_scroll)
    m_scroll->horizontalScrollBar()->setValue(m_restoreScrollX);
  QTimer::singleShot(0, this, [this]() { m_restoreScrollX = -1; });
}

void ZtoryAnimaticPanel::onSegmentDroppedOutside(int srcCol, int origR0,
                                                  int origR1, int dragOffset,
                                                  QPoint globalPos) {
  // Find the target audio track under the drop position
  ZtoryAudioTrack *target = nullptr;
  for (auto *at : m_audioTracks) {
    if (at->columnIndex() == srcCol) continue;
    QRect r = QRect(at->mapToGlobal(QPoint(0, 0)), at->size());
    if (r.contains(globalPos)) { target = at; break; }
  }
  if (!target) return;

  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;

  // Detach from source
  TXshColumn *srcColumn = xsh->getColumn(srcCol);
  TXshSoundColumn *srcSc = srcColumn ? srcColumn->getSoundColumn() : nullptr;
  if (!srcSc) return;
  int dstCol = target->columnIndex();
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  TXshSoundColumn *dstSc = dstColumn ? dstColumn->getSoundColumn() : nullptr;
  if (!dstSc) return;

  // Snapshot both columns before the move.
  TXshSoundColumn *srcBefore = dynamic_cast<TXshSoundColumn *>(srcSc->clone());
  TXshSoundColumn *dstBefore = dynamic_cast<TXshSoundColumn *>(dstSc->clone());

  int midFrame = (origR0 + origR1) / 2;
  ColumnLevel *cl = srcSc->detachLevelByFrame(midFrame);
  if (!cl) { delete srcBefore; delete dstBefore; return; }

  // Compute target vsf: frame under cursor minus the grab offset inside the segment
  QPoint localPos = target->mapFromGlobal(globalPos);
  int cursorFrame = target->frameAtX(localPos.x());
  int targetVsf = qMax(0, cursorFrame - dragOffset);

  // Clamp against existing segments on destination track to avoid overlap.
  int segLen = origR1 - origR0;
  auto dstSegs = target->findSegments();
  for (auto &s : dstSegs) {
    if (s.r1 < targetVsf && targetVsf <= s.r1 + 1)
      targetVsf = s.r1 + 1;
    if (s.r0 >= targetVsf && targetVsf + segLen >= s.r0)
      targetVsf = s.r0 - segLen - 1;
  }
  if (targetVsf < 0) targetVsf = 0;

  // Adopt into target track
  dstSc->adoptLevel(cl, targetVsf);

  // Push undo for both columns as a single grouped operation.
  {
    TUndoScopedBlock undoBlock;
    TXshSoundColumn *srcAfter = dynamic_cast<TXshSoundColumn *>(srcSc->clone());
    TXshSoundColumn *dstAfter = dynamic_cast<TXshSoundColumn *>(dstSc->clone());
    TUndoManager::manager()->add(
        new UndoAudioEdit(srcCol, srcBefore, srcAfter, tr("Move Audio")));
    TUndoManager::manager()->add(
        new UndoAudioEdit(dstCol, dstBefore, dstAfter, tr("Move Audio")));
  }

  // Refresh — invalidate waveform via a queued call so it fires AFTER
  // all pending Qt paint events from refreshAudioTracks() have settled.
  ZtoryAnimaticController::instance()->invalidateSoundTrack();
  xsh->updateFrameCount();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  refreshAudioTracks();
  updateTrackWidths();
  // Force a second waveform rebuild in the next event-loop iteration:
  // the first paint of the new widgets may happen before Qt finishes
  // processing all pending column-data updates.
  QTimer::singleShot(0, this, [this]() {
    for (auto *at : m_audioTracks) {
      at->invalidateWaveform();
      at->update();
    }
  });
}

// ── onRollEdit ────────────────────────────────────────────────────────────────
// Resize colA and colB symmetrically so the seam moves without changing total
// animatic duration.  resequenceXsheet() re-packs the columns afterwards.
void ZtoryAnimaticPanel::onRollEdit(int colA, int newDurA, int colB, int newDurB) {
  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Helper lambda: resize column to newDur frames
  auto resizeCol = [&](int col, int newDur) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) return;
    int r0 = 0, r1 = 0;
    // ignoreLastStop=true: skip the trailing SFH so curDur is the shot's
    // actual animatic length.  Without this, rolling edit would think the
    // shot is +1 frame longer than it really is and miscompute the delta.
    column->getRange(r0, r1, /*ignoreLastStop=*/true);
    int curDur = r1 - r0 + 1;
    TXshCell typeCell;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        typeCell = cell; break;
      }
    }
    if (typeCell.isEmpty()) return;
    if (newDur > curDur) {
      int lastFrameNum = typeCell.m_frameId.getNumber();
      for (int r = r0; r <= r1; r++) {
        TXshCell c = xsh->getCell(r, col);
        if (!c.isEmpty()) lastFrameNum = c.m_frameId.getNumber();
      }
      for (int r = r0 + curDur; r < r0 + newDur; r++) {
        TXshCell c = typeCell;
        c.m_frameId = TFrameId(++lastFrameNum);
        xsh->setCell(r, col, c);
      }
    } else if (newDur < curDur) {
      xsh->removeCells(r0 + newDur, col, curDur - newDur);
    }
  };

  resizeCol(colA, newDurA);
  resizeCol(colB, newDurB);

  resequenceXsheet();
  ztorySetShotRange(colA, 0, newDurA - 1);
  ztorySetShotRange(colB, 0, newDurB - 1);

  // Live-update the play range if one of the two shots is currently open
  // in the sub-scene editor (SHOTEDITOR room).  resequenceXsheet() has
  // already repositioned the cells, so we re-read r0/r1 post-resequence.
  {
    ChildStack *cs = scene->getChildStack();
    if (cs && cs->getAncestorCount() == 1) {
      TXsheet *curXsh = cs->getXsheet();
      auto tryUpdate = [&](int col, int newDur) {
        TXshColumn *c = xsh->getColumn(col);
        if (!c) return;
        int r0 = 0, r1 = 0;
        c->getRange(r0, r1);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, col);
          if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
            TXsheet *shotXsh = cell.m_level->getChildLevel()->getXsheet();
            if (shotXsh && shotXsh == curXsh)
              XsheetGUI::setPlayRange(0, newDur - 1, 1, false);
            break;
          }
        }
      };
      tryUpdate(colA, newDurA);
      tryUpdate(colB, newDurB);
    }
  }

  m_track->refreshFromScene();

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Roll Edit"),
                           std::move(before), std::move(after)));
  }
}


// ── helpers for XD note columns ──────────────────────────────────────────────
// A SoundText column named kXDOutName / kXDInName is inserted in the sub-scene
// to mark the extra cross-dissolve frames (tail of A / head of B).  These names
// are declared near the top of the file (kXDOutName/kXDInName).  The column is
// removed/recreated on every transition change so its length always matches the
// current T/2 value, and read back via xdNoteHalfCount() to reconstruct the
// transition after reload.

static void removeXDNoteCol(TXsheet *subXsh, const std::string &name) {
  for (int c = subXsh->getColumnCount() - 1; c >= 0; c--) {
    TXshColumn *col = subXsh->getColumn(c);
    if (!col || !col->getSoundTextColumn()) continue;
    std::string colName =
        subXsh->getStageObject(subXsh->getColumnObjectId(c))->getName();
    if (colName == name) {
      subXsh->removeColumn(c);
      return;
    }
  }
}

static void addXDNoteCol(TXsheet *subXsh, int startRow, int count,
                         const std::string &name) {
  if (count <= 0) return;
  TXshSoundTextColumn *col = new TXshSoundTextColumn();
  col->setXsheet(subXsh);
  QList<QString> textList;
  textList << QString("[ %1 %2f ]")
                  .arg(QString::fromStdString(name))
                  .arg(count);
  for (int i = 1; i < count; i++) textList << "";
  col->createSoundTextLevel(startRow, textList);
  // Append at the end so existing column indices are never shifted.
  int insertIdx = subXsh->getColumnCount();
  subXsh->insertColumn(insertIdx, col);
  subXsh->getStageObject(subXsh->getColumnObjectId(insertIdx))->setName(name);
}

// ── onTransitionChanged ──────────────────────────────────────────────────────
// Sets the cross-dissolve duration on the seam between colA (outgoing) and
// colB (incoming).  T/2 hold frames are added/removed at the tail of subA and
// at the head of subB.  For subB, the main-xsheet cell frame IDs are shifted
// by +actualDelta so the animatic continues to show the original content.
// Mark out of A and B is extended to cover the extra frames so the animator
// can work on the dissolve content.  A SoundText column (XD-out / XD-in) marks
// the extra frames visually in SHOTEDITOR.
void ZtoryAnimaticPanel::onTransitionChanged(int colA, int colB, int frames) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Helper: find the TXsheet of a child-level column in mainXsh
  auto getSubXsh = [&](int col) -> TXsheet * {
    TXshColumn *c = xsh->getColumn(col);
    if (!c || c->isEmpty()) return nullptr;
    int r0 = 0, r1 = 0;
    c->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
        return cell.m_level->getChildLevel()->getXsheet();
    }
    return nullptr;
  };

  // Helper: last occupied row in a sub-scene (skips audio and note columns)
  auto subLastRow = [](TXsheet *sub) -> int {
    int last = -1;
    for (int c = 0; c < sub->getColumnCount(); c++) {
      TXshColumn *sc = sub->getColumn(c);
      if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
      int sr0 = 0, sr1 = 0;
      sc->getRange(sr0, sr1);
      last = qMax(last, sr1);
    }
    return last;
  };

  TXsheet *subA = getSubXsh(colA);
  TXsheet *subB = (colB >= 0) ? getSubXsh(colB) : nullptr;

  // Previous transition length comes from the XD-out note column (source of
  // truth), NOT from the model which may be stale after reload.
  int prevHalf = xdNoteHalfCount(subA, kXDOutName);
  int newHalf  = frames / 2;
  int delta    = newHalf - prevHalf; // >0 add, <0 remove

  // Keep the model mirror in sync for any other consumer.
  ZtoryModel *model = ZtoryModel::instance();
  for (int si = 0; si < model->shotCount(); si++) {
    if (model->shot(si).xsheetColumn == colA) {
      model->shot(si).transitionFrames = frames;
      break;
    }
  }

  if (delta == 0) {
    StoryboardPanel *board = findBoardPanel();
    if (board) board->saveZtoryc();
    m_track->refreshFromScene();
    return;
  }

  // ── Tail of A: extend / shrink hold at end of sub-scene ──────────────────
  if (subA) {
    int lastRow = subLastRow(subA);
    if (lastRow >= 0) {
      if (delta > 0) {
        for (int c = 0; c < subA->getColumnCount(); c++) {
          TXshColumn *sc = subA->getColumn(c);
          if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
          TXshCell lastCell;
          for (int r = lastRow; r >= 0; r--) {
            TXshCell cell = subA->getCell(r, c);
            if (!cell.isEmpty()) { lastCell = cell; break; }
          }
          if (lastCell.isEmpty()) continue;
          for (int i = 1; i <= delta; i++)
            subA->setCell(lastRow + i, c, lastCell);
        }
      } else {
        int toRemove = -delta;
        for (int c = 0; c < subA->getColumnCount(); c++) {
          TXshColumn *sc = subA->getColumn(c);
          if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
          int sr0 = 0, sr1 = 0;
          sc->getRange(sr0, sr1);
          int canRemove = qMin(toRemove, sr1 - sr0); // keep at least 1 cell
          if (canRemove > 0)
            subA->removeCells(sr1 - canRemove + 1, c, canRemove);
        }
      }
      // Mark out: extend to cover extra frames so animator can work on dissolve
      int newLastA = subLastRow(subA);
      if (newLastA >= 0) ztorySetShotRange(colA, 0, newLastA);
    }
  }

  // ── Head of B: insert / remove hold at start of sub-scene ────────────────
  int actualBDelta = 0; // actual shift applied (may be clamped per-column)
  if (subB) {
    if (delta > 0) {
      actualBDelta = delta;
      for (int c = 0; c < subB->getColumnCount(); c++) {
        TXshColumn *sc = subB->getColumn(c);
        if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
        subB->insertCells(0, c, delta);
        TXshCell firstCell = subB->getCell(delta, c);
        if (firstCell.isEmpty()) continue;
        for (int r = 0; r < delta; r++)
          subB->setCell(r, c, firstCell);
      }
    } else {
      int toRemove = -delta;
      // Clamp to min available across all columns so the shift is uniform
      int minAvail = toRemove;
      for (int c = 0; c < subB->getColumnCount(); c++) {
        TXshColumn *sc = subB->getColumn(c);
        if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
        int sr0 = 0, sr1 = 0;
        sc->getRange(sr0, sr1);
        minAvail = qMin(minAvail, sr1 - sr0); // keep at least 1 cell
      }
      actualBDelta = -minAvail;
      if (minAvail > 0) {
        for (int c = 0; c < subB->getColumnCount(); c++) {
          TXshColumn *sc = subB->getColumn(c);
          if (!sc || sc->isEmpty() || sc->getSoundColumn() || sc->getSoundTextColumn()) continue;
          int sr0 = 0, sr1 = 0;
          sc->getRange(sr0, sr1);
          subB->removeCells(sr0, c, minAvail);
        }
      }
    }
    // Shift main-xsheet frame IDs for colB by actualBDelta
    if (actualBDelta != 0) {
      TXshColumn *mainColB = xsh->getColumn(colB);
      if (mainColB && !mainColB->isEmpty()) {
        int r0 = 0, r1 = 0;
        mainColB->getRange(r0, r1, /*ignoreLastStop=*/true);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, colB);
          if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
            int newId = cell.m_frameId.getNumber() + actualBDelta;
            if (newId < 1) newId = 1;
            cell.m_frameId = TFrameId(newId);
            xsh->setCell(r, colB, cell);
          }
        }
      }
    }
    // Mark out: extend to cover extra head frames (mark in stays at 0)
    int newLastB = subLastRow(subB);
    if (newLastB >= 0) ztorySetShotRange(colB, 0, newLastB);
  }

  // ── XD note columns: remove old, re-add if T > 0 ─────────────────────────
  if (subA) {
    removeXDNoteCol(subA, kXDOutName);
    if (newHalf > 0) {
      int lastA = subLastRow(subA);
      if (lastA >= 0)
        addXDNoteCol(subA, lastA - newHalf + 1, newHalf, kXDOutName);
    }
  }
  if (subB) {
    removeXDNoteCol(subB, kXDInName);
    if (newHalf > 0)
      addXDNoteCol(subB, 0, newHalf, kXDInName);
  }

  // Live-update the play range if colA or colB is currently open in SHOTEDITOR,
  // so changing/clearing the transition immediately moves the mark-out without
  // needing to reopen the shot.  Mark-out = last drawing row (extra frames
  // included); mark-in stays at 0.
  {
    ChildStack *cs = scene->getChildStack();
    if (cs && cs->getAncestorCount() == 1) {
      TXsheet *curXsh = cs->getXsheet();
      auto tryUpdate = [&](TXsheet *sub) {
        if (sub && sub == curXsh) {
          int last = subLastRow(sub);
          if (last >= 0) XsheetGUI::setPlayRange(0, last, 1, false);
        }
      };
      tryUpdate(subA);
      tryUpdate(subB);
    }
  }

  StoryboardPanel *board = findBoardPanel();
  if (board) board->saveZtoryc();
  m_track->refreshFromScene();
  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAnimaticPanel::onShotDurationChanged(int col, int newF1) {
  // Belt-and-suspenders: if newF1 somehow arrives < 0 the duration becomes
  // 0 or negative and removeCells() would wipe the entire column — i.e.
  // delete the shot's cells from the main xsheet.  The drag math already
  // clamps to >= 1 frame but a defensive check here makes that impossible
  // to bypass via any other code path or future regression.  A user reported
  // "adjusted scene length and everything disappeared" on Windows.
  if (newF1 < 0) return;

  StoryboardPanel *board = findBoardPanel();
  std::vector<ZtoryShotSnap> before;
  if (board) before = board->captureSnapshot();

  int newDuration = newF1 + 1;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  int r0 = 0, r1 = 0;
  // ignoreLastStop=true: skip the trailing SFH so currentDuration is the
  // shot's actual length, not +1.  Otherwise the trim math would compare
  // newDuration against an inflated count and add/remove the wrong number
  // of cells (off-by-one shrinking, missing growth).
  column->getRange(r0, r1, /*ignoreLastStop=*/true);
  int currentDuration = r1 - r0 + 1;

  // Trova tipo cella
  TXshCell typeCell;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      typeCell = cell;
      break;
    }
  }
  if (typeCell.isEmpty()) return;

  if (newDuration > currentDuration) {
    // Trova l'ultimo frame index usato
    int lastFrameNum = currentDuration; // frame index dell'ultima cella (0-based -> 1-based)
    for (int r = r1; r >= r0; r--) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty()) {
        lastFrameNum = cell.m_frameId.getNumber();
        break;
      }
    }
    for (int r = r0 + currentDuration; r < r0 + newDuration; r++) {
      TXshCell newCell = typeCell;
      newCell.m_frameId = TFrameId(++lastFrameNum);
      mainXsh->setCell(r, col, newCell);
    }
  } else if (newDuration < currentDuration) {
    mainXsh->removeCells(r0 + newDuration, col, currentDuration - newDuration);
  }

  // Audio shift for subsequent shots is handled entirely by resequenceXsheet()
  // (audio-linked variant records deltas before/after and applies them).
  // Do NOT shift audio here — doing so and then calling resequenceXsheet()
  // causes a double-shift that pushes audio too far and makes it disappear.
  resequenceXsheet();

  // Sync the sub-xsheet's Out marker to the new duration.
  // ztorySetShotRange updates s_frameRangeMap so the correct Out is shown
  // the next time the shot is opened for editing.
  // Range is [0, newDuration-1] (0-based, inclusive).
  ztorySetShotRange(col, 0, newDuration - 1);

  // If this shot's sub-xsheet is currently open, also update the live
  // play range so the FlipConsole markers move immediately.
  // IMPORTANT: re-read r0/r1 POST-resequenceXsheet() — the cells were
  // repositioned by resequence and the old r0 no longer points to them.
  ChildStack *cs = scene->getChildStack();
  if (cs && cs->getAncestorCount() == 1) {
    TXsheet *curXsh = cs->getXsheet();
    int pr0 = 0, pr1 = 0;
    column->getRange(pr0, pr1);
    TXsheet *shotXsh = nullptr;
    for (int r = pr0; r <= pr1 && !shotXsh; r++) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
        shotXsh = cell.m_level->getChildLevel()->getXsheet();
    }
    if (shotXsh && shotXsh == curXsh)
      XsheetGUI::setPlayRange(0, newDuration - 1, 1, false);
  }

  m_track->refreshFromScene();

  if (board) {
    auto after = board->captureSnapshot();
    TUndoManager::manager()->add(
        new UndoBoardState(board, tr("Resize Shot Duration"),
                           std::move(before), std::move(after)));
  }
}

void ZtoryAnimaticPanel::resequenceXsheet() {
  if (!m_audioLinked) {
    ZtoryModel::instance()->resequenceXsheet();
    return;
  }

  // Audio-video link: shift audio ColumnLevels to follow their associated shots.
  // We shift startFrame directly on each ColumnLevel instead of the broken
  // clear+setCell approach (TXshSoundColumn::setCell is designed for sequential
  // insertion, not for restoring cells read via getCell from different positions).
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { ZtoryModel::instance()->resequenceXsheet(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { ZtoryModel::instance()->resequenceXsheet(); return; }

  // 1. Record shot positions BEFORE resequence.
  struct ShotPos { int col; int r0; int duration; };
  std::vector<ShotPos> oldPositions;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    bool isChild = false;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        isChild = true; break;
      }
    }
    if (isChild) oldPositions.push_back({col, r0, r1 - r0 + 1});
  }

  // 2. Run resequence on video columns.
  ZtoryModel::instance()->resequenceXsheet();

  // 3. Read new shot positions AFTER resequence → delta per video column.
  std::map<int, int> shotDelta;
  for (auto &op : oldPositions) {
    TXshColumn *column = xsh->getColumn(op.col);
    if (!column || column->isEmpty()) continue;
    int nr0 = 0, nr1 = 0;
    column->getRange(nr0, nr1);
    shotDelta[op.col] = nr0 - op.r0;
  }

  // 4. Shift audio ColumnLevels so they follow their associated shots.
  //
  //    OLD approach (wrong): call shiftLevelInRange(shotR0, shotR1, delta) for
  //    each shot that moved. This only shifts ColumnLevels whose vsf falls
  //    inside exactly that shot's old range. A long (uncut) audio segment
  //    spanning multiple shots has vsf=0 → is never matched. A razor-cut
  //    segment for Shot C has vsf=146 → is not matched when we process Shot B
  //    with range [89,145]. Result: only the first cut segment ever shifts.
  //
  //    NEW approach: find the earliest shot that moved → shiftFrom.
  //    All shots after that point have the same delta (resequence packs
  //    tightly, so a single duration change propagates uniformly).
  //    Shift ALL audio ColumnLevels with vsf >= shiftFrom by that delta.
  //    This correctly handles: uncut tracks spanning all shots, tracks cut
  //    at shot boundaries, and any number of razor-cut segments.
  {
    int shiftFrom   = INT_MAX;
    int commonDelta = 0;
    for (auto &op : oldPositions) {
      int d = shotDelta.count(op.col) ? shotDelta.at(op.col) : 0;
      if (d != 0 && op.r0 < shiftFrom) {
        shiftFrom   = op.r0;
        commonDelta = d;
      }
    }
    if (commonDelta != 0 && shiftFrom != INT_MAX) {
      for (auto *at : m_audioTracks) {
        int audioCol = at->columnIndex();
        TXshColumn *ac = xsh->getColumn(audioCol);
        if (!ac) continue;
        TXshSoundColumn *sc = ac->getSoundColumn();
        if (!sc) continue;
        sc->shiftLevelFromFrame(shiftFrom, commonDelta);
      }
    }
  }

  // Invalidate waveform caches and repaint all audio tracks.
  // xsheetChanged (fired inside ZtoryModel::resequenceXsheet) rebuilds the
  // widgets via refreshAudioTracks() BEFORE the shift, so the cache is stale.
  // A plain update() won't rebuild the cache — we must mark it dirty first.
  for (auto *at : m_audioTracks) at->invalidateWaveform();

  // Invalidate the cached merged sound track so the next scrub/play rebuilds
  // it with the updated ColumnLevel positions.
  ZtoryAnimaticController::instance()->invalidateSoundTrack();

  xsh->updateFrameCount();

  // Clamp the animatic play range mark-out to the new total duration.
  // If shots were deleted or trimmed, the old mark-out might be beyond the
  // end of the timeline — causing playback to stop at a phantom position.
  m_ruler->clampPlayRangeToTimeline();
}

void ZtoryAnimaticPanel::onMatchSubsceneDuration(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  // Trova la prima riga reale della colonna
  TXshColumn *mainCol = mainXsh->getColumn(col);
  if (!mainCol) return;
  int r0 = 0, r1 = 0;
  mainCol->getRange(r0, r1);
  // Entra nella sottoscena per trovare l'ultimo frame non vuoto
  TXshCell cell = mainXsh->getCell(r0, col);
  if (!cell.m_level || !cell.m_level->getChildLevel()) return;
  TXsheet *subXsh = cell.m_level->getChildLevel()->getXsheet();
  if (!subXsh) return;

  // Trova l'ultimo frame non vuoto in tutta la sottoscena.
  // Sentinel -1 = nessun contenuto trovato.  Non usare 0 come sentinel perché
  // una sottoscena con un solo disegno al frame 0 ha lastFrame==0 e
  // il vecchio guard "if (lastFrame <= 0) return" bloccava quel caso.
  int lastFrame = -1;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    int r0 = 0, r1 = 0;
    TXshColumn *column = subXsh->getColumn(c);
    if (!column) continue;
    // ignoreLastStop=true: skip trailing SFH so r1 lands on the last real
    // drawing, not on the stop-frame cell (which is NOT empty and would
    // inflate lastFrame by +1 → empty frame at mark-out).
    column->getRange(r0, r1, /*ignoreLastStop=*/true);
    // Cerca l'ultimo frame con cella non vuota e non SFH (doppia protezione)
    for (int r = r1; r >= r0; r--) {
      TXshCell sc = subXsh->getCell(r, c);
      if (!sc.isEmpty() && !sc.getFrameId().isStopFrame()) {
        lastFrame = qMax(lastFrame, r);
        break;
      }
    }
  }

  if (lastFrame < 0) return;  // sottoscena completamente vuota
  int newDuration = lastFrame + 1;

  // No-op guard: while drawing inside the sub-scene every stroke burst lands
  // here (xsheetChanged → auto-match debounce), but the duration almost never
  // changes. Without this check each burst ran a full column resize +
  // resequenceXsheet + refreshFromScene, lagging the brush badly.
  {
    int m0 = 0, m1 = 0;
    mainCol->getRange(m0, m1, /*ignoreLastStop=*/true);
    if (m1 - m0 + 1 == newDuration) return;
  }

  // Apply new duration. Call directly — the signal connection would cause a
  // double execution since onShotDurationChanged is also connected to
  // m_track::shotDurationChanged.  The track visuals are rebuilt by
  // resequenceXsheet() → refreshFromScene() inside onShotDurationChanged.
  onShotDurationChanged(col, newDuration - 1);
}

void ZtoryAnimaticPanel::onFitAll() {
  // Compute the ppf that fits all video blocks into the current viewport width.
  int maxFrame = 0;
  for (auto &b : m_track->blocks())
    maxFrame = qMax(maxFrame, b.startFrameInMain + (b.f1 - b.f0 + 1));
  if (maxFrame <= 0) return;
  int vpW = m_scroll ? m_scroll->viewport()->width() - kLabelW : 800;
  if (vpW <= 0) vpW = 800;
  double fitPpf = qBound(kMinPpf, (double)vpW / maxFrame, kMaxPpf);
  onZoomChanged(fitPpf);
  // Scroll back to frame 0 so the whole timeline is in view from the start.
  if (m_scroll)
    m_scroll->horizontalScrollBar()->setValue(0);
}

void ZtoryAnimaticPanel::onZoomChanged(double ppf) {
  // Zoom-to-cursor: compute cursor position in content coords BEFORE changing ppf.
  int oldScrollX = m_scroll ? m_scroll->horizontalScrollBar()->value() : 0;
  int cursorScreenX = -1;
  if (m_scroll) {
    QPoint vp = m_scroll->viewport()->mapFromGlobal(QCursor::pos());
    if (m_scroll->viewport()->rect().contains(vp))
      cursorScreenX = vp.x();
  }

  double oldPpf = m_ppf;
  m_ppf = ppf;
  m_ruler->setPixelsPerFrame(ppf);
  m_track->setPixelsPerFrame(ppf);
  for (auto *at : m_audioTracks)
    at->setPixelsPerFrame(ppf);
  // Sync slider without triggering another valueChanged
  if (m_zoomSlider) {
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue((int)(ppf * 100));
    m_zoomSlider->blockSignals(false);
  }
  updateTrackWidths();

  // Force the scroll content to its new (zoomed) width NOW, so the horizontal
  // scrollbar's maximum is updated before we reposition it below.  Without this,
  // setValue() is clamped to the pre-zoom maximum when zooming in, and the frame
  // under the cursor drifts off-center.
  if (m_scrollContent && m_scrollContent->layout())
    m_scrollContent->resize(m_scrollContent->layout()->sizeHint().width(),
                            m_scrollContent->height());

  // Adjust scrollbar so the frame under the cursor stays at the same screen position.
  if (m_scroll && oldPpf > 0 && cursorScreenX >= 0) {
    int contentX = cursorScreenX + oldScrollX;
    if (contentX >= kLabelW) {
      double frame = (double)(contentX - kLabelW) / oldPpf;
      int newScrollX = (int)(kLabelW + frame * ppf) - cursorScreenX;
      m_scroll->horizontalScrollBar()->setValue(qMax(0, newScrollX));
    }
  }
}

void ZtoryAnimaticPanel::onFrameChanged(int frame) {
  m_ruler->setCurrentFrame(frame);
  m_track->setCurrentFrame(frame);
  for (auto *at : m_audioTracks)
    at->setCurrentFrame(frame);
  // Write to the dedicated animatic frame handle — this drives the animatic
  // viewer without touching TApp's global frame (BUG-03 fix).
  ZtoryAnimaticController::instance()->setCurrentFrame(frame);
}



void ZtoryAnimaticPanel::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);
  QAction *loadAudio = menu.addAction(tr("Load Audio..."));
  QAction *addTrack  = menu.addAction(tr("Add Audio Track"));
  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == addTrack) {
    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;
    int insertCol = xsh->getColumnCount();
    xsh->insertColumn(insertCol, TXshColumn::eSoundType);
    TUndoManager::manager()->add(new UndoAddAudioTrack(insertCol));
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    refreshAudioTracks();
    updateTrackWidths();
    app->getCurrentScene()->notifyCastChange();
    return;
  }
  if (chosen == loadAudio) {
    // In storyboard workflow, audio can only be loaded from the main xsheet.
    // Show the warning before opening the file dialog so the user doesn't
    // waste time selecting a file only to be rejected afterward.
    if (ZtoryModel::instance()->isStoryboardWorkflow()) {
      ToonzScene *checkScene = TApp::instance()->getCurrentScene()->getScene();
      if (checkScene && checkScene->getChildStack()->getAncestorCount() > 0) {
        DVGui::warning(tr(
            "In storyboard workflow, audio can only be loaded from the main "
            "xsheet.\nPlease close the current sub-scene first."));
        return;
      }
    }
    // Formati nativi: wav, aiff. mp3/ogg richiedono FFmpeg configurato.
    // NOTE: pass nullptr as parent (not 'this') — on macOS a docked panel widget
    // can cause the native sheet to open behind the main window or not appear.
    QString path = QFileDialog::getOpenFileName(
        nullptr, tr("Load Audio"),
        QString(),
        tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.ogg);;WAV (*.wav);;AIFF (*.aiff *.aif);;All Files (*)"));
    if (path.isEmpty()) return;

    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;

    // Inserisci nella prima colonna libera dopo le colonne esistenti
    int insertCol = xsh->getColumnCount();

    IoCmd::LoadResourceArguments args(TFilePath(path.toStdWString()));
    args.row0 = 0;
    args.col0 = insertCol;
    args.expose = true;
    IoCmd::loadResources(args);
    refreshAudioTracks();
  }
}

class ZtoryAnimaticPanelTFactory final : public TPanelFactory {
public:
  ZtoryAnimaticPanelTFactory() : TPanelFactory("ZtoryAnimaticT") {}
  TPanel *createPanel(QWidget *parent) override {
    auto *panel = new ZtoryAnimaticPanel(parent, /*switchEnabled=*/true);
    panel->setObjectName("ZtoryAnimaticT");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticPanelTFactory;

class ZtoryAnimaticPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticPanelFactory() : TPanelFactory("ZtoryAnimatic") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticPanel(parent);
    panel->setObjectName("ZtoryAnimatic");
    panel->setWindowTitle("Ztoryc Animatic");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticPanelFactory;

class ZtoryAnimaticViewerPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticViewerPanelFactory() : TPanelFactory("ZtoryAnimaticViewer") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticViewerPanel(parent);
    panel->setObjectName("ZtoryAnimaticViewer");
    panel->setWindowTitle("Ztoryc Viewer");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticViewerPanelFactory;

class ZtoryStoryStripPanelFactory final : public TPanelFactory {
public:
  ZtoryStoryStripPanelFactory() : TPanelFactory("ZtoryStoryStrip") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryStoryStripPanel(parent);
    panel->setObjectName("ZtoryStoryStrip");
    panel->setWindowTitle("Ztoryc Story Strip");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryStoryStripPanelFactory;

class ZtoryRightPanelFactory final : public TPanelFactory {
public:
  ZtoryRightPanelFactory() : TPanelFactory("ZtoryRightPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryRightPanel(parent);
    panel->setObjectName("ZtoryRightPanel");
    // Title intentionally left empty: switcher row labels the panel.
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryRightPanelFactory;

class ZtoryLeftPanelFactory final : public TPanelFactory {
public:
  ZtoryLeftPanelFactory() : TPanelFactory("ZtoryLeftPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryLeftPanel(parent);
    panel->setObjectName("ZtoryLeftPanel");
    // Title intentionally left empty: switcher row labels the panel.
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryLeftPanelFactory;


// ---- ZtoryDrawLeftPanel ----

ZtoryDrawLeftPanel::ZtoryDrawLeftPanel(QWidget *parent)
    : TPanel(parent) {
  // No window title: the switcher row (BOARD / SHOT) already labels this
  // composite panel — see ZtoryRightPanel for rationale.
  setWindowTitle("");
  setPanelType("ZtoryDrawLeftPanel");

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay   = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  // Toggle bar: BOARD / SHOT (manual switch, also auto-driven by shotActivatedForViewing)
  QWidget *bar      = new QWidget(container);
  QHBoxLayout *barL = new QHBoxLayout(bar);
  barL->setContentsMargins(2, 2, 2, 2);
  barL->setSpacing(2);
  m_toggleBtn = new QToolButton(bar);
  m_toggleBtn->setText(tr("BOARD / SHOT"));
  m_toggleBtn->setCheckable(true);
  m_toggleBtn->setChecked(false);
  m_toggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_toggleBtn->setFixedHeight(22);
  barL->addWidget(m_toggleBtn);

  m_linkBtn = new QToolButton(bar);
  m_linkBtn->setText(tr("\xF0\x9F\x94\x97"));  // 🔗
  m_linkBtn->setToolTip(tr("Link side panels to viewer toggle\n"
                            "When active, Board/Shot switches automatically\n"
                            "with the viewer on shot enter/exit"));
  m_linkBtn->setCheckable(true);
  m_linkBtn->setChecked(ZtoryModel::instance()->sidePanelsLinked());
  m_linkBtn->setFixedSize(28, 22);
  barL->addWidget(m_linkBtn);
  lay->addWidget(bar);

  m_stack = new QStackedWidget(container);

  // Page 0: Board
  m_boardPanel = new StoryboardPanel(m_stack);
  m_boardPanel->getTitleBar()->hide();
  m_boardPanel->setEmbedded();
  m_stack->addWidget(m_boardPanel);  // page 0

  // Page 1: Panel Navigator
  m_navigator = new ZtoryPanelNavigator(m_stack);
  m_navigator->getTitleBar()->hide();
  m_navigator->setEmbedded();
  m_stack->addWidget(m_navigator);  // page 1

  lay->addWidget(m_stack, 1);
  setWidget(container);

  // Manual toggle
  connect(m_toggleBtn, &QToolButton::toggled, this, [this](bool shotMode) {
    if (shotMode) showNavigatorMode();
    else          showBoardMode();
  });

  connect(m_linkBtn, &QToolButton::toggled, this, [](bool on) {
    ZtoryModel::instance()->setSidePanelsLinked(on);
  });

  // Auto-switch on shot activated / returned to main (only when linked)
  connect(ZtoryModel::instance(), &ZtoryModel::shotActivatedForViewing,
          this, [this](int) { showNavigatorMode(); });
  connect(ZtoryModel::instance(), &ZtoryModel::returnToViewerMainRequested,
          this, &ZtoryDrawLeftPanel::showBoardMode);
  // Safety net: xsheet navigated back to main by any path (not just via Board)
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    if (scene->getChildStack()->getAncestorCount() == 0)
      showBoardMode();
    else
      showNavigatorMode();
  });
}

void ZtoryDrawLeftPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  if (scene->getChildStack()->getAncestorCount() > 0)
    showNavigatorMode();
  else
    showBoardMode();
}

void ZtoryDrawLeftPanel::showBoardMode() {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  m_stack->setCurrentIndex(0);
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(false);
  m_toggleBtn->setText(tr("BOARD / SHOT"));
  m_toggleBtn->blockSignals(false);
}

void ZtoryDrawLeftPanel::showNavigatorMode() {
  if (!ZtoryModel::instance()->sidePanelsLinked()) return;
  m_stack->setCurrentIndex(1);
  m_toggleBtn->blockSignals(true);
  m_toggleBtn->setChecked(true);
  m_toggleBtn->setText(tr("SHOT / BOARD"));
  m_toggleBtn->blockSignals(false);
}


class ZtoryDrawLeftPanelFactory final : public TPanelFactory {
public:
  ZtoryDrawLeftPanelFactory() : TPanelFactory("ZtoryDrawLeftPanel") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryDrawLeftPanel(parent);
    panel->setObjectName("ZtoryDrawLeftPanel");
    // Title intentionally left empty: switcher row labels the panel.
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryDrawLeftPanelFactory;

class ZtoryPanelNavigatorFactory final : public TPanelFactory {
public:
  ZtoryPanelNavigatorFactory() : TPanelFactory("ZtoryPanelNavigator") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryPanelNavigator(parent);
    panel->setObjectName("ZtoryPanelNavigator");
    panel->setWindowTitle("Shot Board");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryPanelNavigatorFactory;


