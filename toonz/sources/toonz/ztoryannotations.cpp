#include "ztoryannotations.h"

#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/tframehandle.h"
#include "toonz/preferences.h"
#include "toonz/stage.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "tpalette.h"
#include "tpixel.h"
#include "tgeometry.h"
#include "tenv.h"
#include "tsystem.h"
#include "tlevel_io.h"
#include "timage_io.h"
#include "tlevel.h"
#include "pane.h"

#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QFrame>
#include <QSettings>
#include <QFileDialog>
#include <QDir>

#include <algorithm>
#include <vector>
#include <map>

// ── Constants ────────────────────────────────────────────────────────────────

static const std::wstring kLevelName  = L"Annotazioni";
static const int          kAnnotStyle = 1;  // palette style 1 = red
static const int          kThumb      = 64;  // thumbnail size (px)

// ── Column helpers ────────────────────────────────────────────────────────────

static int findAnnotationCol(TXsheet *xsh) {
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *col = xsh->getColumn(c);
    if (!col) continue;
    TXshLevelColumn *lc = col->getLevelColumn();
    if (!lc || lc->isEmpty()) continue;
    int r0, r1;
    lc->getRange(r0, r1);
    TXshCell cell = lc->getCell(r0);
    if (cell.isEmpty() || !cell.m_level) continue;
    if (cell.m_level->getName() == kLevelName) return c;
  }
  return -1;
}

static int computeShotDuration(TXsheet *subXsh) {
  int dur = 0;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    TXshColumn *col = subXsh->getColumn(c);
    if (!col) continue;
    TXshLevelColumn *lc = col->getLevelColumn();
    if (lc && !lc->isEmpty()) dur = std::max(dur, lc->getRowCount());
  }
  return dur > 0 ? dur : 24;
}

// ── Arrow library loading ──────────────────────────────────────────────────────

// Reads the first frame of a .pli and returns its vector image (strokes + the
// original palette), so colors can be preserved on insertion.
static TVectorImageP loadArrowImage(const TFilePath &path) {
  try {
    TLevelReaderP lr(path);
    TLevelP level = lr->loadInfo();
    if (!level || level->begin() == level->end()) return TVectorImageP();
    TFrameId fid     = level->begin()->first;
    TImageReaderP ir = lr->getFrameReader(fid);
    if (!ir) return TVectorImageP();
    TImageP img      = ir->load();
    TVectorImageP vi = img;
    // The PLI reader stores the palette on the LEVEL, not the image. Attach it
    // so mergeImage() can merge the source palette and preserve the arrow's
    // original colors (otherwise its style indices fall back to the annotation
    // level's red and the arrows come in red).
    if (vi && !vi->getPalette() && level->getPalette())
      vi->setPalette(level->getPalette());
    return vi;
  } catch (...) {
  }
  return TVectorImageP();
}

// Folders scanned for arrow .pli files: bundled library + optional user folder.
static std::vector<TFilePath> arrowFolders() {
  std::vector<TFilePath> folders;
  folders.push_back(TEnv::getStuffDir() + "library" + "directional arrows");
  QString userDir = QSettings().value("Ztoryc/ArrowsFolder").toString();
  if (!userDir.isEmpty())
    folders.push_back(TFilePath(userDir.toStdWString()));
  return folders;
}

// Leading integer of a file name ("10 Truck In" → 10) for natural sorting.
static int leadingNumber(const QString &name) {
  int i = 0, n = name.size(), v = 0;
  bool any = false;
  while (i < n && name[i].isDigit()) { v = v * 10 + name[i].digitValue(); i++; any = true; }
  return any ? v : 1 << 30;
}

// ── Core logic ────────────────────────────────────────────────────────────────

int ZtoryCameraMovesPanel::ensureAnnotationColumn() {
  TApp       *app    = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet    *subXsh = app->getCurrentXsheet()->getXsheet();
  if (!scene || !subXsh) return -1;

  int existing = findAnnotationCol(subXsh);
  if (existing >= 0) return existing;

  // Create PLI level "Annotazioni"
  TXshSimpleLevel *sl = new TXshSimpleLevel();
  sl->setType(PLI_XSHLEVEL);
  sl->setName(kLevelName);

  TFilePath path = scene->getDefaultLevelPath(PLI_XSHLEVEL, kLevelName);
  sl->setPath(path);

  // Palette: style 0 = transparent (default), style 1 = red annotations
  TPalette *pal = new TPalette();
  pal->addStyle(TPixel32(220, 50, 50));
  sl->setPalette(pal);

  // setScene must be called before setFrame — setFrame calls getScene() internally
  sl->setScene(scene);
  scene->getLevelSet()->insertLevel(sl);

  // Frame 1: empty TVectorImage (setFrame also calls img->setPalette(getPalette()))
  TVectorImageP vi = new TVectorImage();
  sl->setFrame(TFrameId(1), vi.getPointer());

  // Insert column at position 0 — existing columns shift right
  subXsh->insertColumn(0);

  int dur = computeShotDuration(subXsh);
  for (int r = 0; r < dur; r++)
    subXsh->setCell(r, 0, TXshCell(sl, TFrameId(1)));

  app->getCurrentXsheet()->notifyXsheetChanged();
  return 0;
}

