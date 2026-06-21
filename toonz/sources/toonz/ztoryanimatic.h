#pragma once
#ifndef ZTORYANIMATIC_H
#define ZTORYANIMATIC_H

#include "viewerpane.h"
#include "tapp.h"
#include "pane.h"
#include "ztorymodel.h"
#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QHBoxLayout>
#include <set>
#include <QKeyEvent>
#include <QHash>
#include <QMap>
#include <map>
#include <QSet>
#include <QStackedWidget>
#include <QToolButton>

// ---- ZtoryAnimaticController ----
// Singleton that owns the dedicated frame state for the animatic timeline
// and viewer, isolating them from TApp's global TFrameHandle.
class TFrameHandle;
class ZtoryAnimaticViewer;
#include "tsound.h"

class ZtoryAnimaticController : public QObject {
  Q_OBJECT
public:
  static ZtoryAnimaticController *instance();
  TFrameHandle *frameHandle() const { return m_frameHandle; }
  TXsheet *mainXsheet() const;
  void setCurrentFrame(int frame);
  int currentFrame() const;
  // Cached merged sound track — filled lazily on first scrub / on play start.
  // Call invalidateSoundTrack() after any ColumnLevel shift to force rebuild.
  TSoundTrackP soundTrack() const { return m_soundTrack; }
  void setSoundTrack(TSoundTrackP st) { m_soundTrack = st; }
  void invalidateSoundTrack() {
    m_soundTrack        = TSoundTrackP();
    m_soundBuildPending = false;  // Allow a fresh async build
    m_columnSoundTracks.clear();  // Per-column caches must rebuild too
    // Bump the generation: any in-flight async pre-build started for the
    // PREVIOUS scene/track must discard its result on delivery instead of
    // writing it into m_soundTrack — otherwise audio from a previously open
    // scene (or a deleted track) leaks into the current one.
    ++m_soundGen;
  }

  // Per-column un-mixed sound track, cached.  Used during play so each audio
  // column can be played on its own TSoundOutputDevice (column->m_player) for
  // real-time per-track volume control via QAudioOutput::setVolume().
  TSoundTrackP requireColumnSoundTrack(int col);

  // Self-invalidate the audio caches if the main xsheet's sound columns no
  // longer match what was cached.  Call before any cache read.
  void validateSoundCache();

  // Start per-column audio playback from |startMainFrame| on each audio
  // column's own m_player.  Each player runs at the column's m_volume,
  // updated in real-time via TXshSoundColumn::setVolume().
  void startPerColumnAudio(int startMainFrame);
  // Stop every audio column's m_player.
  void stopPerColumnAudio();
  // Returns elapsed microseconds since play start, taken from the first
  // audio column's player processedUsecs.  Used as the audio-master clock
  // by ZtoryAnimaticViewer::onDrawFrame in place of the previous
  // mainXsh->getAudioPlayedUSecs() (the muted mix on mainXsh->m_player did
  // not advance processedUsecs reliably on macOS CoreAudio when volume=0).
  qint64 getMasterAudioUsecs() const;
  // Viewer registers itself so the panel can call restartAudioIfPlaying().
  void setViewer(ZtoryAnimaticViewer *v) { m_viewer = v; }
  ZtoryAnimaticViewer *viewer() const { return m_viewer; }
  // Returns true when the animatic viewer is active at the main level,
  // meaning it owns audio and the native ComboViewer must not compete.
  bool ownsAudioAtMainLevel() const;

  // Returns true when we are inside a sub-scene AND the main xsheet has
  // audio. In that case the controller streams main-xsheet audio at the
  // mapped time offset (onNativePlayingStatusChanged), and the native
  // ComboViewer's per-frame playAudioFrame must yield to avoid a double
  // playback (one from sample 0, one from mainFrame*spf).
  bool ownsSubSceneAudio() const;

  // Build (or return cached) merged track from the main xsheet.
  // Safe to call from any scrub handler — returns null if no audio.
  TSoundTrackP requireSoundTrack();

  // Start an async background build of the merged sound track.
  // Result is delivered to m_soundTrack via QueuedConnection (main thread).
  // No-op if a build is already in progress or the track is already cached.
  void preBuildSoundTrackAsync();

  // Stops native-viewer audio that the controller is streaming on behalf of
  // sub-scene playback. Called by ZtoryAnimaticViewer::stopAudio().
  void stopNativeAudio() {
    TXsheet *xsh = mainXsheet();
    if (xsh) xsh->stopScrub();
    m_nativeAudioPlaying = false;
    if (m_scrubDevice) m_scrubDevice->stop();
  }

  // Dedicated audio device used ONLY for per-frame scrub audio.
  // Kept separate from mainXsh->m_player so that reset() on scrub
  // does not disrupt continuous playback.
  TSoundOutputDevice *scrubDevice();
  void stopScrubDevice() { if (m_scrubDevice) m_scrubDevice->stop(); }

  // Re-triggers native sub-scene audio if play is active and audio is enabled.
  // Call this when un-muting (MI_ToggleMainAudio re-enabled) during playback.
  void restartNativeAudioIfPlaying();

