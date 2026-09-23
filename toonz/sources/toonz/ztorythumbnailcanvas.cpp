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
#include <QThreadPool>
#include <QRunnable>
#include <QPolygonF>
#include <QLineF>
#include <QApplication>
#include <QScrollBar>
#include "toonz/preferences.h"       // le gesture di annulla/ripeti
#include "toonzqt/menubarcommand.h"  // CommandManager
#include "menubarcommandids.h"       // MI_TouchGestureControl
#include <QTouchEvent>
#include <QGestureEvent>
#include <QGesture>
#include <QResizeEvent>

#include <cmath>

//=============================================================================

// The page is TRANSPARENT, not opaque white.  White is painted UNDER the
// surface by paintEvent, so drawing still happens on what looks like paper —
// but an eraser can now take pixels away instead of covering them with white
// paint, which is what "erase" is supposed to mean, and what lets an exported
// panel carry its transparency.
static const TPixel32 kPaper(0, 0, 0, 0);

namespace {

// The canvas write, with no dependency on the widget: it owns its pixels, so it
// is safe to run on a worker while the user keeps drawing on the live raster.

// Is this pixel ink, as opposed to paper?
//
// Paper is TRANSPARENT since the room learned to export with alpha, and the old
// test — "darker than near-white" — calls every blank pixel ink, because a
// transparent pixel is (0,0,0,0) and 0 is darker than 250.  That is what made
// every panel look occupied: lastNonEmptyRow() then returned the bottom of the
// grid, so each imported Procreate page landed a whole sheet below the previous
// one and left a band of empty rows between pages (and before the first one).
// inkBBox() had the same flaw, which would have made the camera reflow think the
// drawing filled the raster.
//
// Both conditions are needed.  Alpha alone would call a canvas migrated from the
// old opaque-white format completely full; near-white alone is what broke.
inline bool ztoryIsInk(const TPixel32 &p, int nearWhite = 250) {
  return p.m > 8 && (p.r < nearWhite || p.g < nearWhite || p.b < nearWhite);
}

// One band of the canvas on its way to disk.
struct ThumbBand {
  int index;
  QImage img;
};

// Write the bands that changed, plus the little manifest that says how to put
// them back together.  Only \a bands are touched: everything else on disk stays
// as it is, which is the whole point — the cost of a save follows what was
// drawn, not how long the storyboard is.
void writeThumbBands(const QString &dirStr, int cols, int rows, double boxH,
                     int bandCount, const QVector<ThumbBand> &bands,
                     const QVector<QRect> &merges, bool complete) {
  QDir qd(dirStr);
  if (!qd.exists() && !qd.mkpath(".")) return;

  for (const ThumbBand &b : bands) {
    const QString finalName =
        QString("_ztorythumbs_band%1.png").arg(b.index, 3, 10, QChar('0'));
    const QString finalPath = dirStr + "/" + finalName;
    // Beside it, then rename: a crash mid-encode must never leave the band
    // truncated.  The temporary name deliberately does not match the glob used
    // when loading.
    const QString tmpPath =
        dirStr + QString("/.ztorythumbs_writing_%1.png").arg(b.index);
    QFile::remove(tmpPath);
    if (!b.img.save(tmpPath, "PNG")) continue;  // keep the previous band
    QFile::remove(finalPath);
    if (!QFile::rename(tmpPath, finalPath)) QFile::remove(tmpPath);
  }

  // Bands beyond the current grid (rows were removed): drop them, or a later
  // load would stitch in a stale strip below the canvas.
  QRegExp bandRe("_ztorythumbs_band(\\d+)\\.png");
  for (const QString &f :
       qd.entryList(QStringList() << "_ztorythumbs_band*.png", QDir::Files)) {
    if (bandRe.indexIn(f) >= 0 && bandRe.cap(1).toInt() >= bandCount)
      qd.remove(f);
  }

  // The manifest is written LAST: until it is there, a half-written set of
  // bands is not loadable, and the loader falls back to whatever it had.
  QFile gf(dirStr + "/_ztorythumbs_grid.txt");
  if (gf.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream ts(&gf);
    ts << cols << ' ' << rows << ' ' << boxH << ' ' << bandCount << '\n';
  }

  // Merged regions in a tiny sidecar ("col row w h" per line).
  const QString mergesFile = dirStr + "/_ztorythumbs_merges.txt";
  if (merges.isEmpty()) {
    QFile::remove(mergesFile);
  } else {
    QFile mf(mergesFile);
    if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream ts(&mf);
      for (const QRect &m : merges)
        ts << m.x() << ' ' << m.y() << ' ' << m.width() << ' ' << m.height()
           << '\n';
    }
  }

  // Once the whole canvas has been written as bands, the old single image is no
  // longer what gets loaded — but it is not deleted: it is RENAMED out of the
  // way.  This is the first release of the banded format and that file is, for
  // a scene drawn before today, the only complete copy of the drawings; a bug
  // here would take a storyboard with it and no undo reaches that far.  The new
  // name deliberately drops the leading underscore so it cannot match the
  // loader's "_ztorythumbs_*x*.png" glob, and a later version can drop these
  // once the format has proved itself on real scenes.
  if (complete)
    for (const QString &old :
         qd.entryList(QStringList() << "_ztorythumbs_*x*.png", QDir::Files)) {
      const QString backup = "ztorythumbs_backup_" + old.mid(13);
      QFile::remove(dirStr + "/" + backup);
      if (!QFile::rename(dirStr + "/" + old, dirStr + "/" + backup))
        qd.remove(old);  // renaming failed: the bands are complete, let it go
    }
}

void writeThumbCanvas(const QImage &img, const QString &dirStr, int cols,
                      int rows, const QVector<QRect> &merges) {
  QDir qd(dirStr);
  if (!qd.exists() && !qd.mkpath(".")) return;

  const QString finalName =
      QString("_ztorythumbs_%1x%2.png").arg(cols).arg(rows);
  const QString finalPath = dirStr + "/" + finalName;
  // Write beside it and rename, instead of deleting the old file first.  The
  // old way left the scene with NO canvas for the whole encode — a third of a
  // second at 4x26, and growing — so a crash or a pulled cable in that window
  // lost the drawings outright.  The temporary name deliberately does NOT match
  // persistLoad()'s "_ztorythumbs_*x*.png" glob, so a half-written file can
  // never be mistaken for the canvas.  It also stops the delete/recreate churn
  // from re-uploading the whole file on a cloud-synced project folder.
  const QString tmpPath = dirStr + "/.ztorythumbs_writing.png";
  QFile::remove(tmpPath);
  if (!img.save(tmpPath, "PNG")) return;  // keep the previous canvas
  QFile::remove(finalPath);
  if (!QFile::rename(tmpPath, finalPath)) {
    QFile::remove(tmpPath);
    return;
  }

  // One canvas per scene: drop canvases saved at a DIFFERENT grid size.
  // persistLoad() takes the most recent match, so a stale one left over from
  // another row count would win after a row is removed.
  for (const QString &old :
       qd.entryList(QStringList() << "_ztorythumbs_*x*.png", QDir::Files))
    if (old != finalName) qd.remove(old);

  // Merged regions in a tiny sidecar ("col row w h" per line).
  const QString mergesFile = dirStr + "/_ztorythumbs_merges.txt";
  if (merges.isEmpty()) {
    QFile::remove(mergesFile);
  } else {
    QFile mf(mergesFile);
    if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream ts(&mf);
      for (const QRect &m : merges)
        ts << m.x() << ' ' << m.y() << ' ' << m.width() << ' ' << m.height()
           << '\n';
    }
  }
}

// Carries its own copy of everything it needs, so nothing it touches can be
// mutated (or destroyed) by the UI thread while it runs.
class ThumbBandSaveTask final : public QRunnable {
public:
  ThumbBandSaveTask(QVector<ThumbBand> bands, QString dir, int cols, int rows,
                    double boxH, int bandCount, QVector<QRect> merges,
                    bool complete, QObject *canvas)
      : m_bands(std::move(bands))
      , m_dir(std::move(dir))
      , m_cols(cols)
      , m_rows(rows)
      , m_boxH(boxH)
      , m_bandCount(bandCount)
      , m_merges(std::move(merges))
      , m_complete(complete)
      , m_canvas(canvas) {
    setAutoDelete(true);
  }
  void run() override {
    writeThumbBands(m_dir, m_cols, m_rows, m_boxH, m_bandCount, m_bands,
                    m_merges, m_complete);
    // Queued: the slot runs on the UI thread.  The canvas is guaranteed to
    // outlive this call because its destructor waits on the pool.
    QMetaObject::invokeMethod(m_canvas, "onPersistSaveFinished",
                              Qt::QueuedConnection);
  }

private:
  QVector<ThumbBand> m_bands;
  QString m_dir;
  int m_cols, m_rows;
  double m_boxH;
  int m_bandCount;
  QVector<QRect> m_merges;
  bool m_complete;
  QObject *m_canvas;
};

class ThumbCanvasSaveTask final : public QRunnable {
public:
  ThumbCanvasSaveTask(QImage img, QString dir, int cols, int rows,
                      QVector<QRect> merges, QObject *canvas)
      : m_img(std::move(img))
      , m_dir(std::move(dir))
      , m_cols(cols)
      , m_rows(rows)
      , m_merges(std::move(merges))
      , m_canvas(canvas) {
    setAutoDelete(true);
  }
  void run() override {
    writeThumbCanvas(m_img, m_dir, m_cols, m_rows, m_merges);
    // Queued: the slot runs on the UI thread.  The canvas is guaranteed to
    // outlive this call because its destructor waits on the pool.
    QMetaObject::invokeMethod(m_canvas, "onPersistSaveFinished",
                              Qt::QueuedConnection);
  }

private:
  QImage m_img;
  QString m_dir;
  int m_cols, m_rows;
  QVector<QRect> m_merges;
  QObject *m_canvas;
};

}  // namespace

