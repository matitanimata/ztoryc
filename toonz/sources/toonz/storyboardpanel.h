#pragma once
#ifndef STORYBOARDPANEL_H
#define STORYBOARDPANEL_H

#include "pane.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshchildlevel.h"
#include "toonzqt/icongenerator.h"
#include "toonz/childstack.h"
#include "viewerpane.h"

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QResizeEvent>
#include <QPixmap>
#include <QLineEdit>
#include <QComboBox>
#include <QStackedWidget>

#include "ztorymodel.h"
#include "ztoryundo.h"
#include <set>
#include <utility>

class PanelWidget final : public QFrame {
  Q_OBJECT
  int  m_shotIndex;
  int  m_panelIndex;
  int  m_panelCount;
  int  m_fps;
  bool m_selected;
  QLineEdit   *m_seqField;       // editable sequence label (shows number part only)
  QLabel      *m_seqLabel;       // "SQ:" header label — hidden in Simple mode
  QLineEdit   *m_shotLabel;      // editable shot label (shows number part only)
  QString      m_storedShotPrefix;  // e.g. "SH" — restored when shotNumber() is read
  QString      m_storedSeqPrefix;   // e.g. "SQ" — restored on seqLabelEdited
  QLabel      *m_panelLabel;
  QPushButton *m_prevPanelBtn = nullptr;  // ◀ navigate panels in collapsed view
  QPushButton *m_nextPanelBtn = nullptr;  // ▶
  QLabel      *m_durationLabel;
  QLabel      *m_totalLabel;
  QSpinBox    *m_totalSpin;
  QSpinBox    *m_durationSpin;
  QPushButton *m_matchButton;
  QLabel      *m_previewLabel;
  QPixmap      m_previewPixmap;
  QTextEdit   *m_dialogField;
  QTextEdit   *m_actionField;
  QTextEdit   *m_notesField;
  QLabel* makeFieldLabel(const QString &text);
  QString framesToTimecode(int frames) const;
  void    updateBorderStyle();
  // ── Light-direction gizmo editing (task 40 FASE 3) ────────────────────────
  // Edit mode + colour are shared by all panels and driven by the Board
  // toolbar; the drag-in-progress points are per-widget (normalized 0-1
  // over the preview area).
  static bool    s_lightEditMode;
  static QString s_lightColor;
  bool    m_lightDragging = false;
  QPointF m_lightDragTail, m_lightDragTip;
  double  m_lightDragDepth = 0.0;   // mouse wheel during drag: -1..+1
  // Beam opening angle (degrees, Shift+wheel). Kept across drags so a chosen
  // softness applies to subsequent arrows too.
  double  m_lightDragSpread = 35.0;
  QPointF normalizedPreviewPos(const QPoint &pos) const;
public:
  static void setLightEditMode(bool on)        { s_lightEditMode = on; }
  static void setLightColor(const QString &c)  { s_lightColor = c; }
  explicit PanelWidget(QWidget *parent = nullptr);
  void setShotIndex(int si);
  void setPanelIndex(int pi, int total);
  void setFps(int fps);
  void setDuration(int frames);
  void setTotalDuration(int frames);
  void setPreviewPixmap(const QPixmap &px);
  void setSelected(bool sel);
  void setDialog(const QString &t);
  void setAction(const QString &t);
  void setNotes(const QString &t);
  void setShotNumber(const QString &n);
  void setSeqLabel(const QString &seq);
  void setSeqVisible(bool visible);
  // Collapsed Board view ("one card per shot"): show ◀ ▶ to flip through the
  // shot's panels in place.  total<=1 hides the arrows.
  void setPanelNavigator(bool enabled, int total);
  void rescalePreview();
  int     shotIndex()  const { return m_shotIndex; }
  int     panelIndex() const { return m_panelIndex; }
  int     duration()   const;
  QString dialog()     const;
  QString action()     const;
  QString notes()      const;
  const QPixmap &previewPixmap() const { return m_previewPixmap; }
  bool    isSelected() const { return m_selected; }
  // Returns the full shot label (prefix + number), e.g. "SH020".
  // For Sequence-style combined labels, returns the SH part only.
  QString shotNumber() const;
  QList<QTextEdit *> textFields() const { return {m_dialogField, m_actionField, m_notesField}; }
signals:
  void totalDurationChanged(int frames);
  void editRequested(int shotIndex);
  void matchDurationRequested(int shotIndex);
  void durationChanged(int shotIndex, int panelIndex, int frames);
  void dataChanged(int shotIndex, int panelIndex);
  // Emitted when the user edits the SQ field; fullLabel is the reconstructed
  // sequence label including prefix, e.g. "SQ020".
  void seqLabelEdited(int shotIndex, QString fullLabel);
  void clicked(int shotIndex, int panelIndex, Qt::KeyboardModifiers modifiers);
  // Collapsed view: user clicked ◀ (delta=-1) or ▶ (delta=+1) to change which
  // panel of this shot is shown.
  void panelNavRequested(int shotIndex, int delta);
  void dropReceived(int fromShot, int toShot);
  // Emitted from resizeEvent when the stored pixmap is too small to fill the
  // panel at native (HiDPI) resolution — StoryboardPanel re-renders on receipt.
  void previewRerenderNeeded(int shotIdx, int panelIdx);
  // Light-direction gizmo placed by drag (coords normalized 0-1, depth from
  // mouse wheel) or removal requested by right-click in light edit mode.
  void lightPlaced(int shotIndex, int panelIndex,
                   double tailX, double tailY, double tipX, double tipY,
                   double depth, double spread);
  void lightRemoveRequested(int shotIndex, int panelIndex);
protected:
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void enterEvent(QEvent *e) override;
  void leaveEvent(QEvent *e) override;
private slots:
  void onDurationSpinChanged(int value);
};

