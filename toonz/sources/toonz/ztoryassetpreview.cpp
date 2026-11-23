#include "ztoryassetpreview.h"

#include "ztorymodel.h"

#include "toonz/preferences.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonzqt/gutil.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include "trop.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QImage>
#include <QPainter>

namespace {

// The asset's task named `name` in the chain, matched without case like the
// rest of the task names (ZtoryTaskFlow::foldTaskNames).
const TaskState *taskNamed(const Asset &a, const QString &name,
                           QString *key = nullptr) {
  for (auto it = a.tasks.constBegin(); it != a.tasks.constEnd(); ++it)
    if (it.key().compare(name, Qt::CaseInsensitive) == 0) {
      if (key) *key = it.key();
      return &it.value();
    }
  return nullptr;
}

// The task in progress: the chain's first task not Done on Kitsu, or the
// last when all are. Only tasks with a base (hasSynced) — the ones Kitsu is
// known to have: a task of the chain that Kitsu hides for this asset type
// would make the upload fail.
QString carryingTask(const Asset &a, TaskStatus *status) {
  const QStringList chain =
      ZtoryModel::instance()->assetTaskTypesForType(a.type);
  QString last;
  for (const QString &name : chain) {
    QString key;
    const TaskState *t = taskNamed(a, name, &key);
    if (!t || !t->hasSynced) continue;
    last    = key;
    *status = t->synced;
    if (t->synced != TaskStatus::Done) return key;
  }
  return last;  // all Done (or none on Kitsu: empty)
}

}  // namespace

// The PSDs a character's scene is built from, read in the .tnz itself: its
// levels are their layers, «+extras/BRONTOLO/ch_brontolo#@1#group.psd».
// For a character with no PSD to rig linked — fifteen of seventeen on a real episode,
// whose scenes were made before the link existed (2026-09-27).
QStringList ztoryPsdsUsedByScene(const QString &tnz) {
  QStringList out;
  QFile f(tnz);
  if (!f.open(QIODevice::ReadOnly)) return out;
  const QString text = QString::fromUtf8(f.readAll());
  static const QRegularExpression kPsd(QStringLiteral(R"(([^<>"\n]+?\.psd))"),
                                       QRegularExpression::CaseInsensitiveOption);
  std::shared_ptr<TProject> project;  // loaded once, at the first alias
  QRegularExpressionMatchIterator it = kPsd.globalMatch(text);
  while (it.hasNext()) {
    QString path = it.next().captured(1).trimmed();
    // XML text: «&amp;» in a name is «&» on disk.
    path.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"")
        .replace("&apos;", "'").replace("&amp;", "&");
    // «name#layer#group.psd» is one layer of «name.psd».
    const int slash = path.lastIndexOf('/');
    const int hash  = path.indexOf('#', slash + 1);
    if (hash >= 0) path = path.left(hash) + ".psd";
    TFilePath fp(path.toStdWString());
    if (path.startsWith('+')) {
      // A project folder alias: the project decides where «+extras» is.
      const int cut       = path.indexOf('/');
      const QString alias = path.mid(1, cut < 0 ? -1 : cut - 1);
      const QString rest  = cut < 0 ? QString() : path.mid(cut + 1);
      if (!project)
        project = TProjectManager::instance()->loadSceneProject(
            TFilePath(tnz.toStdWString()));
      if (!project) continue;
      const TFilePath folder = project->getFolder(alias.toStdString(), true);
      if (folder.isEmpty()) continue;
      fp = folder + TFilePath(rest.toStdWString());
    } else if (!fp.isAbsolute())
      fp = TFilePath(tnz.toStdWString()).getParentDir() + fp;
    const QString abs = fp.getQString();
    if (!out.contains(abs) && QFileInfo::exists(abs)) out << abs;
  }
  return out;
}

