#pragma once

// ZtoryThumbnailPanel — Thumbnail room: a palette toolbar over a grid sketch
// canvas (ZtoryThumbnailCanvas). See ztorythumbnailcanvas.h for the drawing
// architecture (custom raster surface + real MyPaint brushes).
//
// This panel must NOT embed a Tahoma drawing viewer: a ComboViewerPanel is
// itself a TPanel, and nesting TPanels breaks Tahoma's active-viewer routing.

#include "pane.h"

#include <QWidget>
#include <QList>
#include <vector>
#include <QString>

#include "ztorythumbnailcanvas.h"
#include "tpalette.h"          // TPaletteP — the brush palette
#include "toonz/tpalettehandle.h"
#include "tfilepath.h"

class QToolButton;
class QButtonGroup;
class QHBoxLayout;
class QSpinBox;
class QCheckBox;
class QSlider;
class QDialog;
class QLabel;
class QTimer;

class ZtoryThumbnailPanel final : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryThumbnailPanel(QWidget *parent = nullptr);
  ~ZtoryThumbnailPanel() override;

private:
  void selectColor(const QColor &c);  // set canvas ink + update swatch
  // Build one raster per selected panel (at scene camera res) and hand them to
  // ZtoryModel as a new multi-panel shot, then clear the selection.
  void exportSelectionToBoard();
  // Print the canvas grid to a multi-page A4 PDF (task 63). `withContent` prints
  // the thumbnails currently on the canvas (a contact sheet); otherwise a blank
  // grid to draw on by hand — the one meant to be photocopied and re-imported.
  void printPaperSheet(bool withContent);
  // Import a photographed/scanned sheet from an image file: de-warp, crop and
  // blit the cells back into the grid (task 63, phase 2).
  void importPaperSheetFromFile();
  // Same, shooting the sheet with a webcam / capture card from inside Ztoryc.
  void importPaperSheetFromCamera();
  // Run one photographed sheet through the pipeline and drop it into the grid
  // after whatever is already drawn. Shared by the file and camera paths, so
  // both behave identically. Returns the number of panels placed (0 on failure,
  // with the reason appended to `failed`).
  // `faint` accumulates cells that were skipped as blank but did carry very
  // light marks — a too-light sketch would otherwise vanish without a word.
  // `digital` conta i fogli riconosciuti come PAGINE DIGITALI (gia' dritte:
  // non fotografate), che entrano a colori. Contarli serve a dirlo all'utente:
  // il programma decide da solo, e una decisione presa da sola va detta.
  int importOneSheet(const QImage &photo, const QString &label,
                     QStringList &failed, int &faint, int &digital);

  // The brush strip is its own scrolling row, separate from the rest of the
  // toolbar: the buttons used to share one QHBoxLayout with the colour chips,
  // the size slider and the export controls, so every brush added squeezed
  // those. In a DvScrollWidget (the same overflow widget the Board toolbar
  // uses) the row scrolls instead, and "how many brushes fit" stops being a
  // question.
  void rebuildBrushStrip();
  // Right-click on a brush: replace it with another .myb, or drop it.
  void showBrushContextMenu(int id, const QPoint &globalPos);

  ZtoryThumbnailCanvas *m_canvas = nullptr;
  QButtonGroup *m_brushGroup     = nullptr;
  QHBoxLayout *m_brushBarLay     = nullptr;
  // Show the live brush radius in px next to the slider, and move the slider to
  // the active brush when it changes.
  void updateSizeValueLabel();
  void syncSizeSliderToPreset();
  void syncColorToPreset();

  QSlider *m_sizeSlider          = nullptr;
  QLabel *m_sizeValue            = nullptr;
  QWidget *m_brushStrip          = nullptr;  // holds only the brush buttons
  QHBoxLayout *m_brushStripLay   = nullptr;
  int m_currentPreset            = 0;
  QToolButton *m_swatch          = nullptr;  // shows / picks current colour
  QSpinBox *m_shrinkSpin         = nullptr;  // export resolution divisor (1 = full)
  QCheckBox *m_exportTransparent = nullptr;  // export with alpha, not on white
  // THE brush palette.  Not a list of presets any more: a real TPalette of
  // TMyPaintBrushStyle, which is what lets it be saved as a .tpl — names,
  // customised parameters and input curves included — reloaded next time, and
  // one day handed to the Style Editor for editing.
  TPaletteP m_brushPalette;
  // Our own handle on that palette.  The Style Editor is built against a
  // PaletteHandle, so giving it OURS means we never touch the shared one the
  // rest of the application edits — no hijacking, nothing to put back when the
  // user leaves the room, and no way to break drawing in the other rooms.
  TPaletteHandle *m_brushHandle = nullptr;

  void onBrushStyleEdited();  // the Style Editor changed the current brush
  void openBrushEditor();     // browse + customise, in our own Style Editor
  // Double-click on a brush button reopens the editor on it.
  bool eventFilter(QObject *watched, QEvent *e) override;
  QDialog *m_brushEditor = nullptr;  // created on first use, then reused

  // Seed the five brushes the room ships with, baking their opacity and eraser
  // role into the style so nothing has to be applied on top later.
  void seedBrushPalette();
  void loadBrushPalette();
  void saveBrushPalette() const;
  // Dragging the size slider used to write the whole .tpl on every step, on the
  // UI thread — the same mistake the canvas save had, in miniature.  Coalesce
  // instead: the last change in a burst wins, and the destructor flushes a
  // pending one so nothing is lost by leaving the room.
  void scheduleBrushPaletteSave();
  QTimer *m_paletteSaveTimer = nullptr;
  // mutable: saveBrushPalette() is const, and "already told the user" is
  // bookkeeping about the warning, not about the palette.
  mutable bool m_paletteSaveFailed = false;
  static TFilePath brushPalettePath();
  std::vector<int> brushStyleIds() const;  // palette ids of the MyPaint styles
  TMyPaintBrushStyle *styleAt(int i) const;
  int brushCount() const;
};