  // Animatic-owned play range — independent from scene->getPreviewProperties()
  // which is shared with (and overwritten by) the native xsheet viewer when
  // entering/leaving sub-scenes.  The ruler and animatic marker logic read
  // this instead of XsheetGUI::getPlayRange().
  // Sets animatic play range AND mirrors to XsheetGUI (native viewer sync).
  void setAnimaticPlayRange(int r0, int r1);
  void getAnimaticPlayRange(int &r0, int &r1) const { r0 = m_animaticR0; r1 = m_animaticR1; }
  bool isAnimaticPlayRangeEnabled() const { return m_animaticR0 <= m_animaticR1; }

public slots:
  // Called by the ruler whenever the In/Out play range changes, so the
  // animatic viewer's FlipConsole markers stay in sync.
  void notifyPlayRangeChanged() { emit playRangeChanged(); }

private slots:
  // Fired by TApp::getCurrentFrame()->isPlayingStatusChanged.
  // When the native viewer starts playing inside a shot sub-scene, this
  // starts the main-xsheet audio at the correct time offset so the animator
  // can hear the soundtrack while working on animation to picture.
  void onNativePlayingStatusChanged();
  // Fired by TApp::getCurrentFrame()->frameSwitched.
  // Provides per-frame scrub audio from the main xsheet while the user
  // drags the playhead inside a shot sub-scene (not during continuous play).
  void onNativeFrameSwitched();

signals:
  void playRangeChanged();

private:
  ZtoryAnimaticController();
  ~ZtoryAnimaticController();
  TFrameHandle         *m_frameHandle;
  TSoundTrackP          m_soundTrack;
  int m_animaticR0 = 0;
  int m_animaticR1 = -1;  // -1 = no range set (full range)
  ZtoryAnimaticViewer  *m_viewer = nullptr;
  // True while we are streaming main-xsheet audio on behalf of the native viewer.
  bool m_nativeAudioPlaying  = false;
  // Last frame seen by onNativeFrameSwitched during native play — used to
  // detect FlipConsole loop-back (frame jumps backward) so we can restart
  // audio after the playhead returns to the start of the play range.
  int m_lastNativePlayFrame  = -1;
  // Guards against launching a second async build while one is already running.
  bool m_soundBuildPending   = false;
  // Incremented by invalidateSoundTrack().  An async pre-build captures the
  // value at launch and discards its result if the generation changed by the
  // time it finishes — prevents stale cross-scene audio.
  unsigned m_soundGen        = 0;
  // Fingerprint of the main xsheet's sound columns (pointers + lengths) at the
  // time the cache was built.  validateSoundCache() compares it against the
  // live xsheet and self-invalidates if a track was added/removed/reordered/
  // edited — a catch-all so no mutation path can leave stale audio cached.
  size_t   m_soundFingerprint = 0;
  // Dedicated device for per-frame scrub audio — separate from mainXsh->m_player
  // so that reset() for precision scrub never disrupts continuous playback.
  TSoundOutputDevice   *m_scrubDevice = nullptr;
  // Main-xsheet frame up to which scrub audio has been played.  The next scrub
  // segment continues from here so the whole scrubbed range is heard without
  // gaps.  -1 = uninitialized (next scrub event resyncs).
  int                   m_scrubAudioFrame = -1;
  // Per-column un-mixed sound tracks built on demand, keyed by the sound
  // column POINTER (not the column index).  Index keying aliased after a
  // column delete/reorder — index 2 would still return the track of the
  // column that used to be there, so a deleted/wrong audio file played.
  // The cache is fully cleared by invalidateSoundTrack() on every scene or
  // column-structure change, so a reused pointer cannot survive across them.
  std::map<TXshSoundColumn *, TSoundTrackP> m_columnSoundTracks;
};

class ZtoryAnimaticRuler : public QWidget {
  Q_OBJECT
public:
  ZtoryAnimaticRuler(QWidget *parent = nullptr);
  void setFps(double fps) { m_fps = fps; update(); }
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  void setShowTimecode(bool on) { m_showTimecode = on; update(); }
  bool showTimecode() const { return m_showTimecode; }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  int currentFrame() const { return m_currentFrame; }
  void initPlayRangeIfNeeded();
  void resetPlayRangeToFull();
  void clampPlayRangeToTimeline();  // shrinks mark-out if beyond new duration

protected:
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;
signals:
  void frameChanged(int frame);
  void zoomChanged(double ppf);
private:
  // Opens the native navigation-tag editor (label + color) for the marker at
  // the given main-xsheet frame, then writes the result back to the xsheet.
  void editMarker(int frame);

  double m_fps = 24.0;
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  bool m_showTimecode = false;
  // In/Out marker drag state (13b)
  enum DragMode { None, DragIn, DragOut };
  DragMode m_dragMode = None;
  // Navigation-tag marker whose label is shown on hover (-1 = none).
  int m_hoverTagFrame = -1;
};

// Shared limits for the user-resizable track height (bottom-edge grip).
static constexpr int kZtoryMinTrackH  = 24;   // label + lock/mute only
static constexpr int kZtoryMaxTrackH  = 120;  // generous waveform / thumbnail
static constexpr int kZtoryResizeGrip = 5;    // bottom px reserved for the grip

class ZtoryAnimaticTrack : public QWidget {
  Q_OBJECT
public:
  enum Tool { SelectTool, TrimTool, RazorTool };

  struct ShotBlock {
    int col;
    int startFrameInMain; // frame di inizio nel main xsheet
    int f0, f1;           // marker In/Out della sottoscena
    QString shotNumber;
    QPixmap thumbnail;
    int transitionFrames = 0; // total cross-dissolve frames (T/2 tail, T/2 head)
  };

  ZtoryAnimaticTrack(QWidget *parent = nullptr);
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  void setTool(Tool t) { m_tool = t; updateCursor(); }
  Tool tool() const { return m_tool; }
  // Vertical track height — user-resizable via the grip on the bottom edge.
  int trackHeight() const { return m_trackHeight; }
  void setTrackHeight(int h);
  void refreshFromScene();
  // Invalidate cached thumbnails so they are re-rendered on next refresh.
  void clearThumbCache() { m_thumbCache.clear(); }
  // Razor hover: set the absolute frame under the cursor (or -1 to clear).
  // Also called by the panel to sync the hover position across tracks.
  void setRazorHoverFrame(int frame);
  // Returns the blocks vector (for panel to read cut frame positions).
  const std::vector<ShotBlock> &blocks() const { return m_blocks; }
  const std::set<int> &selectedCols() const { return m_selectedCols; }
  // Apply a selection coming from the shared model (Board ↔ Animatic sync)
  // WITHOUT re-emitting selectionChanged — avoids an update loop.
  void setSelectedColsFromShared(const std::set<int> &cols) {
    if (cols == m_selectedCols) return;
    m_selectedCols = cols;
    update();
  }
  // Lock — blocks drag/resize on the video track
  bool isLocked() const { return m_locked; }
  void setLocked(bool on);

  // Snap (magnet): frames to snap dragged shot boundaries to.
  void setSnapEnabled(bool on) { m_snapEnabled = on; }
  void setSnapFrames(const QVector<int> &f) { m_snapFrames = f; }

protected:
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;

signals:
  void shotClicked(int col);
  void shotDoubleClicked(int col);
  void selectionChanged(std::set<int> selectedCols);
  void shotDurationChanged(int col, int newF1);
  void shotMoved(int col, int newStartFrame);
  void zoomChanged(double ppf);
  void matchSubsceneDuration(int col);
  void razorRequested(int col, int splitFrame);
  void mergeWithNextRequested(int col);
  void returnToMainRequested();
  // Emitted on mouse move when razor is active — absolute frame under cursor,
  // or -1 when the mouse leaves. Panel forwards this to audio tracks.
  void razorHoverFrameChanged(int frame);
  void lockedChanged(bool on);
  // Roll: colA new duration, colB new duration (colB repositioned by resequenceXsheet)
  void rollEdit(int colA, int newDurA, int colB, int newDurB);
  // Cross-dissolve transition: colA is the outgoing shot, colB is the incoming, frames = total overlap (0 = no transition)
  void transitionChanged(int colA, int colB, int frames);
  // Emitted while/after the bottom resize grip is dragged (final height on release).
  void trackHeightChanged(int h);

private:
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  std::vector<ShotBlock> m_blocks;

  // ── Drag state ────────────────────────────────────────────────────────────
  enum DragMode { NoDrag, RippleTrim, Roll, TransitionTrim, Resize };
  DragMode m_dragMode     = NoDrag;
  int m_trackHeight       = 80;  // current height (px), resizable via bottom grip
  int m_dragStartX        = 0;   // pixel X at drag start
  int m_dragColA          = -1;  // RippleTrim/Roll/TransitionTrim: primary col (left for Roll/Transition)
  int m_dragColB          = -1;  // Roll/TransitionTrim: right col
  int m_dragOrigDurA      = 0;   // original duration of colA at drag start
  int m_dragOrigDurB      = 0;   // original duration of colB at drag start
  int m_dragOrigStartB    = 0;   // original startFrameInMain of colB (Roll)
  int m_dragOrigTransition = 0;  // original transitionFrames at drag start
  // For RippleTrim: saved positions/durations of all blocks
  QMap<int, int> m_origStarts;
  QMap<int, int> m_origDurations;
  std::set<int> m_selectedCols;
  int m_lastClickedCol = -1; // for Shift+click range selection
  QHash<int, QPixmap> m_thumbCache; // col → rendered composite thumbnail
  Tool m_tool = SelectTool;
  int m_razorHoverFrame = -1;
  // Lock button painted in paintEvent, toggled via mousePressEvent hit-test
  bool m_locked = false;
  // Snap (magnet)
  bool m_snapEnabled = false;
  QVector<int> m_snapFrames;

  void updateCursor();
};

