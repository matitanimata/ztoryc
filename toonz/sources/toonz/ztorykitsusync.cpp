#include "ztorykitsusync.h"
#include "ztorystoryboardlock.h"

#include "ztoryassetpreview.h"
#include "ztorymodel.h"
#include "ztorylocks.h"
#include "ztorytaskflow.h"

#include "iocommand.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"

#include "toonzqt/dvdialog.h"

#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>

//=============================================================================
// Applying what Kitsu sends back to the model. Moved as they were from the
// tracker's constructor; only the UI refresh became model signals.
//=============================================================================

namespace {

// What a pull did, for the Sync's summary.
struct PullCounts {
  int updated = 0, conflicts = 0, notSent = 0, adopted = 0;
};

// Merges one pulled task and counts what happened. Returns true when
// Ztoryc's own change was kept — something not sent yet.
bool mergeAndCount(ZtoryTaskFlow::Entity entity, const QString &uuid,
                   const QString &taskType, TaskStatus server,
                   PullCounts &c) {
  switch (ZtoryTaskFlow::mergeFromServer(entity, uuid, taskType, server)) {
  case ZtoryTaskFlow::Merge::TookServer: ++c.updated; return false;
  case ZtoryTaskFlow::Merge::Conflict: ++c.updated; ++c.conflicts; return false;
  case ZtoryTaskFlow::Merge::KeptLocal: return true;
  case ZtoryTaskFlow::Merge::Same: return false;
  }
  return false;
}

// --- Breakdown, three-way (2026-09-27) ---------------------------------
// The same table as the statuses (ztorytaskflow.h), on each (shot, asset)
// link, where «absent» is a value too: a link that is in the base and no
// longer in Ztoryc was REMOVED in Ztoryc, and must go on Kitsu as well —
// otherwise the two drift apart (Franco: «se le cancellazioni non si
// propagano poi si disallinea»).
// Without a base (a shot never synced, or re-created on Kitsu since) the two
// sides are joined, and nothing is removed from either.

struct CastingPlan {
  QVector<KitsuCastingPush>               pushes;
  QHash<QString, QVector<BreakdownEntry>> bases;  // by Kitsu shot id
  int taken = 0, conflicts = 0;
  int outOfScope = 0, sharedShotId = 0, notOnKitsuYet = 0;
  QStringList unknownOnKitsu;  // assets Kitsu casts and Ztoryc does not have
};

using CastMap = QHash<QString, BreakdownEntry>;  // by asset uuid

struct AssetIds {
  QHash<QString, QString> uuidByKitsu, kitsuByUuid;
};

AssetIds assetIds() {
  AssetIds ids;
  for (const Asset &a : ZtoryModel::instance()->assets()) {
    if (a.kitsuAssetId.isEmpty()) continue;
    ids.uuidByKitsu.insert(a.kitsuAssetId, a.uuid);
    ids.kitsuByUuid.insert(a.uuid, a.kitsuAssetId);
  }
  return ids;
}

// The three sides of one shot, keyed by asset uuid.
struct CastSides {
  CastMap                    local, server, base;
  QStringList                order;  // Ztoryc's order, then Kitsu's additions
  QVector<BreakdownEntry>    localOnly;      // asset not on Kitsu yet
  QVector<KitsuCastingEntry> serverUnknown;  // asset Ztoryc does not know
};

CastSides castSidesOf(const ProjectShot &ps, bool hasBase,
                      const QVector<KitsuCastingEntry> &serverLines,
                      const AssetIds &ids) {
  CastSides sd;
  // An asset not on Kitsu yet cannot be in its breakdown: it stays here,
  // out of the comparison.
  for (const BreakdownEntry &be : ps.breakdown) {
    if (!ids.kitsuByUuid.contains(be.assetUuid)) { sd.localOnly << be; continue; }
    if (sd.local.contains(be.assetUuid)) continue;
    sd.local.insert(be.assetUuid, be);
    sd.order << be.assetUuid;
  }
  // An asset Ztoryc does not know is not ours to judge: it stays on Kitsu,
  // passed through in the write (the PUT replaces the whole casting).
  for (const KitsuCastingEntry &e : serverLines) {
    const QString uuid = ids.uuidByKitsu.value(e.kitsuAssetId);
    if (uuid.isEmpty()) { sd.serverUnknown << e; continue; }
    if (sd.server.contains(uuid)) continue;
    BreakdownEntry be;
    be.assetUuid     = uuid;
    be.nbOccurrences = e.nbOccurrences;
    be.label         = e.label;
    sd.server.insert(uuid, be);
    if (!sd.order.contains(uuid)) sd.order << uuid;
  }
  if (hasBase)
    for (const BreakdownEntry &be : ps.breakdownBase) {
      if (sd.base.contains(be.assetUuid)) continue;
      sd.base.insert(be.assetUuid, be);
      if (!sd.order.contains(be.assetUuid)) sd.order << be.assetUuid;
    }
  return sd;
}

bool sameCast(const BreakdownEntry *a, const BreakdownEntry *b) {
  if (!a || !b) return a == b;
  return *a == *b;
}

// The table, on one link. nullptr = the link is not there.
const BreakdownEntry *resolveLink(const BreakdownEntry *l,
                                  const BreakdownEntry *s,
                                  const BreakdownEntry *b, bool hasBase,
                                  int &conflicts) {
  if (!hasBase) return s ? s : l;  // joined; on a line both have, Kitsu's
  const bool localChanged  = !sameCast(l, b);
  const bool serverChanged = !sameCast(s, b);
  if (!serverChanged) return l;
  if (!localChanged || sameCast(l, s)) return s;
  ++conflicts;  // changed on both sides, differently: Kitsu wins
  return s;
}

const BreakdownEntry *findCast(const CastMap &m, const QString &uuid) {
  auto it = m.constFind(uuid);
  return it == m.constEnd() ? nullptr : &it.value();
}

// `inScope`: the Kitsu shots this Sync saw (statuses or casting). The bulk
// casting lists only shots that HAVE links, so a missing shot may be empty
// on Kitsu — or outside the bound episode, or deleted. With a base to lose,
// only a shot seen elsewhere is trusted to be empty.
CastingPlan planBreakdown(const QVector<KitsuCastingEntry> &entries,
                          QSet<QString> inScope) {
  ZtoryModel *m      = ZtoryModel::instance();
  const AssetIds ids = assetIds();
  QHash<QString, QVector<KitsuCastingEntry>> serverByShot;
  for (const KitsuCastingEntry &e : entries) {
    serverByShot[e.kitsuShotId] << e;
    inScope.insert(e.kitsuShotId);
  }
  // Two Ztoryc shots on ONE Kitsu shot (same seq+label in two storyboards)
  // would overwrite each other at every Sync: left out, and said.
  QHash<QString, int> shotIdUses;
  for (const ProjectShot &ps : m->projectShots())
    if (!ps.kitsuShotId.isEmpty()) ++shotIdUses[ps.kitsuShotId];

  CastingPlan plan;
  QSet<QString> unknownNames;
  for (int i = 0; i < (int)m->projectShots().size(); i++) {
    const ProjectShot &ps = m->projectShots()[i];
    if (ps.kitsuShotId.isEmpty()) continue;
    if (shotIdUses.value(ps.kitsuShotId) > 1) { ++plan.sharedShotId; continue; }
    const bool hasBase =
        ps.hasBreakdownBase && ps.breakdownBaseShotId == ps.kitsuShotId;
    if (hasBase && !ps.breakdownBase.isEmpty() &&
        !inScope.contains(ps.kitsuShotId)) {
      ++plan.outOfScope;
      continue;
    }
    const CastSides sd =
        castSidesOf(ps, hasBase, serverByShot.value(ps.kitsuShotId), ids);
    plan.notOnKitsuYet += sd.localOnly.size();
    for (const KitsuCastingEntry &e : sd.serverUnknown)
      unknownNames.insert(e.assetName.isEmpty() ? e.kitsuAssetId : e.assetName);

    QVector<BreakdownEntry> merged;
    CastMap mergedMap;
    for (const QString &uuid : sd.order) {
      const BreakdownEntry *r =
          resolveLink(findCast(sd.local, uuid), findCast(sd.server, uuid),
                      findCast(sd.base, uuid), hasBase, plan.conflicts);
      if (!r) continue;
      merged << *r;
      mergedMap.insert(uuid, *r);
    }
    // Compared as sets: the order, or an asset not on Kitsu yet sitting in
    // the middle, is not a change.
    if (mergedMap != sd.local) {
      m->setShotBreakdown(i, merged + sd.localOnly);
      ++plan.taken;
    }
    if (mergedMap == sd.server) {
      m->setShotBreakdownBase(i, ps.kitsuShotId, merged);  // Kitsu has it
      continue;
    }
    KitsuCastingPush push;
    push.kitsuShotId = ps.kitsuShotId;
    QSet<QString> sent;  // one line per Kitsu asset, or Zou may refuse it
    for (const BreakdownEntry &be : merged) {
      KitsuCastingEntry e;
      e.kitsuAssetId  = ids.kitsuByUuid.value(be.assetUuid);
      e.nbOccurrences = be.nbOccurrences;
      e.label         = be.label;
      if (sent.contains(e.kitsuAssetId)) continue;
      sent.insert(e.kitsuAssetId);
      push.entries << e;
    }
    for (const KitsuCastingEntry &e : sd.serverUnknown)
      if (!sent.contains(e.kitsuAssetId)) {
        sent.insert(e.kitsuAssetId);
        push.entries << e;
      }
    plan.pushes << push;
    plan.bases.insert(ps.kitsuShotId, merged);
  }
  plan.unknownOnKitsu = unknownNames.values();
  plan.unknownOnKitsu.sort();
  return plan;
}

// What step 5 left alone, said once in the end-of-Sync warnings: a
// breakdown that is quietly short is the failure to avoid.
QStringList breakdownWarnings(const CastingPlan &plan) {
  QStringList out;
  if (!plan.unknownOnKitsu.isEmpty())
    out << ZtoryKitsuSync::tr("Breakdown: Kitsu casts %1 asset(s) this project does not have "
            "(left as they are on Kitsu): %2")
             .arg(plan.unknownOnKitsu.size())
             .arg(plan.unknownOnKitsu.join(", "));
  if (plan.notOnKitsuYet > 0)
    out << ZtoryKitsuSync::tr("Breakdown: %1 link(s) to assets not on Kitsu yet stay only in "
            "Ztoryc.")
             .arg(plan.notOnKitsuYet);
  if (plan.outOfScope > 0)
    out << ZtoryKitsuSync::tr("Breakdown: %1 shot(s) skipped — not found in the Kitsu episode "
            "this project is bound to.")
             .arg(plan.outOfScope);
  if (plan.sharedShotId > 0)
    out << ZtoryKitsuSync::tr("Breakdown: %1 shot(s) skipped — they point at the same Kitsu "
            "shot as another (same sequence and name in two storyboards).")
             .arg(plan.sharedShotId);
  return out;
}

// The Kitsu ids of the shots a push created or matched.
void applyShotIds(const QHash<QString, QString> &byKey) {
  ZtoryModel *mm = ZtoryModel::instance();
  bool dirty     = false;
  for (ProjectShot &ps : mm->projectShots_rw()) {
    const QString seq = ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
    auto it = byKey.find(seq + "\n" + ps.label.trimmed());
    if (it != byKey.end() && ps.kitsuShotId != it.value()) {
      ps.kitsuShotId = it.value();
      dirty          = true;
    }
  }
  if (dirty) mm->saveProjectDb();
}

// The Kitsu ids of the assets a push created or matched.  Kitsu answers by
// type + name, so two local assets with the same type and name are the same
// key: the id goes to ONE of them (the one that already has it, else the
// first without an id), never to all.  Giving it to all is how thirteen
// «New asset» ended up sharing one Kitsu id (CS2605 Cascina, 2026-10-23).
// Returns the names found more than once, to be told.
QStringList applyAssetIds(const QHash<QString, QString> &byKey) {
  ZtoryModel *mm = ZtoryModel::instance();
  bool dirty     = false;
  QHash<QString, QVector<Asset *>> byLocalKey;
  for (Asset &a : mm->assets())
    byLocalKey[a.type + "\n" + a.name.trimmed()].push_back(&a);
  QStringList duplicated;
  for (auto it = byKey.constBegin(); it != byKey.constEnd(); ++it) {
    const QVector<Asset *> same = byLocalKey.value(it.key());
    if (same.isEmpty()) continue;
    if (same.size() > 1) duplicated << same.first()->name.trimmed();
    // One asset: it takes the id, even replacing an old one (an asset
    // recreated on Kitsu). Several: the one that has it, else an empty one.
    Asset *target = same.size() == 1 ? same.first() : nullptr;
    if (!target)
      for (Asset *a : same)
        if (a->kitsuAssetId == it.value()) target = a;
    if (!target)
      for (Asset *a : same)
        if (a->kitsuAssetId.isEmpty()) { target = a; break; }
    if (target && target->kitsuAssetId != it.value()) {
      target->kitsuAssetId = it.value();
      dirty                = true;
    }
  }
  if (dirty) mm->saveProjectDb();
  return duplicated;
}

// Shot task statuses from Kitsu, merged on the base.
PullCounts applyShotStatuses(const QVector<KitsuPullEntry> &entries) {
  ZtoryModel *mm = ZtoryModel::instance();
  PullCounts c;
  bool dirty = false;
  // Done tasks seen in this pull: their next task is readied AFTER every
  // entry is applied, or a later «next task: Todo» entry would undo it.
  QVector<QPair<QString, QString>> doneTasks;  // shot uuid, task type
  for (const KitsuPullEntry &e : entries) {
    const QString ekey = KitsuClient::normalizeTaskType(e.taskType);
    for (ProjectShot &ps : mm->projectShots_rw()) {
      bool match;
      if (!ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty())
        match = (ps.kitsuShotId == e.kitsuShotId);
      else {
        const QString psseq =
            ps.seq.trimmed().isEmpty() ? "SQ01" : ps.seq.trimmed();
        match = (ps.label.trimmed() == e.shot.trimmed() &&
                 psseq == e.seq.trimmed());
      }
      if (!match) continue;
      if (ps.kitsuShotId.isEmpty() && !e.kitsuShotId.isEmpty()) {
        ps.kitsuShotId = e.kitsuShotId;
        dirty          = true;
      }
      for (const QString &tt : mm->taskTypesForProjectShot(ps))
        if (KitsuClient::normalizeTaskType(tt) == ekey) {
          if (mergeAndCount(ZtoryTaskFlow::Entity::Shot, ps.uuid, tt,
                            e.status, c))
            ++c.notSent;
          dirty = true;  // the base moved
          // Add-only assignee merge (mirrors the add-only push).
          for (const QString &nm : e.assignees)
            if (!ps.tasks[tt].assignees.contains(nm)) {
              ps.tasks[tt].assignees.push_back(nm);
              dirty = true;
            }
          if (e.status == TaskStatus::Done) doneTasks.push_back({ps.uuid, tt});
          break;
        }
    }
  }
  // Mirrored from Kitsu: the dependency is applied without announcing a
  // transition, so the automatic push does not send it back.
  for (const auto &d : doneTasks)
    if (ZtoryTaskFlow::readyNextAfter(ZtoryTaskFlow::Entity::Shot, d.first,
                                      d.second))
      dirty = true;
  if (dirty) mm->saveAndNotifyTasks();
  return c;
}

// Asset task statuses from Kitsu, merged on the base.
PullCounts applyAssetStatuses(const QVector<KitsuAssetStatusEntry> &entries) {
  ZtoryModel *mm = ZtoryModel::instance();
  PullCounts c;
  bool dirty = false;
  QVector<QPair<QString, QString>> doneTasks;  // asset uuid, task type
  for (const KitsuAssetStatusEntry &e : entries) {
    const QString ekey = KitsuClient::normalizeTaskType(e.taskType);
    for (Asset &a : mm->assets()) {
      bool match;
      if (!a.kitsuAssetId.isEmpty() && !e.kitsuAssetId.isEmpty())
        match = (a.kitsuAssetId == e.kitsuAssetId);
      else
        match = (a.type == e.assetType &&
                 a.name.trimmed().compare(e.assetName.trimmed(),
                                          Qt::CaseInsensitive) == 0);
      if (!match) continue;
      if (a.kitsuAssetId.isEmpty() && !e.kitsuAssetId.isEmpty()) {
        a.kitsuAssetId = e.kitsuAssetId;
        dirty          = true;
      }
      QString target;
      for (const QString &tt : mm->assetTaskTypesForType(a.type))
        if (KitsuClient::normalizeTaskType(tt) == ekey) {
          target = tt;
          break;
        }
      // No counterpart in this asset type's pipeline: ADOPT the Kitsu task
      // type instead of dropping it. Silently discarding it is what made a
      // pull look like it had worked while leaving the tasks empty — on
      // a real show, Modeling and Rigging vanished this way because
      // Ztoryc's canonical pipeline is Concept/Rough/Clean/Color.
      // Kitsu is the source of truth for the pipeline, so it gets appended.
      if (target.isEmpty()) {
        mm->addAssetTaskType(a.type, e.taskType);
        target = e.taskType;
        ++c.adopted;
        dirty = true;
      }
      if (mergeAndCount(ZtoryTaskFlow::Entity::Asset, a.uuid, target,
                        e.status, c))
        ++c.notSent;
      dirty = true;  // the base moved
      for (const QString &nm : e.assignees)
        if (!a.tasks[target].assignees.contains(nm)) {
          a.tasks[target].assignees.push_back(nm);
          dirty = true;
        }
      // As for shots: a Done from Kitsu readies the next task — after the loop.
      if (e.status == TaskStatus::Done) doneTasks.push_back({a.uuid, target});
    }
  }
  for (const auto &d : doneTasks)
    if (ZtoryTaskFlow::readyNextAfter(ZtoryTaskFlow::Entity::Asset, d.first,
                                      d.second))
      dirty = true;
  if (dirty) {
    mm->saveProjectDb();
    emit mm->assetsChanged();
  }
  return c;
}

// Assets authored on Kitsu: matched by id, then by type + name; the new ones
// are added. Returns how many asset TYPES had to be created.
int importAssets(const QVector<KitsuAsset> &assets) {
  ZtoryModel *mm = ZtoryModel::instance();
  int newTypes   = 0;
  bool dirty     = false;
  for (const KitsuAsset &ka : assets) {
    if (ka.name.trimmed().isEmpty()) continue;
    Asset *found = nullptr;
    for (Asset &a : mm->assets()) {
      if (!ka.kitsuAssetId.isEmpty() && a.kitsuAssetId == ka.kitsuAssetId) {
        found = &a;
        break;
      }
      if (a.kitsuAssetId.isEmpty() && a.type == ka.type &&
          a.name.trimmed().compare(ka.name.trimmed(), Qt::CaseInsensitive) == 0)
        found = &a;  // keep looking for a stronger id match
    }
    if (found) {
      if (found->kitsuAssetId != ka.kitsuAssetId) {
        found->kitsuAssetId = ka.kitsuAssetId;
        dirty               = true;
      }
      if (found->kitsuMainPack != ka.mainPack) {
        found->kitsuMainPack = ka.mainPack;
        dirty                = true;
      }
      continue;
    }
    // A Kitsu asset type this project has no pipeline for (the instance also
    // defines Scene and analisi_target beyond our canonical four): create it,
    // or the asset lands with a type that shows nowhere in the Asset Types
    // tab and silently borrows the canonical task order.
    if (!ka.type.trimmed().isEmpty() && !mm->findAssetType(ka.type)) {
      mm->assetTypes().push_back(
          AssetType{ka.type, ZtoryModel::canonicalAssetTaskOrder()});
      ++newTypes;
    }
    mm->addAsset(ka.type, ka.name.trimmed());
    mm->assets().back().kitsuAssetId  = ka.kitsuAssetId;
    mm->assets().back().kitsuMainPack = ka.mainPack;
    dirty                             = true;
  }
  if (dirty) {
    mm->saveProjectDb();
    emit mm->assetsChanged();
  }
  return newTypes;
}

// The project's people on Kitsu, added to the team (never removed).
int applyTeam(const QVector<KitsuPerson> &persons) {
  ZtoryModel *mm     = ZtoryModel::instance();
  QStringList roster = mm->team();
  int added          = 0;
  for (const KitsuPerson &p : persons)
    if (!p.name.trimmed().isEmpty() &&
        !roster.contains(p.name, Qt::CaseInsensitive)) {
      roster.push_back(p.name);
      ++added;
    }
  if (added > 0) {
    mm->setTeam(roster);
    mm->saveProjectDb();
  }
  return added;
}

}  // namespace

