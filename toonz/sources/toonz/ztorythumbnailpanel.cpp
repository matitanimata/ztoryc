#include "ztorythumbnailpanel.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"  // createQIcon (native toolbar icons)
#include "menubarcommandids.h"
#include "toonz/mypaintbrushstyle.h"  // getBrushesDirs()

#include "ztorymodel.h"    // addShotFromRasters
#include "ztoryshotops.h"  // cameraRes
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "tmsgcore.h"  // DVGui::info / warning

#include <QWidget>
#include <QRegExp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
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

#include <algorithm>

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
            if (id >= 0 && id < m_presets.size())
              m_canvas->setPreset(m_presets[id]);
          });

  // Default brushes (icons come from each brush's MyPaint preview).
  addBrushButton("classic/pencil.myb", 1.0, false, tr("Pencil"));
  addBrushButton("classic/charcoal.myb", 1.0, false, tr("Brush"));
  addBrushButton("deevad/airbrush.myb", 1.0, false, tr("Airbrush"));
  addBrushButton("deevad/kneaded_eraser.myb", 0.3, true,
                 tr("Kneaded eraser (lightens gradually)"));
  addBrushButton("deevad/large_hard_eraser.myb", 1.0, true, tr("Eraser"));

  // "+" add a brush from the library.
  auto *addBrush = new QToolButton(bar);
  addBrush->setText("+");
  addBrush->setToolTip(tr("Add a brush from the library…"));
  connect(addBrush, &QToolButton::clicked, this, [this] {
    QString start;
    for (const TFilePath &d : TMyPaintBrushStyle::getBrushesDirs()) {
      QString r = QString::fromStdWString(d.getWideString());
      if (QFileInfo::exists(r)) { start = r; break; }
    }
    QString f = QFileDialog::getOpenFileName(this, tr("Add MyPaint brush"), start,
                                             tr("MyPaint brushes (*.myb)"));
    if (f.isEmpty()) return;
    // Store with the absolute path; resolveBrushFile passes absolute paths
    // through unchanged so this works for brushes outside the library too.
    auto *btn = addBrushButton(f, 1.0, false, QFileInfo(f).baseName());
    if (btn) btn->click();
  });
  m_brushBarLay->addWidget(addBrush);

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
  connect(size, &QSlider::valueChanged, this,
          [this](int v) { m_canvas->setSizeModifier(-2.0 + 6.0 * (v / 100.0)); });
  m_brushBarLay->addWidget(size);

  m_brushBarLay->addStretch(1);

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

  root->addWidget(bar);
  root->addWidget(m_canvas, /*stretch=*/1);

  // Defaults: black pencil, black ink.
  if (auto *b = m_brushGroup->button(0)) b->setChecked(true);
  m_canvas->setPreset(m_presets[0]);
  selectColor(Qt::black);

  setWidget(container);
  setMinimumSize(360, 280);
  resize(980, 600);
}

//=============================================================================

QToolButton *ZtoryThumbnailPanel::addBrushButton(const QString &relPath,
                                                 double opacity, bool eraser,
                                                 const QString &tip) {
  auto *btn = new QToolButton(this);
  btn->setCheckable(true);
  btn->setIconSize(QSize(28, 28));
  QIcon ic = brushIcon(relPath);
  if (ic.isNull())
    btn->setText(tip.left(3));  // fallback when no preview is available
  else
    btn->setIcon(ic);
  btn->setToolTip(tip);

  const int id = m_presets.size();
  m_presets.append({relPath, opacity, eraser});
  m_brushGroup->addButton(btn, id);
  // Insert before the trailing "+"/spacers if already built, else just append.
  m_brushBarLay->insertWidget(id, btn);
  return btn;
}

void ZtoryThumbnailPanel::selectColor(const QColor &c) {
  m_canvas->setColor(TPixel32(c.red(), c.green(), c.blue(), 255));
  if (m_swatch) m_swatch->setIcon(activeSwatchIcon(c));
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

  ZtoryModel::instance()->addShotFromRasters(QString(), frames);
  m_canvas->clearSelection();
  DVGui::info(tr("Exported %1 panel(s) to the Board as one shot.")
                  .arg((int)frames.size()));
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
