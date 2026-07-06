#pragma once

#ifndef EXPORTSCENEPOPUP_H
#define EXPORTSCENEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/filefield.h"
#include "tfilepath.h"
#include "filebrowsermodel.h"
#include "dvdirtreeview.h"

#include <QTreeView>
#include <QItemDelegate>

// forward declaration
class QLabel;
class ExportSceneTreeView;
class QRadioButton;

//=============================================================================
// ExportSceneDvDirModelFileFolderNode

class ExportSceneDvDirModelFileFolderNode : public DvDirModelFileFolderNode {
public:
  ExportSceneDvDirModelFileFolderNode(DvDirModelNode *parent, std::wstring name,
                                      const TFilePath &path)
      : DvDirModelFileFolderNode(parent, name, path) {}
  ExportSceneDvDirModelFileFolderNode(DvDirModelNode *parent,
                                      const TFilePath &path)
      : DvDirModelFileFolderNode(parent, path) {}

  DvDirModelNode *makeChild(std::wstring name) override;
  virtual DvDirModelFileFolderNode *createExposeSceneNode(
      DvDirModelNode *parent, const TFilePath &path);
};

//=============================================================================
// ExportSceneDvDirModelSpecialFileFolderNode

class ExportSceneDvDirModelSpecialFileFolderNode final
    : public ExportSceneDvDirModelFileFolderNode {
  QPixmap m_pixmap;

public:
  ExportSceneDvDirModelSpecialFileFolderNode(DvDirModelNode *parent,
                                             std::wstring name,
                                             const TFilePath &path)
      : ExportSceneDvDirModelFileFolderNode(parent, name, path) {}
  QPixmap getPixmap(bool isOpen) const override { return m_pixmap; }
  void setPixmap(const QPixmap &pixmap) { m_pixmap = pixmap; }
};

//=============================================================================
// ExportSceneDvDirModelProjectNode

class ExportSceneDvDirModelProjectNode final
    : public ExportSceneDvDirModelFileFolderNode {
public:
  ExportSceneDvDirModelProjectNode(DvDirModelNode *parent,
                                   const TFilePath &path)
      : ExportSceneDvDirModelFileFolderNode(parent, path) {}
  void makeCurrent() {}
  bool isCurrent() const;
  QPixmap getPixmap(bool isOpen) const override;

  virtual DvDirModelFileFolderNode *createExposeSceneNode(
      DvDirModelNode *parent, const TFilePath &path) override;
};

//=============================================================================
// ExportSceneDvDirModelRootNode

class ExportSceneDvDirModelRootNode final : public DvDirModelNode {
  std::vector<ExportSceneDvDirModelFileFolderNode *> m_projectRootNodes;
  ExportSceneDvDirModelFileFolderNode *m_sandboxProjectNode;
  void add(std::wstring name, const TFilePath &path);

public:
  ExportSceneDvDirModelRootNode();

  void refreshChildren() override;
  DvDirModelNode *getNodeByPath(const TFilePath &path) override;
};

//=============================================================================
// ExportSceneDvDirModel

class ExportSceneDvDirModel final : public QAbstractItemModel {
  DvDirModelNode *m_root;

public:
  ExportSceneDvDirModel();
  ~ExportSceneDvDirModel();

  DvDirModelNode *getNode(const QModelIndex &index) const;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  QModelIndex childByName(const QModelIndex &parent,
                          const std::wstring &name) const;
  int columnCount(const QModelIndex &parent) const override { return 1; }
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole) override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  bool hasChildren(const QModelIndex &parent) const override;
  void refresh(const QModelIndex &index);
};

//=============================================================================
// ExportSceneTreeViewDelegate

class ExportSceneTreeViewDelegate final : public QItemDelegate {
  Q_OBJECT
  ExportSceneTreeView *m_treeView;

public:
  ExportSceneTreeViewDelegate(ExportSceneTreeView *parent);
  ~ExportSceneTreeViewDelegate();
  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

//=============================================================================
// ExportSceneTreeView

class ExportSceneTreeView final : public StyledTreeView {
  Q_OBJECT
  ExportSceneDvDirModel *m_model;

public:
  ExportSceneTreeView(QWidget *parent);
  QSize sizeHint() const override;
  DvDirModelNode *getCurrentNode() const;

protected:
  void refresh();
  void showEvent(QShowEvent *) override;
  void focusInEvent(QFocusEvent *event) override;

public slots:
  void resizeToConts();

signals:
  void focusIn();
};

//=============================================================================
// ExportScenePopup

class ExportScenePopup final : public DVGui::Dialog {
  Q_OBJECT

