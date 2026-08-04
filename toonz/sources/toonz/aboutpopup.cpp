#include "aboutpopup.h"
#include "tenv.h"
#include "tsystem.h"
#include "tapp.h"

#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <QScreen>
#include <QTextEdit>
#include <QDesktopServices>
#include <QMainWindow>

AboutClickableLabel::AboutClickableLabel(QWidget* parent, Qt::WindowFlags f)
    : QLabel(parent) {
  setStyleSheet("text-decoration: underline;");
}

AboutClickableLabel::~AboutClickableLabel() {}

void AboutClickableLabel::mousePressEvent(QMouseEvent* event) {
  emit clicked();
}

AboutPopup::AboutPopup(QWidget* parent)
    : DVGui::Dialog(parent, true, true, "About Ztoryc") {
  setFixedWidth(360);
  setFixedHeight(380);

  setWindowTitle(tr("About Ztoryc"));
  setTopMargin(0);

  TFilePath baseLicensePath   = TEnv::getStuffDir() + "doc/LICENSE";
  TFilePath tahomaLicensePath = baseLicensePath + "LICENSE.txt";

  QVBoxLayout* mainLayout = new QVBoxLayout();

  QLabel* logo = new QLabel(this);
  QPixmap logoPixmap = QPixmap::fromImage(
      QImage(":Resources/ztoryc_about.png")).scaled(
      80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  logo->setPixmap(logoPixmap);
  logo->setAlignment(Qt::AlignHCenter);
  mainLayout->addWidget(logo);

  QString name = QString::fromStdString(TEnv::getApplicationFullName());
  name += "\nBuilt: " __DATE__;
  QLabel* nameLabel = new QLabel(name, this);
  nameLabel->setAlignment(Qt::AlignHCenter);
  mainLayout->addWidget(nameLabel);

  QString baseVer =
      QString::fromStdString(TEnv::getTahomaBaseVersionString());
  QLabel* forkLabel =
      new QLabel(tr("Tahoma2D base %1").arg(baseVer), this);
  forkLabel->setAlignment(Qt::AlignHCenter);
  mainLayout->addWidget(forkLabel);

  AboutClickableLabel* githubLink = new AboutClickableLabel(this);
  githubLink->setText(tr("github.com/matitanimata/ztoryc"));
  githubLink->setAlignment(Qt::AlignHCenter);
  connect(githubLink, &AboutClickableLabel::clicked, [=]() {
    QDesktopServices::openUrl(QUrl("https://github.com/matitanimata/ztoryc"));
  });
  mainLayout->addWidget(githubLink);

  AboutClickableLabel* licenseLink = new AboutClickableLabel(this);
  licenseLink->setText(tr("Ztoryc License (GPL v3)"));

  connect(licenseLink, &AboutClickableLabel::clicked, [=]() {
    if (TSystem::isUNC(tahomaLicensePath))
      QDesktopServices::openUrl(
          QUrl(QString::fromStdWString(tahomaLicensePath.getWideString())));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(
          QString::fromStdWString(tahomaLicensePath.getWideString())));
  });

  mainLayout->addWidget(licenseLink);

  AboutClickableLabel* thirdPartyLink = new AboutClickableLabel(this);
  thirdPartyLink->setText(tr("Third Party Licenses"));

  connect(thirdPartyLink, &AboutClickableLabel::clicked, [=]() {
    if (TSystem::isUNC(baseLicensePath))
      QDesktopServices::openUrl(
          QUrl(QString::fromStdWString(baseLicensePath.getWideString())));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(
          QString::fromStdWString(baseLicensePath.getWideString())));
  });

  mainLayout->addWidget(thirdPartyLink);

  QLabel* thirdPartyTools = new QLabel(
      tr("Ztoryc ships with FFmpeg (LGPLv2.1)\nand Rhubarb Lip Sync (MIT)."), this);
  thirdPartyTools->setAlignment(Qt::AlignHCenter);
  mainLayout->addWidget(thirdPartyTools);

  mainLayout->addSpacerItem(new QSpacerItem(0, 10));

  QLabel* thanksLabel = new QLabel(
      tr("Special thanks to manongjohn and the entire\n"
         "Tahoma2D team for the open-source foundation\n"
         "this project builds upon."), this);
  thanksLabel->setAlignment(Qt::AlignHCenter);
  thanksLabel->setWordWrap(true);
  mainLayout->addWidget(thanksLabel);

  mainLayout->addSpacerItem(new QSpacerItem(0, 10));

  // The people who fund the work. The sponsorship tiers state that the name
  // appears in the credits, so being listed here is part of what was agreed --
  // it does not need asking again. Only PUBLIC sponsorships go in, and the list
  // is kept by hand: the gh token on the dev machine has no read:user scope, so
  // the dashboard cannot be read from a script and this cannot self-update.
  // Checked against SUPPORTERS.md at every release.
  QLabel* patronsLabel = new QLabel(
      tr("Ztoryc is made possible by the people who fund it.\n"
         "Special thanks to:"), this);
  patronsLabel->setAlignment(Qt::AlignHCenter);
  mainLayout->addWidget(patronsLabel);

  QLabel* patronNames = new QLabel(tr("Slam Rockwell"), this);
  patronNames->setAlignment(Qt::AlignHCenter);
  patronNames->setWordWrap(true);
  {
    QFont f = patronNames->font();
    f.setItalic(true);
    patronNames->setFont(f);
  }
  mainLayout->addWidget(patronNames);

  // Clickable: telling someone to go to GitHub without taking them there is
  // half an invitation.
  QLabel* sponsorInvite = new QLabel(this);
  sponsorInvite->setText(
      tr("Please consider <a href=\"https://github.com/sponsors/matitanimata\">"
         "sponsoring Ztoryc's development</a> on GitHub."));
  sponsorInvite->setTextFormat(Qt::RichText);
  sponsorInvite->setTextInteractionFlags(Qt::TextBrowserInteraction);
  sponsorInvite->setOpenExternalLinks(true);
  sponsorInvite->setAlignment(Qt::AlignHCenter);
  sponsorInvite->setWordWrap(true);
  mainLayout->addWidget(sponsorInvite);

  mainLayout->addStretch();

  QFrame* mainFrame = new QFrame(this);
  mainFrame->setLayout(mainLayout);
  mainFrame->setFixedWidth(360);

  addWidget(mainFrame);

  QPushButton* button = new QPushButton(tr("Close"), this);
  button->setDefault(true);
  addButtonBarWidget(button);
  connect(button, SIGNAL(clicked()), this, SLOT(accept()));
}

void AboutPopup::showEvent(QShowEvent *event) {

  // center window
  QScreen* currentScreen         = TApp::instance()->getMainWindow()->screen();
  QPoint activeMonitorCenter     = currentScreen->availableGeometry().center();
  QPoint thisPopupCenter         = this->rect().center();
  QPoint centeredOnActiveMonitor = activeMonitorCenter - thisPopupCenter;
  this->move(centeredOnActiveMonitor);

}
