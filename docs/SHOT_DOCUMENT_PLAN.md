# Copia unica dei dati degli shot — piano

Decisione di Franco, 2026-10-06: **i dati degli shot (testi, dialoghi, note, luci, tracking)
hanno una sola copia, nel modello**. Il Board resta il posto dove si scrivono e si leggono; non
è più il posto dove abitano. Punto di partenza: `docs/SHOT_OPS_AUDIT.md` (§7 per i difetti
provati).

Ramo: `feature/shot-document` (parte da `fix/navigator-stale-shot-index`).

---

## Perché

- Oggi gli stessi dialoghi esistono in **cinque copie**: i widget dei pannelli, `m_shots` di
  ognuno dei **tre Board** vivi (uno per room: `storyboardpanel.cpp:10347`,
  `ztoryanimatic.cpp:5059`, `:8839`) e `ZtoryModel::m_shots`. Si allineano per posizione; chi
  salva per ultimo vince.
- Il `.ztoryc` lo scrivono solo gli eventi dei Board; il ⌘S della scena no. Un testo scritto
  nel Navigator va perso (§7.5 dell'audit, provato).
- Lo stesso schema del pannello Xsheet di Tahoma: il pannello non possiede le celle, le mostra.
  Per questo due pannelli Xsheet non si contraddicono mai.

## Vincoli

1. **Il formato del `.ztoryc` non cambia.** I file vecchi si aprono uguali; una versione
   precedente di Ztoryc apre i file nuovi.
2. **Ogni passo è rilasciabile da solo** e passa la rete di sicurezza (sotto) prima del commit.
3. **Le prove si fanno solo su copie** in `reference/test_projects/`, con il tracker spento
   (`productionTracker="off"`), mai su un progetto vero.

## Identità di uno shot

- **In memoria**: la colonna dello xsheet principale (`TXshColumn*`). È unica anche per due
  Copy della stessa sotto-scena (che condividono il `TXshChildLevel`), e sopravvive al
  riordino (`TXsheet::moveColumn` ruota i puntatori, `txsheet.cpp:2099`).
- **Su disco**: come oggi, ordine + nome della sotto-scena (`level="sub_1"`).
- **Caso scoperto**: l'undo di Tahoma di una cancellazione di colonna ricrea colonne nuove
  (clona da `StageObjectsData`, `columncommand.cpp:591`). Lì il modello riaggancia i dati per
  sotto-scena, da un «cimitero» degli shot cancellati di recente. Con due Copy della stessa
  sotto-scena cancellate e ripristinate l'abbinamento può scambiarle: tollerato.

## Passi

| # | Passo | Cosa cambia per l'utente |
|---|---|---|
| 0 | **Rete di sicurezza + nessun testo perso**: il ⌘S della scena scrive anche il `.ztoryc`; il Navigator segna la scena come modificata; modalità di prova `ZTORYC_ROUNDTRIP` per confrontare i `.ztoryc` | il dialogo del Navigator non si perde più |
| 1 | **Il modello riconosce gli shot per colonna**: si riallinea dallo xsheet a ogni cambiamento, invece di essere un elenco da tenere in passo | nessuno shot riceve dati di un altro |
| 2 | **Il Board legge e scrive nel modello**: tiene solo i widget; i tre Board diventano tre finestre sulla stessa copia | due Board non divergono più |
| 3 | **Caricamento e salvataggio nel modello** (oggi `loadZtoryc`/`saveZtoryc` del Board) | il `.ztoryc` si salva anche senza Board nella room |
| 4 | **Via le sincronizzazioni**: `syncShotPanels`, `pushTrackingToBoard`/`pullTrackingFromBoard`, lo specchio per indice di `shotDataChanged` | — |
| 5 | **Operazioni uniche** (Add, Delete, Cut, Copy, Clone, Paste, Merge, Split, Move) in `ZtoryShotOps`, chiamate da Board, Animatic, Monitor | stessi comandi, stesso comportamento ovunque |
| 6 | **Undo sullo xsheet** (decisione ancora da prendere con Franco) | undo anche senza Board |

## Rete di sicurezza

Con `ZTORYC_ROUNDTRIP=1`, Ztoryc apre la scena passata sulla riga di comando, riscrive il
`.ztoryc` e si chiude. Su copie di scene vere (tracker spento) si produce una **base** con il
codice di partenza; dopo ogni passo si rifà il giro e i `.ztoryc` devono essere **identici byte
per byte** alla base.

## Stato

- [~] Passo 0 — fatto: il ⌘S della scena scrive il `.ztoryc` (segnale `sceneSaved` → Board),
  il Navigator segna la scena come modificata; provato sul Mac (la battuta è nel `.ztoryc` dopo
  ⌘S; alla chiusura Ztoryc chiede di salvare). **Manca**: la modalità `ZTORYC_ROUNDTRIP` e la base
  dei `.ztoryc` di riferimento.
- Noto per il passo 3: dopo «Save Scene As» `saveZtoryc` non scrive (il percorso non coincide
  con `m_currentZtoryPath`), come già prima.
- Scoperto il 2026-10-06: **Cut → Paste dal Board perde i testi dello shot** e dà uuid doppi
  (audit §7.6). Lo risolvono i passi 1 e 5 (la clip porta lo shot intero; nessun ripiego per
  posizione).
