#include "ztoriglibrary.h"

#include "tapp.h"
#include "iocommand.h"
#include "ztorycharacter.h"
#include "ztorymodel.h"
#include "ztorymouthlibrary.h"  // characterScenesByName

#include "ext/plasticskeleton.h"
#include "ext/plasticskeletondeformation.h"
#include "toonz/childstack.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"
#include "toonz/txshcell.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcolumn.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "tsystem.h"
#include "tundo.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QLabel>
#include <QListWidget>
#include <QStyle>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace {

const char *kAskOnSaveKey = "Ztoryc/RigLibrary/askOnSave";
const char *kAskOnOpenKey = "Ztoryc/RigLibrary/askOnOpen";

// ── The library's data ───────────────────────────────────────────────────

struct PosePart {
  QString column;
  std::set<int> skelIds;
  std::map<QString, std::vector<double>> deltas;  // vertex name -> POSE_PARAMS
};

struct LibPose {
  QString name;
  int mode    = PoseAction::ADD;
  bool isBase = false;
  QVector<PosePart> parts;
};

struct LibCorrective {
  QString column;
  QString meshPrint;  // fingerprint of the mesh files it was sculpted on
  MeshCorrective c;
};

struct Library {
  QVector<LibPose> poses;
  QVector<LibCorrective> correctives;
};

TFilePath libraryFile(const TFilePath &charScene) {
  return charScene.withType("zrig");
}

// true when the library was read, or when there is none yet (a character with
// nothing published). false = the file is there and cannot be read or is
// damaged: whoever writes it back must not, or what it held is lost.
bool loadLibrary(const TFilePath &charScene, Library *lib) {
  QFile f(libraryFile(charScene).getQString());
  if (!f.exists()) return true;
  if (!f.open(QIODevice::ReadOnly)) return false;
  QXmlStreamReader rx(&f);
  LibPose *pose = nullptr;
  PosePart *part = nullptr;
  LibCorrective *corr = nullptr;
  while (!rx.atEnd()) {
    rx.readNext();
    if (!rx.isStartElement()) continue;
    const QXmlStreamAttributes a = rx.attributes();
    const QStringRef n = rx.name();
    if (n == QLatin1String("ztorig-library")) {
      // A newer format than this build knows: not read, so not overwritten.
      if (a.value("version").toInt() > 1) return false;
    } else if (n == QLatin1String("pose")) {
      lib->poses.push_back(LibPose());
      pose         = &lib->poses.last();
      pose->name   = a.value("name").toString();
      pose->mode   = qBound(int(PoseAction::ADD), a.value("mode").toInt(),
                            int(PoseAction::PART));
      pose->isBase = a.value("base") == QLatin1String("1");
      part         = nullptr;  // a <d> before this pose's first <part> is
      corr         = nullptr;  // ignored, not given to the previous pose
    } else if (n == QLatin1String("part") && pose) {
      pose->parts.push_back(PosePart());
      part         = &pose->parts.last();
      part->column = a.value("column").toString();
      for (const QString &s :
           a.value("skels").toString().split(' ', Qt::SkipEmptyParts))
        part->skelIds.insert(s.toInt());
    } else if (n == QLatin1String("d") && part && !corr) {
      std::vector<double> v;
      for (const QString &s :
           a.value("values").toString().split(' ', Qt::SkipEmptyParts))
        v.push_back(s.toDouble());
      // One value per pose param, as the .tnz loader makes it.
      v.resize(SkVD::POSE_PARAMS_COUNT, 0.0);
      part->deltas[a.value("v").toString()] = v;
    } else if (n == QLatin1String("corrective")) {
      lib->correctives.push_back(LibCorrective());
      corr                        = &lib->correctives.last();
      corr->column                = a.value("column").toString();
      corr->meshPrint             = a.value("mesh").toString();
      corr->c.m_name              = a.value("name").toString();
      corr->c.m_driverVertexName  = a.value("driver").toString();
      corr->c.m_restAngle         = a.value("rest").toDouble();
      corr->c.m_fullAngle         = a.value("full").toDouble();
      pose = nullptr;
      part = nullptr;
    } else if (n == QLatin1String("o") && corr) {
      corr->c.setDelta(a.value("m").toInt(), a.value("v").toInt(),
                       TPointD(a.value("x").toDouble(), a.value("y").toDouble()));
    }
  }
  return !rx.hasError();
}

bool saveLibrary(const TFilePath &charScene, const Library &lib,
                 QString *why) {
  const QString path = libraryFile(charScene).getQString();
  // Replaced in one step, and only when the whole file is written: removing
  // the old one first lost the WHOLE library if the rename then failed
  // (review, 2026-09-27).
  QSaveFile f(path);
  if (!f.open(QIODevice::WriteOnly)) {
    if (why) *why = QObject::tr("cannot write %1").arg(path);
    return false;
  }
  QXmlStreamWriter w(&f);
  w.setAutoFormatting(true);
  w.writeStartDocument();
  w.writeStartElement("ztorig-library");
  w.writeAttribute("version", "1");
  for (const LibPose &p : lib.poses) {
    w.writeStartElement("pose");
    w.writeAttribute("name", p.name);
    w.writeAttribute("mode", QString::number(p.mode));
    if (p.isBase) w.writeAttribute("base", "1");
    for (const PosePart &pp : p.parts) {
      w.writeStartElement("part");
      w.writeAttribute("column", pp.column);
      QStringList skels;
      for (int s : pp.skelIds) skels << QString::number(s);
      if (!skels.isEmpty()) w.writeAttribute("skels", skels.join(' '));
      for (const auto &d : pp.deltas) {
        w.writeStartElement("d");
        w.writeAttribute("v", d.first);
        QStringList vals;
        for (double x : d.second) vals << QString::number(x, 'g', 12);
        w.writeAttribute("values", vals.join(' '));
        w.writeEndElement();
      }
      w.writeEndElement();
    }
    w.writeEndElement();
  }
  for (const LibCorrective &lc : lib.correctives) {
    w.writeStartElement("corrective");
    w.writeAttribute("name", lc.c.m_name);
    w.writeAttribute("column", lc.column);
    w.writeAttribute("mesh", lc.meshPrint);
    w.writeAttribute("driver", lc.c.m_driverVertexName);
    w.writeAttribute("rest", QString::number(lc.c.m_restAngle, 'g', 12));
    w.writeAttribute("full", QString::number(lc.c.m_fullAngle, 'g', 12));
    for (const auto &m : lc.c.m_deltas)
      for (const auto &v : m.second) {
        w.writeStartElement("o");
        w.writeAttribute("m", QString::number(m.first));
        w.writeAttribute("v", QString::number(v.first));
        w.writeAttribute("x", QString::number(v.second.x, 'g', 12));
        w.writeAttribute("y", QString::number(v.second.y, 'g', 12));
        w.writeEndElement();
      }
    w.writeEndElement();
  }
  w.writeEndElement();
  w.writeEndDocument();
  if (!f.commit()) {
    if (why) *why = QObject::tr("cannot write %1").arg(path);
    return false;
  }
  return true;
}

