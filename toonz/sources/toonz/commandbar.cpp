

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "tsystem.h"
#include "commandbarpopup.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/toonzfolders.h"
#include "toonz/preferences.h"

// Ztoryc includes
#include "ztorymodel.h"

// Qt includes
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QFile>
#include <QtDebug>
#include <QMenuBar>
#include <QContextMenuEvent>
#include <QToolButton>

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible, CommandBarType barType)
    : QToolBar(parent)
    , m_isCollapsible(isCollapsible)
    , m_barType(barType)
    , m_barId("")
    , m_isDefault(true) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");
  if (barType == CommandBarType::Command) {
    QDateTime date = QDateTime::currentDateTime();
    m_barId        = date.toString("yyyyMMddhhmmss");
  }
  // Sets up default.
  fillToolbar(this, m_barType, m_barId);
  setIconSize(QSize(20, 20));
  QIcon moreIcon(":Resources/more.svg");
  QToolButton *more = findChild<QToolButton *>("qt_toolbar_ext_button");
  more->setIcon(moreIcon);

  if (barType == CommandBarType::Command)
    connect(parentWidget(), SIGNAL(closeButtonPressed()), this,
            SLOT(onCloseButtonPressed()));
}

//-----------------------------------------------------------------------------

// SaveLoadQSettings
void CommandBar::save(QSettings &settings, bool forPopupIni) const {
  // Don't save barId during popup.ini save
  if (forPopupIni) return;
  settings.setValue("barId", m_barId);
}

//-----------------------------------------------------------------------------

void CommandBar::load(QSettings &settings) {
  QVariant barId = settings.value("barId");
  if (barId.canConvert(QVariant::String)) {
    m_barId = barId.toString();
    fillToolbar(this, m_barType, m_barId);
  }
}

//-----------------------------------------------------------------------------

// ─── Ztoryc: travaso una-tantum dei comandi nuovi ────────────────────────────

namespace {

// Comandi entrati nella Quick Toolbar dopo che gli utenti avevano gia' un file
// personale. Il numero e' il «giro»: alzandolo e aggiungendo righe qui, il
// travaso riparte solo per le voci nuove.
struct QuickToolbarAddition {
  int round;
  const char *commandId;
};

const QuickToolbarAddition l_quickToolbarAdditions[] = {
    {1, "MI_ZtoryShowMesh"},
    {1, "MI_ToggleKeyframesFollowExposure"},
};

const int l_quickToolbarMigrationRound = 1;

// Aggiunge i comandi mancanti in coda al file, subito prima di </commandbar>.
// Si lavora sul testo invece di rileggere e riscrivere l'XML apposta per non
// poter perdere niente di quello che l'utente ci aveva messo.
bool appendCommandsToBar(const TFilePath &fp, const QStringList &commandIds) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) return false;
  QString content = QString::fromUtf8(file.readAll());
  file.close();

  int closing = content.lastIndexOf("</commandbar>");
  if (closing < 0) return false;  // non e' un file che riconosciamo: non tocco

  QString toAdd;
  for (const QString &id : commandIds) {
    if (content.contains("<command>" + id + "</command>")) continue;
    toAdd += "    <command>" + id + "</command>\n";
  }
  if (toAdd.isEmpty()) return false;

  content.insert(closing, "    <separator/>\n" + toAdd);

  if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) return false;
  file.write(content.toUtf8());
  file.close();
  return true;
}

}  // namespace

//-----------------------------------------------------------------------------

void CommandBar::migrateQuickToolbars() {
  const int done = Preferences::instance()->getZtoryQuickToolbarMigration();
  if (done >= l_quickToolbarMigrationRound) return;

  QStringList commandIds;
  for (const QuickToolbarAddition &a : l_quickToolbarAdditions)
    if (a.round > done) commandIds << QString(a.commandId);

  if (!commandIds.isEmpty()) {
    // La barra comune e' quella da cui ricadono tutti i workflow, quindi basta
    // lei; le barre per workflow che nascessero dopo partono gia' da questa.
    QStringList tags;
    tags << QString();
    tags << allWorkflowTags();

    for (const QString &tag : tags) {
      TFilePath fp = quickToolbarPath(tag, false);
      if (TSystem::doesExistFileOrLevel(fp)) appendCommandsToBar(fp, commandIds);
    }
  }

  Preferences::instance()->setValue(ztoryQuickToolbarMigration,
                                    l_quickToolbarMigrationRound);
}

// ─── Ztoryc: Quick Toolbar per workflow ──────────────────────────────────────

QString CommandBar::currentWorkflowTag() {
  if (!Preferences::instance()->isZtoryPerWorkflowQuickToolbarEnabled())
    return QString();

  switch (ZtoryModel::instance()->currentWorkflow()) {
  case ZtoryWorkflow::Storyboard:
    return QString("storyboard");
  case ZtoryWorkflow::Tradigital:
    return QString("tradigital");
  case ZtoryWorkflow::CutoutDigital:
    return QString("cutout");
  case ZtoryWorkflow::StopMotion:
    return QString("stopmotion");
  case ZtoryWorkflow::Character:
    return QString("character");
  }
  return QString();
}

//-----------------------------------------------------------------------------

QStringList CommandBar::allWorkflowTags() {
  return QStringList() << "storyboard" << "tradigital" << "cutout"
                       << "stopmotion" << "character";
}

//-----------------------------------------------------------------------------

QString CommandBar::workflowDisplayName(const QString &workflowTag) {
  if (workflowTag == "storyboard") return tr("Storyboard");
  if (workflowTag == "tradigital") return tr("2D Tradigital");
  if (workflowTag == "cutout") return tr("Cutout Digital");
  if (workflowTag == "stopmotion") return tr("Stop Motion");
  return tr("All Workflows");
}

