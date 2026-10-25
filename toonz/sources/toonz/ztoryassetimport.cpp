#include "ztoryassetimport.h"

#include "ztorymodel.h"
#include "iocommand.h"
#include "psdsettingspopup.h"
#include "tapp.h"

#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/tstageobject.h"
#include "tundo.h"
#include "tfiletype.h"
#include "tsystem.h"
#include "timage_io.h"
#include "trasterimage.h"
#include "ztoryassetpreview.h"
#include "../common/psdlib/psd.h"

#include <QDateTime>
#include <QFileInfo>
#include <QObject>
#include <QSet>

namespace {

const ProjectShot *shotByUuid(const QString &uuid) {
  if (uuid.isEmpty()) return nullptr;
  for (const ProjectShot &ps : ZtoryModel::instance()->projectShots())
    if (ps.uuid == uuid) return &ps;
  return nullptr;
}

}  // namespace

//-----------------------------------------------------------------------------

// Where the layers' pixel data ends, by what the layers themselves declare:
// it starts after the last layer record and holds every channel of every
// layer, one after the other (psd.cpp, doImage).
static qint64 layerDataEnd(const TPSDHeaderInfo &h) {
  if (h.layersCount <= 0) return 0;
  const TPSDLayerInfo &last = h.linfo[h.layersCount - 1];
  qint64 end = qint64(last.additionalpos) + qint64(last.additionallen);
  for (int i = 0; i < h.layersCount; ++i)
    for (int ch = 0; ch < h.linfo[i].channels; ++ch)
      end += qint64(h.linfo[i].chan[ch].length);
  return end;
}

ZtoryPsdCheck ztoryCheckPsd(const QString &file) {
  const QFileInfo fi(file);
  const QString key = QStringLiteral("%1|%2|%3")
                          .arg(fi.absoluteFilePath())
                          .arg(fi.size())
                          .arg(fi.lastModified().toMSecsSinceEpoch());
  ZtoryModel *m = ZtoryModel::instance();
  ZtoryPsdCheck r;
  if (m->psdCheckCached(key, &r.problem, &r.note)) return r;
  const QString name = fi.fileName();
  bool certain       = true;  // worth remembering until the file changes
  try {
    const TFilePath fp(file.toStdWString());
    TPSDReader reader(fp);  // header, color mode, resources, layer info
    const TPSDHeaderInfo h = reader.getPSDHeaderInfo();
    // The modes the reader has pixels for (psd.cpp, doImage): the others
    // come in as empty levels, without a word.
    const bool readable =
        h.mode == ModeBitmap || h.mode == ModeGrayScale ||
        h.mode == ModeGray16 || h.mode == ModeDuotone ||
        h.mode == ModeDuotone16 || h.mode == ModeIndexedColor ||
        h.mode == ModeRGBColor || h.mode == ModeRGB48;
    if (!readable)
      r.problem = QObject::tr("%1 is not RGB (CMYK, Lab or multichannel): "
                              "Tahoma reads only RGB — convert it and save it "
                              "again")
                      .arg(name);
    else if (h.depth == 32)
      r.problem = QObject::tr("%1 is 32 bits per channel: save it at 8 or 16")
                      .arg(name);
    else if (qint64 need = layerDataEnd(h); need > fi.size())
      // A file cut short reads WITHOUT errors: past the end fread gives
      // nothing and the layers come in with garbage (measured on a copy of
      // a real background cut at 60%). The layers say how long their data is.
      r.problem = QObject::tr("%1 is cut short (%2 MB of at least %3): an "
                              "interrupted copy? Copy it again from the "
                              "original")
                      .arg(name)
                      .arg(fi.size() / 1048576.0, 0, 'f', 1)
                      .arg(need / 1048576.0, 0, 'f', 1);
    else {
      QStringList damaged, dotted;
      for (int i = 0; i < h.layersCount; ++i) {
        const TPSDLayerInfo &li = h.linfo[i];
        // A dot in a layer's name: Tahoma builds a path from it
        // («name#libreriaDett.RAF#group.psd») and reads «.RAF» as the file's
        // type — «Opening File Error», and the layer is not loaded (a real
        // background, 2026-09-27). Said before, with the name to change.
        if (li.name && QString::fromLocal8Bit(li.name).contains('.'))
          dotted << QString::fromLocal8Bit(li.name);
        if (li.section > 0) continue;  // a folder's start or end: no pixels
        if (li.right <= li.left || li.bottom <= li.top) continue;  // empty
        TRasterImageP img;
        reader.load(img, int(li.layerId));
        if (!img)
          damaged << (li.name ? QString::fromLocal8Bit(li.name)
                              : QString::number(li.layerId));
      }
      QStringList notes;
      if (!damaged.isEmpty())
        notes << QObject::tr("%1: these layers cannot be read and will come "
                             "in empty — %2. Open it in Photoshop and save it "
                             "again")
                     .arg(name, damaged.join(", "));
      if (!dotted.isEmpty())
        notes << QObject::tr("%1: layers with a dot in the name are not "
                             "loaded by Tahoma — rename them without the "
                             "dot: %2")
                     .arg(name, dotted.join(", "));
      r.note = notes.join("\n    ");
    }
  } catch (TException &e) {
    r.problem = QObject::tr("%1 cannot be read (%2) — open it in Photoshop and "
                            "save it again")
                    .arg(name, QString::fromStdWString(e.getMessage()));
  } catch (...) {
    // Not remembered: out of memory, a network volume that hiccuped — the
    // next export tries again.
    certain   = false;
    r.problem = QObject::tr("%1 cannot be read — open it in Photoshop and save "
                            "it again")
                    .arg(name);
  }
  if (certain) m->setPsdCheck(key, r.problem, r.note);
  return r;
}

