#include "ztorythumbnailpanel.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"  // createQIcon (native toolbar icons)
#include "menubarcommandids.h"
#include "toonz/mypaintbrushstyle.h"  // getBrushesDirs()

#include "ztorymodel.h"    // addShotFromRasters
#include "toonz/studiopalette.h"
#include "toonz/toonzfolders.h"
#include "tsystem.h"
#include "tstream.h"
#include "toonzqt/dvscrollwidget.h"  // brush strip overflow
#include "ztoryundo.h"     // ztoryFindBoardPanel — undo for the export
#include "storyboardpanel.h"
#include "ztoryshotops.h"  // cameraRes, cameraAspect
#include "ztorypapersheet.h"     // printSheet / importSheet (paper import)
#include "ztorypapercapture.h"   // webcam capture dialog
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "tmsgcore.h"  // DVGui::info / warning

#include <QWidget>
#include <QRegExp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QButtonGroup>
#include <QSlider>
#include <QSpinBox>
#include <QSignalBlocker>
#include <QSize>
#include <QLabel>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

#include <algorithm>

// The room ships with five brushes and those five slots stay put.
static const int kFixedBrushSlots = 5;

namespace {

// Icon for a brush = its MyPaint preview PNG ("<brush>_prev.png" next to .myb).
QIcon brushIcon(const QString &relPath) {
  QString abs = ZtoryThumbnailCanvas::resolveBrushFile(relPath);
  if (abs.isEmpty()) return QIcon();
  QString prev = abs;
  prev.replace(QRegExp("\\.myb$"), "_prev.png");
  return QFileInfo::exists(prev) ? QIcon(prev) : QIcon();
}

// Hand-drawn tool icons (so they render regardless of the icon theme and match
// exactly what each tool does). Light grey to read on the dark toolbar.
QIcon arrowIcon() {  // selection-tool pointer (panel select)
  QPixmap pm(20, 20);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPolygonF a({QPointF(4, 3), QPointF(4, 16), QPointF(8, 12), QPointF(11, 18),
               QPointF(13, 17), QPointF(10, 11), QPointF(15, 11)});
  p.setBrush(QColor(220, 220, 220));
  p.setPen(QPen(QColor(40, 40, 40), 1));
  p.drawPolygon(a);
  return QIcon(pm);
}

QIcon dashedRectIcon() {  // rectangular marquee
  QPixmap pm(20, 20);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  QPen pen(QColor(225, 225, 225), 1.6);
  pen.setStyle(Qt::DashLine);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawRect(3, 4, 14, 12);
  return QIcon(pm);
}

QIcon lassoIcon() {  // classic dashed lasso loop
  QPixmap pm(20, 20);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen pen(QColor(225, 225, 225), 1.6);
  pen.setStyle(Qt::DashLine);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  QPainterPath path;
  path.moveTo(6, 4);
  path.cubicTo(16, 2, 19, 11, 12, 13);
  path.cubicTo(5, 15, 3, 8, 9, 7);
  p.drawPath(path);
  p.drawLine(QPointF(9, 13), QPointF(7, 18));  // the lasso tail
  return QIcon(pm);
}

// A flat swatch icon filled with a solid colour (for the preset colour chips).
QIcon swatchIcon(const QColor &c) {
  QPixmap pm(24, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  p.setPen(QColor(80, 80, 80));
  p.setBrush(c);
  p.drawRoundedRect(1, 1, 22, 22, 3, 3);
  return QIcon(pm);
}

// The active-colour swatch: larger, with a strong frame and a small ▾ so it
// reads as "current ink — click to pick", distinct from the preset chips.
QIcon activeSwatchIcon(const QColor &c) {
  QPixmap pm(38, 24);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setRenderHint(QPainter::Antialiasing, true);
  QPen frame(QColor(225, 225, 225));
  frame.setWidth(2);
  p.setPen(frame);
  p.setBrush(c);
  p.drawRoundedRect(1, 1, 35, 21, 4, 4);
  // ▾ chevron, bottom-right, in a colour that contrasts with the fill.
  const bool dark = (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 < 128;
  p.setPen(QPen(dark ? Qt::white : Qt::black, 1.4));
  p.drawLine(27, 15, 30, 18);
  p.drawLine(30, 18, 33, 15);
  return QIcon(pm);
}

}  // namespace

//=============================================================================
// ZtoryThumbnailPanel
//=============================================================================

ZtoryThumbnailPanel::ZtoryThumbnailPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle(tr("Ztoryc Thumbnails"));
  setObjectName("ZtoryThumbnailPanel");

  auto *container = new QWidget(this);
  auto *root      = new QVBoxLayout(container);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_canvas = new ZtoryThumbnailCanvas(container);

  // --- Palette toolbar -------------------------------------------------------
  auto *bar          = new QWidget(container);
  m_brushBarLay      = new QHBoxLayout(bar);
  m_brushBarLay->setContentsMargins(4, 3, 4, 3);
  m_brushBarLay->setSpacing(3);

  m_brushGroup = new QButtonGroup(this);
  m_brushGroup->setExclusive(true);
  connect(m_brushGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
          [this](int id) {
            if (id < 0 || id >= brushCount()) return;
            m_currentPreset = id;
            m_canvas->setBrushStyle(styleAt(id));
            syncSizeSliderToPreset();
            syncColorToPreset();
          });

  // Default brushes (icons come from each brush's MyPaint preview).  These are
  // only the seed: from here on the strip is whatever the user has made of it.
  loadBrushPalette();
  // Our own handle on the brush palette.  Nothing drives it yet: StyleEditor::
  // setPaletteHandle() is declared in the header but its body is COMMENTED OUT
  // in styleeditor.cpp, so the editor cannot be pointed anywhere but the
  // application's current palette.  Restoring it properly (the commented body
  // swaps the pointer without re-connecting the signals the constructor bound
  // to the old handle) is shared-code work — and an upstream candidate.  The
  // handle and the refresh below are the half that is ours, ready for it.
  m_brushHandle = new TPaletteHandle();
  m_brushHandle->setPalette(m_brushPalette.getPointer());
  connect(m_brushHandle, &TPaletteHandle::colorStyleChanged, this,
          [this](bool) { onBrushStyleEdited(); });
  connect(m_brushHandle, &TPaletteHandle::colorStyleSwitched, this,
          [this]() { onBrushStyleEdited(); });

  // The strip: only the brush buttons, wrapped so it scrolls on overflow.
  m_brushStrip    = new QWidget(bar);
  m_brushStripLay = new QHBoxLayout(m_brushStrip);
  m_brushStripLay->setContentsMargins(0, 0, 0, 0);
  m_brushStripLay->setSpacing(3);
  // No scroll widget of its own: the WHOLE toolbar scrolls (see the end of the
  // constructor), the way Tahoma's other toolbars do.  Two nested scrollers
  // would mean two sets of arrows and a strip that shrinks instead of letting
  // the bar overflow — which is how the selection arrow ended up sitting on
  // top of the size slider on a narrow panel.
  m_brushBarLay->addWidget(m_brushStrip, 1);
  rebuildBrushStrip();

  m_brushBarLay->addSpacing(10);

  // --- Colour chips + picker -------------------------------------------------
  m_brushBarLay->addWidget(new QLabel(tr("Color"), bar));
  struct { const char *tip; QColor c; } chips[] = {
      {"Animation blue #1D5C83", QColor(29, 92, 131)},
      {"Black", QColor(0, 0, 0)},
      {"Red", QColor(200, 30, 30)},
  };
  for (auto &ch : chips) {
    auto *b = new QToolButton(bar);
    b->setIcon(swatchIcon(ch.c));
    b->setToolTip(tr(ch.tip));
    QColor col = ch.c;
    connect(b, &QToolButton::clicked, this, [this, col] { selectColor(col); });
    m_brushBarLay->addWidget(b);
  }
  auto *vl = new QFrame(bar);
  vl->setFrameShape(QFrame::VLine);
  vl->setFrameShadow(QFrame::Sunken);
  m_brushBarLay->addWidget(vl);
  m_brushBarLay->addWidget(new QLabel(tr("Ink"), bar));

  m_swatch = new QToolButton(bar);
  m_swatch->setIconSize(QSize(38, 24));
  m_swatch->setToolTip(tr("Active ink — click to pick a custom color…"));
  connect(m_swatch, &QToolButton::clicked, this, [this] {
    QColor c = QColorDialog::getColor(Qt::black, this, tr("Ink color"));
    if (c.isValid()) selectColor(c);
  });
  m_brushBarLay->addWidget(m_swatch);

  m_brushBarLay->addSpacing(10);

  // --- Size ------------------------------------------------------------------
  m_brushBarLay->addWidget(new QLabel(tr("Size"), bar));
  auto *size = new QSlider(Qt::Horizontal, bar);
  size->setRange(0, 100);  // -> log size modifier [-2 .. +4]
  size->setValue(33);      // ~0 (brush default)
  size->setFixedWidth(110);
  m_sizeValue = new QLabel(bar);
  m_sizeValue->setMinimumWidth(38);
  m_sizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  connect(size, &QSlider::valueChanged, this, [this](int v) {
    TMyPaintBrushStyle *st = styleAt(m_currentPreset);
    if (!st) return;
    // Write the radius INTO the brush: the size belongs to it, so it travels
    // with the palette and survives a save/reload without anything applied on
    // top. The slider spans a sensible sketching range in log-radius units.
    st->setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                     (float)(-0.5 + 5.0 * (v / 100.0)));
    m_canvas->setBrushStyle(st);  // refresh the cached radius + cursor
    updateSizeValueLabel();
    saveBrushPalette();
  });
  m_sizeSlider = size;
  m_brushBarLay->addWidget(size);
  m_brushBarLay->addWidget(m_sizeValue);

