// Ztoryc: Export Layout (Franco, 2026-10-25, generalised 2026-10-27).
//
// One folder per shot, <chosen folder>/layout/<shot>/, for whoever works the
// shot after the storyboard — an animator, compositing, a generator:
//   panels/<shot>_pNN_fFFFF_dDD.png   every panel's first frame as the camera
//                                     sees it, named with start and duration
//   NN_<column>/NN_<column>.FFFF.png  every drawing of every column whose
//                                     preview is on, alone and with alpha,
//                                     named with the shot frame it starts at
//   <shot>.fcpxml                     the columns as tracks, to rebuild the
//                                     shot's compositing in an editor
//   <shot>.psd                        the panels flattened, or the columns as
//                                     layers (a group for a column with more
//                                     drawings), with the camera fields
//   <shot>_panels.csv, manifest.json  timing, texts, layers, casting
//   refs/, <shot>_audio.wav, <shot>_animatic.mp4
// A column whose preview is off is not rendered: that is how a reference
// (frames of another film, say) stays out of the package.
// The format is decided by its use (filorosso's 08_ai/doc/export_ai_feedback.md).

#include "storyboardpanel.h"
#include "ztorymodel.h"
#include "ztoryshotops.h"
#include "ztorypsdwriter.h"

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
#include "toonz/stage.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobject.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"
#include "toutputproperties.h"

// TnzCore includes
#include "tproperty.h"
#include "tsound_io.h"
#include "tsystem.h"

// Qt includes
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
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
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// One progress bar for the whole export, instead of a flipbook per render.
QProgressDialog *s_progress = nullptr;

QString csvField(QString s) {
  s.replace('"', "\"\"");
  return '"' + s.replace('\n', ' ') + '"';
}

QString safeName(QString s) {
  s = s.simplified();
  for (QChar &c : s)
    if (!c.isLetterOrNumber() && c != '_' && c != '-') c = '_';
  return s.isEmpty() ? QString("col") : s;
}

double round2(double v) { return qRound(v * 100.0) / 100.0; }

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

bool isPictureColumn(TXshColumn *column) {
  return column && !column->isEmpty() && !column->getSoundColumn() &&
         !column->getSoundTextColumn();
}

