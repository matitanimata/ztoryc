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

QVector<ZtoryAssetCheck> ztoryCheckShotAssets(const QStringList &shotUuids) {
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
        c.file = m->resolveAssetFile(*a, &c.reason);
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
    if (c.ok()) continue;
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

ZtoryImportedAssets ztoryImportShotAssets(const QString &shotUuid,
                                          TXsheet *subXsheet) {
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
    QString why;
    const QString file = m->resolveAssetFile(*a, &why);
    if (file.isEmpty()) {
      // Il controllo di prima l'ha gia' mostrato all'utente, che ha scelto di
      // proseguire: qui basta lasciarne traccia.
      res.log << QObject::tr("skipped %1: %2").arg(a->name, why);
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
    res.log << QObject::tr("%1 (%2) ← %3").arg(a->name, a->type, file);
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
    IoCmd::loadResources(args, /*updateRecentFile=*/false);
    for (TXshLevel *lv : args.loadedLevels)
      if (lv) res.levels.append(lv);
  };
  runLoad(toLoad, false);
  runLoad(toImport, true);

  // I psd, uno per volta: il popup porta le scelte di QUEL psd, quindi va
  // riconfigurato fra un file e l'altro.
  for (const auto &pr : psdFiles) {
    PsdSettingsPopup popup;
    popup.setPath(pr.first);
    popup.applySettings(pr.second.psdLoadAs, pr.second.psdLevelName,
                        pr.second.psdGroups, pr.second.psdSubScene);
    IoCmd::LoadResourceArguments args;
    args.importPolicy = pr.second.mode == AssetImportPolicy::Import
                            ? IoCmd::LoadResourceArguments::IMPORT
                            : IoCmd::LoadResourceArguments::LOAD;
    args.row0 = 0;
    args.col0 = subXsheet->getColumnCount();
    IoCmd::loadPsdResource(args, &popup);
    for (TXshLevel *lv : args.loadedLevels)
      if (lv) res.levels.append(lv);
  }

  // Le colonne nate adesso sono quelle dopo `before`. Contate cosi' e non
  // dedotte dai valori che loadResources riporta: se un file fallisce a meta',
  // il conto suo e quello vero non coincidono, e toglierne una di troppo vuol
  // dire togliere una colonna dello storyboard.
  for (int c = before; c < subXsheet->getColumnCount(); c++)
    res.columns.append(c);
  return res;
}
