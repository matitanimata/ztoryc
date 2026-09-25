#pragma once

//============================================================================
// ZtoryRecovery — recupero del lavoro non salvato dopo un crash.
//
// Franco, 2026-09-25, dopo aver perso un taglio di mesh su DOTTO: «quella
// specie di save automatico che hai messo nella thumbs room ... non si
// potrebbe estendere anche al tnz?».
//
// ── NON E' L'AUTOSAVE DI TAHOMA ────────────────────────────────────────────
// Quello (TApp::autosave → IoCmd::saveAll) scrive SOPRA i file veri: rende
// definitivo cio' che forse non si voleva, blocca, e si pesta i piedi con i
// salvataggi manuali ravvicinati. Qui i file ufficiali non si toccano mai.
//
// ── COME FUNZIONA ──────────────────────────────────────────────────────────
//   - ogni qualche minuto, solo se la scena e' sporca, il tool non lavora e
//     non c'e' un salvataggio in corso: la scena e i LIVELLI MODIFICATI (il
//     lavoro perso il 25/09 era nel .mesh, non nel .tnz) si scrivono in una
//     cartella di recupero, con un manifesto che dice dove va ogni file;
//   - salvare la scena cancella il recupero; anche cambiare scena o uscire,
//     perche' li' la domanda «salvare?» l'ha gia' fatta Tahoma;
//   - quindi un recupero che sopravvive vuol dire che Ztoryc NON e' uscito
//     pulito: riaprendo quella scena si chiede se rimetterlo.
//
// I livelli si scrivono con TXshSimpleLevel::save(dst, orig): verso un
// percorso diverso e' una COPIA — rimette il percorso in memoria, e
// saveSimpleLevel azzera il flag «modificato» solo salvando sull'originale.
// ⚠️ NON basta: il ramo delle palette legate a una studio palette (Toonz
// Raster) azzera il dirty della palette sempre. Per questo snapshot()
// fotografa e rimette i due flag attorno alla copia (review del 25/09).
//
// Salvare cancella il recupero solo quando non resta nessun livello
// modificato: Save Scene scrive solo il .tnz, e Save All emette sceneSaved
// prima di scrivere i livelli.
//
// I .bak di Tahoma restano come sono: il recupero si aggiunge, non li
// sostituisce (Franco, 2026-09-25).
//============================================================================

#include <QObject>
#include <QString>

class QTimer;

class ZtoryRecovery final : public QObject {
  Q_OBJECT

public:
  static ZtoryRecovery *instance();

  //! Avvia il timer e aggancia i segnali. Una volta, all'avvio.
  void start();

  //! La cartella dei recuperi (una sottocartella per scena): dentro il
  //! progetto della scena corrente, `.ztoryc_recovery/`.
  static QString rootFolder();

private slots:
  void snapshot();
  void onSceneSaved();
  void afterSceneSaved();
  void onSceneSwitched();
  void onAboutToQuit();
  void checkForRecovery();
  void onToolEditingFinished();

private:
  ZtoryRecovery();

  bool restore(const QString &dir, const QString &scenePath);

  QTimer *m_timer = nullptr;
  //! La cartella di recupero scritta in QUESTA sessione per la scena
  //! corrente: e' quella da togliere quando la scena si salva o si lascia.
  QString m_activeDir;
  bool m_busy = false;
  //! Un giro saltato perche' il tool era a meta' di un gesto: la copia si fa
  //! appena il gesto finisce, non tre minuti dopo.
  bool m_pending = false;
};
