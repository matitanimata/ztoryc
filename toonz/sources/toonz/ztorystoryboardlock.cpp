#include "ztorystoryboardlock.h"

// Ztoryc includes
#include "ztorymodel.h"
#include "kitsuclient.h"
#include "iocommand.h"
#include "tapp.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "tmsgcore.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/levelset.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshleveltypes.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>

namespace {

int s_bypass = 0;

QString tr(const char *text) {
  return QCoreApplication::translate("ZtoryStoryboardLock", text);
}

ToonzScene *currentScene() {
  return TApp::instance()->getCurrentScene()->getScene();
}

QString sidecarOf(const TFilePath &scenePath) {
  QString path = scenePath.getQString();
  path.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
  return path;
}

// The lock lives in the scene's own .ztoryc, «<numbering … locked="1">».
// Read from there rather than from the model, which holds whatever the last
// storyboard left in it: a scene without the setting must never look locked.
bool lockedOnDisk(const QString &sidecar) {
  struct Cache {
    QString path;
    QDateTime modified;
    bool locked = false;
  };
  static Cache cache;
  const QFileInfo info(sidecar);
  if (!info.exists()) return false;
  if (cache.path == sidecar && cache.modified == info.lastModified())
    return cache.locked;
  bool locked = false;
  QFile file(sidecar);
  if (file.open(QIODevice::ReadOnly)) {
    static const QRegularExpression re("<numbering\\b[^>]*\\blocked=\"1\"");
    locked = re.match(QString::fromUtf8(file.readAll())).hasMatch();
  }
  cache = {sidecar, info.lastModified(), locked};
  return locked;
}

QString storyboardTaskOf(const ZtoryModel *m, const ProjectShot &ps) {
  const QString wanted = KitsuClient::normalizeTaskType("Storyboard");
  for (const QString &tt : m->taskTypesForProjectShot(ps))
    if (KitsuClient::normalizeTaskType(tt) == wanted) return tt;
  return QString();
}

// Copies every drawing level of the scene at `src` into a folder of the new
// version and saves the copy at `dst`: the new version must not draw on the
// locked storyboard's files. Sound and other references stay shared.
bool copyVersion(const TFilePath &src, const TFilePath &dst,
                 const QString &oldName, const QString &newName) {
  std::vector<TFilePath> written;
  try {
    ToonzScene copy;
    copy.load(src);
    TLevelSet *levels = copy.getLevelSet();
    for (int i = 0; i < levels->getLevelCount(); i++) {
      TXshSimpleLevel *sl = levels->getLevel(i)->getSimpleLevel();
      if (!sl) continue;
      const int type = sl->getType();
      if (type != PLI_XSHLEVEL && type != TZP_XSHLEVEL && type != OVL_XSHLEVEL)
        continue;
      const TFilePath from = copy.decodeFilePath(sl->getPath());
      if (from.getType() == "psd" || !TSystem::doesExistFileOrLevel(from))
        continue;
      // drawings/<old>/A.tlv → drawings/<new>/A.tlv; elsewhere a subfolder.
      const TFilePath parent = from.getParentDir();
      const TFilePath toDir =
          parent.getWideName() == oldName.toStdWString()
              ? parent.getParentDir() + TFilePath(newName.toStdWString())
              : parent + TFilePath(newName.toStdWString());
      const TFilePath to = toDir + from.withoutParentDir();
      if (TSystem::doesExistFileOrLevel(to))
        throw TException(L"The new version's drawing already exists: " +
                         to.getWideString());
      TXshSimpleLevel::copyFiles(to, from);
      written.push_back(to);
      sl->setPath(copy.codeFilePath(to));
    }
    copy.save(dst);
    written.push_back(dst);

    // The .ztoryc follows, unlocked.
    QFile in(sidecarOf(src));
    if (in.open(QIODevice::ReadOnly)) {
      QString text = QString::fromUtf8(in.readAll());
      text.remove(QRegularExpression("\\s+locked=\"1\""));
      QFile out(sidecarOf(dst));
      if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
          out.write(text.toUtf8()) < 0)
        throw TException(L"Cannot write the new version's .ztoryc.");
    }
  } catch (const TException &e) {
    for (const TFilePath &fp : written) TXshSimpleLevel::removeFiles(fp);
    DVGui::error(tr("The new version could not be created:\n%1")
                     .arg(QString::fromStdWString(e.getMessage())));
    return false;
  } catch (...) {
    for (const TFilePath &fp : written) TXshSimpleLevel::removeFiles(fp);
    DVGui::error(tr("The new version could not be created."));
    return false;
  }
  return true;
}

}  // namespace

//=============================================================================

