#include "ztoryrecovery.h"

#include "iocommand.h"
#include "tapp.h"
#include "ztorymodel.h"

#include "tools/toolhandle.h"
#include "toonz/cleanupparameters.h"
#include "toonz/levelproperties.h"
#include "toonz/levelset.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonzqt/dvdialog.h"
#include "tpalette.h"
#include "tsystem.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

const char *kManifest = "manifest.xml";

QString toQ(const TFilePath &fp) {
  return QString::fromStdWString(fp.getWideString());
}

//! La cartella di recupero di UNA scena: nome leggibile + impronta del
//! percorso completo, perche' due scene con lo stesso nome in cartelle
//! diverse non si pestino i piedi.
QString dirFor(const QString &decodedScenePath) {
  const QByteArray h = QCryptographicHash::hash(decodedScenePath.toUtf8(),
                                                QCryptographicHash::Sha1)
                           .toHex()
                           .left(10);
  return ZtoryRecovery::rootFolder() + "/" +
         QFileInfo(decodedScenePath).completeBaseName() + "_" +
         QString::fromLatin1(h);
}

QString currentScenePath() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return QString();
  return toQ(scene->decodeFilePath(scene->getScenePath()));
}

//! Vero se c'e' ancora lavoro solo in memoria nei livelli: un livello o la
//! sua palette modificati. La scena puo' essere pulita (Save Scene salva solo
//! il .tnz) mentre i livelli no.
bool anyLevelDirty(ToonzScene *scene) {
  if (!scene) return false;
  TLevelSet *ls = scene->getLevelSet();
  for (int i = 0; i < ls->getLevelCount(); i++) {
    TXshLevel *lv       = ls->getLevel(i);
    TXshSimpleLevel *sl = lv ? lv->getSimpleLevel() : nullptr;
    if (!sl) continue;
    if (sl->getProperties()->getDirtyFlag()) return true;
    if (sl->getPalette() && sl->getPalette()->getDirtyFlag()) return true;
  }
  return false;
}

//! Where a recovery offered and left unanswered (Esc) waits for the next
//! opening of its scene.
QString laterDir(const QString &dir) { return dir + "_later"; }

void removeDir(const QString &dir) {
  if (!dir.isEmpty() && QDir(dir).exists()) QDir(dir).removeRecursively();
}

//! Copia \p src su \p dst, mettendo prima da parte in \p backupDir cio' che
//! \p dst conteneva. Mai sovrascrivere senza una copia.
bool replaceFile(const QString &src, const QString &dst,
                 const QString &backupDir) {
  if (QFile::exists(dst)) {
    QDir().mkpath(backupDir);
    const QString bak = backupDir + "/" + QFileInfo(dst).fileName();
    QFile::remove(bak);
    if (!QFile::copy(dst, bak)) return false;
    if (!QFile::remove(dst)) return false;
  } else {
    QDir().mkpath(QFileInfo(dst).absolutePath());
  }
  return QFile::copy(src, dst);
}

}  // namespace

//============================================================================

ZtoryRecovery *ZtoryRecovery::instance() {
  static ZtoryRecovery *s = new ZtoryRecovery();
  return s;
}

ZtoryRecovery::ZtoryRecovery() : QObject(nullptr) {}

QString ZtoryRecovery::rootFolder() {
  // On this computer, in Ztoryc's own data folder (Franco, 2026-10-24) — the
  // one that holds the profiles on macOS, since the stuff folder sits inside
  // the app bundle there (and under Program Files with the Windows
  // installer), where we cannot write. It used to be inside the project:
  // on Google Drive that meant a folder deleted and recreated every three
  // minutes on a streaming disk, synced to every computer sharing it, and a
  // write Windows refused (Messina, Crash-20261024-165757). The position is
  // here, and only here.
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (base.isEmpty()) base = QDir::tempPath();  // never "/recovery" at the root
  return base + "/recovery";
}