//=============================================================================
// The Sync
//=============================================================================

namespace {

// The signature of «the cover is the render of the rig»: no file of ours
// behind it. A newer render replaces it by itself — the WFA upload makes it
// the cover.
QString rigCoverSignature(const Asset &a) {
  return QStringLiteral("rigcover|") + a.kitsuAssetId;
}

// The character's Rigging task when it is Done on Kitsu, else empty.
QString doneRigTask(const Asset &a) {
  if (!ZtoryModel::isCharacterType(a.type)) return QString();
  const QString rig = ZtoryTaskFlow::characterSceneTask(a.uuid);
  if (rig.isEmpty()) return QString();
  for (auto it = a.tasks.constBegin(); it != a.tasks.constEnd(); ++it)
    if (it.key().compare(rig, Qt::CaseInsensitive) == 0)
      return it.value().hasSynced && it.value().synced == TaskStatus::Done
                 ? it.key()
                 : QString();
  return QString();
}

}  // namespace

ZtoryKitsuSync *ZtoryKitsuSync::instance() {
  static ZtoryKitsuSync *s = new ZtoryKitsuSync();  // app lifetime
  return s;
}

int ZtoryKitsuSync::pendingSends() {
  int toSend = 0, skippedShots = 0;
  QVector<KitsuTaskPush> shotTasks;
  KitsuClient::buildShotPushFromProject(0, shotTasks, skippedShots);
  for (const KitsuTaskPush &t : shotTasks)
    if (!t.createOnly) ++toSend;
  for (const KitsuAssetTaskPush &t : KitsuClient::buildAssetTasksFromModel())
    if (!t.createOnly) ++toSend;
  return toSend;
}