  // NO addStretch() here.  It used to sit between Size and the selection tools,
  // which is what left the wide empty gap in the middle of the bar while the
  // brush strip was squeezed to three buttons: the free space went to the hole
  // instead of to the brushes.  The strip carries the stretch factor now, so
  // Colour / Ink / Size ride next to the selection arrow and everything to
  // their left belongs to the brushes.

  // --- Selection (for export-to-board) ---------------------------------------
  auto *selSep = new QFrame(bar);
  selSep->setFrameShape(QFrame::VLine);
  selSep->setFrameShadow(QFrame::Sunken);
  m_brushBarLay->addWidget(selSep);

  // Three mutually-exclusive tool toggles (plus "none" = drawing):
  //   Select    — pick panels for export
  //   Transform — rectangular region: move / copy / scale / rotate
  //   Lasso     — same transform tool, but a freehand selection
  auto *selectBtn = new QToolButton(bar);
  selectBtn->setIcon(arrowIcon());
  selectBtn->setCheckable(true);
  selectBtn->setToolTip(
      tr("Select panels (click in order) to export them as one shot.\n"
         "Click a panel again to deselect it; turn this off to deselect all."));
  m_brushBarLay->addWidget(selectBtn);

  auto *xformBtn = new QToolButton(bar);
  xformBtn->setIcon(dashedRectIcon());
  xformBtn->setCheckable(true);
  xformBtn->setToolTip(
      tr("Rectangular selection: move it, copy (Cmd/Ctrl+C, V), scale "
         "(corners)\nor rotate (top handle). Enter applies, Esc cancels, "
         "Del/Backspace erases."));
  m_brushBarLay->addWidget(xformBtn);