// ---- ZtoryAudioTrack ----
// Una traccia audio orizzontale con waveform, nome colonna e altezza regolabile
class ZtoryAudioTrack : public QWidget {
  Q_OBJECT
public:
  ZtoryAudioTrack(int col, const QString &name, QWidget *parent = nullptr);
  void setPixelsPerFrame(double ppf) {
    if (m_ppf != ppf) { m_ppf = ppf; m_waveformDirty = true; update(); }
  }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  void invalidateWaveform() { m_waveformDirty = true; update(); }
  // Abort any in-progress drag without committing the move.
  // Call before deleting the widget to prevent stale drag state from firing.
  void cancelDrag() { m_dragMode = NoDrag; m_draggingPreview = false; }
  int trackHeight() const { return m_trackHeight; }
  void setTrackHeight(int h) {
    h = qBound(kZtoryMinTrackH, h, kZtoryMaxTrackH);
    if (h == m_trackHeight) return;
    m_trackHeight = h; m_waveformDirty = true; setFixedHeight(h); update();
  }
  int columnIndex() const { return m_col; }
  void setColumnIndex(int col) { m_col = col; }  // update after column shift
  TXshSoundColumn *soundColumn() const { return m_soundCol; }
  void setRazorActive(bool on);
  // Cut frame markers — drawn as bright separator lines in the waveform.
  // Set by the panel after every shot change; frame indices are absolute
  // (same coordinate space as the audio column).
  void setCutFrames(const QVector<int> &frames);
  // Razor hover: absolute frame under the razor cursor, or -1 to clear.
  void setRazorHoverFrame(int frame);

  // Audio segment: a contiguous range of non-empty cells in this column
  struct Segment { int r0; int r1; };  // inclusive frame range
  std::vector<Segment> findSegments() const;

  // Selection
  bool hasSelection() const { return m_selSeg.r0 >= 0; }
  Segment selectedSegment() const { return m_selSeg; }

  // ── Group move (multi-track) ────────────────────────────────────────────────
  // Coordinated by the panel: when one track's selected segment is dragged, the
  // same frame delta is previewed/committed on every other selected track.
  void beginGroupDrag();                 // snapshot orig position + undo state
  void previewGroupMove(int deltaFrames);// visual move of this track's selection
  void commitGroupMove(int deltaFrames); // apply shift to the audio data (+undo)

  // Snap (magnet): targets are frames to snap dragged edges to.
  void setSnapEnabled(bool on) { m_snapEnabled = on; }
  void setSnapFrames(const QVector<int> &f) { m_snapFrames = f; }

  // Clipboard (shared across all audio tracks)
  static void clipboardCut(ZtoryAudioTrack *src);
  static void clipboardCopy(ZtoryAudioTrack *src);
  static void clipboardPaste(ZtoryAudioTrack *dst, int frame);

  int frameAtX(int x) const;

  // Lock / Mute / Solo — driven by real QToolButton children
  bool isLocked() const { return m_locked; }
  bool isMuted()  const { return m_muted; }
  bool isSolo()   const { return m_solo; }
  void setLocked(bool on);
  void setMuted(bool on);
  void setSolo(bool on);
  // Called by the panel after computing effective mute (solo logic).
  // Dims waveform visually without corrupting the user's m_muted state.
  void setEffectiveMuted(bool on) { m_effectiveMuted = on; update(); }

signals:
  void zoomChanged(double ppf);   // Ctrl+Scroll on this track
  void razorRequested(int col, int frame);
  void segmentMoved();
  void segmentDroppedOutside(int srcCol, int origR0, int origR1, int dragOffset, QPoint globalPos);
  void lockedChanged(int col, bool on);
  void muteToggleRequested(int col);
  void soloToggleRequested(int col);
  void selectionCleared();  // emitted when this track clears its own selection
  void deleteRequested(int col);  // right-click → Delete Track
  // Group move coordination (handled by ZtoryAnimaticPanel):
  void exclusiveSelectRequested();        // plain click — clear other tracks
  void groupDragStarted();                // SegmentDrag begun on a selected seg
  void groupDragDelta(int deltaFrames);   // live delta during the drag
  void groupDragCommitted(int deltaFrames);// final delta on release
  // Emitted while/after the bottom resize grip is dragged (final height on release).
  void trackHeightChanged(int h);

public slots:
  void clearSelection();    // clears m_selSeg and repaints

protected:
  bool event(QEvent *) override;
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  void focusInEvent(QFocusEvent *) override;
  void focusOutEvent(QFocusEvent *) override;

private:
  // L/M/S buttons are drawn in paintEvent and clicked via hit-test in mousePressEvent

