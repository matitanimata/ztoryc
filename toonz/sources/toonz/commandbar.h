#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "saveloadqsettings.h"

#include "tfilepath.h"
#include "toonz/txsheet.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>
#include <QStringList>

//-----------------------------------------------------------------------------

// forward declaration
class QAction;

//=============================================================================
// CommandBar
//-----------------------------------------------------------------------------

enum CommandBarType { Command = 0, Quick, Main };

class CommandBar : public QToolBar, public SaveLoadQSettings {
  Q_OBJECT
protected:
  bool m_isCollapsible;
  CommandBarType m_barType;
  QString m_barId;
  bool m_isDefault;

public:
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags(),
             bool isCollapsible     = false,
             CommandBarType barType = CommandBarType::Command);

  QString getBarId() { return m_barId; }

  bool isDefault() { return m_isDefault; }
  void setDefault(bool isDefault) { m_isDefault = isDefault; }

  // ─── Ztoryc: Quick Toolbar per workflow ────────────────────────────────────
  // Il workflow e' espresso come "tag" (una stringa) invece che come enum per
  // non tirarsi dentro ztorymodel.h in ogni header che usa la toolbar.
  //
  // currentWorkflowTag() torna una stringa VUOTA quando la preferenza e'
  // spenta, e la stringa vuota significa ovunque «la barra comune» — cosi' il
  // meccanismo si spegne per intero senza rami separati.
  static QString currentWorkflowTag();
  static QStringList allWorkflowTags();
  static QString workflowDisplayName(const QString &workflowTag);

  // Percorso del file della Quick Toolbar. workflowTag vuoto = barra comune
  // (`quicktoolbar.xml`), altrimenti `quicktoolbars/quicktoolbar_<tag>.xml` —
  // stessa convenzione della cartella `commandbars/` gia' usata dalle Command
  // Bar con id.
  static TFilePath quickToolbarPath(const QString &workflowTag,
                                    bool fromTemplate);

  // Ztoryc: travaso una-tantum dei comandi nuovi nelle Quick Toolbar personali.
  // Serve perche' il file personale VINCE sul template e non c'e' nessuna
  // fusione: senza questo, una voce aggiunta al default non arriva mai a chi
  // la barra se l'era gia' personalizzata. Va chiamata una volta all'avvio,
  // dopo defineActions().
  static void migrateQuickToolbars();

  // SaveLoadQSettings
  virtual void save(QSettings &settings,
                    bool forPopupIni = false) const override;
  virtual void load(QSettings &settings) override;

signals:
  void updateVisibility();

protected:
  static void fillToolbar(CommandBar *toolbar,
                          CommandBarType barType = CommandBarType::Command,
                          QString barId          = "");
  static void buildDefaultToolbar(CommandBar *toolbar);
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
  void doResetCommandBar();
  void onCloseButtonPressed();
};

#endif  // COMMANDBAR_H
