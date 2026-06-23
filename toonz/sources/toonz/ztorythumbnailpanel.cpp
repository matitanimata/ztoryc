#include "ztorythumbnailpanel.h"

#include "toonzqt/menubarcommand.h"
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
#include <QLabel>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>

namespace {

// Icon for a brush = its MyPaint preview PNG ("<brush>_prev.png" next to .myb).
QIcon brushIcon(const QString &relPath) {
  QString abs = ZtoryThumbnailCanvas::resolveBrushFile(relPath);
  if (abs.isEmpty()) return QIcon();
  QString prev = abs;
  prev.replace(QRegExp("\\.myb$"), "_prev.png");
  return QFileInfo::exists(prev) ? QIcon(prev) : QIcon();
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

  auto *selectBtn = new QToolButton(bar);
  selectBtn->setText(tr("Select"));
  selectBtn->setCheckable(true);
  selectBtn->setToolTip(
      tr("Select panels (click in order) to export them as one shot"));
  connect(selectBtn, &QToolButton::toggled, this,
          [this](bool on) { m_canvas->setSelectMode(on); });
  m_brushBarLay->addWidget(selectBtn);

  auto *selCount = new QLabel(tr("0 sel"), bar);
  selCount->setStyleSheet("color:#e05a00;");
  m_brushBarLay->addWidget(selCount);

  auto *clearSel = new QToolButton(bar);
  clearSel->setText(tr("Clear"));
  clearSel->setToolTip(tr("Clear the panel selection"));
  connect(clearSel, &QToolButton::clicked, this,
          [this] { m_canvas->clearSelection(); });
  m_brushBarLay->addWidget(clearSel);

  connect(m_canvas, &ZtoryThumbnailCanvas::selectionChanged, this,
          [selCount](int n) { selCount->setText(tr("%1 sel").arg(n)); });

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
  exportBtn->setText(tr("Export to Board"));
  exportBtn->setStyleSheet("font-weight:bold;");
  exportBtn->setToolTip(
      tr("Create a shot in the Board from the selected panels (in order)"));
  connect(exportBtn, &QToolButton::clicked, this,
          [this] { exportSelectionToBoard(); });
  m_brushBarLay->addWidget(exportBtn);

  auto *addRow = new QToolButton(bar);
  addRow->setText(tr("+ Row"));
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
  const TDimension res(qMax(1, cam.lx / shrink), qMax(1, cam.ly / shrink));

  std::vector<TRaster32P> panels;
  panels.reserve(sel.size());
  for (int idx : sel) {
    TRaster32P r = m_canvas->panelRaster(idx, res);
    if (r) panels.push_back(r);
  }
  if (panels.empty()) return;

  // Name is left empty: ZtoryModel assigns the proper SH label on resequence.
  ZtoryModel::instance()->addShotFromRasters(QString(), panels);
  m_canvas->clearSelection();
  DVGui::info(tr("Exported %1 panel(s) to the Board as one shot.")
                  .arg((int)panels.size()));
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