  auto *lassoBtn = new QToolButton(bar);
  lassoBtn->setIcon(lassoIcon());
  lassoBtn->setCheckable(true);
  lassoBtn->setToolTip(
      tr("Freehand selection, then the same move / copy / scale / rotate"));
  m_brushBarLay->addWidget(lassoBtn);

  // One shared handler keeps the three toggles exclusive and drives the canvas
  // modes from their combined state (signals blocked to avoid re-entrancy).
  auto applyTools = [this, selectBtn, xformBtn, lassoBtn](QToolButton *on) {
    QSignalBlocker b1(selectBtn), b2(xformBtn), b3(lassoBtn);
    if (on != selectBtn) selectBtn->setChecked(false);
    if (on != xformBtn) xformBtn->setChecked(false);
    if (on != lassoBtn) lassoBtn->setChecked(false);
    m_canvas->setSelectMode(selectBtn->isChecked());
    m_canvas->setTransformMode(xformBtn->isChecked() || lassoBtn->isChecked());
    m_canvas->setLassoMode(lassoBtn->isChecked());
  };
  connect(selectBtn, &QToolButton::toggled, this,
          [applyTools, selectBtn](bool on) { applyTools(on ? selectBtn : nullptr); });
  connect(xformBtn, &QToolButton::toggled, this,
          [applyTools, xformBtn](bool on) { applyTools(on ? xformBtn : nullptr); });
  connect(lassoBtn, &QToolButton::toggled, this,
          [applyTools, lassoBtn](bool on) { applyTools(on ? lassoBtn : nullptr); });

  auto *selCount = new QLabel(tr("0 sel"), bar);
  selCount->setStyleSheet("color:#e05a00;");
  m_brushBarLay->addWidget(selCount);
  connect(m_canvas, &ZtoryThumbnailCanvas::selectionChanged, this,
          [selCount](int n) { selCount->setText(tr("%1 sel").arg(n)); });

  // Delete the floating Transform selection (same as Del/Backspace, but always
  // reachable regardless of keyboard focus).
  auto *delBtn = new QToolButton(bar);
  delBtn->setIcon(createQIcon("delete"));
  delBtn->setToolTip(tr("Delete the current Transform selection"));
  connect(delBtn, &QToolButton::clicked, this,
          [this] { m_canvas->deleteFloat(); });
  m_brushBarLay->addWidget(delBtn);

  auto *mergeBtn = new QToolButton(bar);
  mergeBtn->setIcon(createQIcon("group"));
  mergeBtn->setToolTip(
      tr("Merge the selected rectangular block of panels into one panorama\n"
         "panel (or split the selected merge back into panels)"));
  connect(mergeBtn, &QToolButton::clicked, this,
          [this] { m_canvas->toggleMergeSelection(); });
  m_brushBarLay->addWidget(mergeBtn);

  // Shrink: export the shot's drawings at 1/shrink of the camera resolution per
  // side (1 = full, 2 = half each side → ¼ of the pixels, …). Lighter levels.
  auto *shrinkLabel = new QLabel(tr("Shrink"), bar);
  m_brushBarLay->addWidget(shrinkLabel);
  m_shrinkSpin = new QSpinBox(bar);
  m_shrinkSpin->setRange(1, 8);
  m_shrinkSpin->setValue(1);
  m_shrinkSpin->setToolTip(
      tr("Divide the exported drawing resolution by this factor per side\n"
         "(1 = full camera resolution, 2 = half, …)"));
  m_brushBarLay->addWidget(m_shrinkSpin);

