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

#include <vector>

class QTimer;
class QScrollBar;
class TFilePath;

#include "mypainttoonzbrush.h"  // RasterController, MyPaintToonzBrush

class TMyPaintBrushStyle;

class ZtoryThumbnailCanvas final : public QWidget, public RasterController {
  Q_OBJECT

public:
  // A palette brush: a .myb file (library-relative, e.g. "classic/pencil.myb"),
  // an opacity multiplier and whether it erases (paints white). Colour is held
  // separately on the canvas so the same brush can draw in any colour.
  struct Preset {
    QString brushFile;
    double opacity;
    bool eraser;
  };

  explicit ZtoryThumbnailCanvas(QWidget *parent = nullptr);
  ~ZtoryThumbnailCanvas() override;

  void setPreset(const Preset &p);          // switch active brush
  void setColor(const TPixel32 &color);     // ink colour (ignored by erasers)
  void setSizeModifier(double logMod);      // brush size (log2 units)
  void addRow();                            // grow the grid by one row

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
  TRaster32P panelRaster(int index, const TDimension &outRes) const;

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
  double brushRadiusWorld() const;  // current brush radius in world px (cursor)
  void updateToolCursor();          // brush(blank)/select/transform per mode

  // Persistence: per-scene folder + the contiguous-raster PNG inside it.
  TFilePath persistDir() const;
  QString sceneKey() const;       // identity of the currently loaded scene
  void schedulePersistSave();     // (re)arm the debounced autosave

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
  bool handleTransformKey(QKeyEvent *e);  // → true if the key was consumed

  // --- Undo / redo (full-canvas snapshots) ---------------------------------
  struct Snapshot {
    TRaster32P ras;
    int cols, rows;
    QVector<QRect> merges;
    double boxAspect;  // camera aspect the raster was laid out at — restored
                       // together with it so undoing across a camera-format
                       // change never squishes the drawings into a stale grid
  };
  void pushUndo();   // snapshot current state before a mutating edit
  void undo();
  void redo();
  void restoreSnapshot(const Snapshot &s);
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
  TMyPaintBrushStyle *m_style = nullptr;
  QString m_styleFile;                     // file currently loaded in m_style
  QString m_brushFile = "classic/pencil.myb";
  TPixel32 m_color    = TPixel32(0, 0, 0, 255);
  double m_opacity    = 1.0;
  bool m_eraser       = false;
  double m_sizeMod    = 0.0;

  // Stroke state
  MyPaintToonzBrush *m_brush = nullptr;
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

  // Persistence
  QTimer *m_saveTimer = nullptr;  // debounced autosave after edits
  QString m_persistKey;           // scene identity currently loaded from disk

  // Undo / redo
  std::vector<Snapshot> m_undo, m_redo;
};