ZtoryThumbnailCanvas::ZtoryThumbnailCanvas(QWidget *parent) : QWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);  // brush cursor follows the mouse without a button

  // Il tocco va CHIESTO, altrimenti Qt lo converte in eventi del mouse e il
  // canvas disegna quando l'utente voleva solo spostare la tela. Si prendono
  // anche Tap e Swipe, non perche' servano: e' la loro presenza a rendere
  // m_gestureActive vero durante un tocco, ed e' cosi' che SceneViewer
  // distingue il dito dalla penna.
  setAttribute(Qt::WA_AcceptTouchEvents);
  grabGesture(Qt::SwipeGesture);
  grabGesture(Qt::PanGesture);
  grabGesture(Qt::PinchGesture);
  grabGesture(Qt::TapGesture);

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
  m_ras->fill(kPaper);

  // React live to camera changes made from Camera Settings while this room is
  // open. xsheetChanged covers most camera edits; sceneChanged covers a scene
  // load/switch with a different camera.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &ZtoryThumbnailCanvas::onSceneChanged);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneChanged, this,
          &ZtoryThumbnailCanvas::onSceneChanged);

  // Persistence: debounced autosave after edits, reload on scene switch.
  // One worker, owned by this widget: saves serialise, and ~ZtoryThumbnailCanvas
  // can wait on it so a write is never abandoned half-done.
  m_savePool = new QThreadPool(this);
  m_savePool->setMaxThreadCount(1);
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
  // Flush any pending edit so closing the app never loses the canvas.  Three
  // cases, and all of them must end with the newest pixels on disk:
  //   - the debounce is still armed  -> there is an unsaved edit;
  //   - a worker is mid-write        -> wait for it;
  //   - an edit landed while it wrote -> m_saveQueued, and the slot that would
  //     have honoured it will never run, because there is no event loop here.
  // So: wait for the worker, then write synchronously if anything is pending.
  const bool pending =
      (m_saveTimer && m_saveTimer->isActive()) || m_saveQueued;
  if (m_savePool) m_savePool->waitForDone();
  if (pending && m_ras) {
    TFilePath dir = persistDir();
    if (!dir.isEmpty())
      writeThumbCanvas(rasterToQImage(m_ras, /*premultiplied=*/false),
                       QString::fromStdWString(dir.getWideString()), m_cols,
                       m_rows, m_merges);
  }
  delete m_brush;
  // NIENTE `delete m_style`.
  //
  // m_style non e' posseduto: e' lo stesso oggetto di m_styleRef, gia' convertito
  // (lo dice l'header: "m_styleRef, already downcast"), e appartiene alla palette
  // dei pennelli, che lo conta per riferimenti. Il `delete` era un residuo di
  // quando il canvas si costruiva lo stile da solo a partire da un percorso di
  // file; con la palette introdotta la notte del 15 quel codice e' rimasto li'.
  //
  // Faceva danni due volte: liberava un oggetto ancora vivo per la palette, e
  // subito dopo — alla parentesi di chiusura di questo distruttore — il
  // distruttore di m_styleRef decrementava il contatore DENTRO la memoria appena
  // liberata. E' la scrittura di 8 byte che AddressSanitizer ha inchiodato il
  // 2026-09-16 (heap-use-after-free, riga 346 scrive quel che la 345 ha liberato).
  //
  // Da qui veniva l'heap corrotto che poi faceva cadere il programma molto piu'
  // tardi e sempre altrove — icone SVG, barra dei menu, animazioni di finestra —
  // e che quattro ipotesi plausibili non erano riuscite a spiegare.
}

//=============================================================================
// Tool / palette
//=============================================================================