  auto *exportBtn = new QToolButton(bar);
  exportBtn->setIcon(createQIcon("clapboard"));
  exportBtn->setToolTip(
      tr("Create a shot in the Board from the selected panels (in order)"));
  connect(exportBtn, &QToolButton::clicked, this,
          [this] { exportSelectionToBoard(); });
  m_brushBarLay->addWidget(exportBtn);

  auto *addRow = new QToolButton(bar);
  addRow->setIcon(createQIcon("add_cells"));
  addRow->setToolTip(tr("Add a row of panels to the grid"));
  connect(addRow, &QToolButton::clicked, this, [this] { m_canvas->addRow(); });
  m_brushBarLay->addWidget(addRow);

  auto *printSep = new QFrame(bar);
  printSep->setFrameShape(QFrame::VLine);
  printSep->setFrameShadow(QFrame::Sunken);
  m_brushBarLay->addWidget(printSep);

  auto *printBtn = new QToolButton(bar);
  printBtn->setIcon(createQIcon("printer"));
  printBtn->setToolTip(tr("Print the grid as an A4 PDF"));
  printBtn->setPopupMode(QToolButton::InstantPopup);
  auto *printMenu = new QMenu(printBtn);
  printMenu->addAction(
      tr("Blank sheet to draw on…"), this, [this] { printPaperSheet(false); });
  printMenu->addAction(
      tr("Sheet with the current thumbnails…"), this,
      [this] { printPaperSheet(true); });
  printBtn->setMenu(printMenu);
  m_brushBarLay->addWidget(printBtn);

  auto *importBtn = new QToolButton(bar);
  importBtn->setIcon(createQIcon("import"));
  importBtn->setToolTip(
      tr("Import a photo/scan of a printed sheet: de-warp, crop and drop the\n"
         "hand-drawn panels back into the grid"));
  connect(importBtn, &QToolButton::clicked, this,
          [this] { importPaperSheetFromFile(); });
  m_brushBarLay->addWidget(importBtn);

  auto *camBtn = new QToolButton(bar);
  camBtn->setIcon(createQIcon("camera"));
  camBtn->setToolTip(
      tr("Shoot the printed sheet with a webcam or capture card and import it"));
  connect(camBtn, &QToolButton::clicked, this,
          [this] { importPaperSheetFromCamera(); });
  m_brushBarLay->addWidget(camBtn);

  // The toolbar scrolls as one.  DvScrollWidget hands its widget the viewport
  // width only when the widget expands horizontally, otherwise it uses the
  // widget's own width and shows the arrows — so with Expanding set we get
  // BOTH behaviours: while everything fits, the layout hands the free space to
  // the brush strip; once the controls no longer fit, the width is clamped to
  // the bar's minimum, it overflows, and the arrows appear instead of the
  // widgets climbing over each other.
  bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  auto *barScroll = new DvScrollWidget(container);
  barScroll->setWidget(bar);
  root->addWidget(barScroll);
  root->addWidget(m_canvas, /*stretch=*/1);

  // Defaults: black pencil, black ink.
  if (auto *b = m_brushGroup->button(0)) b->setChecked(true);
  m_canvas->setBrushStyle(styleAt(0));
  syncSizeSliderToPreset();  // slider + "NN px" coherent from the first frame
  syncColorToPreset();
  selectColor(Qt::black);

  setWidget(container);
  setMinimumSize(360, 280);
  resize(980, 600);
}

//=============================================================================

QString ZtoryThumbnailPanel::pickBrushFile(const QString &title) {
  QString start;
  for (const TFilePath &d : TMyPaintBrushStyle::getBrushesDirs()) {
    QString r = QString::fromStdWString(d.getWideString());
    if (QFileInfo::exists(r)) { start = r; break; }
  }
  // Absolute path on purpose: resolveBrushFile() passes absolute paths through
  // unchanged, so a brush kept outside the MyPaint library works too.
  return QFileDialog::getOpenFileName(this, title, start,
                                      tr("MyPaint brushes (*.myb)"));
}

void ZtoryThumbnailPanel::showBrushContextMenu(int id, const QPoint &globalPos) {
  if (id < 0 || id >= brushCount()) return;
  QMenu menu(this);
  QAction *replace = menu.addAction(tr("Replace Brush…"));
  QAction *remove  = menu.addAction(tr("Remove Brush"));
  // The first five slots are the room's standard ones: replaceable, not
  // removable.  They are the places a storyboard artist reaches for without
  // looking, and a strip that shifts under the fingers is worse than a brush
  // never used.  Never let it go empty either.
  const bool fixedSlot = (id < kFixedBrushSlots);
  remove->setEnabled(!fixedSlot && brushCount() > 1);
  if (fixedSlot)
    remove->setToolTip(tr("One of the room's standard brushes: it can be "
                          "replaced, but not removed."));
  QAction *chosen = menu.exec(globalPos);
  if (!chosen) return;

  if (chosen == replace) {
    const QString f = pickBrushFile(tr("Replace with MyPaint brush"));
    if (f.isEmpty()) return;
    // Swap the tip, keep the slot: a fresh style from the chosen .myb, put
    // back at the same palette id so the strip order does not move.
    const std::vector<int> ids = brushStyleIds();
    auto *st = new TMyPaintBrushStyle(TFilePath(f.toStdWString()));
    st->setName(QFileInfo(f).baseName().toStdWString());
    m_brushPalette->setStyle(ids[id], st);
  } else if (chosen == remove) {
    const std::vector<int> ids = brushStyleIds();
    // TPalette has no "erase style": turning it into a plain colour takes it
    // out of the brush list (which is found by type) without disturbing the
    // ids of the others.
    m_brushPalette->setStyle(ids[id], new TSolidColorStyle(TPixel32::Black));
    if (m_currentPreset >= brushCount()) m_currentPreset = brushCount() - 1;
    if (m_currentPreset < 0) m_currentPreset = 0;
  }
  saveBrushPalette();
  rebuildBrushStrip();
  m_canvas->setBrushStyle(styleAt(m_currentPreset));
}

