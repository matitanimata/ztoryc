#pragma once

//============================================================================
// KitsuConnectDialog — Phase 1 (M5) UI for the Kitsu integration.
//
// A self-contained connection panel: URL + email + password, a Connect button,
// then (after a successful JWT login) the list of open projects and the task
// statuses pulled from the server, each shown with its Kitsu colour and the
// Ztoryc status it maps onto. No push / sync yet — that lands in later phases.
//============================================================================

#include <QDialog>

class KitsuClient;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QComboBox;
class QLabel;
class QTableWidget;

class KitsuConnectDialog final : public QDialog {
  Q_OBJECT
public:
  explicit KitsuConnectDialog(QWidget *parent = nullptr);

private:
  void onConnectClicked();
  void onLinkClicked();    // bind the selected Kitsu project (pull)
  void onCreateClicked();  // create a new Kitsu project from the Ztoryc model (push)
  void onPushShotsClicked();  // push the Ztoryc shot list up to Kitsu
  void setBusy(bool busy);
  void updateBindingButtons();

  KitsuClient  *m_client   = nullptr;
  QLineEdit    *m_urlEdit  = nullptr;
  QLineEdit    *m_emailEdit = nullptr;
  QLineEdit    *m_pwdEdit  = nullptr;
  QCheckBox    *m_savePwd  = nullptr;
  QPushButton  *m_connectBtn = nullptr;
  QLabel       *m_statusLabel = nullptr;
  QComboBox    *m_projectCombo = nullptr;
  QPushButton  *m_linkBtn   = nullptr;
  QPushButton  *m_createBtn = nullptr;
  QPushButton  *m_pushShotsBtn = nullptr;
  QTableWidget *m_statusTable  = nullptr;
};