namespace ZtoryStoryboardLock {

bool isLocked() {
  ToonzScene *scene = currentScene();
  if (!scene || scene->isUntitled()) return false;
  const bool locked =
      lockedOnDisk(sidecarOf(scene->decodeFilePath(scene->getScenePath())));
  // Keep the model in step, so that a save writes what the disk says.
  NumberingConfig &cfg = ZtoryModel::instance()->numberingConfig();
  if (cfg.storyboardLocked != locked && s_bypass == 0)
    cfg.storyboardLocked = locked;
  return locked;
}

bool refuse(bool silent) {
  if (s_bypass > 0 || !isLocked()) return false;
  if (!silent)
    DVGui::warning(
        tr("This storyboard is locked (approved): it can only be exported.\n\n"
           "To change it, unlock it from the lock button in the Board: a new "
           "version will be created, and this one stays as it is."));
  return true;
}

QStringList shotsNotApproved() {
  const ZtoryModel *m = ZtoryModel::instance();
  QStringList missing;
  for (const ShotData &sd : m->shots()) {
    const ProjectShot *found = nullptr;
    for (const ProjectShot &ps : m->projectShots())
      if (ps.uuid == sd.uuid) {
        found = &ps;
        break;
      }
    bool approved = false;
    if (found) {
      const QString task = storyboardTaskOf(m, *found);
      bool has           = false;
      approved           = !task.isEmpty() &&
                 m->taskStatusOf(0, sd.uuid, task, &has) == TaskStatus::Done &&
                 has;
    }
    if (!approved) missing << sd.label();
  }
  return missing;
}

bool lock(QWidget *) {
  ZtoryModel *m     = ZtoryModel::instance();
  ToonzScene *scene = currentScene();
  if (!scene || scene->isUntitled() || m->shots().empty()) {
    DVGui::warning(tr("Only a saved storyboard with shots can be locked."));
    return false;
  }
  const QStringList missing = shotsNotApproved();
  if (!missing.isEmpty()) {
    DVGui::warning(
        tr("The storyboard can be locked once the Storyboard task of every "
           "shot is approved.\n\nNot approved yet: %1")
            .arg(missing.join(", ")));
    return false;
  }
  const int answer = DVGui::MsgBox(
      tr("Lock the storyboard?\n\nFrom now on this file can only be exported: "
         "shots cannot be entered and nothing can be saved over it. Unlocking "
         "will create a new version."),
      tr("Lock"), tr("Cancel"), 0);
  if (answer != 1) return false;

  NumberingConfig &cfg = m->numberingConfig();
  cfg.storyboardLocked = true;
  m->setAutoRenumber(false);
  bool saved = false;
  {
    SaveBypass bypass;
    saved = IoCmd::saveScene(0);
  }
  if (!saved || !isLocked()) {
    cfg.storyboardLocked = false;
    DVGui::warning(tr("The storyboard could not be saved locked."));
    return false;
  }
  return true;
}

bool unlock(QWidget *parent) {
  ToonzScene *scene = currentScene();
  if (!scene || scene->isUntitled()) return false;
  const QString name = QString::fromStdWString(scene->getSceneName());
  bool ok            = false;
  const QString typed =
      QInputDialog::getText(
          parent, tr("Unlock Storyboard"),
          tr("This storyboard is locked (approved). Unlocking does not change "
             "it: a new version is created, unlocked, with the same shots in "
             "Keep # numbering.\n\nType the file name to confirm:  %1")
              .arg(name),
          QLineEdit::Normal, QString(), &ok)
          .trimmed();
  if (!ok) return false;
  if (typed != name) {
    DVGui::warning(tr("The name does not match: the storyboard stays locked."));
    return false;
  }

  // name → name_v2; name_v2 → name_v3; skipping versions that exist.
  const TFilePath src = scene->decodeFilePath(scene->getScenePath());
  QString base        = name;
  int number          = 2;
  const QRegularExpressionMatch match =
      QRegularExpression("^(.*)_v(\\d+)$").match(name);
  if (match.hasMatch()) {
    base   = match.captured(1);
    number = match.captured(2).toInt() + 1;
  }
  QString newName;
  TFilePath dst;
  for (;; number++) {
    newName = base + "_v" + QString::number(number);
    dst     = src.getParentDir() + TFilePath((newName + ".tnz").toStdWString());
    if (!TFileStatus(dst).doesExist() && !QFile::exists(sidecarOf(dst))) break;
  }
  if (!copyVersion(src, dst, name, newName)) return false;

  // The new version takes over the shots in the Production Tracker (same
  // uuids, same Kitsu ids): otherwise it would read as a copy of the old one.
  // The tracker names a storyboard by its sidecar («name.ztoryc»); the .tnz
  // form is moved too, for entries written by older versions.
  ZtoryModel *m = ZtoryModel::instance();
  m->renameStoryboardSource(QFileInfo(sidecarOf(src)).fileName(),
                            QFileInfo(sidecarOf(dst)).fileName());
  m->renameStoryboardSource(name + ".tnz", newName + ".tnz");
  m->saveProjectDb();

  IoCmd::loadScene(dst);
  m->setAutoRenumber(false);
  DVGui::info(tr("Unlocked as a new version: %1\nThe locked storyboard %2 "
                 "stays as it was.")
                  .arg(newName, name));
  return true;
}

SaveBypass::SaveBypass() { ++s_bypass; }
SaveBypass::~SaveBypass() { --s_bypass; }
bool SaveBypass::active() { return s_bypass > 0; }

}  // namespace ZtoryStoryboardLock
