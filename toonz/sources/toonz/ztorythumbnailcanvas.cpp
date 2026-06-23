#include "ztorythumbnailcanvas.h"

#include "ztoryshotops.h"   // cameraAspect
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/mypaint.h"

#include "tpixelutils.h"   // RGB2HSV, PixelConverter

#include "toonzqt/gutil.h"  // rasterToQImage

#include <QPainter>
#include <QFont>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QWheelEvent>
#include <QDir>
#include <QFileInfo>

//=============================================================================

ZtoryThumbnailCanvas::ZtoryThumbnailCanvas(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);

  // Panel boxes follow the scene camera aspect so the thumbnail grid matches the
  // framing used by the Board/animatic (e.g. a square camera → square panels).
  // Width is kept fixed; height is derived from the camera aspect.
  double aspect =
      ZtoryShotOps::cameraAspect(TApp::instance()->getCurrentScene()->getScene());
  if (aspect > 0.0) m_boxH = m_boxW / aspect;

  m_ras = TRaster32P((int)gridW(), (int)gridH());
  m_ras->fill(TPixel32::White);
}

ZtoryThumbnailCanvas::~ZtoryThumbnailCanvas() {
  delete m_brush;
  delete m_style;
}

//=============================================================================
// Tool / palette
//=============================================================================

void ZtoryThumbnailCanvas::setPreset(const Preset &p) {
  m_brushFile = p.brushFile;
  m_opacity   = p.opacity;
  m_eraser    = p.eraser;
  ensureStyle();
}

void ZtoryThumbnailCanvas::setColor(const TPixel32 &color) { m_color = color; }

void ZtoryThumbnailCanvas::setSizeModifier(double logMod) { m_sizeMod = logMod; }

QString ZtoryThumbnailCanvas::resolveBrushFile(const QString &relPath) {
  // Absolute paths (e.g. a brush added via the file dialog) pass through, so the
  // added brush both draws with its own style and shows its preview icon.
  if (QFileInfo(relPath).isAbsolute())
    return QFileInfo::exists(relPath) ? relPath : QString();
  for (const TFilePath &dir : TMyPaintBrushStyle::getBrushesDirs()) {
    QString root = QString::fromStdWString(dir.getWideString());
    QString full = root + "/" + relPath;
    if (QFileInfo::exists(full)) return full;
  }
  return QString();
}

void ZtoryThumbnailCanvas::ensureStyle() {
  if (m_style && m_styleFile == m_brushFile) return;
  QString full = resolveBrushFile(m_brushFile);
  if (full.isEmpty()) return;  // keep previous style if the file is missing
  delete m_style;
  m_style     = new TMyPaintBrushStyle(TFilePath(full.toStdWString()));
  m_styleFile = m_brushFile;
}

//=============================================================================
// Grid
//=============================================================================

void ZtoryThumbnailCanvas::addRow() {
  const int oldH  = m_ras->getLy();
  m_rows += 1;
  const int newH   = (int)gridH();
  const int addedH = newH - oldH;
  TRaster32P nr((int)gridW(), newH);
  nr->fill(TPixel32::White);
  nr->copy(m_ras, TPoint(0, addedH));  // keep existing content at the same world Y
  m_ras = nr;
  update();
}

//=============================================================================
// Selection
//=============================================================================

void ZtoryThumbnailCanvas::setSelectMode(bool on) {
  if (m_selectMode == on) return;
  m_selectMode = on;
  setCursor(on ? Qt::PointingHandCursor : Qt::ArrowCursor);
  update();
}

void ZtoryThumbnailCanvas::clearSelection() {
  if (m_selection.isEmpty()) return;
  m_selection.clear();
  emit selectionChanged(0);
  update();
}

int ZtoryThumbnailCanvas::panelAtWorld(const QPointF &world) const {
  if (world.x() < 0 || world.y() < 0 || world.x() >= gridW() ||
      world.y() >= gridH())
    return -1;
  int col = (int)(world.x() / m_boxW);
  int row = (int)(world.y() / m_boxH);
  if (col < 0 || col >= m_cols || row < 0 || row >= m_rows) return -1;
  return row * m_cols + col;
}

