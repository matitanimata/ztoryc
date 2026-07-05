#include "ztorythumbnailcanvas.h"

#include "ztoryshotops.h"   // cameraAspect
#include "tapp.h"
#include "trop.h"           // resample (raster rescale on camera-aspect change)
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"  // OVL_XSHLEVEL (persist folder resolution)
#include "toonz/mypaintbrushstyle.h"
#include "toonz/mypaint.h"

#include "tpixelutils.h"   // RGB2HSV, PixelConverter

#include "toonzqt/gutil.h"  // rasterToQImage / rasterFromQImage

#include <QPainter>
#include <QFont>
#include <QMouseEvent>
#include <QTabletEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QImage>
#include <QRegExp>
#include <QFile>
#include <QTextStream>
#include <QPolygonF>
#include <QLineF>
#include <QApplication>
#include <QScrollBar>
#include <QResizeEvent>

#include <cmath>

//=============================================================================

ZtoryThumbnailCanvas::ZtoryThumbnailCanvas(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);  // brush cursor follows the mouse without a button

  // Side scrollbars, shown only when the content overflows the viewport. They
  // drive m_pan; middle-drag pan keeps them in sync via updateScrollBars().
  m_hbar = new QScrollBar(Qt::Horizontal, this);
  m_vbar = new QScrollBar(Qt::Vertical, this);
  m_hbar->hide();
  m_vbar->hide();
  connect(m_hbar, &QScrollBar::valueChanged, this, [this](int v) {
    if (m_syncingBars) return;
    m_pan.setX(-v);
    update();
  });
  connect(m_vbar, &QScrollBar::valueChanged, this, [this](int v) {
    if (m_syncingBars) return;
    m_pan.setY(-v);
    update();
  });

  // Panel boxes follow the scene camera aspect so the thumbnail grid matches the
  // framing used by the Board/animatic (e.g. a square camera → square panels).
  // Width is kept fixed; height is derived from the camera aspect.
  double aspect =
      ZtoryShotOps::cameraAspect(TApp::instance()->getCurrentScene()->getScene());
  if (aspect > 0.0) {
    m_boxH      = m_boxW / aspect;
    m_boxAspect = aspect;
  }

  m_ras = TRaster32P((int)gridW(), (int)gridH());
  m_ras->fill(TPixel32::White);

  // React live to camera changes made from Camera Settings while this room is
  // open. xsheetChanged covers most camera edits; sceneChanged covers a scene
  // load/switch with a different camera.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &ZtoryThumbnailCanvas::onSceneChanged);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneChanged, this,
          &ZtoryThumbnailCanvas::onSceneChanged);

  // Persistence: debounced autosave after edits, reload on scene switch.
  m_saveTimer = new QTimer(this);
  m_saveTimer->setSingleShot(true);
  m_saveTimer->setInterval(700);
  connect(m_saveTimer, &QTimer::timeout, this,
          &ZtoryThumbnailCanvas::persistSave);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryThumbnailCanvas::persistLoad);
  // Transform-tool shortcuts must work even when a toolbar button holds focus.
  qApp->installEventFilter(this);
  // Load the scene that is already open when the panel is created.
  persistLoad();
  updateToolCursor();  // start in drawing mode → brush-circle cursor
}

ZtoryThumbnailCanvas::~ZtoryThumbnailCanvas() {
  // Flush any pending edit so closing the app never loses the canvas.
  if (m_saveTimer && m_saveTimer->isActive()) persistSave();
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
  // Cache the brush's base radius (log px) so the cursor circle can show its
  // real size without starting a stroke.
  mypaint::Brush b;
  b.fromBrush(m_style->getBrush());
  m_brushBaseRadiusLog =
      b.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
}

//=============================================================================
// Grid
//=============================================================================

void ZtoryThumbnailCanvas::addRow() {
  pushUndo();
  const int oldH  = m_ras->getLy();
  m_rows += 1;
  const int newH   = (int)gridH();
  const int addedH = newH - oldH;
  TRaster32P nr((int)gridW(), newH);
  nr->fill(TPixel32::White);
  nr->copy(m_ras, TPoint(0, addedH));  // keep existing content at the same world Y
  m_ras = nr;
  updateScrollBars();
  update();
  schedulePersistSave();
}

void ZtoryThumbnailCanvas::onSceneChanged() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  const double aspect = ZtoryShotOps::cameraAspect(scene);
  if (aspect <= 0.0) return;
  // Cheap guard: skip the (most common) changes that don't touch the camera.
  if (qAbs(aspect - m_boxAspect) < 1e-4) return;
  // Don't relayout mid-stroke; the next change will catch up once it ends.
  if (m_stroking) return;

  const double oldBoxH = m_boxH;
  const int oldH       = m_ras ? m_ras->getLy() : 0;
  if (oldH <= 0 || oldBoxH <= 0.0 || !m_ras) return;

  // Snapshot the pre-reshape canvas (raster + its aspect) so Cmd-Z reverts the
  // camera-format reflow cleanly instead of leaving a stale grid.
  pushUndo();
  m_boxAspect = aspect;
  m_boxH      = m_boxW / aspect;
  m_ras       = reanchorRaster(m_ras, oldBoxH, m_boxH);
  update();
}