// Two stored numbers are the same value when they differ by less than a
// thousandth, relative: the .tnz writes about six digits and the .zrig twelve,
// so a pose read back from the scene is the library's pose, rounded — and a
// strict comparison called «different» every pose that had made the round
// trip (Franco, 2026-09-27: «-36.5004 / -36.5004»).
bool sameNumber(double a, double b) {
  return std::abs(a - b) <= 1e-3 * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

// ── A character's parts in an xsheet ─────────────────────────────────────

struct Part {
  QString column;
  int col = -1;
  PlasticSkeletonDeformationP sd;
  QString meshPrint;
};

QString columnName(TXsheet *xsh, int col) {
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(col));
  QString n = obj ? QString::fromStdString(obj->getName()) : QString();
  return n.isEmpty() ? QString("Col%1").arg(col + 1) : n;
}

// The files of the mesh level a part stands on, as one fingerprint: a
// corrective is by mesh vertex INDEX, valid only on the same mesh.
QString meshPrintOf(ToonzScene *scene, TXsheet *xsh, int col) {
  int r0 = 0, r1 = -1;
  xsh->getCellRange(col, r0, r1);
  for (int r = r0; r <= r1; r++) {
    const TXshCell cell = xsh->getCell(r, col);
    TXshSimpleLevel *sl = cell.m_level ? cell.m_level->getSimpleLevel() : nullptr;
    if (!sl || sl->getType() != MESH_XSHLEVEL) continue;
    const TFilePath fp = scene->decodeFilePath(sl->getPath());
    const QFileInfo fi(fp.getQString());
    QStringList files;
    if (fi.isFile())
      files << fi.absoluteFilePath();
    else {
      const QDir dir(fi.absolutePath());
      const QString name = QString::fromStdWString(fp.getWideName());
      const QString ext  = QString::fromStdString(fp.getType());
      for (const QString &f :
           dir.entryList(QStringList() << name + ".*." + ext, QDir::Files,
                         QDir::Name))
        files << dir.absoluteFilePath(f);
    }
    QCryptographicHash h(QCryptographicHash::Md5);
    for (const QString &f : files) {
      QFile in(f);
      if (in.open(QIODevice::ReadOnly)) h.addData(&in);
    }
    return files.isEmpty() ? QString() : QString(h.result().toHex());
  }
  return QString();
}

QVector<Part> partsOf(ToonzScene *scene, TXsheet *xsh) {
  QVector<Part> out;
  if (!xsh) return out;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
    if (!obj) continue;
    const PlasticSkeletonDeformationP &sd = obj->getPlasticSkeletonDeformation();
    if (!sd) continue;
    Part p;
    p.column    = columnName(xsh, c);
    p.col       = c;
    p.sd        = sd;
    p.meshPrint = meshPrintOf(scene, xsh, c);
    out << p;
  }
  return out;
}

// The poses and correctives the parts hold, as library entries.
Library collect(const QVector<Part> &parts) {
  Library lib;
  QMap<QString, int> byName;
  for (const Part &p : parts) {
    for (int i = 0; i < p.sd->poseActionsCount(); i++) {
      const PoseAction *a = p.sd->poseAction(i);
      if (!a || a->m_name.isEmpty()) continue;
      if (!byName.contains(a->m_name)) {
        byName.insert(a->m_name, lib.poses.size());
        LibPose lp;
        lp.name   = a->m_name;
        lp.mode   = a->m_mode;
        lp.isBase = a->m_isBase;
        lib.poses << lp;
      }
      PosePart pp;
      pp.column  = p.column;
      pp.skelIds = a->m_skelIds;
      pp.deltas  = a->m_deltas;
      lib.poses[byName[a->m_name]].parts << pp;
    }
    for (int i = 0; i < p.sd->meshCorrectivesCount(); i++) {
      const MeshCorrective *mc = p.sd->meshCorrective(i);
      if (!mc) continue;
      LibCorrective lc;
      lc.column    = p.column;
      lc.meshPrint = p.meshPrint;
      lc.c         = *mc;
      lib.correctives << lc;
    }
  }
  return lib;
}

// Why `pose` does not fit these parts, or empty if it does: every column it
// names must be there, with every vertex its deltas name.
QString poseMisfit(const LibPose &pose, const QVector<Part> &parts) {
  for (const PosePart &pp : pose.parts) {
    const Part *target = nullptr;
    for (const Part &p : parts)
      if (p.column == pp.column) target = &p;
    if (!target)
      return QObject::tr("no column «%1»").arg(pp.column);
    for (const auto &d : pp.deltas)
      if (!target->sd->vertexDeformation(d.first))
        return QObject::tr("column «%1» has no vertex «%2»")
            .arg(pp.column, d.first);
  }
  return QString();
}

QString correctiveMisfit(const LibCorrective &lc, const QVector<Part> &parts) {
  for (const Part &p : parts) {
    if (p.column != lc.column) continue;
    if (p.meshPrint.isEmpty() || p.meshPrint != lc.meshPrint)
      return QObject::tr("column «%1»: the mesh is not the one it was "
                         "sculpted on")
          .arg(lc.column);
    if (!p.sd->vertexDeformation(lc.c.m_driverVertexName))
      return QObject::tr("column «%1» has no vertex «%2»")
          .arg(lc.column, lc.c.m_driverVertexName);
    return QString();
  }
  return QObject::tr("no column «%1»").arg(lc.column);
}

// ── Where the character is ───────────────────────────────────────────────

struct Context {
  QString character;   // asset name
  TFilePath libScene;  // the character's scene
  TXsheet *xsh = nullptr;
  bool isLibraryScene = false;  // working in the character's own scene
};