// The PSDs an asset's file stands on: itself, or those a character's scene
// is built from.
static QStringList psdsBehind(const QString &file) {
  if (file.endsWith(".psd", Qt::CaseInsensitive)) return {file};
  if (file.endsWith(".tnz", Qt::CaseInsensitive))
    return ztoryPsdsUsedByScene(file);
  return {};
}

QVector<ZtoryAssetCheck> ztoryCheckShotAssets(const QStringList &shotUuids,
                                              bool withReferences) {
  ZtoryModel *m = ZtoryModel::instance();
  QVector<ZtoryAssetCheck> out;
  // ⚠️ UNA volta per asset distinto. Gli asset si ripetono su molti shot, e
  // risolverli per shot vuol dire rileggere la stessa cartella centinaia di
  // volte: su un volume esterno la differenza si sente.
  QHash<QString, int> seen;  // uuid dell'asset -> posizione in `out`

  for (const QString &su : shotUuids) {
    const ProjectShot *ps = shotByUuid(su);
    if (!ps) continue;
    for (const BreakdownEntry &be : ps->breakdown) {
      if (!withReferences)
        if (const Asset *ra = m->assetByUuid(be.assetUuid))
          if (ZtoryModel::isReferenceType(ra->type)) continue;
      auto it = seen.find(be.assetUuid);
      if (it != seen.end()) {
        // Gia' risolto: qui si aggiunge solo CHI lo chiede, che e' l'altra
        // meta' del rapporto («manca, e blocca questi cinque shot»).
        if (!out[it.value()].shots.contains(ps->label))
          out[it.value()].shots << ps->label;
        continue;
      }
      ZtoryAssetCheck c;
      c.uuid = be.assetUuid;
      if (const Asset *a = m->assetByUuid(be.assetUuid)) {
        c.name = a->name;
        c.type = a->type;
        // No file on purpose (drawn inside another asset): listed as fine —
        // not missing, not reported, and «the breakdown is empty» is not said.
        c.onPurpose = a->noFile;
        if (!c.onPurpose) c.file = m->resolveAssetFile(*a, &c.reason);
        // The load test, now and not during the export: a PSD that does not
        // load is reported with the others, and the import skips it.
        // Every PSD a character's scene stands on, not only the first.
        for (const QString &psd : psdsBehind(c.file)) {
          const ZtoryPsdCheck pc = ztoryCheckPsd(psd);
          if (!pc.problem.isEmpty()) {
            c.file.clear();
            c.reason = pc.problem;
            break;
          }
          if (!pc.note.isEmpty())
            c.note += (c.note.isEmpty() ? QString() : QString("\n    ")) +
                      pc.note;
        }
      } else {
        // Voce che punta a un asset cancellato: non e' «manca il file», e'
        // «manca l'asset». Dirlo con le stesse parole confonderebbe le idee su
        // dove andare a sistemare.
        c.name   = QObject::tr("⟨asset %1⟩").arg(be.assetUuid.left(8));
        c.reason = QObject::tr(
            "the breakdown points at an asset that is no longer in the "
            "project");
      }
      if (!c.shots.contains(ps->label)) c.shots << ps->label;
      seen.insert(be.assetUuid, out.size());
      out.append(c);
    }
  }
  return out;
}

//-----------------------------------------------------------------------------

QString ztoryAssetReport(const QVector<ZtoryAssetCheck> &checks) {
  QStringList lines;
  for (const ZtoryAssetCheck &c : checks) {
    if (c.ok()) {
      // Entra, ma con un difetto: detto qui, prima, e non scoperto nello shot.
      if (!c.note.isEmpty())
        lines << QString("• %1\n    %2").arg(c.name, c.note);
      continue;
    }
    // Il MOTIVO, non un conteggio: resolveAssetFile risponde gia' con frasi che
    // dicono cosa fare («linked file is missing: X», «no folder set for type
    // Prop»). Sostituirle con «manca» butterebbe via l'unica cosa utile.
    QString line = QString("• %1").arg(c.name);
    if (!c.type.isEmpty()) line += QString(" (%1)").arg(c.type);
    line += QString("\n    %1").arg(c.reason);
    line += QString("\n    %1").arg(
        QObject::tr("needed by: %1").arg(c.shots.join(", ")));
    lines << line;
  }
  return lines.join("\n\n");
}

//-----------------------------------------------------------------------------

// With Import, the PSD copied where the project puts it for `scenePath`, as
// Tahoma's import does for any other file; the copy's path, or `psd` itself
// (already in the project, or the copy failed). An existing copy is kept.
static TFilePath importPsdCopy(const TFilePath &psd, const TFilePath &scenePath,
                               QStringList *log) {
  ToonzScene *cur = TApp::instance()->getCurrentScene()->getScene();
  if (!cur || !cur->isExternPath(psd)) return psd;  // in the project: stays
  ToonzScene target;
  target.setProject(cur->getProject());
  target.setScenePath(scenePath.isEmpty() ? cur->getScenePath() : scenePath);
  const TFilePath dst = cur->decodeFilePath(target.getImportedLevelPath(psd));
  if (dst.isEmpty() || dst == psd) return psd;
  // THE NEWER FILE WINS (Franco, 2026-09-27), as for the other imported
  // assets (iocommand.cpp, ResourceImportDialog::process, silent).
  if (TSystem::doesExistFileOrLevel(dst)) {
    const QDateTime srcTime = QFileInfo(psd.getQString()).lastModified();
    const QDateTime dstTime = QFileInfo(dst.getQString()).lastModified();
    if (!(srcTime > dstTime)) {
      if (log)
        *log << QObject::tr("kept the existing copy of %1 (not older than its "
                            "source)")
                    .arg(dst.getQString());
      return dst;
    }
    if (log)
      *log << QObject::tr("replacing %1: its source is newer")
                  .arg(dst.getQString());
    try {
      TSystem::removeFileOrLevel(dst);
    } catch (...) {
    }
  }
  try {
    TSystem::touchParentDir(dst);
    TSystem::copyFile(dst, psd);
  } catch (...) {
  }
  if (!TSystem::doesExistFileOrLevel(dst)) {
    if (log)
      *log << QObject::tr("could not copy %1 into the project: loaded from "
                          "where it is")
                  .arg(psd.getQString());
    return psd;
  }
  if (log) *log << QObject::tr("copied into %1").arg(dst.getQString());
  return dst;
}

QList<TXshLevel *> ztoryLoadPsdWithPolicy(const TFilePath &psd,
                                          const AssetImportPolicy &policy,
                                          int col0, QStringList *errors,
                                          const TFilePath &importScenePath) {
  const TFilePath source = policy.mode == AssetImportPolicy::Import
                               ? importPsdCopy(psd, importScenePath, errors)
                               : psd;
  PsdSettingsPopup popup;
  popup.setPath(source);
  popup.applySettings(policy.psdLoadAs, policy.psdLevelName, policy.psdGroups,
                      policy.psdSubScene);
  IoCmd::LoadResourceArguments args;
  args.importPolicy = policy.mode == AssetImportPolicy::Import
                          ? IoCmd::LoadResourceArguments::IMPORT
                          : IoCmd::LoadResourceArguments::LOAD;
  args.row0 = 0;
  args.col0 = col0;
  // With somewhere to write the errors, no questions (see silent).
  args.silent = errors != nullptr;
  IoCmd::loadPsdResource(args, &popup);
  if (errors) *errors += args.errors;
  QList<TXshLevel *> levels;
  for (TXshLevel *lv : args.loadedLevels)
    if (lv) levels.append(lv);
  return levels;
}

ZtoryImportedAssets ztoryImportShotAssets(const QString &shotUuid,
                                          TXsheet *subXsheet,
                                          const TFilePath &shotScenePath,
                                          bool withReferences) {
  ZtoryImportedAssets res;
  if (!subXsheet) return res;
  const ProjectShot *ps = shotByUuid(shotUuid);
  if (!ps || ps->breakdown.isEmpty()) return res;

  ZtoryModel *m     = ZtoryModel::instance();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return res;
  // Le funzioni di caricamento di Tahoma lavorano sullo xsheet CORRENTE, non su
  // uno passato: se la sotto-scena aperta non e' questa, i livelli finirebbero
  // nello storyboard invece che nello shot. Meglio non importare niente che
  // importare nel posto sbagliato.
  if (scene->getXsheet() != subXsheet) {
    res.log << QObject::tr("assets NOT imported: the shot's sub-scene is not "
                           "the current one");
    return res;
  }

  // Due liste perche' la politica e' per CHIAMATA, non per file: gli asset da
  // copiare nella scena e quelli a cui puntare e basta vanno in due giri.
  std::vector<TFilePath> toLoad, toImport;
  // I psd a parte, ognuno con la SUA politica: le impostazioni sono per asset,
  // e due psd nello stesso shot possono volerne di diverse.
  QVector<QPair<TFilePath, AssetImportPolicy>> psdFiles;
  QSet<QString> already;  // stesso file due volte = una modale «Allow duplicate?»

  for (const BreakdownEntry &be : ps->breakdown) {
    const Asset *a = m->assetByUuid(be.assetUuid);
    if (!a) continue;
    if (!withReferences && ZtoryModel::isReferenceType(a->type)) continue;
    if (a->noFile) {
      res.log << QObject::tr("%1: no file on purpose (drawn inside another "
                             "asset)").arg(a->name);
      continue;
    }
    QString why;
    ZtoryModel::AssetMatch how = ZtoryModel::AssetMatch::None;
    const QString file = m->resolveAssetFile(*a, &why, nullptr, &how);
    const bool isNear = how == ZtoryModel::AssetMatch::NearName ||
                        how == ZtoryModel::AssetMatch::Convention;
    if (file.isEmpty()) {
      // Il controllo di prima l'ha gia' mostrato all'utente, che ha scelto di
      // proseguire: qui basta lasciarne traccia.
      res.log << QObject::tr("skipped %1: %2").arg(a->name, why);
      continue;
    }
    QString psdProblem;  // from the check's memory: no second reading
    for (const QString &psd : psdsBehind(file)) {
      const ZtoryPsdCheck pc = ztoryCheckPsd(psd);
      if (!pc.problem.isEmpty()) {
        psdProblem = pc.problem;
        break;
      }
      if (!pc.note.isEmpty()) res.log << QObject::tr("%1: %2").arg(a->name, pc.note);
    }
    if (!psdProblem.isEmpty()) {
      res.log << QObject::tr("skipped %1: %2").arg(a->name, psdProblem);
      continue;
    }
    if (already.contains(file)) {
      res.log << QObject::tr("%1: already imported in this shot").arg(a->name);
      continue;
    }
    already.insert(file);

    const AssetImportPolicy pol = m->effectiveImportPolicy(*a);
    const TFilePath fp(file.toStdWString());
    // ⚠️ L'audio dentro una sotto-scena e' vietato in Ztoryc (vive solo nel
    // main xsheet) e loadResources risponde con una finestra di avviso: in
    // mezzo a un export sarebbe una modale per shot. Si salta qui, dicendo
    // perche'.
    if (TFileType::getInfo(fp) == TFileType::AUDIO_LEVEL) {
      res.log << QObject::tr(
                     "skipped %1: audio lives in the main xsheet, not inside a "
                     "shot")
                     .arg(a->name);
      continue;
    }
    if (QFileInfo(file).suffix().compare("psd", Qt::CaseInsensitive) == 0) {
      // ⚠️ Un PSD passato da loadResources apre PsdSettingsPopup, UNA VOLTA PER
      // FILE: un export di quaranta shot diventa quaranta finestre da
      // confermare. Le impostazioni pero' ci sono gia' — quelle dell'asset
      // sopra quelle di progetto — e si applicano direttamente al popup senza
      // mostrarlo.
      psdFiles.push_back(qMakePair(fp, pol));
    } else if (pol.mode == AssetImportPolicy::Import) {
      toImport.push_back(fp);
    } else {
      toLoad.push_back(fp);
    }
    res.log << QObject::tr("%1 (%2) ← %3").arg(a->name, a->type, file) +
                   (isNear ? QObject::tr("  (%1)").arg(why) : QString());
  }

  if (toLoad.empty() && toImport.empty() && psdFiles.isEmpty()) return res;

  const int before = subXsheet->getColumnCount();

  auto runLoad = [&](const std::vector<TFilePath> &paths, bool import) {
    if (paths.empty()) return;
    IoCmd::LoadResourceArguments args;
    for (const TFilePath &fp : paths) args.resourceDatas.push_back(fp);
    // Esplicito e non ASK_USER: e' cio' che rende l'import silenzioso. Con
    // ASK_USER Tahoma chiede «importare o caricare?» al primo file.
    args.importPolicy = import ? IoCmd::LoadResourceArguments::IMPORT
                               : IoCmd::LoadResourceArguments::LOAD;
    args.expose = true;
    // In coda, sempre: le colonne dello storyboard restano dove sono, e le
    // nuove si riconoscono perche' vengono dopo.
    args.row0 = 0;
    args.col0 = subXsheet->getColumnCount();
    // No questions: an existing file is kept, the errors go to the log.
    args.silent = true;
    args.importScenePath = shotScenePath;  // copies in the SHOT's folder
    IoCmd::loadResources(args, /*updateRecentFile=*/false);
    for (const QString &e : args.errors) res.log << e;
    for (TXshLevel *lv : args.loadedLevels)
      if (lv) res.levels.append(lv);
  };
  runLoad(toLoad, false);
  runLoad(toImport, true);

  // I psd, uno per volta: il popup porta le scelte di QUEL psd, quindi va
  // riconfigurato fra un file e l'altro.
  for (const auto &pr : psdFiles) {
    QStringList psdErrors;
    res.levels.append(ztoryLoadPsdWithPolicy(pr.first, pr.second,
                                             subXsheet->getColumnCount(),
                                             &psdErrors, shotScenePath));
    for (const QString &e : psdErrors) res.log << e;
  }

  // Le colonne nate adesso sono quelle dopo `before`. Contate cosi' e non
  // dedotte dai valori che loadResources riporta: se un file fallisce a meta',
  // il conto suo e quello vero non coincidono, e toglierne una di troppo vuol
  // dire togliere una colonna dello storyboard.
  for (int c = before; c < subXsheet->getColumnCount(); c++)
    res.columns.append(c);
  return res;
}
