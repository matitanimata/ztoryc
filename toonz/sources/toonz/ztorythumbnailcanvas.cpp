#include "ztorythumbnailcanvas.h"

#include "ztoryshotops.h"   // cameraAspect
#include "tundo.h"
#include <QPointer>
#include "tapp.h"
#include "ztorymodel.h"  // sceneSaved: the working copy becomes official
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
#include <QThread>
#include <QRunnable>
#include <QPolygonF>
#include <QLineF>
#include <QApplication>
#include <QScrollBar>
#include <QMessageBox>
#include <QPushButton>
#include <QLocale>
#include <QDateTime>
#include <QSet>
#include "toonz/preferences.h"       // le gesture di annulla/ripeti
#include "toonzqt/menubarcommand.h"  // CommandManager
#include "menubarcommandids.h"       // MI_TouchGestureControl
#include <QTouchEvent>
#include <QGestureEvent>
#include <QGesture>
#include <QResizeEvent>

#include <cmath>
#include <climits>
#include <cstring>
#include <set>

//=============================================================================

// The page is TRANSPARENT, not opaque white.  White is painted UNDER the
// surface by paintEvent, so drawing still happens on what looks like paper —
// but an eraser can now take pixels away instead of covering them with white
// paint, which is what "erase" is supposed to mean, and what lets an exported
// panel carry its transparency.
static const TPixel32 kPaper(0, 0, 0, 0);

// How many raster rows a sheet of height \a h (world units) takes. ONE
// rounding rule for every place that creates or measures the canvas: a sheet
// read from the old single-image format gets its box height from PNG height /
// rows, and rows * that can come back as 1598.9999999 — truncated, one row
// short, and every coordinate below shifted by a pixel.
static inline int ztoryPixelRows(double h) {
  return std::max(1, (int)(h + 1e-6));
}

// One band of the canvas on its way to disk. Outside the anonymous namespace:
// the canvas collects them (collectDirtyBands), so the header names it.
struct ThumbBand {
  int index;
  QImage img;
};

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

// ── The working copy («chiudi senza salvare», Franco 2026-09-23) ──────────────
//
// The canvas autosaves 700 ms after a stroke, without waiting for the scene to
// be saved — on purpose: it is big, redrawing it costs, and losing it to a
// crash would be worse. But it made the scene stop being ONE thing: "Don't
// save" reverted the .tnz and left the thumbnails as they were. Now the
// autosave writes here, a scene save moves it over the official files, and a
// scene left without saving throws it away.
//
// A subfolder of the official one: it travels with the scene, and the loader's
// globs (QDir::Files) never see into it.
static const char *const kWorkingSubdir = "_ztorythumbs_working";

// Working copies written by THIS session. One found on open that is not here
// was left by a session that did not close normally, and is offered back
// instead of being applied in silence; one that IS here is ours — a Thumbnail
// room panel rebuilt mid-session picks it up without asking.
static QSet<QString> s_sessionWorkDirs;

static bool readThumbManifest(const QString &dirStr, int &cols, int &rows,
                              double &boxH, int &bands) {
  QFile gf(dirStr + "/_ztorythumbs_grid.txt");
  if (!gf.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
  QTextStream ts(&gf);
  cols = rows = bands = 0;
  boxH            = 0.0;
  ts >> cols >> rows >> boxH >> bands;
  return cols > 0 && rows > 0 && boxH > 0.0 && bands > 0;
}

// Save As: the thumbnails follow the scene to its new name. The destination is
// cleaned first even when there is nothing to copy — it may be the folder of a
// scene being overwritten, and its thumbnails must not survive under ours.
static void copyThumbFiles(const QString &srcDir, const QString &dstDir) {
  const QStringList pats{"_ztorythumbs_*", "ztorythumbs_backup_*"};
  QDir dst(dstDir);
  if (!dst.exists() && !dst.mkpath(".")) return;
  for (const QString &f : dst.entryList(pats, QDir::Files)) dst.remove(f);
  QDir src(srcDir);
  if (!src.exists()) return;
  for (const QString &f : src.entryList(pats, QDir::Files))
    QFile::copy(srcDir + "/" + f, dstDir + "/" + f);
}

// Move the working copy over the official files. The manifest goes LAST, and
// the working folder is removed only once everything is across: a failure — or
// a crash — halfway leaves it in place, and the next open offers it back.
// Nothing is ever the only copy while it moves.
static void promoteThumbWorkingCopy(const QString &workDir,
                                    const QString &offDir) {
  int cols, rows, bands;
  double boxH;
  if (!readThumbManifest(workDir, cols, rows, boxH, bands)) {
    // No manifest: its first write never completed, so it holds nothing a
    // load would ever read. Leaving it would only get it offered back.
    QDir(workDir).removeRecursively();
    return;
  }
  QDir off(offDir);
  if (!off.exists() && !off.mkpath(".")) return;
  auto moveOver = [&](const QString &name) {
    const QString from = workDir + "/" + name, to = offDir + "/" + name;
    QFile::remove(to);
    return QFile::rename(from, to) || QFile::copy(from, to);
  };
  bool ok = true;
  for (const QString &f : QDir(workDir).entryList(
           QStringList() << "_ztorythumbs_band*.png", QDir::Files))
    ok = moveOver(f) && ok;
  // Bands beyond the working grid (rows were removed).
  QRegExp bandRe("_ztorythumbs_band(\\d+)\\.png");
  for (const QString &f :
       off.entryList(QStringList() << "_ztorythumbs_band*.png", QDir::Files))
    if (bandRe.indexIn(f) >= 0 && bandRe.cap(1).toInt() >= bands) off.remove(f);
  if (QFile::exists(workDir + "/_ztorythumbs_merges.txt"))
    ok = moveOver("_ztorythumbs_merges.txt") && ok;
  else
    QFile::remove(offDir + "/_ztorythumbs_merges.txt");
  if (!ok) return;
  if (!moveOver("_ztorythumbs_grid.txt")) return;

  // Every band is now official: the old single-image canvas can be retired,
  // renamed and not deleted, exactly as writeThumbBands does it.
  bool complete = true;
  for (int b = 0; b < bands && complete; b++)
    complete = QFile::exists(
        offDir + QString("/_ztorythumbs_band%1.png").arg(b, 3, 10, QChar('0')));
  if (complete)
    for (const QString &old :
         off.entryList(QStringList() << "_ztorythumbs_*x*.png", QDir::Files)) {
      const QString backup = "ztorythumbs_backup_" + old.mid(13);
      QFile::remove(offDir + "/" + backup);
      if (!QFile::rename(offDir + "/" + old, offDir + "/" + backup))
        off.remove(old);
    }
  QDir(workDir).removeRecursively();
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

//=============================================================================
// Router di Annulla/Ripeti
//-----------------------------------------------------------------------------
// La Thumbs room ha una pila SUA, separata da quella dell'applicazione, e la
// tiene apposta: le sue fotografie condividono le pagine fra loro e il bilancio
// della memoria le conta una volta sola — cosa che TUndoManager non sa fare,
// perche' chiede la dimensione a ogni oggetto separatamente e conterebbe la
// stessa pagina piu' volte, troncando la cronologia molto prima del necessario.
//
// Due pile separate vogliono pero' UN SOLO gestore del comando, o due
// rispondono allo stesso clic. Questo sostituisce quello di MainWindow e
// smista: se la Thumbs room e' a schermo e ha storia, tocca a lei; altrimenti
// si fa esattamente cio' che faceva MainWindow::onUndo/onRedo.
namespace {

class ZtoryUndoRouter final : public CommandHandlerInterface {
public:
  explicit ZtoryUndoRouter(bool redo) : m_redo(redo) {}
  void execute() override {
    // ⚠️ Si CERCA la tela a schermo, non si tiene "l'ultima costruita". Di
    // pannelli Thumbs ne possono esistere piu' d'uno (uno per room, anche non
    // visibili), e ricordando l'ultimo costruito si finiva a puntare a uno
    // NASCOSTO: isThumbsContextActive() diceva no e il comando passava sempre
    // all'applicazione. Da qui il "dal menu va sulla pila dell'app" mentre la
    // tastiera — che non passa di qui ma dal filtro della tela — funzionava.
    ZtoryThumbnailCanvas *c = nullptr;
    for (const auto &p : s_canvases)
      if (p && p->isThumbsContextActive()) { c = p.data(); break; }
    if (c && c->routeUndoHere(m_redo)) return;
    // Comportamento dell'applicazione, copiato da MainWindow::onUndo/onRedo —
    // attesa del salvataggio compresa, o un annullamento durante una scrittura
    // lavorerebbe su uno stato a meta'.
    while (TApp::instance()->isSaveInProgress()) {
    }
    if (m_redo)
      TUndoManager::manager()->redo();
    else
      TUndoManager::manager()->undo();
  }
  static std::vector<QPointer<ZtoryThumbnailCanvas>> s_canvases;

private:
  bool m_redo;
};

std::vector<QPointer<ZtoryThumbnailCanvas>> ZtoryUndoRouter::s_canvases;

}  // namespace

// ⚠️ SI REINSTALLA A OGNI SHOW, e non basta farlo alla nascita.
// MainWindow costruisce le room — quindi anche questo pannello — dentro
// readSettings(), e SOLO DOPO, cinque righe piu' giu', chiama
// setCommandHandler("MI_Undo", ...). E setHandler non affianca: ELIMINA il
// gestore precedente. Il nostro router nasceva e veniva distrutto prima ancora
// che l'applicazione finisse di partire, e il menu tornava all'applicazione.
// Trovato con una sonda che non ha scritto NIENTE: execute() non era mai
// nostra (2026-09-23).
static void ztoryInstallUndoRouter(ZtoryThumbnailCanvas *canvas) {
  auto &v = ZtoryUndoRouter::s_canvases;
  // Via le tele morte: i QPointer si azzerano da soli, ma l'elenco no.
  v.erase(std::remove_if(v.begin(), v.end(),
                         [](const QPointer<ZtoryThumbnailCanvas> &p) {
                           return p.isNull();
                         }),
          v.end());
  bool nuova = true;
  for (const auto &p : v)
    if (p.data() == canvas) { nuova = false; break; }
  if (nuova) v.push_back(canvas);
  // Niente guardia "una volta sola": se MainWindow ce l'ha cancellato dopo,
  // l'unico modo di riaverlo e' rimetterlo. setHandler distrugge il vecchio,
  // quindi non si accumula niente.
  CommandManager::instance()->setHandler(MI_Undo, new ZtoryUndoRouter(false));
  CommandManager::instance()->setHandler(MI_Redo, new ZtoryUndoRouter(true));
}

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
    m_pan.setX(-v - pageBoxNoPan().left());
    update();
  });
  connect(m_vbar, &QScrollBar::valueChanged, this, [this](int v) {
    if (m_syncingBars) return;
    m_pan.setY(-v - pageBoxNoPan().top());
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

  m_ras = TRaster32P((int)gridW(), canvasLy());
  m_ras->fill(kPaper);


  // React live to camera changes made from Camera Settings while this room is
  // open. xsheetChanged covers most camera edits; sceneChanged covers a scene
  // load/switch with a different camera.
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &ZtoryThumbnailCanvas::onSceneChanged);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneChanged, this,
          &ZtoryThumbnailCanvas::onSceneChanged);

  // Le voci Annulla/Ripeti del menu Edit agiscono sulla tela quando e' lei ad
  // avere la storia. Il gestore dell'applicazione gira comunque, ma con la sua
  // pila vuota — e ci arriviamo solo in quel caso — non fa niente.
  // MainWindow::onHistoryChanged() riscrive lo stato delle due voci a ogni
  // cambio della cronologia dell'APPLICAZIONE, cancellando il nostro. Ci
  // agganciamo allo stesso segnale: siamo costruiti dopo la finestra
  // principale, quindi il nostro slot gira per ultimo ed e' l'ultima parola.
  if (TUndoManager *um = TUndoManager::manager())
    connect(um, &TUndoManager::historyChanged, this,
            &ZtoryThumbnailCanvas::syncAppUndoActions);

  // Un SOLO gestore per Annulla/Ripeti, che smista. Qui ce n'erano due che
  // rispondevano allo stesso clic — il nostro agganciato a triggered() e quello
  // di MainWindow — e vinceva il suo: il menu ignorava le operazioni della
  // Thumbs room. Due programmi adiacenti non possono rispondere entrambi allo
  // stesso comando.
  ztoryInstallUndoRouter(this);

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
  // The working copy: official on a scene save, thrown away when the scene is
  // left without one. sceneSwitching (NOT sceneSwitched) fires only when the
  // scene object really changes — New, Load, Revert — and always AFTER the
  // "save changes?" question; re-selecting the same scene does not emit it.
  // aboutToQuit comes after MainWindow's own question on Quit.
  connect(ZtoryModel::instance(), &ZtoryModel::sceneSaved, this,
          &ZtoryThumbnailCanvas::onSceneSaved);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitching,
          this, &ZtoryThumbnailCanvas::discardWorkingCopy);
  connect(qApp, &QCoreApplication::aboutToQuit, this,
          &ZtoryThumbnailCanvas::discardWorkingCopy);
  // Transform-tool shortcuts must work even when a toolbar button holds focus.
  qApp->installEventFilter(this);
  // Load the scene that is already open when the panel is created.
  persistLoad();
  // Autocollaudo della finestra, solo su richiesta (vedi runPagingSelfTest).
  if (qEnvironmentVariableIntValue("ZTORYC_THUMBS_SELFTEST") == 1)
    QTimer::singleShot(4000, this, [this]() { runPagingSelfTest(); });
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
  // Into the working copy, as bands — like every other autosave. This used to
  // write the OLD single-image format into the official folder, which the
  // loader ignores as soon as a band manifest exists: an edit made in the last
  // 700 ms before quitting was written, and then never read back.
  // (On Quit, aboutToQuit has already run discardWorkingCopy(), which stops the
  // timer: nothing is pending here and nothing is written — correctly, since
  // the user has just been asked whether to keep it.)
  if (pending && m_ras) writeDirtyBandsNow();
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
  // Sulle pagine: cambia solo l'ultima, le altre si riusano (vedi
  // resizeRowsPagewise). Il contenuto resta alla stessa y del mondo.
  resizeRowsPagewise(m_rows + 1);
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
  if (minRows > m_rows) resizeRowsPagewise(minRows);

  const int lx = m_ras->getLx(), ly = canvasLy();  // ly: righe della TELA
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
    // Una pagina importata puo' cadere ovunque nel foglio: la finestra la
    // raggiunge prima di scriverci (ry0/ry1 sono righe della tela).
    ensureWindowCoversRows(ry0, ry1 - 1);
    const int off = winY0();

    // Smooth-scale to the box, then copy with a vertical flip: the QImage's top
    // row (j = 0) goes to the box's top, which is the HIGHEST raster row.
    const QImage s = b.image
                         .scaled(dw, dh, Qt::IgnoreAspectRatio,
                                 Qt::SmoothTransformation)
                         .convertToFormat(QImage::Format_RGB888);
    m_ras->lock();
    for (int j = 0; j < dh; ++j) {
      const int rr = ry1 - 1 - j - off;  // riga della FINESTRA
      if (rr < 0 || rr >= m_ras->getLy()) continue;
      TPixel32 *drow      = m_ras->pixels(rr);
      const uchar *srow   = s.scanLine(j);
      for (int i = 0; i < dw; ++i) {
        const uchar *px = srow + i * 3;
        drow[x0 + i]    = TPixel32(px[0], px[1], px[2], 255);
      }
    }
    m_ras->unlock();
  }
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
  // Straighten first: "fit the grid width" is undefined on a tilted sheet, and
  // someone asking to be shown a row (typically right after importing pages)
  // wants to see it square, not at the angle they were drawing at.
  m_rot = 0.0;
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
  // Tutta la TELA, anche le pagine fuori dalla finestra.
  const TRaster32P all = readCanvas(0, 0, (int)gridW() - 1, canvasLy() - 1);
  if (!all) return QImage();
  return rasterToQImage(all, /*premultiplied=*/true).copy();
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
  const int oldH       = m_ras ? canvasLy() : 0;  // la TELA, non la finestra
  if (oldH <= 0 || oldBoxH <= 0.0 || !m_ras) return;

  const double newBoxH = m_boxW / aspect;
  const int newH       = ztoryPixelRows(m_rows * newBoxH);
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
  // Si ricompone la tela intera il solo tempo di re-impaginarla: cambia la
  // forma di TUTTE le pagine, e il re-impaginamento lavora sulla tela.
  flushWindowToPages();
  const TRaster32P whole = canvasFromPages(m_pages, (int)gridW(), oldH);
  m_boxAspect = aspect;
  m_boxH      = newBoxH;
  m_pages     = pagesFromCanvas(reanchorRaster(whole, oldBoxH, m_boxH));
  rebuildWindowFromPages();
  // EVERY page changed height: all of them go to disk, and the page store is
  // refilled from the window (flushWindowToPages takes the dirty ones). Without
  // this a reflow after the first save was persisted one page at a time, as
  // strokes happened to touch them — a canvas with pages of two heights on
  // disk. It went unnoticed because at scene open, where reflows usually
  // happen, every page is still dirty from the load.
  // Scheduled WITHOUT marking the scene modified: a reflow is derived from the
  // camera, not work, and if nobody saves, the next open simply reflows again.
  markAllBandsDirty();
  schedulePersistSaveTimer();
  update();
}