void ZtoryCameraMovesPanel::insertArrowFromFile(const TFilePath &pliPath) {
  TApp       *app    = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet    *subXsh = app->getCurrentXsheet()->getXsheet();

  if (!scene || !subXsh ||
      scene->getChildStack()->getAncestorCount() == 0) {
    DVGui::warning(
        tr("Open a shot first: double-click a shot in the Board to go "
           "inside it."));
    return;
  }

  TVectorImageP srcVi = loadArrowImage(pliPath);
  if (!srcVi || srcVi->getStrokeCount() == 0) {
    DVGui::warning(tr("Could not read the arrow: %1")
                       .arg(QString::fromStdWString(pliPath.getWideName())));
    return;
  }

  int col = ensureAnnotationColumn();
  if (col < 0) return;

  TXshLevelColumn *lc = subXsh->getColumn(col)->getLevelColumn();
  if (!lc) return;
  int r0, r1;
  lc->getRange(r0, r1);

  // Annotation level: take it from the column's first cell.
  TXshSimpleLevel *sl =
      dynamic_cast<TXshSimpleLevel *>(lc->getCell(r0).m_level.getPointer());
  if (!sl) return;

  // Frame-aware insertion. Insert on the CURRENT row; create a new drawing if
  // the cell is empty, or if it's a held cell and "Enable Creation in Hold
  // Cells" is on — so a new arrow on a different frame doesn't pile onto the
  // previous drawing.
  int row = TApp::instance()->getCurrentFrame()->getFrame();
  if (row < 0) row = r0;

  TXshCell cur   = lc->getCell(row);
  bool isHold = false;
  if (!cur.isEmpty() && row > 0) {
    TXshCell prev = lc->getCell(row - 1);
    isHold = (!prev.isEmpty() && prev.m_level == cur.m_level &&
              prev.m_frameId == cur.m_frameId);
  }
  bool createNew = cur.isEmpty() ||
                   (isHold && Preferences::instance()->isCreationInHoldCellsEnabled());

  TFrameId      fid;
  TVectorImageP vi;
  if (createNew) {
    fid = TFrameId(sl->getLastFid().getNumber() + 1);
    vi  = new TVectorImage();
    vi->setPalette(sl->getPalette());
    sl->setFrame(fid, vi.getPointer());
    lc->setCell(row, TXshCell(sl, fid));
  } else {
    fid = cur.m_frameId;
    vi  = sl->getFrame(fid, true);
    if (!vi) {
      vi = new TVectorImage();
      vi->setPalette(sl->getPalette());
      sl->setFrame(fid, vi.getPointer());
    }
  }

  // mergeImage() (3-arg form) auto-merges the source palette into the
  // annotation palette and copies strokes AND region fills, preserving the
  // arrow's ORIGINAL colors. (Doing the palette remap by hand made them red.)
  int before = (int)vi->getStrokeCount();
  vi->mergeImage(srcVi, TAffine(), false);
  int added = (int)vi->getStrokeCount() - before;

  // Group the inserted strokes so the whole arrow selects as one unit.
  if (added > 1) vi->group(before, added);

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->notifyCastChange();
  IconGenerator::instance()->invalidate(sl, fid);
}

// ── Library grid ────────────────────────────────────────────────────────────────