QRectF ZtoryThumbnailCanvas::panelWorldRect(int index) const {
  if (index < 0 || index >= m_cols * m_rows) return QRectF();
  int col = index % m_cols;
  int row = index / m_cols;
  return QRectF(col * m_boxW, row * m_boxH, m_boxW, m_boxH);
}

//=============================================================================
// View transform
//=============================================================================

QPointF ZtoryThumbnailCanvas::worldToWidget(const QPointF &w) const {
  return QPointF(w.x() * m_zoom + m_pan.x(), w.y() * m_zoom + m_pan.y());
}

QPointF ZtoryThumbnailCanvas::widgetToWorld(const QPointF &p) const {
  return QPointF((p.x() - m_pan.x()) / m_zoom, (p.y() - m_pan.y()) / m_zoom);
}

TPointD ZtoryThumbnailCanvas::widgetToRaster(const QPointF &widgetPos) const {
  const QPointF w = widgetToWorld(widgetPos);
  return TPointD(w.x(), gridH() - w.y());  // flip Y to bottom-up raster origin
}

void ZtoryThumbnailCanvas::zoomAt(const QPointF &widgetAnchor, double factor) {
  const QPointF worldAnchor = widgetToWorld(widgetAnchor);
  m_zoom = qBound(0.1, m_zoom * factor, 8.0);
  m_pan = widgetAnchor - QPointF(worldAnchor.x() * m_zoom, worldAnchor.y() * m_zoom);
  update();
}

//=============================================================================
// Stroke lifecycle
//=============================================================================

void ZtoryThumbnailCanvas::beginStroke(const QPointF &widgetPos, double pressure) {
  if (m_selectMode) return;  // selection mode suspends drawing (incl. tablet)
  ensureStyle();
  if (!m_style || !m_ras) return;
  const QPointF w = widgetToWorld(widgetPos);
  if (w.x() < 0 || w.y() < 0 || w.x() > gridW() || w.y() > gridH()) return;

  // Erasers paint white onto the opaque page; brushes use the chosen ink.
  const TPixel32 ink = m_eraser ? TPixel32(255, 255, 255, 255) : m_color;
  TPixelD c = PixelConverter<TPixelD>::from(ink);
  double h = 0.0, s = 0.0, v = 0.0;
  RGB2HSV(c.r, c.g, c.b, &h, &s, &v);

  mypaint::Brush brush;
  brush.fromBrush(m_style->getBrush());
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, (float)(h / 360.0));
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, (float)s);
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, (float)v);
  // Always paint (never MyPaint-erase): erasers are modelled as white paint.
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_ERASER, 0.0);
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE,
                     brush.getBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE) *
                         (float)m_opacity);
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                     brush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC) +
                         (float)m_sizeMod);

  delete m_brush;
  m_brush    = new MyPaintToonzBrush(m_ras, *this, brush);
  m_stroking = true;
  m_brush->beginStroke();
  m_timer.restart();
  m_brush->strokeTo(widgetToRaster(widgetPos), pressure, 0.0, 0.0, 0.0);
  update();
}

void ZtoryThumbnailCanvas::strokeTo(const QPointF &widgetPos, double pressure) {
  if (!m_stroking || !m_brush) return;
  double dtime = m_timer.nsecsElapsed() * 1e-9;
  m_timer.restart();
  m_brush->strokeTo(widgetToRaster(widgetPos), pressure, 0.0, 0.0, dtime);
  update();
}

void ZtoryThumbnailCanvas::endStroke() {
  if (!m_stroking || !m_brush) return;
  m_brush->endStroke();
  delete m_brush;
  m_brush    = nullptr;
  m_stroking = false;
  update();
}

//=============================================================================
// Events
//=============================================================================

void ZtoryThumbnailCanvas::tabletEvent(QTabletEvent *e) {
  switch (e->type()) {
  case QEvent::TabletPress:
    beginStroke(e->posF(), e->pressure());
    break;
  case QEvent::TabletMove:
    if (m_stroking) strokeTo(e->posF(), e->pressure());
    break;
  case QEvent::TabletRelease:
    endStroke();
    break;
  default:
    break;
  }
  e->accept();  // swallow so Qt does not synthesize duplicate mouse events
}