TRaster32P ZtoryThumbnailCanvas::reanchorRaster(const TRaster32P &oldRas,
                                                double oldBoxH,
                                                double newBoxH) const {
  const int oldW = oldRas ? oldRas->getLx() : 0;
  const int oldH = oldRas ? oldRas->getLy() : 0;
  const int gw   = (int)gridW();
  const int newH = qMax(1, (int)(m_rows * newBoxH));
  const int bw   = (int)m_boxW;

  TRaster32P nr(gw, newH);
  nr->fill(TPixel32::White);
  if (!oldRas || oldW <= 0 || oldH <= 0 || oldBoxH <= 0.0) return nr;

  // Work per REGION rather than per box: a merged pan is one region, every
  // unmerged box is a 1×1 region.  A per-box pass split a cross-box panorama at
  // the box edges — that broke merged pans and dropped their content.
  QVector<QRect> regions = m_merges;  // box-coord rects (col,row,wspan,hspan)
  for (int r = 0; r < m_rows; ++r)
    for (int c = 0; c < m_cols; ++c)
      if (mergeIndexAt(c, r) < 0) regions.push_back(QRect(c, r, 1, 1));

  // Tight bounding box of the inked pixels in a raster (near-white ignored so
  // resampling half-tones don't grow it), or an empty rect if the region is
  // blank. Extracting only the ink — never the region's white margins — is what
  // stops old box borders from accumulating as a faint ghost grid across
  // repeated camera changes.
  auto inkBBox = [](const TRaster32P &ras) -> TRect {
    ras->lock();
    const int w = ras->getLx(), h = ras->getLy();
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y) {
      TPixel32 *row = ras->pixels(y);
      for (int x = 0; x < w; ++x) {
        const TPixel32 &p = row[x];
        if (p.r < 248 || p.g < 248 || p.b < 248) {
          if (x < x0) x0 = x;
          if (x > x1) x1 = x;
          if (y < y0) y0 = y;
          if (y > y1) y1 = y;
        }
      }
    }
    ras->unlock();
    return (x1 < x0 || y1 < y0) ? TRect() : TRect(x0, y0, x1, y1);
  };

  // Paint an n-pixel white frame around a raster. do_resample() fills pixels
  // whose filter kernel reaches past the source edge with black; a white frame
  // over that outer band removes the faint border it would otherwise leave
  // around the drawing (the band sits in the white margin, clear of the ink).
  auto whiteFrame = [](TRaster32P r, int n) {
    const int w = r->getLx(), h = r->getLy();
    n           = qMin(n, qMin(w, h) / 2);
    if (n <= 0) return;
    TRaster32P(r->extract(0, 0, w - 1, n - 1))->fill(TPixel32::White);  // bottom
    TRaster32P(r->extract(0, h - n, w - 1, h - 1))->fill(TPixel32::White);  // top
    TRaster32P(r->extract(0, 0, n - 1, h - 1))->fill(TPixel32::White);   // left
    TRaster32P(r->extract(w - n, 0, w - 1, h - 1))->fill(TPixel32::White);  // right
  };

  for (const QRect &reg : regions) {
    const int sx0 = qBound(0, reg.x() * bw, oldW);
    const int sx1 = qBound(0, (reg.x() + reg.width()) * bw, oldW);
    const int sy0 =
        qBound(0, (int)(oldH - (reg.y() + reg.height()) * oldBoxH), oldH);
    const int sy1 = qBound(0, (int)(oldH - reg.y() * oldBoxH), oldH);
    if (sx1 - sx0 < 1 || sy1 - sy0 < 1) continue;

    // Isolate this region's ink (drop the white margins so they never carry
    // forward as ghost seams).
    TRaster32P src = oldRas->extract(sx0, sy0, sx1 - 1, sy1 - 1)->clone();
    TRect ink      = inkBBox(src);
    if (ink.isEmpty()) continue;  // blank region → stays white
    const int inkW = ink.getLx();
    const int inkH = ink.getLy();

    const int nregW = reg.width() * bw;
    const int nregH = qMax(1, (int)(reg.height() * newBoxH));

    // Keep the drawing's height fraction of the frame constant: scale by the
    // box-height ratio (grows AND shrinks, so a round-trip of camera changes
    // restores the original size).  Reduce further only when the scaled ink
    // would overflow the box — width first (fixed box width), then height.
    double sc = newBoxH / oldBoxH;
    sc        = qMin(sc, (double)nregW / qMax(1, inkW));
    sc        = qMin(sc, (double)nregH / qMax(1, inkH));
    if (sc <= 0.0) sc = 1.0;

    TRaster32P content;
    int cw, ch;
    if (qAbs(sc - 1.0) < 1e-3) {
      // No scaling → no resample, no edge artifact: copy the ink straight.
      content = src->extract(ink)->clone();
      cw      = content->getLx();
      ch      = content->getLy();
    } else {
      // Pad the ink with a white margin so the resample filter samples white at
      // the edges instead of running off the raster (which do_resample fills
      // with black → a dark border around the drawing).  A white frame over the
      // outermost band then clears any residue at the padded edge.
      const int m = 6;
      TRaster32P padded(inkW + 2 * m, inkH + 2 * m);
      padded->fill(TPixel32::White);
      padded->copy(src->extract(ink)->clone(), TPoint(m, m));
      const int pw = padded->getLx(), ph = padded->getLy();
      cw           = qMax(1, (int)(pw * sc));
      ch           = qMax(1, (int)(ph * sc));
      content      = TRaster32P(cw, ch);
      content->fill(TPixel32::White);
      TRop::resample(content, padded,
                     TScale((double)cw / pw, (double)ch / ph));
      whiteFrame(content, 2);
    }

    // Re-centre the drawing inside this region's new rectangle.
    const int nx0 = qBound(0, reg.x() * bw, gw);
    const int ny0 =
        qBound(0, (int)(newH - (reg.y() + reg.height()) * newBoxH), newH);
    const int offX = qBound(0, nx0 + (nregW - cw) / 2, qMax(0, gw - cw));
    const int offY = qBound(0, ny0 + (nregH - ch) / 2, qMax(0, newH - ch));
    nr->copy(content, TPoint(offX, offY));
  }
  return nr;
}

//=============================================================================
// Selection
//=============================================================================

void ZtoryThumbnailCanvas::updateToolCursor() {
  if (m_xformMode)
    setCursor(Qt::CrossCursor);
  else if (m_selectMode)
    setCursor(Qt::PointingHandCursor);
  else
    setCursor(Qt::BlankCursor);  // drawing: the brush circle is the cursor
}

void ZtoryThumbnailCanvas::setSelectMode(bool on) {
  if (m_selectMode == on) return;
  m_selectMode = on;
  if (!on) clearSelection();  // leaving Select mode deselects all panels
  updateToolCursor();
  update();
}

void ZtoryThumbnailCanvas::clearSelection() {
  if (m_selection.isEmpty()) return;
  m_selection.clear();
  emit selectionChanged(0);
  update();
}

