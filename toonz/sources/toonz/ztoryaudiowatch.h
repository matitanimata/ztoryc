#pragma once

// Ztoryc: a sentinel on the audio of the main xsheet (Franco, 2026-10-25).
//
// Small audio desyncs on the animatic that nobody can tie to an action: after
// a copy/paste of audio and some editing «without moving the tracks»,
// something had moved. Instead of guessing from the code, every change of
// position of a sound clip is written to crash/audio_watch.log with the last
// operation in the undo history and the call stack that notified it.
//
// Read only: it takes snapshots on xsheetChanged and compares them. Temporary,
// to be removed once the cause is found.

#include <QObject>
#include <QString>

#include <vector>

class ZtoryAudioWatch final : public QObject {
  Q_OBJECT

public:
  static ZtoryAudioWatch *instance();
  void start();

private:
  struct Clip {
    QString name;
    int startFrame, startOffset, endOffset;
    bool operator==(const Clip &o) const {
      return name == o.name && startFrame == o.startFrame &&
             startOffset == o.startOffset && endOffset == o.endOffset;
    }
  };
  struct Track {
    int column;
    std::vector<Clip> clips;
  };

  ZtoryAudioWatch();
  std::vector<Track> snapshot() const;
  QString lastAction() const;
  void write(const QString &text) const;

  std::vector<Track> m_tracks;
  bool m_started = false;

private slots:
  void onSceneSwitched();
  void onXsheetChanged();
};
