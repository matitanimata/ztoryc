#include "ztorymouthlibrary.h"

#include "tapp.h"
#include "ztoryassetpreview.h"  // ztoryPsdsUsedByScene
#include "ztorycharacter.h"
#include "ztorymodel.h"

#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txshsimplelevel.h"

#include <QCheckBox>
#include <QInputDialog>
#include <QMenu>
#include <QUuid>
#include <memory>
#include <QMainWindow>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <cstdio>

namespace {

const char *kAskOnSaveKey = "Ztoryc/MouthLibrary/askOnSave";
const char *kAskOnOpenKey = "Ztoryc/MouthLibrary/askOnOpen";

// Everything that makes two sets the same set.
QString signatureOf(const MouthSet &s) {
  QStringList parts{s.name, s.view, s.expression, s.variant};
  for (int i = 0; i < 10; i++) {
    QStringList slot;
    for (const MouthTarget &t : s.mouths[i])
      slot << t.levelName + ":" +
                  QString::fromStdString(t.frameId.expand()) + ":" +
                  t.poseName;
    parts << slot.join(",");
  }
  return parts.join("|");
}

// The highest frame of the anchor the set points at (0 = none).
int maxAnchorFrame(const MouthSet &s) {
  int top = 0;
  for (int i = 0; i < 10; i++)
    for (const MouthTarget &t : s.mouths[i])
      if (t.isAnchorLevel() && !t.isPose())
        top = std::max(top, t.frameId.getNumber());
  return top;
}

struct Library {
  QString   character;
  TFilePath scene;  // the character's scene (decoded)
};

// The characters' libraries, by the NAME of their scene file: it is the name
// the sub-scene takes when the scene is brought into a shot (loadChildLevel
// names it after the file), so it is what ties a shot back to its library.
QHash<QString, Library> librariesByName() {
  QHash<QString, Library> out;
  ZtoryModel *m = ZtoryModel::instance();
  for (const Asset &a : m->assets()) {
    if (!ZtoryModel::isCharacterType(a.type)) continue;
    const QString file = m->resolveAssetFile(a);
    if (!file.endsWith(".tnz", Qt::CaseInsensitive)) continue;
    Library l;
    l.character = a.name;
    l.scene     = TFilePath(file.toStdWString());
    out.insert(QFileInfo(file).completeBaseName().toLower(), l);
  }
  return out;
}

// Down the tree of sub-scenes from `top`, the first one called `name`: its
// xsheet, the names of the sub-scenes above it (outermost first) and, in
// `columns`, the column of each step down — above ones, then its own.
// With `target`, that very sub-scene instead: names repeat (see
// libraryMapForSubScene in the header).
TXsheet *findSubScene(TXsheet *top, const QString &name, QStringList *above,
                      QVector<int> *columns = nullptr,
                      TXsheet *target = nullptr) {
  QVector<int> colPath;
  std::function<TXsheet *(TXsheet *, QStringList, QSet<TXsheet *> &, int)>
      walk = [&](TXsheet *xsh, QStringList path, QSet<TXsheet *> &seen,
                 int depth) -> TXsheet * {
    if (!xsh || depth > 8 || seen.contains(xsh)) return nullptr;
    seen.insert(xsh);
    for (int c = 0; c < xsh->getColumnCount(); c++) {
      TXshColumn *col = xsh->getColumn(c);
      if (!col || col->isEmpty()) continue;
      int r0 = 0, r1 = -1;
      xsh->getCellRange(c, r0, r1);
      QSet<TXshLevel *> done;
      for (int r = r0; r <= r1; r++) {
        const TXshCell cell = xsh->getCell(r, c);
        if (cell.isEmpty() || !cell.m_level) continue;
        TXshChildLevel *cl = cell.m_level->getChildLevel();
        if (!cl || done.contains(cell.m_level.getPointer())) continue;
        done.insert(cell.m_level.getPointer());
        const QString n = QString::fromStdWString(cell.m_level->getName());
        colPath.push_back(c);
        if (target ? cl->getXsheet() == target : n == name) {
          if (above) *above = path;
          if (columns) *columns = colPath;
          return cl->getXsheet();
        }
        QStringList deeper = path;
        deeper << n;
        if (TXsheet *found = walk(cl->getXsheet(), deeper, seen, depth + 1))
          return found;
        colPath.pop_back();
      }
    }
    return nullptr;
  };
  QSet<TXsheet *> seen;
  return walk(top, QStringList(), seen, 0);
}

// The library of the sub-scene `sub`: the nearest sub-scene ABOVE it named as
// a character's scene. The sub-scene's own name is no help — «sub» is
// Tahoma's default, SOFIA and BRONTOLO both have one.
// `inside`: the columns from the character's sub-scene down to `sub` — the
// same steps lead to it in the character's own scene (libraryName below).
bool libraryOfSubScene(TXsheet *top, const QString &sub,
                       const QHash<QString, Library> &libs, Library *out,
                       QVector<int> *inside = nullptr,
                       TXsheet *subXsh = nullptr) {
  QStringList above;
  QVector<int> columns;
  if (!findSubScene(top, sub, &above, &columns, subXsh)) return false;
  for (int i = above.size() - 1; i >= 0; --i) {
    // Not the bare name: a scene brought into a shot takes a «_1».
    auto it = libs.constFind(ZtoryMouthLibrary::characterKey(above[i]));
    if (it == libs.constEnd()) it = libs.constFind(above[i].toLower());
    if (it != libs.constEnd()) {
      *out = it.value();
      if (inside) *inside = columns.mid(i + 1);
      return true;
    }
  }
  return false;
}

// What the sub-scene is called in the character's OWN scene.  Bringing the
// character into a shot that already has a sub-scene of the same name makes
// Tahoma rename its copy: SOFIA's «sub» became «sub_3» in sh100, and her
// sets — written for «sub» — were looked for under «sub_3» and not found
// (Franco, 2026-10-11: «ho piu' set ma nello shot me ne vede solo uno»).  The
// copy has the scene's structure, so the same columns lead to the original
// sub-scene; its name is read there.  If the structure no longer matches
// (the copy was edited), the shot's name, as before.
QString libraryName(const Library &lib, const QVector<int> &inside,
                    const QString &shotName) {
  if (inside.isEmpty()) return shotName;
  static QHash<QString, QString> cache;  // the scene file is read once
  const QString file = lib.scene.getQString();
  QString key = file + "|" +
                QString::number(QFileInfo(file).lastModified().toSecsSinceEpoch());
  for (int c : inside) key += "/" + QString::number(c);
  auto hit = cache.constFind(key);
  if (hit != cache.constEnd()) return hit->isEmpty() ? shotName : *hit;
  QString found;
  try {
    ToonzScene scene;
    scene.loadTnzFile(lib.scene);
    TXsheet *xsh = scene.getXsheet();
    for (int step = 0; xsh && step < inside.size(); step++) {
      const int c = inside[step];
      TXshColumn *col = (c < xsh->getColumnCount()) ? xsh->getColumn(c) : nullptr;
      TXsheet *next = nullptr;
      if (col && !col->isEmpty()) {
        int r0 = 0, r1 = -1;
        xsh->getCellRange(c, r0, r1);
        for (int r = r0; r <= r1 && !next; r++) {
          const TXshCell cell = xsh->getCell(r, c);
          if (cell.isEmpty() || !cell.m_level) continue;
          TXshChildLevel *cl = cell.m_level->getChildLevel();
          if (!cl) continue;
          if (step == inside.size() - 1)
            found = QString::fromStdWString(cell.m_level->getName());
          next = cl->getXsheet();
        }
      }
      xsh = next;
    }
  } catch (...) {
    found.clear();
  }
  cache.insert(key, found);
  return found.isEmpty() ? shotName : found;
}

// The library recognised by WHAT the sub-scene holds, when its name and the
// names above it say nothing.  sh370 (Franco, 2026-10-12): SOFIA's chain was
// SH030 → sub_6 → SOFIA → sub_5 …; taking one nesting out removed «SOFIA»,
// the only name that told whose mouths they were, and the shot saw one set
// instead of nine.  The mouth drawings did not change: same files as in the
// character's own scene.  So: the files of the levels right inside `sub`
// (by file name — Tahoma renames a level «…_1» on import, the file keeps its
// name), looked for in each character's scene; the first scene holding them
// is the library, and the sub-scene holding them there names the sets.
bool libraryByContent(TXsheet *top, const QString &sub,
                      const QHash<QString, Library> &libs, Library *out,
                      QString *libSub, TXsheet *subXsh = nullptr) {
  TXsheet *xsh = findSubScene(top, sub, nullptr, nullptr, subXsh);
  if (!xsh) {
    if (qEnvironmentVariableIsSet("ZTORYC_SCENETEST"))
      fprintf(stderr, "[ZTORY] bycontent '%s': sub-scene not found\n", qPrintable(sub));
    return false;
  }
  QSet<QString> files;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    int r0 = 0, r1 = -1;
    xsh->getCellRange(c, r0, r1);
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = xsh->getCell(r, c);
      if (cell.isEmpty() || !cell.m_level || cell.m_level->getChildLevel())
        continue;
      const QString f = QString::fromStdWString(
          cell.m_level->getPath().withoutParentDir().getWideString());
      if (!f.isEmpty()) files.insert(f.toLower());
      break;
    }
  }
  static const bool diag = qEnvironmentVariableIsSet("ZTORYC_SCENETEST");
  if (diag)
    fprintf(stderr, "[ZTORY] bycontent '%s': %d files (%s), %d libraries\n",
            qPrintable(sub), files.size(),
            qPrintable(QStringList(files.values()).join(",")), libs.size());
  if (files.isEmpty()) return false;
  // Per character scene: drawing file → the sub-scene holding it.  Read once
  // per version of the file, like libraryName().
  static QHash<QString, QHash<QString, QString>> index;
  // The library holding MOST of the drawings, not the first holding one: a
  // generic drawing («A», a..png) is in several characters' scenes, and the
  // first match picked another character, with no sets (sh370, 2026-10-12).
  int bestScore = 0;
  bool tie      = false;
  for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
    const QString file = it.value().scene.getQString();
    const QString key =
        file + "|" +
        QString::number(QFileInfo(file).lastModified().toSecsSinceEpoch());
    if (!index.contains(key)) {
      QHash<QString, QString> map;
      try {
        ToonzScene scene;
        scene.loadTnzFile(it.value().scene);
        std::function<void(TXsheet *, int, QSet<TXsheet *> &)> walk =
            [&](TXsheet *x, int depth, QSet<TXsheet *> &seen) {
              if (!x || depth > 8 || seen.contains(x)) return;
              seen.insert(x);
              for (int c = 0; c < x->getColumnCount(); c++) {
                int r0 = 0, r1 = -1;
                x->getCellRange(c, r0, r1);
                for (int r = r0; r <= r1; r++) {
                  const TXshCell cell = x->getCell(r, c);
                  if (cell.isEmpty() || !cell.m_level) continue;
                  if (TXshChildLevel *cl = cell.m_level->getChildLevel()) {
                    const QString name =
                        QString::fromStdWString(cell.m_level->getName());
                    TXsheet *in = cl->getXsheet();
                    for (int cc = 0; in && cc < in->getColumnCount(); cc++) {
                      int a = 0, b = -1;
                      in->getCellRange(cc, a, b);
                      for (int rr = a; rr <= b; rr++) {
                        const TXshCell c2 = in->getCell(rr, cc);
                        if (c2.isEmpty() || !c2.m_level ||
                            c2.m_level->getChildLevel())
                          continue;
                        const QString f = QString::fromStdWString(
                            c2.m_level->getPath().withoutParentDir()
                                .getWideString()).toLower();
                        if (!f.isEmpty() && !map.contains(f)) map.insert(f, name);
                        break;
                      }
                    }
                    walk(in, depth + 1, seen);
                  }
                  break;
                }
              }
            };
        QSet<TXsheet *> seen;
        walk(scene.getXsheet(), 0, seen);
      } catch (...) {
        map.clear();
      }
      index.insert(key, map);
    }
    const QHash<QString, QString> &map = index.value(key);
    if (diag) {
      fprintf(stderr, "[ZTORY] bycontent   %s: %d drawings indexed\n",
              qPrintable(QFileInfo(file).fileName()), map.size());
    }
    int score = 0;
    QHash<QString, int> subVotes;  // the library sub-scene most of them are in
    for (const QString &f : files) {
      auto hit = map.constFind(f);
      if (hit == map.constEnd()) continue;
      score++;
      subVotes[hit.value()]++;
    }
    if (score == 0 || score < bestScore) continue;
    if (score == bestScore) {
      tie = true;
      continue;
    }
    bestScore = score;
    tie       = false;
    *out      = it.value();
    if (libSub) {
      int votes = 0;
      for (auto v = subVotes.constBegin(); v != subVotes.constEnd(); ++v)
        if (v.value() > votes) votes = v.value(), *libSub = v.key();
    }
  }
  // Two characters equally plausible: better no library than the wrong one.
  return bestScore > 0 && !tie;
}

