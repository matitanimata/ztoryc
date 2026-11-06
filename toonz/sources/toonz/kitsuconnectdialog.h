#pragma once

//============================================================================
// KitsuConnectDialog — M5 connection + project-binding UI for Kitsu.
//
// Scope is deliberately narrow: server settings (URL + optional LAN upload URL
// + email + password), a Connect button, and — after a successful JWT login —
// the list of open projects with Link / Create-new to bind one to this Ztoryc
// project. The recurring sync actions (push/pull shots, statuses, assets and
// preview upload) live in the Production Tracker's Project tab, not here.
//============================================================================

#include <QDialog>

#include "kitsuclient.h"  // KitsuClient

class QLineEdit;
class QCheckBox;
class QPushButton;
class QComboBox;
class QLabel;

class KitsuConnectDialog final : public QDialog {
  Q_OBJECT
public:
  explicit KitsuConnectDialog(QWidget *parent = nullptr);

private:
  void onConnectClicked();
  void onLinkClicked();    // bind the selected Kitsu project (pull)
  void onCreateClicked();  // create a new Kitsu project from the Ztoryc model (push)
  void setBusy(bool busy);
  void updateBindingButtons();
  // Moving a project that is already bound to another Kitsu production or
  // episode. A wrong row there mixes two episodes in one tracker, and the Sync
  // only ever adds: so the user must type the name of the new target.
  // True when there is nothing bound yet, or the user typed it and confirmed.
  bool confirmRebind(const QString &newTarget, const QString &typeThis);
  // Fill the production list. A tvshow is listed once PER EPISODE, because a
  // Ztoryc project maps to one episode and not to the whole show; everything
  // else keeps a single row. Each row carries the project id in Qt::UserRole
  // and the episode id (empty when there is none) in Qt::UserRole + 1.
  void rebuildProjectCombo();
  void updateConnectButton();  // hidden while connected with these very fields

  KitsuClient  *m_client   = nullptr;
  QLineEdit    *m_urlEdit  = nullptr;
  QLineEdit    *m_localUrlEdit = nullptr;
  QLineEdit    *m_emailEdit = nullptr;
  QLineEdit    *m_pwdEdit  = nullptr;
  QCheckBox    *m_savePwd  = nullptr;
  QPushButton  *m_connectBtn = nullptr;
  QLabel       *m_statusLabel = nullptr;
  QLabel       *m_bindingLabel = nullptr;  // which production this project writes to
  QString       m_loginPassword;  // what the password field held at login
  QComboBox    *m_projectCombo = nullptr;
  QPushButton  *m_linkBtn   = nullptr;
  QPushButton  *m_createBtn = nullptr;
};