void ZtoryRecovery::start() {
  if (m_timer) return;
  m_timer = new QTimer(this);
  // Tre minuti: abbastanza spesso da non perdere un pomeriggio, abbastanza
  // di rado da non farsi sentire. ZTORYC_RECOVERY_SECONDS per provarlo.
  int seconds = qEnvironmentVariableIntValue("ZTORYC_RECOVERY_SECONDS");
  if (seconds <= 0) seconds = 180;
  m_timer->setInterval(seconds * 1000);
  connect(m_timer, &QTimer::timeout, this, &ZtoryRecovery::snapshot);
  m_timer->start();

  connect(ZtoryModel::instance(), &ZtoryModel::sceneSaved, this,
          &ZtoryRecovery::onSceneSaved);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryRecovery::onSceneSwitched);
  connect(qApp, &QCoreApplication::aboutToQuit, this,
          &ZtoryRecovery::onAboutToQuit);
  connect(TApp::instance()->getCurrentTool(), &ToolHandle::toolEditingFinished,
          this, &ZtoryRecovery::onToolEditingFinished);
}

//----------------------------------------------------------------------------

void ZtoryRecovery::snapshot() {
  if (m_busy) return;
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return;
  if (!app->getCurrentScene()->getDirtyFlag() && !anyLevelDirty(scene)) return;
  // Mai a meta' di un gesto o di un salvataggio: si riprova al giro dopo.
  if (app->getCurrentTool()->isToolBusy() || app->isSaveInProgress()) {
    m_pending = true;  // riprova appena il gesto finisce
    return;
  }
  m_pending = false;

  const QString scenePath = currentScenePath();
  if (scenePath.isEmpty()) return;

  m_busy = true;
  QElapsedTimer clock;
  clock.start();

  const QString dir = dirFor(scenePath);
  const QString tmp = dir + ".tmp";
  removeDir(tmp);
  QDir().mkpath(tmp + "/levels");
  QDir().mkpath(tmp + "/scene");

  struct Entry {
    QString src, dst, name;
  };
  QVector<Entry> entries;
  int failed = 0;

  // ── I livelli modificati ────────────────────────────────────────────────
  TLevelSet *ls = scene->getLevelSet();
  for (int i = 0; i < ls->getLevelCount(); i++) {
    TXshLevel *lv = ls->getLevel(i);
    TXshSimpleLevel *sl = lv ? lv->getSimpleLevel() : nullptr;
    if (!sl || sl->getPath().isEmpty()) continue;
    const bool dirty = sl->getProperties()->getDirtyFlag() ||
                       (sl->getPalette() && sl->getPalette()->getDirtyFlag());
    if (!dirty) continue;

    const TFilePath orig = scene->decodeFilePath(sl->getPath());
    const QString sub    = QString("levels/%1").arg(i);
    QDir().mkpath(tmp + "/" + sub);
    const TFilePath dst =
        TFilePath((tmp + "/" + sub).toStdWString()) + orig.withoutParentDir();
    // ⚠️ I flag «modificato» si fotografano e si RIMETTONO: la copia verso un
    // percorso diverso non li tocca in saveSimpleLevel, ma il ramo delle
    // palette legate a una studio palette (Toonz Raster) azzera quello della
    // palette senza condizioni (txshsimplelevel.cpp, «global name»). Senza
    // questo il Save All vero saltava il livello e il colore cambiato si
    // perdeva in silenzio (review del 25/09).
    const bool levelDirty = sl->getProperties()->getDirtyFlag();
    const bool palDirty   = sl->getPalette() && sl->getPalette()->getDirtyFlag();
    try {
      sl->save(dst, orig, true);
      // A writer that fails while closing can no longer throw (the .tzm one
      // used to terminate the app): it removes the file, and we look for it.
      if (!TSystem::doesExistFileOrLevel(dst)) throw TSystemException(dst, "not written");
      entries.push_back({sub, toQ(orig.getParentDir()),
                         QString::fromStdWString(sl->getName())});
    } catch (...) {
      ++failed;
      qWarning("[RECOVERY] level %s not written",
               QString::fromStdWString(sl->getName()).toUtf8().constData());
    }
    sl->getProperties()->setDirtyFlag(levelDirty);
    if (sl->getPalette()) sl->getPalette()->setDirtyFlag(palDirty);
  }

  // ── La scena ────────────────────────────────────────────────────────────
  // Stesso nome del file vero: ToonzScene::save rinomina i percorsi dei
  // livelli solo quando il NOME della scena cambia, e il .bak scatta solo
  // salvando sullo stesso percorso — qui nessuno dei due.
  bool sceneOk          = false;
  const TFilePath oldSp = scene->getScenePath();
  const TFilePath recSp = TFilePath((tmp + "/scene/").toStdWString()) +
                          TFilePath(scene->decodeFilePath(oldSp))
                              .withoutParentDir();
  CleanupParameters *cp = scene->getProperties()->getCleanupParameters();
  CleanupParameters keepCP(*cp);
  cp->assign(&CleanupParameters::GlobalParameters, false);
  app->setSaveInProgress(true);
  ToonzScene::setSkipSceneIcon(true);
  try {
    scene->save(recSp);
    sceneOk = true;
  } catch (...) {
    qWarning("[RECOVERY] scene not written");
  }
  ToonzScene::setSkipSceneIcon(false);
  scene->setScenePath(oldSp);  // save() l'ha spostata sulla copia
  app->setSaveInProgress(false);
  cp->assign(&keepCP, false);

  if (!sceneOk) {
    removeDir(tmp);
    m_busy = false;
    return;
  }

  // ── Il .ztoryc ──────────────────────────────────────────────────────────
  // Dal passo 3b si scrive solo col salvataggio della scena: i testi degli
  // shot non salvati stanno in memoria come i livelli. La copia porta anche
  // il documento, dello stesso momento del .tnz — rimettere un .tnz accanto a
  // un .ztoryc di un altro momento abbina male gli shot (una Copy e il suo
  // originale si scambiano i dati). Non per le scene shot e personaggio, che
  // hanno un sidecar loro; non con zero shot, per non rimettere un documento
  // vuoto sopra uno vero.
  QString ztorycFile;
  {
    ZtoryModel *zm                                  = ZtoryModel::instance();
    const ZtoryModel::ShotDocumentState &doc = zm->shotDocumentState();
    // Only if memory holds THIS scene's document: a snapshot taken between a
    // scene switch and the reading of its file carried empty texts, and a
    // restore would have put them over the real sidecar (review 0.17, M3).
    // The path is built as the Board builds it (StoryboardPanel::ztoryPath):
    // raw, not decoded, or the comparison could fail for a project path.
    QString loadedKey;
    if (ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene()) {
      loadedKey = toQ(sc->getScenePath());
      loadedKey.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
    }
    if (!doc.keepsOwnSidecar() && zm->shotCount() > 0 &&
        zm->shotDataLoadedFor(loadedKey)) {
      std::vector<const ShotData *> shots;
      for (int i = 0; i < zm->shotCount(); i++) shots.push_back(&zm->shot(i));
      const QString name = QFileInfo(scenePath).completeBaseName() + ".ztoryc";
      if (zm->writeShotDocument(tmp + "/scene/" + name, shots))
        ztorycFile = "scene/" + name;
      else
        qWarning("[RECOVERY] .ztoryc not written");
    }
  }

  // ── Il manifesto ────────────────────────────────────────────────────────
  {
    QFile f(tmp + "/" + kManifest);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QXmlStreamWriter xml(&f);
      xml.setAutoFormatting(true);
      xml.writeStartDocument();
      xml.writeStartElement("ztoryrecovery");
      xml.writeAttribute("version", "1");
      xml.writeAttribute("scene", scenePath);
      xml.writeAttribute("sceneFile", "scene/" + QFileInfo(toQ(recSp)).fileName());
      if (!ztorycFile.isEmpty()) xml.writeAttribute("ztorycFile", ztorycFile);
      xml.writeAttribute("time", QDateTime::currentDateTime().toString(Qt::ISODate));
      for (const Entry &e : entries) {
        xml.writeStartElement("level");
        xml.writeAttribute("name", e.name);
        xml.writeAttribute("src", e.src);
        xml.writeAttribute("dst", e.dst);
        xml.writeEndElement();
      }
      xml.writeEndElement();
      xml.writeEndDocument();
    }
  }

  // Sostituzione in un colpo solo: un crash durante la scrittura lascia il
  // recupero precedente intatto invece di uno a meta'.
  removeDir(dir);
  QDir().rename(tmp, dir);
  m_activeDir = dir;
  qWarning("[RECOVERY] snapshot %s: %d levels%s, %lld ms",
           QFileInfo(scenePath).fileName().toUtf8().constData(),
           int(entries.size()),
           failed ? QString(" (%1 failed)").arg(failed).toUtf8().constData()
                  : "",
           (long long)clock.elapsed());
  m_busy = false;
}

