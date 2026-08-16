#include "ztorymouthapply.h"

#include "tapp.h"
#include "ztorymodel.h"

#include "toonz/childstack.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/txshsoundtextlevel.h"
#include "toonz/tstageobject.h"
#include "tundo.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>

//============================================================================

QString MouthApplyReport::summary() const {
  QStringList parts;
  parts << QObject::tr("%1 frames written").arg(written);
  if (noMap > 0)
    parts << QObject::tr("%1 phonemes not in the set").arg(noMap);
  if (poses > 0) parts << QObject::tr("%1 poses (not written here)").arg(poses);
  if (offStage > 0)
    parts << QObject::tr("%1 frames where the character is not on stage")
                 .arg(offStage);
  if (!conflicts.isEmpty()) {
    QStringList f;
    for (int i = 0; i < conflicts.size() && i < 6; i++)
      f << QString::number(conflicts[i]);
    QString list = f.join(", ");
    if (conflicts.size() > 6) list += "…";
    parts << QObject::tr("%1 CLASHES on sub-scene frames %2 — the character is "
                         "held there, so two different mouths land on one cell")
                 .arg(conflicts.size())
                 .arg(list);
  }
  return parts.join("  ·  ");
}

//----------------------------------------------------------------------------
// Trovare i posti che hanno una mappa
//----------------------------------------------------------------------------

namespace {

//! Tutti i `.zmouth` del progetto, per nome di file. UNA scansione.
//!
//! ⚠️ Indicizzati in blocco e non cercati uno per uno: cercare per ogni livello
//! senza mappa vorrebbe dire riscandire l'albero degli extras tante volte
//! quanti sono i livelli, su un volume esterno. E' lo stesso errore che oggi ha
//! fatto caricare uno storyboard in un minuto e mezzo (collectColumnNames, che
//! ricorreva per cella invece che per sotto-scena).
QHash<QString, TFilePath> indexProjectMaps(ToonzScene *scene) {
  QHash<QString, TFilePath> out;
  if (!scene) return out;
  for (const char *alias : {"+extras", "+drawings", "+scenes"}) {
    const TFilePath dir = scene->decodeFilePath(TFilePath(alias));
    if (dir.isEmpty()) continue;
    const QString root = QString::fromStdWString(dir.getWideString());
    if (!QDir(root).exists()) continue;
    QDirIterator it(root, QStringList() << "*.zmouth", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
      const QString f = it.next();
      const QFileInfo fi(f);
      // Il primo che si trova vince: se lo stesso nome esiste in due punti
      // sono la stessa mappa copiata, non due mappe diverse.
      if (!out.contains(fi.completeBaseName()))
        out.insert(fi.completeBaseName(), TFilePath(f.toStdWString()));
    }
  }
  return out;
}

void collectTargets(ToonzScene *scene, TXsheet *xsh, int depth,
                    QSet<TXshLevel *> &seen,
                    const QHash<QString, TFilePath> &mapIndex,
                    QVector<MouthApplyTarget> &out) {
  // Come in ztorigmouths: un limite alla discesa, perche' una scena malfatta
  // puo' contenere un ciclo e qui si sta solo compilando un elenco.
  if (!xsh || depth > 8) return;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    if (!c || c->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    c->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = xsh->getCell(r, col);
      if (cell.isEmpty() || !cell.m_level) continue;
      TXshLevel *lv = cell.m_level.getPointer();
      if (seen.contains(lv)) continue;

      TXshChildLevel *cl = lv->getChildLevel();
      if (cl) {
        seen.insert(lv);
        // Una sotto-scena: la sua mappa (se c'e') sta accanto alla SCENA.
        const QString sub = QString::fromStdWString(lv->getName());
        MouthApplyTarget t;
        t.level    = lv;
        t.subScene = sub;
        t.owner    = scene->getScenePath();
        // Si aggiunge ANCHE senza mappa: potrebbe averla nella scena di
        // libreria da cui e' stata importata, e quel recupero lo fa
        // findTargets. Chi non ne ha nessuna viene scartato alla fine.
        ZtoryMouthMap::load(t.owner, sub, t.map);
        t.label = QObject::tr("%1  (sub-scene)").arg(sub);
        out.push_back(t);
        // Si scende comunque: la bocca puo' stare PIU' IN FONDO, ed e' il caso
        // normale di un personaggio riggato (bocca dentro testa dentro corpo).
        collectTargets(scene, cl->getXsheet(), depth + 1, seen, mapIndex,
                       out);
        continue;
      }

      TXshSimpleLevel *sl = lv->getSimpleLevel();
      if (!sl) continue;
      seen.insert(lv);
      MouthApplyTarget t;
      t.level = lv;
      t.owner = scene->decodeFilePath(sl->getPath());
      ZtoryMouthMap::load(t.owner, QString(), t.map);
      if (t.map.sets.isEmpty()) {
        // La copia nello shot puo' non avere la mappa accanto (l'import di
        // maggio non la copiava): si cerca quella dell'originale, per NOME DI
        // FILE ESATTO. E' lo stesso livello copiato, quindi il nome coincide —
        // non e' un indovinello.
        const QString base =
            QString::fromStdWString(t.owner.getWideName());
        auto found = mapIndex.constFind(base);
        if (found != mapIndex.constEnd()) {
          const TFilePath alt = found.value().withType(t.owner.getType());
          if (ZtoryMouthMap::load(alt, QString(), t.map) &&
              !t.map.sets.isEmpty()) {
            t.owner = alt;
            t.label = QObject::tr("%1  (map found next to the original)")
                          .arg(QString::fromStdWString(lv->getName()));
            out.push_back(t);
            continue;
          }
        }
      }
      if (!t.map.sets.isEmpty()) {
        t.label = QString::fromStdWString(lv->getName());
        out.push_back(t);
      }
    }
  }
}