void ZtoryThumbnailCanvas::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::MiddleButton) {
    m_panning    = true;
    m_lastPanPos = e->pos();
    setCursor(Qt::ClosedHandCursor);
    return;
  }
  if (e->button() == Qt::LeftButton) {
    if (m_selectMode) {
      int idx = panelAtWorld(widgetToWorld(e->localPos()));
      if (idx >= 0) {
        int pos = m_selection.indexOf(idx);
        if (pos >= 0)
          m_selection.remove(pos);   // toggle off → following panels renumber
        else
          m_selection.append(idx);   // toggle on  → appended at end of order
        emit selectionChanged(m_selection.size());
        update();
      }
      return;
    }
    beginStroke(e->localPos(), 0.5);
  }
}

void ZtoryThumbnailCanvas::mouseMoveEvent(QMouseEvent *e) {
  if (m_panning) {
    m_pan += e->pos() - m_lastPanPos;
    m_lastPanPos = e->pos();
    update();
    return;
  }
  if (m_stroking) strokeTo(e->localPos(), 0.5);
}

void ZtoryThumbnailCanvas::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() == Qt::MiddleButton) {
    m_panning = false;
    unsetCursor();
    return;
  }
  if (e->button() == Qt::LeftButton) endStroke();
}

void ZtoryThumbnailCanvas::wheelEvent(QWheelEvent *e) {
  const QPointF pos = e->position();
  if (e->modifiers() & Qt::ControlModifier) {
    const double factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    zoomAt(pos, factor);
  } else {
    const QPoint d = e->angleDelta();
    if (e->modifiers() & Qt::ShiftModifier)
      m_pan += QPointF(d.y(), 0);
    else
      m_pan += QPointF(d.x(), d.y());
    update();
  }
  e->accept();
}

//=============================================================================
// Paint
//=============================================================================

void ZtoryThumbnailCanvas::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(40, 40, 40));
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (!m_ras) return;

  const QPointF tl = worldToWidget(QPointF(0, 0));
  const QRectF target(tl, QSizeF(gridW() * m_zoom, gridH() * m_zoom));

  QImage img = rasterToQImage(m_ras, /*premultiplied=*/true, /*mirrored=*/true);
  p.drawImage(target, img);

  // Thin panel separators (overlay only — the surface itself is contiguous).
  QPen sep(QColor(170, 170, 170));
  sep.setCosmetic(true);
  p.setPen(sep);
  for (int c = 1; c < m_cols; ++c) {
    const double x = worldToWidget(QPointF(c * m_boxW, 0)).x();
    p.drawLine(QPointF(x, target.top()), QPointF(x, target.bottom()));
  }
  for (int r = 1; r < m_rows; ++r) {
    const double y = worldToWidget(QPointF(0, r * m_boxH)).y();
    p.drawLine(QPointF(target.left(), y), QPointF(target.right(), y));
  }

  QPen border(QColor(110, 110, 110));
  border.setCosmetic(true);
  p.setPen(border);
  p.drawRect(target);

  // Selection overlay: tint selected panels + a numbered badge showing the
  // export order. Always drawn (so the user keeps the order visible after
  // switching back to a brush), but only editable in Select mode.
  for (int i = 0; i < m_selection.size(); ++i) {
    const QRectF wr = panelWorldRect(m_selection[i]);
    if (wr.isNull()) continue;
    const QRectF sr(worldToWidget(wr.topLeft()),
                    QSizeF(wr.width() * m_zoom, wr.height() * m_zoom));
    p.fillRect(sr, QColor(224, 90, 0, 60));
    QPen selPen(QColor(224, 90, 0));
    selPen.setCosmetic(true);
    selPen.setWidth(2);
    p.setPen(selPen);
    p.drawRect(sr);

    // Order badge (1-based) in the top-left corner of the panel.
    const double bs = 20.0;
    QRectF badge(sr.left() + 3, sr.top() + 3, bs, bs);
    p.setBrush(QColor(224, 90, 0));
    p.setPen(Qt::NoPen);
    p.drawEllipse(badge);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(10.0);
    p.setFont(f);
    p.drawText(badge, Qt::AlignCenter, QString::number(i + 1));
    p.setBrush(Qt::NoBrush);
  }
}