void ZtoryThumbnailCanvas::toggleMergeSelection() {
  if (m_selection.isEmpty()) return;

  // If the selection contains any merged region, split those back into boxes.
  bool anyMerge = false;
  for (int idx : m_selection) {
    const int col = idx % m_cols, row = idx / m_cols;
    if (mergeIndexAt(col, row) >= 0) anyMerge = true;
  }
  if (anyMerge) {
    pushUndo();
    QVector<QRect> kept;
    for (const QRect &m : m_merges) {
      const int tl = m.y() * m_cols + m.x();
      if (m_selection.indexOf(tl) < 0) kept.push_back(m);
    }
    m_merges = kept;
    clearSelection();
    schedulePersistSave();
    update();
    return;
  }

  // Otherwise merge the bounding rectangle of the selection.  We auto-fill the
  // boxes the user didn't click (e.g. a diagonal pick) so the result is always a
  // valid rectangular panorama — no need to select every box by hand.
  int c0 = m_cols, r0 = m_rows, c1 = -1, r1 = -1;
  for (int idx : m_selection) {
    const int col = idx % m_cols, row = idx / m_cols;
    c0 = qMin(c0, col); r0 = qMin(r0, row);
    c1 = qMax(c1, col); r1 = qMax(r1, row);
  }
  const int w = c1 - c0 + 1, h = r1 - r0 + 1;
  if (w * h < 2) return;  // need at least two boxes to form a panorama
  // None of the covered boxes may already belong to a merge.
  for (int rr = r0; rr <= r1; ++rr)
    for (int cc = c0; cc <= c1; ++cc)
      if (mergeIndexAt(cc, rr) >= 0) return;

  pushUndo();
  m_merges.push_back(QRect(c0, r0, w, h));
  clearSelection();
  schedulePersistSave();
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

int ZtoryThumbnailCanvas::mergeIndexAt(int col, int row) const {
  for (int i = 0; i < m_merges.size(); ++i)
    if (m_merges[i].contains(col, row)) return i;
  return -1;
}

QRect ZtoryThumbnailCanvas::regionBoxRect(int topLeftIndex) const {
  if (topLeftIndex < 0 || topLeftIndex >= m_cols * m_rows)
    return QRect(0, 0, 1, 1);
  const int col = topLeftIndex % m_cols, row = topLeftIndex / m_cols;
  const int mi = mergeIndexAt(col, row);
  return mi >= 0 ? m_merges[mi] : QRect(col, row, 1, 1);
}

int ZtoryThumbnailCanvas::regionIndexOf(int boxIndex) const {
  const QRect r = regionBoxRect(boxIndex);  // resolves merge → its rect
  return r.y() * m_cols + r.x();            // top-left box's linear index
}

QSize ZtoryThumbnailCanvas::panelSpan(int index) const {
  const QRect r = regionBoxRect(index);
  return QSize(r.width(), r.height());
}

QRectF ZtoryThumbnailCanvas::panelWorldRect(int index) const {
  if (index < 0 || index >= m_cols * m_rows) return QRectF();
  const QRect r = regionBoxRect(index);
  return QRectF(r.x() * m_boxW, r.y() * m_boxH, r.width() * m_boxW,
                r.height() * m_boxH);
}

bool ZtoryThumbnailCanvas::isPanelEmpty(int index) const {
  if (!m_ras || index < 0 || index >= m_cols * m_rows) return true;
  const QRect br = regionBoxRect(index);
  const int lx = m_ras->getLx(), ly = m_ras->getLy();
  const int x0 = qBound(0, (int)(br.x() * m_boxW), lx);
  const int x1 = qBound(0, (int)((br.x() + br.width()) * m_boxW), lx);
  // World y is top-down; the raster is bottom-up, so flip when computing rows.
  const int ry0 = qBound(0, (int)(ly - (br.y() + br.height()) * m_boxH), ly);
  const int ry1 = qBound(0, (int)(ly - br.y() * m_boxH), ly);

  m_ras->lock();
  bool empty = true;
  for (int y = ry0; y < ry1 && empty; ++y) {
    const TPixel32 *pix = m_ras->pixels(y);
    for (int x = x0; x < x1; ++x) {
      const TPixel32 &p = pix[x];
      if (p.r < 250 || p.g < 250 || p.b < 250) { empty = false; break; }
    }
  }
  m_ras->unlock();
  return empty;
}

TRaster32P ZtoryThumbnailCanvas::panelRaster(int index,
                                             const TDimension &outRes) const {
  if (!m_ras || index < 0 || index >= m_cols * m_rows) return TRaster32P();
  const QRect br = regionBoxRect(index);  // whole region (merged or single box)
  const int lx = m_ras->getLx(), ly = m_ras->getLy();
  const int x0 = qBound(0, (int)(br.x() * m_boxW), lx);
  const int x1 = qBound(0, (int)((br.x() + br.width()) * m_boxW), lx);
  // World y is top-down; the raster is bottom-up, so flip when computing rows.
  const int ry0 = qBound(0, (int)(ly - (br.y() + br.height()) * m_boxH), ly);
  const int ry1 = qBound(0, (int)(ly - br.y() * m_boxH), ly);
  if (x1 <= x0 || ry1 <= ry0 || outRes.lx <= 0 || outRes.ly <= 0)
    return TRaster32P();

  // extract() shares memory with m_ras (inclusive coords) — fine as a read-only
  // source for resample, which writes into the independent output raster.
  TRaster32P sub = m_ras->extract(x0, ry0, x1 - 1, ry1 - 1);
  TRaster32P out(outRes.lx, outRes.ly);
  out->fill(TPixel32::White);
  TRop::resample(out, sub,
                 TScale((double)outRes.lx / sub->getLx(),
                        (double)outRes.ly / sub->getLy()));
  return out;
}

//=============================================================================
// Persistence — the whole contiguous canvas is stored as a single PNG in the
// scene's extras/<scene>/ folder (same family the export-to-board uses), named
// with its grid dimensions so they round-trip.  Saved debounced after edits and
// flushed on close; loaded when the scene is opened/switched.
//=============================================================================

QString ZtoryThumbnailCanvas::sceneKey() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return QString();
  return QString::fromStdWString(scene->getScenePath().getWideString());
}

TFilePath ZtoryThumbnailCanvas::persistDir() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return TFilePath();
  // Reuse the export-to-board folder resolution so canvas + exported shots live
  // together: decode a dummy OVL level path and take its parent directory.
  return scene
      ->decodeFilePath(scene->getDefaultLevelPath(OVL_XSHLEVEL, L"_ztorythumbs"))
      .getParentDir();
}