class StoryboardPanel final : public TPanel {
  Q_OBJECT
  QScrollArea *m_scrollArea;
  QWidget     *m_container;
  QGridLayout *m_grid;
  QToolButton *m_addShotButton;
  QToolButton *m_deleteButton;
  QToolButton *m_mergeButton;
  QToolButton *m_copyButton;
  QToolButton *m_cloneButton;
  QToolButton *m_pasteButton;
QToolButton *m_numberingBtn;   // ⚙ Numbering config button
  QTimer      *m_panelDetectTimer;
  QToolButton *m_exportPdfButton;
  QToolButton *m_exportSpreadsheetButton;
  QToolButton *m_techniqueButton;
  QToolButton *m_exportShotsButton;
  QToolButton *m_exportAnimaticButton;
  QToolButton *m_camLabelButton;   // toggle camera-move type labels on thumbnails
  bool         m_showCamMoveType = true;  // persisted in QSettings
  // Light-direction gizmo (task 40 FASE 3)
  QToolButton *m_lightEditButton  = nullptr; // checkable: drag-to-place mode
  QToolButton *m_lightShowButton  = nullptr; // checkable: visibility (shortcut L)
  QToolButton *m_lightColorButton = nullptr; // colour swatch → QColorDialog
  bool         m_showLights = true; // persisted in QSettings
  QSpinBox    *m_columnsPerRowSpin;
  // "Compact view": one card per shot (show only the current panel), with
  // ◀ ▶ to navigate panels in place.  Useful for animated scenes that generate
  // one panel per keyframe/frame.  Persisted in QSettings.
  QToolButton *m_collapsePanelsButton = nullptr;
  bool         m_collapsePanels = false;
  QComboBox   *m_numberingCombo;
  QStackedWidget   *m_stack;
  SceneViewerPanel *m_comboViewer;
struct Shot {
    ShotData               data;
    std::vector<PanelWidget*> panels;
    bool selected = false;
    int  viewPanel = 0;  // panel shown in collapsed "Compact view" view
  };
  std::vector<Shot> m_shots;
  int  m_columnsPerRow;
  int              m_selectedShotIndex;
  std::set<int>     m_selectedIndices;