void ZtoryThumbnailCanvas::setBrushStyle(TMyPaintBrushStyle *style) {
  if (!style) return;
  // Take a reference: see m_styleRef.  Without it the next parameter edit in
  // the Style Editor frees this object under us.
  m_styleRef = style;
  m_style    = style;
  // The cursor circle needs the radius without starting a stroke, and the
  // style's own modified value is the whole truth now.
  m_brushBaseRadiusLog =
      m_style->getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
  updateToolCursor();
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


//=============================================================================
// Grid
//=============================================================================

void ZtoryThumbnailCanvas::addRow() {
  // NOT pushUndo(): that clones the whole canvas (51 MB at 4x26, and growing),
  // and sixteen of those filled the undo history with hundreds of megabytes —
  // the machine started swapping and never recovered.  Adding a row destroys
  // nothing, so the row count is the entire undo.
  {
    Snapshot s     = makeMetaSnapshot();
    s.geometryOnly = true;
    m_undo.push_back(std::move(s));
    trimHistory();
  }
  const int oldH  = m_ras->getLy();
  m_rows += 1;
  const int newH   = (int)gridH();
  const int addedH = newH - oldH;
  TRaster32P nr((int)gridW(), newH);
  nr->fill(kPaper);
  nr->copy(m_ras, TPoint(0, addedH));  // keep existing content at the same world Y
  m_ras = nr;
  updateScrollBars();
  update();
  schedulePersistSave();
}

void ZtoryThumbnailCanvas::applyImportedCells(
    const std::vector<ImportedBlit> &cells, int minRows) {
  if (!m_ras) return;
  pushUndo();

  // Grow the grid (rows are added at the world bottom; existing content keeps
  // its world Y, exactly like addRow()) so the next page has room to land.
  if (minRows > m_rows) {
    const int oldH   = m_ras->getLy();
    m_rows           = minRows;
    const int newH   = (int)gridH();
    const int addedH = newH - oldH;
    TRaster32P nr((int)gridW(), newH);
    nr->fill(kPaper);
    nr->copy(m_ras, TPoint(0, addedH));
    m_ras = nr;
  }

  const int lx = m_ras->getLx(), ly = m_ras->getLy();
  m_ras->lock();
  for (const ImportedBlit &b : cells) {
    if (b.image.isNull() || b.col < 0 || b.col >= m_cols || b.row < 0 ||
        b.row >= m_rows)
      continue;
    const int x0 = qBound(0, (int)(b.col * m_boxW), lx);
    const int x1 = qBound(0, (int)((b.col + 1) * m_boxW), lx);
    // World y is top-down, the raster is bottom-up: flip when placing rows.
    const int ry0 = qBound(0, (int)(ly - (b.row + 1) * m_boxH), ly);
    const int ry1 = qBound(0, (int)(ly - b.row * m_boxH), ly);
    const int dw = x1 - x0, dh = ry1 - ry0;
    if (dw < 2 || dh < 2) continue;

    // Smooth-scale to the box, then copy with a vertical flip: the QImage's top
    // row (j = 0) goes to the box's top, which is the HIGHEST raster row.
    const QImage s = b.image
                         .scaled(dw, dh, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_RGB888);
    for (int j = 0; j < dh; ++j) {
      const int rr = ry1 - 1 - j;
      if (rr < 0 || rr >= ly) continue;
      TPixel32 *drow      = m_ras->pixels(rr);
      const uchar *srow   = s.scanLine(j);
      for (int i = 0; i < dw; ++i) {
        const uchar *px = srow + i * 3;
        drow[x0 + i]    = TPixel32(px[0], px[1], px[2], 255);
      }
    }
  }
  m_ras->unlock();
  updateScrollBars();
  update();
  schedulePersistSave();
}

int ZtoryThumbnailCanvas::lastNonEmptyRow() const {
  for (int r = m_rows - 1; r >= 0; --r)
    for (int c = 0; c < m_cols; ++c)
      if (!isPanelEmpty(r * m_cols + c)) return r;
  return -1;
}

void ZtoryThumbnailCanvas::revealRow(int row) {
  if (row < 0 || m_rows <= 0) return;
  row = qBound(0, row, m_rows - 1);

  // Fit the grid width, so a freshly imported sheet is shown whole rather than
  // zoomed into one panel.
  const double margin = 28.0;
  if (gridW() > 0.0)
    m_zoom = qBound(0.05, (width() - 2 * margin) / gridW(), 4.0);

  const double rowTop = row * m_boxH * m_zoom;
  m_pan.setX(margin);
  m_pan.setY(margin - rowTop);
  updateScrollBars();
  update();
}

QImage ZtoryThumbnailCanvas::canvasImage() const {
  if (!m_ras) return QImage();
  // ⚠️ PREMOLTIPLICATO, e la parola non converte niente: rasterToQImage()
  // ETICHETTA gli stessi byte come ARGB32_Premultiplied o come ARGB32. Quindi
  // sbagliare l'etichetta non sposta un pixel in memoria, ma cambia cosa ne
  // capisce chi compone.
  //
  // Il buffer E' premoltiplicato — misurato il 2026-09-18 su una tela vera:
  // 124.216 pixel a trasparenza parziale, ZERO violazioni dell'invariante
  // R,G,B <= A. Qui c'era `false`, e questa immagine finisce nel contenuto
  // della STAMPA del foglio (printPaperSheet → p.content): un pixel bianco al
  // 37% di copertura, che premoltiplicato vale (95,95,95,95), letto come non
  // premoltiplicato diventa "grigio 95 al 37%" e sul bianco esce 196 invece di
  // 255. Cioe' i bordi morbidi — le sfumature della gomma, i contorni del lazo
  // — si stampavano piu' scuri di come li vedi a schermo.
  //
  // ⚠️ NON si corregge allo stesso modo il salvataggio e il caricamento della
  // tela, che pure dicono `false`: li' l'etichetta sbagliata c'e' in ANDATA e
  // in RITORNO, quindi il giro e' byte per byte identico e i file sono sani.
  // Cambiarne uno solo, o cambiarli tutti e due, farebbe ripremoltiplicare dati
  // gia' premoltiplicati: TUTTE le tele esistenti si scurirebbero sui bordi
  // morbidi, in silenzio. Si corregge il punto da cui l'errore ESCE, non quelli
  // dove si annulla.
  //
  // (mirrored resta al suo valore di default: l'orientamento e' quello del
  // mondo, dall'alto in basso, non quello del raster.)
  return rasterToQImage(m_ras, /*premultiplied=*/true).copy();
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

  const double newBoxH = m_boxW / aspect;
  const int newH       = qMax(1, (int)(m_rows * newBoxH));
  // Compare the resulting PIXEL layout, not the float aspect. On reopen,
  // persistLoad reconstructs m_boxAspect from the saved PNG's integer height, so
  // it drifts from the true camera aspect by sub-pixel rounding (harmless on a
  // clean 16:9, but ~0.0015 on e.g. a 1.85:1 camera — above the 1e-4 guard).
  // That drift used to trigger a spurious reanchor that sliced every cross-box
  // drawing into its own box and left ghost seams in the empty panels. When the
  // new layout has the SAME raster height, nothing really changed: just adopt
  // the exact aspect and skip the destructive reflow.
  if (newH == oldH) {
    m_boxAspect = aspect;  // correct the drift so the fast-path guard sticks
    return;
  }

  // ⚠️ NIENTE pushUndo() QUI, ed e' una scelta, non una dimenticanza.
  //
  // Qui c'era una fotografia della tela, «so Cmd-Z reverts the camera-format
  // reflow cleanly». Era l'origine di tutto il guaio trovato il 2026-09-23:
  // rendeva annullabile la CONSEGUENZA (la griglia si re-impagina) mentre la
  // CAUSA (il cambio di formato camera) non lo e' — e non lo e' nemmeno in
  // Tahoma2D ne' in OpenToonz, verificato: zero TUndo in camerasettingspopup e
  // in camerasettingswidget su entrambi. Da quell'asimmetria venivano il
  // disallineamento fra griglia e Camera Settings, e la fotografia a 16:9 che
  // ogni apertura di scena lasciava in pila pronta a saltare fuori al primo ⌘Z.
  //
  // Il principio, di Franco (2026-09-23): cambiare il formato della camera e'
  // una modifica alle IMPOSTAZIONI DEL PROGETTO, non al lavoro, e non deve
  // stare nella catena dell'annullamento — come il frame rate. Il caso che lo
  // decide: disegno, cambio camera, continuo a disegnare; se il cambio fosse
  // nella catena, per annullare le ultime pennellate sarei costretto a
  // passarci attraverso e mi ritroverei la camera cambiata senza averlo
  // chiesto.
  //
  // Il prezzo, dichiarato: reanchorRaster() re-impagina i disegni e da questa
  // operazione non si torna indietro. E' lo stesso patto di Tahoma su qualsiasi
  // cambio di formato, ed e' cio' che un cambio di formato SIGNIFICA.
  m_boxAspect = aspect;
  m_boxH      = newBoxH;
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
  nr->fill(kPaper);
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
        if (ztoryIsInk(p, 248)) {
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

  // Two tolerances, and both are needed — measured on a real canvas rather than
  // guessed.  A stroke drawn near the bottom of a panel leaves an antialiased
  // trail one or two pixels into the panel BELOW, at alpha 10-21: three to five
  // such pixels were enough to call an untouched panel "drawn", which pushed
  // every imported page one row further down.
  //
  //  - a 2 px inset, because that bleed lives exactly on the shared edge, and a
  //    drawing that exists only in the outermost two pixels of a panel is not a
  //    drawing (it also covers a long stroke bleeding along the whole edge,
  //    which no pixel count could tell from a deliberate thin line);
  //  - a minimum count, because specks can land anywhere. Twelve pixels out of
  //    129,600 is far below the smallest mark anyone makes on purpose and far
  //    above what bleed produces.
  const int kInset  = 2;
  const int kMinInk = 12;
  const int ix0 = x0 + kInset, ix1 = x1 - kInset;
  const int iy0 = ry0 + kInset, iy1 = ry1 - kInset;
  if (ix0 >= ix1 || iy0 >= iy1) return true;

  m_ras->lock();
  int ink = 0;
  for (int y = iy0; y < iy1 && ink <= kMinInk; ++y) {
    const TPixel32 *pix = m_ras->pixels(y);
    for (int x = ix0; x < ix1; ++x)
      if (ztoryIsInk(pix[x]) && ++ink > kMinInk) break;
  }
  m_ras->unlock();
  return ink <= kMinInk;
}

TRaster32P ZtoryThumbnailCanvas::panelRaster(int index, const TDimension &outRes,
                                            bool onWhite) const {
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
  out->fill(kPaper);
  TRop::resample(out, sub,
                 TScale((double)outRes.lx / sub->getLx(),
                        (double)outRes.ly / sub->getLy()));
  // The surface is transparent, and resample WRITES its output rather than
  // compositing into it — a white pre-fill would simply be overwritten.  So the
  // white sheet, when it is wanted, goes under the drawing explicitly.
  if (!onWhite) return out;
  TRaster32P sheet(outRes.lx, outRes.ly);
  sheet->fill(TPixel32::White);
  TRop::over(sheet, out);
  return sheet;
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

  // PER-SCENE, in extras. This used to resolve to the level folder so the canvas
  // would sit next to the shots exported to the board — but that folder belongs
  // to the PROJECT, and the file name carries only the grid size, so every scene
  // in a project read and wrote the same canvas: opening a fresh scene showed
  // the previous one's thumbnails, merged panorama frames included.
  //
  // Same resolution the screenplay import uses (ztoryscriptpanel.cpp), which
  // mirrors ToonzScene::getDefaultLevelPath: +extras + getSavePath() + subdir.
  // It honours <folder name="extras" useScenePath="yes"/> instead of guessing.
  const TFilePath savePath = scene->getSavePath();
  // Ztoryc does not let you work in an unsaved scene, precisely to avoid this
  // family of problems, so this should never be empty. Kept as a guard for the
  // instant during scene creation when the path is not set yet: writing then
  // would land in a shared folder, which is the bug this fixes.
  if (savePath.isEmpty()) return TFilePath();

  return scene->decodeFilePath(TFilePath("+extras")) + savePath +
         TFilePath("thumbs");
}

void ZtoryThumbnailCanvas::schedulePersistSave(const QRect &rasterRect) {
  markBandsDirty(rasterRect);
  schedulePersistSaveTimer();
}

void ZtoryThumbnailCanvas::schedulePersistSave() {
  markAllBandsDirty();
  schedulePersistSaveTimer();
}

void ZtoryThumbnailCanvas::schedulePersistSaveTimer() {
  if (m_saveTimer) m_saveTimer->start();  // (re)arm the debounce
}

int ZtoryThumbnailCanvas::bandCount() const {
  return (qMax(1, m_rows) + kRowsPerBand - 1) / kRowsPerBand;
}

void ZtoryThumbnailCanvas::markAllBandsDirty() {
  m_bandDirty.assign(bandCount(), true);
}

// Bands are numbered from the TOP of the canvas, in the order the rows are
// drawn — band 0 is the first five rows.  The raster is bottom-up and rows are
// appended at the world bottom, which in raster coordinates shifts every
// existing pixel upwards: numbering bands by raster Y would renumber the whole
// canvas each time a row is added, and every band would have to be rewritten.
// Anchored to the top, adding a row touches one band.
void ZtoryThumbnailCanvas::bandRasterRange(int b, int ly, int &y0,
                                           int &y1) const {
  const int bandRasterH = (int)std::lround(kRowsPerBand * m_boxH);
  y1                    = ly - 1 - b * bandRasterH;
  y0                    = qMax(0, ly - (b + 1) * bandRasterH);
  // The LAST band always runs down to 0.  With a non-16:9 camera m_boxH is not
  // an integer, so bandCount() * round(5 * m_boxH) can fall a pixel or two short
  // of the raster height: those rows would belong to no band, never be written,
  // and take a thin strip of drawing with them at the bottom of the canvas.
  // Found by checking that the bands tile the raster exactly — 20 cases out of
  // 310 left a gap, all of them with a non-integer box height.
  if (b == bandCount() - 1) y0 = 0;
}

void ZtoryThumbnailCanvas::markBandsDirty(const QRect &rasterRect) {
  const int n = bandCount();
  if ((int)m_bandDirty.size() != n) m_bandDirty.resize(n, true);
  if (rasterRect.isNull() || m_boxH <= 0.0 || !m_ras) {  // unknown: all of it
    markAllBandsDirty();
    return;
  }
  const double bandH = kRowsPerBand * m_boxH;
  const int ly       = m_ras->getLy();
  // Raster Y counts from the bottom; turn it into a distance from the top,
  // which is what the band numbering follows.
  const double fromTopOfHighest = ly - 1 - rasterRect.bottom();
  const double fromTopOfLowest  = ly - 1 - rasterRect.top();
  int b0 = (int)std::floor(fromTopOfHighest / bandH);
  int b1 = (int)std::floor(fromTopOfLowest / bandH);
  b0     = qBound(0, b0, n - 1);
  b1     = qBound(0, b1, n - 1);
  for (int b = b0; b <= b1; b++) m_bandDirty[b] = true;
}

void ZtoryThumbnailCanvas::persistSave() {
  if (!m_ras) return;
  TFilePath dir = persistDir();
  if (dir.isEmpty()) return;

  // Never run two encodes at once: they would queue up behind the pen and the
  // last one to land would not necessarily be the newest canvas.  Remember
  // instead that there is something newer to write — the dirty flags of the
  // bands are NOT cleared here, so nothing is forgotten in the meantime.
  if (m_saveRunning) {
    m_saveQueued = true;
    return;
  }

  const int n = bandCount();
  if ((int)m_bandDirty.size() != n) m_bandDirty.resize(n, true);

  const int lx = m_ras->getLx();
  const int ly = m_ras->getLy();

  QVector<ThumbBand> bands;
  int dirtyCount = 0;
  for (int b = 0; b < n; b++) {
    if (!m_bandDirty[b]) continue;
    dirtyCount++;
    int y0, y1;
    bandRasterRange(b, ly, y0, y1);
    if (y0 > y1 || y1 < 0 || y0 >= ly) continue;
    // Copy into a CONTIGUOUS raster rather than extracting a view: a view keeps
    // the parent's row stride, and rasterToQImage() builds the QImage without a
    // stride argument — the image would come out skewed.  This copy is the only
    // part the UI thread pays for, and it is one band (~10 MB), not the canvas.
    TRaster32P band(lx, y1 - y0 + 1);
    band->copy(m_ras->extract(0, y0, lx - 1, y1));
    bands.push_back({b, rasterToQImage(band, /*premultiplied=*/false)});
  }

  const bool complete = (dirtyCount == n);
  m_bandDirty.assign(n, false);

  if (bands.isEmpty()) return;  // nothing changed since the last write

  m_saveRunning = true;
  m_saveQueued  = false;
  m_persistKey  = sceneKey();  // this scene's canvas is (about to be) on disk
  m_savePool->start(new ThumbBandSaveTask(
      std::move(bands), QString::fromStdWString(dir.getWideString()), m_cols,
      m_rows, m_boxH, n, m_merges, complete, this));
}

void ZtoryThumbnailCanvas::onPersistSaveFinished() {
  m_saveRunning = false;
  if (m_saveQueued) {
    m_saveQueued = false;
    persistSave();  // an edit landed mid-write: the canvas on disk is stale
  }
}

void ZtoryThumbnailCanvas::loadMerges(const QString &dirStr) {
  QFile mf(dirStr + "/_ztorythumbs_merges.txt");
  if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QTextStream ts(&mf);
  while (!ts.atEnd()) {
    int c, r, w, h;
    ts >> c >> r >> w >> h;
    if (w > 0 && h > 0) m_merges.push_back(QRect(c, r, w, h));
  }
}

void ZtoryThumbnailCanvas::persistLoad() {
  const QString key = sceneKey();
  // sceneSwitched also fires for re-selecting the same scene; only reload when
  // the scene identity actually changed, so in-RAM edits are never clobbered.
  if (key == m_persistKey) return;
  m_persistKey = key;

  // ⚠️ La scena e' UN'ALTRA: tutto lo stato di modifica della tela precedente
  // deve morire qui. Restava in piedi, e un annullamento qualunque — voluto o
  // accidentale — ripescava uno stato della scena PRECEDENTE dentro quella
  // appena aperta; il salvataggio automatico poi lo scriveva su disco al posto
  // dei thumbs veri. Segnalato da Franco il 2026-09-23 provando sulla
  // Companion: «un undo successivo fa apparire i thumbs della scena
  // precedente, tutti insieme».
  //
  // Non serve il tocco per inciamparci: bastano due scene e un ⌘Z.
  m_undo.clear();
  m_redo.clear();
  m_strokeTiles.clear();
  // La selezione flottante e' un pezzo di raster dell'altra scena: confermarla
  // qui la incollerebbe in questa.
  m_floatImg     = QImage();
  m_floatDrag    = -1;
  m_floatWasMove = false;

  TFilePath dir = persistDir();
  QStringList matches;
  QString dirStr;
  bool haveBands = false;
  int gCols = 0, gRows = 0, gBands = 0;
  double gBoxH = 0.0;
  if (!dir.isEmpty()) {
    dirStr = QString::fromStdWString(dir.getWideString());
    QDir qd(dirStr);
    // The banded format first: its manifest is written last, so its presence
    // means a complete set of bands is on disk.
    QFile gf(dirStr + "/_ztorythumbs_grid.txt");
    if (gf.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream ts(&gf);
      ts >> gCols >> gRows >> gBoxH >> gBands;
      haveBands = (gCols > 0 && gRows > 0 && gBoxH > 0.0 && gBands > 0);
    }
    matches = qd.entryList(QStringList() << "_ztorythumbs_*x*.png", QDir::Files,
                           QDir::Time);
  }

  m_merges.clear();
  if (haveBands) {
    m_cols = gCols;
    m_rows = gRows;
    m_boxH = gBoxH;
    m_boxAspect = m_boxW / gBoxH;
    loadMerges(dirStr);
    TRaster32P r((int)gridW(), (int)gridH());
    r->fill(kPaper);
    for (int b = 0; b < gBands; b++) {
      QImage img(dirStr + QString("/_ztorythumbs_band%1.png")
                              .arg(b, 3, 10, QChar('0')));
      if (img.isNull()) continue;  // a missing band leaves blank paper, not a hole
      TRaster32P bandRas = rasterFromQImage(img, /*premultiply=*/false);
      int y0, y1;
      bandRasterRange(b, r->getLy(), y0, y1);
      y1 = qMin(y1, r->getLy() - 1);
      const int h = qMin(bandRas->getLy(), y1 - y0 + 1);
      if (h <= 0) continue;
      // Both sides count from the band's own top, so a short last band lands
      // where it was cut from.
      r->extract(0, y1 - h + 1, r->getLx() - 1, y1)
          ->copy(bandRas->extract(0, bandRas->getLy() - h, bandRas->getLx() - 1,
                                  bandRas->getLy() - 1));
    }
    m_ras = r;
    markAllBandsDirty();  // nothing written yet in THIS session
    clearSelection();
    updateScrollBars();
    update();
    return;
  }

  if (matches.isEmpty()) {
    // New scene with no saved canvas: start blank at the DEFAULT grid size (do
    // not inherit rows added with +Row in the previous scene).
    m_cols = kDefaultCols;
    m_rows = kDefaultRows;
    m_ras  = TRaster32P((int)gridW(), (int)gridH());
    m_ras->fill(kPaper);
    clearSelection();
    updateScrollBars();
    update();
    return;
  }

  // Merged regions, if any (saved alongside the canvas).
  loadMerges(QString::fromStdWString(dir.getWideString()));

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
  // Read from the old single-image format: every band still has to be written
  // before that file can be dropped (writeThumbBands only removes it on a
  // complete save).
  markAllBandsDirty();
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
  // MyPaint radius is logarithmic (natural log of px).  No modifier to add: the
  // style's value IS the size.
  if (!m_style) return std::exp(m_brushBaseRadiusLog);
  return std::exp(m_style->getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC));
}

//=============================================================================
// Stroke lifecycle
//=============================================================================

void ZtoryThumbnailCanvas::beginStroke(const QPointF &widgetPos, double pressure) {
  if (m_selectMode || m_xformMode) return;  // these modes suspend drawing
  if (!m_style || !m_ras) return;
  const QPointF w = widgetToWorld(widgetPos);
  if (w.x() < 0 || w.y() < 0 || w.x() > gridW() || w.y() > gridH()) return;

  // Record only the tiles the stroke touches (see askWrite): a full-canvas
  // clone here was the stall at stroke start.
  beginStrokeRecording();
  setFocus();   // so Cmd-Z reaches us right after drawing

  // A real eraser now: it takes pixels away instead of covering them with white.
  // On screen the result is identical, because paintEvent paints the white page
  // underneath — but the pixels are actually gone, so a soft eraser fades
  // properly and an exported panel can keep its transparency.
  const TPixel32 ink = m_color;
  TPixelD c = PixelConverter<TPixelD>::from(ink);
  double h = 0.0, s = 0.0, v = 0.0;
  RGB2HSV(c.r, c.g, c.b, &h, &s, &v);

  // getBrush() already returns the style WITH its modifications applied, so
  // radius, opacity and eraser come straight from it.  They used to be applied
  // here as an offset and a multiplier on top, which is exactly what made a
  // brush drift every time its values were written out and read back.
  mypaint::Brush brush;
  brush.fromBrush(m_style->getBrush());
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, (float)(h / 360.0));
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, (float)s);
  brush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, (float)v);

  delete m_brush;
  m_brush    = new MyPaintToonzBrush(m_ras, *this, brush);
  m_stroking   = true;
  m_cursorPrev = widgetPos;
  m_brush->beginStroke();
  m_timer.restart();
  m_brush->strokeTo(widgetToRaster(widgetPos), pressure, 0.0, 0.0, 0.0);
  update();
}