void ZtoryThumbnailCanvas::schedulePersistSave() {
  if (m_saveTimer) m_saveTimer->start();  // (re)arm the debounce
}

void ZtoryThumbnailCanvas::persistSave() {
  if (!m_ras) return;
  TFilePath dir = persistDir();
  if (dir.isEmpty()) return;
  QString dirStr = QString::fromStdWString(dir.getWideString());
  QDir qd(dirStr);
  if (!qd.exists()) qd.mkpath(".");

  // One canvas per scene: drop any previous size-tagged PNG before writing.
  for (const QString &old :
       qd.entryList(QStringList() << "_ztorythumbs_*.png", QDir::Files))
    qd.remove(old);

  QImage img = rasterToQImage(m_ras, /*premultiplied=*/false);
  QString file =
      dirStr + QString("/_ztorythumbs_%1x%2.png").arg(m_cols).arg(m_rows);
  img.save(file, "PNG");

  // Merged regions in a tiny sidecar ("col row w h" per line).
  const QString mergesFile = dirStr + "/_ztorythumbs_merges.txt";
  if (m_merges.isEmpty()) {
    QFile::remove(mergesFile);
  } else {
    QFile mf(mergesFile);
    if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream ts(&mf);
      for (const QRect &m : m_merges)
        ts << m.x() << ' ' << m.y() << ' ' << m.width() << ' ' << m.height()
           << '\n';
    }
  }
  m_persistKey = sceneKey();  // we now hold this scene's canvas on disk
}

void ZtoryThumbnailCanvas::persistLoad() {
  const QString key = sceneKey();
  // sceneSwitched also fires for re-selecting the same scene; only reload when
  // the scene identity actually changed, so in-RAM edits are never clobbered.
  if (key == m_persistKey) return;
  m_persistKey = key;

  TFilePath dir = persistDir();
  QStringList matches;
  if (!dir.isEmpty()) {
    QDir qd(QString::fromStdWString(dir.getWideString()));
    matches = qd.entryList(QStringList() << "_ztorythumbs_*x*.png", QDir::Files,
                           QDir::Time);
  }

  m_merges.clear();
  if (matches.isEmpty()) {
    // New scene with no saved canvas: start blank at the DEFAULT grid size (do
    // not inherit rows added with +Row in the previous scene).
    m_cols = kDefaultCols;
    m_rows = kDefaultRows;
    m_ras  = TRaster32P((int)gridW(), (int)gridH());
    m_ras->fill(TPixel32::White);
    clearSelection();
    updateScrollBars();
    update();
    return;
  }

  // Merged regions, if any (saved alongside the PNG).
  QFile mf(QString::fromStdWString(dir.getWideString()) +
           "/_ztorythumbs_merges.txt");
  if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream ts(&mf);
    while (!ts.atEnd()) {
      int c, r, w, h;
      ts >> c >> r >> w >> h;
      if (w > 0 && h > 0) m_merges.push_back(QRect(c, r, w, h));
    }
  }

  const QString fn = matches.first();  // most-recently modified
  QRegExp re("_ztorythumbs_(\\d+)x(\\d+)\\.png");
  if (re.indexIn(fn) >= 0) {
    m_cols = qMax(1, re.cap(1).toInt());
    m_rows = qMax(1, re.cap(2).toInt());
  }
  QImage img(QString::fromStdWString(dir.getWideString()) + "/" + fn);
  if (img.isNull()) return;
  TRaster32P r = rasterFromQImage(img, /*premultiply=*/false);

  // Adopt the saved box geometry as-is — do NOT reflow against the live camera
  // here. At scene-open time the app camera is often still the default (e.g.
  // 1920x1080) and only switches to the scene's real format a moment later,
  // firing onSceneChanged(). Reflowing now would target the wrong (default)
  // aspect, and onSceneChanged would then reflow AGAIN: a lossy double pass that
  // — because each region is clamped to fit a single box — compressed tall
  // drawings (a full-column stroke got squished to one row on reopen). Instead
  // take the raster at its saved box height and set m_boxAspect from it, so
  // onSceneChanged does the single correct reflow only if the real camera aspect
  // actually differs (and short-circuits when it matches, the common case).
  const double savedBoxH = r->getLy() / (double)qMax(1, m_rows);
  m_boxH                 = savedBoxH;
  m_boxAspect            = m_boxW / savedBoxH;
  m_ras                  = r;
  clearSelection();
  updateScrollBars();
  update();
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

void ZtoryThumbnailCanvas::updateScrollBars() {
  if (!m_hbar || !m_vbar) return;
  const double contentW = gridW() * m_zoom, contentH = gridH() * m_zoom;
  const int thick = 16;  // match the app's native scrollbar width
  const bool needH  = contentW > width() + 0.5;
  const bool needV  = contentH > height() + 0.5;
  const int viewW   = width() - (needV ? thick : 0);
  const int viewH   = height() - (needH ? thick : 0);

  m_syncingBars = true;
  m_hbar->setVisible(needH);
  m_vbar->setVisible(needV);
  if (needH) {
    m_hbar->setGeometry(0, height() - thick, viewW, thick);
    m_hbar->setRange(0, (int)std::ceil(contentW - viewW));
    m_hbar->setPageStep(viewW);
    m_hbar->setValue(qBound(0, (int)(-m_pan.x() + 0.5), m_hbar->maximum()));
  }
  if (needV) {
    m_vbar->setGeometry(width() - thick, 0, thick, viewH);
    m_vbar->setRange(0, (int)std::ceil(contentH - viewH));
    m_vbar->setPageStep(viewH);
    m_vbar->setValue(qBound(0, (int)(-m_pan.y() + 0.5), m_vbar->maximum()));
  }
  m_syncingBars = false;
}

void ZtoryThumbnailCanvas::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  updateScrollBars();
}

void ZtoryThumbnailCanvas::enterEvent(QEvent *) {
  m_cursorOnCanvas = true;
  update();
}

void ZtoryThumbnailCanvas::leaveEvent(QEvent *) {
  m_cursorOnCanvas = false;
  update();
}