bool ZtoryKitsuSync::takeAutoSync() {
  const bool armed = m_autoSyncArmed;
  m_autoSyncArmed  = false;
  return armed;
}

ZtoryKitsuSync::ZtoryKitsuSync(QObject *parent) : QObject(parent) {
  connect(KitsuClient::instance(), &KitsuClient::loginFinished, this,
          [this](bool ok, const QString &) { m_autoSyncArmed = ok; });
  // A show linked before it had episodes has a NAME but no episode id: the
  // push finds or creates the episode, and binding it here is what keeps the
  // next steps (and every later Sync) inside it. Only when nothing is bound
  // yet: an id already chosen in the Kitsu dialog is never overwritten.
  connect(KitsuClient::instance(), &KitsuClient::episodeResolved, this,
          [](const QString &projectId, const QString &episodeId,
             const QString &episodeName) {
            ZtoryModel *m = ZtoryModel::instance();
            if (m->kitsuProjectId() != projectId || m->isKitsuEpisodeLinked())
              return;
            m->setKitsuEpisode(episodeId, episodeName);
            m->saveProjectDb();
          });
  connect(this, &ZtoryKitsuSync::finished, this, [this]() {
    if (!m_projectLock) return;
    m_projectLock.reset();
    ZtoryModel::instance()->holdDiskReload(false);
  });
  m_watchdog = new QTimer(this);
  m_watchdog->setSingleShot(true);
  connect(m_watchdog, &QTimer::timeout, this, [this]() {
    advance(false, tr("no answer from Kitsu for two minutes"));
  });

  KitsuClient *kc = KitsuClient::instance();
  // Ids: applied whenever they come.
  connect(kc, &KitsuClient::shotIdsResolved, this, &applyShotIds);
  connect(kc, &KitsuClient::removedShotsHandled, this, [](const QStringList &ids) {
    ZtoryModel::instance()->forgetRemovedKitsuShots(ids);
  });
  connect(kc, &KitsuClient::assetIdsResolved, this,
          [this](const QHash<QString, QString> &byKey) {
            const QStringList dup = applyAssetIds(byKey);
            if (!dup.isEmpty())
              warn(tr("These assets appear more than once with the same type "
                      "and name, and Kitsu keeps them as ONE asset: only one "
                      "of each was linked to it. Rename or remove the "
                      "copies in the Production Tracker:\n\n%1")
                       .arg(dup.join("\n")));
          });
  connect(kc, &KitsuClient::taskTypesMissing, this,
          [this](const QStringList &names) {
            warn(tr("These workflow tasks were NOT created in Kitsu: the server "
                    "has no Shot task type with that name, and it refused to "
                    "create one (only a Kitsu admin can):\n\n%1\n\nCreate them "
                    "in Kitsu (Settings > Task Types) and sync again.")
                     .arg(names.join("\n")));
          });
  connect(kc, &KitsuClient::assetsSkipped, this,
          [this](const QStringList &lines) {
            warn(tr("Some assets were NOT sent to Kitsu, because the Kitsu "
                    "server has no asset type with that name:\n\n%1\n\nCreate "
                    "the type in Kitsu (or change the asset's type here) and "
                    "sync again.")
                     .arg(lines.join("\n")));
          });

  // Assets LINKED to Kitsu that Kitsu now puts in ANOTHER episode leave this
  // tracker, without asking (Franco, 2026-09-29): an asset belongs to its
  // episode or to the Main Pack, nowhere else. Before, the Sync only added,
  // and one imported while in reach (an asset of another
  // episode) stayed after being moved on Kitsu. Nothing is deleted on Kitsu,
  // and assets not linked to Kitsu are never touched.
  connect(kc, &KitsuClient::assetsInOtherEpisodes, this,
          [this](const QStringList &ids) {
            if (m_step != 3 || ids.isEmpty()) return;
            ZtoryModel *m = ZtoryModel::instance();
            auto &assets  = m->assets();
            QStringList gone;
            for (int i = int(assets.size()) - 1; i >= 0; --i)
              if (!assets[i].kitsuAssetId.isEmpty() &&
                  ids.contains(assets[i].kitsuAssetId)) {
                gone.prepend(assets[i].name);
                m->removeAssetAt(i);
              }
            if (gone.isEmpty()) return;
            m_droppedAssets += gone.size();
            m->saveProjectDb();
            warn(tr("Taken out of this tracker — on Kitsu they belong to "
                    "another episode:\n\n%1")
                     .arg(gone.join("\n")));
          });

  connect(kc, &KitsuClient::assetsLeftInMainPack, this,
          [this](const QStringList &names) {
            warn(tr("These assets were created on Kitsu but are in the Main "
                    "Pack, shared by every episode — the episode could not be "
                    "set:\n\n%1\n\nMove them to the episode in Kitsu.")
                     .arg(names.join("\n")));
          });

  connect(kc, &KitsuClient::assetTasksUnlinked, this, [this](int count) {
    warn(tr("%1 asset task status(es) changed in Ztoryc were NOT sent: on Kitsu "
            "their task type is not linked to the asset's type (Kitsu hides "
            "those tasks). Remove the task type from that asset type's pipeline "
            "in Ztoryc, or link it to the asset type on Kitsu.")
             .arg(count));
  });

  // Step 1: shots, then their task statuses.
  connect(kc, &KitsuClient::shotsPushed, this,
          [this](bool ok, int, int, const QString &msg) {
            if (m_step != 1) return;
            if (ok && !m_pendingTasks.isEmpty()) {
              // Rebuilt now: shotIdsResolved (emitted just before) has stored
              // the Kitsu id of the shots this push CREATED, and the task push
              // finds shots by id — by name it can't tell apart the "SQ01" of
              // one episode from another's.
              int unusedSkipped = 0;
              KitsuClient::buildShotPushFromProject(0, m_pendingTasks,
                                                    unusedSkipped);
              m_watchdog->start(120000);  // the second half of the step
              KitsuClient::instance()->pushTasks(
                  ZtoryModel::instance()->kitsuProjectId(), m_pendingTasks);
              m_pendingTasks.clear();
              return;
            }
            advance(ok, msg);
          });
  connect(kc, &KitsuClient::tasksPushed, this,
          [this](bool ok, int, const QString &msg) {
            if (m_step == 1) advance(ok, msg);
          });

  // Step 2: assets, then their task statuses.
  connect(kc, &KitsuClient::assetsPushed, this,
          [this](bool ok, int, int, const QString &msg) {
            if (m_step != 2) return;
            if (ok && !m_pendingAssetTasks.isEmpty()) {
              m_watchdog->start(120000);  // the second half of the step
              KitsuClient::instance()->pushAssetTasks(
                  ZtoryModel::instance()->kitsuProjectId(), m_pendingAssetTasks);
              m_pendingAssetTasks.clear();
              return;
            }
            advance(ok, msg);
          });
  connect(kc, &KitsuClient::assetTasksPushed, this,
          [this](bool ok, int, const QString &msg) {
            if (m_step == 2) advance(ok, msg);
          });

  // Step 3: assets from Kitsu, then their task statuses.
  connect(kc, &KitsuClient::assetsPulled, this,
          [this](bool ok, const QVector<KitsuAsset> &assets, const QString &msg) {
            if (ok && importAssets(assets) > 0) emit assetTypesChanged();
            if (m_step != 3) return;
            if (!ok) {
              advance(false, msg);
              return;
            }
            ZtoryModel *m = ZtoryModel::instance();
            KitsuClient::instance()->pullAssetStatuses(m->kitsuProjectId(),
                                                       m->kitsuEpisodeId());
          });
  connect(kc, &KitsuClient::assetStatusesPulled, this,
          [this](bool ok, const QVector<KitsuAssetStatusEntry> &entries,
                 const QString &msg) {
            if (ok) {
              const PullCounts c = applyAssetStatuses(entries);
              m_updated += c.updated;
              m_conflicts += c.conflicts;
              m_notSent += c.notSent;
              // A new column in the asset table must not look like it came
              // from nowhere: the Asset Types tab shows the pipelines.
              if (c.adopted > 0) emit assetTypesChanged();
            }
            if (m_step == 3) advance(ok, msg);
          });

  // Step 4: team and shot statuses from Kitsu.
  connect(kc, &KitsuClient::teamPulled, this,
          [this](bool ok, const QVector<KitsuPerson> &persons, const QString &) {
            const int added = ok ? applyTeam(persons) : 0;
            if (added > 0) emit teamChanged(added);
          });
  connect(kc, &KitsuClient::statusesPulled, this,
          [this](bool ok, const QVector<KitsuPullEntry> &entries,
                 const QString &msg) {
            if (ok) {
              const PullCounts c = applyShotStatuses(entries);
              m_updated += c.updated;
              m_conflicts += c.conflicts;
              m_notSent += c.notSent;
              // The shots Kitsu has in this episode: step 5's scope.
              if (m_step == 4)
                for (const KitsuPullEntry &e : entries)
                  m_castScope.insert(e.kitsuShotId);
            }
            if (m_step == 4) advance(ok, msg);
          });

  // Step 5: breakdown, merged on the base, then written where it differs.
  connect(kc, &KitsuClient::breakdownPulled, this,
          [this](bool ok, const QVector<KitsuCastingEntry> &entries,
                 const QString &msg) {
            if (m_step != 5) return;
            if (!ok) {
              advance(false, msg);
              return;
            }
            ZtoryModel *m         = ZtoryModel::instance();
            const CastingPlan plan = planBreakdown(entries, m_castScope);
            m_castTaken     = plan.taken;
            m_castConflicts = plan.conflicts;
            m_castBases     = plan.bases;
            m_castProjectId = m->kitsuProjectId();
            for (const QString &w : breakdownWarnings(plan)) warn(w);
            m->saveProjectDb();
            if (plan.pushes.isEmpty()) {
              advance(true, QString());
              return;
            }
            m_watchdog->start(120000);  // the writes: a second half
            KitsuClient::instance()->pushCasting(m_castProjectId, plan.pushes);
          });
  connect(kc, &KitsuClient::castingProgress, this, [this](int done, int total) {
    if (m_step != 5) return;
    m_watchdog->start(120000);  // each answer proves Kitsu is still there
    emit progress(tr("Sync 5/6 — breakdown %1/%2…").arg(done).arg(total));
  });
  // Step 6: asset previews, one after the other.
  connect(kc, &KitsuClient::reviewPreviewUploaded, this,
          [this](int token, bool ok, const QString &msg) {
            if (m_step != 6 || token != m_previewToken) return;
            m_previewToken = 0;
            ZtoryModel *m  = ZtoryModel::instance();
            if (ok) {
              if (!msg.isEmpty())  // uploaded, but e.g. the cover refused
                warn(tr("Preview of %1: %2")
                         .arg(QFileInfo(m_previewJob.file).fileName())
                         .arg(msg));
              for (int i = 0; i < (int)m->assets().size(); i++)
                if (m->assets()[i].uuid == m_previewJob.assetUuid) {
                  m->setAssetPreviewSig(i, m_previewJob.signature);
                  m->saveProjectDb();
                  break;
                }
              ++m_previewsSent;
            } else {
              ++m_previewsFailed;
              warn(tr("Preview of an asset not uploaded (%1): %2")
                       .arg(QFileInfo(m_previewJob.file).fileName())
                       .arg(msg));
            }
            nextAssetPreview();
          });
  // A rigged character: the render of its rig became the cover — or there
  // is none, and its file (the PSD) goes as for any asset.
  connect(kc, &KitsuClient::taskCoverSet, this,
          [this](int token, int result, const QString &msg) {
            if (m_step != 6 || token != m_previewToken) return;
            m_previewToken = 0;
            ZtoryModel *m  = ZtoryModel::instance();
            if (result == KitsuClient::CoverSet) {
              for (int i = 0; i < (int)m->assets().size(); i++)
                if (m->assets()[i].uuid == m_previewJob.assetUuid) {
                  m->setAssetPreviewSig(i, rigCoverSignature(m->assets()[i]));
                  m->saveProjectDb();
                  break;
                }
              ++m_previewsSent;
            } else if (result == KitsuClient::CoverFailed) {
              ++m_previewsFailed;
              warn(tr("Cover of an asset not set: %1").arg(msg));
            } else {
              PreviewJob again = m_previewJob;  // no render: the file, if any
              again.rigTask.clear();
              m_previewJobs.prepend(again);
              ++m_previewTotal;
            }
            nextAssetPreview();
          });
  // Not a failure: posting would have run a Kitsu automation. No signature
  // saved — it goes when the task has moved on.
  connect(kc, &KitsuClient::reviewPreviewSkipped, this,
          [this](int token, const QString &) {
            if (m_step != 6 || token != m_previewToken) return;
            m_previewToken = 0;
            ++m_previewsSkipped;
            nextAssetPreview();
          });
  connect(kc, &KitsuClient::castingPushed, this,
          [this](bool ok, const QStringList &written, const QString &msg) {
            if (m_step != 5) return;
            ZtoryModel *m = ZtoryModel::instance();
            // By Kitsu shot id, not by index: the shot list can change while
            // the writes travel (a scene saved, another project opened).
            if (m->kitsuProjectId() == m_castProjectId) {
              for (const QString &shotId : written) {
                auto it = m_castBases.constFind(shotId);
                if (it == m_castBases.constEnd()) continue;
                for (int i = 0; i < (int)m->projectShots().size(); i++)
                  if (m->projectShots()[i].kitsuShotId == shotId) {
                    m->setShotBreakdownBase(i, shotId, it.value());
                    break;
                  }
              }
              m->saveProjectDb();
            }
            const int failed = m_castBases.size() - written.size();
            m_castWritten = written.size();
            m_castBases.clear();
            // The last step: a shot that did not go is a warning, not a
            // stopped Sync — it keeps its old base and goes next time.
            if (!ok)
              warn(tr("The breakdown of %1 shot(s) could not be written on "
                      "Kitsu: %2")
                       .arg(failed)
                       .arg(msg));
            advance(true, QString());
          });
}