// The painted brush circle, with room for its halo and the little cross.
QRect ZtoryThumbnailCanvas::cursorRect(const QPointF &widgetPos) const {
  const double r = qBound(1.5, brushRadiusWorld() * m_zoom, 2000.0);
  return QRectF(widgetPos.x() - r, widgetPos.y() - r, 2 * r, 2 * r)
      .toAlignedRect()
      .adjusted(-4, -4, 4, 4);
}

// Raster is bottom-up, the widget is top-down: mirror Y about the grid height,
// then apply zoom and pan exactly as worldToWidget() does.
QRect ZtoryThumbnailCanvas::rasterRectToWidget(const QRect &r) const {
  const QPointF tl = worldToWidget(QPointF(r.left(), gridH() - r.bottom() - 1));
  const QPointF br = worldToWidget(QPointF(r.right() + 1, gridH() - r.top()));
  // A pixel of margin each way absorbs the rounding and the painter's smoothing,
  // which can tint the pixel just outside the dab.
  return QRectF(tl, br).toAlignedRect().adjusted(-2, -2, 2, 2);
}

void ZtoryThumbnailCanvas::strokeTo(const QPointF &widgetPos, double pressure) {
  if (!m_stroking || !m_brush) return;
  double dtime = m_timer.nsecsElapsed() * 1e-9;
  m_timer.restart();
  m_brush->strokeTo(widgetToRaster(widgetPos), pressure, 0.0, 0.0, dtime);

  // The brush cursor is PAINTED (the system cursor is blank in drawing mode),
  // so a partial repaint has to cover where the circle was and where it is now
  // — otherwise it sits frozen at the spot where the stroke began while the
  // pen moves away from it.  Most visible with an eraser, where the circle is
  // the only thing telling you what you are about to remove.
  QRect region = cursorRect(m_cursorPrev).united(cursorRect(widgetPos));
  m_cursorPrev = widgetPos;
  if (!m_strokeDirty.isNull()) {
    region = region.united(rasterRectToWidget(m_strokeDirty));
    m_strokeDirty = QRect();
  }
  update(region);
}

