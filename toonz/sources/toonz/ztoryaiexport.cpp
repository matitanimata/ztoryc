// Ztoryc: Export for AI (Franco, 2026-10-25).
//
// One folder per shot, for a generator that animates between key drawings:
//   panels/<shot>_pNN_fFFFF_dDD.png  every panel's first frame as the camera
//                                    sees it; the name says where the panel
//                                    starts in the shot and how long it lasts
//   <shot>_panels.csv                the same, one row per panel, with texts
//   refs/<ASSET>__<file>             the files of the shot's breakdown,
//                                    Reference assets included
//   <shot>_audio.wav                 the shot's slice of each sound column
//   manifest.json                    all of the above in one file
// What the generator is asked to do is not decided here: the panels and their
// timing are the data, the prompts come from the person (DESIGN_export_to_ai).

#include "storyboardpanel.h"
#include "ztorymodel.h"
#include "ztoryshotops.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/tcamera.h"
#include "toonz/sceneproperties.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/tscenehandle.h"
#include "toutputproperties.h"

// TnzCore includes
#include "tsound_io.h"
#include "tsystem.h"

// Qt includes
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString csvField(QString s) {
  s.replace('"', "\"\"");
  return '"' + s.replace('\n', ' ') + '"';
}

// The column of the main xsheet that shows `cl`; `fallback` when none does.
int columnShowing(TXsheet *xsh, TXshChildLevel *cl, int fallback) {
  if (!cl) return fallback;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    int a, b;
    if (!xsh->getCellRange(c, a, b)) continue;
    for (int r = a; r <= b; r++) {
      const TXshCell &cell = xsh->getCell(r, c);
      if (cell.isEmpty()) continue;
      if (cell.getChildLevel() == cl) return c;
      break;
    }
  }
  return fallback;
}

// Renders main-xsheet rows [r0, r1] to `out` — a .png still or a .mp4 clip,
// by its extension — the shot column `ownCol` alone: the other shots are
// hidden while the render is set up, as the per-shot animatic export does.
// Tahoma names a still «name.FFFF.png»; the result is renamed to `out`.
bool renderRange(ToonzScene *scene, int r0, int r1, int ownCol, const QString &out) {
  TOutputProperties *prop = scene->getProperties()->getOutputProperties();
  const QFileInfo fi(out);
  const QString stem = "ztoryai_tmp_" + fi.completeBaseName();
  prop->setPath(TFilePath(fi.absolutePath().toStdWString()) +
                TFilePath((stem + "." + fi.suffix()).toStdWString()));
  prop->setRange(r0, r1, 1);

  TXsheet *top = scene->getChildStack()->getTopXsheet();
  std::vector<TXshColumn *> hidden;
  for (int c = 0; c < top->getColumnCount(); c++) {
    TXshColumn *column = top->getColumn(c);
    if (c == ownCol || !column || column->isEmpty() || !column->isPreviewVisible())
      continue;
    int a, b;
    if (!top->getCellRange(c, a, b)) continue;
    bool isShot = false;
    for (int r = a; r <= b; r++) {
      const TXshCell &cell = top->getCell(r, c);
      if (cell.isEmpty()) continue;
      isShot = cell.getChildLevel() != nullptr;
      break;
    }
    if (!isShot) continue;
    column->setPreviewVisible(false);
    hidden.push_back(column);
  }

  QEventLoop loop;
  QMetaObject::Connection conn = QObject::connect(
      ZtoryModel::instance(), &ZtoryModel::renderFinished, &loop, &QEventLoop::quit);
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  CommandManager::instance()->execute(MI_Render);
  for (TXshColumn *column : hidden) column->setPreviewVisible(true);
  timeout.start(10 * 60 * 1000);
  loop.exec();
  QObject::disconnect(conn);

  const QStringList made =
      QDir(fi.absolutePath()).entryList({stem + ".*"}, QDir::Files);
  if (made.isEmpty()) return false;
  QFile::remove(out);
  return QFile::rename(fi.absolutePath() + "/" + made.first(), out);
}

}  // namespace

//-----------------------------------------------------------------------------