  int m_col;
  TXshSoundColumn *m_soundCol = nullptr;  // stable pointer — survives column shifts
  QString m_name;
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  int m_trackHeight = 50;
  QPixmap m_waveformCache;
  bool m_waveformDirty = true;
  int  m_cacheOffsetX  = 0;   // track-coord x of the left edge of m_waveformCache
  // Preview bar
  int  m_previewR0        = -1;
  int  m_previewR1        = -1;
  bool m_draggingPreview  = false;
  int  m_previewDragStart = -1;
  bool m_razorActive      = false;
  int  m_razorHoverFrame  = -1;
  QVector<int> m_cutFrames;
  // Segment selection & drag
  Segment m_selSeg{-1, -1};
  enum DragMode { NoDrag, SegmentDrag, TrimLeft, TrimRight, Resize };
  DragMode m_dragMode     = NoDrag;
  int  m_dragStartFrame   = -1;
  int  m_dragOrigR0       = -1;
  int  m_dragOrigR1       = -1;
  // L/M/S state — toggled via mousePressEvent, rendered in paintEvent
  bool m_locked = false;
  bool m_muted  = false;
  bool m_solo   = false;
  // Volume — mirrors TXshSoundColumn::getVolume() (range [0,1]).  Drawn as a
  // small horizontal slider in the label area; drag updates the column's
  // m_volume so the next mixingTogether() picks it up.
  double m_volume         = 1.0;
  bool   m_draggingVolume = false;
  // Set by applyMuteSolo() to dim the waveform when another track is solo'd.
  // Separate from m_muted so user state is never corrupted.
  bool m_effectiveMuted = false;
  // Focus highlight — true while this widget has keyboard focus
  bool m_hasFocus = false;
  // Undo snapshot taken at drag/trim start; committed in mouseReleaseEvent
  TXshSoundColumn *m_undoBefore = nullptr;
  // Group-move: orig position of this track's selection captured at drag start.
  // -1 when this track is not part of an active group move.
  int m_groupOrigR0 = -1;
  int m_groupOrigR1 = -1;
  // True while THIS track is the one being dragged (drives group broadcast) so
  // it commits itself; sibling tracks are committed by the panel.
  bool m_isGroupLeader = false;
  // Snap (magnet)
  bool m_snapEnabled = false;
  QVector<int> m_snapFrames;
};

// ---- ZtoryStoryStrip ----
// Horizontal thumbnail strip showing all shots; clicking navigates to a shot.
class ZtoryStoryStrip : public QWidget {
  Q_OBJECT
public:
  explicit ZtoryStoryStrip(QWidget *parent = nullptr);
  void refreshFromScene();
  void setCurrentCol(int col);

signals:
  void shotClicked(int col);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *) override;

private:
  struct ThumbEntry {
    int col;
    QString shotNumber;
    QPixmap thumb;
  };
  std::vector<ThumbEntry> m_entries;
  int m_currentCol = -1;
  int m_scrollOffset = 0; // horizontal pixel offset
  QHash<int, QPixmap> m_thumbCache; // col → rendered composite thumbnail
  static constexpr int kThumbH = 54;
  static constexpr int kThumbW = 80;
  static constexpr int kSpacing = 4;
};

// ---- ZtoryStoryStripPanel ----
// Standalone TPanel wrapper for ZtoryStoryStrip
class ZtoryStoryStripPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryStoryStripPanel(QWidget *parent = nullptr);
  void refreshFromScene();

protected:
  void showEvent(QShowEvent *e) override;

private:
  ZtoryStoryStrip *m_strip;
};

// ---- ZtoryAnimaticViewer ----
// Standalone scene viewer that always shows the main xsheet.
// Overrides onDrawFrame and updateFrameRange so that playback is driven by the
// dedicated ZtoryAnimaticController::frameHandle() instead of the global
// TApp::getCurrentFrame().  This decouples the animatic play button from the
// native timeline / sub-scene currently open in the editor (BUG-03 play fix).
class ZtoryAnimaticViewer : public BaseViewerPanel {
  Q_OBJECT
public:
  ZtoryAnimaticViewer(QWidget *parent = nullptr);
  ~ZtoryAnimaticViewer() override;
  void stopAudio();
  void updateShowHide() override {}
  void addShowHideContextMenu(QMenu *) override {}
  void checkOldVersionVisblePartsFlags(QSettings &) override {}

  // Set up minimal title-bar buttons for the animatic viewer:
  // Camera Stand View, Camera View, and Preview.
  // Called by ZtoryAnimaticViewerPanel after construction.
  void initializeAnimaticTitleBar(TPanelTitleBar *titleBar);

  // Called by the panel after applyMuteSolo() — if playback is in progress,
  // stops and restarts audio so the new mix takes effect immediately.
  void restartAudioIfPlaying();

  // Schedule an audio restart on the next onDrawFrame tick.  Safe to call
  // from any slot (including button-click handlers) because onDrawFrame runs
  // between CoreAudio XPC callbacks.
  void requestAudioRestart() { m_pendingAudioRestart = true; }

  // True when the animatic is actively streaming audio (full-track continuous
  // play). Used by the controller to decide whether to suppress scrub audio.
  bool isContinuousPlaying() const { return m_continuousPlay; }

  // Override: write frame to controller's handle, NOT TApp::getCurrentFrame().
  // Base implementation always uses the global handle → during play it would
  // advance the sub-scene's frame instead of the animatic frame.
  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings,
                   QElapsedTimer *timer     = nullptr,
                   qint64 targetInstant     = 0) override;

  // Override: always read markers from the main xsheet play range.
  // Base implementation reads from scene->getProperties() but the base's
  // onSceneChanged() calls this after xsheetChanged — which fires when entering
  // a sub-scene — and would overwrite whatever updateAnimaticFrameMarkers set.
  void updateFrameMarkers() override;