// How many frames the library's own copy of the sub-scene has (-1 unknown).
int librarySubSceneFrames(const TFilePath &libScene, const QString &sub) {
  try {
    ToonzScene lib;
    lib.loadTnzFile(libScene);
    TXsheet *xsh = findSubScene(lib.getXsheet(), sub, nullptr);
    return xsh ? xsh->getFrameCount() : -1;
  } catch (...) {
    return -1;
  }
}

// The sets of `from` that `to` does not have, or has differently.
void compareSets(const MouthMap &from, const MouthMap &to,
                 MouthLibraryItem *item) {
  for (const MouthSet &s : from.sets) {
    if (!s.isUsable()) continue;
    const int i = to.indexOfSet(s.name);
    if (i < 0)
      item->added << s;
    else if (signatureOf(to.sets[i]) != signatureOf(s))
      item->changed << s;
  }
}

// The physical PSD behind a layer level «name#7#group.psd».
QString physicalPsdName(const TFilePath &level) {
  const QString name = QString::fromStdWString(level.getWideName());
  const int hash     = name.indexOf('#');
  return (hash < 0 ? name : name.left(hash)) + ".psd";
}

// What a scene has mapped. False for an untitled scene or a character's own
// scene, which IS the library.
bool scanScene(ToonzScene *scene, TFilePath *scenePath, QSet<QString> *subs,
               QVector<TFilePath> *levels) {
  if (!scene || scene->isUntitled()) return false;
  *scenePath = scene->decodeFilePath(scene->getScenePath());
  if (ZtoryCharacter::roleOf(scenePath->getQString()) ==
      QLatin1String("character"))
    return false;
  ZtoryMouthMap::mappedSubScenes(*scenePath, subs);
  TLevelSet *ls = scene->getLevelSet();
  for (int i = 0; i < ls->getLevelCount(); i++) {
    TXshSimpleLevel *sl = ls->getLevel(i)->getSimpleLevel();
    if (!sl || sl->getPath().getType() != "psd") continue;
    const TFilePath level = scene->decodeFilePath(sl->getPath());
    if (ZtoryMouthMap::exists(level)) levels->push_back(level);
  }
  return true;
}

