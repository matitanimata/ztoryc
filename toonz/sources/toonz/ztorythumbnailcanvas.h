#pragma once

// ZtoryThumbnailCanvas — custom raster sketch surface driven by the real Tahoma
// MyPaint brush engine, laid out as a grid of thumbnail panels.
//
// Why this exists: the SceneViewer draws one frame at a time and can't host many
// independently drawable cells at once. To let the user sketch freely across a
// grid of panels we use our own QWidget canvas backed by ONE contiguous
// TRaster32P, and drive MyPaintToonzBrush directly on it — decoupled from
// SceneViewer / TTool. Genuine .myb MyPaint brushes are used, so quality and
// pressure match the app.
//
// Contiguous surface (not per-box rasters) so a stroke can cross panel borders:
// the user can draw a horizontal/vertical panorama spanning several panels as
// one ad-hoc canvas. Panel borders are a thin overlay only. Export-to-board then
// just crops each panel's rectangle out of the big raster (trivial in raster).
//
// The palette is a set of presets (a .myb brush + colour + opacity). Erasers are
// modelled as white paint (the page is opaque white paper): normal eraser =
// opaque white, kneaded = low-opacity white that lightens gradually.

#include "traster.h"
#include "tcolorstyles.h"  // TColorStyleP — the brush style is ref-counted
#include "tpixel.h"

#include <QWidget>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QString>
#include <QTransform>
#include <QVector>

#include <map>
#include <utility>
#include <vector>

class QTouchEvent;
class QGestureEvent;
class QTimer;
class QScrollBar;
class QThreadPool;
class TFilePath;

#include "mypainttoonzbrush.h"  // RasterController, MyPaintToonzBrush

class TMyPaintBrushStyle;

class ZtoryThumbnailCanvas final : public QWidget, public RasterController {
  Q_OBJECT

public:
  // A palette brush: a .myb file (library-relative, e.g. "classic/pencil.myb"),
  // an opacity multiplier and whether it erases (paints white). Colour is held
  // separately on the canvas so the same brush can draw in any colour.
  // One entry of the brush palette.  Size and opacity live HERE, per brush,
  // not as one global setting: switching from a fine pencil to a broad eraser
  // and back used to hand the pencil the eraser's size, which is the opposite
  // of how every drawing app behaves and a daily annoyance when sketching.
  struct Preset {
    QString brushFile;
    double opacity = 1.0;
    bool eraser    = false;
    // Log-size modifier, same units as setSizeModifier(): 0 = the brush's own
    // default, the UI slider spans [-2 .. +4].
    double sizeMod = 0.0;
    // One of the five that ship with the room.  Replaceable — swap the tip for
    // whatever you prefer — but not removable: they are the fixed slots a
    // storyboard artist reaches for without looking, and an empty strip or a
    // shifting one is worse than a brush you never use.
    bool builtIn = false;
  };

  explicit ZtoryThumbnailCanvas(QWidget *parent = nullptr);
  ~ZtoryThumbnailCanvas() override;

  // The active brush IS a palette style.  Size, opacity and "this one erases"
  // are all MyPaint base settings, so the style carries the whole definition —
  // nothing is applied on top of it here any more, and a .tpl round-trips a
  // brush exactly as the artist tuned it.  The palette owns the style; this is
  // a borrowed pointer.
  void setBrushStyle(TMyPaintBrushStyle *style);
  TMyPaintBrushStyle *brushStyle() const { return m_style; }
  // Live brush radius in canvas pixels — the toolbar shows it as a diameter, so
  // the number next to the slider means something to whoever is drawing.
  double brushRadiusWorld() const;
  void setColor(const TPixel32 &color);     // ink colour (ignored by erasers)
  void setSizeModifier(double logMod);      // brush size (log2 units)
  void addRow();                            // grow the grid by one row