namespace {

// The formats a picture is read from. A video would go through ffmpeg,
// which extracts every frame on the UI thread; a .tlv or .pli is not a
// raster and would fail at every Sync, forever.
bool isPictureFile(const QString &file) {
  static const QStringList kPictures = {"psd", "png", "jpg", "jpeg", "tif",
                                        "tiff", "bmp", "tga"};
  return kPictures.contains(QFileInfo(file).suffix().toLower());
}

// The file's NAME, not its path: the same project opened on the Dell or
// another mount must not upload every preview again.
QString signatureOf(const QString &file, const Asset &a) {
  const QFileInfo fi(file);
  // «2»: the rendering's version. Previews made before 2026-09-27 got no
  // cover (the 400 above) and no white background: a new version uploads
  // them once more.
  // «3» only for a name Tahoma reads as a frame («CASA_INT_2.png»): before
  // 2026-11-23 those were rendered from the level's FIRST frame, maybe
  // another file. Only they are sent again, not every preview.
  const bool framed =
      !TFilePath(file.toStdWString()).getFrame().isNoFrame();
  return QStringLiteral("%1|%2|%3|%4|%5")
      .arg(framed ? 3 : 2)
      .arg(fi.fileName())
      .arg(fi.size())
      .arg(fi.lastModified().toMSecsSinceEpoch())
      .arg(a.kitsuAssetId);
}

}  // namespace

ZtoryAssetPreviewSource ztoryAssetPreviewSource(
    const Asset &a, QHash<QString, QFileInfoList> *dirCache) {
  ZtoryAssetPreviewSource src;
  src.task = carryingTask(a, &src.status);
  if (a.noFile || src.task.isEmpty()) return src;  // nothing to look for
  ZtoryModel *m = ZtoryModel::instance();
  ZtoryModel::AssetMatch match = ZtoryModel::AssetMatch::None;
  const QString file = m->resolveAssetFile(a, nullptr, dirCache, &match);
  if (match == ZtoryModel::AssetMatch::NearName) {
    src.nearNameOnly = true;
    return src;
  }
  const bool sure = match == ZtoryModel::AssetMatch::Linked ||
                    match == ZtoryModel::AssetMatch::Exact ||
                    match == ZtoryModel::AssetMatch::Convention;
  QString picture;
  // A character: its PSD to rig first. The icon of its scene is only a
  // fallback — on a real episode all seventeen were fully transparent, written on
  // an empty frame (2026-09-27).
  if (ZtoryModel::isCharacterType(a.type)) {
    const QString rig = m->resolveAssetRigPsd(a);
    if (!rig.isEmpty() && QFileInfo::exists(rig))
      picture = rig;
    else if (sure && file.endsWith(".tnz", Qt::CaseInsensitive))
      picture = ztoryPsdsUsedByScene(file).value(0);  // its scene's PSD
  }
  if (picture.isEmpty() && sure && !file.isEmpty()) {
    if (file.endsWith(".tnz", Qt::CaseInsensitive)) {
      // A scene is not a picture: its icon is, written at every save.
      const QString icon =
          ToonzScene::getIconPath(TFilePath(file.toStdWString())).getQString();
      if (QFileInfo::exists(icon)) picture = icon;
    } else if (isPictureFile(file))
      picture = file;
  }
  if (picture.isEmpty() || !isPictureFile(picture) ||
      !QFileInfo::exists(picture))
    return src;
  src.file      = picture;
  src.signature = signatureOf(picture, a);
  return src;
}