//=============================================================================
// Brush palette
//=============================================================================


TFilePath ZtoryThumbnailPanel::brushPalettePath() {
  // The user's palette folder — the same place Tahoma keeps <type>_default.tpl.
  // Global on purpose: a brush palette is a personal tool and follows the artist
  // between projects.
  return ToonzFolder::getMyPalettesDir() + TFilePath("ztoryc_thumbs_brushes.tpl");
}

// The brushes are the MyPaint styles of the palette, found by type rather than
// by index.  A fresh TPalette already carries two plain colour styles
// (color_0 / color_1), and a .tpl loaded from disk may hold colours next to the
// brushes — a TLV palette normally does — so counting from a fixed offset would
// be wrong in both directions.
std::vector<int> ZtoryThumbnailPanel::brushStyleIds() const {
  std::vector<int> ids;
  if (!m_brushPalette) return ids;
  for (int i = 0; i < m_brushPalette->getStyleCount(); i++)
    if (dynamic_cast<TMyPaintBrushStyle *>(m_brushPalette->getStyle(i)))
      ids.push_back(i);
  return ids;
}

int ZtoryThumbnailPanel::brushCount() const { return (int)brushStyleIds().size(); }

TMyPaintBrushStyle *ZtoryThumbnailPanel::styleAt(int i) const {
  const std::vector<int> ids = brushStyleIds();
  if (i < 0 || i >= (int)ids.size()) return nullptr;
  return dynamic_cast<TMyPaintBrushStyle *>(m_brushPalette->getStyle(ids[i]));
}

namespace {
// Build a style and bake the values that used to be applied on top of it.
TMyPaintBrushStyle *makeBrushStyle(const QString &relPath, double opacity,
                                   bool eraser, const QString &name) {
  const QString full = ZtoryThumbnailCanvas::resolveBrushFile(relPath);
  if (full.isEmpty()) return nullptr;
  auto *st = new TMyPaintBrushStyle(TFilePath(full.toStdWString()));
  // Opacity used to multiply the brush's own value, so bake the product; the
  // eraser used to be "paint white", and is a real MyPaint setting now.
  if (opacity < 1.0)
    st->setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE,
                     st->getBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE) *
                         (float)opacity);
  if (eraser) st->setBaseValue(MYPAINT_BRUSH_SETTING_ERASER, 1.0f);
  st->setName(name.toStdWString());
  return st;
}
}  // namespace

void ZtoryThumbnailPanel::seedBrushPalette() {
  m_brushPalette = new TPalette();
  struct Seed { const char *path; double opacity; bool eraser; QString name; };
  const Seed seeds[] = {
      {"classic/pencil.myb", 1.0, false, tr("Pencil")},
      {"classic/charcoal.myb", 1.0, false, tr("Brush")},
      {"deevad/airbrush.myb", 1.0, false, tr("Airbrush")},
      {"deevad/kneaded_eraser.myb", 0.3, true, tr("Kneaded eraser")},
      {"deevad/large_hard_eraser.myb", 1.0, true, tr("Eraser")},
  };
  for (const Seed &s : seeds)
    if (TMyPaintBrushStyle *st = makeBrushStyle(s.path, s.opacity, s.eraser, s.name))
      m_brushPalette->addStyle(st);
}

void ZtoryThumbnailPanel::loadBrushPalette() {
  const TFilePath fp = brushPalettePath();
  if (TSystem::doesExistFileOrLevel(fp)) {
    // Same way Tahoma reads its own <type>_default.tpl (palettecontroller.cpp):
    // StudioPalette::load() is private, and its id machinery is not wanted here.
    TIStream is(fp);
    std::string tagName;
    if (is && is.matchTag(tagName) && tagName == "palette") {
      TPalette *p = new TPalette();
      p->loadData(is);
      m_brushPalette = p;
      // A palette with no usable brush would leave the room with nothing to
      // draw with: fall back to the shipped set rather than to an empty strip.
      if (brushCount() > 0) return;
    }
  }
  seedBrushPalette();
}

void ZtoryThumbnailPanel::saveBrushPalette() const {
  if (!m_brushPalette) return;
  const TFilePath fp = brushPalettePath();
  try {
    TSystem::mkDir(fp.getParentDir());
  } catch (...) {
  }
  StudioPalette::instance()->save(fp, m_brushPalette.getPointer());
}