//-----------------------------------------------------------------------------

TFilePath CommandBar::quickToolbarPath(const QString &workflowTag,
                                       bool fromTemplate) {
  TFilePath dir = fromTemplate ? ToonzFolder::getTemplateModuleDir()
                               : ToonzFolder::getMyModuleDir();
  if (workflowTag.isEmpty()) return dir + TFilePath("quicktoolbar.xml");

  return dir + TFilePath("quicktoolbars") +
         TFilePath("quicktoolbar_" + workflowTag + ".xml");
}

//-----------------------------------------------------------------------------

void CommandBar::fillToolbar(CommandBar *toolbar, CommandBarType barType,
                             QString barId) {
  toolbar->clear();
  toolbar->setDefault(true);
  TFilePath personalPath;
  bool fileFound = false;
  if (barType == CommandBarType::Quick) {
    // Ztoryc: prima la barra del workflow corrente, poi quella comune. Con la
    // preferenza spenta (tag vuoto) il primo ramo non si prende nemmeno, e il
    // comportamento e' identico a quello di sempre.
    QString wfTag = currentWorkflowTag();
    personalPath  = quickToolbarPath(wfTag, false);
    if (!wfTag.isEmpty() && !TSystem::doesExistFileOrLevel(personalPath))
      personalPath = quickToolbarPath("", false);
  } else if (barType == CommandBarType::Main) {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");
  } else if (!barId.isEmpty()) {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbars") +
                   TFilePath("commandbar_" + barId + ".xml");
    if (!TSystem::doesExistFileOrLevel(personalPath))
      personalPath =
          ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  } else {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  }

  fileFound = TSystem::doesExistFileOrLevel(personalPath);
  toolbar->setDefault(!fileFound);

  if (!fileFound) {
    if (barType == CommandBarType::Quick) {
      // Stessa ricaduta sui template: cosi' si potranno spedire default per
      // singolo workflow senza toccare piu' il codice.
      QString wfTag = currentWorkflowTag();
      personalPath  = quickToolbarPath(wfTag, true);
      if (!wfTag.isEmpty() && !TSystem::doesExistFileOrLevel(personalPath))
        personalPath = quickToolbarPath("", true);
    } else if (barType == CommandBarType::Main) {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("maintoolbar.xml");
    } else {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }

  QFile file(toQString(personalPath));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    buildDefaultToolbar(toolbar);
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName    = reader.readElementText();
          std::string cmdStr = cmdName.toStdString();
          QAction *action =
              CommandManager::instance()->getAction(cmdStr.c_str());
          if (action) toolbar->addAction(action);
        } else if (reader.name() == "separator") {
          toolbar->addSeparator();
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  } else {
    reader.raiseError(QObject::tr("Cannot Read XML File"));
  }

  if (reader.hasError()) {
    buildDefaultToolbar(toolbar);
    return;
  }
}

//-----------------------------------------------------------------------------

void CommandBar::buildDefaultToolbar(CommandBar *toolbar) {
  toolbar->clear();
  TApp *app = TApp::instance();
  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    toolbar->addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    toolbar->addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    toolbar->addAction(newRasterLevel);
    toolbar->addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    toolbar->addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    toolbar->addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    toolbar->addAction(reframeThrees);

    toolbar->addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    toolbar->addAction(repeat);

    toolbar->addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    toolbar->addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    toolbar->addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    toolbar->addAction(leave);
    QAction *editInPlace =
        CommandManager::instance()->getAction("MI_ToggleEditInPlace");
    toolbar->addAction(editInPlace);
    QAction *mainAudio =
        CommandManager::instance()->getAction("MI_ToggleMainAudio");
    toolbar->addAction(mainAudio);
  }
}

//-----------------------------------------------------------------------------

void CommandBar::onCloseButtonPressed() {
  if (m_barType != CommandBarType::Command || m_barId.isEmpty()) return;

  TFilePath commandbarFile = ToonzFolder::getMyModuleDir() +
                             TFilePath("commandbars") +
                             TFilePath("commandbar_" + m_barId + ".xml");
  if (!TSystem::doesExistFileOrLevel(commandbarFile)) return;

  TSystem::deleteFile(commandbarFile);
}

//-----------------------------------------------------------------------------

void CommandBar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(tr("Customize Command Bar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  menu->addSeparator();

  QAction *resetCommandBar = menu->addAction(tr("Reset Command Bar"));
  connect(resetCommandBar, SIGNAL(triggered()), SLOT(doResetCommandBar()));
  resetCommandBar->setEnabled(!isDefault());

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CommandBar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup(m_barId);

  if (cbPopup->exec()) {
    fillToolbar(this, m_barType, m_barId);
  }
  delete cbPopup;
}

//-----------------------------------------------------------------------------

void CommandBar::doResetCommandBar() {
  TFilePath personalPath;

  switch (m_barType) {
  case CommandBarType::Main:
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");
    break;
  case CommandBarType::Quick:
    personalPath =
        ToonzFolder::getMyModuleDir() + TFilePath("quicktoolbar.xml");
    break;
  default:
    if (!m_barId.isEmpty()) {
      personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbars") +
                     TFilePath("commandbar_" + m_barId + ".xml");
      if (!TSystem::doesExistFileOrLevel(personalPath)) {
        personalPath =
            ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
      }
      break;
    }
  }

  if (TSystem::doesExistFileOrLevel(personalPath))
    TSystem::deleteFile(personalPath);

  fillToolbar(this, m_barType, m_barId);
}