TRaster32P ZtoryThumbnailCanvas::reanchorRaster(const TRaster32P &oldRas,
                                                double oldBoxH,
                                                double newBoxH) const {
  const int oldW = oldRas ? oldRas->getLx() : 0;
  const int oldH = oldRas ? oldRas->getLy() : 0;
  const int gw   = (int)gridW();
  const int newH = ztoryPixelRows(m_rows * newBoxH);
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
  const int lx = m_ras->getLx(), ly = canvasLy();  // righe della TELA
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

  // Il pannello puo' stare fuori dalla finestra (lastNonEmptyRow li scorre
  // tutti): si legge dalla TELA — finestra dove c'e', pagine altrove.
  const TRaster32P area = readCanvas(ix0, iy0, ix1 - 1, iy1 - 1);
  if (!area) return true;
  area->lock();
  int ink = 0;
  for (int y = 0; y < area->getLy() && ink <= kMinInk; ++y) {
    const TPixel32 *pix = area->pixels(y);
    for (int x = 0; x < area->getLx(); ++x)
      if (ztoryIsInk(pix[x]) && ++ink > kMinInk) break;
  }
  area->unlock();
  return ink <= kMinInk;
}

TRaster32P ZtoryThumbnailCanvas::panelRaster(int index, const TDimension &outRes,
                                            bool onWhite) const {
  if (!m_ras || index < 0 || index >= m_cols * m_rows) return TRaster32P();
  const QRect br = regionBoxRect(index);  // whole region (merged or single box)
  const int lx = m_ras->getLx(), ly = canvasLy();  // righe della TELA
  const int x0 = qBound(0, (int)(br.x() * m_boxW), lx);
  const int x1 = qBound(0, (int)((br.x() + br.width()) * m_boxW), lx);
  // World y is top-down; the raster is bottom-up, so flip when computing rows.
  const int ry0 = qBound(0, (int)(ly - (br.y() + br.height()) * m_boxH), ly);
  const int ry1 = qBound(0, (int)(ly - br.y() * m_boxH), ly);
  if (x1 <= x0 || ry1 <= ry0 || outRes.lx <= 0 || outRes.ly <= 0)
    return TRaster32P();

  // Dalla TELA: il pannello puo' stare fuori dalla finestra (esportazione al
  // Board, anteprime). readCanvas() da' una copia contigua.
  TRaster32P sub = readCanvas(x0, ry0, x1 - 1, ry1 - 1);
  if (!sub) return TRaster32P();
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

// Both are called for edits the USER made, so both mark the scene modified.
// They never did, and without it "Don't save" could not work: with only the
// thumbnails changed, closing asked nothing at all, and there was no way to say
// "keep them" (2026-09-24). The camera reflow is not an edit and does not go
// through here — see onSceneChanged().
void ZtoryThumbnailCanvas::schedulePersistSave(const QRect &rasterRect) {
  markBandsDirty(rasterRect);
  schedulePersistSaveTimer();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
}

void ZtoryThumbnailCanvas::schedulePersistSave() {
  markAllBandsDirty();
  schedulePersistSaveTimer();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
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

// ── Raster per pagina, passo 1 ──────────────────────────────────────────────

std::vector<TRaster32P> ZtoryThumbnailCanvas::pagesFromCanvas(
    const TRaster32P &canvas) const {
  std::vector<TRaster32P> pages;
  if (!canvas) return pages;
  const int lx = canvas->getLx(), ly = canvas->getLy();
  const int n  = bandCount();
  pages.reserve(n);
  for (int b = 0; b < n; b++) {
    int y0, y1;
    bandRasterRange(b, ly, y0, y1);
    if (y0 > y1 || y1 < 0 || y0 >= ly) { pages.push_back(TRaster32P()); continue; }
    // Copia in un raster CONTIGUO, non una vista: una vista conserva il passo
    // di riga del genitore, e chi poi la codifica in PNG non lo sa — l'immagine
    // uscirebbe storta.  Stessa ragione per cui persistSave copia.
    TRaster32P page(lx, y1 - y0 + 1);
    page->copy(canvas->extract(0, y0, lx - 1, y1));
    pages.push_back(page);
  }
  return pages;
}

TRaster32P ZtoryThumbnailCanvas::canvasFromPages(
    const std::vector<TRaster32P> &pages, int lx, int ly) const {
  if (lx <= 0 || ly <= 0) return TRaster32P();
  TRaster32P canvas(lx, ly);
  canvas->fill(kPaper);
  for (int b = 0; b < (int)pages.size(); b++) {
    if (!pages[b]) continue;  // pagina mancante = carta bianca, non un buco
    int y0, y1;
    bandRasterRange(b, ly, y0, y1);
    if (y0 > y1 || y1 < 0 || y0 >= ly) continue;
    const int h = qMin(pages[b]->getLy(), y1 - y0 + 1);
    if (h <= 0) continue;
    // Allineate dall'ALTO della pagina, come fa il caricatore delle bande:
    // «so a short last band lands where it was cut from». Finche' la pagina e'
    // alta quanto il suo intervallo le due scelte coincidono — cioe' sempre,
    // oggi — ma divergono proprio nei casi storti (una pagina salvata con una
    // camera diversa), che sono quelli in cui un disallineamento si porta via
    // una striscia di disegno.
    canvas->extract(0, y1 - h + 1, lx - 1, y1)
        ->copy(pages[b]->extract(0, pages[b]->getLy() - h, lx - 1,
                                 pages[b]->getLy() - 1));
  }
  return canvas;
}

bool ZtoryThumbnailCanvas::pagingRoundTripIsIdentity(
    const TRaster32P &canvas) const {
  if (!canvas) return true;
  const int lx = canvas->getLx(), ly = canvas->getLy();
  TRaster32P back = canvasFromPages(pagesFromCanvas(canvas), lx, ly);
  if (!back || back->getLx() != lx || back->getLy() != ly) return false;
  for (int y = 0; y < ly; y++) {
    const TPixel32 *a = canvas->pixels(y);
    const TPixel32 *b = back->pixels(y);
    if (std::memcmp(a, b, sizeof(TPixel32) * lx) != 0) return false;
  }
  return true;
}

void ZtoryThumbnailCanvas::syncPageCount() {
  const int n = bandCount();
  if ((int)m_pages.size() != n) m_pages.resize(n);
}

// Finestra -> pagine. Riversa OGNI pagina della finestra che differisce dalla
// sua copia in magazzino, e la marca da salvare lei stessa.
//
// Prima si fidava delle marcature (m_bandDirty): copiava solo le pagine gia'
// segnate. Con la finestra piena era innocuo — la finestra ERA la tela, e una
// modifica non marcata restava comunque sotto gli occhi. Con la finestra che
// si sposta, una strada che si dimentica di marcare avrebbe perso la sua
// modifica al primo spostamento, e in silenzio. Confrontare costa una memcmp
// della finestra; dimenticare costerebbe un disegno. E' la regola decisa a
// settembre: una strada dimenticata deve costare prestazioni, non dati.
void ZtoryThumbnailCanvas::flushWindowToPages() {
  if (!m_ras) return;
  syncPageCount();
  const int n = bandCount();
  if ((int)m_bandDirty.size() != n) m_bandDirty.resize(n, true);
  const int lx = m_ras->getLx(), wly = m_ras->getLy();
  const int cly = canvasLy(), off = winY0();
  const int first = winFirst(), last = first + winCount() - 1;
  m_ras->lock();
  for (int b = first; b <= last && b < n; b++) {
    int y0, y1;
    bandRasterRange(b, cly, y0, y1);
    const int wy0 = y0 - off, wy1 = y1 - off;  // righe della finestra
    if (wy0 > wy1 || wy1 < 0 || wy0 >= wly) continue;
    const int h = wy1 - wy0 + 1;
    const TRaster32P &old = m_pages[b];
    if (old && old->getLx() == lx && old->getLy() == h) {
      bool same = true;
      old->lock();
      for (int y = 0; y < h && same; y++)
        same = std::memcmp(old->pixels(y), m_ras->pixels(wy0 + y),
                           sizeof(TPixel32) * lx) == 0;
      old->unlock();
      if (same) continue;
    }
    // Un raster NUOVO, mai una scrittura in quello vecchio: le fotografie
    // dell'annullamento tengono i puntatori alle pagine, e sono valide proprio
    // perche' nessuno le modifica sul posto.
    TRaster32P page(lx, h);
    page->copy(m_ras->extract(0, wy0, lx - 1, wy1));
    m_pages[b]     = page;
    m_bandDirty[b] = true;
  }
  m_ras->unlock();
}

// Pagine -> finestra, per l'intervallo di pagine che la finestra copre.
void ZtoryThumbnailCanvas::rebuildWindowFromPages() {
  const int lx = (int)gridW(), ly = canvasLy();
  if (lx <= 0 || ly <= 0) return;
  syncPageCount();
  if (m_winPageCount <= 0) {  // la finestra copre tutto: com'era
    m_ras = canvasFromPages(m_pages, lx, ly);
    return;
  }
  const int n    = bandCount();
  m_winPageCount = qMin(m_winPageCount, n);
  m_winFirstPage = qBound(0, m_winFirstPage, n - m_winPageCount);
  int t0, top, bot, b1;
  bandRasterRange(m_winFirstPage, ly, t0, top);
  bandRasterRange(m_winFirstPage + m_winPageCount - 1, ly, bot, b1);
  TRaster32P win(lx, top - bot + 1);
  win->fill(kPaper);
  for (int b = m_winFirstPage; b < m_winFirstPage + m_winPageCount; b++) {
    if (!m_pages[b]) continue;  // pagina mancante = carta bianca
    int y0, y1;
    bandRasterRange(b, ly, y0, y1);
    const int h = qMin(m_pages[b]->getLy(), y1 - y0 + 1);
    if (h <= 0) continue;
    // Allineata dall'ALTO, come canvasFromPages().
    win->extract(0, y1 - h + 1 - bot, lx - 1, y1 - bot)
        ->copy(m_pages[b]->extract(0, m_pages[b]->getLy() - h, lx - 1,
                                   m_pages[b]->getLy() - 1));
  }
  m_ras = win;
}

int ZtoryThumbnailCanvas::canvasLy() const { return ztoryPixelRows(gridH()); }

int ZtoryThumbnailCanvas::winFirst() const {
  if (m_winPageCount <= 0) return 0;
  return qBound(0, m_winFirstPage, qMax(0, bandCount() - 1));
}

int ZtoryThumbnailCanvas::winCount() const {
  if (m_winPageCount <= 0) return bandCount();
  return qMax(1, qMin(m_winPageCount, bandCount() - winFirst()));
}

int ZtoryThumbnailCanvas::winY0() const {
  if (m_winPageCount <= 0) return 0;
  int y0, y1;
  bandRasterRange(winFirst() + winCount() - 1, canvasLy(), y0, y1);
  return y0;
}

int ZtoryThumbnailCanvas::bandOfCanvasRow(int y) const {
  const int n     = bandCount();
  const int bandH = qMax(1, (int)std::lround(kRowsPerBand * m_boxH));
  const int fromTop = canvasLy() - 1 - y;
  if (fromTop < 0) return 0;
  // L'ultima pagina arriva fino al fondo (vedi bandRasterRange): le righe che
  // cadono oltre l'ultima pagina "piena" sono sue.
  return qBound(0, fromTop / bandH, n - 1);
}

void ZtoryThumbnailCanvas::ensureWindowCovers(int b0, int b1) {
  if (m_windowPages <= 0 && m_winPageCount <= 0) return;  // finestra piena
  const int n = bandCount();
  b0          = qBound(0, b0, n - 1);
  b1          = qBound(b0, b1, n - 1);
  if (m_winPageCount > 0 && b0 >= winFirst() &&
      b1 < winFirst() + winCount())
    return;  // gia' coperte
  const int span  = b1 - b0 + 1;
  const int count = qMin(n, qMax(m_windowPages, span));
  int first       = b0 - (count - span) / 2;
  first           = qBound(0, first, n - count);
  moveWindow(first, count);
}

void ZtoryThumbnailCanvas::ensureWindowCoversRows(int canvasY0, int canvasY1) {
  if (canvasY1 < canvasY0) std::swap(canvasY0, canvasY1);
  const int ly = canvasLy();
  canvasY0     = qBound(0, canvasY0, ly - 1);
  canvasY1     = qBound(0, canvasY1, ly - 1);
  // Righe in alto = pagine di indice basso.
  ensureWindowCovers(bandOfCanvasRow(canvasY1), bandOfCanvasRow(canvasY0));
}

void ZtoryThumbnailCanvas::moveWindow(int first, int count) {
  // Il pennello tiene m_ras per puntatore: spostare la finestra a meta'
  // pennellata lo farebbe disegnare su un raster che non e' piu' la finestra.
  if (m_stroking) return;
  flushWindowToPages();
  m_winFirstPage = first;
  m_winPageCount = count;
  rebuildWindowFromPages();
  m_windowMoves++;
}

TRaster32P ZtoryThumbnailCanvas::readCanvas(int x0, int y0, int x1,
                                            int y1) const {
  const int lx = (int)gridW(), ly = canvasLy();
  x0 = qBound(0, x0, lx - 1);
  x1 = qBound(0, x1, lx - 1);
  y0 = qBound(0, y0, ly - 1);
  y1 = qBound(0, y1, ly - 1);
  if (x1 < x0 || y1 < y0) return TRaster32P();
  TRaster32P out(x1 - x0 + 1, y1 - y0 + 1);
  out->fill(kPaper);
  const int off = winY0();
  const int wf = winFirst(), wl = wf + winCount() - 1;
  out->lock();
  if (m_ras) m_ras->lock();
  for (int y = y0; y <= y1; y++) {
    const int b = bandOfCanvasRow(y);
    const TPixel32 *src = nullptr;
    if (m_ras && b >= wf && b <= wl) {
      const int wy = y - off;
      if (wy >= 0 && wy < m_ras->getLy()) src = m_ras->pixels(wy) + x0;
    } else if (b < (int)m_pages.size() && m_pages[b]) {
      // Pagina allineata dall'alto dentro la sua banda.
      int by0, by1;
      bandRasterRange(b, ly, by0, by1);
      const TRaster32P &pg = m_pages[b];
      const int py = y - (by1 - pg->getLy() + 1);
      if (py >= 0 && py < pg->getLy() && x1 < pg->getLx())
        src = pg->pixels(py) + x0;
    }
    if (src)
      std::memcpy(out->pixels(y - y0), src, sizeof(TPixel32) * (x1 - x0 + 1));
  }
  if (m_ras) m_ras->unlock();
  out->unlock();
  return out;
}

// Righe aggiunte o tolte IN FONDO al foglio. Le pagine sono ancorate
// all'ALTO, quindi cambiano solo l'ultima pagina di prima e quelle che
// nascono o spariscono: le altre si riusano cosi' come sono (sono le stesse
// righe del mondo, alla stessa altezza). Prima si ricopiava TUTTA la tela a
// ogni riga aggiunta — 617 MB all'obiettivo di produzione.
// Da usare solo se m_boxH e m_cols non cambiano: altrimenti cambia la forma
// di tutte le pagine (vedi restoreGeometry).
void ZtoryThumbnailCanvas::resizeRowsPagewise(int newRows) {
  newRows = qMax(1, newRows);
  if (newRows == m_rows) return;
  flushWindowToPages();
  syncPageCount();
  const int lx = (int)gridW();
  const int oldN  = bandCount();
  const int oldLy = canvasLy();
  const std::vector<TRaster32P> old = m_pages;
  std::vector<int> oldH(oldN, 0);
  for (int b = 0; b < oldN; b++) {
    int y0, y1;
    bandRasterRange(b, oldLy, y0, y1);
    oldH[b] = y1 - y0 + 1;
  }
  m_rows          = newRows;
  const int n     = bandCount();
  const int newLy = canvasLy();
  std::vector<TRaster32P> np(n);
  m_bandDirty.resize(n, true);
  for (int b = 0; b < n; b++) {
    int y0, y1;
    bandRasterRange(b, newLy, y0, y1);
    const int h = y1 - y0 + 1;
    if (h <= 0) continue;
    if (b >= oldN || !old[b]) {  // pagina nuova, o mai scritta: carta bianca
      m_bandDirty[b] = true;
      continue;
    }
    if (oldH[b] == h && old[b]->getLy() == h && old[b]->getLx() == lx) {
      np[b] = old[b];  // stessa pagina, stesse righe: si condivide
      continue;
    }
    // L'ultima pagina di prima, allungata o accorciata: il contenuto resta in
    // ALTO (le righe si aggiungono e si tolgono in fondo al foglio).
    TRaster32P pg(lx, h);
    pg->fill(kPaper);
    const int ho = old[b]->getLy();
    const int wo = qMin(lx, old[b]->getLx());
    if (h >= ho)
      pg->copy(old[b]->extract(0, 0, wo - 1, ho - 1), TPoint(0, h - ho));
    else
      pg->copy(old[b]->extract(0, ho - h, wo - 1, ho - 1), TPoint(0, 0));
    np[b]          = pg;
    m_bandDirty[b] = true;
  }
  m_pages = np;
  rebuildWindowFromPages();
}

void ZtoryThumbnailCanvas::markBandsDirty(const QRect &rasterRect) {
  const int n = bandCount();
  if ((int)m_bandDirty.size() != n) m_bandDirty.resize(n, true);
  if (rasterRect.isNull() || m_boxH <= 0.0 || !m_ras) {  // unknown: all of it
    markAllBandsDirty();
    return;
  }
  const double bandH = kRowsPerBand * m_boxH;
  const int ly       = canvasLy();  // rasterRect e' in righe della TELA
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

QString ZtoryThumbnailCanvas::workDirStr() const {
  if (m_officialDir.isEmpty()) return QString();
  return m_officialDir + "/" + kWorkingSubdir;
}

int ZtoryThumbnailCanvas::collectDirtyBands(QVector<ThumbBand> &bands) {
  const int n = bandCount();
  if ((int)m_bandDirty.size() != n) m_bandDirty.resize(n, true);

  const int lx = (int)gridW();
  const int ly = canvasLy();

  // La finestra e' dove si e' disegnato: si riversa nel magazzino PRIMA di
  // scrivere, o si salverebbero pagine vecchie. Il riversamento marca anche
  // da se' ogni pagina che trova cambiata.
  flushWindowToPages();

  int dirtyCount = 0;
  for (int b = 0; b < n; b++) {
    if (!m_bandDirty[b]) continue;
    dirtyCount++;
    int y0, y1;
    bandRasterRange(b, ly, y0, y1);
    if (y0 > y1 || y1 < 0 || y0 >= ly) continue;
    // Dal magazzino: dopo il riversamento ogni pagina della finestra c'e'.
    // Una pagina che non c'e' non e' mai stata scritta — carta bianca.
    TRaster32P band = (b < (int)m_pages.size() && m_pages[b])
                          ? m_pages[b]
                          : TRaster32P();
    if (!band) {
      band = TRaster32P(lx, y1 - y0 + 1);
      band->fill(kPaper);
    }
    bands.push_back({b, rasterToQImage(band, /*premultiplied=*/false)});
  }
  m_bandDirty.assign(n, false);
  return dirtyCount;
}

void ZtoryThumbnailCanvas::writeDirtyBandsNow() {
  if (!m_ras) return;
  if (m_officialDir.isEmpty()) {
    const TFilePath dir = persistDir();
    if (dir.isEmpty()) return;
    m_officialDir = QString::fromStdWString(dir.getWideString());
  }
  QVector<ThumbBand> bands;
  collectDirtyBands(bands);
  if (bands.isEmpty()) return;
  const QString work = workDirStr();
  writeThumbBands(work, m_cols, m_rows, m_boxH, bandCount(), bands, m_merges,
                  /*complete=*/false);
  s_sessionWorkDirs.insert(work);
}

void ZtoryThumbnailCanvas::persistSave() {
  if (!m_ras) return;
  if (m_selfTesting) return;  // l'autocollaudo non tocca il disco
  if (m_officialDir.isEmpty()) {
    const TFilePath dir = persistDir();
    if (dir.isEmpty()) return;
    m_officialDir = QString::fromStdWString(dir.getWideString());
  }

  // Never run two encodes at once: they would queue up behind the pen and the
  // last one to land would not necessarily be the newest canvas.  Remember
  // instead that there is something newer to write — the dirty flags of the
  // bands are NOT cleared here, so nothing is forgotten in the meantime.
  if (m_saveRunning) {
    m_saveQueued = true;
    return;
  }

  QVector<ThumbBand> bands;
  collectDirtyBands(bands);
  if (bands.isEmpty()) return;  // nothing changed since the last write

  // Into the WORKING copy, never the official files: only a scene save makes
  // these pages official (onSceneSaved). complete=false because the old
  // single-image canvas lives in the official folder, and retiring it is the
  // promotion's job.
  const QString work = workDirStr();
  s_sessionWorkDirs.insert(work);
  m_saveRunning = true;
  m_saveQueued  = false;
  m_persistKey  = sceneKey();  // this scene's canvas is (about to be) on disk
  m_savePool->start(new ThumbBandSaveTask(std::move(bands), work, m_cols,
                                          m_rows, m_boxH, bandCount(),
                                          m_merges, /*complete=*/false, this));
}

// The scene reached the disk: what the canvas has becomes official too.
void ZtoryThumbnailCanvas::onSceneSaved() {
  if (!m_ras) return;
  const TFilePath dir = persistDir();
  if (dir.isEmpty()) return;
  const QString newOff = QString::fromStdWString(dir.getWideString());
  if (m_officialDir.isEmpty()) m_officialDir = newOff;

  // 1. Everything drawn so far into the working copy first — including an edit
  //    still waiting on the 700 ms debounce, or the scene would be saved with
  //    the stroke drawn just before ⌘S missing from its thumbnails.
  //    Only if something IS pending: after a load every page is marked dirty,
  //    and flushing unconditionally would re-encode the whole canvas on every
  //    save of a scene whose thumbnails nobody touched.
  const bool pending = (m_saveTimer && m_saveTimer->isActive()) ||
                       m_saveQueued || m_saveRunning;
  if (m_saveTimer) m_saveTimer->stop();
  if (m_savePool) m_savePool->waitForDone();
  m_saveRunning = false;  // the queued finish slot may still arrive: harmless
  m_saveQueued  = false;
  if (pending) writeDirtyBandsNow();

  // 2. Save As: the thumbnails follow the scene to its new folder.
  const QString work = workDirStr();
  if (newOff != m_officialDir) copyThumbFiles(m_officialDir, newOff);

  // 3. The working copy becomes official.
  if (QDir(work).exists()) promoteThumbWorkingCopy(work, newOff);
  if (!QDir(work).exists()) s_sessionWorkDirs.remove(work);
  m_officialDir = newOff;
  m_persistKey  = sceneKey();
}

// The scene is being left without saving (the "save changes?" question has
// been answered by now), or the application is quitting after it: what the
// working copy holds is exactly what the user chose not to keep.
void ZtoryThumbnailCanvas::discardWorkingCopy() {
  if (m_saveTimer) m_saveTimer->stop();
  if (m_savePool) m_savePool->waitForDone();  // or it would recreate the folder
  m_saveRunning = false;
  m_saveQueued  = false;
  const QString work = workDirStr();
  if (!work.isEmpty()) {
    QDir(work).removeRecursively();
    s_sessionWorkDirs.remove(work);
  }
  // The next persistLoad must read the disk again even for the SAME path:
  // Revert Scene reloads it, and the canvas still holds what was discarded.
  m_persistKey.clear();
  m_officialDir.clear();
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
  // The page store belongs to the scene too. Opening a scene WITHOUT saved
  // thumbnails left the previous scene's pages in it, and flushWindowToPages()
  // only refreshes dirty or missing pages: an undo snapshot could then carry
  // them into this scene — the same family as the undo stack above.
  m_pages.clear();
  // La finestra riparte dalla prima pagina (0 = tutte, finche' e' piena).
  m_winFirstPage = 0;
  m_winPageCount = m_windowPages > 0 ? m_windowPages : 0;

  {
    const TFilePath od = persistDir();
    m_officialDir =
        od.isEmpty() ? QString() : QString::fromStdWString(od.getWideString());
  }

  // The official files, exactly as before the working copy existed.
  auto loadOfficial = [&]() {
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
      // Le pagine arrivano da disco gia' separate: si caricano nel MAGAZZINO, e
      // la finestra si costruisce da li'. Prima si montavano dritte in una tela
      // unica e le pagine si buttavano via.
      m_pages.assign(bandCount(), TRaster32P());
      for (int b = 0; b < gBands && b < (int)m_pages.size(); b++) {
        QImage img(dirStr + QString("/_ztorythumbs_band%1.png")
                                .arg(b, 3, 10, QChar('0')));
        if (img.isNull()) continue;  // a missing band leaves blank paper, not a hole
        m_pages[b] = rasterFromQImage(img, /*premultiply=*/false);
      }
      rebuildWindowFromPages();
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
      m_ras  = TRaster32P((int)gridW(), canvasLy());
      m_ras->fill(kPaper);
      if (m_winPageCount > 0) rebuildWindowFromPages();  // pagine vuote
      markAllBandsDirty();  // nothing of this scene is on disk yet
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
    // Finestra ristretta: la tela del vecchio formato si taglia in pagine.
    if (m_winPageCount > 0) {
      m_pages = pagesFromCanvas(r);
      rebuildWindowFromPages();
    }
    // Read from the old single-image format: every band still has to be written
    // before that file can be dropped (writeThumbBands only removes it on a
    // complete save).
    markAllBandsDirty();
    clearSelection();
    updateScrollBars();
    update();
  };
  loadOfficial();

  // Then the working copy on top, if there is one. Ours (a panel rebuilt
  // mid-session) is applied silently; one left by a session that did not
  // close normally is applied too — so the reflow and the display act on the
  // full content — and then offered to the user to keep or throw away.
  const QString work = workDirStr();
  if (!work.isEmpty() && overlayWorkingCopy() &&
      !s_sessionWorkDirs.contains(work))
    askAboutRecoveredWork(work);

  // Un foglio NUOVO si mostra dall'alto, adattato alla larghezza. La vista
  // restava quella della scena di prima: scorso in fondo a un foglio lungo e
  // aperto uno corto, la vista puntava sotto la fine del foglio e la room
  // sembrava vuota — "è sparita la pagina con i thumbs" (Franco, 2026-09-24;
  // i file erano intatti, ⌘⌥0 la faceva ricomparire).
  // revealRow() adatta lo zoom alla LARGHEZZA della tela: se la room non e'
  // a schermo (scena aperta da un'altra room, o all'avvio prima del layout)
  // quella larghezza non e' vera, e si rimanda alla prima volta che si mostra.
  if (isVisible() && width() > 0) {
    m_revealPending = false;
    revealRow(0);
  } else {
    m_revealPending = true;
  }
}

bool ZtoryThumbnailCanvas::overlayWorkingCopy() {
  const QString work = workDirStr();
  int wCols, wRows, wBands;
  double wBoxH;
  if (work.isEmpty() || !readThumbManifest(work, wCols, wRows, wBoxH, wBands))
    return false;
  if (!m_ras) return false;

  // The official pages as just loaded — from the bands, or cut out of the old
  // single image. Every page is dirty after a load, so this takes them all.
  flushWindowToPages();
  const std::vector<TRaster32P> official = m_pages;
  // A page the working copy lacks is taken from the official ones only if the
  // two agree on the page geometry. They always do when a page is missing:
  // every change of geometry (rows, merges, camera reflow, undo of those)
  // marks the WHOLE canvas dirty, so it writes every page.
  const bool sameGeometry =
      (wCols == m_cols && std::abs(wBoxH - m_boxH) < 1e-6);

  m_cols      = wCols;
  m_rows      = wRows;
  m_boxH      = wBoxH;
  m_boxAspect = m_boxW / wBoxH;
  m_merges.clear();
  loadMerges(work);
  m_pages.assign(bandCount(), TRaster32P());
  for (int b = 0; b < (int)m_pages.size(); b++) {
    QImage img(work + QString("/_ztorythumbs_band%1.png")
                          .arg(b, 3, 10, QChar('0')));
    if (!img.isNull())
      m_pages[b] = rasterFromQImage(img, /*premultiply=*/false);
    else if (sameGeometry && b < (int)official.size())
      m_pages[b] = official[b];
  }
  rebuildWindowFromPages();
  markAllBandsDirty();
  clearSelection();
  updateScrollBars();
  update();
  return true;
}

void ZtoryThumbnailCanvas::askAboutRecoveredWork(const QString &work) {
  // Once per working copy, however many Thumbnail room panels are open.
  static QSet<QString> asked;
  if (asked.contains(work)) return;
  asked.insert(work);
  // Not from inside sceneSwitched: the scene is still being loaded.
  QPointer<ZtoryThumbnailCanvas> self(this);
  const QString key = m_persistKey;
  QTimer::singleShot(0, this, [self, work, key]() {
    if (!self || self->m_persistKey != key || !QDir(work).exists()) return;
    const QDateTime when =
        QFileInfo(work + "/_ztorythumbs_grid.txt").lastModified();
    QMessageBox box(self->window());
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Thumbnail room"));
    box.setText(tr("This scene has Thumbnail room drawings that were never "
                   "saved, left by a session that did not close normally "
                   "(last change: %1).")
                    .arg(QLocale().toString(when, QLocale::ShortFormat)));
    box.setInformativeText(
        tr("Keep them? They stay unsaved until you save the scene."));
    QPushButton *keep =
        box.addButton(tr("Keep Them"), QMessageBox::AcceptRole);
    box.addButton(tr("Discard Them"), QMessageBox::DestructiveRole);
    box.setDefaultButton(keep);
    box.exec();
    if (!self) return;
    if (box.clickedButton() == keep) {
      s_sessionWorkDirs.insert(work);
      // Unsaved work again: the scene must ask before it is closed.
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    } else {
      self->discardWorkingCopy();
      self->persistLoad();  // back to the official thumbnails
    }
  });
}

//=============================================================================
// View transform
//=============================================================================

// The view is ONE transform: translate(pan) * rotate(rot) * scale(zoom).
// With m_rot == 0 this maps (x,y) to (x*zoom + pan.x, y*zoom + pan.y) -- i.e.
// exactly what the hand-written arithmetic did before, which is what makes
// this change safe to land before the rotation gesture exists.
// Angolo del puntatore attorno al centro della finestra, in gradi.  Il verso e'
// quello di QTransform::rotate() su un widget con la y in giu', cosi' il foglio
// segue il mouse senza altri segni da indovinare — che e' l'errore che la
// rotazione col pizzico aveva gia' fatto pagare una volta.
static double ztoryAngleAround(const QPointF &p, const QPointF &centre) {
  return std::atan2(p.y() - centre.y(), p.x() - centre.x()) * 180.0 / M_PI;
}

QTransform ZtoryThumbnailCanvas::viewTransform() const {
  QTransform t;
  t.translate(m_pan.x(), m_pan.y());
  t.rotate(m_rot);
  t.scale(m_zoom, m_zoom);
  return t;
}

QTransform ZtoryThumbnailCanvas::viewTransformInv() const {
  return viewTransform().inverted();
}

// Rotation + scale WITHOUT the translation: what an anchor-preserving zoom or
// rotation needs, since widget = pan + R*S*world  =>  pan = widget - R*S*world.
static QPointF ztoryRotScale(const QPointF &w, double rot, double zoom) {
  QTransform t;
  t.rotate(rot);
  t.scale(zoom, zoom);
  return t.map(w);
}

QPointF ZtoryThumbnailCanvas::worldToWidget(const QPointF &w) const {
  return viewTransform().map(w);
}

QPointF ZtoryThumbnailCanvas::widgetToWorld(const QPointF &p) const {
  return viewTransformInv().map(p);
}

TPointD ZtoryThumbnailCanvas::widgetToRaster(const QPointF &widgetPos) const {
  const QPointF w = widgetToWorld(widgetPos);
  // Flip Y to the bottom-up raster origin, then into the WINDOW: the brush
  // draws on m_ras, which starts winY0() rows above the bottom of the sheet.
  return TPointD(w.x(), gridH() - w.y() - winY0());
}

void ZtoryThumbnailCanvas::zoomAt(const QPointF &widgetAnchor, double factor) {
  const QPointF worldAnchor = widgetToWorld(widgetAnchor);
  m_zoom = qBound(0.1, m_zoom * factor, 8.0);
  m_pan  = widgetAnchor - ztoryRotScale(worldAnchor, m_rot, m_zoom);
  update();
}

// Same anchor-preserving shape as zoomAt: the world point under the fingers
// stays under the fingers while the sheet turns around it.
void ZtoryThumbnailCanvas::rotateAt(const QPointF &widgetAnchor, double degrees) {
  if (degrees == 0.0) return;
  const QPointF worldAnchor = widgetToWorld(widgetAnchor);
  m_rot = std::fmod(m_rot + degrees, 360.0);
  m_pan = widgetAnchor - ztoryRotScale(worldAnchor, m_rot, m_zoom);
  updateScrollBars();
  update();
}

void ZtoryThumbnailCanvas::resetRotation() {
  if (m_rot == 0.0) return;
  rotateAt(QPointF(width() * 0.5, height() * 0.5), -m_rot);
}

QRectF ZtoryThumbnailCanvas::pageBoxNoPan() const {
  QTransform t;
  t.rotate(m_rot);
  t.scale(m_zoom, m_zoom);
  return t.mapRect(QRectF(0.0, 0.0, gridW(), gridH()));
}

void ZtoryThumbnailCanvas::updateScrollBars() {
  if (!m_hbar || !m_vbar) return;
  const QRectF box      = pageBoxNoPan();
  const double contentW = box.width(), contentH = box.height();
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
    m_hbar->setValue(
        qBound(0, (int)(-(m_pan.x() + box.left()) + 0.5), m_hbar->maximum()));
  }
  if (needV) {
    m_vbar->setGeometry(width() - thick, 0, thick, viewH);
    m_vbar->setRange(0, (int)std::ceil(contentH - viewH));
    m_vbar->setPageStep(viewH);
    m_vbar->setValue(
        qBound(0, (int)(-(m_pan.y() + box.top()) + 0.5), m_vbar->maximum()));
  }
  m_syncingBars = false;
}

void ZtoryThumbnailCanvas::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  updateScrollBars();
}

void ZtoryThumbnailCanvas::showEvent(QShowEvent *e) {
  QWidget::showEvent(e);
  // La room torna a schermo: ci si riprende il comando, che MainWindow puo'
  // averci tolto all'avvio.
  ztoryInstallUndoRouter(this);
  syncAppUndoActions();
  // Scena aperta mentre la room non era a schermo: il foglio si mostra
  // dall'alto adesso, con la larghezza vera (vedi persistLoad).
  if (m_revealPending) {
    m_revealPending = false;
    QTimer::singleShot(0, this, [this]() { revealRow(0); });
  }
}

void ZtoryThumbnailCanvas::enterEvent(QEvent *) {
  m_cursorOnCanvas = true;
  syncAppUndoActions();
  update();
}

void ZtoryThumbnailCanvas::leaveEvent(QEvent *) {
  syncAppUndoActions();
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

  // La finestra deve contenere la pagina sotto la penna e le due vicine
  // PRIMA che il pennello nasca: lo tiene per puntatore, e durante la
  // pennellata la finestra non si sposta. Le vicine danno margine a un
  // tratto che esce dalla pagina («un disegno non attraversa mai due pagine»,
  // Franco 2026-09-15: il margine copre chi ci arriva vicino).
  {
    const int b = bandOfCanvasRow((int)(gridH() - w.y()));
    ensureWindowCovers(b - 1, b + 1);
  }

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
  // ⚠️ TUTTI E QUATTRO gli angoli, non due. Qui ce n'erano due, che bastano
  // finche' la vista e' dritta: un rettangolo del mondo resta un rettangolo
  // dritto sullo schermo e due angoli opposti lo delimitano. Con la ROTAZIONE
  // non e' piu' vero — gli altri due sporgono fuori — e questa funzione decide
  // QUALE ZONA RIDISEGNARE dopo una pennellata: sbagliarla lascia pezzi di
  // tratto non ridisegnati finche' non passa un ridisegno intero.
  // r arriva in righe della FINESTRA (askWrite): si porta nella tela.
  const int off = winY0();
  const double x0 = r.left(), x1 = r.right() + 1;
  const double y0 = gridH() - (r.bottom() + off) - 1,
               y1 = gridH() - (r.top() + off);
  const QPointF c0 = worldToWidget(QPointF(x0, y0));
  const QPointF c1 = worldToWidget(QPointF(x1, y0));
  const QPointF c2 = worldToWidget(QPointF(x1, y1));
  const QPointF c3 = worldToWidget(QPointF(x0, y1));
  const double lx = qMin(qMin(c0.x(), c1.x()), qMin(c2.x(), c3.x()));
  const double rx = qMax(qMax(c0.x(), c1.x()), qMax(c2.x(), c3.x()));
  const double ty = qMin(qMin(c0.y(), c1.y()), qMin(c2.y(), c3.y()));
  const double by = qMax(qMax(c0.y(), c1.y()), qMax(c2.y(), c3.y()));
  // A pixel of margin each way absorbs the rounding and the painter's smoothing,
  // which can tint the pixel just outside the dab.
  return QRectF(QPointF(lx, ty), QPointF(rx, by))
      .toAlignedRect()
      .adjusted(-2, -2, 2, 2);
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
  // (m_strokeBounds e' in righe della finestra: si porta nella tela.)
  schedulePersistSave(m_strokeBounds.isNull()
                          ? m_strokeBounds
                          : m_strokeBounds.translated(0, winY0()));
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
    if (e->modifiers() & Qt::AltModifier) {
      m_mouseRotating = true;
      m_mouseRotAngle =
          ztoryAngleAround(e->pos(), QPointF(width() * 0.5, height() * 0.5));
      setCursor(Qt::ClosedHandCursor);
      return;
    }
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
  if (m_mouseRotating) {
    const QPointF centre(width() * 0.5, height() * 0.5);
    const double a = ztoryAngleAround(e->pos(), centre);
    rotateAt(centre, a - m_mouseRotAngle);
    m_mouseRotAngle = a;
    return;
  }
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
    m_panning       = false;
    m_mouseRotating = false;
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
  // Straighten the sheet: ⌥0 (Option-zero).  Without a way back to square,
  // getting there by hand is a torture -- which is why every drawing program
  // that rotates the view also ships this command.
  // ⌘⌥0 (Ctrl+Alt+0): Reset View, as in the other rooms — straight, fitted
  // to the width, from the top. ⌥0 alone only straightens, keeping zoom and
  // pan (Franco, 2026-09-24): checked first, or ⌥0 would swallow it.
  if ((e->modifiers() & Qt::AltModifier) &&
      (e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_0) {
    revealRow(0);
    return;
  }
  if (e->modifiers() & Qt::AltModifier) {
    const QPointF c(width() * 0.5, height() * 0.5);
    switch (e->key()) {
    case Qt::Key_0:     resetRotation();        return;
    // Rotate by keyboard as well as by pinch.  Not decoration: without it the
    // rotation cannot be exercised at all on a machine with no touch screen,
    // which is every machine we develop on -- and turning the sheet a notch at
    // a time is how a mouse user would want it anyway.
    case Qt::Key_Left:  rotateAt(c, -15.0);     return;
    case Qt::Key_Right: rotateAt(c,  15.0);     return;
    }
  }
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
    // ⚠️ NIENTE underMouse() / hasFocus() QUI. Annulla e Ripeti valgono in
    // TUTTA la Thumbs room, non solo col puntatore sopra la tela: la room e'
    // il contesto, non il widget. Prima funzionavano solo passando sopra il
    // disegno — «se stiamo in quella room dovrebbe funzionare ovunque»
    // (Franco, 2026-09-23) — ed e' la stessa condizione che usa il router del
    // comando, cosi' menu e tastiera non possono divergere.
    const bool undoish = isThumbsContextActive();
    // ⌘Z annulla, ⌘⇧Z ripete — e ANCHE ⌘Y, che e' la scorciatoia di redo
    // dell'applicazione (mainwindow.cpp: MI_Redo = "Ctrl+Y"). Qui si guardava
    // solo Key_Z, quindi ⌘Y non arrivava mai alla tela: finiva al redo
    // dell'applicazione, la cui pila e' un'altra ed e' vuota, e non succedeva
    // niente. Segnalato da Franco il 2026-09-23.
    const bool ctrl  = (ke->modifiers() & Qt::ControlModifier) != 0;
    const bool shift = (ke->modifiers() & Qt::ShiftModifier) != 0;
    const bool isRedoKey =
        ctrl && (ke->key() == Qt::Key_Y || (ke->key() == Qt::Key_Z && shift));
    const bool isUndoKey = ctrl && ke->key() == Qt::Key_Z && !shift;
    const bool wantsUndo = undoish && ((isRedoKey && !m_redo.empty()) ||
                                       (isUndoKey && !m_undo.empty()));
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
        if ((deltaPoint.manhattanLength() > 100) && !m_zooming && !m_rotating) {
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
      m_rotating      = false;
      m_rotationDelta = 0.0;
    } else if (gesture->state() == Qt::GestureFinished) {
      m_gestureActive = false;
      m_zooming       = false;
      m_scaleFactor   = 0.0;
      m_rotating      = false;
      m_rotationDelta = 0.0;
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
      // Rotation of the view.  Ported from SceneViewer
      // (sceneviewerevents.cpp:1394): a 10-degree dead zone before it engages,
      // so a pinch-zoom does not wobble the sheet.
      //
      // ⚠️ IL SEGNO E' POSITIVO, ed e' una CORREZIONE.  Ragionando l'avevo messo
      // negativo — come SceneViewer, che pero' lavora su un sistema di
      // coordinate con la y in su e una TAffine, non su una QTransform con la y
      // in giu': i due meno non sono lo stesso meno.  Provato da Franco sulla
      // Wacom Companion 2 il 2026-09-22: il foglio girava dalla parte sbagliata.
      // Misurato batte dedotto, e qui il dedotto aveva torto.
      if (changeFlags & QPinchGesture::RotationAngleChanged) {
        const qreal rotationDelta =
            gesture->rotationAngle() - gesture->lastRotationAngle();
        if (!m_rotating) {
          m_rotationDelta += rotationDelta;
          if (std::abs(m_rotationDelta) >= 10.0) m_rotating = true;
        }
        if (m_rotating) {
          // Around the centre of the widget, like SceneViewer (which rotates
          // about the centre of the view, not about the fingers).
          rotateAt(QPointF(width() * 0.5, height() * 0.5), rotationDelta);
          m_touchPanning = false;
          m_touchPoints  = 100;  // blocks the undo/redo tap for this touch
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
  const int lx = m_ras->getLx(), ly = canvasLy();  // ly: righe della TELA
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
  // La finestra sopra la zona (righe della tela), poi righe della finestra.
  ensureWindowCoversRows(ry0, ry1 - 1);
  const int off = winY0();

  TRaster32P sub = m_ras->extract(x0, ry0 - off, x1 - 1, ry1 - 1 - off);  // shares m_ras mem
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
    // La FINESTRA, capovolta nel verso del mondo: la sua riga 0 e' la y del
    // mondo windowWorldTop(), quindi il pittore si sposta di tanto.
    const QRectF bbw = poly.boundingRect();
    ensureWindowCoversRows(canvasLy() - (int)std::ceil(bbw.bottom()),
                           canvasLy() - (int)std::floor(bbw.top()));
    const int worldTop = canvasLy() - winY0() - m_ras->getLy();
    QImage canvasImg = rasterToQImage(m_ras, true, true);  // world orientation
    {
      QPainter cp(&canvasImg);
      cp.setRenderHint(QPainter::Antialiasing, true);
      cp.setPen(Qt::NoPen);
      cp.setBrush(Qt::white);
      cp.translate(0, -worldTop);
      cp.drawPolygon(poly);  // world coords (shifted into the window)
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
  // The painter already carries the view transform (paintEvent sets it), so
  // everything here is in WORLD coordinates: COMBINE, never replace -- a plain
  // setTransform() would drop pan, zoom and rotation and draw the float in the
  // widget's top-left corner.
  p.save();
  p.setTransform(floatLocalToWorld(), /*combine=*/true);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.drawImage(0, 0, m_floatImg);
  p.restore();

  // Outline + handles, in world coordinates: they turn with the sheet, and the
  // 1/zoom factor keeps their on-screen size what it was before.
  const double inv = m_zoom > 1e-6 ? 1.0 / m_zoom : 1.0;
  QPolygonF poly;
  for (int c = 0; c < 4; ++c) poly << floatHandleWorld(c);
  QPen pen(QColor(0, 170, 255));
  pen.setCosmetic(true);
  pen.setWidth(2);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(poly);

  // Rotation handle: a stalk + circle.
  const QPointF topMid = (poly[0] + poly[1]) / 2.0;
  const QPointF rot    = floatHandleWorld(4);
  p.drawLine(topMid, rot);
  p.setBrush(QColor(0, 170, 255));
  p.drawEllipse(rot, 5 * inv, 5 * inv);

  // Corner (scale) handles.
  for (int c = 0; c < 4; ++c) {
    const QPointF wp = poly[c];
    p.drawRect(QRectF(wp.x() - 4 * inv, wp.y() - 4 * inv, 8 * inv, 8 * inv));
  }
  p.setBrush(Qt::NoBrush);
}

void ZtoryThumbnailCanvas::commitFloat() {
  if (!hasFloat() || !m_ras) return;
  // La finestra sopra dove la selezione ATTERRA (puo' essere stata spostata
  // lontano da dove e' stata presa).
  {
    const QRectF dst = floatLocalToWorld().mapRect(
        QRectF(0, 0, m_floatImg.width(), m_floatImg.height()));
    ensureWindowCoversRows(canvasLy() - (int)std::ceil(dst.bottom()),
                           canvasLy() - (int)std::floor(dst.top()));
  }
  const int worldTop = canvasLy() - winY0() - m_ras->getLy();
  QImage canvasImg = rasterToQImage(m_ras, /*premul=*/true, /*mirror=*/true);
  {
    QPainter p(&canvasImg);  // canvasImg px == world coords shifted by worldTop
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.translate(0, -worldTop);
    p.setTransform(floatLocalToWorld(), /*combine=*/true);
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
    ensureWindowCoversRows(canvasLy() - m_floatSrcRect.bottom() - 1,
                           canvasLy() - m_floatSrcRect.top());
    const int worldTop = canvasLy() - winY0() - m_ras->getLy();
    QImage canvasImg = rasterToQImage(m_ras, true, true);
    {
      QPainter p(&canvasImg);
      p.translate(0, -worldTop);
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

  // Le pagine sono CONDIVISE fra le fotografie: sommarle una per fotografia
  // conterebbe piu' volte la stessa memoria e troncherebbe la cronologia molto
  // prima del necessario — cioe' si pagherebbe il prezzo della copia senza
  // averla fatta. Si contano una volta sola, per puntatore.
  std::set<const TRaster *> visti;
  auto bytesOf = [&visti](const Snapshot &s) -> size_t {
    size_t n = 0;
    if (s.ras)
      n += (size_t)s.ras->getLx() * (size_t)s.ras->getLy() * 4u;
    for (const TRaster32P &p : s.pages) {
      if (!p) continue;
      if (!visti.insert(p.getPointer()).second) continue;  // gia' contata
      n += (size_t)p->getLx() * (size_t)p->getLy() * 4u;
    }
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
  // Qui c'era `s.ras = m_ras->clone()`: una copia dell'INTERA tela per ogni
  // operazione che non fosse una pennellata (incolla, trasforma, importa,
  // pulisci, aggiungi riga). 51 MB a 4x26 e 617 MB all'obiettivo di
  // produzione — ed e' il motivo per cui l'annullamento era "faticoso".
  // Adesso si riversa e si tengono i RIFERIMENTI alle pagine: quelle non
  // toccate sono condivise con la cronologia, e costano zero.
  flushWindowToPages();
  s.pages = m_pages;
  m_undo.push_back(s);
  trimHistory();
  syncAppUndoActions();
}

// Copy the tiles listed in \a like out of the current raster (used to build the
// opposite-direction snapshot when undoing/redoing a stroke).
std::vector<ZtoryThumbnailCanvas::Patch>
ZtoryThumbnailCanvas::capturePatchesAt(const std::vector<Patch> &like) const {
  std::vector<Patch> out;
  if (!m_ras) return out;
  out.reserve(like.size());
  // p.pos e' in righe della TELA; il chiamante ha gia' portato la finestra
  // sopra queste tessere (undo/redo).
  const int off = winY0();
  for (const Patch &p : like) {
    const int lx = p.before->getLx(), ly = p.before->getLy();
    const int wy = p.pos.y - off;
    // Geometry changed under us (undo across a resize): skip rather than
    // read out of bounds.  The resize's own full snapshot restores the pixels.
    if (p.pos.x < 0 || wy < 0 || p.pos.x + lx > m_ras->getLx() ||
        wy + ly > m_ras->getLy())
      continue;
    TRaster32P cur(lx, ly);
    cur->copy(m_ras->extract(p.pos.x, wy, p.pos.x + lx - 1, wy + ly - 1));
    out.push_back({p.pos, cur});
  }
  return out;
}

void ZtoryThumbnailCanvas::applyPatches(const std::vector<Patch> &patches) {
  if (!m_ras) return;
  const int off = winY0();  // p.pos in righe della TELA
  for (const Patch &p : patches) {
    const int wy = p.pos.y - off;
    if (p.pos.x < 0 || wy < 0 ||
        p.pos.x + p.before->getLx() > m_ras->getLx() ||
        wy + p.before->getLy() > m_ras->getLy())
      continue;
    m_ras->copy(p.before, TPoint(p.pos.x, wy));
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
  // Le tessere sono state prese nella FINESTRA; la fotografia le tiene in
  // righe della TELA, perche' fra la pennellata e il suo annullamento la
  // finestra puo' essersi spostata.
  const int off = winY0();
  for (auto &kv : m_strokeTiles)
    s.patches.push_back(
        {TPoint(kv.first.first * kUndoTile, kv.first.second * kUndoTile + off),
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
  // ⚠️ LA GEOMETRIA PRIMA DEI PIXEL, e non e' un dettaglio di stile:
  // rebuildWindowFromPages() dimensiona la finestra con gridH(), che dipende da
  // m_rows E da m_boxH. Ripristinando l'altezza della casella DOPO, annullando
  // attraverso un cambio di formato camera la finestra nascerebbe con
  // l'altezza vecchia. Il codice di prima non se ne accorgeva perche' clonava
  // un raster gia' dimensionato.
  m_cols   = s.cols;
  m_rows   = s.rows;
  m_merges = s.merges;
  // Restore the grid geometry the raster was laid out at, so it isn't stretched
  // to whatever aspect the camera happens to be now.
  if (s.boxAspect > 0.0) {
    m_boxAspect = s.boxAspect;
    m_boxH      = m_boxW / s.boxAspect;
  }
  // Dalle PAGINE: le si adotta e si ricostruisce la finestra. Non serve
  // clonare — le pagine non si modificano mai in luogo, si sostituiscono.
  if (!s.pages.empty()) {
    m_pages = s.pages;
    rebuildWindowFromPages();
  } else if (s.ras) {
    // (Nessuna fotografia la riempie piu': e' il formato di prima delle
    // pagine.) Una tela intera: si taglia in pagine e si ricostruisce la
    // finestra, invece di metterla nella finestra cosi' com'e'.
    m_pages = pagesFromCanvas(s.ras);
    rebuildWindowFromPages();
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
  const double newBoxH =
      s.boxAspect > 0.0 ? m_boxW / s.boxAspect : m_boxH;
  if (s.cols == m_cols && std::abs(newBoxH - m_boxH) < 1e-9) {
    // Solo il numero di righe (il caso di addRow): sulle pagine.
    if (s.boxAspect > 0.0) m_boxAspect = s.boxAspect;
    resizeRowsPagewise(s.rows);
  } else {
    // Cambia la forma delle pagine: si ricompone la tela con la geometria di
    // PRIMA, si applica la regola di sempre, si ritaglia con quella nuova.
    flushWindowToPages();
    const int oldH = canvasLy();
    const TRaster32P whole = canvasFromPages(m_pages, (int)gridW(), oldH);
    m_cols = s.cols;
    if (s.boxAspect > 0.0) {
      m_boxAspect = s.boxAspect;
      m_boxH      = newBoxH;
    }
    m_rows         = s.rows;
    const int newH = canvasLy();
    TRaster32P nr((int)gridW(), qMax(1, newH));
    nr->fill(kPaper);
    // The raster is bottom-up, so the world bottom is low Y: growing pushes the
    // content up by the difference, shrinking drops that band off the bottom.
    if (whole) {
      if (newH >= oldH)
        nr->copy(whole, TPoint(0, newH - oldH));
      else
        nr->copy(whole->extract(0, oldH - newH, whole->getLx() - 1, oldH - 1),
                 TPoint(0, 0));
    }
    m_pages = pagesFromCanvas(nr);
    markAllBandsDirty();
    rebuildWindowFromPages();
  }
  m_merges = s.merges;
  clearSelection();
  schedulePersistSave();
  updateScrollBars();
  update();
}

// A patch snapshot only swaps the touched tiles: build the opposite entry from
// the current pixels of those same tiles, then paste the stored ones back.
// Riallinea le voci Annulla/Ripeti del menu quando la funzione esce, da
// qualunque ramo: undo() e redo() hanno piu' uscite anticipate, e dimenticarne
// una lascerebbe il menu a raccontare una pila che non c'e' piu'.
struct ZtoryUndoActionSync {
  ZtoryThumbnailCanvas *c;
  explicit ZtoryUndoActionSync(ZtoryThumbnailCanvas *canvas) : c(canvas) {}
  ~ZtoryUndoActionSync() { if (c) c->syncAppUndoActions(); }
};

void ZtoryThumbnailCanvas::undo() {
  ZtoryUndoActionSync sync(this);  // riallinea le voci di menu all'uscita
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
  if (!s.pages.empty() || s.ras) {
    Snapshot cur = makeMetaSnapshot();
    flushWindowToPages();
    cur.pages = m_pages;
    m_redo.push_back(std::move(cur));
    restoreSnapshot(s);
    return;
  }
  // La finestra sopra le tessere, prima di leggerle e riscriverle.
  {
    int lo = INT_MAX, hi = INT_MIN;
    for (const Patch &p : s.patches) {
      lo = std::min(lo, p.pos.y);
      hi = std::max(hi, p.pos.y + p.before->getLy() - 1);
    }
    if (lo <= hi) ensureWindowCoversRows(lo, hi);
  }
  Snapshot cur = makeMetaSnapshot();
  cur.patches  = capturePatchesAt(s.patches);
  m_redo.push_back(std::move(cur));
  applyPatches(s.patches);
  schedulePersistSave();
  update();
}

void ZtoryThumbnailCanvas::redo() {
  ZtoryUndoActionSync sync(this);  // riallinea le voci di menu all'uscita
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
  if (!s.pages.empty() || s.ras) {
    Snapshot cur = makeMetaSnapshot();
    flushWindowToPages();
    cur.pages = m_pages;
    m_undo.push_back(std::move(cur));
    restoreSnapshot(s);
    return;
  }
  // La finestra sopra le tessere, prima di leggerle e riscriverle.
  {
    int lo = INT_MAX, hi = INT_MIN;
    for (const Patch &p : s.patches) {
      lo = std::min(lo, p.pos.y);
      hi = std::max(hi, p.pos.y + p.before->getLy() - 1);
    }
    if (lo <= hi) ensureWindowCoversRows(lo, hi);
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

bool ZtoryThumbnailCanvas::isThumbsContextActive() const {
  // ⚠️ isVisible() E !isHidden(): in un'applicazione a ROOM i pannelli delle
  // room non correnti restano figli di un contenitore nascosto, e i due
  // predicati non dicono la stessa cosa — trappola gia' pagata una volta con
  // "Send to Board". isVisible() da solo puo' essere falso per un pannello che
  // l'utente sta guardando, e isHidden() da solo puo' essere falso per uno che
  // non si vede.
  if (isHidden()) return false;
  if (!isVisible()) return false;
  QWidget *w = window();
  return w && w->isActiveWindow();
}

bool ZtoryThumbnailCanvas::routeUndoHere(bool redoDirection) {
  if (redoDirection) {
    if (m_redo.empty()) return false;
    redo();
  } else {
    if (m_undo.empty()) return false;
    undo();
  }
  return true;
}

void ZtoryThumbnailCanvas::syncAppUndoActions() {
  CommandManager *cm = CommandManager::instance();
  if (!cm) return;
  QAction *au = cm->getAction(MI_Undo);
  QAction *ar = cm->getAction(MI_Redo);
  if (!au || !ar) return;
  TUndoManager *um = TUndoManager::manager();
  if (!um) return;

  // ⚠️ NIENTE underMouse() QUI, ed e' il motivo per cui "Ripeti" non si
  // accendeva MAI: aprendo il menu il puntatore ESCE dalla tela, quindi
  // nell'istante esatto in cui guardi le voci la tela smette di considerarsi
  // in primo piano e le rimette grigie. Il filtro dei tasti puo' usarlo — la
  // tastiera non sposta il puntatore — il menu no.
  // Qui "nostro" vuol dire: la Thumbs room e' quella a schermo.
  const bool mine = isThumbsContextActive();
  if (mine && (!m_undo.empty() || !m_redo.empty())) {
    au->setEnabled(!m_undo.empty());
    ar->setEnabled(!m_redo.empty());
    return;
  }
  // Fuori da quel caso si rimette la verita' dell'applicazione: se restassero
  // accese, un Annulla dal menu in un'altra room non farebbe niente e
  // sembrerebbe rotto.
  au->setEnabled(!um->atBeginning());
  ar->setEnabled(!um->atEnd());
}

bool ZtoryThumbnailCanvas::handleUndoKey(QKeyEvent *e) {
  // ⌘Z annulla; ⌘⇧Z e ⌘Y ripetono. ⌘Y perche' e' la scorciatoia che
  // l'applicazione assegna a MI_Redo (mainwindow.cpp), quindi e' quella che un
  // utente prova per prima — e qui non era gestita.
  // Si consuma il tasto solo quando la pila ha qualcosa, cosi' l'annullamento
  // dell'applicazione continua a funzionare quando la nostra e' vuota.
  if (!(e->modifiers() & Qt::ControlModifier)) return false;
  const bool shift = (e->modifiers() & Qt::ShiftModifier) != 0;
  if (e->key() == Qt::Key_Y || (e->key() == Qt::Key_Z && shift)) {
    if (m_redo.empty()) return false;
    redo();
    return true;
  }
  if (e->key() == Qt::Key_Z && !shift) {
    if (m_undo.empty()) return false;
    undo();
    return true;
  }
  return false;
}

//=============================================================================
// Paint
//=============================================================================

void ZtoryThumbnailCanvas::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(40, 40, 40));
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);

  if (!m_ras) return;

  // Everything below is drawn in WORLD coordinates with the view transform
  // set on the painter.  Under rotation a world rectangle is no longer an
  // upright screen rectangle, so the old "worldToWidget(topLeft) + size*zoom"
  // shape was wrong by construction: this REMOVES arithmetic rather than
  // adding trigonometry.
  const QRectF page(0.0, 0.0, gridW(), gridH());

  p.save();
  p.setTransform(viewTransform(), /*combine=*/true);

  // rasterToQImage() wraps the raster memory without copying, but mirrored=true
  // deep-copies the whole surface (~31 MB on a 4x15 grid) on EVERY repaint —
  // i.e. on every mouse move while drawing.  Take the zero-copy view and let
  // the painter apply the vertical flip: Qt then only touches the pixels inside
  // the clip region.
  // The page.  The surface itself is transparent (see kPaper), so the white a
  // storyboard artist draws on is painted HERE, under the drawing — that is
  // what lets the eraser take pixels away and still look like paper.
  p.fillRect(page, Qt::white);

  // ── Il foglio: la FINESTRA, piu' le pagine che la finestra non copre ────
  // Dentro questo blocco la y e' ribaltata, quindi le righe del raster si
  // usano cosi' come sono. Finche' la finestra copre tutto, il ciclo sulle
  // pagine non disegna niente e resta il solo blit di prima — identico.
  p.save();
  p.translate(0.0, gridH());
  p.scale(1.0, -1.0);
  {
    const int lx = m_ras->getLx(), ly = m_ras->getLy();
    const int first = m_winFirstPage;
    const int count = m_winPageCount > 0 ? m_winPageCount : bandCount();
    // La finestra, in un colpo solo: e' contigua, ed e' dove si sta disegnando.
    int wy0 = 0, wy1 = ly - 1;
    if (m_winPageCount > 0) {
      int a0, a1, b0, b1;
      bandRasterRange(first, canvasLy(), a0, a1);
      bandRasterRange(first + count - 1, canvasLy(), b0, b1);
      wy0 = qMin(a0, b0);
      wy1 = qMax(a1, b1);
    }
    QImage wimg = rasterToQImage(m_ras, /*premultiplied=*/true, /*mirrored=*/false);
    // A finestra piena si usa il rettangolo di PRIMA, non [0, ly]: gridH() puo'
    // non essere intero (altezza casella 205,6 su una camera CinemaScope) e il
    // raster e' troncato, quindi i due rettangoli differiscono di mezzo pixel
    // di scala. Invisibile, ma non sarebbe piu' "identico" — ed e' proprio il
    // tipo di deriva che questo lavoro deve poter escludere.
    if (m_winPageCount > 0)
      p.drawImage(QRectF(0, wy0, lx, wy1 - wy0 + 1), wimg);
    else
      p.drawImage(page, wimg);

    // Le pagine fuori dalla finestra: una per una, a casa loro. Qt ritaglia
    // quelle fuori schermo, quindi disegnarle tutte non costa quanto sembra.
    for (int b = 0; b < (int)m_pages.size(); b++) {
      if (m_winPageCount > 0 && b >= first && b < first + count) continue;
      if (m_winPageCount <= 0) break;  // la finestra copre tutto
      if (!m_pages[b]) continue;
      int y0, y1;
      bandRasterRange(b, canvasLy(), y0, y1);
      if (y0 > y1) continue;
      QImage pimg =
          rasterToQImage(m_pages[b], /*premultiplied=*/true, /*mirrored=*/false);
      p.drawImage(QRectF(0, y1 - m_pages[b]->getLy() + 1, lx,
                         m_pages[b]->getLy()),
                  pimg);
    }
  }
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
      const double x = c * m_boxW;
      p.drawLine(QPointF(x, r * m_boxH), QPointF(x, (r + 1) * m_boxH));
    }
  for (int r = 1; r < m_rows; ++r)
    for (int c = 0; c < m_cols; ++c) {
      if (mergeIndexAt(c, r - 1) >= 0 &&
          mergeIndexAt(c, r - 1) == mergeIndexAt(c, r))
        continue;  // interior horizontal edge of a merge
      const double y = r * m_boxH;
      p.drawLine(QPointF(c * m_boxW, y), QPointF((c + 1) * m_boxW, y));
    }

  QPen border(QColor(29, 92, 131, 120));
  border.setCosmetic(true);
  p.setPen(border);
  p.drawRect(page);

  // Outline each merged (panorama) region a little brighter.
  QPen mergePen(QColor(90, 150, 220));
  mergePen.setCosmetic(true);
  mergePen.setWidth(2);
  p.setPen(mergePen);
  p.setBrush(Qt::NoBrush);
  for (const QRect &m : m_merges) {
    const QRectF wr(m.x() * m_boxW, m.y() * m_boxH, m.width() * m_boxW,
                    m.height() * m_boxH);
    p.drawRect(wr);
  }

  // Selection overlay: tint selected panels + a numbered badge showing the
  // export order. Always drawn (so the user keeps the order visible after
  // switching back to a brush), but only editable in Select mode.
  for (int i = 0; i < m_selection.size(); ++i) {
    const QRectF wr = panelWorldRect(m_selection[i]);
    if (wr.isNull()) continue;
    p.fillRect(wr, QColor(224, 90, 0, 60));
    QPen selPen(QColor(224, 90, 0));
    selPen.setCosmetic(true);
    selPen.setWidth(2);
    p.setPen(selPen);
    p.drawRect(wr);

    // Order badge (1-based) in the top-left corner of the panel.  Drawn in
    // world space so it TURNS with the sheet -- the numbers belong to the
    // page, not to the screen (Franco, 2026-09-18) -- while the 1/zoom factor
    // keeps its on-screen size the 20 px it has always been.
    const double inv = m_zoom > 1e-6 ? 1.0 / m_zoom : 1.0;
    const double bs  = 20.0 * inv;
    QRectF badge(wr.left() + 3 * inv, wr.top() + 3 * inv, bs, bs);
    p.setBrush(QColor(224, 90, 0));
    p.setPen(Qt::NoPen);
    p.drawEllipse(badge);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSizeF(qMax(0.5, 10.0 * inv));
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
        for (const QPointF &wp : m_lassoPath) wpoly << wp;
        p.setBrush(QColor(0, 170, 255, 30));
        p.drawPolygon(wpoly);
      } else {
        const QRectF wr = QRectF(m_marqueeStart, m_marqueeCur).normalized();
        p.setBrush(QColor(0, 170, 255, 30));
        p.drawRect(wr);
      }
      p.setBrush(Qt::NoBrush);
    }
    paintFloat(p);
  }

  p.restore();  // end of the world-coordinate block

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

//=============================================================================
// Autocollaudo della finestra (raster per pagina, passo 3)
//=============================================================================
//
// La finestra stretta NON deve cambiare niente di quello che l'utente vede o
// salva: deve solo usare meno memoria. Questo lo verifica invece di affermarlo.
// La stessa sequenza di operazioni VERE (import, righe, pennellate, selezione
// spostata, lazo, annulla/ripeti) gira a finestra piena e a finestra stretta,
// e le due tele finali si confrontano byte per byte, insieme alla risposta di
// «pannello vuoto?» per ogni pannello.
//
// Si attiva solo con ZTORYC_THUMBS_SELFTEST=1. Lo stato della tela viene
// salvato prima e rimesso dopo; i salvataggi su disco sono bloccati mentre
// gira (m_selfTesting).

namespace {
QImage ztorySelfTestImage(int seed) {
  QImage img(240, 135, QImage::Format_RGB888);
  for (int y = 0; y < img.height(); y++) {
    uchar *row = img.scanLine(y);
    for (int x = 0; x < img.width(); x++) {
      row[3 * x + 0] = (uchar)((x * 7 + seed * 40) & 0xff);
      row[3 * x + 1] = (uchar)((y * 11 + seed * 90) & 0xff);
      row[3 * x + 2] = (uchar)(((x + y) * 3 + seed * 17) & 0xff);
    }
  }
  return img;
}
}  // namespace

void ZtoryThumbnailCanvas::runPagingSelfTest() {
  QFile logf(QDir::homePath() + "/Desktop/ztory_paging_selftest.log");
  if (!logf.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream log(&logf);
  if (!m_ras) {
    log << "nessuna tela: prova saltata\n";
    return;
  }

  // ── Lo stato di adesso, da rimettere alla fine ──
  struct Saved {
    TRaster32P ras;
    std::vector<TRaster32P> pages;
    int rows, cols, winFirst, winCount, windowPages;
    double boxH, boxAspect, zoom, rot;
    QPointF pan;
    QVector<QRect> merges;
    std::vector<bool> dirty;
    std::vector<Snapshot> undo, redo;
    QImage floatImg;
    QPointF floatCenter;
    double floatScale, floatAngle;
    QRect floatSrc;
    bool floatWasMove, selectMode, xformMode, sceneDirty, timerActive;
    QVector<int> selection;
  } sv;
  sv.ras = m_ras->clone();  // il pennello e l'import scrivono DENTRO m_ras
  sv.pages = m_pages;  // le pagine non si modificano mai sul posto
  sv.rows = m_rows; sv.cols = m_cols;
  sv.winFirst = m_winFirstPage; sv.winCount = m_winPageCount;
  sv.windowPages = m_windowPages;
  sv.boxH = m_boxH; sv.boxAspect = m_boxAspect;
  sv.zoom = m_zoom; sv.rot = m_rot; sv.pan = m_pan;
  sv.merges = m_merges; sv.dirty = m_bandDirty;
  sv.undo = m_undo; sv.redo = m_redo;
  sv.floatImg = m_floatImg; sv.floatCenter = m_floatCenter;
  sv.floatScale = m_floatScale; sv.floatAngle = m_floatAngle;
  sv.floatSrc = m_floatSrcRect; sv.floatWasMove = m_floatWasMove;
  sv.selectMode = m_selectMode; sv.xformMode = m_xformMode;
  sv.selection = m_selection;
  sv.sceneDirty  = TApp::instance()->getCurrentScene()->getDirtyFlag();
  sv.timerActive = m_saveTimer && m_saveTimer->isActive();
  m_selfTesting = true;

  int moves = 0;
  struct Result {
    TRaster32P canvas;
    std::vector<bool> empty;
    int moves = 0;
    std::vector<QRect> strokeBoxes;  // pixel cambiati da ogni pennellata
    bool strokeUndoExact = true;     // annulla = tela di prima, byte per byte
  };
  auto wholeCanvas = [this]() {
    flushWindowToPages();
    return canvasFromPages(m_pages, (int)gridW(), canvasLy());
  };
  // Rettangolo (righe della TELA) dei pixel che differiscono fra a e b.
  auto diffBox = [](const TRaster32P &a, const TRaster32P &b) -> QRect {
    if (!a || !b || a->getLx() != b->getLx() || a->getLy() != b->getLy())
      return QRect(-1, -1, 0, 0);
    int x0 = INT_MAX, y0 = INT_MAX, x1 = -1, y1 = -1;
    for (int y = 0; y < a->getLy(); y++) {
      const TPixel32 *pa = a->pixels(y), *pb = b->pixels(y);
      for (int x = 0; x < a->getLx(); x++)
        if (pa[x] != pb[x]) {
          x0 = std::min(x0, x); x1 = std::max(x1, x);
          y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
    }
    return x1 < 0 ? QRect() : QRect(QPoint(x0, y0), QPoint(x1, y1));
  };
  auto sameCanvas = [](const TRaster32P &a, const TRaster32P &b) {
    if (!a || !b || a->getLx() != b->getLx() || a->getLy() != b->getLy())
      return false;
    for (int y = 0; y < a->getLy(); y++)
      if (std::memcmp(a->pixels(y), b->pixels(y),
                      sizeof(TPixel32) * a->getLx()) != 0)
        return false;
    return true;
  };

  auto runOnce = [&](int windowPages, bool withStrokes) -> Result {
    // Tela di prova: 40 righe = 8 pagine, bianca, vista nota.
    m_windowPages  = windowPages;
    m_rows         = 40;
    m_merges.clear();
    m_undo.clear();
    m_redo.clear();
    m_strokeTiles.clear();
    m_floatImg     = QImage();
    m_floatDrag    = -1;
    m_selectMode   = false;
    m_xformMode    = false;
    m_selection.clear();
    m_zoom = 0.5; m_rot = 0.0; m_pan = QPointF(0, 0);
    m_pages.assign(bandCount(), TRaster32P());
    m_bandDirty.assign(bandCount(), true);
    m_winFirstPage = 0;
    m_winPageCount = windowPages > 0 ? windowPages : 0;
    m_ras = TRaster32P((int)gridW(), canvasLy());
    m_ras->fill(kPaper);
    if (m_winPageCount > 0) rebuildWindowFromPages();
    const int movesBefore = m_windowMoves;

    // 1. Pagine importate lontane fra loro.
    std::vector<ImportedBlit> cells;
    const int rowsAt[4] = {0, 17, 33, 39};
    for (int k = 0; k < 4; k++) {
      ImportedBlit b;
      b.row   = rowsAt[k];
      b.col   = k % m_cols;
      b.image = ztorySelfTestImage(k + 1);
      cells.push_back(b);
    }
    applyImportedCells(cells, 40);
    // 2. Righe in fondo.
    addRow();
    addRow();
    // 3. Pennellate col pennello vero, in due pagine lontane.
    std::vector<QRect> boxes;
    bool undoExact = true;
    if (withStrokes && m_style) {
      for (int row : {5, 36}) {
        const QRectF r = panelWorldRect(row * m_cols + 1);
        const QPointF a(r.left() + r.width() * 0.2, r.center().y());
        const QPointF c(r.left() + r.width() * 0.8, r.center().y() + 10);
        const TRaster32P before = wholeCanvas();
        beginStroke(worldToWidget(a), 1.0);
        for (int i = 1; i <= 20; i++) {
          // Una pausa vera fra i punti: MyPaint ricava la velocita' dal tempo,
          // e con tutti i punti nello stesso istante disegnava un punto solo.
          QThread::msleep(8);
          const double t = i / 20.0;
          strokeTo(worldToWidget(a + (c - a) * t), 1.0);
        }
        endStroke();
        const TRaster32P after = wholeCanvas();
        boxes.push_back(diffBox(before, after));
        // L'annulla della pennellata deve ridare ESATTAMENTE la tela di prima:
        // e' la prova delle tessere dell'annullamento in righe della tela.
        undo();
        if (!sameCanvas(before, wholeCanvas())) undoExact = false;
        redo();
        if (!sameCanvas(after, wholeCanvas())) undoExact = false;
      }
    }
    // 4. Selezione sollevata a riga 17 e posata 13 righe piu' in basso.
    liftFloat(panelWorldRect(17 * m_cols + 1), /*copy=*/false);
    if (hasFloat()) {
      m_floatCenter += QPointF(0, 13 * m_boxH);
      commitFloat();
    }
    // 5. Lazo a riga 33, posato a riga 13 una colonna a sinistra.
    {
      const QRectF r = panelWorldRect(33 * m_cols + 2);
      QVector<QPointF> tri;
      tri << QPointF(r.left() + 10, r.top() + 10)
          << QPointF(r.right() - 10, r.top() + 20)
          << QPointF(r.center().x(), r.bottom() - 10);
      liftFloatLasso(tri, /*copy=*/false);
      if (hasFloat()) {
        m_floatCenter += QPointF(-m_boxW, -20 * m_boxH);
        commitFloat();
      }
    }
    // 6. Annulla e ripeti.
    undo();
    undo();
    redo();
    // 7. Una riga aggiunta e annullata (solo geometria).
    addRow();
    undo();

    Result res;
    flushWindowToPages();
    res.canvas = canvasFromPages(m_pages, (int)gridW(), canvasLy());
    for (int i = 0; i < m_cols * m_rows; i++)
      res.empty.push_back(isPanelEmpty(i));
    res.moves = m_windowMoves - movesBefore;
    res.strokeBoxes     = boxes;
    res.strokeUndoExact = undoExact;
    return res;
  };

  auto firstDiffRow = [](const TRaster32P &a, const TRaster32P &b) -> int {
    if (!a || !b) return (a || b) ? 0 : -1;
    if (a->getLx() != b->getLx() || a->getLy() != b->getLy()) return -2;
    for (int y = 0; y < a->getLy(); y++)
      if (std::memcmp(a->pixels(y), b->pixels(y),
                      sizeof(TPixel32) * a->getLx()) != 0)
        return y;
    return -1;
  };

  // Prova 1, deterministica: tutto tranne il pennello, tela byte per byte.
  const bool strokes = false;
  Result full1 = runOnce(0, false);
  log << "PROVA 1 — senza pennellate, tele confrontate byte per byte\n";
  bool allOk = true;
  for (int wp : {1, 2, 3, 5}) {
    Result narrow = runOnce(wp, strokes);
    const int d   = firstDiffRow(full1.canvas, narrow.canvas);
    const bool emptiesOk = (full1.empty == narrow.empty);
    const bool ok        = (d == -1) && emptiesOk;
    allOk                = allOk && ok;
    log << "finestra di " << wp << " pagine: "
        << (ok ? "IDENTICA" : "DIVERSA") << "  (spostamenti della finestra: "
        << narrow.moves << ")";
    if (d == -2)
      log << "  dimensioni diverse "
          << full1.canvas->getLx() << "x" << full1.canvas->getLy() << " vs "
          << narrow.canvas->getLx() << "x" << narrow.canvas->getLy();
    else if (d >= 0)
      log << "  prima riga diversa: " << d;
    if (!emptiesOk) log << "  «pannello vuoto?» diverso";
    log << "\n";
  }
  // Prova 2, il pennello: MyPaint ha una componente casuale, quindi non si
  // confrontano i pixel ma DOVE cade la pennellata (entro 4 px) e che
  // annulla/ripeti rimettano la tela ESATTAMENTE com'era.
  if (m_style) {
    log << "PROVA 2 — pennellate: posizione (entro 4 px) e annulla/ripeti esatti\n";
    Result sf = runOnce(0, true);
    auto near = [](const QRect &a, const QRect &b) {
      return std::abs(a.left() - b.left()) <= 4 &&
             std::abs(a.right() - b.right()) <= 4 &&
             std::abs(a.top() - b.top()) <= 4 &&
             std::abs(a.bottom() - b.bottom()) <= 4;
    };
    for (int wp : {0, 1, 3}) {
      Result sn = wp == 0 ? sf : runOnce(wp, true);
      bool posOk = sn.strokeBoxes.size() == sf.strokeBoxes.size();
      for (size_t i = 0; posOk && i < sn.strokeBoxes.size(); i++)
        posOk = !sn.strokeBoxes[i].isEmpty() &&
                near(sn.strokeBoxes[i], sf.strokeBoxes[i]);
      const bool ok = posOk && sn.strokeUndoExact;
      allOk         = allOk && ok;
      log << "finestra di " << wp << " pagine (0 = piena): "
          << (ok ? "OK" : "DIFETTO") << "  annulla/ripeti "
          << (sn.strokeUndoExact ? "esatti" : "NON esatti");
      for (const QRect &b : sn.strokeBoxes)
        log << "  [" << b.left() << "," << b.top() << " " << b.width() << "x"
            << b.height() << "]";
      log << "\n";
    }
  } else {
    log << "PROVA 2 saltata: nessun pennello caricato\n";
  }
  log << (allOk ? "ESITO: OK\n" : "ESITO: DIFETTO\n");

  // ── Tutto com'era ──
  m_ras = sv.ras;
  m_pages = sv.pages;
  m_rows = sv.rows; m_cols = sv.cols;
  m_winFirstPage = sv.winFirst; m_winPageCount = sv.winCount;
  m_windowPages = sv.windowPages;
  m_boxH = sv.boxH; m_boxAspect = sv.boxAspect;
  m_zoom = sv.zoom; m_rot = sv.rot; m_pan = sv.pan;
  m_merges = sv.merges; m_bandDirty = sv.dirty;
  m_undo = sv.undo; m_redo = sv.redo;
  m_strokeTiles.clear();
  m_floatImg = sv.floatImg; m_floatCenter = sv.floatCenter;
  m_floatScale = sv.floatScale; m_floatAngle = sv.floatAngle;
  m_floatSrcRect = sv.floatSrc; m_floatWasMove = sv.floatWasMove;
  m_selectMode = sv.selectMode; m_xformMode = sv.xformMode;
  m_selection = sv.selection;
  if (m_saveTimer && !sv.timerActive) m_saveTimer->stop();
  TApp::instance()->getCurrentScene()->setDirtyFlag(sv.sceneDirty);
  m_selfTesting = false;
  syncAppUndoActions();
  updateScrollBars();
  update();
}