void ZtoryThumbnailPanel::onBrushStyleEdited() {
  // The editor wrote into the style the canvas is holding: refresh the cached
  // radius and cursor, follow the size and colour in the toolbar, redraw the
  // strip (the icon may now be a different brush) and put it on disk.
  if (TMyPaintBrushStyle *st = styleAt(m_currentPreset)) {
    m_canvas->setBrushStyle(st);
    syncSizeSliderToPreset();
    syncColorToPreset();
  }
  rebuildBrushStrip();
  saveBrushPalette();
}

void ZtoryThumbnailPanel::updateSizeValueLabel() {
  if (!m_sizeValue || !m_canvas) return;
  // The brush's real radius in pixels, not the slider position: "34 px" means
  // something to someone drawing, "62" on a 0-100 scale does not.
  m_sizeValue->setText(QString("%1 px").arg(qRound(m_canvas->brushRadiusWorld() * 2.0)));
}

void ZtoryThumbnailPanel::syncSizeSliderToPreset() {
  if (!m_sizeSlider || m_currentPreset < 0) return;
  TMyPaintBrushStyle *st = styleAt(m_currentPreset);
  if (!st) return;
  const double logR = st->getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
  const int v       = qBound(0, qRound((logR + 0.5) / 5.0 * 100.0), 100);
  // Block signals: this is the slider following the brush, not the user moving
  // it, and writing back would overwrite the very value we are restoring.
  m_sizeSlider->blockSignals(true);
  m_sizeSlider->setValue(v);
  m_sizeSlider->blockSignals(false);
  updateSizeValueLabel();
}

void ZtoryThumbnailPanel::rebuildBrushStrip() {
  if (!m_brushStripLay) return;
  // Tear down: the buttons carry their index as the button-group id, so any
  // add/remove renumbers them and rebuilding is simpler than patching.
  for (QAbstractButton *b : m_brushGroup->buttons()) {
    m_brushGroup->removeButton(b);
    m_brushStripLay->removeWidget(b);
    delete b;
  }
  while (QLayoutItem *it = m_brushStripLay->takeAt(0)) {
    if (QWidget *w = it->widget()) { w->hide(); w->deleteLater(); }
    delete it;
  }

  for (int i = 0; i < brushCount(); i++) {
    TMyPaintBrushStyle *st = styleAt(i);
    if (!st) continue;
    auto *btn = new QToolButton(m_brushStrip);
    btn->setCheckable(true);
    btn->setIconSize(QSize(28, 28));
    const QString path = QString::fromStdWString(st->getPath().getWideString());
    QString name       = QString::fromStdWString(st->getName());
    if (name.isEmpty()) name = QFileInfo(path).baseName();
    QIcon ic = brushIcon(path);
    if (ic.isNull())
      btn->setText(name.left(3));  // no preview available
    else
      btn->setIcon(ic);
    const bool erases =
        st->getBaseValue(MYPAINT_BRUSH_SETTING_ERASER) > 0.5f;
    btn->setToolTip(erases ? tr("%1 (eraser)").arg(name) : name);
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QWidget::customContextMenuRequested, this,
            [this, i, btn](const QPoint &pos) {
              showBrushContextMenu(i, btn->mapToGlobal(pos));
            });
    m_brushGroup->addButton(btn, i);
    m_brushStripLay->addWidget(btn);
    if (i == m_currentPreset) btn->setChecked(true);
  }

  // "+" lives at the end of the strip, so it scrolls with the brushes instead
  // of being pushed off by them.
  auto *addBrush = new QToolButton(m_brushStrip);
  addBrush->setText("+");
  addBrush->setToolTip(tr("Add a brush from the library…"));
  connect(addBrush, &QToolButton::clicked, this, [this] {
    const QString f = pickBrushFile(tr("Add MyPaint brush"));
    if (f.isEmpty()) return;
    auto *st = new TMyPaintBrushStyle(TFilePath(f.toStdWString()));
    st->setName(QFileInfo(f).baseName().toStdWString());
    m_brushPalette->addStyle(st);
    m_currentPreset = brushCount() - 1;
    saveBrushPalette();
    rebuildBrushStrip();
    m_canvas->setBrushStyle(styleAt(m_currentPreset));
  });
  m_brushStripLay->addWidget(addBrush);

  m_brushStripLay->addStretch(1);
}

void ZtoryThumbnailPanel::selectColor(const QColor &c) {
  const TPixel32 ink(c.red(), c.green(), c.blue(), 255);
  m_canvas->setColor(ink);
  // The colour belongs to the brush, like its size: picking blue for the
  // pencil should not turn the charcoal blue too.  TColorStyle carries a
  // colour and saveData() already writes it, so this rides along in the .tpl
  // with no extra storage.
  if (TMyPaintBrushStyle *st = styleAt(m_currentPreset)) {
    st->setMainColor(ink);
    saveBrushPalette();
  }
  if (m_swatch) m_swatch->setIcon(activeSwatchIcon(c));
}