void ZtoryThumbnailCanvas::endStroke() {
  if (!m_stroking || !m_brush) return;
  m_brush->endStroke();
  delete m_brush;
  m_brush    = nullptr;
  m_stroking = false;
  endStrokeRecording();  // turn the touched tiles into one undo entry
  m_strokeDirty = QRect();
  update();  // safety net: one full repaint per stroke, not per tablet event
  // The only cheap save in the class: a stroke knows exactly where it went.
  schedulePersistSave(m_strokeBounds);
  m_strokeBounds = QRect();
}

//=============================================================================
// Events
//=============================================================================

void ZtoryThumbnailCanvas::tabletEvent(QTabletEvent *e) {
  // ⚠️ QUI c'era un rifiuto del palmo basato sulla PROSSIMITA' della penna, e
  // spegneva il tocco per sempre. Su un display con penna (Wacom Companion,
  // Cintiq) la penna e' quasi sempre vicina al vetro: bastava cominciare a
  // disegnare una volta e il dito non spostava piu' la tela. Segnalato da
  // Franco il 2026-09-18 provando sulla Companion 2.
  //
  // SceneViewer fa la cosa giusta e piu' semplice: su Windows abbassa il suo
  // flag a ogni evento della tavoletta che NON sia un tratto in corso
  // (`if (m_tabletState != StartStroke && m_tabletState != OnStroke)
  //   m_tabletEvent = false;`). Cioe' il tocco si blocca SOLO mentre si
  // disegna davvero. Qui l'equivalente e' m_stroking, che era gia' la
  // condizione giusta prima che la peggiorassi.
  //
  // Resta solo: alla PRESSIONE della penna si spegne un gesto eventualmente
  // rimasto acceso, o in Seleziona/Trasforma — le uniche modalita' in cui la
  // penna passa dal mouse sintetizzato — la guardia la bloccherebbe.
  if (e->type() == QEvent::TabletPress) {
    m_gestureActive = false;
    m_touchActive   = false;
    m_touchPanning  = false;
  }
  // In Select / Transform modes let Qt synthesize mouse events (those handlers
  // own the interaction); the tablet only drives the brush.
  if (m_selectMode || m_xformMode) {
    e->ignore();
    return;
  }
  // Keep the PAINTED brush cursor on the pen.  It is drawn at m_cursorWidget,
  // and only mouseMoveEvent ever set that — but this handler calls accept() on
  // every tablet event precisely so Qt does not synthesize mouse events.  With
  // the pen in proximity the circle therefore stayed wherever the mouse had last
  // been, both while hovering and while drawing.  It looked intermittent because
  // some tablet drivers emit mouse moves alongside the tablet ones, and it stood
  // out on the airbrush and the erasers because their circle is big enough that
  // being in the wrong place is unmissable.
  if (e->type() == QEvent::TabletMove || e->type() == QEvent::TabletPress) {
    const QRect before = cursorRect(m_cursorWidget);
    m_cursorWidget     = e->posF();
    m_cursorOnCanvas   = true;
    // While stroking, strokeTo() already repaints the union of the two cursor
    // positions: repainting here as well would just draw the same region twice.
    if (!m_stroking) update(before.united(cursorRect(m_cursorWidget)));
  } else if (e->type() == QEvent::TabletLeaveProximity) {
    m_cursorOnCanvas = false;
    update(cursorRect(m_cursorWidget));
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
  // Un dito su schermo touch arriva anche qui, sintetizzato. Se un gesto e' in
  // corso questo click NON e' una pennellata: e' la mano che sposta la tela.
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) return;
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
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) return;
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
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchScreen) {
    // ➕ Il TOCCO a due dita annulla, a tre ripete. Portato da
    // SceneViewer::mouseReleaseEvent: e' qui che vive, non nel gestore del
    // tocco, perche' si decide al rilascio — quando si sa quante dita ha
    // visto il tocco e se nel frattempo ha gia' spostato o zoomato (100).
    if (m_touchPoints == 2 &&
        Preferences::instance()->getGestureUndoMethod() ==
            Preferences::TwoFingerTap) {
      gestureUndo();
    } else if (m_touchPoints == 3 &&
               Preferences::instance()->getGestureRedoMethod() ==
                   Preferences::ThreeFingerTap) {
      gestureRedo();
    }
    m_touchPoints   = 0;
    m_gestureActive = false;
    m_zooming       = false;
    m_touchPanning  = false;
    return;
  }
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

// Would this key be consumed by the Transform tool? Kept separate from
// handleTransformKey so eventFilter can claim it on ShortcutOverride (see below)
// WITHOUT executing the action — otherwise a global shortcut like Delete (clear
// cells) swallows the key before it ever reaches us as a KeyPress.
bool ZtoryThumbnailCanvas::wantsTransformKey(QKeyEvent *e) const {
  if (!m_xformMode) return false;
  // On macOS Qt maps Cmd → ControlModifier (Cmd-C/V here, Ctrl-C/V elsewhere).
  const bool cmd = e->modifiers() & Qt::ControlModifier;
  if (cmd && e->key() == Qt::Key_C) return hasFloat();
  if (cmd && e->key() == Qt::Key_V) return !m_clip.isNull();
  if (!hasFloat()) return false;
  switch (e->key()) {
  case Qt::Key_Escape:
  case Qt::Key_Return:
  case Qt::Key_Enter:
  case Qt::Key_Delete:
  case Qt::Key_Backspace: return true;
  }
  return false;
}

bool ZtoryThumbnailCanvas::handleTransformKey(QKeyEvent *e) {
  if (!wantsTransformKey(e)) return false;
  const bool cmd = e->modifiers() & Qt::ControlModifier;
  if (cmd && e->key() == Qt::Key_C) {
    copyFloat();
    return true;
  }
  if (cmd && e->key() == Qt::Key_V) {
    pasteFloat();
    return true;
  }
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
  const bool isKey = ev->type() == QEvent::KeyPress ||
                     ev->type() == QEvent::ShortcutOverride;
  if (isKey && isVisible() && window() && window()->isActiveWindow()) {
    auto *ke = static_cast<QKeyEvent *>(ev);
    // Undo only when this canvas is the focus of attention (focused, hovered or
    // in a selection tool) AND we have history — else let the app handle Cmd-Z.
    const bool undoish =
        hasFocus() || underMouse() || m_xformMode || m_selectMode;
    const bool wantsUndo = undoish && (ke->modifiers() & Qt::ControlModifier) &&
                           ke->key() == Qt::Key_Z &&
                           ((ke->modifiers() & Qt::ShiftModifier)
                                ? !m_redo.empty()
                                : !m_undo.empty());
    // ShortcutOverride fires BEFORE a matching global QAction (e.g. Delete =
    // clear cells) would eat the key. Accepting it makes Qt re-deliver the key
    // as an ordinary KeyPress that the branches below then handle — without it,
    // Del/Backspace never reach the Transform tool at all.
    if (ev->type() == QEvent::ShortcutOverride) {
      if (wantsUndo || wantsTransformKey(ke)) {
        ke->accept();
        return true;
      }
      return QWidget::eventFilter(obj, ev);
    }
    if (undoish && handleUndoKey(ke)) return true;
    if (m_xformMode && handleTransformKey(ke)) return true;
  }
  return QWidget::eventFilter(obj, ev);
}

// ── Tocco e gesti ──────────────────────────────────────────────────
//
// Rispecchia SceneViewer (sceneviewerevents.cpp), che questa strada l'ha gia'
// battuta: UN dito sullo schermo touch, DUE sul trackpad. La differenza non e'
// un capriccio — su un trackpad un dito solo e' il puntatore e deve restare
// tale, mentre su uno schermo touch il dito E' la mano che sposta il foglio.