void StoryboardPanel::onExportForAI() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || m_shots.empty()) {
    DVGui::warning(tr("There are no shots to export."));
    return;
  }
  const QString sceneName = QString::fromStdWString(scene->getSceneName());

  // ── The dialog: where, and which shots ──────────────────────────────────
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Export for AI"));
  auto *lay  = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout;
  auto *folderEdit = new QLineEdit(&dlg);
  folderEdit->setText(QSettings().value("Ztoryc/AIExportDir",
      scene->decodeFilePath(TFilePath("+outputs")).getQString()).toString());
  auto *browse = new QPushButton(tr("…"), &dlg);
  browse->setFixedWidth(28);
  auto *folderRow = new QHBoxLayout;
  folderRow->addWidget(folderEdit, 1);
  folderRow->addWidget(browse);
  form->addRow(tr("Folder:"), folderRow);
  auto *fromCombo = new QComboBox(&dlg), *toCombo = new QComboBox(&dlg);
  for (const Shot &s : m_shots) {
    fromCombo->addItem(s.data->label());
    toCombo->addItem(s.data->label());
  }
  const int cur = (m_selectedShotIndex >= 0 &&
                   m_selectedShotIndex < (int)m_shots.size())
                      ? m_selectedShotIndex : 0;
  fromCombo->setCurrentIndex(cur);
  toCombo->setCurrentIndex(cur);
  form->addRow(tr("From shot:"), fromCombo);
  form->addRow(tr("To shot:"), toCombo);
  // The shot's animatic as a clip: timing and motion of the whole shot, for a
  // motion reference and to compare the result against (Franco, 2026-10-25).
  auto *clipChk = new QCheckBox(tr("Include the shot's animatic clip (mp4)"), &dlg);
  clipChk->setChecked(QSettings().value("Ztoryc/AIExportClip", true).toBool());
  form->addRow(QString(), clipChk);
  lay->addLayout(form);
  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
  lay->addWidget(bbox);
  connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  connect(browse, &QPushButton::clicked, &dlg, [&dlg, folderEdit]() {
    const QString d = QFileDialog::getExistingDirectory(&dlg, StoryboardPanel::tr("Export for AI"),
                                                        folderEdit->text());
    if (!d.isEmpty()) folderEdit->setText(d);
  });
  if (dlg.exec() != QDialog::Accepted) return;
  const QString root = folderEdit->text().trimmed();
  if (root.isEmpty() || !QDir().mkpath(root)) {
    DVGui::warning(tr("The folder cannot be created: %1").arg(root));
    return;
  }
  QSettings().setValue("Ztoryc/AIExportDir", root);
  const bool withClip = clipChk->isChecked();
  QSettings().setValue("Ztoryc/AIExportClip", withClip);
  int from = fromCombo->currentIndex(), to = toCombo->currentIndex();
  if (from > to) std::swap(from, to);

  // The render settings and the burn-in are the scene's: put back afterwards.
  TOutputProperties *prop = scene->getProperties()->getOutputProperties();
  const TFilePath origPath = prop->getPath();
  int origR0, origR1, origStep;
  prop->getRange(origR0, origR1, origStep);
  const ZtoryBurnInConfig origBurnIn = ZtoryModel::instance()->burnIn();
  ZtoryModel::instance()->burnIn() = ZtoryBurnInConfig();

  QStringList done, problems;
  for (int si = from; si <= to; si++) {
    QString why;
    const QString dir = exportShotForAI(si, root, sceneName, withClip, &why);
    if (dir.isEmpty()) problems << QString("%1: %2").arg(m_shots[si].data->label(), why);
    else done << dir;
  }

  prop->setPath(origPath);
  prop->setRange(origR0, origR1, origStep);
  ZtoryModel::instance()->burnIn() = origBurnIn;

  QString msg = tr("Exported %1 shot(s) to %2").arg(done.size()).arg(root);
  if (!problems.isEmpty()) msg += "\n\n" + problems.join("\n");
  QMessageBox::information(this, tr("Export for AI"), msg);
}

//-----------------------------------------------------------------------------

