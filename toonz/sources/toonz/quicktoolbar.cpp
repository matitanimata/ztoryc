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
  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(tr("Customize Quick Toolbar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  menu->addSeparator();

  QAction *resetCommandBar = menu->addAction(tr("Reset Quick Toolbar"));
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
  TFilePath personalPath =
      ToonzFolder::getMyModuleDir() + TFilePath("quicktoolbar.xml");

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
