#include "ztoryautomatte.h"
#include "ztoryautomattecore.h"

// Tnz6 includes
#include "tapp.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/levelset.h"
#include "toonz/levelproperties.h"
#include "toonz/tcamera.h"
#include "toonz/stage.h"
#include "toonz/tstageobject.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzimageutils.h"
#include "toonz/imagemanager.h"

// TnzCore includes
#include "tpalette.h"
#include "tcolorstyles.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "trasterimage.h"
#include "ttoonzimage.h"
#include "tundo.h"
#include "tsystem.h"

// Qt includes
#include <limits>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QColorDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <QMainWindow>

#include <map>
#include <set>

namespace {

const wchar_t *kMatteSuffix = L"_matte";
const int kMatteStyle       = 1;  // the matte paints with this palette style

QWidget *dialogParent() { return TApp::instance()->getMainWindow(); }

//-----------------------------------------------------------------------------
// The drawing column

struct Source {
  TXshSimpleLevel *sl = nullptr;
  std::vector<std::pair<int, TFrameId>> cells;  // row, frame of sl
  std::set<TFrameId> fids;
};

Source readSource(TXsheet *xsh, int col) {
  Source s;
  int r0 = 0, r1 = -1;
  if (!xsh->getCellRange(col, r0, r1)) return s;
  for (int r = r0; r <= r1; r++) {
    const TXshCell cell = xsh->getCell(r, col);
    if (cell.isEmpty() || !cell.m_level) continue;
    TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
    if (!sl) continue;
    if (!s.sl) {
      const int t = sl->getType();
      if (t != PLI_XSHLEVEL && t != TZP_XSHLEVEL && t != OVL_XSHLEVEL)
        return s;
      s.sl = sl;
    }
    if (sl != s.sl) continue;  // another level in the same column: no matte
    s.cells.push_back({r, cell.m_frameId});
    s.fids.insert(cell.m_frameId);
  }
  return s;
}

// The matte made earlier for this level, in the column just below.
TXshSimpleLevel *existingMatte(TXsheet *xsh, int col, TXshSimpleLevel *src) {
  if (col <= 0 || !src) return nullptr;
  int r0 = 0, r1 = -1;
  if (!xsh->getCellRange(col - 1, r0, r1)) return nullptr;
  const std::wstring want = src->getName() + kMatteSuffix;
  for (int r = r0; r <= r1; r++) {
    const TXshCell cell = xsh->getCell(r, col - 1);
    if (cell.isEmpty() || !cell.m_level) continue;
    TXshSimpleLevel *sl = cell.m_level->getSimpleLevel();
    if (sl && sl->getType() == TZP_XSHLEVEL && sl->getName() == want)
      return sl;
    return nullptr;
  }
  return nullptr;
}

//-----------------------------------------------------------------------------
// Geometry: one raster size and dpi for the whole matte level, centred on the
// origin like every Toonz raster.

struct Frame {
  int w = 0, h = 0;
  double dpi = 0;
};

Frame matteFrame(const Source &s, int gap) {
  Frame f;
  if (s.sl->getType() == PLI_XSHLEVEL) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    f.dpi             = scene->getCurrentCamera()->getDpi().x;
    if (f.dpi <= 0) f.dpi = Stage::standardDpi;
    const double k = f.dpi / Stage::inch;
    double hx = 0, hy = 0;
    for (const TFrameId &fid : s.fids) {
      TVectorImageP vi = s.sl->getFrame(fid, false);
      if (!vi) continue;
      // The strokes' boxes, not vi->getBBox(): on a drawing just read from
      // its .pli that box is still empty (seen with a probe, 2026-10-19), and
      // the matte would come out a few pixels wide.
      TRectD b;
      for (UINT i = 0; i < vi->getStrokeCount(); i++)
        b += vi->getStroke(i)->getBBox();
      if (b.isEmpty()) continue;
      hx = std::max({hx, std::abs(b.x0) * k, std::abs(b.x1) * k});
      hy = std::max({hy, std::abs(b.y0) * k, std::abs(b.y1) * k});
    }
    // A margin wider than the gap, so the outside reaches round the drawing.
    const int margin = gap + 4;
    f.w = 2 * ((int)std::ceil(hx) + margin);
    f.h = 2 * ((int)std::ceil(hy) + margin);
    return f;
  }
  for (const TFrameId &fid : s.fids) {
    TImageP img = s.sl->getFrame(fid, false);
    if (TToonzImageP ti = img) {
      f.w = ti->getSize().lx, f.h = ti->getSize().ly;
      double dx, dy;
      ti->getDpi(dx, dy);
      f.dpi = dx;
      return f;
    }
    if (TRasterImageP ri = img) {
      if (!ri->getRaster()) continue;
      f.w = ri->getRaster()->getLx(), f.h = ri->getRaster()->getLy();
      double dx, dy;
      ri->getDpi(dx, dy);
      f.dpi = dx;
      return f;
    }
  }
  return f;
}

