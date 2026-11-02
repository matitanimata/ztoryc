#include "ztorymouthapply.h"
#include <QRegularExpression>
#include "ztorymouthlibrary.h"

#include "tapp.h"
#include "toonzqt/icongenerator.h"
#include "ztorycharacter.h"
#include "ztorymodel.h"

#include "toonz/childstack.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/textureutils.h"

#include <QDateTime>
#include <QTextStream>
#include <QFile>
#include "toonz/toonzfolders.h"
#include <functional>
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
  if (noPhoneme > 0)
    parts << QObject::tr("%1 frames with no phoneme").arg(noPhoneme);
  if (unknownShape > 0)
    parts << QObject::tr("%1 phonemes not recognised").arg(unknownShape);
  if (noSet > 0)
    parts << QObject::tr("%1 frames whose set is not in the map").arg(noSet);
  if (!destinations.isEmpty())
    parts << QObject::tr("written into: %1").arg(destinations.join("; "));
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
                    QVector<MouthApplyTarget> &out, QStringList *problems) {
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
                       out, problems);
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
        // A PSD group whose original still has the map under the group's
        // number: the index knows that name, not this one. The folders that
        // hold maps of the same PSD are where to look.
        QString why;
        TXshSimpleLevel::findMouthMap(t.owner, &why);
        if (t.owner.getType() == "psd") {
          const QString file = base.section('#', 0, 0) + "#";
          QSet<QString> dirs;
          for (auto m = mapIndex.constBegin(); m != mapIndex.constEnd(); ++m)
            if (m.key().startsWith(file))
              dirs.insert(m.value().getParentDir().getQString());
          for (const QString &d : dirs) {
            QString whyThere;
            const TFilePath found = TXshSimpleLevel::findMouthMap(
                t.owner, &whyThere, TFilePath(d.toStdWString()));
            if (why.isEmpty()) why = whyThere;
            if (found.isEmpty()) continue;
            const TFilePath alt = found.withType(t.owner.getType());
            if (ZtoryMouthMap::load(alt, QString(), t.map) &&
                !t.map.sets.isEmpty()) {
              t.owner = alt;
              t.label = QObject::tr("%1  (map found next to the original)")
                            .arg(QString::fromStdWString(lv->getName()));
              break;
            }
          }
          if (!t.map.sets.isEmpty()) {
            out.push_back(t);
            continue;
          }
        }
        if (!why.isEmpty() && problems) {
          // Said of the FILE, not of this level: every group of the PSD
          // without a map meets the same lost file, and says it once.
          for (const QString &line : why.split('\n'))
            if (!problems->contains(line)) {
              problems->push_back(line);
              qWarning("Ztoryc mouths: %s", qPrintable(line));
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
// ⚠️ LE CELLE SONO STATE SCRITTE DENTRO UNA SOTTO-SCENA, ma chi guarda e'
// l'xsheet di sopra: la sua immagine resta quella di prima, e
// notifyXsheetChanged() non la rifa'. Sintomo esatto (Franco, 2026-08-17):
// applicato il set giusto, nel main restavano visibili le bocche del set
// sbagliato; entrando nella sotto-scena si vedevano quelle giuste, e
// uscendo il main si aggiornava. Non era un caso: uscire da una sotto-scena
// invalida l'icona del livello che la espone (subscenecommand.cpp,
// closeSubXsheet). Qui si fa la stessa cosa senza dover entrare e uscire.
//
// ⚠️ NON si svuota TImageCache: cancellerebbe anche i disegni su cui si sta
// lavorando, col cursore a pallino rosso per cache miss (vedi la nota in
// ztorymodel.cpp, ZtoryModel::activateShotForViewing).
//
// Una funzione e non un pezzo di apply: la chiama anche l'annullamento, che
// senza lasciava il viewer stantio dopo ⌘Z (revisione, 2026-09-27).
void invalidateMouthDestinations(const QSet<TXsheet *> &destXshs) {
  if (TXsheet *cur = TApp::instance()->getCurrentXsheet()->getXsheet()) {
    if (!destXshs.contains(cur) || destXshs.size() > 1) {
      // ⚠️ NON basta la sotto-scena che e' cambiata: vanno invalidate TUTTE
      // quelle che la contengono, fino a quella che si vede da qui.
      //
      // Le bocche stanno annidate — shot ▸ personaggio ▸ (magari) testa ▸
      // bocche — e ogni livello tiene la propria immagine composita: quella del
      // personaggio non si rifa' solo perche' e' cambiata quella dentro. Prima
      // invalidavo solo l'anello piu' interno, e l'aggiornamento arrivava solo
      // aprendo a mano tutte le nidificazioni (Franco, 2026-08-17) — cioe'
      // facendo a mano, uscendo, cio' che closeSubXsheet fa a ogni livello.
      //
      // Si scende dall'xsheet corrente e si invalida ogni anello della catena
      // che porta a quello modificato. Solo quella catena: invalidare tutto
      // vorrebbe dire rigenerare i personaggi che non c'entrano.
      std::function<bool(TXsheet *, std::set<TXsheet *> &)> invalidateChain =
          [&](TXsheet *xsh, std::set<TXsheet *> &seen) -> bool {
        if (!xsh || !seen.insert(xsh).second) return false;
        // A destination: its texture goes, and the descent CONTINUES — the
        // mouths can sit in another destination deeper down (SOFIA's turn).
          // ⚠️ QUESTA E' LA CACHE VERA, e non erano le icone.
          //
          // Il viewer NON tiene un'immagine composita: `stage.cpp` ricorre dal
          // vivo dentro le sotto-scene a ogni ridisegno. Tiene pero' una
          // TEXTURE OpenGL del CONTENUTO DI UN XSHEET a un dato fotogramma
          // (`texture_utils::getTextureData(const TXsheet*, int)`), e cambiare
          // le celle dentro quell'xsheet non la tocca. E' il motivo per cui
          // uscire dalla sotto-scena aggiustava tutto: TXsheetHandle::setXsheet
          // chiama invalidateTextures() con il commento «we'll be editing
          // m_xsheet - so destroy every texture of his».
        const bool isDest = destXshs.contains(xsh);
        if (isDest) texture_utils::invalidateTextures(xsh);
        bool onPath = isDest;
        for (int c = 0; c < xsh->getColumnCount(); c++) {
          int r0 = 0, r1 = -1;
          xsh->getCellRange(c, r0, r1);
          std::set<TXshLevel *> done;
          for (int r = r0; r <= r1; r++) {
            const TXshCell cell = xsh->getCell(r, c);
            if (cell.isEmpty() || !cell.m_level) continue;
            TXshChildLevel *cl = cell.m_level->getChildLevel();
            if (!cl || !done.insert(cell.m_level.getPointer()).second) continue;
            if (!invalidateChain(cl->getXsheet(), seen)) continue;
            // Questa sotto-scena contiene cio' che e' cambiato: la sua icona
            // non vale piu' su NESSUN fotogramma esposto, non solo sul primo.
            for (int rr = r0; rr <= r1; rr++) {
              const TXshCell cc = xsh->getCell(rr, c);
              if (!cc.isEmpty() && cc.m_level.getPointer() ==
                                       cell.m_level.getPointer())
                IconGenerator::instance()->invalidate(cc.m_level.getPointer(),
                                                      cc.m_frameId);
            }
            onPath = true;
          }
        }
        // Anche gli ANELLI DI SOPRA: la texture di un xsheet e' il suo
        // contenuto composito, quindi quella del personaggio contiene le bocche
        // ed e' vecchia quanto la loro.
        if (onPath && !isDest) texture_utils::invalidateTextures(xsh);
        return onPath;
      };
      std::set<TXsheet *> seen;
      invalidateChain(cur, seen);

      // La notifica che rifa' il render composito — la stessa che l'animatic usa
      // dopo le sue modifiche.
      TApp::instance()->getCurrentScene()->notifySceneChanged();
    }
  }
}

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
    invalidateMouthDestinations(QSet<TXsheet *>{m_xsh});
  }
  void undo() const override { put(m_before); }
  void redo() const override { put(m_after); }
  int getSize() const override {
    return int(sizeof(*this) +
               (m_before.size() + m_after.size()) * sizeof(TXshCell));
  }
  QString getHistoryString() override {
    return QObject::tr("Assign Mouth Drawings");
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

QVector<MouthApplyTarget> ZtoryMouthApply::findTargets(ToonzScene *scene,
                                                      QStringList *problems) {
  if (problems) problems->clear();
  QVector<MouthApplyTarget> out;
  if (!scene) return out;
  QSet<TXshLevel *> seen;
  // Indicizzato una volta sola, prima di scendere.
  const QHash<QString, TFilePath> mapIndex = indexProjectMaps(scene);
  collectTargets(scene, workingXsheet(), 0, seen, mapIndex, out, problems);

  // ⚠️ UNA SCENA PERSONAGGIO NON EREDITA DA NESSUNO. Il recupero qui sotto
  // serve allo SHOT, dove il personaggio arriva importato e la sua mappa e'
  // rimasta nella scena di libreria. Ma nella scena di libreria le sotto-scene
  // si mappano LI', e cercarle fra gli altri personaggi abbina per il solo nome:
  // in BRONTOLO la sotto-scena del rig si chiama «sub», come quella delle bocche
  // di SOFIA, ed era l'unica candidata — quindi «non ambigua» — e la scheda
  // proponeva le bocche di SOFIA dentro Brontolo (Franco, 2026-09-25). Il
  // controllo sull'ambiguita' del 28/08 non poteva vederlo: un candidato solo
  // non e' una prova che sia quello giusto.
  const QString tnz =
      QString::fromStdWString(scene->getScenePath().getWideString());
  const bool characterScene =
      !tnz.isEmpty() &&
      ZtoryCharacter::roleOf(tnz) == QLatin1String("character");

  // ── I set che sono ARRIVATI COL PERSONAGGIO ─────────────────────────────
  // La mappa di una sotto-scena vive accanto alla scena che la contiene.
  // Importando il personaggio in uno shot, la sotto-scena arriva ma la sua
  // mappa resta accanto alla scena di libreria: qui non c'e'.
  //
  // Si ritrova per la catena che Franco aveva progettato dall'inizio —
  // personaggio -> asset -> scena di libreria -> mappa — e l'aggancio e' il
  // NOME della sotto-scena, che l'import non cambia.
  // ── I set della LIBRERIA, sempre, accanto a quelli dello shot ───────────
  // Prima lo shot ereditava dalla libreria SOLO se non aveva set suoi: appena
  // ne creavi uno, quelli del personaggio sparivano, e un set pubblicato dopo
  // l'import non arrivava (Franco, 2026-09-27). Ora si uniscono: a parita' di
  // nome vince quello dello shot, che e' una variante voluta per la scena.
  // La libreria si trova per la catena sicura — la sotto-scena piu' vicina
  // SOPRA che si chiama come la scena di un personaggio — non per il solo
  // nome della sotto-scena (ztorymouthlibrary.cpp).
  for (MouthApplyTarget &t : out) {
    if (characterScene || t.subScene.isEmpty()) continue;
    MouthMap lib;
    QString who;
    TXshChildLevel *tcl = t.level ? t.level->getChildLevel() : nullptr;
    if (!ZtoryMouthLibrary::libraryMapForSubScene(
            scene, t.subScene, &lib, &who, tcl ? tcl->getXsheet() : nullptr))
      continue;
    int added = 0;
    for (MouthSet s : lib.sets) {
      if (t.map.indexOfSet(s.name) >= 0) continue;
      s.fromLibrary = who;
      t.map.sets.push_back(s);
      added++;
    }
    if (added > 0 && t.fromCharacter.isEmpty()) {
      t.fromCharacter = who;
      t.label = QObject::tr("%1  (sub-scene, + library of %2)")
                    .arg(t.subScene, who);
    }
  }

  // Il ripiego di prima, per il solo NOME della sotto-scena: quando la catena
  // non trova la libreria e lo shot non ha set suoi.
  for (MouthApplyTarget &t : out) {
    if (characterScene) break;
    if (!t.map.sets.isEmpty() || t.subScene.isEmpty()) continue;
    // ⚠️ The chain already knows WHOSE sub-scene this is: then it gets that
    // character's sets or none.  In a real shot one character's «sub» — her
    // library has no sets yet — got another character's nine, the only «sub» with a map,
    // so «not ambiguous» (Franco, 2026-10-11): the same mistake as BRONTOLO on
    // 2026-09-25, by the back door of this fallback.
    TXshChildLevel *tcl = t.level ? t.level->getChildLevel() : nullptr;
    if (!ZtoryMouthLibrary::characterOfSubScene(
             scene, t.subScene, tcl ? tcl->getXsheet() : nullptr)
             .isEmpty())
      continue;
    ZtoryModel *model = ZtoryModel::instance();
    // ⚠️ Si raccolgono TUTTI i candidati invece di fermarsi al primo.
    // L'aggancio e' il solo NOME della sotto-scena, e i nomi generici non sono
    // rari: «sub» e' quello di default. Se due personaggi hanno una sotto-scena
    // che si chiama allo stesso modo, quel nome non identifica nessuno dei due,
    // e prendere il primo dell'elenco mette in mano all'utente le bocche di un
    // altro personaggio senza che niente lo segnali (Franco, lavorando sul
    // lupo si e' visto proporre i set di SOFIA).
    QVector<QPair<QString, MouthMap>> candidates;
    for (const Asset &a : model->assets()) {
      if (!ZtoryModel::isCharacterType(a.type)) continue;
      const QString libScene = model->resolveAssetFile(a);
      if (libScene.isEmpty()) continue;
      MouthMap m;
      if (!ZtoryMouthMap::load(TFilePath(libScene.toStdWString()), t.subScene, m))
        continue;
      if (m.sets.isEmpty()) continue;
      candidates.push_back(qMakePair(a.name, m));
    }
    // Uno solo = il nome individua davvero quel personaggio: e' il caso per cui
    // la catena e' stata fatta. Piu' d'uno = ambiguo, e si preferisce non
    // ereditare niente: la sotto-scena resta senza set e si mappa a mano
    // scegliendo il livello, invece di ereditare la bocca sbagliata.
    if (candidates.size() == 1) {
      t.map           = candidates.first().second;
      t.fromCharacter = candidates.first().first;
      t.label         = QObject::tr("%1  (sub-scene, from %2)")
                    .arg(t.subScene, candidates.first().first);
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

MouthCharacters ZtoryMouthApply::characters(ToonzScene *scene) {
  MouthCharacters out;
  TXsheet *top = workingXsheet();
  if (!scene || !top) return out;
  const auto libs = ZtoryMouthLibrary::characterScenesByName();
  // Everything below a sub-scene belongs to it; the first claim wins (a level
  // shared by two characters stays with the first, left of the other).
  std::function<void(TXsheet *, const QString &, int, QSet<TXsheet *> &)> claim =
      [&](TXsheet *xsh, const QString &key, int depth, QSet<TXsheet *> &seen) {
        if (!xsh || depth > 8 || seen.contains(xsh)) return;
        seen.insert(xsh);
        for (int c = 0; c < xsh->getColumnCount(); c++) {
          int r0 = 0, r1 = -1;
          xsh->getCellRange(c, r0, r1);
          for (int r = r0; r <= r1; r++) {
            const TXshCell cell = xsh->getCell(r, c);
            if (cell.isEmpty() || !cell.m_level) continue;
            TXshLevel *lv = cell.m_level.getPointer();
            if (!out.groupOf.contains(lv)) out.groupOf.insert(lv, key);
            if (TXshChildLevel *cl = lv->getChildLevel())
              claim(cl->getXsheet(), key, depth + 1, seen);
          }
        }
      };
  for (int c = 0; c < top->getColumnCount(); c++) {
    int r0 = 0, r1 = -1;
    top->getCellRange(c, r0, r1);
    QSet<TXshLevel *> done;
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = top->getCell(r, c);
      if (cell.isEmpty() || !cell.m_level) continue;
      TXshLevel *lv = cell.m_level.getPointer();
      if (done.contains(lv)) continue;
      done.insert(lv);
      TXshChildLevel *cl = lv->getChildLevel();
      if (!cl) {  // a level right here: «Other levels»
        if (!out.groupOf.contains(lv)) out.groupOf.insert(lv, QString());
        if (!out.keys.contains(QString())) {
          out.keys << QString();
          out.labels.insert(QString(), QObject::tr("Other levels"));
        }
        continue;
      }
      const QString name = QString::fromStdWString(lv->getName());
      if (out.keys.contains(name)) continue;
      out.keys << name;
      // The column's name, unless it is Tahoma's «Col4», which says nothing.
      QString col;
      if (TStageObject *so = top->getStageObject(top->getColumnObjectId(c)))
        col = QString::fromStdString(so->getName());
      static const QRegularExpression kDefault(QStringLiteral("^Col\\d+$"));
      if (kDefault.match(col).hasMatch()) col.clear();
      // WHO and WHERE, both: the character the column holds (brought from its
      // scene: SOFIA_1 is SOFIA) and the column it is in, which is what the
      // user reads in the xsheet.  Only the sub-scene right under the column
      // used to be looked at: in sh370 the wolf showed as «LUPO» (its column,
      // renamed SH020, unsaid) and SOFIA — one sub-scene deeper — as «SH030
      // (in this scene)», so the list had one name of each kind and neither
      // said both (Franco, 2026-10-12).  The character is now searched at any
      // depth, the first found in column order.
      QString libKey;  // the sub-scene matching a character's library
      std::function<void(TXsheet *, int)> findChar = [&](TXsheet *xsh, int depth) {
        if (!xsh || depth > 8 || !libKey.isEmpty()) return;
        for (int cc = 0; cc < xsh->getColumnCount() && libKey.isEmpty(); cc++) {
          int a = 0, b = -1;
          xsh->getCellRange(cc, a, b);
          for (int rr = a; rr <= b && libKey.isEmpty(); rr++) {
            const TXshCell cell2 = xsh->getCell(rr, cc);
            if (cell2.isEmpty() || !cell2.m_level) continue;
            TXshChildLevel *sub = cell2.m_level->getChildLevel();
            if (!sub) continue;
            const QString n = QString::fromStdWString(cell2.m_level->getName());
            if (libs.contains(ZtoryMouthLibrary::characterKey(n))) libKey = n;
            else findChar(sub->getXsheet(), depth + 1);
            break;  // one cell is enough: a column exposes one sub-scene
          }
        }
      };
      if (libs.contains(ZtoryMouthLibrary::characterKey(name)))
        libKey = name;
      else
        findChar(cl->getXsheet(), 1);
      // No sub-scene named as a character (a nesting taken out removed it):
      // the character the DRAWINGS belong to, as the mouth library now finds
      // it (ZtoryMouthLibrary, by content).
      QString byContent;
      if (libKey.isEmpty()) {
        byContent =
            ZtoryMouthLibrary::characterOfSubScene(scene, name, cl->getXsheet());
        std::function<void(TXsheet *, int)> look = [&](TXsheet *xsh, int depth) {
          if (!xsh || depth > 8 || !byContent.isEmpty()) return;
          for (int cc = 0; cc < xsh->getColumnCount() && byContent.isEmpty(); cc++) {
            int a = 0, b = -1;
            xsh->getCellRange(cc, a, b);
            for (int rr = a; rr <= b; rr++) {
              const TXshCell cell2 = xsh->getCell(rr, cc);
              if (cell2.isEmpty() || !cell2.m_level) continue;
              if (TXshChildLevel *sub = cell2.m_level->getChildLevel()) {
                byContent = ZtoryMouthLibrary::characterOfSubScene(
                    scene, QString::fromStdWString(cell2.m_level->getName()),
                    sub->getXsheet());
                if (byContent.isEmpty()) look(sub->getXsheet(), depth + 1);
              }
              break;
            }
          }
        };
        if (byContent.isEmpty()) look(cl->getXsheet(), 1);
      }
      QString who = byContent;
      if (!libKey.isEmpty()) {
        auto lib = libs.constFind(ZtoryMouthLibrary::characterKey(libKey));
        // The asset's name; with the scene's when they differ (in a real
        // project the asset PRINCE points at PRINCESS_01.tnz).
        const QString asset = lib.value().first;
        const QString scn =
            QFileInfo(lib.value().second.getQString()).completeBaseName();
        who = asset.compare(scn, Qt::CaseInsensitive) == 0
                  ? asset
                  : QObject::tr("%1 (%2)").arg(asset, scn);
        // Library lookups by group name hold only when the column IS the
        // character's sub-scene; deeper ones are found by column path
        // (ZtoryMouthLibrary::libraryName).
        if (libKey == name) out.libraryKeys.insert(name);
      }
      QString label;
      if (!who.isEmpty())
        label = (col.isEmpty() ||
                 (!libKey.isEmpty() && col.compare(libKey, Qt::CaseInsensitive) == 0) ||
                 col.compare(who, Qt::CaseInsensitive) == 0)
                    ? who
                    : QObject::tr("%1 — column %2").arg(who, col);
      else
        label = col.isEmpty()
                    ? QObject::tr("sub-scene %1, column %2 (in this scene)")
                          .arg(name)
                          .arg(c + 1)
                    : QObject::tr("%1 (in this scene)").arg(col);
      out.labels.insert(name, label);
      out.groupOf.insert(lv, name);
      QSet<TXsheet *> seen;
      claim(cl->getXsheet(), name, 1, seen);
    }
  }
  return out;
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
    // viseme no. Conta la PROPORZIONE, non poche celle: la colonna delle
    // parole del lip sync scrive «rest» in ogni pausa, e con «bastano tre
    // celle» passava per una colonna di fonemi — era la prima della tendina,
    // e Apply leggeva le parole: 44 fotogrammi su 303, tutti pause (Franco,
    // 2026-09-27, sh040). In una colonna di fonemi quasi tutte le celle
    // piene sono viseme (qualche «—» di punteggiatura al massimo).
    int hits = 0, filled = 0;
    for (int r = r0; r <= r1; r++) {
      const QString text = phonemeAt(xsh, col, r);
      if (text.isEmpty()) continue;
      filled++;
      if (ZtoryMouthMap::shapeIndex(text) >= 0) hits++;
    }
    if (hits >= 3 && hits * 10 >= filled * 8) out.push_back(col);
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
  //
  // ⚠️ Per DESTINAZIONE, non una sola. Le bocche di un personaggio possono
  // stare in piu' posti: SOFIA le ha nella colonna principale e anche dentro
  // la sotto-scena della girata. Con una destinazione unica — quella del primo
  // fotogramma — le righe trovate dentro la girata (0-9 LI') finivano scritte
  // sulle righe 0-9 della colonna principale: bocche sbagliate all'inizio
  // dello shot (Franco, 2026-09-27, sh040).
  using Dest = QPair<TXsheet *, int>;
  QMap<Dest, QMap<int, TXshCell>> planned;
  QMap<Dest, QMap<int, QString>> plannedShape;  // per rilevare i conflitti

  for (const MouthApplyRange &rg : ranges) {
    const int si = target.map.indexOfSet(rg.setName);
    if (si < 0) {
      rep.noSet += rg.to - rg.from + 1;
      continue;
    }
    const MouthSet &set = target.map.sets[si];

    for (int f = rg.from; f <= rg.to; f++) {
      const QString shape = phonemeAt(top, phonemeCol, f - 1);
      if (shape.isEmpty()) { rep.noPhoneme++; continue; }
      const int idx = ZtoryMouthMap::shapeIndex(shape);
      if (idx < 0) { rep.unknownShape++; continue; }

      // Dove cade questo fotogramma dentro l'albero delle sotto-scene.
      TXsheet *xsh = nullptr;
      int col = -1, row = -1;
      if (!locate(top, target.level, f - 1, 0, &xsh, &col, &row)) {
        // Il personaggio non e' esposto qui: non e' un errore, e' una pausa in
        // cui non c'e' niente da animare.
        rep.offStage++;
        continue;
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
      const Dest dest(xsh, col);
      auto prev = plannedShape[dest].constFind(row);
      if (prev != plannedShape[dest].constEnd() && prev.value() != shape) {
        if (!rep.conflicts.contains(row + 1)) rep.conflicts.push_back(row + 1);
        continue;
      }
      plannedShape[dest][row] = shape;

      TXshCell cell = xsh->getCell(row, col);
      cell.m_level   = target.level;
      cell.m_frameId = chosen->frameId;
      planned[dest][row] = cell;
    }
  }

  if (planned.isEmpty()) return rep;

  // Una sola voce nella cronologia: annullare deve togliere tutto il lip sync
  // applicato, non una destinazione per volta.
  TUndoManager::manager()->beginBlock();
  for (auto d = planned.constBegin(); d != planned.constEnd(); ++d) {
    TXsheet *dx = d.key().first;
    const int dc = d.key().second;
    QMap<int, TXshCell> before;
    for (auto it = d.value().constBegin(); it != d.value().constEnd(); ++it)
      before[it.key()] = dx->getCell(it.key(), dc);
    for (auto it = d.value().constBegin(); it != d.value().constEnd(); ++it) {
      dx->setCell(it.key(), dc, it.value());
      rep.written++;
    }
    TUndoManager::manager()->add(new MouthApplyUndo(dx, dc, before, d.value()));
    QString where = QObject::tr("column %1").arg(dc + 1);
    if (TStageObject *so = dx->getStageObject(TStageObjectId::ColumnId(dc))) {
      const QString n = QString::fromStdString(so->getName());
      if (!n.isEmpty()) where = n;
    }
    rep.destinations << QObject::tr("%1 (%2 frames)").arg(where).arg(d.value().size());
  }
  TUndoManager::manager()->endBlock();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);

  QSet<TXsheet *> destXshs;
  for (auto d = planned.constBegin(); d != planned.constEnd(); ++d)
    destXshs.insert(d.key().first);
  invalidateMouthDestinations(destXshs);
  return rep;
}