// Bring the toolbar in line with the brush that was just picked.
void ZtoryThumbnailPanel::syncColorToPreset() {
  TMyPaintBrushStyle *st = styleAt(m_currentPreset);
  if (!st) return;
  const TPixel32 ink = st->getMainColor();
  m_canvas->setColor(ink);
  if (m_swatch)
    m_swatch->setIcon(activeSwatchIcon(QColor(ink.r, ink.g, ink.b)));
}

void ZtoryThumbnailPanel::exportSelectionToBoard() {
  const QVector<int> sel = m_canvas->selection();
  if (sel.isEmpty()) {
    DVGui::warning(tr("Select one or more panels first (use Select mode)."));
    return;
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  // The shot's drawings are framed at the scene camera resolution, optionally
  // shrunk by an integer factor per side (1 = full) to keep the levels light.
  const int shrink     = m_shrinkSpin ? m_shrinkSpin->value() : 1;
  const TDimension cam = ZtoryShotOps::cameraRes(scene);

  // All selected panels become ONE shot on ONE level.  A level has a single
  // resolution, so we size it to the LARGEST selected panel (a merged panorama
  // spans N×M boxes → N·camW × M·camH) and composite every panel, centred, onto
  // a white frame of that size.  Single panels thus sit centred in the larger
  // canvas; the panorama fills it.
  struct Item {
    TRaster32P ras;
    TDimension nat;
  };
  std::vector<Item> items;
  int maxW = 1, maxH = 1;
  for (int idx : sel) {
    const QSize span = m_canvas->panelSpan(idx);
    const TDimension nat(qMax(1, span.width() * cam.lx / shrink),
                         qMax(1, span.height() * cam.ly / shrink));
    TRaster32P r = m_canvas->panelRaster(idx, nat);
    if (!r) continue;
    items.push_back({r, nat});
    maxW = std::max(maxW, nat.lx);
    maxH = std::max(maxH, nat.ly);
  }
  if (items.empty()) return;

  std::vector<TRaster32P> frames;
  frames.reserve(items.size());
  for (const Item &it : items) {
    if (it.nat.lx == maxW && it.nat.ly == maxH) {
      frames.push_back(it.ras);  // already the full canvas
      continue;
    }
    TRaster32P frame(maxW, maxH);
    frame->fill(TPixel32::White);
    frame->copy(it.ras, TPoint((maxW - it.nat.lx) / 2, (maxH - it.nat.ly) / 2));
    frames.push_back(frame);
  }

  // Register the undo the same way every other shot-creating command does.
  // Without this the export left NOTHING on the undo stack: Cmd+Z could not
  // remove a shot exported by mistake, and — worse — it silently undid whatever
  // came before instead (the last brush stroke, the last Board edit).
  StoryboardPanel *board = ztoryFindBoardPanel();
  if (board) board->beginExternalEdit();
  ZtoryModel::instance()->addShotFromRasters(QString(), frames);
  if (board) board->endExternalEdit(tr("Export Panels to Board"));
  m_canvas->clearSelection();
  DVGui::info(tr("Exported %1 panel(s) to the Board as one shot.")
                  .arg((int)frames.size()));
}

void ZtoryThumbnailPanel::printPaperSheet(bool withContent) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  ZtoryPaperSheet::SheetParams p;
  p.gridCols     = m_canvas->gridCols();
  p.gridRows     = m_canvas->gridRows();
  p.cameraAspect = ZtoryShotOps::cameraAspect(scene);
  if (withContent) p.content = m_canvas->canvasImage();

  // Default filename + folder from the scene path (mirrors the Board PDF export).
  QString sceneName = "storyboard", dir;
  if (scene) {
    TFilePath sp = scene->getScenePath();
    QString wn   = QString::fromStdWString(sp.getWideName());
    if (!wn.isEmpty()) sceneName = wn;
    dir = QString::fromStdWString(sp.getParentDir().getWideString());
  }
  p.sceneName = sceneName;

  const QString defaultPath = (dir.isEmpty() ? QDir::homePath() : dir) + "/" +
                              sceneName +
                              (withContent ? "_thumbs.pdf" : "_sheet.pdf");
  QString path = QFileDialog::getSaveFileName(
      this, tr("Print Thumbnail Sheet"), defaultPath, tr("PDF (*.pdf)"));
  if (path.isEmpty()) return;

  if (ZtoryPaperSheet::printSheet(path, p))
    DVGui::info(tr("Printed thumbnail sheet: %1").arg(path));
  else
    DVGui::warning(tr("Could not write the PDF: %1").arg(path));
}