struct LevelPair {
  QString   character;
  TFilePath shotLevel, libLevel;
};

// For each mapped level of the shot, the library's copy of the same layer:
// beside the PSD the character's scene is built from, same file name. Not
// when the shot uses the library's own file (Load): already shared.
QVector<LevelPair> levelPairs(const QVector<TFilePath> &levels,
                              const QHash<QString, Library> &libs) {
  QVector<LevelPair> out;
  QHash<QString, QStringList> libPsds;  // character -> its PSDs, read once
  for (const TFilePath &shotLevel : levels) {
    const QString psdName = physicalPsdName(shotLevel);
    for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
      QStringList &psds = libPsds[it.value().character];
      if (psds.isEmpty())
        psds = ztoryPsdsUsedByScene(it.value().scene.getQString());
      for (const QString &p : psds) {
        if (QFileInfo(p).fileName().compare(psdName, Qt::CaseInsensitive))
          continue;
        const TFilePath libLevel =
            TFilePath(QFileInfo(p).absolutePath().toStdWString()) +
            shotLevel.withoutParentDir();
        if (libLevel.getQString() == shotLevel.getQString()) continue;
        out.push_back({it.value().character, shotLevel, libLevel});
      }
    }
  }
  return out;
}

// The one dialog, both ways: a box per set, new ones ticked, different ones
// not (replacing is the choice to make on purpose), and those that point at
// missing drawings not ticked either. Returns the chosen sets per item.
bool chooseSets(QWidget *parent, const QString &title, const QString &text,
                const QVector<MouthLibraryItem> &items, const char *askKey,
                const QString &dontAskText,
                QVector<QVector<MouthSet>> *chosen) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  dlg.setMinimumWidth(480);
  auto *lay  = new QVBoxLayout(&dlg);
  auto *head = new QLabel(text, &dlg);
  head->setWordWrap(true);
  lay->addWidget(head);

  auto *list = new QListWidget(&dlg);
  struct Pick { int item; const MouthSet *set; };
  QVector<Pick> picks;
  auto add = [&](int i, const MouthSet &s, bool changed) {
    const bool missing = items[i].missingFrames.contains(s.name);
    QString what = changed ? QObject::tr("differs — replaces it")
                           : QObject::tr("new");
    if (missing)
      what += QObject::tr("; ⚠️ uses drawings the library does not have");
    auto *w = new QListWidgetItem(
        QObject::tr("%1: «%2» — %3").arg(items[i].label, s.name, what), list);
    w->setFlags(w->flags() | Qt::ItemIsUserCheckable);
    w->setCheckState(!changed && !missing ? Qt::Checked : Qt::Unchecked);
    picks << Pick{i, &s};
  };
  for (int i = 0; i < items.size(); i++) {
    for (const MouthSet &s : items[i].added) add(i, s, false);
    for (const MouthSet &s : items[i].changed) add(i, s, true);
  }
  lay->addWidget(list);

  QCheckBox *dontAsk = nullptr;
  if (askKey) {
    dontAsk = new QCheckBox(dontAskText, &dlg);
    lay->addWidget(dontAsk);
  }
  auto *bb = new QDialogButtonBox(&dlg);
  bb->addButton(QObject::tr("OK"), QDialogButtonBox::AcceptRole);
  bb->addButton(askKey ? QObject::tr("Not now") : QObject::tr("Cancel"),
                QDialogButtonBox::RejectRole);
  QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  lay->addWidget(bb);

  const bool ok = dlg.exec() == QDialog::Accepted;
  if (dontAsk && dontAsk->isChecked()) QSettings().setValue(askKey, false);
  if (!ok) return false;
  chosen->resize(items.size());
  for (int p = 0; p < picks.size(); p++)
    if (list->item(p)->checkState() == Qt::Checked)
      (*chosen)[picks[p].item] << *picks[p].set;
  return true;
}

