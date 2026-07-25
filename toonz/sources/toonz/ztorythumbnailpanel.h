#pragma once

// ZtoryThumbnailPanel — Thumbnail room: a palette toolbar over a grid sketch
// canvas (ZtoryThumbnailCanvas). See ztorythumbnailcanvas.h for the drawing
// architecture (custom raster surface + real MyPaint brushes).
//
// This panel must NOT embed a Tahoma drawing viewer: a ComboViewerPanel is
// itself a TPanel, and nesting TPanels breaks Tahoma's active-viewer routing.

#include "pane.h"

#include <QWidget>
#include <QList>
#include <QString>

#include "ztorythumbnailcanvas.h"  // ZtoryThumbnailCanvas::Preset

class QToolButton;
class QButtonGroup;
class QHBoxLayout;
class QSpinBox;

class ZtoryThumbnailPanel final : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryThumbnailPanel(QWidget *parent = nullptr);

private:
  // Append a brush tool button (icon = the brush's MyPaint preview) and wire it.
  QToolButton *addBrushButton(const QString &relPath, double opacity,
                              bool eraser, const QString &tip);
  void selectColor(const QColor &c);  // set canvas ink + update swatch
  // Build one raster per selected panel (at scene camera res) and hand them to
  // ZtoryModel as a new multi-panel shot, then clear the selection.
  void exportSelectionToBoard();
  // Print the canvas grid to a multi-page A4 PDF (task 63). `withContent` prints
  // the thumbnails currently on the canvas (a contact sheet); otherwise a blank
  // grid to draw on by hand — the one meant to be photocopied and re-imported.
  void printPaperSheet(bool withContent);
  // Import a photographed/scanned sheet from an image file: de-warp, crop and
  // blit the cells back into the grid (task 63, phase 2).
  void importPaperSheetFromFile();
  // Same, shooting the sheet with a webcam / capture card from inside Ztoryc.
  void importPaperSheetFromCamera();
  // Run one photographed sheet through the pipeline and drop it into the grid
  // after whatever is already drawn. Shared by the file and camera paths, so
  // both behave identically. Returns the number of panels placed (0 on failure,
  // with the reason appended to `failed`).
  // `faint` accumulates cells that were skipped as blank but did carry very
  // light marks — a too-light sketch would otherwise vanish without a word.
  int importOneSheet(const QImage &photo, const QString &label,
                     QStringList &failed, int &faint);

  ZtoryThumbnailCanvas *m_canvas = nullptr;
  QButtonGroup *m_brushGroup     = nullptr;
  QHBoxLayout *m_brushBarLay     = nullptr;
  QToolButton *m_swatch          = nullptr;  // shows / picks current colour
  QSpinBox *m_shrinkSpin         = nullptr;  // export resolution divisor (1 = full)
  QList<ZtoryThumbnailCanvas::Preset> m_presets;  // indexed by brush button id
};