  bool m_updating = false;   // re-entrancy guard for signal reactions
  // Shot clipboard lives in ZtoryModel::sharedClip() — single source of truth
  // shared with the Animatic. The Board no longer keeps a local copy.
  int  m_fps;
  bool m_autoRenumber;
  // True while loadZtoryc() runs — suppresses the scriptFileChanged→saveZtoryc
  // re-save that would otherwise fire when loadZtoryc() publishes the
  // screenplay path it just read.
  bool m_loadingZtoryc = false;
  // The .ztoryc path that corresponds to the current m_shots data.
  // Set at the end of refreshFromScene() (after loadZtoryc completes) and
  // cleared by clearShots(). saveZtoryc() uses this instead of ztoryPath() so
  // that a stale-m_shots / new-scene-path mismatch can never corrupt files:
  // while m_shots is empty (being rebuilt) this is empty → saveZtoryc() returns
  // early.  This eliminates the cross-scene text contamination bug.
  QString m_currentZtoryPath;
  // Column index of the last shot edited while inside a sub-scene.
  // Set when xsheetChanged fires inside a sub-scene; used in showEvent to
  // invalidate that shot's stale thumbnail so it gets re-rendered on Board show.
  int  m_dirtyShotCol = -1;
  // Deferred re-render queue: panels that requested a higher-res thumbnail
  // because the window was resized. Fired 200 ms after the last resize event.
  QTimer                        *m_rerenderTimer = nullptr;
  std::set<std::pair<int,int>>   m_rerenderQueue;
  // Debounce timer for window-resize → column-width recalculation.
  QTimer                        *m_resizeTimer   = nullptr;
  // Coalescing undo for duration spin changes: captures "before" on first change,
  // then commits one UndoBoardState after 600ms of inactivity.
  std::vector<ZtoryShotSnap> m_pendingDurationBefore;
  QTimer                    *m_durationCommitTimer = nullptr;
  void addPanelWidget(int shotIdx, int panelIdx);
  void connectPanelWidget(PanelWidget *pw);
  void renumberAll();
  // Generate shotLabel for shots[si] using the Board's own m_shots as context
  void assignBoardShotLabel(int si);
  void resequenceXsheet();
  void clearShots();
  void updatePreview(int shotIdx, int panelIdx);
  void updateVisiblePreviews();
  void rebuildGrid();
  void selectShot(int shotIdx);
  // Mirror the shared selection (set by the Animatic timeline) onto the board
  // grid, highlighting the matching shots — without writing back to the model.
  void applySharedSelection();
  int  currentShotIndex() const;
  void detectAndUpdatePanels(int shotIdx);
  void assignKeepNumbers(int insertAt);
  QString ztoryPath() const;
  void    syncWidgetsToData();
  void    updateColumnName(int si);
  void    loadZtoryc();
public:
  explicit StoryboardPanel(QWidget *parent = nullptr);
  void    saveZtoryc();
  void refreshFromScene();
  std::vector<ZtoryShotSnap> captureSnapshot();
  void restoreFromSnapshot(const std::vector<ZtoryShotSnap> &snap);
  // Global "New Shot After Current" command (MI_ZtoryNewShotAfter): adds a
  // shot after the selected/open one and, if invoked from inside a sub-scene,
  // enters the new shot directly so drawing can continue without leaving
  // the SHOTEDITOR room.
  void newShotAfterCurrent();
  // Export entry points — also invoked from the File ▸ Export ▸ Ztoryc menu
  // commands, so they must be public.
  void onExportPdf();
  void onExportSpreadsheet();
  void onExportSpreadsheetCsv();
  void onExportShots();
  void onExportAnimatic();
  // Storyboard Settings dialog (production/episode/title/technique/numbering).
  void onStoryboardSettings();
  // Toolbar dedup (Board↔Animatic): hide the shared shot-op buttons
  // (add/delete/merge/copy/clone/paste) when an Animatic panel — the owner of
  // shot ops — lives in the same room. Pure setVisible, no layout rebuild; the
  // Board keeps a complete toolbar in custom rooms that have no Animatic.
  // Driven by MainWindow (room composition is authoritative there), never from
  // showEvent (perturbs detectAndUpdatePanels timing).
  void setShotButtonsHidden(bool hidden);
  // Export entry points. Exports live on the Board; the Animatic toolbar hosts
  // duplicate "Export Shots/Scene" and "Export Animatic" buttons (the Board has
  // little room, the timeline has plenty) that delegate here on the sibling
  // Board. Export logic reads only m_shots + the current scene, so calling it
  // from the Animatic is safe (it never touches the Board↔Animatic sync path).
  void exportShotsCmd() { onExportShots(); }
  void exportAnimaticCmd() { onExportAnimatic(); }
protected:
  void showEvent(QShowEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *e) override;
private slots:
  void onAddShot();
  void onDeleteShot();
  void onCutShot();
  void onCloneShot();
  void onCopyShot();
  void onPasteShot();
protected:
  void keyPressEvent(QKeyEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void onEditShot(int shotIdx);
  void onPanelClicked(int shotIdx, int panelIdx, Qt::KeyboardModifiers modifiers);
  void onDurationChanged(int shotIdx, int panelIdx, int frames);
  void onMoveShot(int fromShot, int toShot);
  void onColumnsChanged(int value);
  void onToggleCollapsePanels(bool on);          // "Compact view" view toggle
  void onPanelNavRequested(int shotIdx, int delta); // ◀ ▶ in collapsed view
  void onNumberingChanged(int comboIndex);
  void onNumberingConfig();   // opens the numbering settings dialog
  void onRefreshPreviews();
  void onSetTechnique();
  void onXsheetChanged();
  void onModelResequenced(); // called when ZtoryModel::resequenceXsheet runs
  void onMergeShots();            // merge selected shots (Board → xsheet + sub-scene merge)
  void onShotInserted(int col);   // called when razor/external op inserts a shot at col
  void onShotRemovedAt(int col);  // called when merge/external op deletes a shot at col
  void onMatchDuration(int shotIdx);  // resize timeline column to sub-scene actual duration
  void onLightPlaced(int shotIdx, int panelIdx,
                     double tailX, double tailY, double tipX, double tipY,
                     double depth, double spread);
  void onLightRemoved(int shotIdx, int panelIdx);
  void commitDurationUndo();          // fires after coalescing timer expires
  void commitTextUndo();              // fires when a text field loses focus
  void onBackToBoard();
private:
  // Text-field undo: snapshot taken on focusIn, pushed on focusOut.
  bool m_textEditing = false;
  std::vector<ZtoryShotSnap> m_textUndoBefore;
};

#endif // STORYBOARDPANEL_H