//! Dove finisce, DENTRO l'albero delle sotto-scene, il fotogramma \p row
//! dell'xsheet \p xsh — se lassu' e' esposto \p target.
//!
//! La corrispondenza SI LEGGE dalla cella: quando una colonna espone una
//! sotto-scena, il fotogramma della cella E' la riga di quella sotto-scena.
//! Leggerla invece di calcolarla e' cio' che fa funzionare anche i fermi, i
//! rallentamenti e i rimontaggi.
bool locate(TXsheet *xsh, TXshLevel *target, int row, int depth,
            TXsheet **outXsh, int *outCol, int *outRow) {
  if (!xsh || depth > 8 || row < 0) return false;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    if (!c || c->isEmpty()) continue;
    const TXshCell cell = xsh->getCell(row, col);
    if (cell.isEmpty() || !cell.m_level) continue;

    if (cell.m_level.getPointer() == target) {
      *outXsh = xsh;
      *outCol = col;
      *outRow = row;
      return true;
    }
    if (TXshChildLevel *cl = cell.m_level->getChildLevel()) {
      // La cella dice a quale fotogramma della sotto-scena siamo. 1-based
      // nella cella, 0-based come riga.
      const int inner = cell.m_frameId.getNumber() - 1;
      if (locate(cl->getXsheet(), target, inner, depth + 1, outXsh, outCol,
                 outRow))
        return true;
    }
  }
  return false;
}

//! Rimette le celle com'erano. Si fotografa PRIMA di scrivere: applicare il lip
//! sync sovrascrive la colonna delle bocche, che e' lavoro dell'animatore.
class MouthApplyUndo final : public TUndo {
  TXsheet *m_xsh;
  int m_col;
  QMap<int, TXshCell> m_before, m_after;

public:
  MouthApplyUndo(TXsheet *xsh, int col, QMap<int, TXshCell> before,
                 QMap<int, TXshCell> after)
      : m_xsh(xsh), m_col(col), m_before(std::move(before))
      , m_after(std::move(after)) {}

  void put(const QMap<int, TXshCell> &cells) const {
    if (!m_xsh) return;
    for (auto it = cells.constBegin(); it != cells.constEnd(); ++it)
      m_xsh->setCell(it.key(), m_col, it.value());
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }
  void undo() const override { put(m_before); }
  void redo() const override { put(m_after); }
  int getSize() const override {
    return int(sizeof(*this) +
               (m_before.size() + m_after.size()) * sizeof(TXshCell));
  }
  QString getHistoryString() override {
    return QObject::tr("Apply Lip Sync to Mouths");
  }
};

}  // namespace