double ZtoryThumbnailCanvas::brushRadiusWorld() const {
  // MyPaint radius is logarithmic (natural log of px); add the size modifier.
  return std::exp(m_brushBaseRadiusLog + m_sizeMod);
}

//=============================================================================
// Stroke lifecycle
//=============================================================================

void ZtoryThumbnailCanvas::beginStroke(const QPointF &widgetPos, double pressure) {
  if (m_selectMode || m_xformMode) return;  // these modes suspend drawing
  ensureStyle();
  if (!m_style || !m_ras) return;
  const QPointF w = widgetToWorld(widgetPos);
  if (w.x() < 0 || w.y() < 0 || w.x() > gridW() || w.y() > gridH()) return;

  pushUndo();   // snapshot before the stroke modifies the canvas
  setFocus();   // so Cmd-Z reaches us right after drawing

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
  schedulePersistSave();
}

//=============================================================================
// Events
//=============================================================================

void ZtoryThumbnailCanvas::tabletEvent(QTabletEvent *e) {
  // In Select / Transform modes let Qt synthesize mouse events (those handlers
  // own the interaction); the tablet only drives the brush.
  if (m_selectMode || m_xformMode) {
    e->ignore();
    return;
  }
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
    if (m_xformMode) {
      setFocus();  // ensure Esc / Enter / Del / Cmd-C/V reach keyPressEvent
      if (hasFloat()) {
        int h = floatHandleAt(e->localPos());
        if (h >= 0) {  // grab a handle (0..3 scale, 4 rotate, 5 move)
          m_floatDrag       = h;
          m_dragStartWorld  = widgetToWorld(e->localPos());
          m_dragStartScale  = m_floatScale;
          m_dragStartAngle  = m_floatAngle;
          m_dragStartCenter = m_floatCenter;
          return;
        }
        commitFloat();  // click outside the float bakes it, then a new marquee
      }
      m_marqueeing   = true;
      m_marqueeStart = m_marqueeCur = widgetToWorld(e->localPos());
      m_lassoPath.clear();
      if (m_lassoMode) m_lassoPath.push_back(m_marqueeStart);
      update();
      return;
    }
    if (m_selectMode) {
      int box = panelAtWorld(widgetToWorld(e->localPos()));
      // Clicking any box of a merged region selects the whole region.
      int idx = box >= 0 ? regionIndexOf(box) : -1;
      // Empty panels carry no drawing → not selectable (export skips them too).
      if (idx >= 0 && m_selection.indexOf(idx) < 0 && isPanelEmpty(idx))
        return;
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
  m_cursorWidget   = e->localPos();
  m_cursorOnCanvas = true;
  if (m_panning) {
    m_pan += e->pos() - m_lastPanPos;
    m_lastPanPos = e->pos();
    updateScrollBars();
    update();
    return;
  }
  const bool drawMode = !m_selectMode && !m_xformMode;
  if (m_xformMode) {
    if (m_marqueeing) {
      m_marqueeCur = widgetToWorld(e->localPos());
      if (m_lassoMode) m_lassoPath.push_back(m_marqueeCur);
      update();
      return;
    }
    if (m_floatDrag >= 0) {
      const QPointF w = widgetToWorld(e->localPos());
      if (m_floatDrag == 5) {  // move
        m_floatCenter = m_dragStartCenter + (w - m_dragStartWorld);
      } else if (m_floatDrag == 4) {  // rotate about center
        const double a0 = std::atan2(m_dragStartWorld.y() - m_dragStartCenter.y(),
                                     m_dragStartWorld.x() - m_dragStartCenter.x());
        const double a1 = std::atan2(w.y() - m_dragStartCenter.y(),
                                     w.x() - m_dragStartCenter.x());
        m_floatAngle = m_dragStartAngle + (a1 - a0);
      } else {  // 0..3 corner → uniform scale about center
        const double d0 = std::hypot(m_dragStartWorld.x() - m_dragStartCenter.x(),
                                     m_dragStartWorld.y() - m_dragStartCenter.y());
        const double d1 = std::hypot(w.x() - m_dragStartCenter.x(),
                                     w.y() - m_dragStartCenter.y());
        if (d0 > 1.0)
          m_floatScale = qBound(0.05, m_dragStartScale * (d1 / d0), 20.0);
      }
      update();
      return;
    }
  }
  if (m_stroking)
    strokeTo(e->localPos(), 0.5);
  else if (drawMode)
    update();  // repaint so the brush-circle cursor follows the mouse
}

void ZtoryThumbnailCanvas::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() == Qt::MiddleButton) {
    m_panning = false;
    updateToolCursor();
    return;
  }
  if (e->button() == Qt::LeftButton) {
    if (m_xformMode) {
      if (m_marqueeing) {
        m_marqueeing      = false;
        const bool copy   = e->modifiers() & Qt::AltModifier;
        if (m_lassoMode) {
          if (m_lassoPath.size() >= 3) liftFloatLasso(m_lassoPath, copy);
          m_lassoPath.clear();
        } else {
          const QRectF r = QRectF(m_marqueeStart, m_marqueeCur).normalized();
          if (r.width() >= 3 && r.height() >= 3) liftFloat(r, copy);
        }
      }
      m_floatDrag = -1;
      return;
    }
    endStroke();
  }
}

bool ZtoryThumbnailCanvas::handleTransformKey(QKeyEvent *e) {
  if (!m_xformMode) return false;
  // On macOS Qt maps Cmd → ControlModifier (Cmd-C/V here, Ctrl-C/V elsewhere).
  const bool cmd = e->modifiers() & Qt::ControlModifier;
  if (cmd && e->key() == Qt::Key_C) {
    if (!hasFloat()) return false;
    copyFloat();
    return true;
  }
  if (cmd && e->key() == Qt::Key_V) {
    if (m_clip.isNull()) return false;
    pasteFloat();
    return true;
  }
  if (!hasFloat()) return false;
  switch (e->key()) {
  case Qt::Key_Escape: cancelFloat(); return true;
  case Qt::Key_Return:
  case Qt::Key_Enter: commitFloat(); return true;
  case Qt::Key_Delete:
  case Qt::Key_Backspace: deleteFloat(); return true;
  }
  return false;
}

