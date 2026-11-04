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
  bool m_previousSession;

public:
  CrashHandler(QWidget *parent, TFilePath crashFile, QString crashTxt,
               bool previousSession = false);

  void reject();

  static void install();
  // Unix: call once the crash report folder is known. It prepares what the
  // signal handler writes, and takes the report of a crash in the last
  // session, if there is one.
  static void prepareReport();
  // Unix: files that report under its crash time, with function names and
  // lines, and shows it once. Nothing to do when the last session ended well.
  static void showPendingReport();
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