bool ztoryRenderAssetPreview(const QString &file, const QString &outPng,
                             QString *why) {
  QImage img;
  // Tahoma's own readers: they read what Ztoryc imports — a PSD with no
  // layer named is read MERGED (tiio_psd.cpp, layerId 0), which Qt cannot.
  try {
    // «CASA_INT_2.png» is, for Tahoma, frame 2 of the level «CASA_INT»
    // (the underscore is a frame separator): its first frame is
    // CASA_INT_1.png, another background. The file's own frame is read,
    // the first only for a file that has none (Franco, 2026-11-23: the
    // cover of casa_int_02 on Kitsu was casa_int_01).
    const TFilePath path(file.toStdWString());
    TLevelReaderP lr(path);
    TLevelP level = lr ? lr->loadInfo() : TLevelP();
    if (level && level->begin() != level->end()) {
      const TFrameId own = path.getFrame();
      const TFrameId fid =
          !own.isNoFrame() && level->getTable()->count(own)
              ? own
              : level->begin()->first;
      TImageReaderP ir = lr->getFrameReader(fid);
      TRasterImageP ri = ir ? ir->load() : TImageP();
      TRasterP ras = ri ? ri->getRaster() : TRasterP();
      // A 16-bit PNG or TIFF comes as a 64-bit raster, which rasterToQImage
      // does not take: brought to 8 bits first. (A 16-bit PSD does not: the
      // PSD reader converts it already.)
      if (TRaster64P ras64 = ras) {
        TRaster32P ras32(ras64->getLx(), ras64->getLy());
        TRop::convert(ras32, ras64);
        ras = ras32;
      }
      // The PSD reader gives the alpha NOT premultiplied: Tahoma does it
      // afterwards, by the level format rule (preferences.cpp, «psd →
      // premultiply»). The same rule here — labelled premultiplied without
      // it, the half-transparent edges come out wrong in the PNG.
      const TFilePath fp(file.toStdWString());
      const Preferences *prefs = Preferences::instance();
      const int format         = prefs->matchLevelFormat(fp);
      TRaster32P ras32         = ras;
      if (ras32 && format >= 0 &&
          prefs->levelFormat(format).m_options.m_premultiply) {
        ras32 = ras32->clone();
        TRop::premultiply(ras32);
        ras = ras32;
      }
      // rasterToQImage ignores the wrap: a raster that has one is copied.
      if (ras32 && ras32->getWrap() != ras32->getLx()) ras = ras32->clone();
      if (ras) img = rasterToQImage(ras);  // premultiplied, bottom-up: as Toonz
    }
  } catch (...) {
  }
  if (img.isNull()) img.load(file);  // what Tahoma could not, Qt may
  if (img.isNull()) {
    // The PSD reader gives nothing for CMYK or 32-bit files; Qt reads no
    // PSD at all. Said, so the user knows what to change.
    if (why)
      *why = file.endsWith(".psd", Qt::CaseInsensitive)
                 ? QObject::tr("cannot read %1 (a CMYK or 32-bit PSD? save it "
                               "as RGB, 8 or 16 bit)")
                       .arg(QFileInfo(file).fileName())
                 : QObject::tr("cannot read %1").arg(QFileInfo(file).fileName());
    return false;
  }
  // Nothing but transparency: a picture of nothing, not a preview.
  bool anyVisible = false;
  const QImage probe = img.convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < probe.height() && !anyVisible; ++y) {
    const QRgb *line = reinterpret_cast<const QRgb *>(probe.constScanLine(y));
    for (int x = 0; x < probe.width(); ++x)
      if (qAlpha(line[x]) > 0) { anyVisible = true; break; }
  }
  if (!anyVisible) {
    if (why) *why = QObject::tr("%1 is empty").arg(QFileInfo(file).fileName());
    return false;
  }
  if (img.width() > 1920 || img.height() > 1920)
    img = img.scaled(1920, 1920, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  // On white: Kitsu shows transparency on its dark background, and a line
  // drawing there is black on black.
  QImage onWhite(img.size(), QImage::Format_RGB32);
  onWhite.fill(Qt::white);
  {
    QPainter painter(&onWhite);
    painter.drawImage(0, 0, img);
  }
  img = onWhite;
  if (!img.save(outPng, "PNG")) {
    if (why) *why = QObject::tr("cannot write the preview");
    return false;
  }
  return true;
}