bool ZtoryKitsuSync::start(int handles, QString *why) {
  ZtoryModel *m   = ZtoryModel::instance();
  KitsuClient *kc = KitsuClient::instance();
  if (!m->isKitsuLinked() || !kc->isLoggedIn()) {
    if (why) *why = tr("Not connected to a Kitsu production.");
    return false;
  }
  if (isRunning()) {
    if (why) *why = tr("A Sync is already running.");
    return false;
  }
  // A series syncs only when bound to ONE episode by id (Franco, 2026-09-29):
  // unbound, the pulls read the WHOLE show — every episode's shots, assets
  // and statuses into this tracker, and the Sync only adds. A new episode is
  // created on Kitsu and then linked, not made up by the Sync from a name.
  if (m->productionType() == "tvshow" && !m->isKitsuEpisodeLinked()) {
    if (why)
      *why = tr("This production is a series: choose its episode in "
                "«Connect to Kitsu…» before syncing.");
    return false;
  }
  if (!kc->hasTaskStatuses()) {
    kc->fetchTaskStatuses();
    if (why)
      *why = tr("Kitsu's statuses are still loading — press Sync again in a "
                "moment.");
    return false;
  }
  // The tracker is written when the scene is saved: a Sync after an unsaved
  // change in the Board sent the shots as they were (a deleted shot went up
  // again, renameTest 2026-10-25). Save first, as ⌘S does (Franco).
  if (TApp::instance()->getCurrentScene()->getDirtyFlag() && !IoCmd::saveAll()) {
    if (why) *why = tr("The scene was not saved: Sync cancelled.");
    return false;
  }
  m_projectLock.reset(
      new QLockFile(ZtoryLocks::lockFilePath("kitsusync", m->projectDbPath())));
  m_projectLock->setStaleLockTime(0);  // a crashed holder: freed by its pid
  if (!m_projectLock->tryLock(0)) {
    m_projectLock.reset();
    if (why)
      *why = tr("This production is already syncing in another Ztoryc "
                "window: wait for it to finish.");
    return false;
  }
  // What other windows write meanwhile is merged at each save, and read back
  // into memory only at the end: the steps work on a tracker that holds still.
  m->holdDiskReload(true);
  m_handles = handles;
  m_updated = m_conflicts = m_notSent = 0;
  m_droppedAssets = 0;
  m_castTaken = m_castWritten = m_castConflicts = 0;
  m_castBases.clear();
  m_castScope.clear();
  m_previewJobs.clear();
  m_previewToken = m_previewTotal = m_previewsSent = m_previewsFailed = 0;
  m_previewsSkipped = 0;
  m_warnings.clear();
  m_step = 0;
  advance(true, QString());
  return true;
}