  std::vector<TFilePath> m_scenes;
  //  QLabel* m_command;
  QLabel *m_newProjectNameLabel;
  DVGui::LineEdit *m_newProjectName;
  ExportSceneTreeView *m_projectTreeView;
  QRadioButton *m_newProjectButton;
  QRadioButton *m_chooseProjectButton;

  QLabel *m_pathFieldLabel;
  DVGui::FileField *m_projectLocationFld;

  // Ztoryc: customizable asset-folder layout for the new project (the user is
  // not forced to inherit the current project's folder structure) and target
  // application toggle (OpenToonz needs compatibility conversions on export).
  QList<QPair<std::string, DVGui::FileField *>> m_folderFlds;
  QRadioButton *m_targetZtorycButton;
  QRadioButton *m_targetTahomaButton;
  QRadioButton *m_targetOTButton;

  bool m_createNewProject;

  // Filled on successful export — lets callers post-process the exported
  // scenes (e.g. the Ztoryc Board copies the .ztoryc back-link companions).
  std::vector<TFilePath> m_exportedScenes;

public:
  ExportScenePopup(std::vector<TFilePath> scenes, bool newProjectMode = false);

  void setScenes(std::vector<TFilePath> scenes) {
    m_scenes = scenes;
    //    updateCommandLabel();
  }

  const std::vector<TFilePath> &exportedScenes() const {
    return m_exportedScenes;
  }

  // Target application of an export. Ztoryc keeps everything as-is; stock
  // Tahoma2D needs the Ztoryc-only per-xsheet In/Out markers stripped and a
  // tahomaproject.xml project file; OpenToonz additionally needs explicit
  // holds and a "<folder>_otprj.xml" project file.
  enum class ExportTarget { Ztoryc, Tahoma, OpenToonz };

  // UI-independent export engine, reusable by other entry points (the Ztoryc
  // Board "Export Shots to New Project" flow embeds these in its own dialog).
  struct NewProjectSpec {
    QString name;
    QString location;  // parent folder the project is created in
    QList<QPair<std::string, QString>> folders;  // folder name → custom path
    bool useSubScenePath = false;  // "Separate assets into scene sub-folders"
    ExportTarget target  = ExportTarget::Ztoryc;
  };
  //! Create the project on disk; empty path (+ DVGui warning) on failure.
  static TFilePath createProjectFromSpec(const NewProjectSpec &spec);
  //! Import scenes + collect assets into the project, then run the
  //! target-specific compatibility pass. Returns exported paths.
  static std::vector<TFilePath> exportScenesToProject(
      const std::vector<TFilePath> &scenes, const TFilePath &projectPath,
      ExportTarget target);
  //! Target-specific compatibility pass alone. Callers that copy extra assets
  //! AFTER exportScenesToProject (the Ztoryc Board flow copies '+' project
  //! folder levels) must export with target Ztoryc and run this at the very
  //! end: the pass re-loads and re-saves each scene, so any resource still
  //! missing at that point (e.g. the animatic audio) would be loaded empty
  //! and silently dropped from the re-saved scene.
  static void applyTargetCompatibility(const std::vector<TFilePath> &scenes,
                                       const TFilePath &projectPath,
                                       ExportTarget target);

protected slots:
  void switchMode(int id);
  void onProjectTreeViweFocusIn();
  void onProjectNameFocusIn();
  void onExport();

protected:
  //! Create new project from the dialog fields and return new project path.
  TFilePath createNewProject();
  //! Target application chosen in the dialog radio buttons.
  ExportTarget selectedTarget() const;
  //! OpenToonz compatibility: materialize implicit holds in an exported scene
  //! (also strips the Ztoryc-only In/Out markers).
  static bool convertSceneToExplicitHolds(const TFilePath &scenePath);
  //! Tahoma2D compatibility: strip the Ztoryc-only per-xsheet In/Out markers
  //! from an exported scene (stock Tahoma rejects the inOutMarkers tag).
  static bool stripSceneInOutMarkers(const TFilePath &scenePath);
  //! OpenToonz compatibility: write a "<folder>_otprj.xml" project file copy.
  static void writeOpenToonzProjectFile(const TFilePath &projectPath);
  //! Tahoma2D compatibility: write a "tahomaproject.xml" project file copy
  //! (Ztoryc projects are saved as ztorycproject.xml, unknown to stock Tahoma).
  static void writeTahomaProjectFile(const TFilePath &projectPath);
  //  void updateCommandLabel();
};

#endif  // EXPORTSCENEPOPUP_H
