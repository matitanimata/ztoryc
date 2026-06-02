#pragma once

#ifndef MAINTOOLBAR_H
#define MAINTOOLBAR_H

#include <memory>

#include "commandbar.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

//=============================================================================
// Main Toolbar
//-----------------------------------------------------------------------------

class MainToolbar final : public CommandBar {
  Q_OBJECT

public:
  MainToolbar(QWidget *parent = nullptr);
  ~MainToolbar();

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;

protected slots:
  void doCustomizeCommandBar();
  void doResetCommandBar();
};

#endif  // MAINTOOLBAR_H