void ZtoryKitsuSync::warn(const QString &text) {
  if (isRunning())
    m_warnings << text;
  else
    DVGui::MsgBoxInPopup(DVGui::WARNING, text);
}

// Shown once at the end, not as popups that stop the Sync halfway.
void ZtoryKitsuSync::showWarnings() {
  if (m_warnings.isEmpty()) return;
  DVGui::MsgBoxInPopup(DVGui::WARNING, m_warnings.join("\n\n"));
  m_warnings.clear();
}

// One step, called when the previous one has answered.
void ZtoryKitsuSync::advance(bool ok, const QString &msg) {
  if (m_step < 0) return;
  // Re-armed at every step: a reply that never comes must not leave the Sync
  // — and the tracker's disabled button — stuck until the app restarts.
  m_watchdog->start(120000);
  if (!ok) {
    m_step = -1;
    m_watchdog->stop();
    m_previewDir.reset();
    emit finished(false, true, tr("Sync stopped: %1").arg(msg));
    showWarnings();
    return;
  }
  ZtoryModel *m   = ZtoryModel::instance();
  KitsuClient *kc = KitsuClient::instance();
  switch (++m_step) {
  case 1: {
    emit progress(tr("Sync 1/6 — shots to Kitsu…"));
    int skipped = 0;
    const QVector<KitsuShotPush> shots =
        KitsuClient::buildShotPushFromProject(m_handles, m_pendingTasks,
                                              skipped);
    if (shots.isEmpty()) break;
    // Not locked yet: the storyboard's names win on Kitsu too.
    kc->pushShots(m->kitsuProjectId(), m->episode(),
                  m->productionType() == "tvshow", shots,
                  !ZtoryStoryboardLock::isLocked(), m->removedKitsuShots());
    return;
  }
  case 2: {
    emit progress(tr("Sync 2/6 — assets to Kitsu…"));
    const QVector<KitsuAsset> assets = KitsuClient::buildAssetsFromModel();
    if (assets.isEmpty()) break;
    m_pendingAssetTasks = KitsuClient::buildAssetTasksFromModel();
    kc->pushAssets(m->kitsuProjectId(), assets, m->kitsuEpisodeId());
    return;
  }
  case 3:
    emit progress(tr("Sync 3/6 — assets from Kitsu…"));
    kc->pullAssets(m->kitsuProjectId(), m->kitsuEpisodeId());
    return;
  case 4:
    emit progress(tr("Sync 4/6 — shot statuses from Kitsu…"));
    kc->pullTeam(m->kitsuProjectId());
    kc->pullStatuses(m->kitsuProjectId(), m->kitsuEpisodeId());
    return;
  case 5:
    emit progress(tr("Sync 5/6 — breakdown…"));
    kc->pullBreakdown(m->kitsuProjectId(), m->kitsuEpisodeId());
    return;
  case 6:
    queueAssetPreviews();
    if (m_previewJobs.isEmpty()) break;
    nextAssetPreview();
    return;
  default: {
    m_step = -1;
    m_watchdog->stop();
    QString text =
        tr("Synced with Kitsu: %1 status(es) taken from Kitsu").arg(m_updated);
    if (m_conflicts)
      text += tr("; %1 conflict(s) — changed on both sides, Kitsu's kept")
                  .arg(m_conflicts);
    if (m_notSent)
      text += tr("; %1 changed in Ztoryc could not be sent (not on Kitsu yet?)")
                  .arg(m_notSent);
    if (m_castTaken || m_castWritten)
      text += tr("; breakdown: %1 shot(s) updated here, %2 written on Kitsu")
                  .arg(m_castTaken)
                  .arg(m_castWritten);
    if (m_castConflicts)
      text += tr("; %1 breakdown link(s) changed on both sides, Kitsu's kept")
                  .arg(m_castConflicts);
    if (m_droppedAssets)
      text += tr("; %1 asset(s) of other episodes taken out").arg(m_droppedAssets);
    if (m_previewsSent || m_previewsFailed)
      text += tr("; %1 asset preview(s) uploaded").arg(m_previewsSent);
    if (m_previewsFailed)
      text += tr(", %1 failed").arg(m_previewsFailed);
    if (m_previewsSkipped)
      text += tr("; %1 asset preview(s) wait — their task's status starts a "
                 "Kitsu automation")
                  .arg(m_previewsSkipped);
    emit finished(true,
                  m_conflicts || m_notSent || m_castConflicts ||
                      m_previewsFailed,
                  text + ".");
    showWarnings();
    return;
  }
  }
  advance(true, QString());  // a step with nothing to do: the next one
}