  // Paper-import blit (task 63): drop each imported cell (an upright, top-down
  // QImage) into its grid box, as ONE undoable edit, growing the grid to at
  // least minRows first so a following printed page has empty rows to land in.
  struct ImportedBlit {
    int row = 0;
    int col = 0;
    QImage image;
  };
  void applyImportedCells(const std::vector<ImportedBlit> &cells, int minRows);
  // Last grid row holding any ink, or -1 when the canvas is blank. Paper import
  // appends after it, so scanning order — not the sheet's printed page number —
  // decides where a sheet lands (blank sheets are meant to be photocopied, so
  // every copy carries the same page code).
  int lastNonEmptyRow() const;
  // Scroll (and zoom out if needed) so `row` is on screen. Imported sheets land
  // BELOW whatever is already drawn, which on a tall grid is off-screen — with
  // no view change the import looks like it did nothing.
  void revealRow(int row);
  // The whole contiguous surface as a top-down QImage (world orientation).
  // Used to print the drawn thumbnails; panoramas stay seamless because the
  // printed grid is contiguous too.
  QImage canvasImage() const;

  // --- Panel selection (for export-to-board) -------------------------------
  // In Select mode a left click toggles a panel's membership in the ordered
  // selection (click order == export order); drawing is suspended.
  void setSelectMode(bool on);
  bool isSelectMode() const { return m_selectMode; }
  void clearSelection();

  // --- Transform tool (raster selection: move / copy / scale / rotate) ------
  // A marquee lifts a rectangular region into a floating buffer that can be
  // dragged, scaled (corner handles) and rotated (top handle), then committed
  // back into the canvas. Free across the whole contiguous surface.
  void setTransformMode(bool on);
  bool isTransformMode() const { return m_xformMode; }
  void setLassoMode(bool on) { m_lassoMode = on; }  // freehand vs rectangular
  void commitFloat();   // bake the floating selection into the canvas
  void cancelFloat();   // drop it (restoring the lifted pixels if it was a move)
  void deleteFloat();   // drop it and leave the source cleared
  void copyFloat();     // current selection → internal clipboard
  void pasteFloat();    // clipboard → a new floating selection
  bool hasFloat() const { return !m_floatImg.isNull(); }
  // Region top-left linear indices (row * cols + col) in selection order. A
  // region is a rectangular block of boxes merged into one logical panel (or a
  // single box if unmerged); see m_merges.
  QVector<int> selection() const { return m_selection; }
  int gridCols() const { return m_cols; }
  int gridRows() const { return m_rows; }
  // Span (in boxes, w×h) of the region whose top-left box is `index`.
  QSize panelSpan(int index) const;
  // Merge the current rectangular selection into one panorama panel, or split
  // the selected merge(s) back into boxes. No-op if the selection is neither.
  void toggleMergeSelection();
  // True if the panel's raster region has no ink (all white) — empty panels are
  // not selectable and are skipped by export-to-board.
  bool isPanelEmpty(int index) const;
  // Crop a panel's region out of the contiguous surface and resample it to
  // outRes (the scene camera resolution). Returned raster is independent of
  // m_ras. Null if the index is out of range.
  // `onWhite` false keeps the surface's alpha, so an exported panel can sit
  // over a background instead of being an opaque sheet.  True flattens it onto
  // white, which is what a storyboard panel has always been.
  TRaster32P panelRaster(int index, const TDimension &outRes,
                         bool onWhite = true) const;

signals:
  // Emitted whenever the ordered selection changes (count = panels selected).
  void selectionChanged(int count);

private slots:
  // Live reaction to a scene/camera change: if the camera aspect changed (e.g.
  // edited in Camera Settings while this room is open) re-derive the panel box
  // height and rescale the contiguous raster so existing drawings stay aligned
  // with their panels.
  void onSceneChanged();
  // Load this scene's saved canvas from disk (on scene switch / panel open);
  // clears the canvas if the scene has none.  Save is debounced after edits.
  void persistLoad();
  void persistSave();
  // The worker finished writing: release the slot and honour any edit that
  // arrived while it was busy.
  void onPersistSaveFinished();

public:

  // Resolve a library-relative brush path ("classic/pencil.myb") to an absolute
  // path; "" if not found. Used by the palette to locate brush preview icons.
  static QString resolveBrushFile(const QString &relPath);

protected:
  void paintEvent(QPaintEvent *) override;
  void tabletEvent(QTabletEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;
  void keyPressEvent(QKeyEvent *) override;
  // App-wide filter: the transform shortcuts (Del/Esc/Enter/Cmd-C/V) must work
  // even when keyboard focus sits on a toolbar button, not the canvas.
  bool eventFilter(QObject *obj, QEvent *ev) override;

  // Tocco e gesti. Senza questi, su uno schermo touch (Surface, iPad in
  // remoto, portatili Windows) un dito diventa un click SINTETIZZATO e il
  // canvas ci disegna sopra: chi prova a spostare la tela si ritrova una riga
  // col pennello attivo. Segnalato da un utente Surface il 2026-09-18.
  // La logica rispecchia quella gia' collaudata di SceneViewer
  // (sceneviewerevents.cpp): un dito sullo schermo touch, due sul trackpad.
  bool event(QEvent *e) override;
  void touchEvent(QTouchEvent *e, int type);
  void gestureEvent(QGestureEvent *e);

  bool m_gestureActive = false;   // un gesto e' in corso: il mouse si ignora
  bool m_touchActive   = false;
  bool m_touchPanning  = false;
  int  m_touchDevice   = 0;       // QTouchDevice::DeviceType
  QPointF m_firstPanPoint;
  bool   m_zooming     = false;
  double m_scaleFactor = 0.0;
  // Quante dita ha visto QUESTO tocco (il massimo raggiunto). Serve alle
  // gesture di undo/redo: 2 dita = annulla, 3 = ripeti, secondo le preferenze.
  // Il valore 100 e' il modo di SceneViewer per dire «questo tocco ha gia'
  // fatto qualcosa (spostamento, zoom), quindi NON contarlo come tocco».
  int m_touchPoints = 0;
  QPointF m_undoPoint;       // riferimento per il trascinamento a tre dita
  QElapsedTimer m_touchClock;

private:
  // Brush
  void ensureStyle();                                  // (re)load m_style
  void beginStroke(const QPointF &widgetPos, double pressure);
  void strokeTo(const QPointF &widgetPos, double pressure);
  void endStroke();

  // Layout / view transform (world == raster px, widget == on-screen px).
  QPointF worldToWidget(const QPointF &w) const;
  QPointF widgetToWorld(const QPointF &p) const;
  double gridW() const { return m_cols * m_boxW; }
  double gridH() const { return m_rows * m_boxH; }
  TPointD widgetToRaster(const QPointF &widgetPos) const;
  void zoomAt(const QPointF &widgetAnchor, double factor);
  void updateScrollBars();          // sync the side bars to pan/zoom/grid
  void updateToolCursor();          // brush(blank)/select/transform per mode

  // Persistence: per-scene folder + the contiguous-raster PNG inside it.
  TFilePath persistDir() const;
  QString sceneKey() const;       // identity of the currently loaded scene
  // Storage is BANDED: the canvas goes to disk as horizontal bands of
  // kRowsPerBand grid rows, and a save re-encodes only the bands that changed.
  // A single image does not scale — at feature length (1080 rows) it is a 2.1 GB
  // raster, ~11 s of PNG per save and a 208 ms copy on the UI thread — while a
  // band is ~10 MB and ~52 ms whatever the storyboard's length.
  static const int kRowsPerBand = 5;
  int bandCount() const;
  // Conservative by design: this marks the WHOLE canvas dirty.  There are a
  // dozen paths that write to the raster (rows, imported cells, float paste and
  // lift, merges, camera reflow, undo restore, load) and a forgotten one would
  // mean a change that is never written — no crash, no message, noticed the day
  // after.  Costing performance is recoverable, losing drawings is not: only the
  // brush stroke, whose touched area is known exactly, takes the cheap path.
  void schedulePersistSave();     // (re)arm the debounced autosave — whole canvas
  void schedulePersistSave(const QRect &rasterRect);  // only the bands touched
  void markAllBandsDirty();
  void markBandsDirty(const QRect &rasterRect);
  void schedulePersistSaveTimer();  // arm the debounce, leaving the flags alone
  void loadMerges(const QString &dirStr);
  void bandRasterRange(int b, int ly, int &y0, int &y1) const;

  // Linear panel index (row*cols+col) at a world point, or -1 if outside grid.
  int panelAtWorld(const QPointF &world) const;
  // World-space rectangle (top-left origin, y down) of a region (top-left index).
  QRectF panelWorldRect(int index) const;

  // Merge support. A region is a w×h block of boxes; m_merges holds the >1-box
  // ones, in BOX coords QRect(col, row, wspan, hspan). Boxes never overlap two
  // merges. A region is named by its top-left box's linear index.
  int mergeIndexAt(int col, int row) const;     // merge covering box, or -1
  int regionIndexOf(int boxIndex) const;        // → region's top-left index
  QRect regionBoxRect(int topLeftIndex) const;  // region rect in box coords

  // Reflow a raster laid out at oldBoxH into the current grid (m_cols/m_rows at
  // m_boxH). Works per region (merged pans stay whole), scales each region
  // uniformly (never deformed) and re-centres it. Used both on a live camera
  // change and when loading a scene whose canvas was saved at another aspect.
  TRaster32P reanchorRaster(const TRaster32P &oldRas, double oldBoxH,
                            double newBoxH) const;

  // --- Transform tool internals --------------------------------------------
  void liftFloat(const QRectF &worldRect, bool copy);  // marquee → floating buf
  void liftFloatLasso(const QVector<QPointF> &worldPath, bool copy);
  bool wantsTransformKey(QKeyEvent *e) const;  // would consume, no side effects
  bool handleTransformKey(QKeyEvent *e);  // → true if the key was consumed

  // --- Undo / redo ----------------------------------------------------------
  // A snapshot is either a full-canvas copy (structural edits: resize, paste,
  // transform…) or, for brush strokes, just the tiles the stroke touched.
  // Cloning the whole surface per stroke cost ~31 MB on a 4x15 grid — the hitch
  // at stroke start, and up to 500 MB of history.
  struct Patch {
    TPoint pos;       // top-left of the tile in raster coords
    TRaster32P before;
  };
  struct Snapshot {
    // Three shapes, cheapest first:
    //   geometryOnly  — only the row count changed.  Adding a row does not
    //                   destroy a pixel (it appends a blank band at the world
    //                   bottom), so undoing it needs no image at all: the old
    //                   code cloned the WHOLE canvas for it, which is what made
    //                   the app slow down the moment rows were added.
    //   patches       — a stroke: the 256x256 tiles it touched.
    //   ras           — everything else (paste, transform, reflow): full copy.
    bool geometryOnly = false;
    TRaster32P ras;   // null when this is a stroke (patches) snapshot
    std::vector<Patch> patches;
    int cols, rows;
    QVector<QRect> merges;
    double boxAspect;  // camera aspect the raster was laid out at — restored
                       // together with it so undoing across a camera-format
                       // change never squishes the drawings into a stale grid
    // Floating Transform selection captured with this snapshot (null image = no
    // float). Lets undo restore a deleted or replaced float — not just the
    // raster — so Cmd+Z after Del re-floats the lifted/pasted drawing.
    QImage floatImg;
    QPointF floatCenter;
    double floatScale = 1.0;
    double floatAngle = 0.0;
    bool floatWasMove = false;
    QRect floatSrcRect;
  };
  void pushUndo();   // snapshot current state before a mutating edit
  void undo();
  void redo();
  // Annulla/ripeti come li intende QUESTA room: la pila del canvas se c'e',
  // altrimenti il comando globale. Stessa politica di handleUndoKey(), cosi'
  // la gesture e la tastiera fanno la stessa cosa — che e' l'unico modo per
  // cui l'utente non debba sapere quale delle due sta usando.
  void gestureUndo();
  void gestureRedo();
  void restoreSnapshot(const Snapshot &s);
  // Apply a geometryOnly snapshot: resize the surface to its row count, keeping
  // the drawings at the same world Y (grow and shrink both happen at the world
  // bottom, exactly as addRow() does it).
  void restoreGeometry(const Snapshot &s);
  Snapshot makeMetaSnapshot() const;      // grid metadata, no pixels
  void applyPatches(const std::vector<Patch> &patches);
  std::vector<Patch> capturePatchesAt(const std::vector<Patch> &like) const;
  void trimHistory();

  // Stroke tile recording (copy-on-write, driven by askWrite()).
  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;
  void beginStrokeRecording();
  void endStrokeRecording();
  bool m_recordingStroke = false;
  std::map<std::pair<int, int>, TRaster32P> m_strokeTiles;  // tile key → before
  bool handleUndoKey(QKeyEvent *e);  // Cmd/Ctrl+Z, Cmd/Ctrl+Shift+Z
  QTransform floatLocalToWorld() const;   // base-image px → world coords
  QPointF floatHandleWorld(int h) const;  // h: 0..3 corners, 4 = rotate handle
  int floatHandleAt(const QPointF &widgetPos) const;  // hit-test → handle, or -1
  void paintFloat(QPainter &p);

  // Grid (one contiguous raster; boxes are logical rectangles)
  static constexpr int kDefaultCols = 4;  // a fresh canvas starts 4×4 so the
  static constexpr int kDefaultRows = 4;  // grid is square-ish vs the camera box

  TRaster32P m_ras;
  int m_cols    = kDefaultCols;
  int m_rows    = kDefaultRows;
  double m_boxW        = 480.0;  // world units == raster px (16:9 panel)
  double m_boxH        = 270.0;
  double m_boxAspect   = 16.0 / 9.0;  // last applied camera aspect (boxW/boxH)

  // View
  double m_zoom  = 1.0;
  QPointF m_pan  = QPointF(28.0, 28.0);
  bool m_panning = false;
  QPoint m_lastPanPos;
  QScrollBar *m_hbar = nullptr;     // side scrollbars (shown only when needed)
  QScrollBar *m_vbar = nullptr;
  bool m_syncingBars = false;       // guard against scrollbar↔pan feedback
  QPointF m_cursorWidget;           // last mouse pos (brush cursor)
  bool m_cursorOnCanvas = false;
  double m_brushBaseRadiusLog = 2.0;  // cached RADIUS_LOGARITHMIC of the brush

  // Active tool
  // OWNING reference, not a borrowed pointer.  The style belongs to the brush
  // palette, and the Style Editor REPLACES the style when it applies a change
  // (setOldStyleToStyle + notifyColorStyleChanged): a raw pointer here became
  // dangling the moment a parameter was edited, and the very next repaint read
  // the radius out of freed memory — SIGSEGV in paintEvent, which is exactly
  // what happened on 2026-09-16 (Crash-20260916-010115.log).  Holding a
  // reference keeps the old object alive until setBrushStyle() is handed the
  // new one by onBrushStyleEdited().
  TColorStyleP m_styleRef;
  TMyPaintBrushStyle *m_style = nullptr;  // m_styleRef, already downcast
  QString m_styleFile;                     // file currently loaded in m_style
  QString m_brushFile = "classic/pencil.myb";
  TPixel32 m_color    = TPixel32(0, 0, 0, 255);
  double m_opacity    = 1.0;
  bool m_eraser       = false;
  double m_sizeMod    = 0.0;

  // Stroke state
  MyPaintToonzBrush *m_brush = nullptr;
  // Which bands still have to reach the disk.  Sized lazily from bandCount().
  std::vector<bool> m_bandDirty;
  // Union of every dab of the stroke in progress, in raster coordinates: the
  // one case where we know exactly what changed.  m_strokeDirty cannot serve —
  // it is cleared at every repaint, so it only ever holds the last few dabs.
  QRect m_strokeBounds;
  bool m_stroking            = false;
  QElapsedTimer m_timer;

  // Selection state
  bool m_selectMode = false;
  QVector<int> m_selection;  // region top-left indices, in click (export) order
  QVector<QRect> m_merges;   // merged regions in box coords (col,row,wspan,hspan)

  // Transform tool state
  bool m_xformMode = false;
  bool m_lassoMode = false;             // freehand selection instead of a rect
  bool m_marqueeing = false;            // dragging the rubber-band rectangle
  QPointF m_marqueeStart, m_marqueeCur; // world coords during the marquee
  QVector<QPointF> m_lassoPath;         // world points of the freehand path
  QImage m_clip;                        // internal copy/paste buffer
  QImage m_floatImg;                    // lifted pixels (world orientation)
  QPointF m_floatCenter;                // world center of the floating buffer
  double m_floatScale = 1.0;            // uniform scale about the center
  double m_floatAngle = 0.0;            // rotation (radians) about the center
  QRect m_floatSrcRect;                 // original lifted world rect (for cancel)
  bool m_floatWasMove = false;          // source was cleared (vs. copy)
  int m_floatDrag = -1;                 // active handle: -1 none, 0..3 scale, 4 rot, 5 move
  QPointF m_dragStartWorld;             // mouse-down world pos
  double m_dragStartScale = 1.0, m_dragStartAngle = 0.0;
  QPointF m_dragStartCenter;

  // What the brush has touched since the last repaint, in RASTER coordinates
  // (bottom-up).  strokeTo() used to call update() with no rectangle, so every
  // tablet event — a hundred-odd per second — repainted the whole widget; this
  // narrows it to the dab.  Filled by askWrite(), which the brush already calls
  // before writing, so the information costs nothing to collect.
  QRect m_strokeDirty;

  // Raster rect -> widget rect, for the partial repaint above.
  QRect rasterRectToWidget(const QRect &r) const;
  // Where the painted brush circle sits, so a partial repaint can carry it.
  QRect cursorRect(const QPointF &widgetPos) const;
  QPointF m_cursorPrev;  // circle position at the previous stroke sample

  // Persistence
  QTimer *m_saveTimer = nullptr;  // debounced autosave after edits
  QString m_persistKey;           // scene identity currently loaded from disk
  // The autosave re-encodes the WHOLE canvas, and that cost grows with every
  // page: measured 71 ms at 4x4 but 270 ms at 4x26 (1920x7020) and 515 ms at
  // 4x52 — on an M4, so more on a slower machine.  On the UI thread it lands as
  // a freeze exactly where the user pauses and puts the pen back down, which is
  // the reported "the more thumbs you draw the more the stroke lags".  So the
  // encode and the write happen on a worker; the UI thread only takes a
  // detached copy of the surface (a memcpy, ~10 ms at this size).
  QThreadPool *m_savePool = nullptr;  // exactly one worker, so saves serialise
  bool m_saveRunning = false;         // a worker is encoding right now
  bool m_saveQueued  = false;         // an edit arrived while it was running

  // Undo / redo
  std::vector<Snapshot> m_undo, m_redo;
  // Vero fra un persistLoad() e la prima riallineata alla camera della scena.
  // Quella riallineata NON e' una modifica dell'utente: e' la tela che si mette
  // in pari con la scena appena aperta, e non deve finire nella pila
  // dell'annullamento (vedi onSceneChanged).
  bool m_awaitingCameraCatchUp = false;
};
