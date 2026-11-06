#include "ztorycharacterreview.h"

#include "ztorycharacter.h"
#include "ztorymodel.h"
#include "ztoryshotops.h"
#include "ztorytaskflow.h"
#include "kitsuclient.h"
#include "tapp.h"

#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/icongenerator.h"

#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QObject>
#include <QPixmap>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QTimer>

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
  const Asset a      = *found;  // a copy: the model may change under the dialogs
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
