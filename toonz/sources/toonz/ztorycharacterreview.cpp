#include "ztorycharacterreview.h"

#include "ztorycharacter.h"
#include "ztorymodel.h"
#include "ztoryshotops.h"
#include "ztorytaskflow.h"
#include "kitsuclient.h"
#include "kitsuconnectdialog.h"
#include "tapp.h"

#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"
#include "tfilepath.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QMainWindow>
#include <QObject>
#include <QPixmap>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QTimer>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>
#include <QCryptographicHash>
#include <QSettings>
#include <memory>

namespace {

// The current frame of the scene's top xsheet, 1280 wide on the camera's
// aspect, saved in <project>/previews/ (the scene's folder without a
// project). Returns the file, or empty on failure.
QString renderPreview(ToonzScene *scene, const Asset &a, const QString &task) {
  TXsheet *top = scene->getTopXsheet();
  // The frame on screen, if it is the top xsheet's; from inside a sub-scene
  // the current row belongs to the child, so the first frame is used.
  TApp *app = TApp::instance();
  int row   = app->getCurrentXsheet()->getXsheet() == top
                  ? app->getCurrentFrame()->getFrame()
                  : 0;
  if (row < 0 || row >= top->getFrameCount()) row = 0;
  const int w   = 1280;
  const int h =
      qMax(1, qRound(w / ZtoryShotOps::xsheetCameraAspect(top)));
  const QPixmap px = IconGenerator::renderXsheetFrame(top, row, TDimension(w, h));
  if (px.isNull()) return QString();

  const QString db = ZtoryModel::instance()->projectDbPath();
  const QString base =
      db.isEmpty()
          ? QFileInfo(QString::fromStdWString(
                          scene->getScenePath().getWideString()))
                .absolutePath()
          : QFileInfo(db).absolutePath();
  const QString dir = base + "/previews";
  if (!QDir().mkpath(dir)) return QString();
  // A name is not a file name: «/» breaks the path, «:» and «?» are not
  // allowed on Windows.
  auto safe = [](QString n) {
    return n.replace(QRegularExpression("[^A-Za-z0-9_-]"), "_");
  };
  const QString file =
      dir + "/" + safe(a.name) + "_" + safe(task) + "_" +
      QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".png";
  return px.save(file, "PNG") ? file : QString();
}

// Uploads `png` to the asset's Kitsu task, in a comment that sets WFA there.
// Waits for the answer (bounded): the scene — or the whole app — is closing,
// and an upload left running would be cut off. Returns true if it arrived.
bool uploadAndWait(const Asset &a, const QString &task, const QString &png,
                   QString *message) {
  // The scene is closing: no question here. Maybe a copy of the project, so
  // nothing leaves on its own (ZtoryKitsuSync::confirmProjectFolder).
  if (!ZtoryModel::instance()->kitsuLinkFolderMatches()) {
    *message = QObject::tr("Not sent: this project was linked to Kitsu from "
                           "another folder. A Sync asks about it.");
    return false;
  }
  KitsuClient *kc = KitsuClient::instance();
  QProgressDialog progress(
      QObject::tr("Sending the preview of %1 to Kitsu…").arg(a.name),
      QString(), 0, 0);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);
  progress.show();

  bool done = false, ok = false;
  int token = -1;  // the answer is ours only if it carries our token
  QEventLoop loop;
  const QMetaObject::Connection c = QObject::connect(
      kc, &KitsuClient::reviewPreviewUploaded, &loop,
      [&](int tok, bool okUpload, const QString &msg) {
        if (tok != token) return;
        ok       = okUpload;
        *message = msg;
        done     = true;
        loop.quit();
      });
  // Closing the progress window stops the wait: the task stays in WIP.
  QObject::connect(&progress, &QProgressDialog::canceled, &loop,
                   &QEventLoop::quit);
  QTimer::singleShot(90000, &loop, &QEventLoop::quit);
  token = kc->uploadReviewPreview(1, a.kitsuAssetId, task, png,
                                  TaskStatus::Wfa);
  if (!done) loop.exec();
  QObject::disconnect(c);
  progress.close();
  if (!done) *message = QObject::tr("no answer from Kitsu");
  return done && ok;
}

}  // namespace