bool ZtoryThumbnailCanvas::event(QEvent *e) {
  // ⚠️ Questa funzione e' scritta come quella di SceneViewer::event(), e le
  // due differenze che aveva prima sono ESATTAMENTE il motivo per cui le
  // gesture funzionavano nelle altre room e non qui. Segnalato da Franco il
  // 2026-09-18 provando sulla Wacom Companion 2.
  //
  // 1. LA SPUNTA. SceneViewer non guarda il tocco se «Enable Touch Gesture
  //    Controls» e' spenta (Preferenze → Touch/Tablet Settings, che e' un
  //    comando, MI_TouchGestureControl, non una preferenza qualsiasi). Qui non
  //    si guardava: i gesti funzionavano anche dopo averli disattivati.
  //
  // 2. `m_gestureActive = true` SUBITO DOPO touchEvent(). E' la riga che non
  //    avevo copiato, ed e' quella che conta. Il TOCCO a due/tre dita per
  //    annulla e ripeti non vive nel gestore del tocco ma in
  //    mouseReleaseEvent, dietro la guardia `if (m_gestureActive && ...)`.
  //    Alzando quel flag solo dentro gestureEvent() — cioe' solo quando Qt
  //    riconosce un gesto suo — sulle piattaforme che non mandano un
  //    TapGesture il flag restava falso e il tocco-undo non partiva MAI.
  const bool gesturesOn = CommandManager::instance()
                              ->getAction(MI_TouchGestureControl)
                              ->isChecked();
  switch (e->type()) {
  case QEvent::TouchBegin:
  case QEvent::TouchUpdate:
  case QEvent::TouchEnd:
  case QEvent::TouchCancel:
    if (!gesturesOn) break;
    touchEvent(static_cast<QTouchEvent *>(e), e->type());
    m_gestureActive = true;
    return true;
  case QEvent::Gesture:
    if (!gesturesOn) break;
    gestureEvent(static_cast<QGestureEvent *>(e));
    return true;
  default:
    break;
  }
  return QWidget::event(e);
}

void ZtoryThumbnailCanvas::touchEvent(QTouchEvent *e, int type) {
  // Portata da ImageViewer::touchEvent (imageviewer.cpp), riga per riga. Le
  // uniche differenze sono segnate qui sotto e sono due: dove si sposta la
  // vista, e il rifiuto del palmo.
  if (type == QEvent::TouchBegin) {
    // ➕ AGGIUNTA rispetto all'originale: una penna che STA DISEGNANDO non deve
    // essere scambiata per un dito — e' mentre si disegna che la mano si
    // appoggia allo schermo. ImageViewer non ne ha bisogno perche' e' un visore.
    //
    // ⚠️ E basta m_stroking. Qui c'era anche la PROSSIMITA' della penna, e
    // spegneva il tocco per sempre su un display con penna, dove la penna e'
    // quasi sempre vicina al vetro: vedi tabletEvent.
    if (m_stroking) return;
    m_touchActive   = true;
    m_touchPoints   = e->touchPoints().count();
    m_undoPoint     = e->touchPoints().at(0).pos();
    m_touchClock.start();
    m_firstPanPoint = e->touchPoints().at(0).pos();
    m_touchDevice   = e->device() ? (int)e->device()->type()
                                  : (int)QTouchDevice::TouchScreen;
  } else if (m_touchActive) {
    m_touchPoints = std::max(e->touchPoints().count(), m_touchPoints);
    // touchpads must have 2 finger panning for tools and navigation to be
    // functional on other devices, 1 finger panning is preferred
    if ((e->touchPoints().count() == 2 &&
         m_touchDevice == QTouchDevice::TouchPad) ||
        (e->touchPoints().count() == 1 &&
         m_touchDevice == QTouchDevice::TouchScreen)) {
      QTouchEvent::TouchPoint panPoint = e->touchPoints().at(0);
      if (!m_touchPanning) {
        QPointF deltaPoint = panPoint.pos() - m_firstPanPoint;
        // minimize accidental and jerky zooming/rotating during 2 finger
        // panning
        if ((deltaPoint.manhattanLength() > 100) && !m_zooming) {
          m_touchPanning = true;
        }
      }
      if (m_touchPanning) {
        // ◆ DIVERSO dall'originale: li' c'e' panQt() su coordinate GL, con il
        // rapporto di pixel del dispositivo. Qui la vista e' m_pan, in
        // coordinate del widget, quindi il delta si usa com'e'.
        m_pan += (panPoint.pos() - panPoint.lastPos()).toPoint();
        updateScrollBars();
        update();
        m_touchPoints = 100;  // ha gia' fatto qualcosa: non e' un tocco secco
      }
    } else if (e->touchPoints().count() == 3) {
      // ➕ Trascinamento a TRE dita = annulla / ripeti. Portato da
      // SceneViewer::touchEvent, che ImageViewer non ha perche' in un visore
      // non c'e' niente da annullare. Rispetta le stesse preferenze del resto
      // dell'applicazione: non si inventa una scorciatoia nuova.
      QPointF newPoint = e->touchPoints().at(0).pos();
      if ((m_undoPoint.x() - newPoint.x()) > 100 &&
          Preferences::instance()->getGestureUndoMethod() ==
              Preferences::ThreeFingerDragLeft) {
        gestureUndo();
        m_undoPoint   = newPoint;
        m_touchPoints = 100;
      }
      if ((m_undoPoint.x() - newPoint.x()) < -100 &&
          Preferences::instance()->getGestureRedoMethod() ==
              Preferences::ThreeFingerDragRight) {
        gestureRedo();
        m_undoPoint   = newPoint;
        m_touchPoints = 100;
      }
    }
  }
  if (type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
    m_touchActive  = false;
    m_touchPanning = false;
    // Un tocco LUNGO non e' un tocco secco: se sono rimaste giu' piu' dita per
    // piu' di un quarto di secondo, non vale come gesture di annulla/ripeti.
    if (m_touchClock.isValid() && m_touchClock.elapsed() > 250 &&
        m_touchPoints > 1)
      m_touchPoints = 100;
  }
  e->accept();
}

void ZtoryThumbnailCanvas::gestureEvent(QGestureEvent *e) {
  // Portata da ImageViewer::gestureEvent (imageviewer.cpp).
  m_gestureActive = false;
  if (e->gesture(Qt::SwipeGesture)) {
    m_gestureActive = true;
  } else if (e->gesture(Qt::PanGesture)) {
    m_gestureActive = true;
  } else if (e->gesture(Qt::TapGesture)) {
    // ➕ Il Tap non c'e' in ImageViewer, e non e' una dimenticanza: li' non si
    // disegna, quindi un tocco fermo non fa danni. Qui si', lascerebbe un
    // punto col pennello attivo. SceneViewer — che e' la superficie da
    // disegno — infatti lo cattura, ed e' da li' che viene questo ramo.
    m_gestureActive = true;
  }
  if (QGesture *pinch = e->gesture(Qt::PinchGesture)) {
    QPinchGesture *gesture = static_cast<QPinchGesture *>(pinch);
    QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();
    QPoint firstCenter                     = gesture->centerPoint().toPoint();
    if (m_touchDevice == QTouchDevice::TouchScreen)
      firstCenter = mapFromGlobal(firstCenter);

    if (gesture->state() == Qt::GestureStarted) {
      m_gestureActive = true;
    } else if (gesture->state() == Qt::GestureFinished) {
      m_gestureActive = false;
      m_zooming       = false;
      m_scaleFactor   = 0.0;
    } else {
      if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        double scaleFactor = gesture->scaleFactor();
        // the scale factor makes for too sensitive scaling
        // divide the change in half
        if (scaleFactor > 1) {
          double decimalValue = scaleFactor - 1;
          decimalValue /= 1.5;
          scaleFactor = 1 + decimalValue;
        } else if (scaleFactor < 1) {
          double decimalValue = 1 - scaleFactor;
          decimalValue /= 1.5;
          scaleFactor = 1 - decimalValue;
        }
        if (!m_zooming) {
          double delta = scaleFactor - 1;
          m_scaleFactor += delta;
          if (m_scaleFactor > .2 || m_scaleFactor < -.2) {
            m_zooming = true;
          }
        }
        if (m_zooming) {
          // ◆ DIVERSO dall'originale: zoomQt() vuole un punto in coordinate GL
          // rispetto al centro del widget; zoomAt() vuole il punto del widget.
          zoomAt(QPointF(firstCenter), scaleFactor);
          updateScrollBars();
          m_touchPanning = false;
          // ⚠️ QUESTA RIGA E' COSTATA I DISEGNI DI UN UTENTE. Senza, dopo un
          // pizzico m_touchPoints resta 2, e al rilascio mouseReleaseEvent lo
          // legge come «tocco secco a due dita» = ANNULLA (che su Windows e' la
          // preferenza predefinita). Cioe' ogni zoom col tocco finiva con un
          // undo silenzioso: i pannelli tornavano a uno stato precedente e i
          // disegni sparivano. L'utente non riusciva nemmeno a rimediare con
          // l'undo, perche' il danno ERA un undo — serviva il REDO.
          // SceneViewer ce l'ha, col suo commento «This will block undo/redo
          // action» (sceneviewerevents.cpp:1390), e io l'ho persa nel port.
          m_touchPoints = 100;
        }
        m_gestureActive = true;
      }
      if (changeFlags & QPinchGesture::CenterPointChanged) {
        // Stesso motivo della riga qui sopra: se le dita si sono SPOSTATE, il
        // tocco ha gia' fatto qualcosa e non e' piu' un tap da annullamento.
        const QPointF centerDelta =
            gesture->centerPoint() - gesture->lastCenterPoint();
        if (centerDelta.manhattanLength() > 10) m_touchPoints = 100;
        m_gestureActive = true;
      }
    }
  }
  e->accept();
}