// 1 where the drawing has ink or paint, in the matte's raster.
std::vector<uint8_t> solidMask(const Source &s, const TFrameId &fid,
                               const Frame &f) {
  std::vector<uint8_t> solid((size_t)f.w * f.h, 0);
  TImageP img = s.sl->getFrame(fid, false);
  TRasterCM32P cm;
  if (TVectorImageP vi = img) {
    const double k = f.dpi / Stage::inch;
    TPalette *pal  = vi->getPalette() ? vi->getPalette() : s.sl->getPalette();
    if (!pal) return solid;
    TToonzImageP ti = ToonzImageUtils::vectorToToonzImage(
        vi, TScale(k), pal, TPointD(-f.w / 2, -f.h / 2), TDimension(f.w, f.h),
        nullptr, true);
    if (ti) cm = ti->getCMapped();
  } else if (TToonzImageP ti = img)
    cm = ti->getCMapped();

  if (cm) {
    if (cm->getLx() != f.w || cm->getLy() != f.h) return solid;
    cm->lock();
    for (int y = 0; y < f.h; y++) {
      const TPixelCM32 *p = cm->pixels(y);
      for (int x = 0; x < f.w; x++)
        solid[(size_t)y * f.w + x] = (p[x].getTone() < 255 || p[x].getPaint());
    }
    cm->unlock();
    return solid;
  }

  if (TRasterImageP ri = img) {
    TRaster32P r = ri->getRaster();
    if (!r || r->getLx() != f.w || r->getLy() != f.h) return solid;
    r->lock();
    for (int y = 0; y < f.h; y++) {
      const TPixel32 *p = r->pixels(y);
      for (int x = 0; x < f.w; x++) {
        // Transparent background, or an opaque white one (a scan).
        const bool white = p[x].m == 255 && p[x].r > 235 && p[x].g > 235 &&
                           p[x].b > 235;
        solid[(size_t)y * f.w + x] = p[x].m > 32 && !white;
      }
    }
    r->unlock();
  }
  return solid;
}

TToonzImageP makeMatteImage(const Source &s, const TFrameId &fid,
                            const Frame &f, int gap) {
  const std::vector<uint8_t> matte =
      ZtoryAutoMatteCore::computeMatte(solidMask(s, fid, f), f.w, f.h, gap);
  TRasterCM32P ras(f.w, f.h);
  ras->fill(TPixelCM32());
  int x0 = f.w, y0 = f.h, x1 = -1, y1 = -1;
  ras->lock();
  for (int y = 0; y < f.h; y++) {
    TPixelCM32 *p = ras->pixels(y);
    for (int x = 0; x < f.w; x++) {
      if (!matte[(size_t)y * f.w + x]) continue;
      p[x] = TPixelCM32(0, kMatteStyle, 255);
      x0 = std::min(x0, x), y0 = std::min(y0, y);
      x1 = std::max(x1, x), y1 = std::max(y1, y);
    }
  }
  ras->unlock();
  TToonzImageP ti(ras, x1 >= 0 ? TRect(x0, y0, x1, y1) : TRect());
  ti->setDpi(f.dpi, f.dpi);
  return ti;
}

void storeFrame(TXshSimpleLevel *sl, const TFrameId &fid, const TImageP &img) {
  sl->setFrame(fid, img);
  // Without the image info the new frame is not rendered (levelcreatepopup).
  if (TImageInfo *info = sl->getFrameInfo(fid, true))
    ImageBuilder::setImageInfo(*info, img.getPointer());
  IconGenerator::instance()->invalidate(sl, fid);
}

void setLevelGeometry(TXshSimpleLevel *sl, const Frame &f) {
  LevelProperties *p = sl->getProperties();
  p->setDpiPolicy(LevelProperties::DP_ImageDpi);
  p->setDpi(f.dpi);
  p->setImageDpi(TPointD(f.dpi, f.dpi));
  p->setImageRes(TDimension(f.w, f.h));
}

void notifyAll() {
  TApp *app = TApp::instance();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->notifyCastChange();
  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentLevel()->notifyLevelChange();
}

