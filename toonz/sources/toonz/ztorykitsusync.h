#pragma once

// Ztoryc: the Sync with Kitsu, out of the tracker (2026-09-27).
//
// One object for the whole app. It runs the four steps of «⇄ Sync with
// Kitsu» and applies to the model what Kitsu sends back — shot and asset ids,
// statuses (three-way merge on the base, ZtoryTaskFlow::mergeFromServer),
// assets created on Kitsu, the team. The tracker is only the button, the
// confirmation and the label.
//
// It used to live in the tracker's constructor: seven handlers testing a step
// number, and ~400 lines of model logic inside UI lambdas — run once PER OPEN
// TRACKER, so two trackers (room + floating) applied every pull twice.
//
// The steps. The pushes come first: each sends only what changed in Ztoryc
// since the last sync, and only over the base Kitsu still has. The pulls
// then merge on the base.
//   1 shots to Kitsu (entities, then task statuses)
//   2 assets to Kitsu (entities, then task statuses)
//   3 assets from Kitsu (new entities, then task statuses)
//   4 team + shot statuses from Kitsu
//   5 breakdown, both ways (three-way merge on each shot's base, then the
//     shots whose merged breakdown differs from Kitsu's are written)
//   6 asset previews: the picture of each asset's file, on its task in
//     progress and as its cover, again only when the file changed

#include "kitsuclient.h"

#include <QHash>
#include <QLockFile>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>  // unique_ptr needs the whole type
#include <QVector>

#include <memory>

class QTimer;

class ZtoryKitsuSync : public QObject {
  Q_OBJECT
public:
  static ZtoryKitsuSync *instance();

  // Before anything is sent to Kitsu by hand: if this project was linked from
  // another folder (it may be a copy), asks. Yes makes this folder the linked
  // one; false means send nothing.
  static bool confirmProjectFolder();

  bool isRunning() const { return m_step >= 0; }
  // How many statuses a Sync would write on Kitsu now: what the tracker asks
  // confirmation for before a large one (typically the first, with no base).
  static int pendingSends();
  // How many asset previews a Sync would upload now (missing or older than
  // their file): the first Sync after the update uploads them all.
  static int pendingPreviews();
  // Starts a Sync. Returns false, saying why, when it cannot: not linked,
  // already running, or Kitsu's statuses not loaded yet (without them every
  // Kitsu status would read as Todo, and the merge would throw away changes
  // not sent yet — they are requested, to try again in a moment).
  bool start(int handles, QString *why);
  // True once per successful login, then false: the Sync that starts by
  // itself when the connection is made (Franco, 2026-09-27: «così Ztoryc
  // parte sempre dalla base aggiornata»). One owner, so that two trackers
  // open (room + floating) do not both start it, or both ask.
  bool takeAutoSync();

signals:
  void progress(const QString &text);
  // warn: something needs a look (conflicts, changes that could not go).
  void finished(bool ok, bool warn, const QString &summary);
  void assetTypesChanged();  // a pull adopted task types or asset types
  void teamChanged(int added);  // the team pulled from Kitsu added people

private:
  explicit ZtoryKitsuSync(QObject *parent = nullptr);
  void advance(bool ok, const QString &msg);
  void showWarnings();
  void warn(const QString &text);
  void queueAssetPreviews();
  void nextAssetPreview();

  int         m_step    = -1;  // -1 idle; 0..6 while syncing
  int         m_handles = 0;
  int         m_updated = 0, m_conflicts = 0, m_notSent = 0;
  QStringList m_warnings;
  int         m_droppedAssets = 0;  // step 3: assets of other episodes removed
  QTimer     *m_watchdog = nullptr;
  bool        m_autoSyncArmed = false;  // a login happened, no auto Sync yet
  QVector<KitsuTaskPush>      m_pendingTasks;       // step 1, after the shots
  QVector<KitsuAssetTaskPush> m_pendingAssetTasks;  // step 2, after the assets
  // Step 5: the base each written shot gets once Kitsu has its breakdown,
  // by Kitsu shot id. Set only on success: a shot that failed keeps its old
  // base, and the next Sync sends it again.
  QHash<QString, QVector<BreakdownEntry>> m_castBases;
  QString       m_castProjectId;  // the project the writes are for
  QSet<QString> m_castScope;      // Kitsu shots seen in step 4
  // Step 6: the asset previews still to upload, one at a time.
  struct PreviewJob {
    QString    assetUuid, kitsuAssetId, taskType;
    TaskStatus status = TaskStatus::Todo;  // the task's, sent unchanged
    QString    file, signature;            // file empty = nothing to upload
    // A rigged character: first the render of the rig already on Kitsu
    // (the WFA's) becomes the cover; the file only if there is none.
    QString    rigTask;
  };
  QVector<PreviewJob> m_previewJobs;
  PreviewJob          m_previewJob;   // the one travelling
  int                 m_previewToken = 0;
  int                 m_previewTotal = 0;
  // The PNGs made for Kitsu. Freed when the step ends or the Sync stops:
  // an upload in flight has its file open already (postPreviewFile).
  std::unique_ptr<QTemporaryDir> m_previewDir;
  int m_previewsSent = 0, m_previewsFailed = 0, m_previewsSkipped = 0;
  int m_castTaken = 0, m_castWritten = 0, m_castConflicts = 0;
  // Held while syncing: two Ztoryc windows on the same project must not sync
  // it at the same time (2026-10-02). See ZtoryLocks.
  std::unique_ptr<QLockFile> m_projectLock;
};