void ZtoryThumbnailCanvas::keyPressEvent(QKeyEvent *e) {
  if (handleUndoKey(e)) return;
  if (handleTransformKey(e)) return;
  QWidget::keyPressEvent(e);
}

bool ZtoryThumbnailCanvas::eventFilter(QObject *obj, QEvent *ev) {
  // Catch our shortcuts regardless of which widget in our window has focus (a
  // toolbar button often steals it). Guarded to our active window and to keys we
  // actually consume, so normal typing / the app's own undo elsewhere is safe.
  if (ev->type() == QEvent::KeyPress && isVisible() && window() &&
      window()->isActiveWindow()) {
    auto *ke = static_cast<QKeyEvent *>(ev);
    // Undo only when this canvas is the focus of attention (focused, hovered or
    // in a selection tool) AND we have history — else let the app handle Cmd-Z.
    if ((hasFocus() || underMouse() || m_xformMode || m_selectMode) &&
        handleUndoKey(ke))
      return true;
    if (m_xformMode && handleTransformKey(ke)) return true;
  }
  return QWidget::eventFilter(obj, ev);
}

void ZtoryThumbnailCanvas::wheelEvent(QWheelEvent *e) {
  // Wheel = zoom at the cursor (scroll is via the side bars / middle-drag pan).
  const int dy = e->angleDelta().y();
  if (dy != 0) {
    zoomAt(e->position(), dy > 0 ? 1.15 : 1.0 / 1.15);
    updateScrollBars();
  }
  e->accept();
}

//=============================================================================
// Transform tool (raster selection: move / copy / scale / rotate)
//=============================================================================

void ZtoryThumbnailCanvas::setTransformMode(bool on) {
  if (m_xformMode == on) return;
  if (!on) commitFloat();          // leaving the tool bakes any floating piece
  m_xformMode  = on;
  m_marqueeing = false;
  m_floatDrag  = -1;
  if (on) {                        // Select and Transform are mutually exclusive
    m_selectMode = false;
    if (!m_selection.isEmpty()) clearSelection();
  }
  updateToolCursor();
  if (on) setFocus();              // so Esc / Enter / Del reach keyPressEvent
  update();
}

void ZtoryThumbnailCanvas::liftFloat(const QRectF &worldRect, bool copy) {
  if (!m_ras) return;
  const int lx = m_ras->getLx(), ly = m_ras->getLy();
  const int x0 = qBound(0, (int)std::floor(worldRect.left()), lx);
  const int x1 = qBound(0, (int)std::ceil(worldRect.right()), lx);
  const int wy0 = qBound(0, (int)std::floor(worldRect.top()), ly);   // world y
  const int wy1 = qBound(0, (int)std::ceil(worldRect.bottom()), ly);
  if (x1 - x0 < 2 || wy1 - wy0 < 2) return;
  // Snapshot the pre-edit canvas now (start of a transform session); commit
  // adds no further snapshot, so one undo reverts the whole move/scale/rotate.
  pushUndo();
  // World y is top-down; the raster is bottom-up, so flip to raster rows.
  const int ry0 = qBound(0, ly - wy1, ly);
  const int ry1 = qBound(0, ly - wy0, ly);

  TRaster32P sub = m_ras->extract(x0, ry0, x1 - 1, ry1 - 1);  // shares m_ras mem
  // clone() gives a CONTIGUOUS copy (wrap == lx); rasterToQImage assumes that,
  // whereas the extracted sub keeps the parent's wrap → "dusty" stride garbage.
  // .copy() detaches from the clone's buffer (freed at scope exit).
  m_floatImg = rasterToQImage(sub->clone(), /*premul=*/true, /*mirror=*/true).copy();
  if (!copy) {  // move → clear the source region to white
    sub->lock();
    for (int y = 0; y < sub->getLy(); ++y) {
      TPixel32 *p = sub->pixels(y);
      for (int x = 0; x < sub->getLx(); ++x) p[x] = TPixel32::White;
    }
    sub->unlock();
  }
  m_floatSrcRect = QRect(x0, wy0, x1 - x0, wy1 - wy0);
  m_floatCenter  = QPointF(x0 + (x1 - x0) / 2.0, wy0 + (wy1 - wy0) / 2.0);
  m_floatScale   = 1.0;
  m_floatAngle   = 0.0;
  m_floatWasMove = !copy;
  if (!copy) schedulePersistSave();  // the source was modified
  update();
}

void ZtoryThumbnailCanvas::liftFloatLasso(const QVector<QPointF> &worldPath,
                                          bool copy) {
  if (!m_ras || worldPath.size() < 3) return;
  QPolygonF poly(worldPath.toList().toVector());
  const QRectF bb = poly.boundingRect();
  // Reuse the rectangular lift for the bounding box, then mask to the polygon.
  liftFloat(bb, /*copy=*/true);  // never let the rect lift clear the source
  if (!hasFloat()) return;

  // Mask: keep only the pixels inside the freehand polygon (polygon → image
  // local coords are world − bbox top-left).
  QImage mask(m_floatImg.size(), QImage::Format_ARGB32_Premultiplied);
  mask.fill(Qt::transparent);
  {
    QPainter mp(&mask);
    mp.setRenderHint(QPainter::Antialiasing, true);
    mp.setPen(Qt::NoPen);
    mp.setBrush(Qt::white);
    mp.drawPolygon(poly.translated(-bb.topLeft()));
  }
  {
    QPainter fp(&m_floatImg);
    fp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    fp.drawImage(0, 0, mask);
  }

  if (!copy && m_ras) {  // erase only the lassoed shape from the canvas
    QImage canvasImg = rasterToQImage(m_ras, true, true);  // world orientation
    {
      QPainter cp(&canvasImg);
      cp.setRenderHint(QPainter::Antialiasing, true);
      cp.setPen(Qt::NoPen);
      cp.setBrush(Qt::white);
      cp.drawPolygon(poly);  // world coords == canvasImg px
    }
    m_ras = rasterFromQImage(canvasImg, true, true);
    m_floatWasMove = true;
    schedulePersistSave();
  }
  update();
}

void ZtoryThumbnailCanvas::copyFloat() {
  if (hasFloat()) m_clip = m_floatImg;
}