namespace ZtoryCharacterReview {

// The Kitsu leg: WFA goes with its preview or not at all. Returns why it did
// not go, or empty if it did.
static QString sendViaKitsu(const Asset &a, const QString &task,
                            const QString &png) {
  if (!KitsuClient::instance()->isLoggedIn())
    return QObject::tr("not connected to Kitsu");
  if (a.kitsuAssetId.isEmpty())
    return QObject::tr("%1 is not on Kitsu yet").arg(a.name);
  QString why;
  if (!uploadAndWait(a, task, png, &why))
    return why.isEmpty() ? QObject::tr("upload failed") : why;
  return QString();
}

void offerWfaOnClose(bool changesDiscarded) {
  if (changesDiscarded) return;
  // The wait for Kitsu runs an event loop inside the scene's closing: a
  // second close reaching here meanwhile must not ask again.
  static bool running = false;
  if (running) return;
  struct Guard {
    bool &f;
    explicit Guard(bool &b) : f(b) { f = true; }
    ~Guard() { f = false; }
  } guard(running);
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return;
  const QString tnz =
      QString::fromStdWString(scene->getScenePath().getWideString());
  if (ZtoryCharacter::roleOf(tnz) != QLatin1String("character")) return;

  QString uuid, name;
  ZtoryCharacter::characterRef(tnz, &uuid, &name);
  ZtoryModel *m = ZtoryModel::instance();
  const Asset *found = m->assetByUuid(uuid);
  if (!found) return;
  Asset a            = *found;  // a copy: the model may change under the dialogs
  const QString task = ZtoryTaskFlow::characterSceneTask(uuid);
  if (task.isEmpty() ||
      m->taskStatusOf(static_cast<int>(ZtoryTaskFlow::Entity::Asset), uuid,
                      task) != TaskStatus::Wip)
    return;

  // «Not now» is the default: Enter pressed without reading must not send
  // anything anywhere.
  const int answer = DVGui::MsgBox(
      QObject::tr("%1 — %2 is in progress (WIP).\n\n"
                  "Send it for approval (WFA), with a preview of the "
                  "current frame?")
          .arg(a.name, task),
      QObject::tr("Send for approval"), QObject::tr("Not now"), 1);
  if (answer != 1) return;

  const QString png = renderPreview(scene, a, task);
  if (png.isEmpty()) {
    DVGui::warning(QObject::tr("The preview of %1 could not be made: the "
                               "task stays in WIP.")
                       .arg(a.name));
    return;
  }

  // A project without Kitsu: WFA here, the preview on disk.
  if (!m->useKitsu() || m->kitsuProjectId().isEmpty()) {
    ZtoryTaskFlow::setStatus(ZtoryTaskFlow::Entity::Asset, uuid, task,
                             TaskStatus::Wfa, ZtoryTaskFlow::Origin::App);
    DVGui::info(QObject::tr("%1 — %2 is waiting for approval.\nPreview:\n%3")
                    .arg(a.name, task, png));
    return;
  }

  // Said yes but not connected: offer the connection instead of failing.
  // The dialog logs in by itself when the password is saved.
  if (!KitsuClient::instance()->isLoggedIn()) {
    const int connectNow = DVGui::MsgBox(
        QObject::tr("You are not connected to Kitsu, so %1 cannot be sent "
                    "for approval yet.\n\nConnect now?")
            .arg(a.name),
        QObject::tr("Connect…"), QObject::tr("Not now"), 0);
    if (connectNow == 1) {
      KitsuConnectDialog dlg(TApp::instance()->getMainWindow());
      dlg.exec();
      // Connecting can link the asset to Kitsu meanwhile: take its id from
      // the model, not from the copy made before the dialog.
      const Asset *now = m->assetByUuid(uuid);
      if (!now) return;
      a.kitsuAssetId = now->kitsuAssetId;
    }
  }

  const QString why = sendViaKitsu(a, task, png);
  if (!why.isEmpty()) {
    DVGui::warning(QObject::tr("Not sent for approval: %1.\nThe task stays in "
                               "WIP; the preview is saved in:\n%2")
                       .arg(why, png));
    return;
  }
  // The comment set WFA on Kitsu: mirrored here as a raw write, not a
  // transition — the automatic push must not send it again.
  ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Asset, uuid, task,
                            TaskStatus::Wfa);
  ZtoryTaskFlow::applyFromServer(ZtoryTaskFlow::Entity::Asset, uuid, task,
                                 TaskStatus::Wfa);
}

}  // namespace ZtoryCharacterReview

