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
#include <QPoint>
#include <QString>
#include <QVector>

class QTimer;
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
  // Linear panel indices (row * cols + col) in selection order.
  QVector<int> selection() const { return m_selection; }
  int gridCols() const { return m_cols; }
  int gridRows() const { return m_rows; }
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

  // Persistence: per-scene folder + the contiguous-raster PNG inside it.
  TFilePath persistDir() const;
  QString sceneKey() const;       // identity of the currently loaded scene
  void schedulePersistSave();     // (re)arm the debounced autosave

  // Linear panel index (row*cols+col) at a world point, or -1 if outside grid.
  int panelAtWorld(const QPointF &world) const;
  // World-space rectangle (top-left origin, y down) of a linear panel index.
  QRectF panelWorldRect(int index) const;

  // Grid (one contiguous raster; boxes are logical rectangles)
  TRaster32P m_ras;
  int m_cols    = 4;
  int m_rows    = 3;
  double m_boxW        = 480.0;  // world units == raster px (16:9 panel)
  double m_boxH        = 270.0;
  double m_boxAspect   = 16.0 / 9.0;  // last applied camera aspect (boxW/boxH)

  // View
  double m_zoom  = 1.0;
  QPointF m_pan  = QPointF(28.0, 28.0);
  bool m_panning = false;
  QPoint m_lastPanPos;

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
  QVector<int> m_selection;  // linear panel indices, in click (export) order

  // Persistence
  QTimer *m_saveTimer = nullptr;  // debounced autosave after edits
  QString m_persistKey;           // scene identity currently loaded from disk
};
