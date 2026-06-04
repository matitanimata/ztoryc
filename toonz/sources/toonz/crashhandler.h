#pragma once

#ifndef CRASHHANDLER_INCLUDED
#define CRASHHANDLER_INCLUDED

#include "tcommon.h"
#include "tfilepath.h"

#include <QDialog>

class CrashHandler : public QDialog {
  Q_OBJECT;

  TFilePath m_crashFile;
  QString m_crashReport;

public:
  CrashHandler(QWidget *parent, TFilePath crashFile, QString crashTxt);

  void reject();

  static void install();
  static void reportProjectInfo(bool enableReport);
  static void attachParentWindow(QWidget *parent);
  static bool trigger(const QString reason, bool showDialog);
  // Call this when the app starts shutting down so the signal handler
  // exits silently instead of trying to open a Qt dialog.
  static void setAppExiting();

public slots:
  void copyClipboard();
  void openWebpage();
  void openFolder();
};

#endif  // CRASHHANDLER_INCLUDED