//----------------------------------------------------------------------------

void ZtoryRecovery::onSceneSaved() {
  // ⚠️ NON si cancella subito. sceneSaved arriva a scena scritta, ma:
  //   - Save Scene salva solo il .tnz, e i livelli modificati restano solo
  //     in memoria;
  //   - Save All lo emette PRIMA di scrivere i livelli.
  // Cancellare qui lasciava quei livelli senza copia (review del 25/09). Si
  // decide dopo, a salvataggio finito.
  QTimer::singleShot(0, this, &ZtoryRecovery::afterSceneSaved);
}

void ZtoryRecovery::afterSceneSaved() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (anyLevelDirty(scene)) {
    // Resta lavoro solo in memoria: una copia nuova, non una cancellazione.
    snapshot();
    return;
  }
  // Tutto su disco: il recupero non serve piu'. Anche quello di prima di un
  // «Salva con nome», che e' in m_activeDir.
  removeDir(m_activeDir);
  const QString sp = currentScenePath();
  if (!sp.isEmpty()) {
    removeDir(dirFor(sp));
    removeDir(laterDir(dirFor(sp)));
  }
  m_activeDir.clear();
}

void ZtoryRecovery::onSceneSwitched() {
  // Entering or leaving a shot emits sceneSwitched too, with the SAME scene
  // object (see ZtoryModel's constructor): the work is still only in memory,
  // and its snapshot must stay (review 0.19.2, B1).
  // Nor is there anything to offer: checking again would ask about a copy
  // left with Esc at every shot entered.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  const QString path = currentScenePath();
  if (scene == m_seenScene && path == m_seenScenePath) return;
  m_seenScene     = scene;
  m_seenScenePath = path;
  // Si e' lasciata la scena: salvata (gia' tolto da afterSceneSaved) o
  // scartata a domanda di Tahoma. Scartata vuol dire scartata: il recupero
  // si butta (Franco, 2026-10-24, che rivede la settimana in kept/ del 2/10).
  removeDir(m_activeDir);
  m_activeDir.clear();
  // Dopo che il caricamento e' finito del tutto.
  QTimer::singleShot(0, this, &ZtoryRecovery::checkForRecovery);
}

