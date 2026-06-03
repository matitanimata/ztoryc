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
public:
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
  void dropReceived(int fromShot, int toShot);
  // Emitted from resizeEvent when the stored pixmap is too small to fill the
  // panel at native (HiDPI) resolution — StoryboardPanel re-renders on receipt.
  void previewRerenderNeeded(int shotIdx, int panelIdx);
protected:
  void mousePressEvent(QMouseEvent *e) override;
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
  QToolButton *m_exportShotsButton;
  QToolButton *m_exportAnimaticButton;
  QSpinBox    *m_columnsPerRowSpin;
  QComboBox   *m_numberingCombo;
  QStackedWidget   *m_stack;
  SceneViewerPanel *m_comboViewer;
struct Shot {
    ShotData               data;
    std::vector<PanelWidget*> panels;
    bool selected = false;
  };
  std::vector<Shot> m_shots;
  int  m_columnsPerRow;
  int              m_selectedShotIndex;
  std::set<int>     m_selectedIndices;

  bool m_updating = false;   // re-entrancy guard for signal reactions
  struct ClipboardEntry {
    ShotData   data;
    bool       isClone;
    int        srcColumn;
    bool       isCut = false;    // true = cut (delete original on paste)
    TXshLevelP cutLevel;         // non-null for immediate cut: level kept alive here
  };
  std::vector<ClipboardEntry> m_clipboard;
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
  void onNumberingChanged(int comboIndex);
  void onNumberingConfig();   // opens the numbering settings dialog
  void onRefreshPreviews();
  void onExportPdf();
  void onExportShots();
  void onExportAnimatic();
  void onXsheetChanged();
  void onModelResequenced(); // called when ZtoryModel::resequenceXsheet runs
  void onMergeShots();            // merge selected shots (Board → xsheet + sub-scene merge)
  void onShotInserted(int col);   // called when razor/external op inserts a shot at col
  void onShotRemovedAt(int col);  // called when merge/external op deletes a shot at col
  void onMatchDuration(int shotIdx);  // resize timeline column to sub-scene actual duration
  void commitDurationUndo();          // fires after coalescing timer expires
  void commitTextUndo();              // fires when a text field loses focus
  void onBackToBoard();
private:
  // Text-field undo: snapshot taken on focusIn, pushed on focusOut.
  bool m_textEditing = false;
  std::vector<ZtoryShotSnap> m_textUndoBefore;
};

#endif // STORYBOARDPANEL_H