// Renders main-xsheet rows [r0, r1] to `out` — a .png still or a .mp4 clip,
// by its extension — with the shot column `ownCol` alone among the shots, as
// the per-shot animatic export does. With `sub`, also only the column
// `subKeep` of the shot's sub-xsheet (-1: all of them). Tahoma names a still
// «name.FFFF.png»; the result is renamed to `out`.
bool renderRange(ToonzScene *scene, int r0, int r1, int ownCol, TXsheet *sub,
                 int subKeep, const QString &out) {
  if (s_progress) {
    if (s_progress->wasCanceled()) return false;
    s_progress->setLabelText(QFileInfo(out).fileName());
    s_progress->setValue(s_progress->value() + 1);
    QApplication::processEvents();
  }
  TOutputProperties *prop = scene->getProperties()->getOutputProperties();
  const QFileInfo fi(out);
  const QString stem = "ztorylayout_tmp_" + fi.completeBaseName();
  prop->setPath(TFilePath(fi.absolutePath().toStdWString()) +
                TFilePath((stem + "." + fi.suffix()).toStdWString()));
  prop->setRange(r0, r1, 1);

  std::vector<TXshColumn *> hidden;
  auto hide = [&hidden](TXshColumn *column) {
    if (!column || !column->isPreviewVisible()) return;
    column->setPreviewVisible(false);
    hidden.push_back(column);
  };
  TXsheet *top = scene->getChildStack()->getTopXsheet();
  for (int c = 0; c < top->getColumnCount(); c++) {
    TXshColumn *column = top->getColumn(c);
    if (c == ownCol || !column || column->isEmpty()) continue;
    int a, b;
    if (!top->getCellRange(c, a, b)) continue;
    for (int r = a; r <= b; r++) {
      const TXshCell &cell = top->getCell(r, c);
      if (cell.isEmpty()) continue;
      if (cell.getChildLevel()) hide(column);
      break;
    }
  }
  if (sub && subKeep >= 0)
    for (int c = 0; c < sub->getColumnCount(); c++)
      if (c != subKeep && isPictureColumn(sub->getColumn(c))) hide(sub->getColumn(c));

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

// The camera fields of a panel with a camera move, on a transparent canvas:
// the rendered frame (the whole canvas) and where the camera goes, with an
// arrow — the overlay the Board draws on the thumbnails.
QImage cameraFields(const PanelData &p, int w, int h) {
  QImage img(w, h, QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  if (p.cameraMoveType == PanelData::CamNone || p.camW < 1e-6 || p.camH < 1e-6)
    return img;
  const double hw = Stage::inch * p.camW / 2.0, hh = Stage::inch * p.camH / 2.0;
  const TAffine a0(p.camA0[0], p.camA0[1], p.camA0[2], p.camA0[3], p.camA0[4], p.camA0[5]);
  const TAffine a1(p.camA1[0], p.camA1[1], p.camA1[2], p.camA1[3], p.camA1[4], p.camA1[5]);
  const TAffine m = a0.inv() * a1;  // end camera, in the start camera's frame
  auto px = [&](const TPointD &q) {
    return QPointF((q.x + hw) / (2 * hw) * w, (hh - q.y) / (2 * hh) * h);
  };
  const TPointD loc[4] = {TPointD(-hw, hh), TPointD(hw, hh), TPointD(hw, -hh),
                          TPointD(-hw, -hh)};
  QPolygonF from, to;
  for (const TPointD &q : loc) {
    from << px(q);
    to << px(m * q);
  }
  QPainter g(&img);
  g.setRenderHint(QPainter::Antialiasing);
  QPen pen(QColor(255, 60, 60), qMax(2, w / 400));
  g.setPen(pen);
  g.drawPolygon(from);
  pen.setStyle(Qt::DashLine);
  g.setPen(pen);
  g.drawPolygon(to);
  const QPointF c0 = (from[0] + from[2]) / 2, c1 = (to[0] + to[2]) / 2;
  g.drawLine(c0, c1);
  g.setFont(QFont("Helvetica", qMax(12, h / 40)));
  g.drawText(to[0] + QPointF(8, h / 30.0), p.cameraMoveLabel);
  return img;
}

// The camera field, always there in the PSD (Franco, 2026-10-27): the frame
// with its centre cross, so what is framed shows even on a still shot.
QImage fieldGuide(int w, int h) {
  QImage img(w, h, QImage::Format_ARGB32);
  img.fill(Qt::transparent);
  QPainter g(&img);
  g.setRenderHint(QPainter::Antialiasing);
  const int t = qMax(2, w / 400);
  g.setPen(QPen(QColor(255, 60, 60), t));
  g.drawRect(QRectF(t / 2.0, t / 2.0, w - t, h - t));
  const double cx = w / 2.0, cy = h / 2.0, arm = h / 20.0;
  g.drawLine(QPointF(cx - arm, cy), QPointF(cx + arm, cy));
  g.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy + arm));
  return img;
}

QString fcpTime(int frames, double fps) {
  if (qAbs(fps - qRound(fps)) < 1e-6)
    return QString("%1/%2s").arg(frames).arg(qRound(fps));
  return QString("%1/%2s").arg(qint64(frames) * 1001).arg(qRound(fps * 1000));
}

struct Pose {
  QString file;  // relative to the shot folder
  int start = 0, duration = 0;  // shot frames, from 1
};
struct Column {
  int index = 0;
  QString name, folder;  // «02_cupido»
  QVector<Pose> poses;
};

// The columns as tracks (column 1 lowest) and every drawing as a clip, on a
// gap as long as the shot: an editor rebuilds the compositing from it.
void writeFcpxml(const QString &path, const QString &shotName,
                 const QVector<Column> &cols, const QString &dir, int frames,
                 double fps, int w, int h) {
  QString res = QString("    <format id=\"r1\" frameDuration=\"%1\" width=\"%2\" height=\"%3\"/>\n")
                    .arg(fcpTime(1, fps)).arg(w).arg(h);
  QString clips;
  int n = 2;
  for (int lane = 0; lane < cols.size(); lane++)
    for (const Pose &p : cols[lane].poses) {
      const QString id = QString("r%1").arg(n++);
      res += QString("    <asset id=\"%1\" name=\"%2\" src=\"%3\" start=\"0s\" duration=\"0s\" hasVideo=\"1\" format=\"r1\"/>\n")
                 .arg(id, QFileInfo(p.file).fileName(),
                      QUrl::fromLocalFile(dir + "/" + p.file).toString(QUrl::FullyEncoded));
      clips += QString("            <video ref=\"%1\" lane=\"%2\" offset=\"%3\" duration=\"%4\" start=\"0s\"/>\n")
                   .arg(id).arg(lane + 1).arg(fcpTime(p.start - 1, fps)).arg(fcpTime(p.duration, fps));
    }
  const QString total = fcpTime(frames, fps);
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QTextStream s(&f);
  s.setCodec("UTF-8");
  s << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE fcpxml>\n<fcpxml version=\"1.9\">\n"
    << "  <resources>\n" << res << "  </resources>\n"
    << "  <library>\n    <event name=\"" << shotName << "\">\n"
    << "      <project name=\"" << shotName << "_layout\">\n"
    << "        <sequence format=\"r1\" duration=\"" << total << "\" tcStart=\"0s\">\n"
    << "          <spine>\n          <gap name=\"" << shotName << "\" offset=\"0s\" duration=\""
    << total << "\" start=\"0s\">\n" << clips
    << "          </gap>\n          </spine>\n        </sequence>\n      </project>\n"
    << "    </event>\n  </library>\n</fcpxml>\n";
}