void ZtoryThumbnailCanvas::pasteFloat() {
  if (m_clip.isNull()) return;
  commitFloat();  // bake any current float first
  pushUndo();     // start of the paste session (canvas = after that bake)
  m_floatImg     = m_clip;
  m_floatCenter  = widgetToWorld(QPointF(width() / 2.0, height() / 2.0));
  m_floatScale   = 1.0;
  m_floatAngle   = 0.0;
  m_floatWasMove = false;  // a paste has no source to restore
  m_floatDrag    = -1;
  setFocus();
  update();
}

QTransform ZtoryThumbnailCanvas::floatLocalToWorld() const {
  const double w = m_floatImg.width(), h = m_floatImg.height();
  QTransform t;
  t.translate(m_floatCenter.x(), m_floatCenter.y());
  t.rotateRadians(m_floatAngle);
  t.scale(m_floatScale, m_floatScale);
  t.translate(-w / 2.0, -h / 2.0);
  return t;
}

QPointF ZtoryThumbnailCanvas::floatHandleWorld(int h) const {
  const double w = m_floatImg.width(), hh = m_floatImg.height();
  const QTransform t = floatLocalToWorld();
  switch (h) {
  case 0: return t.map(QPointF(0, 0));      // top-left
  case 1: return t.map(QPointF(w, 0));      // top-right
  case 2: return t.map(QPointF(w, hh));     // bottom-right
  case 3: return t.map(QPointF(0, hh));     // bottom-left
  case 4: {                                 // rotate: above the top edge
    const QPointF topMid = t.map(QPointF(w / 2.0, 0));
    QPointF up           = topMid - t.map(QPointF(w / 2.0, 1));
    const double n       = std::hypot(up.x(), up.y());
    if (n > 1e-6) up /= n;
    return topMid + up * 30.0;              // ~30 world px gap
  }
  }
  return QPointF();
}

int ZtoryThumbnailCanvas::floatHandleAt(const QPointF &widgetPos) const {
  if (!hasFloat()) return -1;
  for (int h = 4; h >= 0; --h) {  // prefer rotate/corner handles over the body
    const QPointF wp = worldToWidget(floatHandleWorld(h));
    if (QLineF(wp, widgetPos).length() <= 9.0) return h;
  }
  // Inside the (possibly rotated) body → move.
  QPolygonF poly;
  for (int c = 0; c < 4; ++c) poly << worldToWidget(floatHandleWorld(c));
  return poly.containsPoint(widgetPos, Qt::OddEvenFill) ? 5 : -1;
}

void ZtoryThumbnailCanvas::paintFloat(QPainter &p) {
  if (!hasFloat()) return;
  p.save();
  const QPointF o = worldToWidget(QPointF(0, 0));
  QTransform world2widget;
  world2widget.translate(o.x(), o.y());
  world2widget.scale(m_zoom, m_zoom);
  p.setTransform(floatLocalToWorld() * world2widget);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.drawImage(0, 0, m_floatImg);
  p.restore();

  // Outline + handles (drawn in widget space).
  QPolygonF poly;
  for (int c = 0; c < 4; ++c) poly << worldToWidget(floatHandleWorld(c));
  QPen pen(QColor(0, 170, 255));
  pen.setCosmetic(true);
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(poly);

  // Rotation handle: a stalk + circle.
  const QPointF topMid = (poly[0] + poly[1]) / 2.0;
  const QPointF rot    = worldToWidget(floatHandleWorld(4));
  p.drawLine(topMid, rot);
  p.setBrush(QColor(0, 170, 255));
  p.drawEllipse(rot, 5, 5);

  // Corner (scale) handles.
  for (int c = 0; c < 4; ++c) {
    const QPointF wp = poly[c];
    p.drawRect(QRectF(wp.x() - 4, wp.y() - 4, 8, 8));
  }
  p.setBrush(Qt::NoBrush);
}

void ZtoryThumbnailCanvas::commitFloat() {
  if (!hasFloat() || !m_ras) return;
  QImage canvasImg = rasterToQImage(m_ras, /*premul=*/true, /*mirror=*/true);
  {
    QPainter p(&canvasImg);  // canvasImg px == world coords (top-down)
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setTransform(floatLocalToWorld());
    p.drawImage(0, 0, m_floatImg);
  }
  m_ras      = rasterFromQImage(canvasImg, /*premul=*/true, /*mirror=*/true);
  m_floatImg = QImage();
  m_floatDrag = -1;
  schedulePersistSave();
  update();
}

void ZtoryThumbnailCanvas::cancelFloat() {
  if (!hasFloat()) return;
  if (m_floatWasMove && m_ras) {  // put the lifted pixels back where they were
    QImage canvasImg = rasterToQImage(m_ras, true, true);
    {
      QPainter p(&canvasImg);
      p.drawImage(m_floatSrcRect.topLeft(), m_floatImg);
    }
    m_ras = rasterFromQImage(canvasImg, true, true);
    schedulePersistSave();
  }
  m_floatImg  = QImage();
  m_floatDrag = -1;
  update();
}

void ZtoryThumbnailCanvas::deleteFloat() {
  if (!hasFloat()) return;
  m_floatImg  = QImage();  // source already cleared on lift (for a move)
  m_floatDrag = -1;
  update();
}

//=============================================================================
// Undo / redo — full-canvas snapshots (raster + grid + merges)
//=============================================================================

void ZtoryThumbnailCanvas::pushUndo() {
  if (!m_ras) return;
  static const size_t kMaxUndo = 16;
  m_undo.push_back({m_ras->clone(), m_cols, m_rows, m_merges, m_boxAspect});
  if (m_undo.size() > kMaxUndo) m_undo.erase(m_undo.begin());
  m_redo.clear();  // a fresh edit invalidates the redo branch
}

void ZtoryThumbnailCanvas::restoreSnapshot(const Snapshot &s) {
  m_ras    = s.ras->clone();  // clone so the stored snapshot stays immutable
  m_cols   = s.cols;
  m_rows   = s.rows;
  m_merges = s.merges;
  // Restore the grid geometry the raster was laid out at, so it isn't stretched
  // to whatever aspect the camera happens to be now.
  if (s.boxAspect > 0.0) {
    m_boxAspect = s.boxAspect;
    m_boxH      = m_boxW / s.boxAspect;
  }
  m_floatImg = QImage();  // any floating selection is dropped on undo/redo
  m_floatDrag = -1;
  clearSelection();
  schedulePersistSave();
  updateScrollBars();
  update();
}