QString StoryboardPanel::exportShotForAI(int si, const QString &root,
                                         const QString &sceneName, bool withClip,
                                         QString *why) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  ZtoryModel *m     = ZtoryModel::instance();
  const ShotData &sd = *m_shots[si].data;
  TXsheet *top = scene->getChildStack()->getTopXsheet();
  const int col = columnShowing(top, m_shots[si].childLevel, sd.xsheetColumn);
  int ts = 0, td = 0;
  if (!ZtoryShotOps::shotTrueSpan(top, col, ts, td) || td <= 0) {
    if (why) *why = tr("the shot has no frames in the main xsheet");
    return QString();
  }
  QString shotName = sd.label();
  if (const SequenceData *seq = m->findSequence(sd.sequenceId))
    shotName = seq->label + "_" + shotName;
  const QString dir = QDir(root).filePath(sceneName + "_" + shotName + "_AI");
  QDir().mkpath(dir + "/panels");
  const double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  const TDimension res = scene->getCurrentCamera()->getRes();

  // ── Panels ──────────────────────────────────────────────────────────────
  QJsonArray panels;
  QStringList csv = {"image,start_frame,duration_frames,duration_seconds,action,dialog,notes,camera"};
  for (int pi = 0; pi < (int)sd.panels.size(); pi++) {
    const PanelData &p = sd.panels[pi];
    const QString img = QString("%1_p%2_f%3_d%4.png")
                            .arg(shotName)
                            .arg(pi + 1, 2, 10, QChar('0'))
                            .arg(p.startFrame + 1, 4, 10, QChar('0'))
                            .arg(p.duration);
    const int row = ts + p.startFrame;
    if (!renderRange(scene, row, row, col, dir + "/panels/" + img)) {
      if (why) *why = tr("panel %1 was not rendered").arg(pi + 1);
      return QString();
    }
    const double secs = fps > 0 ? p.duration / fps : 0;
    csv << QStringList{csvField("panels/" + img), QString::number(p.startFrame + 1),
                       QString::number(p.duration), QString::number(secs, 'f', 2),
                       csvField(p.action), csvField(p.dialog), csvField(p.notes),
                       csvField(p.cameraMoveLabel)}.join(',');
    QJsonObject o;
    o["image"]           = "panels/" + img;
    o["startFrame"]      = p.startFrame + 1;
    o["durationFrames"]  = p.duration;
    o["durationSeconds"] = secs;
    o["action"]          = p.action;
    o["dialog"]          = p.dialog;
    o["notes"]           = p.notes;
    if (p.cameraMoveType != PanelData::CamNone) o["camera"] = p.cameraMoveLabel;
    panels.append(o);
  }

  // ── References: the breakdown's files ───────────────────────────────────
  QJsonArray assets;
  for (const ProjectShot &ps : m->projectShots()) {
    if (ps.uuid != sd.uuid) continue;
    for (const BreakdownEntry &be : ps.breakdown) {
      const Asset *a = m->assetByUuid(be.assetUuid);
      if (!a) continue;
      QJsonObject o;
      o["name"] = a->name;
      o["type"] = a->type;
      const QString file = a->noFile ? QString() : m->resolveAssetFile(*a);
      if (!file.isEmpty() && QFileInfo::exists(file)) {
        QDir().mkpath(dir + "/refs");
        const QString copy = a->name + "__" + QFileInfo(file).fileName();
        QFile::remove(dir + "/refs/" + copy);
        if (QFile::copy(file, dir + "/refs/" + copy)) o["file"] = "refs/" + copy;
      }
      assets.append(o);
    }
  }

  // ── Audio: the shot's slice of each sound column ────────────────────────
  QJsonArray audio;
  int n = 0;
  for (int c = 0; c < top->getColumnCount(); c++) {
    TXshColumn *column = top->getColumn(c);
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (!sc) continue;
    TSoundTrackP st = sc->getOverallSoundTrack(ts, ts + td, fps);  // end exclusive
    if (!st || st->getSampleCount() == 0) continue;
    const QString wav = shotName + "_audio" + (++n > 1 ? QString::number(n) : QString()) + ".wav";
    try {
      const TFilePath out(QString(dir + "/" + wav).toStdWString());
      if (TSystem::doesExistFileOrLevel(out)) TSystem::removeFileOrLevel(out);
      TSoundTrackWriter::save(out, st);
      audio.append(wav);
    } catch (...) {
    }
  }

  // ── The panel list and the manifest ─────────────────────────────────────
  QFile csvFile(dir + "/" + shotName + "_panels.csv");
  if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream ts2(&csvFile);
    ts2.setCodec("UTF-8");
    ts2 << csv.join('\n') << '\n';
  }
  QJsonObject manifest;
  manifest["scene"]          = sceneName;
  manifest["shot"]           = shotName;
  manifest["fps"]            = fps;
  manifest["width"]          = res.lx;
  manifest["height"]         = res.ly;
  manifest["durationFrames"] = td;
  manifest["panels"]         = panels;
  manifest["assets"]         = assets;
  manifest["audio"]          = audio;
  if (withClip) {
    const QString clip = shotName + "_animatic.mp4";
    if (renderRange(scene, ts, ts + td - 1, col, dir + "/" + clip))
      manifest["animatic"] = clip;
    else if (why)
      *why = tr("the animatic clip was not rendered");
  }
  QFile mf(dir + "/manifest.json");
  if (mf.open(QIODevice::WriteOnly))
    mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
  return dir;
}
