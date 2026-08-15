#pragma once

//============================================================================
// ZtoryLipSync — mettere in fila i pezzi: audio → Whisper → copione → fotogrammi.
//
// Il resto della catena esiste già ed è provato altrove:
//   ZtoryModel::parseDialogue()   chi dice cosa (dal testo del pannello)
//   ZtoryModel::alignToScript()   i tempi di Whisper sulle parole del copione
//   ThirdParty::getWhisperModel() il modello, dal bundle o dalle preferenze
// Qui c'è solo ciò che li unisce: estrarre l'audio dello shot, lanciare
// whisper-cli, leggerne il JSON, e convertire i millisecondi in fotogrammi.
//
// Il lavoro gira in un thread a parte: su una battuta di qualche secondo
// whisper-cli impiega frazioni di secondo, ma su uno shot lungo no — e
// un'interfaccia che si pianta mentre "pensa" è indistinguibile da una bloccata.
//============================================================================

#include "ztorymodel.h"

#include <QObject>
#include <QString>
#include <QVector>

class QProcess;

//----------------------------------------------------------------------------
// Il risultato per UN personaggio: le sue parole, in FOTOGRAMMI dello shot.
// Fuori da qui i millisecondi non servono più a nessuno.
//----------------------------------------------------------------------------
struct ZtoryCharacterTrack {
  QString assetUuid;
  QString characterName;
  struct Word {
    QString text;
    int     startFrame = 0;
    int     endFrame   = 0;
  };
  QVector<Word> words;
};

//----------------------------------------------------------------------------
// Contesto dello shot su cui si sta lavorando: quale shot, che intervallo di
// righe occupa nel main xsheet (dov'e' l'audio), e la sua sotto-scena (dove
// vanno le colonne di testo).
//----------------------------------------------------------------------------
struct ZtoryShotContext {
  int      shotIndex = -1;
  int      column    = -1;   // colonna del main xsheet
  int      firstRow  = 0;    // prima riga occupata nel main xsheet
  int      lastRow   = 0;
  class TXsheet *subXsheet = nullptr;
  bool isValid() const { return shotIndex >= 0 && subXsheet; }
};

// Lo shot la cui sotto-scena e' aperta adesso. Si risale confrontando la
// sotto-scena corrente col TXshChildLevel delle celle del main xsheet: la
// COLONNA sta in ChildStack::AncestorNode ma non ha accessori pubblici, e
// childstack.h e' core condiviso con Tahoma2D.
ZtoryShotContext ztoryCurrentShotContext();

// Estrae in un wav l'audio del main xsheet sull'intervallo dello shot, mixando
// TUTTE le colonne sonore. Il mix va bene: chi parla lo dice il copione, non
// serve una pista per personaggio.
// Stringa vuota = nessun audio (nessuna colonna sonora, o silenzio).
QString ztoryExtractShotAudio(const ZtoryShotContext &ctx);

//----------------------------------------------------------------------------
class ZtoryLipSync final : public QObject {
  Q_OBJECT
public:
  explicit ZtoryLipSync(QObject *parent = nullptr);
  ~ZtoryLipSync() override;

  // Tutto ciò che serve per una passata. Tenuto in un oggetto e non in dieci
  // argomenti: la lista cresce (lingua, modello, offset), e una firma lunga
  // dieci parametri si sbaglia in silenzio scambiandone due dello stesso tipo.
  struct Request {
    QString wavPath;        // audio già estratto, 16 kHz mono
    QString dialogue;       // testo dei pannelli, così com'è scritto
    QString language;       // "it", "en", … vuoto = rilevamento automatico
    double  fps        = 24;
    int     firstFrame = 1;  // fotogramma a cui corrisponde l'istante 0 del wav
  };

  // Avvia. Il risultato arriva con finished(); un solo lavoro alla volta.
  bool start(const Request &req);
  bool isRunning() const { return m_running; }
  void cancel();

  // Perché non si può partire, o stringa vuota se si può. Chiamabile prima di
  // start() per spegnere un comando invece di farlo fallire dopo il clic.
  static QString unavailableReason();

signals:
  void progress(const QString &message);
  // `ok` falso: `message` dice cosa è andato storto. Le tracce sono una per
  // personaggio che parla, già in fotogrammi.
  void finished(bool ok, const QVector<ZtoryCharacterTrack> &tracks,
                const QString &message);

private:
  void onProcessDone(int exitCode);
  // Legge il JSON di whisper-cli. Separata perché è la parte che cambia se
  // cambia il formato, ed è quella da provare senza lanciare un processo.
  static QVector<TimedWord> parseWhisperJson(const QByteArray &json,
                                             QString *error);

  QProcess *m_proc = nullptr;
  bool      m_running = false;
  Request   m_req;
  QString   m_jsonPath;
};