// Computes the matte frames with a progress bar; false if cancelled.
bool computeFrames(const Source &s, const Frame &f, int gap,
                   std::map<TFrameId, TImageP> &out) {
  QProgressDialog progress(QObject::tr("Generating the auto-matte..."),
                           QObject::tr("Cancel"), 0, (int)s.fids.size(),
                           dialogParent());
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(400);
  int i = 0;
  for (const TFrameId &fid : s.fids) {
    progress.setValue(i++);
    if (progress.wasCanceled()) return false;
    out[fid] = makeMatteImage(s, fid, f, gap);
  }
  progress.setValue(i);
  return true;
}

//-----------------------------------------------------------------------------
// Undo

class GenerateUndo final : public TUndo {
  TXsheetP m_xsh;
  int m_col;
  TXshSimpleLevelP m_matte;
  std::vector<std::pair<int, TFrameId>> m_cells;
  std::string m_colName;

public:
  GenerateUndo(TXsheet *xsh, int col, TXshSimpleLevel *matte,
               const std::vector<std::pair<int, TFrameId>> &cells,
               const std::string &colName)
      : m_xsh(xsh)
      , m_col(col)
      , m_matte(matte)
      , m_cells(cells)
      , m_colName(colName) {}

  // The column below the drawing, parented to it: the matte follows every
  // move of the drawing column without keys of its own.
  void place() const {
    m_xsh->insertColumn(m_col);
    for (const auto &c : m_cells)
      m_xsh->setCell(c.first, m_col, TXshCell(m_matte.getPointer(), c.second));
    m_xsh->setStageObjectParent(TStageObjectId::ColumnId(m_col),
                                TStageObjectId::ColumnId(m_col + 1));
    m_xsh->getStageObject(TStageObjectId::ColumnId(m_col))->setName(m_colName);
  }

  void undo() const override {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    m_xsh->removeColumn(m_col);
    scene->getLevelSet()->removeLevel(m_matte.getPointer());
    notifyAll();
  }
  void redo() const override {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    scene->getLevelSet()->insertLevel(m_matte.getPointer());
    place();
    notifyAll();
  }
  int getSize() const override { return sizeof(*this) + (1 << 20); }
  QString getHistoryString() override {
    return QObject::tr("Generate Auto-Matte  %1")
        .arg(QString::fromStdWString(m_matte->getName()));
  }
};

// Bytes the kept frames really hold: the undo manager drops old undos by
// this, and a flat guess let a long column keep hundreds of MB (review 23/10).
static qint64 imagesBytes(const std::map<TFrameId, TImageP> &imgs) {
  qint64 bytes = 0;
  for (const auto &it : imgs) {
    if (TToonzImageP ti = it.second) {
      if (TRasterCM32P ras = ti->getCMapped())
        bytes += (qint64)ras->getLx() * ras->getLy() * ras->getPixelSize();
    } else if (TRasterImageP ri = it.second) {
      if (TRasterP ras = ri->getRaster())
        bytes += (qint64)ras->getLx() * ras->getLy() * ras->getPixelSize();
    }
  }
  return bytes;
}

class UpdateUndo final : public TUndo {
  TXsheetP m_xsh;
  int m_col;
  TXshSimpleLevelP m_matte;
  std::map<TFrameId, TImageP> m_old, m_new;  // null old = frame added
  Frame m_oldFrame, m_newFrame;
  std::vector<TXshCell> m_oldCells, m_newCells;  // the matte column, row 0..

  void apply(const std::map<TFrameId, TImageP> &imgs, const Frame &f,
             const std::vector<TXshCell> &cells) const {
    setLevelGeometry(m_matte.getPointer(), f);
    for (const auto &it : imgs) {
      if (it.second)
        storeFrame(m_matte.getPointer(), it.first, it.second);
      else
        m_matte->eraseFrame(it.first);
    }
    const int n = (int)std::max(m_oldCells.size(), m_newCells.size());
    for (int r = 0; r < n; r++)
      m_xsh->setCell(r, m_col, r < (int)cells.size() ? cells[r] : TXshCell());
    m_matte->setDirtyFlag(true);
    notifyAll();
  }

public:
  UpdateUndo(TXsheet *xsh, int col, TXshSimpleLevel *matte,
             const std::map<TFrameId, TImageP> &oldImgs,
             const std::map<TFrameId, TImageP> &newImgs, const Frame &oldF,
             const Frame &newF, const std::vector<TXshCell> &oldCells,
             const std::vector<TXshCell> &newCells)
      : m_xsh(xsh)
      , m_col(col)
      , m_matte(matte)
      , m_old(oldImgs)
      , m_new(newImgs)
      , m_oldFrame(oldF)
      , m_newFrame(newF)
      , m_oldCells(oldCells)
      , m_newCells(newCells) {}

