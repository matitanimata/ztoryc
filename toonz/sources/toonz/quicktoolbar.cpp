#include "quicktoolbar.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "tapp.h"
#include "menubarcommandids.h"
#include "commandbarpopup.h"
#include "tsystem.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/childstack.h"
#include "toonz/toonzfolders.h"

// Ztoryc includes
#include "ztorymodel.h"

// Qt includes
#include <QWidgetAction>

//=============================================================================

namespace XsheetGUI {

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

QuickToolbar::QuickToolbar(XsheetViewer *parent, Qt::WindowFlags flags,
                           bool isCollapsible)
    : CommandBar(parent, flags, isCollapsible, CommandBarType::Quick)
    , m_viewer(parent) {
  setObjectName("cornerWidget");
  setFixedHeight(29);
  setObjectName("QuickToolbar");
  setIconSize(QSize(20, 20));

  // Ztoryc: con la Quick Toolbar per workflow la barra giusta cambia sotto i
  // piedi al cambio di workflow.  Non basta affidarsi alla ricostruzione della
  // room: se il pannello sopravvive al cambio, resterebbe la barra precedente.
  connect(ZtoryModel::instance(), &ZtoryModel::workflowChanged, this,
          &QuickToolbar::onWorkflowChanged);

  connect(TApp::instance()->getCurrentScene(),
          &TSceneHandle::preferenceChanged, this,
          &QuickToolbar::onPreferenceChanged);
}

//-----------------------------------------------------------------------------

void QuickToolbar::onWorkflowChanged() {
  fillToolbar(this, CommandBarType::Quick);
}

//-----------------------------------------------------------------------------

void QuickToolbar::onPreferenceChanged(const QString &prefName) {
  // Accendendo o spegnendo la separazione per workflow cambia QUALE file si
  // legge, non solo l'ingombro: la barra va ricostruita, non solo riposizionata.
  if (prefName == "QuickToolbarWorkflow") fillToolbar(this, CommandBarType::Quick);
}

//-----------------------------------------------------------------------------

void QuickToolbar::showToolbar(bool show) {
  if (!m_isCollapsible) return;
  show ? this->show() : this->hide();
}

//-----------------------------------------------------------------------------

void QuickToolbar::toggleQuickToolbar() {
  bool toolbarEnabled = Preferences::instance()->isShowQuickToolbarEnabled();
  Preferences::instance()->setValue(showQuickToolbar, !toolbarEnabled);
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("QuickToolbar");
}

//-----------------------------------------------------------------------------

void QuickToolbar::showEvent(QShowEvent *e) {
  if (Preferences::instance()->isShowQuickToolbarEnabled() || !m_isCollapsible)
    show();
  else
    hide();
  emit updateVisibility();
}

//-----------------------------------------------------------------------------

void QuickToolbar::contextMenuEvent(QContextMenuEvent *event) {
  // Ztoryc: le voci dicono su QUALE barra si sta per agire, altrimenti con la
  // preferenza accesa si personalizza un solo workflow credendo di toccarli
  // tutti.
  QString wfTag  = currentWorkflowTag();
  QString wfName = workflowDisplayName(wfTag);

  QString customizeText = wfTag.isEmpty()
                              ? tr("Customize Quick Toolbar")
                              : tr("Customize Quick Toolbar (%1)").arg(wfName);
  QString resetText     = wfTag.isEmpty()
                              ? tr("Reset Quick Toolbar")
                              : tr("Reset Quick Toolbar (%1)").arg(wfName);

  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(customizeText);
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  menu->addSeparator();

  QAction *resetCommandBar = menu->addAction(resetText);
  connect(resetCommandBar, SIGNAL(triggered()), SLOT(doResetCommandBar()));

  resetCommandBar->setEnabled(!isDefault());

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void QuickToolbar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup("", CommandBarType::Quick);

  if (cbPopup->exec()) {
    fillToolbar(this, CommandBarType::Quick);
  }
  delete cbPopup;
}

//-----------------------------------------------------------------------------

void QuickToolbar::doResetCommandBar() {
  // Ztoryc: si azzera CIO' CHE SI STA GUARDANDO. Se esiste una barra per il
  // workflow corrente si cancella quella — e si ricade sulla barra comune, che
  // resta intatta. Solo se non c'e' si azzera la comune.
  QString wfTag = currentWorkflowTag();

  TFilePath personalPath = quickToolbarPath(wfTag, false);
  if (!wfTag.isEmpty() && !TSystem::doesExistFileOrLevel(personalPath))
    personalPath = quickToolbarPath("", false);

  if (TSystem::doesExistFileOrLevel(personalPath))
    TSystem::deleteFile(personalPath);

  fillToolbar(this, CommandBarType::Quick);
}

//============================================================

class ToggleQuickToolbarCommand final : public MenuItemHandler {
public:
  ToggleQuickToolbarCommand() : MenuItemHandler(MI_ToggleQuickToolbar) {}
  void execute() override { QuickToolbar::toggleQuickToolbar(); }
} ToggleQuickToolbarCommand;

//============================================================

}  // namespace XsheetGUI