protected:
  void showEvent(QShowEvent *e) override;
  // Swallows tool mouse/tablet input on the scene viewer while the Animate
  // (Edit) tool is active: the animatic is a timing/preview view, transforming
  // shot objects there makes no sense. Pan/zoom navigation still pass through.
  bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
  // Sets FlipConsole frame range from the main xsheet (top-level), not from
  // TApp::getCurrentFrame() which would return the sub-scene's frame count.
  void updateAnimaticFrameRange();
  // Sets FlipConsole in/out markers from the main xsheet play range.
  // When in a sub-scene, clears markers so the full animatic range is used.
  void updateAnimaticFrameMarkers();
  // Called when FlipConsole play starts: overrides m_sound built by base
  // onPlayingStatusChanged() (which reads from the wrong sub-scene xsheet).
  void onAnimaticPlayingStatusChanged(bool playing);
  void onAudioToggleChanged();

private:
  // Rebuilds m_sound from the main xsheet (not the current/sub-scene xsheet).
  void refreshAnimaticSound();
  // Per-frame audio for scrubbing only (not during play — play uses continuous
  // streaming started in onAnimaticPlayingStatusChanged).
  void playAnimaticAudioFrame(int frame);

  // Keeps the merged sound track alive as long as the viewer may use m_sound.
  // m_sound (base class raw ptr) must not outlive the TSoundTrack object.
  // By holding our own ref here, we guarantee the object stays alive until
  // the next refreshAnimaticSound() call replaces it — even if the controller
  // invalidates its own cache in the meantime.
  TSoundTrackP m_soundTrackRef;

  // True while the full-track continuous play is active.
  // When true, playAnimaticAudioFrame is a no-op (audio already streaming).
  bool m_continuousPlay = false;

  // 0-based animatic frame at which the current play session started.
  // Used by onDrawFrame to compute the audio-master target frame.
  int m_playStartFrame = 0;
  // Last master-audio processedUsecs seen by onDrawFrame.  When it stops
  // advancing the audio has finished: if the mark-out is still ahead (video
  // longer than audio) we keep advancing on the FlipConsole wall-clock.
  qint64 m_lastMasterAudioUsecs = 0;

  // Previous FlipConsole frame (1-based) seen by onDrawFrame.
  // Used to detect loop-back (frame drops below previous value).
  int m_prevFlipFrame = 0;

  // Set by applyMuteSolo() when audio needs to be restarted (mute/solo changed
  // during playback).  Checked at the top of onDrawFrame, which runs on the
  // main thread between CoreAudio XPC callbacks — safe to call stopScrub/play.
  bool m_pendingAudioRestart = false;

  // Tracks ctrl-handle connections so they aren't duplicated across show/hide.
  QMetaObject::Connection m_frameRangeConn;
  QMetaObject::Connection m_audioConn;
};

// ---- ZtoryPanelNavigator ----
// Shot panel navigator for the ZTORYC T drawing room.
// Shows the active panel of the current shot as a large preview with
// prev/next navigation, editable Dialog/Action/Notes fields, and a
// "Sync timeline" toggle that links panel selection to the native playhead.
class QTextEdit;
class QTimer;
class ZtoryPanelNavigator : public TPanel {
  Q_OBJECT
public:
  explicit ZtoryPanelNavigator(QWidget *parent = nullptr);

protected:
  void resizeEvent(QResizeEvent *e) override;
  void showEvent(QShowEvent *e) override;
  // Light-direction gizmo editing on the large preview (mouse events of
  // m_previewLabel are intercepted here).
  bool eventFilter(QObject *obj, QEvent *e) override;

public slots:
  void onShotActivated(int col);
  void onReturnToMain();
  void onShotDataChanged(int shotIdx);
  void onModelReset();
  void onFrameSwitched();
  void onXsheetChanged();

private:
  void setActivePanel(int panelIdx, bool updateFrame = false);
  void refreshPreview();
  void refreshTextFields();
  void refreshActivePanelFromFrame();
  void refreshInfoLabels();    // updates header + panel info + shot info
  void syncFromScene();        // (re)bind m_shotIdx to whatever sub-scene we're in

  int           m_shotIdx      = -1;
  int           m_panelIdx     = 0;
  bool          m_syncEnabled  = true;
  bool          m_blockSignals = false;

  QLabel       *m_headerLabel      = nullptr;
  QLabel       *m_previewLabel     = nullptr;
  QLabel       *m_panelCountLabel  = nullptr;
  QLabel       *m_panelInfoLabel   = nullptr;  // "P001 - 24f / 1.0s"
  QLabel       *m_shotInfoLabel    = nullptr;  // "Total: 48f / 2.0s - 2 panels"
  QToolButton  *m_prevBtn          = nullptr;
  QToolButton  *m_nextBtn          = nullptr;
  QTextEdit    *m_dialogField      = nullptr;
  QTextEdit    *m_actionField      = nullptr;
  QTextEdit    *m_notesField       = nullptr;
  QToolButton  *m_syncBtn          = nullptr;
  QTimer       *m_refreshTimer     = nullptr;
  QPixmap       m_cachedPreview;