void ZtoryCameraMovesPanel::rebuildLibrary() {
  if (!m_grid) return;
  // Clear existing buttons
  QLayoutItem *item;
  while ((item = m_grid->takeAt(0)) != nullptr) {
    if (item->widget()) item->widget()->deleteLater();
    delete item;
  }

  // Collect .pli files across folders, de-duplicated by file name.
  std::vector<TFilePath> files;
  for (const TFilePath &folder : arrowFolders()) {
    if (folder.isEmpty() || !TFileStatus(folder).isDirectory()) continue;
    TFilePathSet fps = TSystem::readDirectory(folder, false, true);
    for (const TFilePath &fp : fps)
      if (fp.getType() == "pli") files.push_back(fp);
  }

  std::sort(files.begin(), files.end(),
            [](const TFilePath &a, const TFilePath &b) {
              QString na = QString::fromStdWString(a.getWideName());
              QString nb = QString::fromStdWString(b.getWideName());
              int la = leadingNumber(na), lb = leadingNumber(nb);
              if (la != lb) return la < lb;
              return na.localeAwareCompare(nb) < 0;
            });

  const int cols = 3;
  int idx = 0;
  for (const TFilePath &fp : files) {
    QString name = QString::fromStdWString(fp.getWideName());
    QToolButton *btn = new QToolButton();
    btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
    btn->setIconSize(QSize(kThumb, kThumb));
    btn->setFixedSize(kThumb + 10, kThumb + 10);
    btn->setToolTip(name);
    btn->setStyleSheet(
        "QToolButton{background:#2b2b2b;border:1px solid #444;border-radius:4px;}"
        "QToolButton:hover{background:#3a3a3a;border-color:#7defa0;}");

    try {
      TRaster32P ras =
          IconGenerator::generateVectorFileIcon(fp, TDimension(kThumb, kThumb),
                                                 TFrameId(1));
      if (ras) {
        QImage img = rasterToQImage(ras, false, true);
        btn->setIcon(QPixmap::fromImage(img));
      }
    } catch (...) {
    }

    TFilePath captured = fp;
    connect(btn, &QToolButton::clicked, this,
            [this, captured] { insertArrowFromFile(captured); });

    m_grid->addWidget(btn, idx / cols, idx % cols);
    idx++;
  }

  if (idx == 0) {
    QLabel *empty = new QLabel(
        tr("No arrows found.\nAdd some .pli files to the library folder."));
    empty->setWordWrap(true);
    empty->setStyleSheet("color:#999;font-size:11px;");
    m_grid->addWidget(empty, 0, 0, 1, cols);
  }
}

void ZtoryCameraMovesPanel::chooseUserFolder() {
  QString cur = QSettings().value("Ztoryc/ArrowsFolder").toString();
  QString dir = QFileDialog::getExistingDirectory(
      this, tr("Choose your arrows folder"), cur);
  if (dir.isEmpty()) return;
  QSettings().setValue("Ztoryc/ArrowsFolder", dir);
  rebuildLibrary();
}

// ── Panel UI ──────────────────────────────────────────────────────────────────

ZtoryCameraMovesPanel::ZtoryCameraMovesPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle(tr("Arrows"));
  setObjectName("ZtoryCameraMoves");

  QWidget     *root = new QWidget(this);
  QVBoxLayout *lay  = new QVBoxLayout(root);
  lay->setContentsMargins(8, 8, 8, 8);
  lay->setSpacing(6);

  QLabel *title = new QLabel(tr("Directional Arrows"), root);
  title->setStyleSheet("color:#ccc;font-weight:bold;");
  lay->addWidget(title);

  // Scrollable thumbnail grid
  QScrollArea *scroll = new QScrollArea(root);
  scroll->setWidgetResizable(true);
  scroll->setStyleSheet("QScrollArea{background:#1e1e1e;border:none;}");
  QWidget *gridHost = new QWidget();
  gridHost->setStyleSheet("background:#1e1e1e;");
  m_grid = new QGridLayout(gridHost);
  m_grid->setSpacing(6);
  m_grid->setContentsMargins(4, 4, 4, 4);
  m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  scroll->setWidget(gridHost);
  lay->addWidget(scroll, 1);

  // Footer: choose user folder + refresh
  QHBoxLayout *foot = new QHBoxLayout();
  QPushButton *folderBtn = new QPushButton(tr("Custom folder…"), root);
  folderBtn->setFixedHeight(26);
  connect(folderBtn, &QPushButton::clicked, this,
          &ZtoryCameraMovesPanel::chooseUserFolder);
  QPushButton *refreshBtn = new QPushButton(tr("↻"), root);
  refreshBtn->setFixedSize(26, 26);
  refreshBtn->setToolTip(tr("Reload the library"));
  connect(refreshBtn, &QPushButton::clicked, this,
          [this] { rebuildLibrary(); });
  foot->addWidget(folderBtn, 1);
  foot->addWidget(refreshBtn);
  lay->addLayout(foot);

  setWidget(root);
  rebuildLibrary();
}

// ── Panel factory ─────────────────────────────────────────────────────────────

class ZtoryCameraMovesPanelFactory final : public TPanelFactory {
public:
  ZtoryCameraMovesPanelFactory() : TPanelFactory("ZtoryCameraMoves") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryCameraMovesPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Arrows"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryCameraMovesPanelFactory;
