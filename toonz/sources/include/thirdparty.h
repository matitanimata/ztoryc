#pragma once

#ifndef THIRDPARTY_INCLUDED
#define THIRDPARTY_INCLUDED

#include "tcommon.h"

#include <QProcess>
#include <QString>
#include <QStringList>

#undef DVAPI
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#else
#define DVAPI DV_IMPORT_API
#endif

namespace ThirdParty {

//-----------------------------------------------------------------------------

DVAPI void initialize();

//-----------------------------------------------------------------------------

DVAPI void getFFmpegVideoSupported(QStringList &exts);
DVAPI void getFFmpegAudioSupported(QStringList &exts);

DVAPI bool findFFmpeg(QString dir);
DVAPI bool checkFFmpeg();
DVAPI QString autodetectFFmpeg();

DVAPI QString getFFmpegDir();
DVAPI void setFFmpegDir(const QString &dir);
DVAPI int getFFmpegTimeout();
DVAPI void setFFmpegTimeout(int secs);

DVAPI void runFFmpeg(QProcess &process, const QStringList &arguments);
DVAPI void runFFprobe(QProcess &process, const QStringList &arguments);

DVAPI void runFFmpegAudio(QProcess &process, QString srcPath, QString dstPath,
                          int samplerate = 44100, int bpp = 16,
                          int channels = 2);
DVAPI bool readFFmpegAudio(QProcess &process, QByteArray &rawData);

//-----------------------------------------------------------------------------

DVAPI bool findRhubarb(QString dir);
DVAPI bool checkRhubarb();
DVAPI QString autodetectRhubarb();

DVAPI QString getRhubarbDir();
DVAPI void setRhubarbDir(const QString &dir);
DVAPI int getRhubarbTimeout();
DVAPI void setRhubarbTimeout(int secs);

DVAPI void runRhubarb(QProcess &process, const QStringList &arguments);

//=============================================================================
// whisper.cpp — riconoscimento vocale, per i TEMPI delle parole.
//
// Stesso schema di Rhubarb e ffmpeg (processo esterno, percorso in preferenze +
// ricerca automatica), per due motivi: e' cosi' che il progetto tratta gia' gli
// strumenti di terze parti, e il confine di processo e' anche cio' che tiene
// pulita la licenza quando accanto ci finira' espeak-ng, che e' GPL-3.
//
// ⚠️ whisper.cpp NON fa allineamento forzato di un testo dato: trascrive, e
// `--prompt` si limita a orientarlo. Misurato il 2026-08-15 su audio italiano:
// col modello `tiny` «credi» diventa «cre di», con `base` si aggiusta ma
// compare «non me vale» dove tiny aveva ragione. Cioe' NESSUNA dimensione di
// modello rende la trascrizione affidabile — ma i TEMPI sono buoni in entrambi.
// Da qui l'architettura: Whisper da' i tempi, il copione da' le parole.
//=============================================================================

DVAPI bool findWhisper(QString dir);
DVAPI bool checkWhisper();
DVAPI QString autodetectWhisper();
DVAPI QString getWhisperDir();
DVAPI void setWhisperDir(const QString &dir);
// Il modello da usare: la preferenza se impostata e il file esiste, altrimenti
// quello imballato nel bundle. Vuoto = nessun modello, Whisper non puo' partire.
DVAPI QString getWhisperModel();

//-----------------------------------------------------------------------------

// return  0 = No error
// return -1 = error code
// return -2 = timed out
DVAPI int waitAsyncProcess(const QProcess &process, int timeout);

//-----------------------------------------------------------------------------

}  // namespace ThirdParty

#endif