int ZtoryThumbnailPanel::importOneSheet(const QImage &photo,
                                        const QString &label,
                                        QStringList &failed, int &faint) {
  ZtoryPaperSheet::ImportResult r = ZtoryPaperSheet::importSheet(photo);
  if (!r.ok) {
    failed << QString("%1: %2").arg(label, r.error);
    return 0;
  }
  // Columns are never split across pages, so the sheet must have been printed
  // for this room's grid width. Rows, instead, simply grow.
  if (r.gridCols != m_canvas->gridCols()) {
    failed << tr("%1: printed for a %2-column grid, the room has %3")
                  .arg(label)
                  .arg(r.gridCols)
                  .arg(m_canvas->gridCols());
    return 0;
  }

  // Placement is by CAPTURE / SCANNING ORDER, not by the page number printed on
  // the sheet: blank sheets are meant to be photocopied, so every copy carries
  // the same page code. Each sheet lands on the rows after whatever is drawn.
  const int baseRow = m_canvas->lastNonEmptyRow() + 1;

  std::vector<ZtoryThumbnailCanvas::ImportedBlit> blits;
  int maxRow = -1, rowsOnSheet = 0;
  for (const ZtoryPaperSheet::ImportedCell &c : r.cells) {
    rowsOnSheet = std::max(rowsOnSheet, c.gridRow - r.startRow + 1);
    if (c.faint) ++faint;
    if (c.empty) continue;  // blank cells never overwrite what is there
    const int row = baseRow + (c.gridRow - r.startRow);
    blits.push_back({row, c.gridCol, c.image});
    maxRow = std::max(maxRow, row);
  }
  if (blits.empty()) {
    failed << tr("%1: no hand-drawn panels found").arg(label);
    return 0;
  }

  // Keep one sheet's worth of empty rows ready for the next page.
  const int ensureRows = maxRow + 1 + std::max(1, rowsOnSheet);
  m_canvas->applyImportedCells(blits, ensureRows);
  // Scroll to what was just imported: on a grid that already had drawings the
  // new sheet lands below the fold, and the import would look like a no-op.
  m_canvas->revealRow(baseRow);
  return (int)blits.size();
}

void ZtoryThumbnailPanel::importPaperSheetFromFile() {
  // Several sheets at once: they are imported in the order picked, which is the
  // order they land in the grid.
  QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Import Sheet Photos"), QDir::homePath(),
      tr("Images (*.jpg *.jpeg *.png *.bmp *.tif *.tiff)"));
  if (paths.isEmpty()) return;
  paths.sort();  // scanner output is usually numbered — keep that order

  int totalPanels = 0, totalSheets = 0, totalFaint = 0;
  QStringList failed;
  for (const QString &path : paths) {
    QImage photo(path);
    const QString label = QFileInfo(path).fileName();
    if (photo.isNull()) {
      failed << tr("%1: not a readable image").arg(label);
      continue;
    }
    const int n = importOneSheet(photo, label, failed, totalFaint);
    if (n > 0) {
      totalPanels += n;
      ++totalSheets;
    }
  }

  if (totalSheets > 0) {
    QString msg = tr("Imported %1 sheet(s), %2 panel(s).")
                      .arg(totalSheets)
                      .arg(totalPanels);
    if (totalFaint > 0)
      msg += "\n" + tr("%1 panel(s) were skipped as blank but do carry very "
                       "light marks — draw them darker and shoot again.")
                        .arg(totalFaint);
    DVGui::info(msg);
  }
  if (!failed.isEmpty())
    DVGui::warning(tr("Not imported:\n%1").arg(failed.join("\n")));
}

void ZtoryThumbnailPanel::importPaperSheetFromCamera() {
  ZtoryPaperCaptureDialog dlg(this);
  if (dlg.exec() != QDialog::Accepted) return;

  const QList<QImage> shots = dlg.captured();
  if (shots.isEmpty()) {
    DVGui::info(tr("No sheet was captured: press “Capture sheet” while the "
                   "outline is green, then import."));
    return;
  }

  int totalPanels = 0, totalSheets = 0, totalFaint = 0;
  QStringList failed;
  for (int i = 0; i < shots.size(); ++i) {
    const int n = importOneSheet(shots.at(i), tr("Capture %1").arg(i + 1),
                                 failed, totalFaint);
    if (n > 0) {
      totalPanels += n;
      ++totalSheets;
    }
  }

  if (totalSheets > 0) {
    QString msg = tr("Imported %1 sheet(s), %2 panel(s).")
                      .arg(totalSheets)
                      .arg(totalPanels);
    if (totalFaint > 0)
      msg += "\n" + tr("%1 panel(s) were skipped as blank but do carry very "
                       "light marks — draw them darker and shoot again.")
                        .arg(totalFaint);
    DVGui::info(msg);
  }
  if (!failed.isEmpty())
    DVGui::warning(tr("Not imported:\n%1").arg(failed.join("\n")));
}

//=============================================================================
// Factory
//=============================================================================

class ZtoryThumbnailPanelFactory final : public TPanelFactory {
public:
  ZtoryThumbnailPanelFactory() : TPanelFactory("ZtoryThumbnailPanel") {}

  TPanel *createPanel(QWidget *parent) override {
    auto *panel = new ZtoryThumbnailPanel(parent);
    panel->setObjectName("ZtoryThumbnailPanel");
    panel->setWindowTitle("Ztoryc Thumbnails");
    return panel;
  }

  void initialize(TPanel *) override { assert(0); }

} ztoryThumbnailPanelFactory;