//----------------------------------------------------------------------------

TXsheet *ZtoryMouthApply::workingXsheet() {
  // ⚠️ L'xsheet CORRENTE, non quello in cima alla scena.
  //
  // In uno storyboard ogni shot e' una sotto-scena, e il comando del lip sync
  // scrive le colonne dei fonemi LI' DENTRO. Cercando solo in cima non si
  // trovava niente, e sembrava che il lip sync non fosse stato generato
  // (Franco, 2026-08-16: «non trova ne' la colonna dei fonemi ne' i set»).
  //
  // L'xsheet corrente e' anche quello che l'utente sta guardando, quindi i
  // numeri di fotogramma che scrive nel popup sono i suoi.
  TXsheet *cur = TApp::instance()->getCurrentXsheet()->getXsheet();
  if (cur) return cur;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  return scene ? scene->getChildStack()->getTopXsheet() : nullptr;
}

QVector<MouthApplyTarget> ZtoryMouthApply::findTargets(ToonzScene *scene) {
  QVector<MouthApplyTarget> out;
  if (!scene) return out;
  QSet<TXshLevel *> seen;
  // Indicizzato una volta sola, prima di scendere.
  const QHash<QString, TFilePath> mapIndex = indexProjectMaps(scene);
  collectTargets(scene, workingXsheet(), 0, seen, mapIndex, out);

  // ── I set che sono ARRIVATI COL PERSONAGGIO ─────────────────────────────
  // La mappa di una sotto-scena vive accanto alla scena che la contiene.
  // Importando il personaggio in uno shot, la sotto-scena arriva ma la sua
  // mappa resta accanto alla scena di libreria: qui non c'e'.
  //
  // Si ritrova per la catena che Franco aveva progettato dall'inizio —
  // personaggio -> asset -> scena di libreria -> mappa — e l'aggancio e' il
  // NOME della sotto-scena, che l'import non cambia.
  for (MouthApplyTarget &t : out) {
    if (!t.map.sets.isEmpty() || t.subScene.isEmpty()) continue;
    ZtoryModel *model = ZtoryModel::instance();
    for (const Asset &a : model->assets()) {
      if (a.type.compare("Character", Qt::CaseInsensitive) != 0) continue;
      const QString libScene = model->resolveAssetFile(a);
      if (libScene.isEmpty()) continue;
      MouthMap m;
      if (!ZtoryMouthMap::load(TFilePath(libScene.toStdWString()), t.subScene, m))
        continue;
      if (m.sets.isEmpty()) continue;
      t.map   = m;
      t.label = QObject::tr("%1  (sub-scene, from %2)").arg(t.subScene, a.name);
      break;
    }
  }

  // Restano solo i posti che una mappa ce l'hanno davvero: un elenco con
  // dentro voci senza set farebbe scegliere qualcosa che poi non applica
  // niente.
  QVector<MouthApplyTarget> usable;
  for (const MouthApplyTarget &t : out)
    if (!t.map.sets.isEmpty()) usable.push_back(t);
  return usable;
}

//----------------------------------------------------------------------------

QString ZtoryMouthApply::phonemeAt(TXsheet *xsh, int col, int row) {
  if (!xsh || col < 0 || row < 0) return QString();
  TXshColumn *c = xsh->getColumn(col);
  if (!c) return QString();
  TXshSoundTextColumn *sc = c->getSoundTextColumn();
  if (!sc) return QString();
  const TXshCell cell = xsh->getCell(row, col);
  if (cell.isEmpty() || !cell.m_level) return QString();
  TXshSoundTextLevel *lvl =
      dynamic_cast<TXshSoundTextLevel *>(cell.m_level.getPointer());
  if (!lvl) return QString();
  return lvl->getFrameText(cell.m_frameId.getNumber() - 1).trimmed();
}