  // ── Light-direction gizmo (task 40 FASE 3) on the large preview ──────────
  QToolButton *m_lightEditBtn   = nullptr;  // checkable: drag-to-place mode
  QToolButton *m_lightShowBtn   = nullptr;  // checkable: visibility (mirrors Board)
  QToolButton *m_lightColorBtn  = nullptr;  // colour swatch (mirrors Board)
  bool         m_lightDragging  = false;
  QPointF      m_lightDragTail, m_lightDragTip;   // normalized 0-1
  double       m_lightDragDepth  = 0.0;
  double       m_lightDragSpread = 35.0;
  // Debounced re-render on panel resize: the cached pixmap is rescaled
  // immediately (cheap), the full render fires after the resize burst.
  QTimer      *m_resizeRenderTimer = nullptr;
  QPointF      normalizedPreviewPos(const QPoint &labelPos) const;
  void         drawLightRubberBand();
  void         commitLightEdit(bool remove);
};

// ---- ZtoryRightPanel ----
// Single panel that shows the animatic-mode or shot-mode right column.
//   Page 0 (animatic): Script viewer + Record Audio button
//   Page 1 (shot):     Studio Palette + Style Editor + Level Palette
// Links to the viewer toggle via ZtoryModel::shotActivatedForViewing.
class ZtoryScriptView;
class FileBrowser;
class StyleEditorPanel;
class StudioPaletteViewerPanel;
class PaletteViewerPanel;
class ZtoryRightPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryRightPanel(QWidget *parent = nullptr);

public slots:
  void showAnimaticMode();
  void showShotMode(int col = -1);

private:
  QStackedWidget        *m_stack      = nullptr;
  QToolButton           *m_toggleBtn  = nullptr;
  QToolButton           *m_linkBtn    = nullptr;
  // Shot-mode panels (lazy)
  StyleEditorPanel      *m_styleEditor    = nullptr;
  StudioPaletteViewerPanel *m_studioPalette = nullptr;
  PaletteViewerPanel    *m_levelPalette  = nullptr;
};

// ---- ZtoryLeftPanel ----
// Single panel that shows Board (page 0) or XSheet (page 1) in the same space.
// A toggle button switches between them; an optional 🔗 links the switch to the
// viewer toggle (ZtoryModel::shotActivatedForViewing / returnToViewerMainRequested).
class StoryboardPanel;
class XsheetViewerPanel;
class TimelineViewerPanel;
class ZtoryLeftPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryLeftPanel(QWidget *parent = nullptr);

public slots:
  void showBoardMode();       // switch to page 0 (Board)
  void showShotMode(int col = -1);  // switch to page 1 (XSheet)

private:
  StoryboardPanel   *m_boardPanel  = nullptr;
  XsheetViewerPanel *m_xsheetPanel = nullptr;
  QStackedWidget    *m_stack       = nullptr;
  QToolButton       *m_toggleBtn   = nullptr;
  QToolButton       *m_linkBtn     = nullptr;
};


// ---- ZtoryDrawLeftPanel ----
// Left panel for the ZTORYC T (drawing) room.
// Toggles between Board (page 0) and the Photoshop-style Navigator (page 1).
// There is no XSheet toggle here — drawing context does not need it.
class ZtoryDrawLeftPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryDrawLeftPanel(QWidget *parent = nullptr);

protected:
  void showEvent(QShowEvent *e) override;

public slots:
  void showBoardMode();
  void showNavigatorMode();

private:
  StoryboardPanel      *m_boardPanel  = nullptr;
  ZtoryPanelNavigator  *m_navigator   = nullptr;
  QStackedWidget       *m_stack       = nullptr;
  QToolButton          *m_toggleBtn   = nullptr;
  QToolButton          *m_linkBtn     = nullptr;
};

// ---- ZtoryAnimaticViewerPanel ----
// Standalone TPanel wrapper for ZtoryAnimaticViewer.
// Contains a QStackedWidget with two pages:
//   Page 0: ZtoryAnimaticViewer — animatic view (main xsheet, dedicated frame handle)
//   Page 1: ComboViewerPanel    — shot view (current sub-scene, drawing toolbar)
// A toggle button at the top switches between modes and auto-opens/closes the sub-scene.
class ComboViewerPanel;
class ZtoryAnimaticViewerPanel : public TPanel {  // forward-decl already above
  Q_OBJECT
public:
  ZtoryAnimaticViewerPanel(QWidget *parent = nullptr);

public slots:
  // Triggered by ZtoryModel::shotActivatedForViewing — switch to shot view page.
  // Caller has already opened the sub-scene; this only switches the stack page.
  void enterShotMode(int col);
  // Triggered by m_toggleBtn click or ZtoryModel::returnToViewerMainRequested —
  // close sub-scene and switch back to animatic view page.
  void returnToAnimaticMode();

protected:
  void showEvent(QShowEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *e) override;

private:
  void restoreAnimaticButtons();
  void updateTitle();   // "Animatic - <scene>" in animatic mode, "SQxx_SHxxx" in shot mode
  ZtoryAnimaticViewer *m_viewer     = nullptr;
  ComboViewerPanel    *m_shotViewer = nullptr;
  QStackedWidget      *m_stack      = nullptr;
  QWidget             *m_topBar     = nullptr;  // back btn + link btn bar
  QToolButton         *m_backBtn    = nullptr;
  QToolButton         *m_linkBtn    = nullptr;
  // Overlay buttons (symmetry, perspective, safe area, field guide):
  // added to the panel title bar but only visible in shot mode.
  QList<QWidget *>     m_overlayButtons;
  // Single source of truth for the shared view-mode button set (Camera Stand /
  // Camera View). Tracked here and re-applied to whichever viewer becomes active
  // on switch, so the button and the visible view never desync.
  int m_currentRefMode = 3 /*SceneViewer::CAMERA_REFERENCE*/;
};