// Adds / replaces `sets` in the map at `owner`, keeping the rest. The
// character is taken from `fallbackOwner` when the map had none yet.
bool mergeInto(const TFilePath &owner, const QString &sub,
               const TFilePath &fallbackOwner, const QVector<MouthSet> &sets,
               QString *why) {
  MouthMap map;
  QString readError;
  if (!ZtoryMouthMap::load(owner, sub, map, &readError) &&
      QFile::exists(ZtoryMouthMap::pathFor(owner).getQString())) {
    // The file is there and cannot be read: saving would rewrite it with
    // only these sets, and the rest would be lost.
    if (why) *why = readError;
    return false;
  }
  if (map.characterName.isEmpty()) {
    MouthMap other;
    ZtoryMouthMap::load(fallbackOwner, sub, other);
    map.characterUuid = other.characterUuid;
    map.characterName = other.characterName;
  }
  for (const MouthSet &s : sets) {
    const int i = map.indexOfSet(s.name);
    if (i >= 0)
      map.sets[i] = s;
    else
      map.sets << s;
  }
  return ZtoryMouthMap::save(owner, sub, map, why);
}

void report(QWidget *parent, const QString &title, const QStringList &done,
            const QStringList &failed) {
  QString msg;
  if (!done.isEmpty()) msg += QObject::tr("Done:\n%1").arg(done.join("\n"));
  if (!failed.isEmpty())
    msg += (msg.isEmpty() ? QString() : QString("\n\n")) +
           QObject::tr("Not done:\n%1").arg(failed.join("\n"));
  if (!msg.isEmpty()) QMessageBox::information(parent, title, msg);
}

}  // namespace

//----------------------------------------------------------------------------

QVector<MouthLibraryItem> ZtoryMouthLibrary::pending(ToonzScene *scene) {
  QVector<MouthLibraryItem> out;
  TFilePath scenePath;
  QSet<QString> subs;
  QVector<TFilePath> levels;
  // Called at every save by the user: the libraries (a folder listing per
  // character) are looked for only when the scene HAS mouth maps.
  if (!scanScene(scene, &scenePath, &subs, &levels)) return out;
  if (subs.isEmpty() && levels.isEmpty()) return out;
  const QHash<QString, Library> libs = librariesByName();
  if (libs.isEmpty()) return out;

  for (const QString &sub : subs) {
    Library lib;
    QVector<int> inside;
    if (!libraryOfSubScene(scene->getXsheet(), sub, libs, &lib, &inside))
      continue;
    // The library knows the sub-scene by ITS name (libraryName).
    const QString libSub = libraryName(lib, inside, sub);
    MouthMap shotMap, libMap;
    if (!ZtoryMouthMap::load(scenePath, sub, shotMap)) continue;
    ZtoryMouthMap::load(lib.scene, libSub, libMap);  // none yet is fine
    MouthLibraryItem item;
    item.character = lib.character;
    item.subScene  = sub;
    item.libSubScene = libSub;
    item.label     = QObject::tr("%1 ▸ %2 (sub-scene)").arg(lib.character, sub);
    item.shotOwner = scenePath;
    item.libOwner  = lib.scene;
    compareSets(shotMap, libMap, &item);
    if (item.added.isEmpty() && item.changed.isEmpty()) continue;
    // Drawings made in the shot: the set would point at frames the
    // library's sub-scene does not have.
    const int frames = librarySubSceneFrames(lib.scene, libSub);
    if (frames >= 0) {
      for (const QVector<MouthSet> *v : {&item.added, &item.changed})
        for (const MouthSet &s : *v)
          if (maxAnchorFrame(s) > frames) item.missingFrames.insert(s.name);
    }
    out << item;
  }

  for (const LevelPair &lp : levelPairs(levels, libs)) {
    MouthMap shotMap, libMap;
    if (!ZtoryMouthMap::load(lp.shotLevel, QString(), shotMap)) continue;
    ZtoryMouthMap::load(lp.libLevel, QString(), libMap);
    MouthLibraryItem item;
    item.character = lp.character;
    item.label     = QObject::tr("%1 ▸ %2").arg(
        lp.character, QString::fromStdWString(lp.shotLevel.getWideName()));
    item.shotOwner = lp.shotLevel;
    item.libOwner  = lp.libLevel;
    compareSets(shotMap, libMap, &item);
    if (!item.added.isEmpty() || !item.changed.isEmpty()) out << item;
  }
  return out;
}

void ZtoryMouthLibrary::showPublishDialog(QWidget *parent, bool onSave) {
  const QString title = QObject::tr("Publish mouths to the library");
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
  const QVector<MouthLibraryItem> items = pending(scene);
  if (items.isEmpty()) {
    if (!onSave)
      QMessageBox::information(
          parent, title,
          QObject::tr("Nothing to publish: every mouth set of this scene is "
                      "already in its character's library."));
    return;
  }
  QVector<QVector<MouthSet>> chosen;
  if (!chooseSets(parent, title,
                  QObject::tr("These mouth sets are in this scene and not in "
                              "the character's library. Published, every "
                              "scene with the character sees them."),
                  items, onSave ? kAskOnSaveKey : nullptr,
                  QObject::tr("Don't ask again when saving (the Publish button "
                              "in ZtoRig ▸ Mouths stays)"),
                  &chosen))
    return;
  QStringList done, failed;
  for (int i = 0; i < items.size(); i++) {
    if (chosen[i].isEmpty()) continue;
    QString why;
    // Into the library under ITS name for the sub-scene, and the sets with it.
    QVector<MouthSet> sets = chosen[i];
    const QString libSub = items[i].libSubScene.isEmpty() ? items[i].subScene
                                                          : items[i].libSubScene;
    if (!items[i].subScene.isEmpty())
      for (MouthSet &s : sets) s.subScene = libSub;
    if (mergeInto(items[i].libOwner, libSub, items[i].shotOwner, sets, &why))
      done << QObject::tr("%1: %2 set(s)").arg(items[i].label).arg(chosen[i].size());
    else
      failed << QObject::tr("%1: %2").arg(items[i].label, why);
  }
  report(parent, title, done, failed);
}

void ZtoryMouthLibrary::runWhenNoModal(std::function<void()> fn) {
  auto *timer = new QTimer(qApp);
  auto tries  = std::make_shared<int>(0);
  QObject::connect(timer, &QTimer::timeout, timer, [timer, fn, tries]() {
    if (QApplication::activeModalWidget() && ++*tries < 240) return;
    timer->stop();
    timer->deleteLater();
    if (!QApplication::activeModalWidget()) fn();
  });
  timer->start(250);
}

QWidget *ZtoryMouthLibrary::dialogParent() {
  return TApp::instance()->getMainWindow();
}

void ZtoryMouthLibrary::offerOnSave() {
  if (!QSettings().value(kAskOnSaveKey, true).toBool()) return;
  // After the save has finished, and with no modal window open.
  runWhenNoModal([]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!pending(scene).isEmpty()) showPublishDialog(dialogParent(), true);
  });
}

// The library whose MARKED mouth column (MouthColumnTag) uses the same
// drawings as `subXsh`: names play no part.  The mark covering most of them
// wins; MORE than half of the marked drawings must be here (SOFIA's mouths
// are «a..png» + her PSD group: «a..png» alone, a name any character can
// have, is not her mouths); a tie between two characters
// gives nothing.  `libSub` is the marked sub-scene's name in the library,
// the one its sets are filed under.
static bool libraryByMark(TXsheet *subXsh, const QHash<QString, Library> &libs,
                   Library *out, QString *libSub) {
  if (!subXsh) return false;
  const QSet<QString> here = ZtoryMouthLibrary::drawingFingerprint(subXsh);
  if (here.isEmpty()) return false;
  int best = 0;
  bool tie = false;
  QString bestChar;
  for (auto it = libs.constBegin(); it != libs.constEnd(); ++it) {
    QVector<MouthColumnTag> tags;
    ZtoryMouthMap::loadColumnTags(it.value().scene, &tags);
    for (const MouthColumnTag &t : tags) {
      int n = 0;
      for (const QString &f : t.files)
        if (here.contains(f)) n++;
      // Both ways: most of the marked drawings are here, AND most of what is
      // here is the marked drawings — the sub-scenes ABOVE the mouths contain
      // them too (SOFIA_1, sub_5 hold SOFIA's «sub»), and must not pass for it.
      if (n == 0 || n * 2 <= t.files.size() || n * 2 <= here.size() ||
          n < best)
        continue;
      if (n == best) {
        if (it.value().character != bestChar) tie = true;
        continue;
      }
      best     = n;
      tie      = false;
      bestChar = it.value().character;
      *out     = it.value();
      if (libSub) *libSub = t.subScene;
    }
  }
  return best > 0 && !tie;
}

// How many frames the library's own copy of the sub-scene has (-1 unknown).
bool ZtoryMouthLibrary::libraryMapForSubScene(ToonzScene *scene,
                                              const QString &sub,
                                              MouthMap *map,
                                              QString *character,
                                              TXsheet *subXsh) {
  if (!scene || scene->isUntitled()) return false;
  const TFilePath scenePath = scene->decodeFilePath(scene->getScenePath());
  if (ZtoryCharacter::roleOf(scenePath.getQString()) ==
      QLatin1String("character"))
    return false;
  Library lib;
  QVector<int> inside;
  QString libSub;
  const QHash<QString, Library> libs = librariesByName();
  // First the marked mouth column, recognised by its drawings.
  if (libraryByMark(subXsh, libs, &lib, &libSub) &&
      ZtoryMouthMap::load(lib.scene, libSub, *map) && !map->sets.isEmpty()) {
    if (qEnvironmentVariableIsSet("ZTORYC_SCENETEST"))
      fprintf(stderr, "[ZTORY] mapfor '%s': by mark -> '%s' (%s) %d sets\n",
              qPrintable(sub), qPrintable(libSub), qPrintable(lib.character),
              (int)map->sets.size());
    for (MouthSet &s : map->sets) s.subScene = sub;
    if (character) *character = lib.character;
    return true;
  }
  lib    = Library();
  libSub.clear();
  *map   = MouthMap();
  // Then the names, as before the marks existed.
  if (libraryOfSubScene(scene->getXsheet(), sub, libs, &lib, &inside, subXsh))
    libSub = libraryName(lib, inside, sub);
  if (qEnvironmentVariableIsSet("ZTORYC_SCENETEST"))
    fprintf(stderr, "[ZTORY] mapfor '%s': by name -> '%s' (%s)\n", qPrintable(sub),
            qPrintable(libSub), qPrintable(lib.character));
  // By name failed, or found a library without sets for it: by content.
  if (libSub.isEmpty() || !ZtoryMouthMap::load(lib.scene, libSub, *map) ||
      map->sets.isEmpty()) {
    *map = MouthMap();
    if (!libraryByContent(scene->getXsheet(), sub, libs, &lib, &libSub, subXsh))
      return false;
    if (!ZtoryMouthMap::load(lib.scene, libSub, *map) || map->sets.isEmpty())
      return false;
  }
  // In this scene the sets belong to ITS sub-scene: saved here, they must not
  // land on another one that happens to carry the library's name.
  for (MouthSet &s : map->sets) s.subScene = sub;
  if (character) *character = lib.character;
  return true;
}

QVector<MouthLibraryItem> ZtoryMouthLibrary::pendingFromLibrary(
    ToonzScene *scene) {
  QVector<MouthLibraryItem> out;
  TFilePath scenePath;
  QSet<QString> subs;
  QVector<TFilePath> levels;
  if (!scanScene(scene, &scenePath, &subs, &levels) || levels.isEmpty())
    return out;
  const QHash<QString, Library> libs = librariesByName();
  for (const LevelPair &lp : levelPairs(levels, libs)) {
    MouthMap shotMap, libMap;
    if (!ZtoryMouthMap::load(lp.libLevel, QString(), libMap)) continue;
    ZtoryMouthMap::load(lp.shotLevel, QString(), shotMap);
    MouthLibraryItem item;
    item.character = lp.character;
    item.label     = QObject::tr("%1 ▸ %2").arg(
        lp.character, QString::fromStdWString(lp.shotLevel.getWideName()));
    item.shotOwner = lp.shotLevel;
    item.libOwner  = lp.libLevel;
    compareSets(libMap, shotMap, &item);  // the LIBRARY's sets
    if (!item.added.isEmpty() || !item.changed.isEmpty()) out << item;
  }
  return out;
}

void ZtoryMouthLibrary::showUpdateDialog(QWidget *parent, bool onOpen) {
  const QString title = QObject::tr("Update mouths from the library");
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
  const QVector<MouthLibraryItem> items = pendingFromLibrary(scene);
  if (items.isEmpty()) {
    if (!onOpen)
      QMessageBox::information(
          parent, title,
          QObject::tr("Nothing to update: this scene has every mouth set of "
                      "its characters' libraries."));
    return;
  }
  QVector<QVector<MouthSet>> chosen;
  if (!chooseSets(parent, title,
                  QObject::tr("The characters' libraries have mouth sets this "
                              "scene's imported copy does not have. A set "
                              "that differs here may be a variant made on "
                              "purpose for this scene: it is not ticked."),
                  items, onOpen ? kAskOnOpenKey : nullptr,
                  QObject::tr("Don't ask again when opening a scene (the "
                              "button in ZtoRig ▸ Mouths stays)"),
                  &chosen))
    return;
  QStringList done, failed;
  for (int i = 0; i < items.size(); i++) {
    if (chosen[i].isEmpty()) continue;
    QString why;
    if (mergeInto(items[i].shotOwner, QString(), items[i].libOwner, chosen[i],
                  &why))
      done << QObject::tr("%1: %2 set(s)").arg(items[i].label).arg(chosen[i].size());
    else
      failed << QObject::tr("%1: %2").arg(items[i].label, why);
  }
  report(parent, title, done, failed);
}