  void undo() const override { apply(m_old, m_oldFrame, m_oldCells); }
  void redo() const override { apply(m_new, m_newFrame, m_newCells); }
  int getSize() const override {
    const qint64 bytes = sizeof(*this) + imagesBytes(m_old) + imagesBytes(m_new);
    return (int)std::min<qint64>(bytes, std::numeric_limits<int>::max());
  }
  QString getHistoryString() override {
    return QObject::tr("Update Auto-Matte  %1")
        .arg(QString::fromStdWString(m_matte->getName()));
  }
};

//-----------------------------------------------------------------------------
// Settings dialog: colour and gap radius, remembered between uses.  With no
// colour (Update) only the radius is asked: the matte's colour then lives in
// its palette.

bool askSettings(QColor *color, int *gap) {
  const bool withColor = color != nullptr;
  QColor dummy;
  if (!color) color = &dummy;
  QSettings set;
  *color = set.value("ZtoryAutoMatte/color", QColor(Qt::white)).value<QColor>();
  *gap   = set.value("ZtoryAutoMatte/gap", 10).toInt();

  QDialog dlg(dialogParent());
  dlg.setWindowTitle(withColor ? QObject::tr("Generate Auto-Matte")
                               : QObject::tr("Update Auto-Matte"));
  QFormLayout *lay = new QFormLayout(&dlg);

  QPushButton *colorBtn = new QPushButton(&dlg);
  colorBtn->setFixedSize(60, 22);
  auto paint = [colorBtn](const QColor &c) {
    colorBtn->setStyleSheet(
        QString("QPushButton { background-color: %1; border: 1px solid #000; }")
            .arg(c.name()));
  };
  paint(*color);
  QObject::connect(colorBtn, &QPushButton::clicked, [&]() {
    const QColor c = QColorDialog::getColor(*color, &dlg,
                                            QObject::tr("Matte Colour"));
    if (c.isValid()) *color = c, paint(c);
  });
  if (withColor)
    lay->addRow(QObject::tr("Colour:"), colorBtn);
  else
    colorBtn->hide();

  QSpinBox *gapBox = new QSpinBox(&dlg);
  gapBox->setRange(0, 60);
  gapBox->setValue(*gap);
  gapBox->setSuffix(QObject::tr(" px"));
  gapBox->setToolTip(
      QObject::tr("Openings in the outline narrower than about twice this "
                  "are closed. Raise it for loose sketches."));
  lay->addRow(QObject::tr("Close gaps up to:"), gapBox);

  QDialogButtonBox *bb =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addRow(bb);

  if (dlg.exec() != QDialog::Accepted) return false;
  *gap = gapBox->value();
  if (withColor) set.setValue("ZtoryAutoMatte/color", *color);
  set.setValue("ZtoryAutoMatte/gap", *gap);
  return true;
}

//-----------------------------------------------------------------------------
// Commands

TXshSimpleLevel *generateWith(TXsheet *xsh, int col, const QColor &color,
                              int gap);

void generate(TXsheet *xsh, int col) {
  Source s = readSource(xsh, col);
  if (!s.sl || s.fids.empty()) return;
  QColor color;
  int gap = 0;
  if (!askSettings(&color, &gap)) return;
  generateWith(xsh, col, color, gap);
}

