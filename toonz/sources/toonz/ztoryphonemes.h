#pragma once

//============================================================================
// ZtoryPhonemes — dalle parole ai VISEME, cioe' alle caselle delle bocche.
//
// La divisione del lavoro, dopo le misure del 2026-08-16:
//   Vosk      dice QUANDO cade ogni parola   (10 ms di scarto, misurato)
//   espeak-ng dice DI COSA e' fatta          (grafemi -> fonemi)
//   qui       si distribuiscono i fonemi dentro l'intervallo della parola
//
// Perche' basta: l'errore grosso e' gia' stato tolto. Sbagliare la
// distribuzione dentro una parola da 400 ms costa un fotogramma o due, su
// bocche che comunque si succedono; sbagliare DOVE cade la parola ne costava
// dieci.
//
// ⚠️ espeak-ng e' GPL-3.0 e va invocato come PROCESSO SEPARATO, mai linkato:
// linkarlo contaminerebbe la licenza BSD di Ztoryc. Stesso schema gia' usato
// per Rhubarb e ffmpeg. E' la trappola gia' incontrata con Krita e AnimeEffects.
//============================================================================

#include <QString>
#include <QStringList>
#include <QVector>

namespace ZtoryPhonemes {

// Un suono e quanto tiene, RELATIVAMENTE agli altri della stessa parola: non
// sono millisecondi, contano solo l'uno rispetto all'altro.
struct Viseme {
  QString shape;   // "ai" "e" "o" "u" "fv" "l" "mbp" "wq" "etc" "rest"
  double  weight = 1.0;
};

// I nomi delle caselle sono ESATTAMENTE quelli che LipSyncPopup si aspetta
// (lipsyncpopup.cpp:145-165), cosi' la colonna di testo prodotta qui e' gia'
// leggibile dal percorso Rhubarb che esiste da prima.
extern const char *kRest;

bool    isAvailable();
QString unavailableReason();

// Una sola invocazione di espeak-ng per tutte le parole: su uno shot lungo
// lanciarne uno per parola si sente. Se il numero di token in uscita non
// combacia con le parole in ingresso si ripiega su una chiamata per parola,
// perche' un disallineamento qui sposterebbe i fonemi sulla parola sbagliata.
// Lista vuota in uscita = non disponibile, e chi chiama scrive le parole.
QVector<QVector<Viseme>> forWords(const QStringList &words,
                                  const QString &language, QString *error);

}  // namespace ZtoryPhonemes
