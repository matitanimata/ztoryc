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
`.ztoryc` e si chiude (`_exit`, senza scrivere preferenze né `env.ini`); chiude da solo le finestre
modali (i «file mancante» delle copie senza disegni).

Corpus in `/Volumes/ZioSam/tahoma2d-workspace/reference/test_projects/roundtrip/` — copie, tracker
spento (`productionTracker="off"`), mai i file veri:

| copia | origine | shot |
|---|---|---|
| rt01 | Messina, storyboard `CS26_06ME_STB_NCP_V1` | 70 |
| rt02 | Cascina, `SB_` | 71 |
| rt03 | Maggiolata Zombie | 34 |
| rt04 | filorosso `SB_test` (il `.ztoryc` era rimasto a 7 shot, la scena ne ha 1) | 1 |
| rt05 | argo intro | 3 |
| rt06 | ZtorycTest `demo_ztoryc_v1` (vecchio, senza uuid) | 8 |

- `run_roundtrip.sh <etichetta>` → `out/<etichetta>/rtNN.ztoryc` (rimette `env.ini` com'era).
- `compare_roundtrip.py <base> <altro>` confronta, con gli uuid sostituiti da segnaposto in ordine di
  comparsa (rt06 riceve uuid casuali a ogni apertura).
- **Base** = `out/base1`, prodotta con il codice della 0.16.2 + passo 0. Due giri sullo stesso codice
  (`base1`, `base2`): identici.
- Dopo ogni passo: `run_roundtrip.sh passoN` e `compare_roundtrip.py base1 passoN` deve dire
  «identico» per tutti, salvo differenze volute e spiegate.

## Stato

- [x] Passo 0 — il ⌘S della scena scrive il `.ztoryc` (segnale `sceneSaved` → Board), il Navigator
  segna la scena come modificata (provato sul Mac); rete di sicurezza `ZTORYC_ROUNDTRIP` con base
  `out/base1` (2026-10-06).
- [~] Passo 1 — `ZtoryModel::reconcileWithXsheet()` alla fine di `resequenceXsheet`, identità per
  voce in `m_shotIds` (colonna + sotto-scena, solo in memoria); abbinamento: stessa colonna → stessa
  sotto-scena con colonna sparita (undo) → stessa posizione per le voci appena aggiunte
  (`addShotNamed`, `addShotFromRasters`). Rete di sicurezza: sei scene identiche alla base
  (2026-10-06). **Manca la prova a mano**: «+» dell'Animatic in mezzo e Navigator sullo shot nuovo;
  tecnica dopo il «+»; Cut/Paste; undo di un Delete; Send to Board.
- Noto per il passo 3: dopo «Save Scene As» `saveZtoryc` non scrive (il percorso non coincide
  con `m_currentZtoryPath`), come già prima.
- Scoperto il 2026-10-06: **Cut → Paste dal Board perde i testi dello shot** e dà uuid doppi
  (audit §7.6). Lo risolvono i passi 1 e 5 (la clip porta lo shot intero; nessun ripiego per
  posizione).