TXshSimpleLevel *generateWith(TXsheet *xsh, int col, const QColor &color,
                              int gap) {
  Source s = readSource(xsh, col);
  if (!s.sl || s.fids.empty()) return nullptr;

  const Frame f = matteFrame(s, gap);
  if (f.w <= 0 || f.h <= 0 || f.dpi <= 0 || (qint64)f.w * f.h > 64000000LL) {
    QMessageBox::warning(dialogParent(), QObject::tr("Generate Auto-Matte"),
                         QObject::tr("The drawings of this column are empty "
                                     "or too large for a matte."));
    return nullptr;
  }
  std::map<TFrameId, TImageP> imgs;
  if (!computeFrames(s, f, gap, imgs)) return nullptr;

  // A free level name, and a file name no other level uses.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  std::wstring name = s.sl->getName() + kMatteSuffix;
  for (int n = 2; scene->getLevelSet()->getLevel(name) ||
                  TSystem::doesExistFileOrLevel(scene->decodeFilePath(
                      scene->getDefaultLevelPath(TZP_XSHLEVEL, name)));
       n++)
    name = s.sl->getName() + kMatteSuffix + L"_" + std::to_wstring(n);

  TXshLevel *level = scene->createNewLevel(TZP_XSHLEVEL, name);
  TXshSimpleLevel *matte = level ? level->getSimpleLevel() : nullptr;
  if (!matte) return nullptr;
  setLevelGeometry(matte, f);
  TPalette *pal = matte->getPalette();
  if (!pal) {
    pal = new TPalette();
    matte->setPalette(pal);
  }
  while (pal->getStyleCount() <= kMatteStyle)
    pal->getPage(0)->addStyle(TPixel32::White);
  pal->getStyle(kMatteStyle)
      ->setMainColor(TPixel32(color.red(), color.green(), color.blue()));
  pal->setPaletteName(name);
  for (const auto &it : imgs) storeFrame(matte, it.first, it.second);
  matte->setDirtyFlag(true);

  const std::string colName =
      QString::fromStdWString(s.sl->getName() + L" matte").toStdString();
  GenerateUndo *undo = new GenerateUndo(xsh, col, matte, s.cells, colName);
  undo->place();
  TUndoManager::manager()->add(undo);
  notifyAll();
  return matte;
}

void update(TXsheet *xsh, int col, TXshSimpleLevel *matte) {
  Source s = readSource(xsh, col);
  if (!s.sl || s.fids.empty() || !matte) return;
  // The radius again: the first one may have closed too little or too much.
  int gap = 0;
  if (!askSettings(nullptr, &gap)) return;

  Frame oldF;
  oldF.w   = matte->getProperties()->getImageRes().lx;
  oldF.h   = matte->getProperties()->getImageRes().ly;
  oldF.dpi = matte->getProperties()->getImageDpi().x;
  const Frame f = matteFrame(s, gap);
  if (f.w <= 0 || f.h <= 0 || f.dpi <= 0) return;

  std::map<TFrameId, TImageP> newImgs, oldImgs;
  if (!computeFrames(s, f, gap, newImgs)) return;
  for (const auto &it : newImgs) {
    TImageP old = matte->isFid(it.first) ? matte->getFrame(it.first, false)
                                         : TImageP();
    oldImgs[it.first] = old ? old->cloneImage() : TImageP();
  }

  // The matte column's cells follow the drawing column again.
  int r0 = 0, r1 = -1, m0 = 0, m1 = -1;
  xsh->getCellRange(col, r0, r1);
  xsh->getCellRange(col - 1, m0, m1);
  const int n = std::max(r1, m1) + 1;
  std::vector<TXshCell> oldCells(n), newCells(n);
  for (int r = 0; r < n; r++) oldCells[r] = xsh->getCell(r, col - 1);
  for (const auto &c : s.cells) newCells[c.first] = TXshCell(matte, c.second);

  UpdateUndo *undo = new UpdateUndo(xsh, col - 1, matte, oldImgs, newImgs,
                                    oldF, f, oldCells, newCells);
  undo->redo();
  TUndoManager::manager()->add(undo);
}

}  // namespace

//=============================================================================

void ZtoryAutoMatte::addColumnActions(QMenu *menu, TXsheet *xsh, int col) {
  if (!menu || !xsh || col < 0 || col >= xsh->getColumnCount()) return;
  Source s = readSource(xsh, col);
  if (!s.sl) return;
  // A matte column itself gets no matte.
  if (s.sl->getName().size() > 6 &&
      s.sl->getName().find(kMatteSuffix) != std::wstring::npos)
    return;

  if (TXshSimpleLevel *matte = existingMatte(xsh, col, s.sl)) {
    QAction *act = menu->addAction(QObject::tr("Update Auto-Matte..."));
    act->setToolTip(QObject::tr("Recompute the matte below after redrawing"));
    QObject::connect(act, &QAction::triggered,
                     [xsh, col, matte]() { update(xsh, col, matte); });
    return;
  }
  QAction *act = menu->addAction(QObject::tr("Generate Auto-Matte..."));
  QObject::connect(act, &QAction::triggered,
                   [xsh, col]() { generate(xsh, col); });
}

QString ZtoryAutoMatte::generateForTest(TXsheet *xsh, int col, int gap) {
  TXshSimpleLevel *m = generateWith(xsh, col, QColor(Qt::white), gap);
  return m ? QString::fromStdWString(m->getName()) : QString();
}