// The character an xsheet IS, recognised by its meshes — when no sub-scene
// is named as a character's scene.  sh370 (Franco, 2026-10-12): a nesting
// taken out of SOFIA's column removed the sub-scene «SOFIA», and her poses
// disappeared with it (the mouths had the same problem, fixed by their
// drawings in ZtoryMouthLibrary).  A part's mesh fingerprint (meshPrintOf:
// MD5 of the mesh files) is copied unchanged when the character is brought
// into a shot, so the xsheet holding parts with a library's prints is that
// character's own.  The library's prints are read once per version of its
// scene.
bool characterByMeshes(ToonzScene *scene, TXsheet *xsh,
                       const QHash<QString, QPair<QString, TFilePath>> &chars,
                       Context *ctx) {
  if (!scene || !xsh || chars.isEmpty()) return false;
  QSet<QString> here;
  for (const Part &p : partsOf(scene, xsh))
    if (!p.meshPrint.isEmpty()) here.insert(p.meshPrint);
  if (here.isEmpty()) return false;
  static QHash<QString, QSet<QString>> prints;  // scene file|mtime → prints
  for (auto it = chars.constBegin(); it != chars.constEnd(); ++it) {
    const QString file = it.value().second.getQString();
    const QString key =
        file + "|" +
        QString::number(QFileInfo(file).lastModified().toSecsSinceEpoch());
    if (!prints.contains(key)) {
      QSet<QString> set;
      try {
        ToonzScene holder;
        holder.loadTnzFile(it.value().second);
        std::function<void(TXsheet *, int, QSet<TXsheet *> &)> walk =
            [&](TXsheet *x, int depth, QSet<TXsheet *> &seen) {
              if (!x || depth > 8 || seen.contains(x)) return;
              seen.insert(x);
              for (const Part &p : partsOf(&holder, x))
                if (!p.meshPrint.isEmpty()) set.insert(p.meshPrint);
              for (int c = 0; c < x->getColumnCount(); c++) {
                int r0 = 0, r1 = -1;
                x->getCellRange(c, r0, r1);
                for (int r = r0; r <= r1; r++) {
                  const TXshCell cell = x->getCell(r, c);
                  if (cell.isEmpty() || !cell.m_level) continue;
                  if (TXshChildLevel *cl = cell.m_level->getChildLevel())
                    walk(cl->getXsheet(), depth + 1, seen);
                  break;
                }
              }
            };
        QSet<TXsheet *> seen;
        walk(holder.getXsheet(), 0, seen);
      } catch (...) {
        set.clear();
      }
      prints.insert(key, set);
    }
    const QSet<QString> &lib = prints.value(key);
    for (const QString &m : here)
      if (lib.contains(m)) {
        ctx->character = it.value().first;
        ctx->libScene  = it.value().second;
        ctx->xsh       = xsh;
        return true;
      }
  }
  return false;
}

// The character being worked on: the scene itself when it is a character's
// scene; in a shot, the sub-scene one is in — or the nearest above it —
// named as a character's scene (the name the import gives it).
bool currentContext(Context *ctx) {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return false;
  const QHash<QString, QPair<QString, TFilePath>> chars =
      ZtoryMouthLibrary::characterScenesByName();
  const TFilePath scenePath = scene->decodeFilePath(scene->getScenePath());
  if (ZtoryCharacter::roleOf(scenePath.getQString()) ==
      QLatin1String("character")) {
    auto it = chars.constFind(
        QString::fromStdWString(scenePath.getWideName()).toLower());
    ctx->character = it != chars.constEnd() ? it.value().first
                                            : QString::fromStdWString(
                                                  scenePath.getWideName());
    ctx->libScene       = scenePath;
    ctx->xsh            = scene->getXsheet();
    ctx->isLibraryScene = true;
    return true;
  }
  ChildStack *stack = scene->getChildStack();
  for (int i = stack->getAncestorCount() - 1; i >= 0; --i) {
    AncestorNode *node = stack->getAncestorInfo(i);
    if (!node || !node->m_cl) continue;
    auto it = chars.constFind(ZtoryMouthLibrary::characterKey(
        QString::fromStdWString(node->m_cl->getName())));
    if (it == chars.constEnd()) continue;
    ctx->character = it.value().first;
    ctx->libScene  = it.value().second;
    // The character's own xsheet: the child entered at depth i.
    ctx->xsh = node->m_cl->getXsheet();
    return true;
  }
  // No name to go by: the meshes of the xsheet one is in, then of the ones
  // above it.
  // Only from inside a sub-scene: at the top of a shot one is in no
  // character, and allContexts() must find every one of them.
  if (stack->getAncestorCount() > 0 &&
      characterByMeshes(scene, scene->getXsheet(), chars, ctx))
    return true;
  for (int i = stack->getAncestorCount() - 1; i >= 0; --i) {
    AncestorNode *node = stack->getAncestorInfo(i);
    if (node && node->m_cl &&
        characterByMeshes(scene, node->m_cl->getXsheet(), chars, ctx))
      return true;
  }
  return false;
}

