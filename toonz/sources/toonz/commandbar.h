#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "saveloadqsettings.h"

#include "toonz/txsheet.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

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
