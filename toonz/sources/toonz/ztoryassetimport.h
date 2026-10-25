#pragma once

//============================================================================
// ZtoryAssetImport — dal breakdown agli asset dentro lo shot esportato.
//
// La catena era gia' pronta e non aveva il consumatore: breakdown -> asset ->
// file risolto (`ZtoryModel::resolveAssetFile`) esisteva solo per DIRE, nella
// colonna «File» del pannello Breakdown, cosa avrebbe trovato l'export. Qui
// quella stessa risposta viene usata per importare davvero.
//
// Due operazioni, e sono due cose diverse:
//   - il CONTROLLO, che si fa PRIMA di esportare e produce un rapporto. Franco:
//     «fa un check degli asset e fa un report se manca qualcosa cosi' l'utente
//     puo' decidere se proseguire saltando gli asset mancanti oppure
//     interrompere, inserire le cose che mancano e rilanciare l'export»;
//   - l'IMPORT vero, una volta per shot, dentro la sua sotto-scena aperta.
//
// ⚠️ Il controllo e' per ASSET DISTINTO, non per shot. Gli asset si ripetono su
// molti shot: controllando per shot diventano centinaia di accessi al disco, e
// su un volume esterno si sentono.
//============================================================================

#include "tfilepath.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class TXsheet;
class TXshLevel;
class TFilePath;
struct AssetImportPolicy;

//----------------------------------------------------------------------------
// Cosa si sa di UN asset distinto richiesto dagli shot che si stanno per
// esportare.
//----------------------------------------------------------------------------
struct ZtoryAssetCheck {
  QString     uuid;
  QString     name;
  QString     type;
  QString     file;    // risolto; vuoto se non si risolve
  QString     reason;  // perche' non si risolve; vuoto se si risolve
  QStringList shots;   // le etichette degli shot che lo chiedono
  bool onPurpose = false;  // nessun file di proposito (disegnato in un altro)
  QString note;  // entra, ma con un difetto da sapere (layer PSD rovinati)
  bool ok() const { return onPurpose || !file.isEmpty(); }
};

// La prova di caricamento di un PSD, con lo stesso lettore dell'import.
// - problem: il file non entra (intestazione illeggibile — Tahoma aprirebbe
//   una finestra d'errore per shot —, oppure un modo colore che il lettore non
//   sa leggere: i livelli arriverebbero vuoti senza una parola). Si salta.
// - note: layer rovinati. TPSDReader::load ingoia l'errore (psd.cpp, catch
//   (...) in fondo) e restituisce un layer VUOTO: Tahoma lo carica cosi', e
//   qui lo si riconosce perche' ha un'area ma niente dentro. Entra lo
//   stesso, e il rapporto dice quali.
// Entrambi gia' come frasi che dicono cosa fare. Il risultato si ricorda per
// percorso+dimensione+data (solo quelli certi: un errore di lettura del
// disco si riprova).
struct ZtoryPsdCheck {
  QString problem, note;
};
ZtoryPsdCheck ztoryCheckPsd(const QString &file);

// Controlla il breakdown degli shot indicati (uuid dello shot di progetto).
// Una voce per asset distinto, nell'ordine in cui compaiono.
// Reference assets are left out unless `withReferences`.
QVector<ZtoryAssetCheck> ztoryCheckShotAssets(const QStringList &shotUuids,
                                              bool withReferences = false);

// Il rapporto da mostrare: solo cio' che non si risolve, una riga per asset con
// il MOTIVO — che e' gia' una frase che dice cosa fare — e chi lo chiede.
// Vuoto = non manca niente.
QString ztoryAssetReport(const QVector<ZtoryAssetCheck> &checks);

//----------------------------------------------------------------------------
// Il risultato di un import: cosa e' entrato nella sotto-scena, e cosa va
// tolto dopo il salvataggio. La sotto-scena e' LA STESSA dello storyboard: se
// le colonne restassero, al secondo export lo shot ne avrebbe il doppio.
//----------------------------------------------------------------------------
struct ZtoryImportedAssets {
  QList<int>         columns;  // indici creati nella sotto-scena
  QList<TXshLevel *> levels;   // livelli aggiunti al cast della scena
  QStringList        log;      // una riga per asset, per il registro di export
};

// Importa nella sotto-scena APERTA dello shot gli asset del suo breakdown.
// Va chiamata con la sotto-scena corrente (l'export ci e' gia' dentro): le
// funzioni di caricamento di Tahoma lavorano sullo xsheet corrente.
// Gli asset che non si risolvono si saltano, con la riga di registro che dice
// perche': il controllo di prima li ha gia' mostrati all'utente.
// `shotScenePath`: il .tnz che lo shot diventera'. Gli asset su Import si
// copiano nella SUA cartella (+extras/sh040/… con «use scene path»), non in
// quella dello storyboard aperto.
ZtoryImportedAssets ztoryImportShotAssets(const QString &shotUuid,
                                          TXsheet *subXsheet,
                                          const TFilePath &shotScenePath,
                                          bool withReferences = false);

// Carica UN psd nello xsheet corrente, dalla colonna `col0`, con le opzioni
// PSD e il modo (Load / Import) di `policy`. Restituisce i livelli caricati.
// Serve all'export degli shot e alla creazione della scena del personaggio
// (il PSD da riggare): due copie delle stesse righe divergono.
// `errors`: se c'e', nessuna domanda e gli errori finiscono li' (l'export);
// se manca, Tahoma li mostra in finestra come sempre.
// `importScenePath`: con Import, la scena nella cui cartella va la copia
// (vuoto = la corrente). Tahoma non copia MAI un PSD, neanche su Import
// (loadPSDResource non passa dall'import): la copia la fa questa funzione.
QList<TXshLevel *> ztoryLoadPsdWithPolicy(
    const TFilePath &psd, const AssetImportPolicy &policy, int col0,
    QStringList *errors = nullptr,
    const TFilePath &importScenePath = TFilePath());