// Every character in the scene: the one being worked in when inside one;
// otherwise each sub-scene of the tree named as a character's scene — at
// open, and at the top of a shot, one is inside none of them.
QVector<Context> allContexts() {
  QVector<Context> out;
  Context cur;
  if (currentContext(&cur)) {
    out << cur;
    return out;
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return out;
  const QHash<QString, QPair<QString, TFilePath>> chars =
      ZtoryMouthLibrary::characterScenesByName();
  if (chars.isEmpty()) return out;
  QSet<TXsheet *> seen;
  std::function<void(TXsheet *, int)> walk = [&](TXsheet *xsh, int depth) {
    if (!xsh || depth > 8 || seen.contains(xsh)) return;
    seen.insert(xsh);
    for (int c = 0; c < xsh->getColumnCount(); c++) {
      int r0 = 0, r1 = -1;
      xsh->getCellRange(c, r0, r1);
      QSet<TXshLevel *> done;
      for (int r = r0; r <= r1; r++) {
        const TXshCell cell = xsh->getCell(r, c);
        TXshChildLevel *cl = cell.m_level ? cell.m_level->getChildLevel() : nullptr;
        if (!cl || done.contains(cell.m_level.getPointer())) continue;
        done.insert(cell.m_level.getPointer());
        auto it = chars.constFind(ZtoryMouthLibrary::characterKey(
            QString::fromStdWString(cell.m_level->getName())));
        if (it != chars.constEnd()) {
          Context ctx;
          ctx.character = it.value().first;
          ctx.libScene  = it.value().second;
          ctx.xsh       = cl->getXsheet();
          out << ctx;
          continue;  // its insides are the character's, not another one
        }
        Context byMesh;
        if (characterByMeshes(scene, cl->getXsheet(), chars, &byMesh)) {
          out << byMesh;
          continue;
        }
        walk(cl->getXsheet(), depth + 1);
      }
    }
  };
  walk(scene->getXsheet(), 0);
  return out;
}

// The character's scene, read in the background: its parts are what a pose
// must fit to enter the library.
QVector<Part> libraryParts(const TFilePath &libScene, ToonzScene *holder) {
  try {
    holder->loadTnzFile(libScene);
    return partsOf(holder, holder->getXsheet());
  } catch (...) {
    return QVector<Part>();
  }
}

// ── The dialog, both ways ────────────────────────────────────────────────

struct Offer {
  enum Kind { Pose, Corrective } kind;
  int index;       // in the source Library
  bool changed;    // same name, different
  QString misfit;  // why it does not fit, or empty
  QString how;     // for a changed one: WHAT differs, said to the user
};

// What differs between two correctives of the same name and column.
QString correctiveDifference(const LibCorrective &a, const LibCorrective &b) {
  if (a.meshPrint != b.meshPrint) return QObject::tr("mesh");
  if (a.c.m_driverVertexName != b.c.m_driverVertexName)
    return QObject::tr("driver vertex");
  if (!sameNumber(a.c.m_restAngle, b.c.m_restAngle) ||
      !sameNumber(a.c.m_fullAngle, b.c.m_fullAngle))
    return QObject::tr("angles");
  auto count = [](const MeshCorrective &c) {
    size_t n = 0;
    for (const auto &m : c.m_deltas) n += m.second.size();
    return n;
  };
  if (count(a.c) != count(b.c)) return QObject::tr("sculpted vertices");
  for (const auto &m : a.c.m_deltas)
    for (const auto &v : m.second) {
      const TPointD o = b.c.delta(m.first, v.first);
      if (!sameNumber(v.second.x, o.x) || !sameNumber(v.second.y, o.y))
        return QObject::tr("sculpt of vertex %1").arg(v.first);
    }
  return QString();
}

// What differs between two poses of the same name — said in the list, so a
// «differs» the user does not recognise can be checked instead of trusted.
QString poseDifference(const LibPose &a, const LibPose &b) {
  if (a.mode != b.mode) {
    static const char *kModes[] = {"ADD", "POSE", "PART"};
    auto name = [](int m) {
      return m >= 0 && m < 3 ? QString(kModes[m]) : QString::number(m);
    };
    // Neutral wording: the same function serves publish and update.
    return QObject::tr("mode: %1 offered / %2 now")
        .arg(name(a.mode), name(b.mode));
  }
  if (a.isBase != b.isBase) return QObject::tr("base pose");
  QMap<QString, const PosePart *> pa, pb;
  for (const PosePart &p : a.parts) pa[p.column] = &p;
  for (const PosePart &p : b.parts) pb[p.column] = &p;
  if (pa.keys() != pb.keys())
    return QObject::tr("columns (%1 / %2)")
        .arg(QStringList(pa.keys()).join(","), QStringList(pb.keys()).join(","));
  for (auto it = pa.constBegin(); it != pa.constEnd(); ++it) {
    const PosePart *x = it.value(), *y = pb[it.key()];
    if (x->skelIds != y->skelIds) return QObject::tr("skeletons of %1").arg(it.key());
    if (x->deltas.size() != y->deltas.size())
      return QObject::tr("vertices of %1 (%2 / %3)")
          .arg(it.key())
          .arg(x->deltas.size())
          .arg(y->deltas.size());
    for (const auto &d : x->deltas) {
      auto o = y->deltas.find(d.first);
      if (o == y->deltas.end()) return QObject::tr("vertex «%1»").arg(d.first);
      for (size_t i = 0; i < d.second.size() && i < o->second.size(); i++)
        if (!sameNumber(d.second[i], o->second[i]))
          return QObject::tr("vertex «%1»: %2 / %3")
              .arg(d.first)
              .arg(d.second[i])
              .arg(o->second[i]);
    }
  }
  return QString();
}

bool choose(QWidget *parent, const QString &title, const QString &text,
            const QString &character, const Library &src,
            QVector<Offer> *offers, const char *askKey,
            const QString &dontAskText) {
  QDialog dlg(parent);
  dlg.setWindowTitle(title);
  dlg.setMinimumWidth(500);
  auto *lay  = new QVBoxLayout(&dlg);
  auto *head = new QLabel(text, &dlg);
  head->setWordWrap(true);
  lay->addWidget(head);
  auto *list = new QListWidget(&dlg);
  for (const Offer &o : *offers) {
    const QString name = o.kind == Offer::Pose ? src.poses[o.index].name
                                               : src.correctives[o.index].c.m_name;
    const QString kind = o.kind == Offer::Pose ? QObject::tr("pose")
                                               : QObject::tr("corrective");
    QString what = o.changed
                       ? (o.how.isEmpty()
                              ? QObject::tr("differs — replaces it")
                              : QObject::tr("differs (%1) — replaces it")
                                    .arg(o.how))
                       : QObject::tr("new");
    if (!o.misfit.isEmpty())
      what = QObject::tr("does not fit %1: %2").arg(character, o.misfit);
    auto *w = new QListWidgetItem(
        QObject::tr("%1 «%2» — %3").arg(kind, name, what), list);
  // No emoji in a list row: Qt 5 cannot draw «⚠️» there, and the WHOLE row
  // came out as one empty box (Franco, 2026-11-27). The warning is the row's
  // icon instead.
    if (!o.misfit.isEmpty())
      w->setIcon(dlg.style()->standardIcon(QStyle::SP_MessageBoxWarning));
    w->setFlags(w->flags() | Qt::ItemIsUserCheckable);
    w->setCheckState(!o.changed && o.misfit.isEmpty() ? Qt::Checked
                                                      : Qt::Unchecked);
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
  QVector<Offer> kept;
  for (int i = 0; i < offers->size(); i++)
    if (list->item(i)->checkState() == Qt::Checked) kept << (*offers)[i];
  *offers = kept;
  return true;
}

// What `src` has that `dst` lacks or has differently.
QVector<Offer> differences(const Library &src, const Library &dst) {
  QVector<Offer> out;
  for (int i = 0; i < src.poses.size(); i++) {
    int j = -1;
    for (int k = 0; k < dst.poses.size(); k++)
      if (dst.poses[k].name == src.poses[i].name) j = k;
    if (j < 0)
      out << Offer{Offer::Pose, i, false, QString()};
    else {
      const QString how = poseDifference(src.poses[i], dst.poses[j]);
      if (!how.isEmpty()) out << Offer{Offer::Pose, i, true, QString(), how};
    }
  }
  for (int i = 0; i < src.correctives.size(); i++) {
    int j = -1;
    for (int k = 0; k < dst.correctives.size(); k++)
      if (dst.correctives[k].c.m_name == src.correctives[i].c.m_name &&
          dst.correctives[k].column == src.correctives[i].column)
        j = k;
    if (j < 0)
      out << Offer{Offer::Corrective, i, false, QString()};
    else {
      const QString how =
          correctiveDifference(src.correctives[i], dst.correctives[j]);
      if (!how.isEmpty())
        out << Offer{Offer::Corrective, i, true, QString(), how};
    }
  }
  return out;
}

// ── Undo of an update ────────────────────────────────────────────────────

class RigLibraryUndo final : public TUndo {
  struct State {
    PlasticSkeletonDeformationP sd;
    std::vector<PoseAction> poses;
    std::vector<MeshCorrective> correctives;
  };
  std::vector<State> m_before, m_after;

public:
  void capture(const QVector<Part> &parts, bool after) {
    std::vector<State> &v = after ? m_after : m_before;
    v.clear();
    for (const Part &p : parts)
      v.push_back({p.sd, p.sd->getPoseActions(), p.sd->getMeshCorrectives()});
  }
  static void put(const std::vector<State> &v) {
    for (const State &s : v) {
      s.sd->setPoseActions(s.poses);
      s.sd->setMeshCorrectives(s.correctives);
    }
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }
  void undo() const override { put(m_before); }
  void redo() const override { put(m_after); }
  int getSize() const override { return sizeof(*this) + 1024; }
  QString getHistoryString() override {
    return QObject::tr("ZtoRig: Update from Library");
  }
};

// Writes a library pose onto the parts (created, or replaced by name).
void applyPose(const LibPose &lp, const QVector<Part> &parts) {
  for (const PosePart &pp : lp.parts) {
    for (const Part &p : parts) {
      if (p.column != pp.column) continue;
      PoseAction *a = p.sd->poseAction(lp.name);
      int idx       = -1;
      if (!a) {
        idx = p.sd->addPoseAction(lp.name);
        a   = p.sd->poseAction(idx);
      } else {
        for (int i = 0; i < p.sd->poseActionsCount(); i++)
          if (p.sd->poseAction(i) == a) idx = i;
      }
      if (!a) continue;
      a->m_mode    = lp.mode;
      a->m_skelIds = pp.skelIds;
      a->m_deltas  = pp.deltas;
      // Both ways: a pose that is no longer the base in the library stops
      // being it here too.
      if (idx >= 0 && a->m_isBase != lp.isBase)
        p.sd->setPoseActionAsBase(idx, lp.isBase);
    }
  }
}

void applyCorrective(const LibCorrective &lc, const QVector<Part> &parts) {
  for (const Part &p : parts) {
    if (p.column != lc.column) continue;
    MeshCorrective *mc = p.sd->meshCorrective(lc.c.m_name);
    if (!mc) mc = p.sd->meshCorrective(p.sd->addMeshCorrective(lc.c.m_name));
    if (mc) *mc = lc.c;
  }
}

void merge(Library *dst, const Library &src, const QVector<Offer> &chosen) {
  for (const Offer &o : chosen) {
    if (o.kind == Offer::Pose) {
      const LibPose &p = src.poses[o.index];
      bool replaced    = false;
      for (LibPose &d : dst->poses)
        if (d.name == p.name) { d = p; replaced = true; }
      if (!replaced) dst->poses << p;
    } else {
      const LibCorrective &c = src.correctives[o.index];
      bool replaced          = false;
      for (LibCorrective &d : dst->correctives)
        if (d.c.m_name == c.c.m_name && d.column == c.column) {
          d        = c;
          replaced = true;
        }
      if (!replaced) dst->correctives << c;
    }
  }
}

// A v2 of the character from the copy being worked on: this sub-scene saved
// as a scene of its own beside the original, with its asset in the tracker.
// Its library starts with what is being published.
bool createVersion2(const Context &ctx, TFilePath *newScene, QString *why) {
  const QString base = QString::fromStdWString(ctx.libScene.getWideName());
  QString name       = base + "_v2";
  for (int n = 3; QFileInfo::exists(
           ctx.libScene.getParentDir().getQString() + "/" + name + ".tnz");
       n++)
    name = base + "_v" + QString::number(n);
  bool ok = false;
  name    = QInputDialog::getText(
      ZtoryMouthLibrary::dialogParent(),
      QObject::tr("New version of %1").arg(ctx.character),
      QObject::tr("Name of the new character scene:"), QLineEdit::Normal, name,
      &ok);
  // A file name, not a path: same cleaning as the clip names.
  name.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
  if (!ok || name.trimmed().isEmpty()) return false;
  *newScene = ctx.libScene.getParentDir() +
              TFilePath((name.trimmed() + ".tnz").toStdWString());
  if (QFileInfo::exists(newScene->getQString())) {
    if (why) *why = QObject::tr("%1 exists already").arg(newScene->getQString());
    return false;
  }
  // The new version is the character's copy: SAVE_SUBXSHEET saves the CURRENT
  // xsheet, so the copy is made current for the save and the previous one put
  // back after. It used to ask to open the sub-scene first, and at save time
  // one is in the shot's main xsheet: «Create a new version» always failed
  // there (Franco, 2026-11-27).
  // setXsheet EMITS xsheetSwitched: blocked, or Board, animatic and viewers
  // would take the character for the current xsheet with the shot's column
  // and frame indices (review 2026-11-27). saveScene also writes the shot's
  // play range into the current xsheet's markers and clears the dirty flag:
  // both put back after.
  if (!ctx.xsh) {
    if (why) *why = QObject::tr("the character's sub-scene was not found");
    return false;
  }
  TSceneHandle *sh  = TApp::instance()->getCurrentScene();
  TXsheetHandle *xh = TApp::instance()->getCurrentXsheet();
  TXsheet *was      = xh->getXsheet();
  const bool dirty  = sh->getDirtyFlag();
  int markIn = 0, markOut = -1;
  ctx.xsh->getInOutMarkers(markIn, markOut);
  bool saved = false;
  {
    QSignalBlocker block(xh);
    xh->setXsheet(ctx.xsh);
    saved = IoCmd::saveScene(*newScene, IoCmd::SAVE_SUBXSHEET);
    xh->setXsheet(was);
  }
  ctx.xsh->setInOutMarkers(markIn, markOut);
  sh->setDirtyFlag(dirty);
  if (!saved) {
    if (why) *why = QObject::tr("the new scene could not be saved");
    return false;
  }
  ZtoryCharacter::setRole(newScene->getQString(), "character");
  ZtoryModel *m = ZtoryModel::instance();
  QString type;
  for (const Asset &a : m->assets())
    if (a.name == ctx.character) type = a.type;
  if (!type.isEmpty()) {
    // In the tracker, the character's name with the version: «KIKO_v2», not
    // the file's «lib_kiko_v2» (sh320, Franco 2026-11-27).
    QString assetName = name.trimmed();
    if (assetName.startsWith(base, Qt::CaseInsensitive) &&
        assetName.size() > base.size())
      assetName = ctx.character + assetName.mid(base.size());
    m->addAsset(type, assetName);  // appended at the end
    m->setAssetFilePath(int(m->assets().size()) - 1, newScene->getQString());
    m->saveProjectDb();
  }
  // The shot's copy now IS the new version: a shot knows its character by
  // the sub-scene's name (characterScenesByName: the scene file's name), and
  // with the old name it kept comparing with v1 and offering a new version at
  // every save (Franco, 2026-11-27). The scene is left modified: the new name
  // is kept at the next save.
  if (!ctx.isLibraryScene) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    TLevelSet *levels = scene ? scene->getLevelSet() : nullptr;
    const std::wstring newName = newScene->getWideName();
    for (int i = 0; levels && i < levels->getLevelCount(); i++) {
      TXshChildLevel *cl = levels->getLevel(i)->getChildLevel();
      if (!cl || cl->getXsheet() != ctx.xsh) continue;
      if (levels->renameLevel(cl, newName)) {
        TApp::instance()->getCurrentScene()->setDirtyFlag(true);
        TApp::instance()->getCurrentScene()->notifyCastChange();
      }
      break;
    }
  }
  return true;
}

}  // namespace

//----------------------------------------------------------------------------

// One character: returns false when there was nothing to offer.
// The library as it really is: what the character's own scene holds — its
// poses are the character's to begin with — plus what was published in the
// .zrig, which wins on a name (it is the newer). Without the scene's own, the
// first publish offered every pose of the character as «new» (Franco,
// 2026-09-27: «l'unica posa nuova dovrebbe essere saluti»).
static Library effectiveLibrary(const Context &ctx, ToonzScene *holder,
                                QVector<Part> *libParts, Library *published,
                                bool *readable = nullptr) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  const bool ok     = loadLibrary(ctx.libScene, published);
  if (readable) *readable = ok;
  *libParts = ctx.isLibraryScene ? partsOf(scene, ctx.xsh)
                                 : libraryParts(ctx.libScene, holder);
  Library eff = collect(*libParts);
  QVector<Offer> all;
  for (int i = 0; i < published->poses.size(); i++)
    all << Offer{Offer::Pose, i, false, QString()};
  for (int i = 0; i < published->correctives.size(); i++)
    all << Offer{Offer::Corrective, i, false, QString()};
  merge(&eff, *published, all);
  return eff;
}

static bool publishFor(const Context &ctx, QWidget *parent, bool onSave) {
  const QString title = QObject::tr("Publish poses to the library");
  // The character's own scene IS the library: nothing to publish from it.
  if (ctx.isLibraryScene) return false;
  ToonzScene *scene   = TApp::instance()->getCurrentScene()->getScene();
  const Library here  = collect(partsOf(scene, ctx.xsh));
  ToonzScene holder;
  QVector<Part> libParts;
  Library lib;  // the .zrig: what is written back
  bool readable = true;
  const Library eff =
      effectiveLibrary(ctx, &holder, &libParts, &lib, &readable);
  if (!readable) {
    // Writing back what could be read would lose the rest (a truncated file,
    // a Drive placeholder).
    QMessageBox::warning(
        parent, title,
        QObject::tr("The library of %1 cannot be read (%2): nothing is "
                    "published, so that it is not overwritten.")
            .arg(ctx.character)
            .arg(libraryFile(ctx.libScene).getQString()));
    return true;
  }
  QVector<Offer> offers = differences(here, eff);
  if (offers.isEmpty()) return false;
  for (Offer &o : offers)
    o.misfit = o.kind == Offer::Pose
                   ? poseMisfit(here.poses[o.index], libParts)
                   : correctiveMisfit(here.correctives[o.index], libParts);
  if (!choose(parent, title,
              QObject::tr("Poses and correctives of %1 in this scene that its "
                          "library does not have. Published, every scene "
                          "with %1 can take them.")
                  .arg(ctx.character),
              ctx.character, here, &offers, onSave ? kAskOnSaveKey : nullptr,
              QObject::tr("Don't ask again when saving (the Publish button in "
                          "ZtoRig stays)")))
    return true;
  if (offers.isEmpty()) return true;

  // A pose that does not fit is REFUSED (Franco, 2026-08-16); the user
  // decides: a v2 of the character, or discard.
  QStringList misfits;
  for (const Offer &o : offers)
    if (!o.misfit.isEmpty())
      misfits << (o.kind == Offer::Pose ? here.poses[o.index].name
                                        : here.correctives[o.index].c.m_name) +
                     ": " + o.misfit;
  TFilePath target = ctx.libScene;
  if (!misfits.isEmpty()) {
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Warning);
    box.setText(QObject::tr("Some of what you chose does not fit %1's "
                            "structure, so it cannot enter its library:\n\n"
                            "%2\n\nCreate a new version of the character from "
                            "this copy, with its own library?")
                    .arg(ctx.character, misfits.join("\n")));
    QPushButton *v2 = box.addButton(QObject::tr("Create a new version"),
                                    QMessageBox::AcceptRole);
    QPushButton *fit =
        box.addButton(QObject::tr("Publish only what fits"),
                      QMessageBox::DestructiveRole);
    box.addButton(QObject::tr("Discard"), QMessageBox::RejectRole);
    box.exec();
    if (box.clickedButton() == v2) {
      QString why;
      if (!createVersion2(ctx, &target, &why)) {
        if (!why.isEmpty()) QMessageBox::warning(parent, title, why);
        return true;
      }
      lib = Library();  // the new version's library starts here
    } else if (box.clickedButton() == fit) {
      QVector<Offer> fitting;
      for (const Offer &o : offers)
        if (o.misfit.isEmpty()) fitting << o;
      offers = fitting;
      if (offers.isEmpty()) return true;
    } else
      return true;
  }
  merge(&lib, here, offers);
  QString why;
  if (!saveLibrary(target, lib, &why)) {
    QMessageBox::warning(parent, title, why);
    return true;
  }
  QMessageBox::information(
      parent, title,
      QObject::tr("%1 item(s) published in %2.")
              .arg(offers.size())
              .arg(libraryFile(target).getQString()) +
          (target != ctx.libScene
               ? QObject::tr("\n\nThis shot now uses %1: save the scene to "
                             "keep it.")
                     .arg(QString::fromStdWString(target.getWideName()))
               : QString()));
  return true;
}