QVector<int> ZtoryMouthApply::findPhonemeColumns(TXsheet *xsh) {
  QVector<int> out;
  if (!xsh) return out;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *c = xsh->getColumn(col);
    if (!c || c->isEmpty() || !c->getSoundTextColumn()) continue;
    int r0 = 0, r1 = 0;
    c->getRange(r0, r1);
    // Si riconosce dal CONTENUTO: il nome della colonna e' rinominabile, i
    // viseme no. Bastano poche celle riconosciute — una colonna di parole non
    // le produce, salvo che qualcuno parli solo di «rest» ed «etc».
    int hits = 0;
    for (int r = r0; r <= r1 && hits < 3; r++)
      if (ZtoryMouthMap::shapeIndex(phonemeAt(xsh, col, r)) >= 0) hits++;
    if (hits >= 3) out.push_back(col);
  }
  return out;
}

//----------------------------------------------------------------------------

MouthApplyReport ZtoryMouthApply::apply(ToonzScene *scene, int phonemeCol,
                                        const MouthApplyTarget &target,
                                        const QVector<MouthApplyRange> &ranges) {
  MouthApplyReport rep;
  if (!scene || !target.level) return rep;
  TXsheet *top = workingXsheet();
  if (!top) return rep;

  // Cosa scrivere, riga per riga della sotto-scena. Si RACCOGLIE tutto prima e
  // si scrive dopo: e' l'unico modo per accorgersi che due fotogrammi dello
  // shot cadono sulla stessa riga, che e' il caso che va detto e non risolto.
  TXsheet *destXsh = nullptr;
  int destCol      = -1;
  QMap<int, TXshCell> planned;
  QMap<int, QString> plannedShape;  // per rilevare i conflitti

  for (const MouthApplyRange &rg : ranges) {
    const int si = target.map.indexOfSet(rg.setName);
    if (si < 0) continue;
    const MouthSet &set = target.map.sets[si];

    for (int f = rg.from; f <= rg.to; f++) {
      const QString shape = phonemeAt(top, phonemeCol, f - 1);
      if (shape.isEmpty()) continue;
      const int idx = ZtoryMouthMap::shapeIndex(shape);
      if (idx < 0) continue;

      // Dove cade questo fotogramma dentro l'albero delle sotto-scene.
      TXsheet *xsh = nullptr;
      int col = -1, row = -1;
      if (!locate(top, target.level, f - 1, 0, &xsh, &col, &row)) {
        // Il personaggio non e' esposto qui: non e' un errore, e' una pausa in
        // cui non c'e' niente da animare.
        rep.offStage++;
        continue;
      }
      if (!destXsh) {
        destXsh = xsh;
        destCol = col;
      }

      // Il bersaglio sul livello ANCORA. Gli altri livelli (denti, lingua) li
      // scrive il percorso multi-bersaglio, che qui non serve: in una
      // sotto-scena un viseme e' gia' un fotogramma solo.
      const MouthTarget *chosen = nullptr;
      for (const MouthTarget &t : set.mouths[idx]) {
        if (t.isPose()) { rep.poses++; continue; }
        if (t.isAnchorLevel()) { chosen = &t; break; }
      }
      if (!chosen) { rep.noMap++; continue; }

      // ⚠️ Due fotogrammi dello shot sulla STESSA riga della sotto-scena: c'e'
      // una cella sola. Succede su un fermo, ed e' esattamente il caso che
      // «vince l'ultimo» renderebbe invisibile.
      auto prev = plannedShape.constFind(row);
      if (prev != plannedShape.constEnd() && prev.value() != shape) {
        if (!rep.conflicts.contains(row + 1)) rep.conflicts.push_back(row + 1);
        continue;
      }
      plannedShape[row] = shape;

      TXshCell cell = xsh->getCell(row, col);
      cell.m_level   = target.level;
      cell.m_frameId = chosen->frameId;
      planned[row]   = cell;
    }
  }

  if (!destXsh || destCol < 0 || planned.isEmpty()) return rep;

  QMap<int, TXshCell> before;
  for (auto it = planned.constBegin(); it != planned.constEnd(); ++it)
    before[it.key()] = destXsh->getCell(it.key(), destCol);

  for (auto it = planned.constBegin(); it != planned.constEnd(); ++it) {
    destXsh->setCell(it.key(), destCol, it.value());
    rep.written++;
  }
  TUndoManager::manager()->add(
      new MouthApplyUndo(destXsh, destCol, before, planned));
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  return rep;
}