// Step 6. The assets whose preview on Kitsu is missing or older than their
// file (Franco, 2026-09-27); the task that carries it is the one in progress
// (see ztoryassetpreview.h for why not the last Done).
int ZtoryKitsuSync::pendingPreviews() {
  int n = 0;
  QHash<QString, QFileInfoList> dirCache;
  for (const Asset &a : ZtoryModel::instance()->assets()) {
    if (a.kitsuAssetId.isEmpty()) continue;
    if (a.previewSig == rigCoverSignature(a)) continue;
    const ZtoryAssetPreviewSource src = ztoryAssetPreviewSource(a, &dirCache);
    if (!doneRigTask(a).isEmpty() ||
        (!src.task.isEmpty() && !src.file.isEmpty() &&
         src.signature != a.previewSig))
      ++n;
  }
  return n;
}

void ZtoryKitsuSync::queueAssetPreviews() {
  m_previewJobs.clear();
  int nearOnly = 0;
  QHash<QString, QFileInfoList> dirCache;  // each folder listed once
  for (const Asset &a : ZtoryModel::instance()->assets()) {
    if (a.kitsuAssetId.isEmpty()) continue;
    if (a.previewSig == rigCoverSignature(a)) continue;  // the rig's, set
    const ZtoryAssetPreviewSource src = ztoryAssetPreviewSource(a, &dirCache);
    const QString rigTask             = doneRigTask(a);
    const bool fileToSend = !src.task.isEmpty() && !src.nearNameOnly &&
                            !src.file.isEmpty() &&
                            src.signature != a.previewSig;
    if (src.nearNameOnly && rigTask.isEmpty()) ++nearOnly;
    if (rigTask.isEmpty() && !fileToSend) continue;
    m_previewJobs << PreviewJob{a.uuid,
                                a.kitsuAssetId,
                                src.task,
                                src.status,
                                fileToSend ? src.file : QString(),
                                src.signature,
                                rigTask};
  }
  m_previewTotal = m_previewJobs.size();
  if (nearOnly > 0)
    warn(tr("Asset previews: %1 asset(s) have a file found only by a similar "
            "name (blue dot) — rename or link it, and the preview follows.")
             .arg(nearOnly));
}

void ZtoryKitsuSync::nextAssetPreview() {
  if (m_step != 6) return;
  if (m_previewJobs.isEmpty()) {
    m_previewDir.reset();  // the PNGs were only for the upload
    advance(true, QString());
    return;
  }
  m_previewJob = m_previewJobs.takeFirst();
  emit progress(tr("Sync 6/6 — asset previews %1/%2…")
                    .arg(m_previewTotal - m_previewJobs.size())
                    .arg(m_previewTotal));
  if (!m_previewJob.rigTask.isEmpty()) {
    m_watchdog->start(120000);
    m_previewToken = KitsuClient::instance()->setCoverFromTaskPreview(
        1, m_previewJob.kitsuAssetId, m_previewJob.rigTask,
        tr("Asset preview, from Ztoryc"));
    return;
  }
  if (m_previewJob.file.isEmpty()) {  // no render of the rig, no file
    QTimer::singleShot(0, this, &ZtoryKitsuSync::nextAssetPreview);
    return;
  }
  if (!m_previewDir) m_previewDir.reset(new QTemporaryDir());
  // A name per job: an upload of a stopped Sync may still be reading the
  // file of the same asset.
  static int jobNumber = 0;
  const QString png = m_previewDir->filePath(
      QStringLiteral("%1_%2.png").arg(m_previewJob.assetUuid).arg(++jobNumber));
  QString why;
  if (!m_previewDir->isValid() ||
      !ztoryRenderAssetPreview(m_previewJob.file, png, &why)) {
    ++m_previewsFailed;
    warn(tr("Preview of an asset not made: %1").arg(why));
    // Queued: a run of unreadable files must not nest one call per asset.
    QTimer::singleShot(0, this, &ZtoryKitsuSync::nextAssetPreview);
    return;
  }
  // Re-armed AFTER the render: a large PSD read on this thread must not
  // count as Kitsu not answering.
  m_watchdog->start(120000);
  // Kitsu wants a status with every comment: the one the task has on Kitsu,
  // read there (keepServerStatus) — an Approved must stay Approved.
  m_previewToken = KitsuClient::instance()->uploadReviewPreview(
      1, m_previewJob.kitsuAssetId, m_previewJob.taskType, png,
      m_previewJob.status, tr("Asset preview, from Ztoryc"), true);
}