void ZtoryThumbnailCanvas::undo() {
  if (m_undo.empty()) return;
  if (!m_ras) return;
  m_redo.push_back({m_ras->clone(), m_cols, m_rows, m_merges, m_boxAspect});
  Snapshot s = m_undo.back();
  m_undo.pop_back();
  restoreSnapshot(s);
}

void ZtoryThumbnailCanvas::redo() {
  if (m_redo.empty()) return;
  if (!m_ras) return;
  m_undo.push_back({m_ras->clone(), m_cols, m_rows, m_merges, m_boxAspect});
  Snapshot s = m_redo.back();
  m_redo.pop_back();
  restoreSnapshot(s);
}

bool ZtoryThumbnailCanvas::handleUndoKey(QKeyEvent *e) {
  // Cmd/Ctrl+Z = undo, Cmd/Ctrl+Shift+Z = redo. Only consume when we actually
  // have history, so the app's own undo still works when ours is empty.
  if (!(e->modifiers() & Qt::ControlModifier) || e->key() != Qt::Key_Z)
    return false;
  if (e->modifiers() & Qt::ShiftModifier) {
    if (m_redo.empty()) return false;
    redo();
  } else {
    if (m_undo.empty()) return false;
    undo();
  }
  return true;
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
  // Drawn per box-edge so the borders INTERNAL to a merged region are skipped,
  // making the merge read as one panorama panel.  Classic animation blue
  // (#1D5C83) so the guide never reads as pencil — any residual GREY line is
  // then obviously a drawing artifact, not the grid.  Kept faint (low alpha) so
  // it stays as unobtrusive as the old grey guide.
  QPen sep(QColor(29, 92, 131, 90));
  sep.setCosmetic(true);
  p.setPen(sep);
  for (int c = 1; c < m_cols; ++c)
    for (int r = 0; r < m_rows; ++r) {
      if (mergeIndexAt(c - 1, r) >= 0 &&
          mergeIndexAt(c - 1, r) == mergeIndexAt(c, r))
        continue;  // interior vertical edge of a merge
      const double x  = worldToWidget(QPointF(c * m_boxW, 0)).x();
      const double y0 = worldToWidget(QPointF(0, r * m_boxH)).y();
      const double y1 = worldToWidget(QPointF(0, (r + 1) * m_boxH)).y();
      p.drawLine(QPointF(x, y0), QPointF(x, y1));
    }
  for (int r = 1; r < m_rows; ++r)
    for (int c = 0; c < m_cols; ++c) {
      if (mergeIndexAt(c, r - 1) >= 0 &&
          mergeIndexAt(c, r - 1) == mergeIndexAt(c, r))
        continue;  // interior horizontal edge of a merge
      const double y  = worldToWidget(QPointF(0, r * m_boxH)).y();
      const double x0 = worldToWidget(QPointF(c * m_boxW, 0)).x();
      const double x1 = worldToWidget(QPointF((c + 1) * m_boxW, 0)).x();
      p.drawLine(QPointF(x0, y), QPointF(x1, y));
    }

  QPen border(QColor(29, 92, 131, 120));
  border.setCosmetic(true);
  p.setPen(border);
  p.drawRect(target);

  // Outline each merged (panorama) region a little brighter.
  QPen mergePen(QColor(90, 150, 220));
  mergePen.setCosmetic(true);
  mergePen.setWidth(2);
  p.setPen(mergePen);
  p.setBrush(Qt::NoBrush);
  for (const QRect &m : m_merges) {
    const QRectF wr(m.x() * m_boxW, m.y() * m_boxH, m.width() * m_boxW,
                    m.height() * m_boxH);
    const QRectF sr(worldToWidget(wr.topLeft()),
                    QSizeF(wr.width() * m_zoom, wr.height() * m_zoom));
    p.drawRect(sr);
  }

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

  // Transform tool: rubber-band marquee + the floating selection with handles.
  if (m_xformMode) {
    if (m_marqueeing) {
      QPen mp(QColor(0, 170, 255));
      mp.setCosmetic(true);
      mp.setStyle(Qt::DashLine);
      p.setPen(mp);
      if (m_lassoMode) {
        QPolygonF wpoly;
        for (const QPointF &wp : m_lassoPath) wpoly << worldToWidget(wp);
        p.setBrush(QColor(0, 170, 255, 30));
        p.drawPolygon(wpoly);
      } else {
        const QRectF wr = QRectF(m_marqueeStart, m_marqueeCur).normalized();
        const QRectF sr(worldToWidget(wr.topLeft()),
                        QSizeF(wr.width() * m_zoom, wr.height() * m_zoom));
        p.setBrush(QColor(0, 170, 255, 30));
        p.drawRect(sr);
      }
      p.setBrush(Qt::NoBrush);
    }
    paintFloat(p);
  }

  // Brush cursor: a circle of the real brush size (the system cursor is blank in
  // drawing mode). Drawn last so it sits on top of everything.
  if (!m_selectMode && !m_xformMode && m_cursorOnCanvas && !m_panning) {
    const double r = qBound(1.5, brushRadiusWorld() * m_zoom, 2000.0);
    p.setBrush(Qt::NoBrush);
    p.setRenderHint(QPainter::Antialiasing, true);
    // White halo + dark ring so it reads on any background.
    QPen halo(QColor(255, 255, 255, 200));
    halo.setCosmetic(true);
    halo.setWidthF(2.4);
    p.setPen(halo);
    p.drawEllipse(m_cursorWidget, r, r);
    QPen ring(QColor(30, 30, 30, 220));
    ring.setCosmetic(true);
    ring.setWidthF(1.0);
    p.setPen(ring);
    p.drawEllipse(m_cursorWidget, r, r);
    p.drawLine(m_cursorWidget + QPointF(-3, 0), m_cursorWidget + QPointF(3, 0));
    p.drawLine(m_cursorWidget + QPointF(0, -3), m_cursorWidget + QPointF(0, 3));
  }
}
