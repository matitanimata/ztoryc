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

class ZtoryThumbnailPanel final : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryThumbnailPanel(QWidget *parent = nullptr);

private:
  // Append a brush tool button (icon = the brush's MyPaint preview) and wire it.
  QToolButton *addBrushButton(const QString &relPath, double opacity,
                              bool eraser, const QString &tip);
  void selectColor(const QColor &c);  // set canvas ink + update swatch

  ZtoryThumbnailCanvas *m_canvas = nullptr;
  QButtonGroup *m_brushGroup     = nullptr;
  QHBoxLayout *m_brushBarLay     = nullptr;
  QToolButton *m_swatch          = nullptr;  // shows / picks current colour
  QList<ZtoryThumbnailCanvas::Preset> m_presets;  // indexed by brush button id
};
