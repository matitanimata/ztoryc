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
#include "toonz/stage.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "tvectorimage.h"
#include "tstroke.h"
#include "tpalette.h"
#include "tpixel.h"
#include "tgeometry.h"
#include "pane.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

#include <cmath>
#include <algorithm>
#include <vector>

// ── Constants ────────────────────────────────────────────────────────────────

static const std::wstring kLevelName  = L"Annotazioni";
static const int          kAnnotStyle = 1;  // palette style 1 = red

// ── Arrow geometry ────────────────────────────────────────────────────────────
// Stage::inch ≈ 53.33 units/inch → a 16×9" camera spans ±426 × ±240 units.
// Arrow is sized at ~40% of frame width (≈ 340 units total).

static TStroke *makeLine(TPointD a, TPointD b, double thick, int style) {
  std::vector<TThickPoint> pts = {
      {a.x, a.y, thick},
      {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5, thick},
      {b.x, b.y, thick},
  };
  TStroke *s = new TStroke(pts);
  s->setStyle(style);
  return s;
}

static std::vector<TStroke *> buildArrow(double dx, double dy) {
  double len = std::sqrt(dx * dx + dy * dy);
  if (len < 1e-9) return {};
  TPointD dir(dx / len, dy / len);
  TPointD perp(-dir.y, dir.x);

  const double shaft     = 155.0;  // half-length from center to near-tip
  const double headLen   = 38.0;   // depth of arrowhead
  const double headWidth = 24.0;   // half-width of arrowhead
  const double thick     = 5.0;

  TPointD tail = TPointD(-dir.x * shaft, -dir.y * shaft);
  TPointD tip  = TPointD(dir.x * (shaft + headLen), dir.y * (shaft + headLen));
  TPointD base = TPointD(dir.x * shaft, dir.y * shaft);
  TPointD b1   = base + perp * headWidth;
  TPointD b2   = base - perp * headWidth;

  return {
      makeLine(tail, base, thick, kAnnotStyle),
      makeLine(tip,  b1,   thick, kAnnotStyle),
      makeLine(tip,  b2,   thick, kAnnotStyle),
  };
}

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

  // Fill column for the full shot duration (computed after the insert,
  // so columns 1..N still carry the original content)
  int dur = computeShotDuration(subXsh);
  for (int r = 0; r < dur; r++)
    subXsh->setCell(r, 0, TXshCell(sl, TFrameId(1)));

  app->getCurrentXsheet()->notifyXsheetChanged();
  return 0;
}

void ZtoryCameraMovesPanel::insertArrow(double dx, double dy) {
  TApp       *app    = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet    *subXsh = app->getCurrentXsheet()->getXsheet();

  if (!scene || !subXsh ||
      scene->getChildStack()->getAncestorCount() == 0) {
    DVGui::warning(
        tr("Apri prima uno shot in SHOTEDITOR (doppio click su uno shot nel "
           "Board)."));
    return;
  }

  int col = ensureAnnotationColumn();
  if (col < 0) return;

  // Get TVectorImage at frame 1 of the annotation level
  TXshLevelColumn *lc = subXsh->getColumn(col)->getLevelColumn();
  if (!lc) return;
  int r0, r1;
  lc->getRange(r0, r1);
  TXshCell cell = lc->getCell(r0);
  TXshSimpleLevel *sl =
      dynamic_cast<TXshSimpleLevel *>(cell.m_level.getPointer());
  if (!sl) return;

  TVectorImageP vi = sl->getFrame(TFrameId(1), false);
  if (!vi) {
    vi = new TVectorImage();
    vi->setPalette(sl->getPalette());
    sl->setFrame(TFrameId(1), vi.getPointer());
  }

  auto strokes = buildArrow(dx, dy);
  for (TStroke *s : strokes) vi->addStroke(s, false);

  app->getCurrentXsheet()->notifyXsheetChanged();
  app->getCurrentScene()->notifyCastChange();
  IconGenerator::instance()->invalidate(sl, TFrameId(1));
}

// ── Panel UI ──────────────────────────────────────────────────────────────────

ZtoryCameraMovesPanel::ZtoryCameraMovesPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle(tr("Camera Moves"));
  setObjectName("ZtoryCameraMoves");

  QWidget    *root = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(root);
  lay->setContentsMargins(8, 8, 8, 8);
  lay->setSpacing(4);

  auto addSep = [&]() {
    QFrame *sep = new QFrame(root);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    lay->addWidget(sep);
  };

  auto addBtn = [&](const QString &label, double dx, double dy) {
    QPushButton *btn = new QPushButton(label, root);
    btn->setFixedHeight(28);
    connect(btn, &QPushButton::clicked,
            [this, dx, dy] { insertArrow(dx, dy); });
    lay->addWidget(btn);
  };

  lay->addWidget(new QLabel(tr("Pan"), root));
  addBtn(tr("→  Pan Right"),  1,  0);
  addBtn(tr("←  Pan Left"),  -1,  0);
  addBtn(tr("↑  Pan Up"),     0,  1);
  addBtn(tr("↓  Pan Down"),   0, -1);

  addSep();
  lay->addWidget(new QLabel(tr("Diagonale"), root));
  addBtn(tr("↗  Pan Up-Right"),   1,  1);
  addBtn(tr("↖  Pan Up-Left"),   -1,  1);
  addBtn(tr("↘  Pan Down-Right"),  1, -1);
  addBtn(tr("↙  Pan Down-Left"),  -1, -1);

  lay->addStretch();
  setWidget(root);
}

// ── Panel factory ─────────────────────────────────────────────────────────────

class ZtoryCameraMovesPanelFactory final : public TPanelFactory {
public:
  ZtoryCameraMovesPanelFactory() : TPanelFactory("ZtoryCameraMoves") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryCameraMovesPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Camera Moves"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryCameraMovesPanelFactory;
