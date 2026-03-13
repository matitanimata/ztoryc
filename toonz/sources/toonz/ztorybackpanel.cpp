#include "ztorybackpanel.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/menubarcommand.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

ZtoryBackPanel::ZtoryBackPanel(QWidget *parent)
    : TPanel(parent)
{
  setObjectName("ZtoryBackPanel");
  setWindowTitle(tr("Ztoryc"));

  QWidget *main = new QWidget(this);
  QVBoxLayout *layout = new QVBoxLayout(main);
  layout->setSpacing(8);
  layout->setContentsMargins(8, 8, 8, 8);

  QPushButton *backBtn = new QPushButton("< Back to Storyboard");
  backBtn->setStyleSheet(
    "QPushButton{background:#3a3a6a;color:white;border-radius:4px;padding:8px 16px;font-size:12px;}"
    "QPushButton:hover{background:#5a5a8a;}");

  QLabel *hint = new QLabel("Edit shot, then return\nto Storyboard when done.");
  hint->setStyleSheet("color:#888;font-size:10px;");
  hint->setAlignment(Qt::AlignCenter);

  layout->addWidget(backBtn);
  layout->addWidget(hint);
  layout->addStretch();

  setWidget(main);
  connect(backBtn, &QPushButton::clicked, this, &ZtoryBackPanel::onBackClicked);
}

void ZtoryBackPanel::onBackClicked() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  MainWindow *mw = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (mw) mw->switchToRoom("BOARD");
}

class ZtoryBackPanelFactory final : public TPanelFactory {
public:
  ZtoryBackPanelFactory() : TPanelFactory("ZtoryBack") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryBackPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Ztoryc"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryBackPanelFactory;