QByteArray fileHash(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) return QByteArray();
  return QCryptographicHash::hash(f.readAll(), QCryptographicHash::Md5);
}

}  // namespace

//-----------------------------------------------------------------------------

void StoryboardPanel::onExportLayout() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || m_shots.empty()) {
    DVGui::warning(tr("There are no shots to export."));
    return;
  }
  const QString sceneName = QString::fromStdWString(scene->getSceneName());

  // ── The dialog ──────────────────────────────────────────────────────────
  QDialog dlg(this);
  dlg.setWindowTitle(tr("Export Layout"));
  auto *lay  = new QVBoxLayout(&dlg);
  auto *form = new QFormLayout;
  auto *folderEdit = new QLineEdit(&dlg);
  folderEdit->setText(QSettings().value("Ztoryc/LayoutExportDir",
      scene->decodeFilePath(TFilePath("+outputs")).getQString()).toString());
  auto *browse = new QPushButton(tr("…"), &dlg);
  browse->setFixedWidth(28);
  auto *folderRow = new QHBoxLayout;
  folderRow->addWidget(folderEdit, 1);
  folderRow->addWidget(browse);
  form->addRow(tr("Output folder:"), folderRow);
  auto *fromCombo = new QComboBox(&dlg), *toCombo = new QComboBox(&dlg);
  for (const Shot &s : m_shots) {
    fromCombo->addItem(s.data->label());
    toCombo->addItem(s.data->label());
  }
  const int cur = (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
                      ? m_selectedShotIndex : 0;
  fromCombo->setCurrentIndex(cur);
  toCombo->setCurrentIndex(cur);
  form->addRow(tr("From shot:"), fromCombo);
  form->addRow(tr("To shot:"), toCombo);
  QSettings st;
  auto *layersChk = new QCheckBox(tr("Columns as image sequences, and an FCPXML to rebuild them"), &dlg);
  layersChk->setChecked(st.value("Ztoryc/LayoutExportLayers", true).toBool());
  layersChk->setToolTip(tr("Every drawing of every column whose preview is on, alone and with "
                           "alpha. A column with its preview off (a reference) is left out."));
  form->addRow(QString(), layersChk);
  auto *psdCombo = new QComboBox(&dlg);
  psdCombo->addItems({tr("No PSD"), tr("PSD: panels as flattened images"),
                      tr("PSD: columns as layers")});
  psdCombo->setCurrentIndex(st.value("Ztoryc/LayoutExportPsd", 2).toInt());
  form->addRow(tr("Photoshop:"), psdCombo);
  auto *clipChk = new QCheckBox(tr("Include the shot's animatic clip (mp4)"), &dlg);
  clipChk->setChecked(st.value("Ztoryc/LayoutExportClip", true).toBool());
  form->addRow(QString(), clipChk);
  lay->addLayout(form);
  auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Export"));
  lay->addWidget(bbox);
  connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
  connect(browse, &QPushButton::clicked, &dlg, [&dlg, folderEdit]() {
    const QString d = QFileDialog::getExistingDirectory(
        &dlg, StoryboardPanel::tr("Export Layout"), folderEdit->text());
    if (!d.isEmpty()) folderEdit->setText(d);
  });
  if (dlg.exec() != QDialog::Accepted) return;

  const QString root = folderEdit->text().trimmed();
  if (root.isEmpty() || !QDir().mkpath(root + "/layout")) {
    DVGui::warning(tr("The folder cannot be created: %1").arg(root));
    return;
  }
  LayoutOptions opt;
  opt.layers  = layersChk->isChecked();
  opt.psdMode = psdCombo->currentIndex();
  opt.clip    = clipChk->isChecked();
  st.setValue("Ztoryc/LayoutExportDir", root);
  st.setValue("Ztoryc/LayoutExportLayers", opt.layers);
  st.setValue("Ztoryc/LayoutExportPsd", opt.psdMode);
  st.setValue("Ztoryc/LayoutExportClip", opt.clip);
  int from = fromCombo->currentIndex(), to = toCombo->currentIndex();
  if (from > to) std::swap(from, to);

  // The scene's render settings, burn-in and PNG alpha: put back afterwards.
  TOutputProperties *prop = scene->getProperties()->getOutputProperties();
  const TFilePath origPath = prop->getPath();
  int origR0, origR1, origStep;
  prop->getRange(origR0, origR1, origStep);
  const ZtoryBurnInConfig origBurnIn = ZtoryModel::instance()->burnIn();
  ZtoryModel::instance()->burnIn() = ZtoryBurnInConfig();
  TBoolProperty *alpha = nullptr;
  bool origAlpha       = true;
  if (TPropertyGroup *pg = prop->getFileFormatProperties("png"))
    if ((alpha = dynamic_cast<TBoolProperty *>(pg->getProperty("Alpha Channel")))) {
      origAlpha = alpha->getValue();
      alpha->setValue(true);
    }

  // Every render would open its result in a flipbook: off while exporting.
  Preferences *pref     = Preferences::instance();
  const bool origFlip   = pref->isGeneratedMovieViewEnabled();
  pref->setValue(generatedMovieViewEnabled, false, false);
  // the renders to come: the panels, every drawing of the shown columns, the clip
  int steps = 0;
  for (int si = from; si <= to; si++) {
    steps += (int)m_shots[si].data->panels.size() + (opt.clip ? 1 : 0);
    TXsheet *sub = m_shots[si].childLevel ? m_shots[si].childLevel->getXsheet() : nullptr;
    if (!sub || !(opt.layers || opt.psdMode == 2)) continue;
    for (int c = 0; c < sub->getColumnCount(); c++) {
      if (!isPictureColumn(sub->getColumn(c)) || !sub->getColumn(c)->isPreviewVisible())
        continue;
      TXshCell prev;
      for (int r = 0; r < sub->getFrameCount(); r++) {
        const TXshCell cell = sub->getCell(r, c);
        if (cell != prev && !cell.isEmpty()) steps++;
        prev = cell;
      }
    }
  }
  QProgressDialog progress(tr("Exporting the layout…"), tr("Cancel"), 0, qMax(1, steps), this);
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(0);
  s_progress = &progress;

  QStringList done, problems;
  for (int si = from; si <= to && !progress.wasCanceled(); si++) {
    QStringList why;
    const QString dir = exportShotLayout(si, root + "/layout", sceneName, opt, &why);
    if (dir.isEmpty()) problems << QString("%1: %2").arg(m_shots[si].data->label(), why.join("; "));
    else {
      done << dir;
      for (const QString &w : why) problems << QString("%1: %2").arg(m_shots[si].data->label(), w);
    }
  }

  s_progress = nullptr;
  progress.close();
  pref->setValue(generatedMovieViewEnabled, origFlip, false);
  prop->setPath(origPath);
  prop->setRange(origR0, origR1, origStep);
  ZtoryModel::instance()->burnIn() = origBurnIn;
  if (alpha) alpha->setValue(origAlpha);
  if (progress.wasCanceled()) problems.prepend(tr("Export cancelled: the last shot is incomplete."));

  QString msg = tr("Exported %1 shot(s) to %2").arg(done.size()).arg(root + "/layout");
  if (!problems.isEmpty()) msg += "\n\n" + problems.join("\n");
  QMessageBox::information(this, tr("Export Layout"), msg);
}

