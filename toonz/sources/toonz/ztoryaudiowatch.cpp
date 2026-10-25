#include "ztoryaudiowatch.h"

// Tnz6 includes
#include "tapp.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

#ifndef _WIN32
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <cstdlib>
#endif

namespace {

// The functions that notified the change, innermost first: the operation
// that moved the audio is usually among the first Ztoryc/Tahoma names.
QString callStack() {
#ifndef _WIN32
  void *frames[40];
  int count = backtrace(frames, 40);
  QStringList lines;
  for (int i = 2; i < count; ++i) {  // skip callStack and onXsheetChanged
    Dl_info info;
    if (!dladdr(frames[i], &info) || !info.dli_sname) continue;
    int status   = 0;
    char *pretty = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
    QString name = QString::fromLatin1(status == 0 && pretty ? pretty : info.dli_sname);
    std::free(pretty);
    if (name.startsWith("Q") || name.contains("QMetaObject") ||
        name.contains("QtPrivate"))
      continue;  // Qt's signal plumbing says nothing
    lines << "      " + name.left(160);
    if (lines.size() >= 12) break;
  }
  return lines.join("\n");
#else
  return "      (call stack not available on Windows)";
#endif
}

}  // namespace

//============================================================================

ZtoryAudioWatch *ZtoryAudioWatch::instance() {
  static ZtoryAudioWatch *s = new ZtoryAudioWatch();
  return s;
}

ZtoryAudioWatch::ZtoryAudioWatch() : QObject(nullptr) {}

void ZtoryAudioWatch::start() {
  if (m_started) return;
  m_started = true;
  TApp *app = TApp::instance();
  connect(app->getCurrentScene(), &TSceneHandle::sceneSwitched, this,
          &ZtoryAudioWatch::onSceneSwitched);
  connect(app->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this,
          &ZtoryAudioWatch::onXsheetChanged);
  m_tracks = snapshot();
}

std::vector<ZtoryAudioWatch::Track> ZtoryAudioWatch::snapshot() const {
  std::vector<Track> tracks;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return tracks;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();  // audio lives here
  if (!xsh) return tracks;
  for (int c = 0; c < xsh->getColumnCount(); ++c) {
    TXshColumn *column = xsh->getColumn(c);
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (!sc) continue;
    Track track{c, {}};
    for (int i = 0; i < sc->getColumnLevelCount(); ++i) {
      ColumnLevel *cl = sc->getColumnLevel(i);
      if (!cl) continue;
      TXshSoundLevel *sl = cl->getSoundLevel();
      track.clips.push_back({sl ? QString::fromStdWString(sl->getName()) : "?",
                             cl->getStartFrame(), cl->getStartOffset(),
                             cl->getEndOffset()});
    }
    tracks.push_back(track);
  }
  return tracks;
}

QString ZtoryAudioWatch::lastAction() const {
  TUndoManager *um = TUndoManager::manager();
  int index        = um->getCurrentHistoryIndex();
  TUndo *undo = index >= 0 && index < um->getHistoryCount() ? um->getUndoItem(index)
                                                            : nullptr;
  return undo ? undo->getHistoryString() : QString("(nothing in the history)");
}

void ZtoryAudioWatch::write(const QString &text) const {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/crash";
  QDir().mkpath(dir);
  QFile f(dir + "/audio_watch.log");
  if (f.open(QIODevice::Append | QIODevice::Text)) QTextStream(&f) << text;
}

void ZtoryAudioWatch::onSceneSwitched() { m_tracks = snapshot(); }

void ZtoryAudioWatch::onXsheetChanged() {
  std::vector<Track> now = snapshot();
  QStringList changes;
  const size_t count = std::max(now.size(), m_tracks.size());
  for (size_t t = 0; t < count; ++t) {
    if (t >= now.size()) {
      changes << QString("  audio track %1 (column %2) gone")
                     .arg(t + 1).arg(m_tracks[t].column + 1);
      continue;
    }
    if (t >= m_tracks.size()) {
      changes << QString("  audio track %1 (column %2) new, %3 clips")
                     .arg(t + 1).arg(now[t].column + 1).arg(now[t].clips.size());
      continue;
    }
    const Track &a = m_tracks[t], &b = now[t];
    if (a.clips == b.clips) continue;
    changes << QString("  audio track %1 (column %2 -> %3):")
                   .arg(t + 1).arg(a.column + 1).arg(b.column + 1);
    auto line = [](const Clip &c) {
      // frames as the timeline shows them, from 1
      return QString("%1 at frame %2, offsets %3/%4")
          .arg(c.name).arg(c.startFrame + 1).arg(c.startOffset).arg(c.endOffset);
    };
    const size_t n = std::max(a.clips.size(), b.clips.size());
    for (size_t i = 0; i < n; ++i) {
      if (i < a.clips.size() && i < b.clips.size() && a.clips[i] == b.clips[i])
        continue;
      changes << QString("    clip %1: %2  ->  %3")
                     .arg(i + 1)
                     .arg(i < a.clips.size() ? line(a.clips[i]) : "-")
                     .arg(i < b.clips.size() ? line(b.clips[i]) : "-");
    }
  }
  m_tracks = now;
  if (changes.isEmpty()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  write(QString("%1  %2\n  last action in the history: %3\n%4\n  notified by:\n%5\n\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
            .arg(scene ? QString::fromStdWString(scene->getSceneName()) : "?")
            .arg(lastAction())
            .arg(changes.join("\n"))
            .arg(callStack()));
}
