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

#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

class TXsheet;
class TXshLevel;

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
  bool ok() const { return !file.isEmpty(); }
};

// Controlla il breakdown degli shot indicati (uuid dello shot di progetto).
// Una voce per asset distinto, nell'ordine in cui compaiono.
QVector<ZtoryAssetCheck> ztoryCheckShotAssets(const QStringList &shotUuids);

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
ZtoryImportedAssets ztoryImportShotAssets(const QString &shotUuid,
                                          TXsheet *subXsheet);