void ZtoryRecovery::onToolEditingFinished() {
  // Come TApp::onToolEditingFinished per l'autosave: solo se un giro e'
  // saltato. Dopo la pila di segnali del gesto, non dentro.
  if (m_pending) QTimer::singleShot(0, this, &ZtoryRecovery::snapshot);
}

void ZtoryRecovery::onAboutToQuit() {
  // Quitting with a snapshot still active means changes the user chose not
  // to save when Tahoma asked: thrown away, like the changes themselves.
  removeDir(m_activeDir);
  m_activeDir.clear();
}

//----------------------------------------------------------------------------

void ZtoryRecovery::checkForRecovery() {
  if (m_busy) return;
  const QString scenePath = currentScenePath();
  if (scenePath.isEmpty()) return;
  // A copy from a new crash wins over one set aside earlier with Esc.
  QString dir = dirFor(scenePath);
  if (!QFile::exists(dir + "/" + kManifest)) dir = laterDir(dirFor(scenePath));
  if (!QFile::exists(dir + "/" + kManifest)) return;

  QDateTime when;
  {
    QFile f(dir + "/" + kManifest);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QXmlStreamReader xml(&f);
      while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QLatin1String("ztoryrecovery"))
          when = QDateTime::fromString(
              xml.attributes().value("time").toString(), Qt::ISODate);
      }
    }
  }
  // La scena su disco e' piu' nuova del recupero (salvata altrove, da
  // un'altra macchina): il recupero e' vecchio, non si propone.
  const QDateTime saved = QFileInfo(scenePath).lastModified();
  if (!when.isValid() || saved > when) {
    removeDir(dir);
    return;
  }

  const QString sceneName = QFileInfo(scenePath).completeBaseName();
  const int answer = DVGui::MsgBox(
      tr("Ztoryc did not close properly while «%1» had unsaved changes.\n"
         "A copy of that work from %2 was kept.\n\n"
         "Recover it? The current files are set aside first, not deleted.")
          .arg(sceneName, when.toString("dd/MM HH:mm")),
      // ⚠️ Pulsanti contati da 0: Invio = Recover. Con 1 Invio era Discard,
      // che cancella il recupero — dopo un crash, il gesto d'istinto
      // (review del 25/09).
      tr("Recover"), tr("Discard"), 0);
  const QString later = laterDir(dirFor(scenePath));
  if (answer == 2) {
    // Discard: a recovery not used when the scene is reopened is thrown away
    // (Franco, 2026-10-24) — with any older copy set aside.
    removeDir(dir);
    removeDir(later);
    return;
  }
  if (answer != 1) {
    // Closed with Esc or the X: not a choice. Set aside, out of the way of
    // the next snapshot (same folder), and offered again at the next opening
    // of the scene (Franco, 2026-10-24, review 0.19.2 M2).
    if (dir != later) {
      removeDir(later);
      QDir().rename(dir, later);
    }
    return;
  }
  if (restore(dir, scenePath) && dir != later) removeDir(later);
}