void ZtoryThumbnailCanvas::wheelEvent(QWheelEvent *e) {
  // ⚠️ Su un TRACKPAD lo scorrimento a due dita arriva due volte: come
  // evento touch (che qui sposta la tela) e come rotella (che zooma). Senza
  // questa riga la tela si sposterebbe e si ingrandirebbe insieme. Sullo
  // schermo touch il problema non esiste, e la rotella resta quella del mouse.
  // Stessa guardia di SceneViewer, che ci era gia' inciampato.
  if (m_gestureActive && m_touchDevice == QTouchDevice::TouchPad) {
    e->accept();
    return;
  }
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
    // ⚠️ premultiply=FALSE, e non e' una svista: qui c'era `true` ed e' costato
    // il bordo scuro attorno alle cancellature E il colore perso dentro.
    //
    // rasterToQImage(..., premul=true) non converte niente: ETICHETTA i byte
    // come gia' premoltiplicati, e il painter lavora in quello spazio. Quindi
    // l'immagine che torna indietro E' GIA' premoltiplicata. Ma
    // rasterFromQImage(..., premultiply=true) chiama TRop::premultiply(), che
    // moltiplica i canali per l'alpha UN'ALTRA VOLTA.
    //
    // Sui pixel opachi non cambia nulla — ed e' per questo che il grosso del
    // disegno stava bene e il difetto sembrava capriccioso. Sui pixel a
    // trasparenza PARZIALE, cioe' il bordo morbido di gomma e lazo, i canali si
    // schiacciano verso lo zero a ogni operazione: un azzurro (37,41,74)
    // diventa (13,15,27), poi (4,5,10), poi (1,1,3). I rapporti fra i canali si
    // perdono nell'arrotondamento e il colore diventa grigio.
    //
    // Misurato sulla tela vera di Franco prima della correzione: i pixel del
    // bordo erano (0,0,0) con alpha 95 — grigio 160 sul bianco. Esattamente
    // dove finisce un colore premoltiplicato due o tre volte.
    m_ras = rasterFromQImage(canvasImg, /*premultiply=*/false, /*mirror=*/true);
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
  // ⚠️ premultiply=FALSE, e non e' una svista: qui c'era `true` ed e' costato
  // il bordo scuro attorno alle cancellature E il colore perso dentro.
  //
  // rasterToQImage(..., premul=true) non converte niente: ETICHETTA i byte
  // come gia' premoltiplicati, e il painter lavora in quello spazio. Quindi
  // l'immagine che torna indietro E' GIA' premoltiplicata. Ma
  // rasterFromQImage(..., premultiply=true) chiama TRop::premultiply(), che
  // moltiplica i canali per l'alpha UN'ALTRA VOLTA.
  //
  // Sui pixel opachi non cambia nulla — ed e' per questo che il grosso del
  // disegno stava bene e il difetto sembrava capriccioso. Sui pixel a
  // trasparenza PARZIALE, cioe' il bordo morbido di gomma e lazo, i canali si
  // schiacciano verso lo zero a ogni operazione: un azzurro (37,41,74)
  // diventa (13,15,27), poi (4,5,10), poi (1,1,3). I rapporti fra i canali si
  // perdono nell'arrotondamento e il colore diventa grigio.
  //
  // Misurato sulla tela vera di Franco prima della correzione: i pixel del
  // bordo erano (0,0,0) con alpha 95 — grigio 160 sul bianco. Esattamente
  // dove finisce un colore premoltiplicato due o tre volte.
  m_ras      = rasterFromQImage(canvasImg, /*premultiply=*/false, /*mirror=*/true);
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
    // ⚠️ premultiply=FALSE, e non e' una svista: qui c'era `true` ed e' costato
    // il bordo scuro attorno alle cancellature E il colore perso dentro.
    //
    // rasterToQImage(..., premul=true) non converte niente: ETICHETTA i byte
    // come gia' premoltiplicati, e il painter lavora in quello spazio. Quindi
    // l'immagine che torna indietro E' GIA' premoltiplicata. Ma
    // rasterFromQImage(..., premultiply=true) chiama TRop::premultiply(), che
    // moltiplica i canali per l'alpha UN'ALTRA VOLTA.
    //
    // Sui pixel opachi non cambia nulla — ed e' per questo che il grosso del
    // disegno stava bene e il difetto sembrava capriccioso. Sui pixel a
    // trasparenza PARZIALE, cioe' il bordo morbido di gomma e lazo, i canali si
    // schiacciano verso lo zero a ogni operazione: un azzurro (37,41,74)
    // diventa (13,15,27), poi (4,5,10), poi (1,1,3). I rapporti fra i canali si
    // perdono nell'arrotondamento e il colore diventa grigio.
    //
    // Misurato sulla tela vera di Franco prima della correzione: i pixel del
    // bordo erano (0,0,0) con alpha 95 — grigio 160 sul bianco. Esattamente
    // dove finisce un colore premoltiplicato due o tre volte.
    m_ras = rasterFromQImage(canvasImg, /*premultiply=*/false, /*mirror=*/true);
    schedulePersistSave();
  }
  // Cancel fully reverts to the pre-lift canvas (a move restored its source, a
  // copy/paste never touched it), so the snapshot pushed when the float was
  // created now matches the current state exactly. Drop it so Esc leaves no
  // dangling undo step that would make the next Cmd+Z a silent no-op.
  if (!m_undo.empty()) m_undo.pop_back();
  m_floatImg  = QImage();
  m_floatDrag = -1;
  update();
}

void ZtoryThumbnailCanvas::deleteFloat() {
  if (!hasFloat()) return;
  // Snapshot the canvas AND the float we're about to drop, so Cmd+Z brings the
  // deleted selection back (the user expects Del to be undoable). makeMetaSnapshot
  // captures the still-live float; pushUndo clones the raster alongside it.
  pushUndo();
  m_floatImg  = QImage();  // source already cleared on lift (for a move)
  m_floatDrag = -1;
  update();
}

//=============================================================================
// Undo / redo — full-canvas snapshots (raster + grid + merges)
//=============================================================================

static const int kUndoTile = 256;  // tile side for stroke copy-on-write

ZtoryThumbnailCanvas::Snapshot ZtoryThumbnailCanvas::makeMetaSnapshot() const {
  Snapshot s;
  s.ras       = TRaster32P();
  s.cols      = m_cols;
  s.rows      = m_rows;
  s.merges    = m_merges;
  s.boxAspect = m_boxAspect;
  // Carry the live floating selection so undo/redo can restore it (see Snapshot).
  s.floatImg      = m_floatImg;
  s.floatCenter   = m_floatCenter;
  s.floatScale    = m_floatScale;
  s.floatAngle    = m_floatAngle;
  s.floatWasMove  = m_floatWasMove;
  s.floatSrcRect  = m_floatSrcRect;
  return s;
}

void ZtoryThumbnailCanvas::trimHistory() {
  // Sixteen steps is stingy for someone sketching, and it was the binding limit:
  // a normal stroke snapshot is a handful of 256x256 tiles (~1,5 MB), so the
  // byte budget below would allow well over a hundred of them.  Raising the
  // count is only safe BECAUSE that budget exists — before it, sixteen full
  // clones were already 823 MB.  Expensive steps still get trimmed by bytes;
  // cheap ones now go as deep as the artist is likely to want.
  static const size_t kMaxUndo = 100;
  // Budget the history by BYTES as well, because counting entries says nothing
  // about what they cost.  A stroke snapshot is a handful of 256x256 tiles, but
  // a resize / paste / transform clones the WHOLE canvas — 51 MB at 4x26, and
  // the canvas only grows.  Sixteen of those are 823 MB (1.9 GB at 4x60), which
  // on a laptop means swapping, and the slowdown never lifts because the
  // history goes on holding the memory.  That is the storyboard artist's "it
  // started slowing down as soon as I added rows": addRow() calls pushUndo(),
  // and pushUndo() clones everything.
  static const size_t kMaxUndoBytes = 256u * 1024u * 1024u;

  auto bytesOf = [](const Snapshot &s) -> size_t {
    size_t n = 0;
    if (s.ras)
      n += (size_t)s.ras->getLx() * (size_t)s.ras->getLy() * 4u;
    for (const Patch &p : s.patches)
      if (p.before)
        n += (size_t)p.before->getLx() * (size_t)p.before->getLy() * 4u;
    if (!s.floatImg.isNull()) n += (size_t)s.floatImg.sizeInBytes();
    return n;
  };

  if (m_undo.size() > kMaxUndo) m_undo.erase(m_undo.begin());

  size_t bytes = 0;
  for (const Snapshot &s : m_undo) bytes += bytesOf(s);
  // Always keep one step, whatever it costs: an undo you cannot take is worse
  // than an expensive one, and on a very large canvas a single full snapshot
  // can exceed the budget on its own.
  while (m_undo.size() > 1 && bytes > kMaxUndoBytes) {
    bytes -= bytesOf(m_undo.front());
    m_undo.erase(m_undo.begin());
  }
  m_redo.clear();  // a fresh edit invalidates the redo branch
}

void ZtoryThumbnailCanvas::pushUndo() {
  if (!m_ras) return;
  Snapshot s = makeMetaSnapshot();
  s.ras      = m_ras->clone();
  m_undo.push_back(s);
  trimHistory();
}

// Copy the tiles listed in \a like out of the current raster (used to build the
// opposite-direction snapshot when undoing/redoing a stroke).
std::vector<ZtoryThumbnailCanvas::Patch>
ZtoryThumbnailCanvas::capturePatchesAt(const std::vector<Patch> &like) const {
  std::vector<Patch> out;
  if (!m_ras) return out;
  out.reserve(like.size());
  for (const Patch &p : like) {
    const int lx = p.before->getLx(), ly = p.before->getLy();
    // Geometry changed under us (undo across a resize): skip rather than
    // read out of bounds.  The resize's own full snapshot restores the pixels.
    if (p.pos.x < 0 || p.pos.y < 0 || p.pos.x + lx > m_ras->getLx() ||
        p.pos.y + ly > m_ras->getLy())
      continue;
    TRaster32P cur(lx, ly);
    cur->copy(m_ras->extract(p.pos.x, p.pos.y, p.pos.x + lx - 1,
                             p.pos.y + ly - 1));
    out.push_back({p.pos, cur});
  }
  return out;
}