//-----------------------------------------------------------------------------

QString StoryboardPanel::exportShotLayout(int si, const QString &root,
                                          const QString &sceneName,
                                          const LayoutOptions &opt,
                                          QStringList *warnings) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  ZtoryModel *m     = ZtoryModel::instance();
  const ShotData &sd = *m_shots[si].data;
  TXsheet *top = scene->getChildStack()->getTopXsheet();
  const int col = columnShowing(top, m_shots[si].childLevel, sd.xsheetColumn);
  int ts = 0, td = 0;
  if (!ZtoryShotOps::shotTrueSpan(top, col, ts, td) || td <= 0) {
    if (warnings) *warnings << tr("the shot has no frames in the main xsheet");
    return QString();
  }
  TXsheet *sub = m_shots[si].childLevel ? m_shots[si].childLevel->getXsheet() : nullptr;
  const int head = sub ? ZtoryShotOps::xdInHeadOffset(sub) : 0;
  // a sub-xsheet row as a main-xsheet row (the head copies of a dissolve)
  auto mainRow = [ts, head](int subRow) { return ts + qMax(0, subRow - head); };

  QString shotName = sd.label();
  if (const SequenceData *seq = m->findSequence(sd.sequenceId))
    shotName = seq->label + "_" + shotName;
  const QString dir = QDir(root).filePath(shotName);
  QDir().mkpath(dir + "/panels");
  const double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  const TDimension res = scene->getCurrentCamera()->getRes();

  // ── Columns: names, and every drawing change while the shot plays ───────
  QVector<Column> cols;
  QHash<QString, QString> assetByName;  // lower-case column/level name → asset
  const ProjectShot *ps = nullptr;
  for (const ProjectShot &p : m->projectShots())
    if (p.uuid == sd.uuid) ps = &p;
  if (ps)
    for (const BreakdownEntry &be : ps->breakdown)
      if (const Asset *a = m->assetByUuid(be.assetUuid))
        assetByName.insert(a->name.toLower(), a->name);
  const int rows = td + head;
  if (sub)
    for (int c = 0; c < sub->getColumnCount(); c++) {
      TXshColumn *column = sub->getColumn(c);
      if (!isPictureColumn(column) || !column->isPreviewVisible()) continue;
      Column cd;
      cd.index = c;
      cd.name  = QString::fromStdString(
          sub->getStageObject(TStageObjectId::ColumnId(c))->getName());
      if (cd.name == QString("Col%1").arg(c + 1))  // unnamed: the level's name
        for (int r = 0; r < rows; r++) {
          const TXshCell cell = sub->getCell(r, c);
          if (!cell.isEmpty() && cell.m_level) {
            cd.name = QString::fromStdWString(cell.m_level->getName());
            break;
          }
        }
      cd.folder = QString("%1_%2").arg(c + 1, 2, 10, QChar('0')).arg(safeName(cd.name));
      TXshCell prev;
      for (int r = 0; r <= rows; r++) {
        const TXshCell cell = r < rows ? sub->getCell(r, c) : TXshCell();
        if (r < rows && cell == prev) continue;
        if (!cd.poses.isEmpty() && cd.poses.last().duration == 0)
          cd.poses.last().duration = r + 1 - cd.poses.last().start;
        prev = cell;
        if (r >= rows || cell.isEmpty()) continue;
        Pose p;
        p.start = r + 1;
        p.file  = QString("%1/%1.%2.png").arg(cd.folder).arg(r + 1, 4, 10, QChar('0'));
        cd.poses << p;
      }
      cols << cd;
    }

  if (opt.layers || opt.psdMode == 2)
    for (const Column &cd : cols) {
      QDir().mkpath(dir + "/" + cd.folder);
      for (const Pose &p : cd.poses)
        if (!renderRange(scene, mainRow(p.start - 1), mainRow(p.start - 1), col, sub,
                         cd.index, dir + "/" + p.file)) {
          if (warnings) *warnings << tr("%1 was not rendered").arg(p.file);
        }
    }

  // ── Panels ──────────────────────────────────────────────────────────────
  QJsonArray panels;
  QStringList csv = {"image,start_frame,duration_frames,duration_seconds,key,beat,hold_of,action,dialog,notes,camera"};
  QByteArray prevHash;
  QString prevId;
  int beat = 0;
  QVector<QPair<QString, QString>> panelFiles;  // id, file
  for (int pi = 0; pi < (int)sd.panels.size(); pi++) {
    const PanelData &p = sd.panels[pi];
    const QString id  = QString("p%1").arg(pi + 1, 2, 10, QChar('0'));
    const QString img = QString("%1_%2_f%3_d%4.png").arg(shotName, id)
                            .arg(p.startFrame + 1, 4, 10, QChar('0')).arg(p.duration);
    const QString file = "panels/" + img;
    if (!renderRange(scene, mainRow(p.startFrame), mainRow(p.startFrame), col, nullptr,
                     -1, dir + "/" + file)) {
      if (warnings) *warnings << tr("panel %1 was not rendered").arg(pi + 1);
      return QString();
    }
    panelFiles << qMakePair(id, file);
    const QString action = p.action.simplified();
    const bool key       = !action.isEmpty();
    if (key || beat == 0) beat++;
    QJsonObject o;
    o["id"]              = id;
    o["image"]           = file;
    o["startFrame"]      = p.startFrame + 1;
    o["durationFrames"]  = p.duration;
    o["durationSeconds"] = round2(fps > 0 ? p.duration / fps : 0);
    o["key"]             = key;
    o["beat"]            = beat;
    // the same picture as the panel before: a held pose, not a new one
    const QByteArray hash = fileHash(dir + "/" + file);
    QString holdOf;
    if (!hash.isEmpty() && hash == prevHash) holdOf = prevId;
    else { prevHash = hash; prevId = id; }
    if (!holdOf.isEmpty()) o["holdOf"] = holdOf;
    o["action"] = action;
    o["dialog"] = p.dialog.simplified();
    o["notes"]  = p.notes.simplified();
    QString camText;
    if (p.cameraMoveType != PanelData::CamNone) {
      camText = p.cameraMoveLabel;
      QJsonObject cam;
      cam["move"] = p.cameraMoveLabel;
      QJsonArray a0, a1;
      for (int k = 0; k < 6; k++) { a0.append(p.camA0[k]); a1.append(p.camA1[k]); }
      cam["from"] = a0;
      cam["to"]   = a1;
      o["camera"] = cam;
    }
    // casting: the assets whose column shows a drawing at the panel's start
    QJsonArray cast, layers;
    if (sub)
      for (int c = 0; c < sub->getColumnCount(); c++) {
        const TXshCell cell = sub->getCell(p.startFrame, c);
        if (cell.isEmpty() || !cell.m_level) continue;
        const QString colName = QString::fromStdString(
            sub->getStageObject(TStageObjectId::ColumnId(c))->getName()).toLower();
        const QString lvName = QString::fromStdWString(cell.m_level->getName()).toLower();
        const QString asset = assetByName.value(colName, assetByName.value(lvName));
        if (!asset.isEmpty() && !cast.contains(asset)) cast.append(asset);
      }
    for (const Column &cd : cols)
      for (const Pose &ps2 : cd.poses)
        if (ps2.start <= p.startFrame + 1 && p.startFrame + 1 < ps2.start + ps2.duration) {
          QJsonObject l;
          l["column"] = cd.folder;
          l["file"]   = ps2.file;
          layers.append(l);
        }
    o["assets"] = cast;
    if (!layers.isEmpty()) o["layers"] = layers;
    panels.append(o);
    csv << QStringList{csvField(file), QString::number(p.startFrame + 1),
                       QString::number(p.duration),
                       QString::number(round2(fps > 0 ? p.duration / fps : 0), 'f', 2),
                       key ? "1" : "0", QString::number(beat), holdOf, csvField(action),
                       csvField(p.dialog.simplified()), csvField(p.notes.simplified()),
                       csvField(camText)}.join(',');
  }

  // ── References: the breakdown's files ───────────────────────────────────
  QJsonArray assets;
  if (ps)
    for (const BreakdownEntry &be : ps->breakdown) {
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
      if (!o.contains("file") && !a->noFile) o["missing"] = true;
      assets.append(o);
    }

  // ── Audio: the shot's slice of each sound column ────────────────────────
  QJsonArray audio;
  int n = 0;
  for (int c = 0; c < top->getColumnCount(); c++) {
    TXshColumn *column  = top->getColumn(c);
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (!sc) continue;
    TSoundTrackP track = sc->getOverallSoundTrack(ts, ts + td, fps);  // end exclusive
    if (!track || track->getSampleCount() == 0) continue;
    const QString wav = shotName + "_audio" + (++n > 1 ? QString::number(n) : QString()) + ".wav";
    try {
      const TFilePath out(QString(dir + "/" + wav).toStdWString());
      if (TSystem::doesExistFileOrLevel(out)) TSystem::removeFileOrLevel(out);
      TSoundTrackWriter::save(out, track);
      audio.append(wav);
    } catch (...) {
      if (warnings) *warnings << tr("the audio was not written");
    }
  }

  // ── FCPXML: the columns as tracks ───────────────────────────────────────
  QJsonArray columnsJson;
  for (const Column &cd : cols) {
    QJsonObject o;
    o["column"] = cd.index + 1;
    o["name"]   = cd.name;
    o["folder"] = cd.folder;
    QJsonArray poses;
    for (const Pose &p : cd.poses) {
      QJsonObject q;
      q["file"]           = p.file;
      q["startFrame"]     = p.start;
      q["durationFrames"] = p.duration;
      poses.append(q);
    }
    o["poses"] = poses;
    columnsJson.append(o);
  }
  QString fcpxml;
  if (opt.layers && !cols.isEmpty()) {
    fcpxml = shotName + ".fcpxml";
    writeFcpxml(dir + "/" + fcpxml, shotName, cols, dir, td, fps, res.lx, res.ly);
  }

  // ── PSD ─────────────────────────────────────────────────────────────────
  QString psd;
  if (opt.psdMode > 0) {
    QVector<ZtoryPsdGroup> groups;
    // the camera fields on top, one layer per panel with a camera move
    ZtoryPsdGroup camGroup;
    for (int pi = 0; pi < (int)sd.panels.size(); pi++)
      if (sd.panels[pi].cameraMoveType != PanelData::CamNone)
        camGroup.layers << ZtoryPsdLayer{
            QString("CAMERA_%1").arg(panelFiles[pi].first),
            cameraFields(sd.panels[pi], res.lx, res.ly), true};
    camGroup.layers << ZtoryPsdLayer{"FIELD_GUIDE", fieldGuide(res.lx, res.ly), true};
    groups << camGroup;
    if (opt.psdMode == 1) {
      ZtoryPsdGroup g;
      g.name = shotName;
      for (const auto &pf : panelFiles)
        g.layers << ZtoryPsdLayer{QFileInfo(pf.second).completeBaseName(),
                                  QImage(dir + "/" + pf.second), true};
      groups << g;
    } else {
      for (int k = cols.size() - 1; k >= 0; k--) {  // the higher column on top
        const Column &cd = cols[k];
        ZtoryPsdGroup g;
        if (cd.poses.size() > 1) g.name = cd.folder;
        for (const Pose &p : cd.poses)
          g.layers << ZtoryPsdLayer{
              cd.poses.size() > 1 ? QFileInfo(p.file).completeBaseName() : cd.folder,
              QImage(dir + "/" + p.file), true};
        if (!g.layers.isEmpty()) groups << g;
      }
    }
    psd = shotName + ".psd";
    QString why;
    const QImage merged = panelFiles.isEmpty() ? QImage()
                                               : QImage(dir + "/" + panelFiles.first().second);
    if (!ztoryWritePsd(dir + "/" + psd, res.lx, res.ly, groups, merged, &why)) {
      if (warnings) *warnings << tr("the PSD was not written: %1").arg(why);
      psd.clear();
    }
  }

  // ── The animatic clip ───────────────────────────────────────────────────
  QString clip;
  if (opt.clip) {
    clip = shotName + "_animatic.mp4";
    if (!renderRange(scene, ts, ts + td - 1, col, nullptr, -1, dir + "/" + clip)) {
      if (warnings) *warnings << tr("the animatic clip was not rendered");
      clip.clear();
    }
  }

  // ── The panel list and the manifest ─────────────────────────────────────
  QFile csvFile(dir + "/" + shotName + "_panels.csv");
  if (csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&csvFile);
    out.setCodec("UTF-8");
    out << csv.join('\n') << '\n';
  }
  QJsonObject manifest;
  manifest["version"]        = 2;
  manifest["scene"]          = sceneName;
  manifest["shot"]           = shotName;
  manifest["fps"]            = fps;
  manifest["width"]          = res.lx;
  manifest["height"]         = res.ly;
  manifest["durationFrames"] = td;
  manifest["panels"]         = panels;
  manifest["columns"]        = columnsJson;
  manifest["assets"]         = assets;
  manifest["audio"]          = audio;
  if (!fcpxml.isEmpty()) manifest["fcpxml"] = fcpxml;
  if (!psd.isEmpty()) manifest["psd"] = psd;
  if (!clip.isEmpty()) manifest["animatic"] = clip;
  QFile mf(dir + "/manifest.json");
  if (mf.open(QIODevice::WriteOnly))
    mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
  return dir;
}