bool ZtoryRecovery::restore(const QString &dir, const QString &scenePath) {
  m_busy = true;
  const QString backup =
      rootFolder() + "/replaced/" + QFileInfo(dir).fileName() + "_" +
      QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");

  QString sceneFile;
  QString ztorycFile;  // assente nei recuperi di prima del passo 3c
  QVector<QPair<QString, QString>> levels;  // src subfolder, dst folder
  {
    QFile f(dir + "/" + kManifest);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
      m_busy = false;
      return false;
    }
    QXmlStreamReader xml(&f);
    while (!xml.atEnd()) {
      xml.readNext();
      if (!xml.isStartElement()) continue;
      const QXmlStreamAttributes a = xml.attributes();
      if (xml.name() == QLatin1String("ztoryrecovery")) {
        sceneFile  = a.value("sceneFile").toString();
        ztorycFile = a.value("ztorycFile").toString();
      } else if (xml.name() == QLatin1String("level"))
        levels.push_back(
            {a.value("src").toString(), a.value("dst").toString()});
    }
  }

  bool ok = true;
  int i   = 0;
  for (const auto &lv : levels) {
    const QDir src(dir + "/" + lv.first);
    const QString bak = backup + "/levels/" + QString::number(i++);
    for (const QString &name : src.entryList(QDir::Files))
      ok = replaceFile(src.filePath(name), lv.second + "/" + name, bak) && ok;
  }
  ok = replaceFile(dir + "/" + sceneFile, scenePath, backup + "/scene") && ok;
  if (!ztorycFile.isEmpty()) {
    QString target = scenePath;
    target.replace(QRegularExpression("\\.tnz$"), ".ztoryc");
    ok = replaceFile(dir + "/" + ztorycFile, target, backup + "/scene") && ok;
  }

  if (!ok) {
    DVGui::warning(
        tr("Some files could not be recovered.\nThe copy is still in:\n%1\n"
           "The files that were replaced are in:\n%2")
            .arg(dir, backup));
    m_busy = false;
    return false;
  }
  removeDir(dir);
  m_busy = false;

  // Si ricarica la scena dai file appena rimessi. Pulita, o Tahoma
  // chiederebbe di salvare quella che si sta sostituendo.
  TApp::instance()->getCurrentScene()->setDirtyFlag(false);
  IoCmd::loadScene(TFilePath(scenePath.toStdWString()), true, false);
  DVGui::info(tr("Work recovered. The files it replaced are in:\n%1")
                  .arg(backup));
  return true;
}