void ZtoryThumbnailCanvas::applyPatches(const std::vector<Patch> &patches) {
  if (!m_ras) return;
  for (const Patch &p : patches) {
    if (p.pos.x < 0 || p.pos.y < 0 ||
        p.pos.x + p.before->getLx() > m_ras->getLx() ||
        p.pos.y + p.before->getLy() > m_ras->getLy())
      continue;
    m_ras->copy(p.before, p.pos);
  }
}

void ZtoryThumbnailCanvas::beginStrokeRecording() {
  m_strokeBounds = QRect();
  m_strokeTiles.clear();
  m_recordingStroke = true;
}

// Turn the tiles saved during the stroke into one undo entry.  Nothing touched
// → nothing to undo.
void ZtoryThumbnailCanvas::endStrokeRecording() {
  m_recordingStroke = false;
  if (m_strokeTiles.empty()) return;
  Snapshot s = makeMetaSnapshot();
  s.patches.reserve(m_strokeTiles.size());
  for (auto &kv : m_strokeTiles)
    s.patches.push_back(
        {TPoint(kv.first.first * kUndoTile, kv.first.second * kUndoTile),
         kv.second});
  m_strokeTiles.clear();
  m_undo.push_back(std::move(s));
  trimHistory();
}

// The brush calls this before writing \a rect: save the untouched pixels of any
// tile it overlaps, once.  This is the whole stroke undo cost.
// The engine calls askRead() for EVERY dab, but askWrite() only when one of the
// blend modes is on (mypainthelpers.hpp) — and an eraser dab turns none of them
// on.  So erasing announced nothing: no tiles were recorded, endStrokeRecording()
// bailed out on an empty set and the stroke never reached the undo stack (Cmd+Z
// after erasing undid whatever came before), and no repaint region was built so
// the painted brush circle sat frozen where the stroke started.
// Recording on the read side covers both: a dab always reads what it is about
// to change.  Tiles that end up unchanged cost one copy-on-write each and
// restore identical pixels, which is harmless.
bool ZtoryThumbnailCanvas::askRead(const TRect &rect) { return askWrite(rect); }

bool ZtoryThumbnailCanvas::askWrite(const TRect &rect) {
  if (!m_ras) return true;
  const int rx0 = std::max(0, rect.x0), ry0 = std::max(0, rect.y0);
  const int rx1 = std::min(m_ras->getLx() - 1, rect.x1);
  const int ry1 = std::min(m_ras->getLy() - 1, rect.y1);
  if (rx0 > rx1 || ry0 > ry1) return true;

  // Remember what is about to change so strokeTo() can repaint just this.
  // Collected even when the undo recorder is off: the repaint has to be right
  // regardless of whether the change is undoable.
  const QRect dab(rx0, ry0, rx1 - rx0 + 1, ry1 - ry0 + 1);
  m_strokeDirty = m_strokeDirty.isNull() ? dab : m_strokeDirty.united(dab);
  // m_strokeDirty is consumed and cleared at every repaint; keep the union of
  // the WHOLE stroke as well, so the save knows exactly which bands to rewrite.
  m_strokeBounds = m_strokeBounds.isNull() ? dab : m_strokeBounds.united(dab);

  if (!m_recordingStroke) return true;

  for (int ty = ry0 / kUndoTile; ty <= ry1 / kUndoTile; ++ty)
    for (int tx = rx0 / kUndoTile; tx <= rx1 / kUndoTile; ++tx) {
      auto key = std::make_pair(tx, ty);
      if (m_strokeTiles.count(key)) continue;  // already saved: copy-on-write
      const int x0 = tx * kUndoTile, y0 = ty * kUndoTile;
      const int x1 = std::min(x0 + kUndoTile - 1, m_ras->getLx() - 1);
      const int y1 = std::min(y0 + kUndoTile - 1, m_ras->getLy() - 1);
      TRaster32P tile(x1 - x0 + 1, y1 - y0 + 1);
      tile->copy(m_ras->extract(x0, y0, x1, y1));
      m_strokeTiles[key] = tile;
    }
  return true;
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
  // Restore whatever floating selection was captured with this snapshot (a null
  // image simply clears the float) — this is what makes an undone Del re-float
  // the drawing, and a redone one drop it again.
  m_floatImg     = s.floatImg;
  m_floatCenter  = s.floatCenter;
  m_floatScale   = s.floatScale;
  m_floatAngle   = s.floatAngle;
  m_floatWasMove = s.floatWasMove;
  m_floatSrcRect = s.floatSrcRect;
  m_floatDrag = -1;
  clearSelection();
  schedulePersistSave();
  updateScrollBars();
  update();
}

void ZtoryThumbnailCanvas::restoreGeometry(const Snapshot &s) {
  if (!m_ras) return;
  m_cols = s.cols;
  if (s.boxAspect > 0.0) {
    m_boxAspect = s.boxAspect;
    m_boxH      = m_boxW / s.boxAspect;
  }
  const int oldH = m_ras->getLy();
  m_rows         = s.rows;
  const int newH = (int)gridH();
  if (newH != oldH && newH > 0) {
    TRaster32P nr((int)gridW(), newH);
    nr->fill(kPaper);
    // The raster is bottom-up, so the world bottom is low Y: growing pushes the
    // content up by the difference, shrinking drops that band off the bottom.
    if (newH > oldH)
      nr->copy(m_ras, TPoint(0, newH - oldH));
    else
      nr->copy(m_ras->extract(0, oldH - newH, m_ras->getLx() - 1, oldH - 1),
               TPoint(0, 0));
    m_ras = nr;
  }
  m_merges = s.merges;
  clearSelection();
  schedulePersistSave();
  updateScrollBars();
  update();
}

// A patch snapshot only swaps the touched tiles: build the opposite entry from
// the current pixels of those same tiles, then paste the stored ones back.
void ZtoryThumbnailCanvas::undo() {
  if (m_undo.empty()) return;
  if (!m_ras) return;
  Snapshot s = m_undo.back();
  m_undo.pop_back();

  // Geometry-only: the opposite entry is geometry-only too, so undoing and
  // redoing a row costs nothing on either side.
  if (s.geometryOnly) {
    Snapshot cur     = makeMetaSnapshot();
    cur.geometryOnly = true;
    m_redo.push_back(std::move(cur));
    restoreGeometry(s);
    return;
  }
  if (s.ras) {
    Snapshot cur = makeMetaSnapshot();
    cur.ras      = m_ras->clone();
    m_redo.push_back(std::move(cur));
    restoreSnapshot(s);
    return;
  }
  Snapshot cur = makeMetaSnapshot();
  cur.patches  = capturePatchesAt(s.patches);
  m_redo.push_back(std::move(cur));
  applyPatches(s.patches);
  schedulePersistSave();
  update();
}

void ZtoryThumbnailCanvas::redo() {
  if (m_redo.empty()) return;
  if (!m_ras) return;
  Snapshot s = m_redo.back();
  m_redo.pop_back();

  // Geometry-only: the opposite entry is geometry-only too, so undoing and
  // redoing a row costs nothing on either side.
  if (s.geometryOnly) {
    Snapshot cur     = makeMetaSnapshot();
    cur.geometryOnly = true;
    m_undo.push_back(std::move(cur));
    restoreGeometry(s);
    return;
  }
  if (s.ras) {
    Snapshot cur = makeMetaSnapshot();
    cur.ras      = m_ras->clone();
    m_undo.push_back(std::move(cur));
    restoreSnapshot(s);
    return;
  }
  Snapshot cur = makeMetaSnapshot();
  cur.patches  = capturePatchesAt(s.patches);
  m_undo.push_back(std::move(cur));
  applyPatches(s.patches);
  schedulePersistSave();
  update();
}

// ⚠️ Il canvas dei thumbnail ha una pila di undo SUA (m_undo/m_redo), separata
// da TUndoManager: le pennellate qui dentro non passano dall'undo
// dell'applicazione. Le gesture pero' eseguivano `MI_Undo`, cioe' il comando
// GLOBALE — quindi partivano davvero e annullavano qualcos'altro, o niente.
// Da qui il «le gesture non funzionano nella Thumbs room ma funzionano nelle
// altre»: nelle altre room il disegno passa da TUndoManager e MI_Undo e' il
// comando giusto. Qui no.
//
// La regola e' la stessa di handleUndoKey(): se abbiamo storia nostra la
// usiamo, altrimenti si lascia fare all'applicazione. Cosi' il gesto e la
// tastiera si comportano allo stesso modo.
void ZtoryThumbnailCanvas::gestureUndo() {
  if (!m_undo.empty())
    undo();
  else
    CommandManager::instance()->execute(MI_Undo);
}

void ZtoryThumbnailCanvas::gestureRedo() {
  if (!m_redo.empty())
    redo();
  else
    CommandManager::instance()->execute(MI_Redo);
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

  // rasterToQImage() wraps the raster memory without copying, but mirrored=true
  // deep-copies the whole surface (~31 MB on a 4x15 grid) on EVERY repaint —
  // i.e. on every mouse move while drawing.  Take the zero-copy view and let
  // the painter apply the vertical flip: Qt then only touches the pixels inside
  // the clip region.
  // The page.  The surface itself is transparent (see kPaper), so the white a
  // storyboard artist draws on is painted HERE, under the drawing — that is
  // what lets the eraser take pixels away and still look like paper.
  p.fillRect(target, Qt::white);

  QImage img = rasterToQImage(m_ras, /*premultiplied=*/true, /*mirrored=*/false);
  p.save();
  p.translate(target.left(), target.top() + target.height());
  p.scale(1.0, -1.0);
  p.drawImage(QRectF(0.0, 0.0, target.width(), target.height()), img);
  p.restore();

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