void ZtoRigLibrary::showPublishDialog(QWidget *parent, bool onSave) {
  const QVector<Context> ctxs = allContexts();
  bool any = false;
  for (const Context &ctx : ctxs) any |= publishFor(ctx, parent, onSave);
  if (!any && !onSave && ctxs.size() == 1 && ctxs.first().isLibraryScene) {
    QMessageBox::information(
        parent, QObject::tr("Publish poses to the library"),
        QObject::tr("This is %1's own scene: it IS the library. Its poses "
                    "reach the shots with «Update from Library».")
            .arg(ctxs.first().character));
    return;
  }
  if (!any && !onSave)
    QMessageBox::information(
        parent, QObject::tr("Publish poses to the library"),
        ctxs.isEmpty()
            ? QObject::tr("No character here. Open the character's scene, or "
                          "a shot with the character in it.")
            : QObject::tr("Nothing to publish: the libraries have every pose "
                          "and corrective of this scene."));
}

static bool updateFor(const Context &ctx, QWidget *parent, bool onOpen,
                      bool ask = true) {
  const QString title = QObject::tr("Update poses from the library");
  ToonzScene holder;
  QVector<Part> libParts;
  Library published;
  const Library lib = effectiveLibrary(ctx, &holder, &libParts, &published);
  if (lib.poses.isEmpty() && lib.correctives.isEmpty()) return false;
  ToonzScene *scene           = TApp::instance()->getCurrentScene()->getScene();
  const QVector<Part> parts   = partsOf(scene, ctx.xsh);
  const Library here          = collect(parts);
  QVector<Offer> offers       = differences(lib, here);
  // What does not fit this copy is not offered: nothing to choose there.
  QVector<Offer> fitting;
  for (Offer &o : offers) {
    o.misfit = o.kind == Offer::Pose
                   ? poseMisfit(lib.poses[o.index], parts)
                   : correctiveMisfit(lib.correctives[o.index], parts);
    if (o.misfit.isEmpty()) fitting << o;
  }
  if (fitting.isEmpty()) return false;

  // The character's OWN scene takes the library's NEW poses by itself: it is
  // the character's home and should already have them — publishing writes
  // the .zrig, not the closed .tnz (Franco, 2026-09-27: «sto aprendo la scena
  // originale di Sofia per cui pensavo ci fosse già»). Only a pose that
  // DIFFERS from the scene's own is asked about: there the user chooses.
  //
  // At OPENING the same holds for every shot (Franco, 2026-11-26: «è
  // importante che il personaggio si aggiorni durante la produzione e venga
  // aggiornato in apertura scena»): new poses and correctives come in by
  // themselves, with a notice; one that differs from the shot's stays a
  // question — it may be a variant made on purpose.
  QVector<Offer> automatic;
  if (ctx.isLibraryScene || onOpen) {
    QVector<Offer> asked;
    for (const Offer &o : fitting) (o.changed ? asked : automatic) << o;
    fitting = asked;
  }
  if (!ask) fitting.clear();  // only the automatic part, no question
  if (!fitting.isEmpty() &&
      !choose(parent, title,
              ctx.isLibraryScene
                  ? QObject::tr("These poses of %1 were published from a shot "
                                "and differ from the ones in this scene. "
                                "Replace the scene's with them?")
                        .arg(ctx.character)
                  : QObject::tr("%1's library has new poses or correctives: "
                                "bring them into this shot? One that differs "
                                "from this shot's may be a variant made on "
                                "purpose here: it is not ticked.")
                        .arg(ctx.character),
              ctx.character, lib, &fitting, onOpen ? kAskOnOpenKey : nullptr,
              QObject::tr("Don't ask again when opening a scene (the Update "
                          "button in ZtoRig stays)")))
    fitting.clear();
  fitting += automatic;
  if (fitting.isEmpty()) return true;
  auto *undo = new RigLibraryUndo();
  undo->capture(parts, false);
  for (const Offer &o : fitting) {
    if (o.kind == Offer::Pose)
      applyPose(lib.poses[o.index], parts);
    else
      applyCorrective(lib.correctives[o.index], parts);
  }
  undo->capture(parts, true);
  TUndoManager::manager()->add(undo);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  // Said, not silent: what came in from the library, and how to take it back.
  if (onOpen && !automatic.isEmpty() && !ctx.isLibraryScene) {
    QStringList names;
    for (const Offer &o : automatic)
      names << (o.kind == Offer::Pose ? lib.poses[o.index].name
                                      : lib.correctives[o.index].c.m_name);
    QMessageBox::information(
        parent, title,
        QObject::tr("%1's library had new poses or correctives: they are now "
                    "in this shot (⌘Z takes them back).\n\n%2")
            .arg(ctx.character, names.join("\n")));
  }
  return true;
}

void ZtoRigLibrary::showUpdateDialog(QWidget *parent, bool onOpen) {
  const QVector<Context> ctxs = allContexts();
  bool any = false;
  for (const Context &ctx : ctxs) any |= updateFor(ctx, parent, onOpen);
  if (!any && !onOpen)
    QMessageBox::information(
        parent, QObject::tr("Update poses from the library"),
        ctxs.isEmpty()
            ? QObject::tr("No character here. Open the character's scene, or "
                          "a shot with the character in it.")
            : QObject::tr("Nothing to update: this scene has everything of "
                          "its characters' libraries that fits it."));
}

bool ZtoRigLibrary::currentCharacter(QString *name, TFilePath *scene,
                                     TXsheet **xsh) {
  Context ctx;
  if (!currentContext(&ctx)) return false;
  if (name) *name = ctx.character;
  if (scene) *scene = ctx.libScene;
  if (xsh) *xsh = ctx.xsh;
  return true;
}

void ZtoRigLibrary::offerOnSave() {
  if (!QSettings().value(kAskOnSaveKey, true).toBool()) return;
  ZtoryMouthLibrary::runWhenNoModal([]() {
    for (const Context &ctx : allContexts())
      if (!ctx.isLibraryScene)
        publishFor(ctx, ZtoryMouthLibrary::dialogParent(), true);
  });
}

// A newer version of a character this scene uses: «KIKO_v2» beside «KIKO»
// (createVersion2 names them so). Franco, 2026-11-27: KIKO has no meshes, its
// v2 has; opening the layout he wants the v2 in. The v2 scene is loaded as a
// sub-scene and put in the old one's cells (same frames, same column, so its
// position stays), or in a column of its own beside it; with «Discard» the old
// sub-scene leaves the cast.
static int versionOf(const QString &name, const QString &base) {
  if (name.compare(base, Qt::CaseInsensitive) == 0) return 1;
  static const QRegularExpression kVer(QStringLiteral("_v(\\d+)$"),
                                       QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch mt = kVer.match(name);
  if (!mt.hasMatch() ||
      name.left(mt.capturedStart()).compare(base, Qt::CaseInsensitive) != 0)
    return 0;
  return mt.captured(1).toInt();
}

static QString versionBase(const QString &name) {
  QString b = name;
  b.remove(QRegularExpression(QStringLiteral("_v\\d+$"),
                              QRegularExpression::CaseInsensitiveOption));
  return b;
}

// Every place the old sub-scene is exposed, down the tree from the top.
struct ChildUse {
  TXsheet *xsh;
  int col;
};
static QVector<ChildUse> usesOf(TXsheet *top, TXshLevel *level) {
  QVector<ChildUse> out;
  QSet<TXsheet *> seen;
  std::function<void(TXsheet *, int)> walk = [&](TXsheet *x, int depth) {
    if (!x || depth > 8 || seen.contains(x)) return;
    seen.insert(x);
    for (int c = 0; c < x->getColumnCount(); c++) {
      int r0 = 0, r1 = -1;
      x->getCellRange(c, r0, r1);
      bool used = false;
      QSet<TXshLevel *> inside;
      for (int r = r0; r <= r1; r++) {
        const TXshCell cell = x->getCell(r, c);
        if (cell.isEmpty() || !cell.m_level) continue;
        if (cell.m_level.getPointer() == level) used = true;
        else if (TXshChildLevel *cl = cell.m_level->getChildLevel())
          inside.insert(cl);
      }
      if (used) out << ChildUse{x, c};
      for (TXshLevel *l : inside) walk(l->getChildLevel()->getXsheet(), depth + 1);
    }
  };
  walk(top, 0);
  return out;
}

static TXshLevel *childLevelOf(ToonzScene *scene, TXsheet *xsh) {
  TLevelSet *levels = scene->getLevelSet();
  for (int i = 0; i < levels->getLevelCount(); i++)
    if (TXshChildLevel *cl = levels->getLevel(i)->getChildLevel())
      if (cl->getXsheet() == xsh) return cl;
  return nullptr;
}

// Returns true when the scene was changed.
static bool offerNewerVersions(QWidget *parent) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->isUntitled()) return false;
  // The loading works on the CURRENT xsheet: only at the top of the scene.
  if (scene->getChildStack()->getAncestorCount() > 0) return false;
  const QVector<Context> ctxs = allContexts();
  if (ctxs.isEmpty() || ctxs.first().isLibraryScene) return false;
  // By the scene FILES' names, as a shot knows its characters: the asset is
  // «KIKO», its scene lib_kiko.tnz, the new version lib_kiko_v2.tnz (sh320,
  // Franco 2026-11-27: by the assets' names the v2 was never found).
  auto fileKey = [](const TFilePath &f) {
    return QString::fromStdWString(f.getWideName()).toLower();
  };
  QSet<QString> present;
  for (const Context &c : ctxs) present.insert(fileKey(c.libScene));
  const QHash<QString, QPair<QString, TFilePath>> chars =
      ZtoryMouthLibrary::characterScenesByName();
  bool changed = false;
  QSet<QString> asked;
  for (const Context &ctx : ctxs) {
    const QString base = versionBase(fileKey(ctx.libScene));
    if (asked.contains(base)) continue;
    asked.insert(base);
    const int mine = versionOf(fileKey(ctx.libScene), base);
    QString newest;
    TFilePath newestScene;
    int best = mine;
    for (auto it = chars.constBegin(); it != chars.constEnd(); ++it) {
      const int v = versionOf(it.key(), base);
      if (v > best && QFileInfo::exists(it.value().second.getQString())) {
        best        = v;
        newest      = it.value().first;
        newestScene = it.value().second;
      }
    }
    if (newest.isEmpty() || present.contains(fileKey(newestScene))) continue;
    TXshLevel *oldLevel = childLevelOf(scene, ctx.xsh);
    if (!oldLevel) continue;

    const QString title = QObject::tr("Newer version of %1").arg(ctx.character);
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr("This scene uses %1, and %2 is newer.\n\nLoad %2 "
                            "in its place?")
                    .arg(ctx.character, newest));
    QPushButton *replace = box.addButton(
        QObject::tr("Load %1, discard previous version").arg(newest),
        QMessageBox::AcceptRole);
    QPushButton *both = box.addButton(
        QObject::tr("Load %1, keep both").arg(newest), QMessageBox::ActionRole);
    box.addButton(QObject::tr("Not now"), QMessageBox::RejectRole);
    // Enter keeps both: discarding has no undo (Franco, 2026-11-27).
    box.setDefaultButton(both);
    box.exec();
    const bool discard = box.clickedButton() == replace;
    if (!discard && box.clickedButton() != both) continue;
    // The dialog runs its own event loop: the scene may have changed meanwhile.
    if (TApp::instance()->getCurrentScene()->getScene() != scene ||
        childLevelOf(scene, ctx.xsh) != oldLevel)
      break;

    TXsheet *top     = scene->getXsheet();
    const int before = top->getColumnCount();
    IoCmd::LoadResourceArguments args;
    args.resourceDatas.push_back(newestScene);
    args.importPolicy = IoCmd::LoadResourceArguments::LOAD;
    args.expose       = true;
    args.silent       = true;
    args.row0         = 0;
    args.col0         = before;
    IoCmd::loadResources(args, /*updateRecentFile=*/false);
    TXshLevel *newLevel = nullptr;
    for (TXshLevel *lv : args.loadedLevels)
      if (lv && lv->getChildLevel()) newLevel = lv;
    // The column the load made is only the way in: the new sub-scene goes
    // where the old one was.
    for (int c = top->getColumnCount() - 1; c >= before; c--)
      top->removeColumn(c);
    top->updateFrameCount();  // removeColumn does not: the load lengthened it
    if (!newLevel) {
      QMessageBox::warning(parent, title,
                           QObject::tr("%1 could not be loaded (%2).")
                               .arg(newest, newestScene.getQString()));
      continue;
    }
    // From the last: a column inserted beside one must not shift the next.
    const QVector<ChildUse> uses = usesOf(top, oldLevel);
    for (int i = uses.size() - 1; i >= 0; --i) {
      const ChildUse &u = uses[i];
      int r0 = 0, r1 = -1;
      u.xsh->getCellRange(u.col, r0, r1);
      int dst = u.col;
      if (!discard) {
        dst = u.col + 1;
        u.xsh->insertColumn(dst);
      }
      for (int r = r0; r <= r1; r++) {
        const TXshCell cell = u.xsh->getCell(r, u.col);
        if (cell.m_level.getPointer() != oldLevel) continue;
        u.xsh->setCell(r, dst, TXshCell(newLevel, cell.m_frameId));
      }
    }
    if (discard && usesOf(top, oldLevel).isEmpty()) {
      // Out of the cast, but not deleted: the level handle, a queued icon or
      // a Context may still point at it — a small leak instead of a dangling
      // pointer (review 2026-11-27).
      if (TApp::instance()->getCurrentLevel()->getLevel() == oldLevel)
        TApp::instance()->getCurrentLevel()->setLevel(nullptr);
      scene->getLevelSet()->removeLevel(oldLevel, false);
    }
    present.insert(fileKey(newestScene));
    changed = true;
  }
  if (changed) {
    // Done by hand after the load's own undo: undoing half of it would leave
    // the cells pointing at a level the undo took away.
    TUndoManager::manager()->reset();
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentScene()->notifyCastChange();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  return changed;
}

void ZtoRigLibrary::offerOnOpen() {
  // «Don't ask again» turns off the QUESTIONS, not the character's own scene
  // taking the library's new poses by itself (Franco, 2026-09-27: «separa»).
  // Since 2026-11-26 the new items come in by themselves in every scene, so
  // the update runs always; the setting only silences the question about
  // items that differ.
  const bool ask = QSettings().value(kAskOnOpenKey, true).toBool();
  ZtoryMouthLibrary::runWhenNoModal([ask]() {
    // First the version, then its library: the update reads the new one.
    // A new version brings its own mouths too: their check again.
    if (offerNewerVersions(ZtoryMouthLibrary::dialogParent()))
      ZtoryMouthLibrary::offerOnOpen();
    for (const Context &ctx : allContexts())
      updateFor(ctx, ZtoryMouthLibrary::dialogParent(), true, ask);
  });
}