QString ZtoryMouthLibrary::characterOfSubScene(ToonzScene *scene,
                                               const QString &sub,
                                               TXsheet *subXsh) {
  if (!scene) return QString();
  Library lib;
  const QHash<QString, Library> libs = librariesByName();
  QString libSub;
  if (libraryByMark(subXsh, libs, &lib, &libSub)) return lib.character;
  if (!libraryOfSubScene(scene->getXsheet(), sub, libs, &lib, nullptr, subXsh) &&
      !libraryByContent(scene->getXsheet(), sub, libs, &lib, &libSub, subXsh))
    return QString();
  return lib.character;
}

QString ZtoryMouthLibrary::characterKey(const QString &subSceneName) {
  QString key = subSceneName.trimmed().toLower();
  static const QRegularExpression kSuffix(QStringLiteral("_\\d+$"));
  key.remove(kSuffix);
  return key;
}

QHash<QString, QPair<QString, TFilePath>>
ZtoryMouthLibrary::characterScenesByName() {
  QHash<QString, QPair<QString, TFilePath>> out;
  const QHash<QString, Library> libs = librariesByName();
  for (auto it = libs.constBegin(); it != libs.constEnd(); ++it)
    out.insert(it.key(), qMakePair(it.value().character, it.value().scene));
  return out;
}

void ZtoryMouthLibrary::offerOnOpen() {
  if (!QSettings().value(kAskOnOpenKey, true).toBool()) return;
  runWhenNoModal([]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!pendingFromLibrary(scene).isEmpty())
      showUpdateDialog(dialogParent(), true);
  });
}

//----------------------------------------------------------------------------

QSet<QString> ZtoryMouthLibrary::drawingFingerprint(TXsheet *xsh) {
  QSet<QString> out;
  QSet<TXsheet *> seen;
  std::function<void(TXsheet *, int)> walk = [&](TXsheet *x, int depth) {
    if (!x || depth > 8 || seen.contains(x)) return;
    seen.insert(x);
    for (int c = 0; c < x->getColumnCount(); c++) {
      int r0 = 0, r1 = -1;
      x->getCellRange(c, r0, r1);
      QSet<TXshLevel *> done;
      for (int r = r0; r <= r1; r++) {
        const TXshCell cell = x->getCell(r, c);
        if (cell.isEmpty() || !cell.m_level) continue;
        TXshLevel *lv = cell.m_level.getPointer();
        if (done.contains(lv)) continue;
        done.insert(lv);
        if (TXshChildLevel *cl = lv->getChildLevel()) {
          walk(cl->getXsheet(), depth + 1);
        } else if (TXshSimpleLevel *sl = lv->getSimpleLevel()) {
          // The file's name: Tahoma renames a level «…_15» on import, the
          // file keeps its name.
          const QString f = QString::fromStdWString(
              sl->getPath().withoutParentDir().getWideString()).toLower();
          if (!f.isEmpty()) out.insert(f);
        }
      }
    }
  };
  walk(xsh, 0);
  return out;
}

// The project's characters, this scene's own first when it is one; false
// when there are none or the question is cancelled.
static bool chooseMouthCharacter(const QString &title, const QString &prompt,
                                 const QString &ownerFile, QString *name,
                                 QString *uuid) {
  ZtoryModel *m = ZtoryModel::instance();
  QStringList names;
  QHash<QString, QString> uuidOf;
  QString mineName;
  for (const Asset &a : m->assets()) {
    if (!ZtoryModel::isCharacterType(a.type)) continue;
    names << a.name;
    uuidOf.insert(a.name, a.uuid);
    if (QFileInfo(m->resolveAssetFile(a)) == QFileInfo(ownerFile))
      mineName = a.name;
  }
  names.sort(Qt::CaseInsensitive);
  if (names.isEmpty()) {
    QMessageBox::information(
        ZtoryMouthLibrary::dialogParent(), title,
        QObject::tr("The project has no characters: add the character in "
                    "the Production Tracker first."));
    return false;
  }
  bool ok = false;
  const QString who = QInputDialog::getItem(
      ZtoryMouthLibrary::dialogParent(), title, prompt, names,
      qMax(0, names.indexOf(mineName)), false, &ok);
  if (!ok || who.isEmpty()) return false;
  *name = who;
  *uuid = uuidOf.value(who);
  return true;
}

