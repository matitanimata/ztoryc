#pragma once

#include "pane.h"

#include <QWidget>
#include <QString>
#include <QTextEdit>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QList>

//=============================================================================
// ZtoryScriptView — widget interno con testo e search
//=============================================================================

class ZtoryScriptView final : public QWidget {
  Q_OBJECT

public:
  explicit ZtoryScriptView(QWidget *parent = nullptr);

  void loadFile(const QString &filePath);
  void clear();
  // Import a screenplay: copy it into the project's +extras/script folder,
  // record the path in ZtoryModel (persisted in .ztoryc), then display it.
  void importScreenplay(const QString &srcPath);
  // Reload (or clear) the screenplay to match ZtoryModel::scriptFile() — call
  // on scene switch / model reset so the panel never keeps a stale screenplay.
  void reloadFromModel();

protected:
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dropEvent(QDropEvent *e) override;

private slots:
  void onImportClicked();
  void onSearchTextChanged(const QString &text);
  void onSearchNext();
  void onSearchPrev();
  // Authoritative per-scene reload: on scene switch, read the scriptFile tag
  // from the current scene's .ztoryc and update the model (clears when none).
  // Independent of StoryboardPanel so the screenplay is always scene-correct.
  void onSceneSwitched();

private:
  // Read the <scriptFile> tag from the current scene's .ztoryc (project-relative
  // path, or "" if the scene has none / is unsaved).
  QString readScriptFileFromZtoryc() const;

  // Parsing
  QString parseFdx(const QString &filePath);
  QString parseFountain(const QString &filePath);
  QString parseDocx(const QString &filePath);
  QString parseOdt(const QString &filePath);
  QString parseTxt(const QString &filePath);

  // Search helpers
  void applySearch(const QString &text);
  void navigateMatch(bool forward);

  // UI
  QToolButton  *m_importButton;
  QLineEdit    *m_searchField;
  QToolButton  *m_searchPrevBtn;
  QToolButton  *m_searchNextBtn;
  QLabel       *m_matchLabel;
  QTextEdit    *m_textEdit;

  // Search state
  QList<int>    m_matchPositions;
  int           m_currentMatch = -1;
  QString       m_lastSearch;

  // Absolute path of the screenplay currently displayed ("" = none).  Used by
  // reloadFromModel() to avoid reloading the same file repeatedly.
  QString       m_currentFilePath;
};

//=============================================================================
// ZtoryScriptPanel — TPanel wrapper dockabile
//=============================================================================

class ZtoryScriptPanel final : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryScriptPanel(QWidget *parent = nullptr);

private:
  ZtoryScriptView *m_view;
};