namespace ZtoryShotReview {

namespace {
// The upload's own box, at the bottom right of the main window, like the
// Animatic's: it shows the bytes going up and stays through scene and
// project changes (the main window does). Green and gone after a few seconds
// when it arrives; red, and left open, when it does not.
struct UploadBox {
  QPointer<QFrame> frame;
  QLabel *title = nullptr;
  // Not a QLabel: an error carries a long path, which a label cut off at the
  // bottom of the window (Franco, 2026-11-25). Scrolls, wraps inside paths,
  // and can be selected and copied.
  QPlainTextEdit *line = nullptr;
  QProgressBar *bar = nullptr;
};

UploadBox showUploadBox(const QString &what) {
  UploadBox b;
  QWidget *mw = TApp::instance()->getMainWindow();
  if (!mw) return b;
  QFrame *box = new QFrame(mw);
  box->setAttribute(Qt::WA_DeleteOnClose);
  box->setObjectName("ztoryShotUploadBox");
  box->setStyleSheet(
      "#ztoryShotUploadBox{background:#2a2a2a;border:1px solid #5a8fd0;}"
      "QLabel{color:#ddd;font-size:11px;border:none;}");
  auto *lay = new QVBoxLayout(box);
  lay->setContentsMargins(10, 8, 10, 8);
  auto *top = new QHBoxLayout();
  b.title   = new QLabel(QObject::tr("Uploading to Kitsu — %1").arg(what), box);
  b.title->setStyleSheet("font-weight:bold;");
  QToolButton *closeBtn = new QToolButton(box);
  closeBtn->setText(QString::fromUtf8("\u00d7"));
  closeBtn->setToolTip(QObject::tr("Hide (the upload goes on)"));
  closeBtn->setStyleSheet("QToolButton{color:#aaa;border:none;font-size:14px;}"
                          "QToolButton:hover{color:#fff;}");
  top->addWidget(b.title);
  top->addStretch(1);
  top->addWidget(closeBtn);
  lay->addLayout(top);
  b.bar = new QProgressBar(box);
  b.bar->setRange(0, 0);  // busy until the file starts going up
  b.bar->setTextVisible(false);
  b.bar->setFixedHeight(6);
  lay->addWidget(b.bar);
  b.line = new QPlainTextEdit(QObject::tr("Preparing…"), box);
  b.line->setReadOnly(true);
  b.line->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  b.line->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  b.line->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  b.line->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  b.line->setFrameShape(QFrame::NoFrame);
  b.line->setStyleSheet(
      "QPlainTextEdit{background:transparent;color:#ddd;font-size:11px;}");
  b.line->setFixedHeight(40);
  lay->addWidget(b.line);
  box->setFixedWidth(320);
  box->adjustSize();
  box->move(mw->width() - box->width() - 16, mw->height() - box->height() - 40);
  box->show();
  box->raise();
  QObject::connect(closeBtn, &QToolButton::clicked, box, &QWidget::close);
  b.frame = box;
  return b;
}

void finishUploadBox(const UploadBox &b, bool ok, const QString &title,
                     const QString &text) {
  if (!b.frame) return;  // hidden by the user meanwhile
  b.title->setText(title);
  b.title->setStyleSheet(ok ? "font-weight:bold;color:#22D160;"
                            : "font-weight:bold;color:#FF3860;");
  b.bar->setRange(0, 1);
  b.bar->setValue(1);
  b.line->setPlainText(text);
  // Tall enough for the message, up to 120 px; beyond that it scrolls.
  b.line->setFixedHeight(qMin(120, int(b.line->document()->size().height() *
                                        b.line->fontMetrics().lineSpacing()) +
                                       12));
  b.frame->adjustSize();
  if (QWidget *mw = b.frame->parentWidget())
    b.frame->move(mw->width() - b.frame->width() - 16,
                  mw->height() - b.frame->height() - 40);
  if (ok) QTimer::singleShot(5000, b.frame.data(), &QWidget::close);
}
}  // namespace

static const char *kRenderUploads = "ZtorycRenderUploads";

static QString renderUploadKey(const QString &file) {
  return QString::fromLatin1(
      QCryptographicHash::hash(file.toUtf8(), QCryptographicHash::Md5).toHex());
}

void setRenderUpload(const QString &file, const QString &shotUuid,
                     const QString &taskType) {
  QSettings s;
  s.beginGroup(kRenderUploads);
  const QString key = renderUploadKey(file);
  if (taskType.isEmpty() || shotUuid.isEmpty()) {
    s.remove(key);
  } else {
    s.setValue(key, QStringList() << file << shotUuid << taskType);
  }
  s.endGroup();
}

// The marked output that `rendered` is a render of: {file, shot, task}, or
// empty. `cleanName` gets the name actually written, without «#RENDERID…»
// (a sequence/timestamp suffix stays: the name only has to start alike).
static QStringList findRenderUpload(const TFilePath &rendered,
                                    QString *cleanName) {
  QString name = QString::fromStdWString(rendered.getWideName());
  const int rid = name.indexOf("#RENDERID");
  if (rid != -1) name = name.left(rid);
  if (cleanName) *cleanName = name;
  QSettings s;
  s.beginGroup(kRenderUploads);
  for (const QString &key : s.childKeys()) {
    const QStringList v = s.value(key).toStringList();
    if (v.size() != 3) continue;
    const TFilePath want(v[0].toStdWString());
    if (rendered.getParentDir() == want.getParentDir() &&
        rendered.getType() == want.getType() &&
        name.startsWith(QString::fromStdWString(want.getWideName())))
      return v;
  }
  return QStringList();
}

void renderStarting(const TFilePath &fp) {
  if (!findRenderUpload(fp, nullptr).isEmpty()) ensureConnected();
}

void renderCompleted(const TFilePath &rendered) {
  QString name;
  const QStringList v = findRenderUpload(rendered, &name);
  if (v.isEmpty()) return;
  const QString file = QString::fromStdWString(
      rendered.withName(name.toStdWString()).getWideString());
  uploadShotRender(file, v[1], v[2]);
}

bool ensureConnected() {
  KitsuClient *kc = KitsuClient::instance();
  if (kc->isLoggedIn()) return true;
  const int answer = DVGui::MsgBox(
      QObject::tr("You are not connected to Kitsu.\n\nConnect now?"),
      QObject::tr("Connect…"), QObject::tr("Not now"), 0);
  if (answer != 1) return false;
  KitsuConnectDialog dlg(TApp::instance()->getMainWindow());
  dlg.exec();
  return kc->isLoggedIn();
}

void uploadShotRender(const QString &file, const QString &shotUuid,
                      const QString &taskType) {
  ZtoryModel *m = ZtoryModel::instance();
  const int i   = m->projectShotIndex(shotUuid);
  if (i < 0) return;
  const ProjectShot ps = m->projectShots()[i];
  const QString what   = QObject::tr("%1 — %2").arg(ps.label, taskType);
  auto notSent = [&](const QString &why) {
    DVGui::warning(QObject::tr("%1: the render was not sent to Kitsu (%2).\n"
                               "It is saved in:\n%3")
                       .arg(what, why, file));
  };
  if (!m->useKitsu() || m->kitsuProjectId().isEmpty())
    return notSent(QObject::tr("the project is not linked to Kitsu"));
  if (ps.kitsuShotId.isEmpty())
    return notSent(QObject::tr("the shot is not on Kitsu yet"));
  KitsuClient *kc = KitsuClient::instance();
  // Asked before the render too; the connection can have dropped meanwhile.
  if (!ensureConnected())
    return notSent(QObject::tr("not connected to Kitsu"));
  if (!m->kitsuLinkFolderMatches())
    return notSent(QObject::tr(
        "this project was linked to Kitsu from another folder; a Sync asks "
        "about it"));

  // Asynchronous: the app stays open, and a movie takes a while to go up.
  // The answer is ours only if it carries our token.
  // It goes on in the background whatever is opened meanwhile: it carries
  // only Kitsu ids and the file. The tracker it reports to is the one open
  // NOW, remembered, so a project opened meanwhile is not written into.
  QObject *ctx = new QObject(kc);
  auto token   = std::make_shared<int>(-1);
  const QString uuid = shotUuid, task = taskType;
  const QString projectDb = m->projectDbPath();
  const UploadBox box     = showUploadBox(what);
  QObject::connect(
      kc, &KitsuClient::reviewPreviewProgress, ctx,
      [token, box](int tok, qint64 sent, qint64 total) {
        if (tok != *token || !box.frame || total <= 0) return;
        box.bar->setRange(0, 1000);
        box.bar->setValue(int(sent * 1000 / total));
        box.line->setPlainText(QObject::tr("%1 of %2 MB")
                              .arg(sent / 1048576.0, 0, 'f', 1)
                              .arg(total / 1048576.0, 0, 'f', 1));
      });
  QObject::connect(
      kc, &KitsuClient::reviewPreviewUploaded, ctx,
      [ctx, token, uuid, task, what, file, projectDb, box](
          int tok, bool ok, const QString &msg) {
        if (tok != *token) return;
        ctx->deleteLater();
        if (!ok) {
          finishUploadBox(box, false, QObject::tr("Kitsu upload stopped"),
                          QObject::tr("%1: %2\nThe render is saved in:\n%3")
                              .arg(what, msg, file));
          return;
        }
        if (ZtoryModel::instance()->projectDbPath() != projectDb) {
          // Another project is open now: its tracker is not touched; this
          // shot's gets WFA from Kitsu at its next Sync.
          finishUploadBox(box, true, QObject::tr("On Kitsu, waiting for approval"),
                          QObject::tr("%1. Its project gets the status at the "
                                      "next Sync.")
                              .arg(what));
          return;
        }
        // The comment set WFA on Kitsu: mirrored as a raw write, not a
        // transition — the automatic push must not send it again.
        ZtoryTaskFlow::markSynced(ZtoryTaskFlow::Entity::Shot, uuid, task,
                                  TaskStatus::Wfa);
        ZtoryTaskFlow::applyFromServer(ZtoryTaskFlow::Entity::Shot, uuid,
                                       task, TaskStatus::Wfa);
        finishUploadBox(box, true, QObject::tr("On Kitsu, waiting for approval"),
                        what);
      });
  QObject::connect(kc, &KitsuClient::reviewPreviewSkipped, ctx,
                   [ctx, token, what, file, box](int tok, const QString &why) {
                     if (tok != *token) return;
                     ctx->deleteLater();
                     finishUploadBox(box, false, QObject::tr("Not sent to Kitsu"),
                                     QObject::tr("%1: %2\nThe render is saved "
                                                 "in:\n%3")
                                         .arg(what, why, file));
                   });
  *token = kc->uploadReviewPreview(0, ps.kitsuShotId, task, file,
                                   TaskStatus::Wfa,
                                   QObject::tr("Render from Ztoryc"));
}

}  // namespace ZtoryShotReview
