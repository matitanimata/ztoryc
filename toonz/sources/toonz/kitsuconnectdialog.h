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
#include <QVector>

#include "kitsuclient.h"  // KitsuClient, KitsuTaskPush

class QLineEdit;
class QCheckBox;
class QPushButton;
class QComboBox;
class QLabel;
class QTableWidget;
class QSpinBox;

class KitsuConnectDialog final : public QDialog {
  Q_OBJECT
public:
  explicit KitsuConnectDialog(QWidget *parent = nullptr);

private:
  void onConnectClicked();
  void onLinkClicked();    // bind the selected Kitsu project (pull)
  void onCreateClicked();  // create a new Kitsu project from the Ztoryc model (push)
  void onPushShotsClicked();  // push the Ztoryc shot list up to Kitsu
  void onPullStatusClicked(); // pull task statuses down from Kitsu (review sync)
  void onUploadPreviewsClicked();  // upload per-shot preview movies to Kitsu
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
  QPushButton  *m_pullStatusBtn = nullptr;
  QPushButton  *m_uploadPrevBtn = nullptr;
  QCheckBox    *m_handlesCheck = nullptr;  // add safety handles on push
  QSpinBox     *m_handlesSpin  = nullptr;
  // Task statuses to push right after the shots land (Phase 3b).
  QVector<KitsuTaskPush> m_pendingTasks;
  // (projectShot uuid, taskType) of the previews being uploaded — applied as
  // WFA in the local tracker once previewsUploaded() reports success.
  QVector<QPair<QString, QString>> m_pendingPreviewStatus;
};
