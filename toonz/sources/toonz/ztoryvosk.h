#pragma once

//============================================================================
// ZtoryVosk — allineamento forzato: dall'audio e dal copione, i TEMPI.
//
// Sostituisce Whisper nel ruolo di cronometro. Whisper resta per le lingue che
// Vosk non copre e per quando il copione non c'e': li' serve capire COSA si
// dice, ed e' il mestiere in cui e' migliore.
//
// PERCHE' NON WHISPER PER I TEMPI — misurato il 2026-08-16 sull'audio vero di
// una battuta di SOFIA, con riscontro sulle fricative dello spettro (la /s/ di
// "questo", la /f/ di "fa": eventi fisici, non l'opinione di un altro modello):
//
//     Vosk                  scarto medio  10 ms (0,2 fotogrammi)
//     Whisper base-q5_1                   30 ms (0,8)   peggiore 110 ms
//     Whisper + DTW                      191 ms (4,8)   peggiore 400 ms
//
// E su 14 battute della stessa registrazione: Whisper ha prodotto 86 parole a
// durata ZERO su 263, e su una battuta e' andato in allucinazione (108 parole,
// coda a 22 secondi oltre la fine dell'audio). Vosk: zero degenerazioni su 158
// parole. I tempi di Whisper vengono dai pesi di attenzione, che sono una stima
// approssimata; quelli di Vosk dall'allineamento del reticolo di Kaldi, che e'
// un Viterbi fotogramma per fotogramma — cioe' la cosa che ci serve.
//
// ⚠️ Vosk NON ricampiona: pretende l'audio alla frequenza dichiarata al
// riconoscitore. Whisper lo faceva da solo, quindi il codice che lo chiamava non
// se ne curava. Qui la conversione a 16 kHz mono la facciamo noi.
//============================================================================

#include "ztorymodel.h"  // TimedWord

#include <QString>
#include <QStringList>
#include <QVector>

namespace ZtoryVosk {

// La libreria si carica a runtime (QLibrary), non si linka: se manca, il lip
// sync ripiega su Whisper invece di impedire l'avvio dell'applicazione.
bool isAvailable();

// Vuota se si puo' partire, altrimenti il motivo, gia' traducibile.
QString unavailableReason();

// Lingue per cui esiste un modello, imballato o gia' scompattato in cache.
QStringList availableLanguages();
bool hasLanguage(const QString &lang);

// Scompatta in cache il modello della lingua, se non c'e' gia'. Lenta la prima
// volta (una manciata di secondi), immediata dopo. Separata da align() perche'
// chi chiama possa mostrare un avviso PRIMA di sembrare bloccato.
bool prepareLanguage(const QString &lang, QString *error = nullptr);

// Il lavoro vero. BLOCCANTE: chiamare da un thread di lavoro, non dalla UI.
//
// `scriptWords` sono le parole del copione: vincolano il riconoscimento a un
// vocabolario chiuso, che e' cio' che trasforma un riconoscitore in un
// allineatore. Le parole che il modello non conosce vengono scartate dal
// vincolo (non dal risultato): un nome di fantasia non deve far fallire tutto.
// Lista vuota = riconoscimento libero.
QVector<TimedWord> align(const QString &wavPath, const QString &lang,
                         const QStringList &scriptWords, QString *error);

}  // namespace ZtoryVosk