// ---- ZtoryAnimaticPanel ----
// Timeline panel: toolbar + ruler + track + audio tracks
class ZtoryAnimaticPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryAnimaticPanel(QWidget *parent = nullptr, bool switchEnabled = false);
  void refreshFromScene();
protected:
  void showEvent(QShowEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *e) override;

// Timeline edit operations — also called by ZtoryMonitorPanel for trim sync.
public slots:
  void onShotDurationChanged(int col, int newF1);
  void onRollEdit(int colA, int newDurA, int colB, int newDurB);
  void onTransitionChanged(int colA, int colB, int frames);
  void onRazorRequested(int col, int splitFrame);
  void onShotMoved(int col, int newStartFrame);
  void onMergeWithNext(int col);
  // Shot edit operations — forwarded from ZtoryMonitorPanel toolbar.
  void onCopyShots();
  void onCutShots();
  void onPasteShots();
  void onDeleteShots();
  void onCloneShots();
  void onShotDoubleClicked(int col);
  void onReturnToMain();
  void onMergeShots();
  void onAddShot();

private slots:
  void onShotClicked(int col);
  void resequenceXsheet();
  void onZoomChanged(double ppf);
  void onFitAll();
  void onMatchSubsceneDuration(int col);
  void onFrameChanged(int frame);
  void onAudioRazorRequested(int col, int frame);
  void onSegmentDroppedOutside(int srcCol, int origR0, int origR1, int dragOffset, QPoint globalPos);
  void showShotTimeline();
  void showAnimaticTimeline();

public:
  void refreshAudioTracks();
  void updateTrackWidths();
  void updateCutFrames();
  // Snap (magnet): rebuild the target frame list (shot boundaries, playhead,
  // audio-segment edges) and push it + the enabled state to every track.
  void updateSnapFrames();

protected:
  void contextMenuEvent(QContextMenuEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;

private:
  // ── Clipboard per Cmd+C/X/V ──────────────────────────────────────────────
  // Clipboard is now shared with StoryboardPanel via ZtoryModel::sharedClip().

  QStackedWidget       *m_outerStack    = nullptr;
  TimelineViewerPanel  *m_timelinePanel = nullptr;
  ZtoryAnimaticRuler *m_ruler;
  ZtoryAnimaticTrack *m_track;
  QWidget *m_scrollContent = nullptr;
  QVBoxLayout *m_scrollLay = nullptr;
  QScrollArea *m_scroll    = nullptr;
  QList<ZtoryAudioTrack *> m_audioTracks;
  QSlider     *m_zoomSlider  = nullptr;
  QToolButton *m_fitAllBtn      = nullptr;
  QToolButton *m_timecodeBtn    = nullptr;
  QToolButton *m_snapBtn        = nullptr;
  bool m_snapEnabled = true;   // magnet on by default
  bool m_audioLinked = true;
  double m_ppf = 8.0;
  // ── Panning state (Space+drag or Middle-mouse drag) ───────────────────────
  bool   m_spaceDown       = false;
  bool   m_panning         = false;
  QPoint m_panAnchorGlobal;
  int    m_panAnchorScrollX = 0;
  bool m_switchEnabled  = false;  // true in ZtoryAnimaticT (ZTORYC T room)
  bool m_refreshing = false;      // re-entrancy guard for refreshFromScene
  bool m_refreshingAudio = false; // re-entrancy guard for refreshAudioTracks
  // While >= 0, every refreshFromScene() restores this horizontal scroll value
  // so an edit (e.g. razor) that triggers a cascade of deferred refreshes does
  // not let the view jump.  Set before the edit, cleared once the cascade ends.
  int  m_restoreScrollX = -1;
  // Auto-match: when ON, onMatchSubsceneDuration fires on every xsheetChanged
  // while inside a sub-scene (debounced 300ms, re-entrancy guarded).
  // State lives in ZtoryModel::autoMatch() so ZtoryPanelNavigator can mirror it.
  QToolButton *m_autoMatchBtn   = nullptr;
  QTimer      *m_autoMatchTimer = nullptr;
  bool         m_autoMatchBusy  = false;
  int          m_autoMatchCol   = -1;
  // Per-column mute/solo/lock state — persists across refreshAudioTracks() rebuilds
  QMap<int, bool> m_colMuted;
  QSet<int>       m_colSolo;
  QMap<int, bool> m_colLocked;
  // Apply effective mute/solo volumes to all audio columns and rebuild soundtrack
  void applyMuteSolo();
  // Restore muted/solo/locked state onto freshly-created track widgets
  void restoreTrackStates();
};

#endif