// Ztoryc: a drawing level marked as a character's mouths before its visemes
// are assigned (Franco, 2026-10-21): the mark is the <character> of the
// level's own .zmouth, so ZtoRig > Mouths finds it first.
static void addMouthLevelActions(QMenu *menu, TXshSimpleLevel *sl) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || !sl) return;
  const TFilePath levelPath = scene->decodeFilePath(sl->getPath());
  if (levelPath.isEmpty()) return;
  MouthMap map;
  const bool hasFile = ZtoryMouthMap::load(levelPath, QString(), map);
  const QString title = QObject::tr("Mark as Mouth Level");
  const QString level = QString::fromStdWString(sl->getName());

  if (hasFile && !map.characterName.isEmpty()) {
    QAction *change = menu->addAction(
        QObject::tr("Mouths of %1: Change Character...").arg(map.characterName));
    QObject::connect(change, &QAction::triggered, [levelPath, level, title]() {
      MouthMap now;
      ZtoryMouthMap::load(levelPath, QString(), now);
      QString who, uuid;
      if (!chooseMouthCharacter(
              title, QObject::tr("The mouths in «%1» belong to:").arg(level),
              QString(), &who, &uuid))
        return;
      now.characterName = who;
      now.characterUuid = uuid;
      QString why;
      if (!ZtoryMouthMap::save(levelPath, QString(), now, &why))
        QMessageBox::warning(ZtoryMouthLibrary::dialogParent(), title, why);
    });
    if (map.sets.isEmpty()) {  // only marked: the mark can go
      QAction *unmark = menu->addAction(QObject::tr("Unmark Mouth Level"));
      QObject::connect(unmark, &QAction::triggered, [levelPath, title]() {
        QString why;
        if (!ZtoryMouthMap::save(levelPath, QString(), MouthMap(), &why))
          QMessageBox::warning(ZtoryMouthLibrary::dialogParent(), title, why);
      });
    }
    return;
  }

  QAction *mark = menu->addAction(QObject::tr("Mark as Mouth Level..."));
  QObject::connect(mark, &QAction::triggered, [levelPath, level, title]() {
    MouthMap now;
    ZtoryMouthMap::load(levelPath, QString(), now);  // absent = empty map
    QString who, uuid;
    if (!chooseMouthCharacter(
            title, QObject::tr("The mouths in «%1» belong to:").arg(level),
            QString(), &who, &uuid))
      return;
    now.characterName = who;
    now.characterUuid = uuid;
    QString why;
    if (!ZtoryMouthMap::save(levelPath, QString(), now, &why)) {
      QMessageBox::warning(ZtoryMouthLibrary::dialogParent(), title, why);
      return;
    }
    QMessageBox::information(
        ZtoryMouthLibrary::dialogParent(), title,
        QObject::tr("«%1» is marked as the mouths of %2.\nAssign the visemes in "
                    "ZtoRig > Mouths: the level is listed first.")
            .arg(level, who));
  });
}

void ZtoryMouthLibrary::addMouthColumnActions(QMenu *menu, TXsheet *xsh,
                                              int col) {
  if (!menu || !xsh || col < 0 || col >= xsh->getColumnCount()) return;
  int r0 = 0, r1 = -1;
  if (!xsh->getCellRange(col, r0, r1)) return;
  TXshChildLevel *cl = nullptr;
  QString sub;
  for (int r = r0; r <= r1 && !cl; r++) {
    const TXshCell cell = xsh->getCell(r, col);
    if (cell.isEmpty() || !cell.m_level) continue;
    cl  = cell.m_level->getChildLevel();
    sub = QString::fromStdWString(cell.m_level->getName());
    break;
  }
  if (!cl) {
    // A drawing level: its map sits next to its file.
    for (int r = r0; r <= r1; r++) {
      const TXshCell cell = xsh->getCell(r, col);
      if (cell.isEmpty() || !cell.m_level) continue;
      if (TXshSimpleLevel *sl = cell.m_level->getSimpleLevel())
        addMouthLevelActions(menu, sl);
      return;
    }
    return;
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  QAction *act = nullptr;
  if (scene->isUntitled()) {
    act = menu->addAction(QObject::tr("Mark as Mouth Column..."));
    act->setEnabled(false);
    act->setToolTip(QObject::tr("Save the scene first"));
    return;
  }
  const TFilePath owner = scene->decodeFilePath(scene->getScenePath());
  QVector<MouthColumnTag> tags;
  ZtoryMouthMap::loadColumnTags(owner, &tags);
  int mine = -1;
  for (int i = 0; i < tags.size(); i++)
    if (tags[i].subScene == sub) mine = i;

  if (mine >= 0) {
    act = menu->addAction(QObject::tr("Unmark Mouth Column (%1)")
                              .arg(tags[mine].character));
    QObject::connect(act, &QAction::triggered, [owner, sub]() {
      QVector<MouthColumnTag> now;
      ZtoryMouthMap::loadColumnTags(owner, &now);
      for (int i = now.size() - 1; i >= 0; --i)
        if (now[i].subScene == sub) now.remove(i);
      QString why;
      if (!ZtoryMouthMap::saveColumnTags(owner, now, &why))
        QMessageBox::warning(dialogParent(), QObject::tr("Mouth Column"), why);
    });
    return;
  }

  act = menu->addAction(QObject::tr("Mark as Mouth Column..."));
  TXsheet *subXsh = cl->getXsheet();
  QObject::connect(act, &QAction::triggered, [owner, sub, subXsh]() {
    QString who, uuid;
    if (!chooseMouthCharacter(
            QObject::tr("Mark as Mouth Column"),
            QObject::tr("The mouths in «%1» belong to:").arg(sub),
            owner.getQString(), &who, &uuid))
      return;

    MouthColumnTag tag;
    tag.subScene      = sub;
    tag.id            = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tag.character     = who;
    tag.characterUuid = uuid;
    tag.files         = drawingFingerprint(subXsh);
    if (tag.files.isEmpty()) {
      QMessageBox::warning(dialogParent(), QObject::tr("Mark as Mouth Column"),
                           QObject::tr("«%1» has no drawings to recognise it by.")
                               .arg(sub));
      return;
    }
    QVector<MouthColumnTag> now;
    ZtoryMouthMap::loadColumnTags(owner, &now);
    for (int i = now.size() - 1; i >= 0; --i)
      if (now[i].subScene == sub) now.remove(i);
    now.push_back(tag);
    QString why;
    if (!ZtoryMouthMap::saveColumnTags(owner, now, &why))
      QMessageBox::warning(dialogParent(), QObject::tr("Mark as Mouth Column"), why);
  });
}
