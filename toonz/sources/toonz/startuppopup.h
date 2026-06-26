#pragma once

#ifndef STARTUPPOPUP_H
#define STARTUPPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"
#include "toonzqt/camerasettingswidget.h"
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>
#include <QListWidget>
#include <QSpinBox>

// forward declaration
class QLabel;
class QComboBox;
class StartupLabel;
class StartupScenesList;

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class StartupPopup final : public DVGui::Dialog {
  Q_OBJECT

public:
  enum Mode {
    DefaultMode,       // cold start — both tabs, block close, Quit on Cancel
    CreateMode,        // File > New Scene — only Create tab, no recent panel
    LoadMode,          // File > Load Scene — only Load tab, single-click loads
    LoadSubSceneMode   // File > Load Sub-Scene — only Load tab, multi-select + button
  };

  DVGui::LineEdit *m_nameFld;
  DVGui::FileField *m_pathFld;
  QLabel *m_widthLabel;
  QLabel *m_heightLabel;
  QLabel *m_fpsLabel;
  QLabel *m_unitsLabel;
  QLabel *m_resXLabel;
  QLabel *m_resTextLabel;
  QLabel *m_dpiLabel;
  QLabel *m_sceneNameLabel;
  DVGui::DoubleLineEdit *m_dpiFld;
  DVGui::MeasuredDoubleLineEdit *m_widthFld;
  DVGui::MeasuredDoubleLineEdit *m_heightFld;
  DVGui::DoubleLineEdit *m_fpsFld;
  DVGui::DoubleLineEdit *m_resXFld;
  DVGui::DoubleLineEdit *m_resYFld;
  DVGui::IntLineEdit *m_autoSaveTimeFld;
  QList<QString> m_sceneNames;
  QList<TFilePath> m_projectPaths;
  QCheckBox *m_showAtStartCB;
  QCheckBox *m_autoSaveOnCB;
  QComboBox *m_unitsCB;
  QPushButton *m_loadOtherSceneButton;
  QPushButton *m_newProjectButton;
  QComboBox *m_presetCombo;
  QComboBox *m_projectsCB;
  QPushButton *m_addPresetBtn, *m_removePresetBtn;
  CameraSettingsWidget *m_cameraSettingsWidget;
  double m_dpi;
  int m_xRes, m_yRes;
  const int RECENT_SCENES_MAX_COUNT = 10;
  bool m_updating                   = false;
  Mode m_mode                       = DefaultMode;
  QPushButton *m_cancelButton       = nullptr;
  QPushButton *m_loadSelectedButton = nullptr;
  QWidget *m_autoSaveBar            = nullptr;

  // Ztoryc: workflow + shot numbering
  QComboBox *m_workflowCB;       // in "Create" tab
  QComboBox *m_loadWorkflowCB;   // in "Load" tab
  QCheckBox *m_autoWorkflowCB = nullptr;  // auto-detect workflow from scene
  QComboBox *m_numberingStyleCB;
  QSpinBox *m_numberingStepSB;
  QLineEdit *m_seqPrefixFld;
  QLineEdit *m_shotPrefixFld;
  QSpinBox *m_numPaddingSB;
  QSpinBox *m_startNumberSB;
  QSpinBox *m_initialShotCountSB;
  QCheckBox *m_resetOnSeqChangeCB;
  QLabel *m_seqPrefixLabel;
  QLineEdit *m_productionFld;
  QLineEdit *m_titleFld;
  QLineEdit *m_episodeFld;
  QComboBox *m_techniqueFld;
  // Numbering widgets to show/hide based on workflow
  QWidget *m_numberingBox;
  QString m_presetListFile;
  QGroupBox *m_projectBox;
  QTabWidget *m_scenesTab;
  QGroupBox *m_recentBox;
  QVBoxLayout *m_recentSceneLay;
  QVector<StartupLabel *> m_recentNamesLabels;
  StartupScenesList *m_existingList;

public:
  explicit StartupPopup(Mode mode = DefaultMode);
  ~StartupPopup() override;

  // Returns an already-visible DefaultMode instance, or nullptr. Used to avoid
  // spawning duplicate startup popups when changing project from the browser.
  static StartupPopup *visibleDefaultInstance();

  // Refresh the project combo + scene list after the current project changed
  // (e.g. project picked from the browser tree), keeping this popup visible.
  void refreshAfterProjectChange();

protected:
  void showEvent(QShowEvent *) override;
  void closeEvent(QCloseEvent *) override;
  void loadPresetList();
  void savePresetList();
  void refreshRecentScenes();
  void refreshExistingScenes(TFilePath scenesFolder = TFilePath());
  QString aspectRatioValueToString(double value, int width = 0, int height = 0);
  double aspectRatioStringToValue(const QString &s);
  bool parsePresetString(const QString &str, QString &name, int &xres,
                         int &yres, double &fx, double &fy, QString &xoffset,
                         QString &yoffset, double &ar, bool forCleanup = false);
  void updateProjectCB();
  void setupProjectChange();

public slots:
  void onCancelButton();
  void onLoadSelectedButton();
  void onRecentSceneClicked(int index);
  void onProjectComboChanged(int index);
  void onExistingSceneClicked(int index);
  void onCreateButton();
  void onShowAtStartChanged(int index);
  void onProjectChanged(int index);
  void onNewProjectButtonPressed();
  void onOpenProjectButtonPressed();
  void onExploreProjectButtonPressed();
  void onLoadSceneButtonPressed();
  void onSceneChanged();
  void updateResolution();
  void updateSize();
  void onDpiChanged();
  void addPreset();
  void removePreset();
  void onPresetSelected(const QString &str);
  void onCameraUnitChanged(int index);
  void onAutoSaveOnChanged(int index);
  void onAutoSaveTimeChanged();
};

class StartupLabel : public QLabel {
  Q_OBJECT
public:
  explicit StartupLabel(const QString &text = "", QWidget *parent = 0,
                        int index = -1);
  ~StartupLabel();
  QString m_text;
  int m_index;
signals:
  void wasClicked(int index);

protected:
  void mousePressEvent(QMouseEvent *event);
};

class StartupScenesList : public QListWidget {
  Q_OBJECT

public:
  StartupScenesList(QWidget *parent, const QSize &iconSize);
  ~StartupScenesList();

  int countScenes() { return count(); }
  QString getName(int index) { return item(index)->text(); }
  QString getPath(int index) {
    return item(index)->data(Qt::UserRole).toString();
  }

  void clearScenes();
  void addScene(const QString &name, const QString &path);
  void findFirstScenePath(const QList<QString> paths);
  // Disables hover-selection and leaveEvent clear — needed for multi-select mode.
  void setMultiSelect(bool on) { m_multiSelect = on; }

protected:
  QPixmap createScenePreview(const QString &name, const TFilePath &fp);
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;

  QSize m_iconSize;
  bool  m_multiSelect = false;

protected slots:
  void onItemClicked(QListWidgetItem *item);

signals:
  void itemClicked(int index);
};

#endif  // STARTUPPOPUP_H
