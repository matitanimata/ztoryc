# Ztoryc — il lavoro aperto (ANIMATIC_TASKS)

> **Riordinato il 2026-09-24** su richiesta di Franco: qui resta SOLO il lavoro
> vivo, con i suoi dettagli tecnici. Le **priorita'** stanno in
> `~/ZtorYc/ROADMAP_1.0.md`. Tutto cio' che e' chiuso e' in
> `ANIMATIC_TASKS_ARCHIVE_2026-09.md` (e prima ancora `_2026-05`), senza
> niente di perso: si cerca li' il *perche'* di una scelta.
> Copia integrale di prima del riordino:
> `/Volumes/ZioSam/tahoma2d-workspace/reference/backup_docs/`.

## Come si tiene questo file — perche' le voci chiuse risultavano aperte

Il 24/09 una verifica ha trovato **una ventina di voci segnate aperte che erano
chiuse da settimane**. Le cause, e la regola che ne esce:

1. **La stessa voce viveva in piu' posti** (segnalazioni del 19/7, lista del
   2/8, Priority Order, blocco del 18/8...): chi la chiudeva ne aggiornava uno.
   → **Una voce, un posto solo.** Se serve citarla altrove, si mette un rimando.
2. **Lo stato era scritto a mano, a fine sessione**, e la chiusura avveniva a
   meta' sessione nel codice. → **Si chiude quando si committa**: nello stesso
   giro la voce passa nell'archivio, con commit e data.
3. **«Da collaudare» restava tale**: la conferma di Franco arrivava a voce, in
   chat, e moriva con la chat. → la conferma si scrive quando arriva.
4. **Commit che sistemavano voci senza nominarle.** → il messaggio di commit
   cita la voce, e la voce il commit.
5. **La macchina Windows scrive nel repo, non su Drive** (vedi AGENTS.md,
   «sessione chiusa», punto 4).

## LEGEND

- BUG = existing code that is broken
- NEW = feature that does not exist yet
- MOD = existing code that needs modification

---

## Categorie

Ztoryc (storyboard, animatic, thumbs) · ZtoRig / personaggio · Puppetoonz ·
Kitsu / produzione · Pipeline AI (Anymatix) · Blender · Infrastruttura.
Kitsu, Anymatix e Blender oggi non hanno voci tecniche aperte: il loro
stato e le prossime mosse sono nella ROADMAP.

---

## 🛑 SOSPESI PER DECISIONE DI FRANCO — non riproporli

> Leggere PRIMA di proporre qualsiasi cosa. Sono voci ancora aperte piu' in
> basso, ma Franco ha deciso di lasciarle stare: una sessione che le rilancia
> gli fa perdere tempo. Si riaprono solo se **lui** le riapre, o se il sintomo
> ricapita da solo lavorando.


- **Il Clone NON copia dialoghi, azione e note dello shot — SI LASCIA COSI'**
  (Franco, 2026-09-24). Lo shot clonato non si porta dietro l'audio, che resta
  nella timeline: *«non ha senso che cloni anche il testo dello shot»*. Il
  testo (dialogo/azione/note per pannello, note e note VFX dello shot)
  descrive il MOMENTO del racconto, non il disegno; copiarlo attribuirebbe due
  volte la stessa battuta e confonderebbe la mappatura dalla sceneggiatura.
  Era la voce B14 della road map 1.0. **Non riproporla.**
- **Room TRADITIONAL (task 38) — RIMANDATA** (Franco, 2026-09-24): *«può
  aspettare, c'è tutto un lavoro da fare sulle room»*. Va dentro un
  ripensamento generale delle room, ancora da definire. Le verifiche fatte
  (cleanup gia' nel codice, scansione tolta da Tahoma nel 2020, HP ENVY 4520
  in wifi → eSCL) restano scritte nel task 38.
- **Entrando in uno shot il VISORE resta sul primo fotogramma — SI LASCIA
  COSI'** (Franco, 2026-09-23). Dal 2026-09-21 entrando in uno shot la cella
  corrente dell'xsheet va su Col1 e sulla riga del MARK OUT, cosi' si allunga
  in fretta la durata dei disegni. Il visore pero' mostra il PRIMO fotogramma
  della colonna, perche' la testina e' governata dal frame handle separato
  dell'animatic, che viene messo di proposito sulla riga dello shot
  nell'xsheet principale.
  Franco ci ha pensato e ha deciso di lasciarlo: *«forse è un caso
  particolare, lascia così»*. Portare anche il visore sul mark out vorrebbe
  dire che entrando in uno shot si vede l'ULTIMO fotogramma invece del primo
  — comodo per allungare, scomodo per vedere da dove parte lo shot.
  **Chi lo vuole ha gia' il comando**: «Last Frame», scorciatoia predefinita
  ⌥. (Option punto). Verificato nel codice: pilota il pulsante «ultimo» del
  FlipConsole, che usa `to = m_markerTo` quando i marker ci sono — e dentro
  uno shot ci sono sempre. Quindi porta sul mark out, non sulla fine scena.
  **Non riproporlo** come difetto.
- **Il secchiello / autofill nella Thumbs room — NON SI FA** (Franco,
  2026-09-21): *«no il secchiello non lo metterei non complichiamoci la
  vita»*. Era nato da una sua osservazione mentre si chiudeva la Thumbs room
  («avrebbe fatto comodo l'autofill come quello che abbiamo sullo smart
  raster»), e la risposta misurata è che **non si trasporta**: l'autofill
  vive in `toonzrasterbrushtool.cpp` e lavora su `TRasterCM32P` — ink, paint
  e tone separati, BFS con barriera `getInk() != 0` e riempimento con
  `setPaint()`. La tela della Thumbs room è `TRaster32P`, RGBA pieno, senza
  ink e senza paint: sarebbe un riempimento **nuovo**, non un travaso.
  In più l'autofill che abbiamo ha un difetto ancora aperto — il bordino
  bianco sui pixel antialiased, cinque approcci tutti regrediti il
  2026-06-13 — e su RGBA lo stesso problema si ripresenta senza nemmeno il
  tone che nel fill nativo dà la direzionalità.
  **Non riproporlo.** Si riapre solo se lo riapre lui.
- **La tendina delle bocche mostra i livelli di tutti i personaggi — SI LASCIA
  COSI'** (Franco, 2026-08-28, dopo aver visto i set di SOFIA mentre lavorava
  sul lupo): *«quando le mappo lo faccio nella scena del singolo character
  quindi non dovrebbe essere una situazione che si verifica troppo spesso e
  visto che i set hanno anche il nome del personaggio potremmo lasciarlo
  cosi'»*. Cioe': la mescolanza c'e', ma si vede solo in una scena con piu'
  personaggi mappati insieme, che non e' come si lavora. **Non riproporre il
  raggruppamento per personaggio nella tendina.**
  Quello che invece E' stato corretto (commit `df4df3a1a`): una sotto-scena non
  eredita piu' la mappa di un omonimo, e c'e' la casella «only mouth levels».
  Resta aperto, ma come lavoro a se' e non come difetto: **collegare la
  sotto-scena al `characterUuid` registrandolo all'import** — oggi non si puo'
  dedurre, `TXshChildLevel` ha solo nome e icona e i `<target>` del `.zmouth`
  sono soli numeri di fotogramma.
- **I crash e i problemi sulle SCENE VECCHIE** (2026-08-14): *«per quanto
  riguarda i crash e i problemi con scene vecchie lascerei stare, vediamo se
  ricapita lavorandoci»*. Coperti da questa decisione:
  - crash su «Salva sotto-scena come scena», mesh non trovate
  - il pin va sul vertice sbagliato su scene precedenti all'IK
  - il personaggio «parte» manipolando le anche — solo su animazioni vecchie
- **L'IK resta com'e'** (2026-08-14): dopo la prova col rilevatore di
  ribaltamento, Franco: *«mi pare piu' stabile e controllabile di quel che
  ricordavo, forse possiamo lasciare l'ik com'era»*. Branch
  `feature/ik-pole-vector` non mergiato. Non riproporre annealing ne' pole
  vector senza un sintomo nuovo.
- **Otter (il secondo fork)** — ⚠️ **NON PIU' SOSPESO, ma nemmeno da fare
  adesso** (Franco, 2026-08-16): *«su Otter sono gia' praticamente convinto e
  dobbiamo lavorare COME SE fosse gia' cosi', pero' non ho urgenza di metterlo
  in atto subito»*. Sostituisce il «ci sto ancora ragionando» del 2026-08-13.
  **Cosa vuol dire in pratica**: le scelte di progetto vanno prese tenendo
  separati storyboard/animatic (Ztoryc) e character animation (ZtoRig,
  deformatori, fisica, auto-shadow → Otter), senza pero' spendere tempo nella
  separazione vera. Non proporre il fork come lavoro; non incrociare i due
  mondi in modo che poi separarli costi. Vedi `COMPETITIVE_ROADMAP.md` sez. 8.
- **Assistenti al disegno da OpenToonz** (2026-08-13): candidato misurato e
  registrato in `OPENTOONZ_PORT_CANDIDATES.md`, ma Franco ha scelto di passare
  prima al rig. Non e' il prossimo lavoro.
- **Render sbagliato sh110**: sospeso dal 2026-08-07. ⚠️ **Novita' del
  2026-08-14**: ora e' **riproducibile a comando** (tcomposer headless, frame
  110, MD5 stabile) — ma resta sospeso finche' Franco non lo riapre. Vedi la
  voce dedicata piu' sotto.

- 🐞 **DUE DIFETTI ZtoRig segnalati usandolo** (Franco, 2026-08-16, mentre
  costruiva il primo personaggio). Non affrontati subito per sua indicazione
  («pero' ora chiudiamo quello che manca sul lipsync»), ma registrati perche'
  vengono da uso vero, non da ispezione:
  1. **Lo sculpt non crea la correttiva su un braccio.** Parole sue: «non mi
     funziona lo sculpt su un braccio non riesco a fare la correttiva». Da
     riprodurre: quale osso, se succede anche su altri arti, se la correttiva
     non nasce o nasce e non si vede nella traccia in gradi.
  2. **`Show SO` resta acceso spegnendo `Order`.** Attivando Order si accende
     Show SO (voluto), ma disattivando Order non si rispegne. E' uno stato che
     si accende in coppia e si spegne da solo: il classico toggle che ricorda
     di accendere e non di ricordare com'era prima.

- **ZtoRig — FERMATO IN PAUSA** (2026-08-14, dopo il collaudo della traccia in
  gradi): *«le correttive impostate cosi' vanno bene [...] riguardo la parte
  ZtoRig mi fermerei un attimo visto che e' piuttosto laboriosa, ma gia' cosi'
  abbiamo degli strumenti utilissimi»*. **Non e' abbandonato, e' in pausa**: si
  riprende quando lo dice lui. Due cose gia' decise, da fare quando si riapre:
  - **evidenziare meglio il diamante della posa che si sta editando** (piccola,
    UI). Vedi la grammatica del diamante in `ztorykeydiamond.h`.
  - **modalita' rig vs modalita' animazione**: in modalita' rig queste
    operazioni **non devono generare chiavi di animazione** — sono modifiche al
    rig del personaggio, come la modalita' «build skeleton». Riferimento
    esplicito di Franco: anche Harmony ha una modalita' Animate e una in cui non
    anima davvero. Stessa direzione gia' registrata il 2026-08-14b (Sculpt e
    Order fuori da Animate).
  - Resta valido, per quando si riapre: il **taglio automatico sulla giuntura**,
    la strada emersa dal disco parcheggiato (memoria
    `project_ztorig_joint_disc`).


---

## 🎬 Ztoryc — storyboard, animatic, thumbs

### 🆕 2026-09-25 — aperti dalla sessione (da decidere / da osservare)

- **Recupero dopo crash — i punti rimandati della review** (`~/ZtorYc/reviews/2026-09-25b_reviewer.md`):
  P2 restore che rimette un livello piu' vecchio di uno salvato a parte; P3 snapshot con livelli
  falliti che sostituisce quello buono + sostituzione non atomica; P4 `replaceFile` che toglie
  l'originale prima di sapere se la copia riesce; M1 marker/startRow nello snapshot; M2 palette di
  scena e frame in piu' al restore; M3 cartella visibile su Windows, pulizia di `kept/`/`replaced/`;
  M7 snapshot ripetuto senza cambiamenti. B1, B2, P1 gia' corretti.
- **Tendina personaggi** (P5): la ricarica del DB puo' perdere un asset aggiunto dal lip sync e non
  salvato (`ztorylipsyncdialog.cpp:162` senza `saveProjectDb`); manca `productionReloaded`.
- **Bocche** (P6 a/b): l'autosave di Tahoma fa aprire da sola la finestra del nome del set; serve
  una guardia di rientro e nessuna finestra dentro `onSceneSaved`.
- **Fill Up** (M4): legge ⌘ anche quando il comando parte da una scorciatoia utente con ⌘.
- **Link anyway** (M5): lascia due asset sulla stessa scena.

- **Repeat con Loop anche su celle+chiavi** — PROPOSTA, non decisa. Oggi il Loop
  c'e' solo sul Repeat di SOLE chiavi (`83882cc3d`, 15/06): non era una
  regressione. Con keys-follow il ciclo continuo si ottiene selezionando un
  fotogramma in meno. Proposta: «l'ultima riga del blocco e' la prima della
  ripetizione dopo», per celle e chiavi insieme.
- **Intercalazioni strane tra chiavi (colonna mesh di FATINA)** — DA OSSERVARE:
  scena vecchia, chiavi buttate. Se ricapita: colonna, chiavi, cosa si vede, e
  salvare prima di cancellare. Forse lo stesso «sussulto fra chiavi» di ZtoRig.

### UX (bassa priorità) — Camera-view editing difficile da controllare

**Priorità: BASSA | Tipo: UX | Segnalato: 2026-05-30**

Comportamento storico di Toonz: stando in **camera view** e provando a modificare
la camera (Animate tool su colonna camera), le modifiche sono quasi impossibili da
controllare — trascinando si muove/zooma la *vista* invece dell'oggetto camera, e i
maniglioni di trasformazione sono ambigui rispetto al frame della camera stessa.

Non è un crash, è un limite di design del tool Animate quando il target è la camera
e si è già nel sistema di riferimento camera.

**Possibili approcci (da valutare):**
- Modalità/toggle "Edit Camera" che disabilita il pan-vista mentre si trascina la
  camera, o inverte la mappatura (drag = muovi camera, non vista).
- Maniglioni dedicati per la camera con feedback chiaro (frame + handle distinti).
- Eventuale gizmo camera custom (come per le annotazioni task 40).

**File (presumibili):** `sceneviewer.cpp` (gestione drag in camera mode),
`edittool.cpp` / `tool.cpp` (Animate tool su camera column).

---

### NEW — Room TRADITIONAL (task 38)

**Priorità: MEDIA | Tipo: NEW**

Workflow tradizionale: Import scansioni → Cleanup → Ink & Paint.
Note: GTS (scanner TWAIN) è solo Windows e separato da Tahoma2D; su macOS la
scansione è esterna. Room dedicata con Cleanup + Xsheet + Viewer + Style Editor.
Pulsante "Import Shot Frames" nel BOARD per caricare immagini scannerizzate
nella sub-scene corretta.

**File:** `mainwindow.cpp` (nuova room), `storyboardpanel.h/.cpp` (pulsante import).

> ✅ **DECISO da Franco il 2026-09-24: si fa, entra nella 1.0 (road map B12).**
> Modello: le room di OpenToonz (Drawing / **Cleanup** / InknPaint…). Deve avere
> la **scansione dall'app** e il **cleanup del segno**.
>
> Cosa c'e' davvero nel codice (verificato il 2026-09-24):
> - **Il cleanup c'e' tutto** (cleanupsettingspane/popup/preview, autocentratura
>   sui fori `autopos.cpp`): manca solo una room che lo metta in vista. La room
>   Cleanup di OpenToonz (`layouts/rooms/OpenToonz/room2.ini`) e' CleanupSettings
>   + Viewer + Xsheet + ToolBar + ToolOptions.
> - **La scansione NON c'e'**: non e' rotta, e' stata **tolta da Tahoma2D** nel
>   2020 (`5957d4d67`, «Remove Scan Stuff»: TWAIN, Epson USB, TScanner, i
>   comandi e i menu). OpenToonz ce l'ha ancora (`tnzbase/tscanner/`).
> - Il codice di OpenToonz parla **TWAIN** (su Windows funziona ancora) e
>   **Epson via USB diretto** (solo vecchi modelli professionali). Sul Mac di
>   Franco lo scanner passa da **Image Capture** (`EPSON Scanner.app` in
>   `/Library/Image Capture/Devices`) e **non ci sono sorgenti TWAIN** → il
>   TWAIN di OpenToonz sul Mac non troverebbe niente. Sul Mac la strada e'
>   **ImageCaptureCore** (`ICScannerDevice`); su Windows il TWAIN di OpenToonz;
>   su Linux SANE (o nessuna scansione nella 1.0).
> - Alternativa unica per i tre sistemi: **eSCL/AirScan** (HTTP in rete), che
>   quasi tutti gli scanner di rete recenti parlano — ma esclude quelli USB.
> - **Lo scanner di Franco e' una multifunzione HP in wifi**: sul Mac e'
>   registrata come `HP ENVY 4520 series` (stampa via `_ipps._tcp`). E' del
>   tipo che parla eSCL → **strada scelta: client eSCL in Qt**, stesso codice sui
>   tre sistemi (GET `/eSCL/ScannerCapabilities`, POST `/eSCL/ScanJobs`, GET
>   `NextDocument`). Scoperta via Bonjour `_uscan._tcp`, con l'indirizzo IP a
>   mano come ripiego (su Windows Bonjour non e' garantito). Il 24/09 la
>   stampante era spenta: **il supporto eSCL va confermato** con una GET delle
>   capabilities appena e' accesa. Piano piatto A4, un foglio alla volta.
> - ⚠️ **Esiste gia' una specifica, di luglio: `SamDrive/Ztoryc/SCAN_MODULE.md`**
>   (Fase 1 solo macOS, **ImageCaptureCore**, soglia B/N con anteprima, import
>   nella cella; dice esplicitamente di NON riciclare il `tscanner` di
>   OpenToonz). Scoperta DOPO l'analisi qui sopra. Le due strade non si
>   escludono: sul Mac ImageCaptureCore vede anche l'HP in wifi (via AirScan)
>   e l'Epson USB; eSCL servirebbe a Windows e Linux. Da decidere quando si
>   riprende la room.

---

---

### 🟡 FATTO 2026-09-24, DA COLLAUDARE — parenthetical dentro la battuta esclusi dal lip sync

Domanda di Franco. Gia' gestiti: parenthetical su riga propria (saltato) ed
estensione sul nome (`MARIO (V.O.)`). Mancava quello DENTRO la battuta
(`MARIO: (ride) Non ci credo`): ora `parseDialogue` toglie `(...)` dal testo
di ogni battuta prima dell'allineamento.

### 🎞️ CONFORM DALL'EDITORIAL — valutato fattibile (2026-08-16)

> Franco, confrontando con Storyboard Pro: *«il pezzo importante e' il conform
> ma per come si lavora in ztoryc la vedo difficile»*. Valutato sul codice: e'
> **fattibile, e meno difficile di come suona**, perche' i pezzi ci sono gia'.

**Cosa esiste gia':**
- **l'ancoraggio d'identita'**: l'export FCPXML scrive `name` su ogni
  `asset-clip` (storyboardpanel.cpp ~6587), e `shotLabel` e' un identificatore
  stabile («primary, v4+» in ztorymodel.h);
- **la durata E' gia' «celle nel main xsheet»**, quindi cambiarla e'
  un'operazione che il programma fa gia' (`onShotDurationChanged`,
  `resequenceXsheet`);
- **riordino, taglio e fusione** esistono come primitive con undo.

Quindi il conform non e' un motore nuovo: e' **leggere l'XML, confrontare, e
applicare con cio' che c'e'**.

**La parte difficile e' semantica, non tecnica**: cosa fare di cio' che il
montatore ha fatto e che nello storyboard non ha corrispettivo — clip nuovi
(non si puo' inventare un pannello), ritempi DENTRO uno shot, transizioni
cambiate. Per questo va fatto come **differenza da approvare**, non
applicazione automatica: «SH020 48→36, SH030 rimosso, ordine cambiato».
Stesso principio con cui l'apply del lip sync elenca i conflitti invece di
sceglierli.

⚠️ **DA PROVARE PRIMA DI SCRIVERE UNA RIGA** (dieci minuti): esportare, importare
in DaVinci, riesportare XML e guardare se `SH020` e' ancora nel nome del clip.
Se DaVinci rinomina o perde il nome, l'aggancio salta e tutto il resto non
serve. Quella prova decide se il conform e' un lavoro o un problema.

---

### 🚪 TOGLIERE L'OBBLIGO DI USCIRE DALLO SHOT

> *«deve essere gestita dal programma senza obbligare ad uscire dallo shot»*
> (Franco, 2026-08-16). ✅ **DECISO il 2026-08-17: si fa, ma IN CODA A TUTTO.**

Ne e' rimasto **UNO solo**: `ztoryanimatic.cpp` ~6270, eliminazione di una
traccia audio mentre si e' dentro uno shot.

> «Exit the shot (Back to Animatic) to delete an audio track.»

⚠️ **STIMA CORRETTA il 2026-08-17 — NON e' piccola**, come era stata valutata il
giorno prima. Guardando il codice:

- `deleteColumnsWithoutUndo()` prende l'xsheet da `TApp::getCurrentXsheet()`, e
  non e' una riga isolata: cancellare una colonna smonta anche gli **FX
  collegati** e la voce nel **peg tree**. Un undo scritto da noi vorrebbe dire
  riscrivere `DeleteColumnsUndo`, che e' una classe intera.
- Scambiare temporaneamente l'xsheet corrente NON e' indolore:
  `TXsheetHandle::setXsheet()` **emette `xsheetSwitched` e invalida le
  texture**. Due scambi = due ricostruzioni di tutti i pannelli, cioe' il costo
  che il 2026-08-16 e' stato tolto a fatica.
- 🎯 **E si capisce perche' quel messaggio esiste**: anche `DeleteColumnsUndo`
  risolve l'xsheet da `TApp` **al momento dell'undo**. Annullare la
  cancellazione di una colonna dopo essere entrati in una sotto-scena la
  ripristinerebbe nell'xsheet sbagliato. Quel guard non e' pigrizia: qualcuno
  ci e' finito dentro e si e' difeso.

**Strada scelta (A)**: rendere i comandi colonna **consapevoli dell'xsheet**,
comando e undo. E' la correzione giusta, vale anche per l'undo, ed e'
**candidato PR upstream** — il difetto e' di Tahoma/OpenToonz, non nostro.
Vedi `UPSTREAM_PR_CANDIDATES.md`.

---

### 🎥 VALUTATO E RIMANDATO — AV1 e l'ffmpeg del 2020

Franco ha chiesto (2026-08-04) se dalle specifiche AV1 di aomedia esca qualcosa
di utile. **Dalla pagina no**: sono definizioni di codec (bitstream, binding
ISOBMFF, payload RTP, HDR10+), roba per chi scrive encoder.

L'unica idea sensata sarebbe **AV1 come formato di uscita** per gli animatic
(30-50% piu' leggeri a parita' di qualita', royalty-free). Ma:
**l'ffmpeg che distribuiamo e' del 2020** (`N-99076`, copyright fino al 2020) e
**non ha alcun encoder AV1** — verificato con `ffmpeg -encoders`, l'unica
corrispondenza e' `wmav1`, che e' audio.

Quindi il costo non e' nel nostro codice (una voce in piu' nella lista formati)
ma nel **sostituire il binario ffmpeg in tre bundle**, e quel binario legge e
scrive TUTTI i video, importazione compresa.

**Decisione: non ora.** Se un giorno si aggiorna ffmpeg, la modifica che si
ripaga non e' AV1 ma [[project_video_import_slow]] — l'importazione che estrae
tutti i frame su disco nel thread dell'interfaccia. Quella fa perdere tempo a
ogni import; AV1 farebbe risparmiare megabyte una volta a consegna. Aggiornando
ffmpeg, AV1 arriva quasi gratis nello stesso giro.

### 🔮 Famiglia futura: GENERATORI di keyframe (overshoot, bounce, shake cam)
Idea di Franco, 2026-08-04. Sono **una feature a se'**, non dei preset:
- **Overshoot** — il valore supera il bersaglio e ci rientra (una posa che non
  si raggiunge ma si oltrepassa: e' la differenza fra meccanico e con peso).
- **Bounce** — la stessa cosa ripetuta e **smorzata**: supera, torna, risupera
  di meno, si posa.
- **Shake cam** — aggiunta da Franco: tremolio di camera.

**Perche' non sono preset**: un segmento fra due chiavi e' una singola bezier
cubica, puo' fare UNA gobba. Un'oscillazione smorzata richiede **piu' chiavi con
ampiezza calante**, cioe' vanno GENERATE. Gli altri preset scrivono due numeri su
un segmento che esiste gia'; questi cambiano quante chiavi ci sono.

**Avranno dei settings** (intuizione di Franco, ed e' giusta): durata, ampiezza,
numero di oscillazioni, smorzamento. Quindi servono un dialogo e delle scelte:
dove finisce l'oscillazione se dopo c'e' un'altra chiave? spostando la chiave
finale, le chiavi generate seguono o restano? il Rove che fa su chiavi non messe
dall'utente? Probabilmente la risposta e' la stessa data al Rove — **one-shot**,
nessun flag serializzato — ma va deciso e collaudato a parte.

✅ **FUNCTION EDITOR COMPLETO — 2026-08-04** (commit `9b8e73921`). Tutte e tre
le voci escluse dalla prima fase sono chiuse e collaudate da Franco: preset di
easing, **speed graph** in sola lettura, **curve linkate**.

Decisioni di progetto prese con Franco, da non ribaltare senza motivo:
- **Speed graph**: riquadro sotto ad asse tempo condiviso (non sovrapposto — lo
  screenshot di una scena vera con sei curve ciano ha mostrato che sovrapporre
  le derivate sarebbe stato illeggibile); **solo curve selezionate**, come
  «only show selected» di Blender; nessun asse verticale etichettato.
- **Curve linkate**: il flusso parte dalle **guide**, non dalla curva da
  pilotare — quella spesso non esiste ancora e non si potrebbe selezionare
  (difetto trovato da Franco al primo collaudo). Canali abbinati **per nome**,
  bersaglio = elenco di **colonne** a scelta multipla. L'intervallo lo decide la
  **selezione**, perche' guidare un tratto e animare a mano il resto e'
  legittimo.
- **Niente trascinamento** dall'albero: e' un gesto uno-a-uno e il modello e'
  molti-a-molti. Scartato da Franco: «per come funziona non avrebbe senso».
- **Niente dialogo di modifica** del collegamento: l'espressione e' gia'
  visibile e modificabile selezionando la curva guidata. Sarebbe stata una
  seconda strada peggiore per la stessa cosa.
- **Marcatori nell'albero**: freccia + colore + corsivo. La freccia perche' un
  canale puo' essere guida E guidato insieme (catena) e il caso misto si mostra
  da se'; il colore serve a trovare la riga scorrendo, non a dire cosa fa.

⬜ ~~Restano DUE voci~~ **(storico, ora chiuse)** delle tre escluse dalla prima fase (i **preset di easing** sono ✅ **FATTI e collaudati il 2026-08-04**, commit `07e6528f7`: quindici curve nominate Sine/Quad/Cubic/Quart/Expo x In/Out/In-Out nel menu contestuale del grafico). Nessuna e' bloccata dal
modello dati — verificato leggendo il codice, la nota iniziale che diceva il
contrario era sbagliata:
- **Speed graph** — vista della derivata, dove gli errori di spacing si vedono
  (nel valore no). In **sola lettura** e' contenuto e vale il 90% del beneficio;
  renderlo editabile e' un progetto a se', perche' un punto spostato nello spazio
  della velocita' va reintegrato in quello del valore.
- **Funzioni linkate** — una curva che pilota piu' parametri con un offset.
  **Gia' possibile oggi**: `TDoubleKeyframe::Expression` + grammatica che
  referenzia altre colonne + rilevamento dei riferimenti circolari, e
  `Channel::getExprRefName()` e' gia' il payload del trascinamento col tasto
  centrale dall'albero. Manca solo la UI: un gesto invece di sintassi digitata.
- ✅ **Preset di easing — FATTO 2026-08-04.** Si sdoppiava, e infatti e' stata fatta solo la meta' facile. Le **forme di ease** sono valori di
  `m_speedIn`/`m_speedOut` su un segmento e viaggiano sulla macchina di
  applicazione in blocco gia' costruita: poche ore. **Overshoot e bounce no**:
  richiedono di GENERARE keyframe, ed e' una feature diversa e piu' grande.

Ordine consigliato per valore/rischio: preset di ease, speed graph in sola
lettura, funzioni linkate.

**🔧 IN LAVORAZIONE — ZtoRig pose-blend (task 59 + correttive di giuntura).**
⚠️ **Worktree separato**: `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d-superplastic`,
branch `feature/ztorig-pose-blend`. **Il bundle da lanciare e deployare è
`Ztoryc-SP.app`, NON `Ztoryc.app`** (quello è master e NON contiene ZtoRig).
Master resta releasabile: il lavoro non va lì.

Stato al 2026-07-26d — il motore c'è (pose assolute/offset, stamping delle chiavi
plastic sull'xsheet, correttive di giuntura milestone 1/3). Franco ha collaudato
il rework finale dello slider auto-keying (l'unica parte mai verificata a mano):

- ✅ **Offset che schizzava via — RISOLTO** (`422461463`, verificato).
- ✅ **Gli altri slider non si azzerano — RISOLTO** (2026-07-26c, verificato). La forza
  ora è REGISTRATA nella curva guida invece che dedotta. Trappola trovata strada
  facendo: `m_guide` alimentava ancora il *blend* a runtime, quindi il fix proposto
  avrebbe applicato la posa due volte → blend rimosso (era vestigiale).
- ✅ **Pose di PERSONAGGIO, non di colonna** (2026-07-26c, verificato). Sparivano
  cambiando colonna sul rig esploso. `characterParts()` usa la stessa risalita di
  `PlasticTool::characterColumns()`; Record scrive su tutte le colonne, le operazioni
  si propagano per nome, un solo undo per gesto.
- ✅ **Posa Base** (2026-07-26c, verificato). Su un rig esploso il riposo vero è il
  disassemblato: si marca un'azione come Base e lo stamping interpola da lì.
- ✅ **Modalità `Part`** (2026-07-26c). Richiamo esatto sui soli parametri registrati —
  per fonemi e pose per-arto. `Offset` rinominato `Add`.
- ✅ **IK spento spegne davvero i pin** (2026-07-26c, verificato). `pinsEnabled` non era
  guardato da NESSUNA parte nella valutazione.
- ✅ **Angle bounds: il gizmo non creava mai la prima chiave** (2026-07-26c). Il ramo
  animato era irraggiungibile. Ora chiavia sempre → i bound seguono anche i livelli.
- ✅ **Multi-pin — MIGLIORATO molto** (2026-07-26d). A/B fatto: non era una regressione,
  era preesistente. Causa vera: `solveMultiAnchor` (drag) non clampa ai limiti d'angolo,
  `plant()` (valutazione) sì → il pin secondario mollava appena un arto aveva bound. Ora
  i limiti cedono al pin; più «il corpo resiste» (bisezione di fattibilità nel drag).
  Residuo peggiore da 10.4% a ~1.5% della diagonale del rig. Nuovo slider **IK Max Step**
  (1-90 gradi/evento, default 15).
- 📐 **Anche e spalle — LE MISURE.** Lo *stato* di questa voce sta in «Aperti al
  2026-08-02», dove il 2/8 Franco l'ha ricaratterizzata ancora: succede **solo su
  animazioni vecchie**, il che sposta il sospetto dal solver al dato già in scena. Qui
  sotto restano le misure del 27/7, che valgono comunque e che nessuno deve rifare.

  RICARATTERIZZATO
  2026-07-27 **con misure**, la diagnosi precedente era sbagliata. Franco: il controllo
  delle anche è ORA BUONO (dopo «il corpo resiste» + IK Max Step del 26d) e va
  **conservato**: NON fare la riscrittura «leva = cursore» a tappeto. Resta che
  «a volte basta poco e il personaggio scatta di colpo».

  **Misura** (build con `[IK_FEASIBLE]`, `lib_gino` rig single level, frame 23, pin sui
  due talloni, anca sx = `v=1`, anca dx = `v=5`, ramo `multiAnchor`):

  | | bersaglio | mouse | movimento |
  |---|---|---|---|
  | v=1, 1° evento | 9.93 | 9.93 | **204.77** (20x), poi il giunto si PIANTA |
  | v=5, metà corsa | 114 | 164 | **360** |
  | v=5, metà corsa | 87 | 486 | **359** — ma 90 → 16 |

  **ESCLUSO con misura** (non per deduzione): NON è la bisezione di fattibilità né i
  limiti d'angolo — `accepted` medio 0.995, 45 eventi su 46 al valore pieno, nessuno
  sotto 0.1: la bisezione non contratta quasi mai. Non è il Distance, non sono i bound
  semiaperti (il codice ripiega correttamente sul limite statico, `plastictool_animate.cpp:2034`).

  **Dove sta**: l'amplificazione è FRA il bersaglio e la posa risolta, con il vincolo
  pienamente soddisfatto. **La posa risolta non è funzione continua del bersaglio**:
  bersagli vicini → pose lontane (90→16, 87→359). Firma di un solver con più bacini di
  convergenza (FABRIK dentro `solveMultiAnchor`), non di un anello di retroazione.

  **Prossimo passo**: strumentare DENTRO `solveMultiAnchor`/`poseAt` — confrontare il
  bersaglio `t` con `P[v]` risolto e vedere se passate FABRIK vicine convergono a
  configurazioni diverse. Serve una build.

  **Strumenti già in codice** (non sono fix, non vanno in release): `ZTORYC_NO_ANGLE_CLAMP`
  spegne il clamp dei limiti (keyed E statici) per A/B; `[IK_FEASIBLE]` logga quanta parte
  del passo sopravvive alla bisezione. **Sono su master** (ci sono arrivati con il merge
  di ZtoRig, non col merge del 3/8): innocui a variabile spenta, ma restano due
  interruttori di debug in una build che si rilascia — da togliere quando la voce si
  chiude. I cinque membri `m_ikSweep*` che erano segnati come inutilizzati **non ci sono
  più**: verificato il 2026-08-03, quella pulizia è già stata fatta.
- ✅ **Angle bounds che risentono della rotazione del padre** (solo multi-colonna) —
  **RISOLTO 2026-07-27, verificato da Franco** («ora questa cosa è perfetta»).
  **Non era un problema di limiti: era il PIAZZAMENTO.** Il parenting a un hook portava
  la POSIZIONE del vertice ma non l'orientamento del suo osso, quindi piegando il busto
  la colonna del braccio non ruotava — e il braccio finiva fuori da bound che invece
  seguivano il corpo. Fix in `TStageObject::computeLocalPlacement` (`tstageobject.cpp`
  ~1884): un figlio agganciato a un vertice di mesh **plastica** eredita anche di quanto
  l'osso di quel vertice ha ruotato dal riposo → `makeRotation(ang + hookAng)`. Guardia
  stretta: un hook ordinario su un disegno normale non ha osso né deformazione e si
  comporta esattamente come prima. Interruttore A/B: `ZTORYC_NO_HOOK_ROT`.
  **Candidato upstream** (è comportamento storico di Toonz, non un bug): vedi
  UPSTREAM_PR_CANDIDATES.md, sezione feature request.

  **Due vicoli ciechi, annotati perché costano ore a chi li ripercorre:**
  1. `parentColumnRefDirs_animate()` cablato nel clamp — il vecchio TODO lo indicava come
     la cura. È la cura sbagliata: una volta che la colonna ruota, lo spazio locale ruota
     con lei e osso/bound/ventaglio seguono da soli. Sommare anche uno scostamento ai
     limiti **conta la rotazione due volte** (sintomo: i bound «si modificano leggermente»
     e compaiono linee con valori diversi da quelli del gizmo). Scritto e ritirato in
     giornata.
  2. Il TODO diceva di strumentare `writeBackAnglesFor_animate`. Misurato: per il drag di
     un giunto non-IK **non viene mai chiamata** (0 righe su 52 eventi). Il clamp che conta
     è `PlasticSkeletonDeformation::updateAngle`. E il range misurato era **identico** nelle
     due pose del busto (-95.38 in entrambe): il range non è mai stato il difetto.
- ✅ **Riattivando l'IK il personaggio salta — RISOLTO** (`fbafaeee5`, mergiato su master
  il 2026-08-03). Uscire dall'IK fa il bake in FK e molla i pin, ma i target di scena
  (PINWX/PINWY) catturati prima restavano indietro: descrivevano dove stava il piede
  quando venne piantato, cioè una posa che il bake aveva già assorbito. Rientrando, il
  primo solve trascinava il personaggio su quel bersaglio stantio. Ora rientrando si
  **ri-piantano i pin ATTIVI dove il personaggio sta in quel momento**, con lo stesso
  identico calcolo di `togglePinAtCurrentFrame` (per non avere due modi di catturare un
  bersaglio che un giorno divergono) e solo per i pin già attivi al frame corrente
  (riaccendere l'IK non deve inventare vincoli che non c'erano).
- ✅ **Correttive di giuntura, milestone 2 (authoring) — FATTA** (`d32e6c5ea` +
  `a9263e0a2` + `960a856e9`, mergiati il 2026-08-03). Il pennello misura la distanza
  **lungo la maglia** (BFS su `buildDistances`) e non a schermo, così col gomito piegato
  lavora su ciò che tocca invece che su ciò che copre; selezione multipla dei giunti con
  shift+clic; lo stacking order si assegna a tutta la selezione in un solo undo.
  ⚠️ Trappola pagata due volte: `buildDistances` scrive **solo** i vertici che la BFS
  visita, quindi con l'array a zero le isole staccate (braccia, gambe) restavano a
  distanza 0 — cioè più vicine di tutto. **Non raggiunto deve voler dire lontano.**

> **Le voci ancora aperte di ZtoRig non stanno più qui.** Vivevano in due posti che
> avevano già cominciato a divergere. Stanno tutte in **«Aperti al 2026-08-02»**, che è
> l'unica lista di stato; qui restano solo le prove e le misure, che è ciò per cui
> questa sezione serve.

**✅ FATTO — Import da carta nella Thumbnail room (task 63)**, completo e mergiato su
master il 2026-07-26 (`d7b7ff283`). Stampa foglio A4 (vuoto fotocopiabile o con i
thumbs), import da file multi-foglio e cattura da webcam, tutto verificato da Franco.
Dettagli nel CHANGELOG del 2026-07-26.

**🆕 NUOVO FILONE — Pose vettoriali / blend shape (avviato 2026-07-27).**
Primo test **funzionante e verificato**: due disegni vettoriali come estremi,
`TInbetween` come motore, slider a pilotare → «è una bocca eccome» (Franco).
Riquadro di test nel pannello ZtoRig, distruttivo e opt-in.

Architettura decisa: **NON serve un nuovo tipo di livello**. Una colonna con
deformazione Plastic espone già UN disegno e cambia forma via parametri
chiaviabili applicati al render — serve una **deformazione nuova sullo stage
object**. E il sistema di pose (Add/Pose/Part + curva di forza) **è già un
sistema di blend shape**: manca solo un secondo tipo di bersaglio, delta di
PUNTI per id di stroke accanto ai delta dei parametri.

Mattoni, in ordine:
1. **ID persistenti sui punti** — primo e non aggirabile. Inserimento a forma
   invariata via suddivisione di de Casteljau (stesso *t* su tutte le pose).
   Cancellazione = punto nascosto con peso 0, mai rimozione vera.
2. **Delta vettoriale dentro `PoseAction`**, accanto ai delta dei parametri.
3. **Sostituzione a render-time**, come le dissolvenze animatic (v0.8.0).

Da decidere prima di partire: stroke interi che compaiono/scompaiono, unione e
divisione di stroke, e che gli ID sopravvivano al salvataggio (entrano nel
formato file — la decisione più vincolante).
⚠️ Il vettoriale è **PLI**. TLV è raster colormappato.

**🆕🆕 POI — M5: Integrazione Kitsu [brainstorming 2026-06-26/27].**
Il prossimo grande filone. Tracking/review della pipeline via Kitsu (CGWire).
- **Client config-driven** (`KitsuClient`, QtNetwork + QJsonDocument): un solo URL+login,
  funziona identico su istanza locale docker, LAN, tunnel Cloudflare e **CGWire hosted**.
- **Modello sync = partizione di autorità** (NON bidirezionale campo-per-campo):
  Ztoryc autorità su struttura pre-produzione (shot/sequenze/timing/tecnica) → *push*;
  Kitsu autorità su review (WFA→DONE/RETAKE del supervisor) → *pull*. Conflitto vero
  ridotto a 1 campo (status stesso task) → last-write-wins su `updated_at`, Kitsu vince
  sugli stati di approvazione.
- **Upload-on-render**: opzione nei Render Settings → a render finito carica il filmato
  come preview sul task e fa **WIP→WFA**. Niente problema 100MB: upload su **endpoint locale**
  (LAN, salta Cloudflare); doppio URL `kitsu_local_url` / `kitsu_remote_url`. Proxy ffmpeg
  opzionale per review leggere da remoto.
- **Deployment-agnostico per altri utenti**: thin launcher sopra il docker-compose
  UFFICIALE CGWire (immagini pullate a runtime, no fork del deploy) come companion opzionale;
  oppure CGWire cloud (solo URL). Status/task_type **letti dal server**, mai hardcoded.
- **Fasi:** (1) login JWT + pull progetti/task_status reali + mappa status; (2) push shot
  list; (3) upload-on-render + sync status. **Partire dalla Fase 1** sull'istanza locale.

**🆕 DOPO KITSU — Export montaggio (DaVinci Resolve) [brainstorming 2026-06-27].**
Ultimo tassello della pipeline. L'animatic È già un rough edit (shot+timing+in/out+audio).
- **Via consigliata:** export **OTIO** (OpenTimelineIO, nativo in Resolve) o FCPXML/EDL —
  one-way Ztoryc→Resolve, portabile anche a Premiere/FCP. Round-trip solo se serve davvero.
- Alternativa "live": Resolve Scripting API (Python) per popolare la timeline con un click.
- Da verificare: cosa importa Resolve più pulito (OTIO vs FCPXML) + relink dei media.

**✅ FATTO — RILASCIATO in 0.6.3 (2026-06-27) — Production Tracker DI PROGETTO (roadmap A→B3d).**
Il tracker è ora un sottosistema di progetto completo (`production.ztrack`): shot list da
TUTTI gli storyboard del progetto con timing + task per-tecnica + status; Asset list, Team,
production data, **naming convention** e **Workflow** definibili/customizzabili. UUID v5
stabili shot+asset, **back-link nei .tnz esportati** (Fase A), pipeline status automatica
(export→READY, primo open→WIP), auto-workflow detection per tecnica, badge SB sugli
storyboard. Restano per il design doc completo: **export-to-AI per animatix** (proiezione
non ancora implementata) e l'integrazione **Kitsu** (M5, sopra). Vedi `DESIGN_production_tracker.md`.

**✅ FATTO — RILASCIATO in 0.6.3 — Export to worksheet (XLSX di progetto).**
`exportFullProject` (QXlsx): un .xlsx con tutti gli storyboard + tutti i tab (Project /
Overview / per-tecnica / Team / Assets / Workflows), status colorati Kitsu + dropdown.
Anche export per-scena sul Board ("Export Storyboard Spreadsheet").

**✅ FATTO — RILASCIATO in 0.6.2 (2026-06-23) — Thumbnail panel (sketch grid → export to board).**
Pannello (non ancora una "room") con griglia di panel su un raster contiguo MyPaint:
export-to-board (ritaglio panel → livello OVL multi-frame + sotto-scena + shot reale) +
shrink; persistenza per scena; **merge panoramiche** (merge esplicito di panel adiacenti, no
slicer geometrico); **transform tool** (move/copy/scale/rotate + lazo); **undo/redo**;
zoom-rotella + scrollbar + cursore pennello; griglia 4x4 default.
Aperti (prossime release): tasto Canc nudo (focus), icone Lucide/Phosphor — vedi
[[project_thumbnail_room_fase3]].

**✅ FATTO — Task 54: Custom logo nel PDF storyboard.** Campo UI + resolve path + render
(`painter.drawPixmap(...logoPixmap)` in `storyboardpanel.cpp`).

**✅ FATTO (auto-return) — Task 53: Shot ops in edit-shot mode.** Copy/Clone/Cut/Paste/Delete
da dentro una sub-scena funzionano: `onCopyShot()` & co. risalgono al main xsheet
(`while getAncestorCount()>0: MI_CloseChild`) prima di operare. Se in futuro serve l'operazione
**in-place** (senza uscire dalla sub-scena), è un raffinamento separato.

**✅ FATTO (2026-06-19, commit `d194149ad`) — Finalizzazione UI dedup: toolbar Board↔Animatic.**
Niente bottoni shot duplicati tra Board e Animatic nelle room Ztoryc. I comandi shot
condivisi vanno sulla toolbar della **timeline Animatic, parte SINISTRA** (così cadono
sotto il Board); i tool di editing a seguire a destra. Questo libera la toolbar del
**Board** per: menu auto/keep/renumber, numbering options, light arrow (+opzioni),
export PDF/scene/animatic (gli export forse anch'essi sull'Animatic). Motivo: il panel
Board può essere ristretto → troppe icone danno problemi.
- **Vincolo invariante:** ogni panel resta self-sufficient — una room custom col solo
  Board deve riavere TUTTI i bottoni. Quindi NON spostare i bottoni: ogni panel tiene la
  toolbar completa e nella room di default **nasconde i duplicati a runtime** se rileva il
  panel "owner" vicino (owner = Animatic).
- **Come:** enumerare i vicini con `currentRoom->findChildren<TPanel*>()` (room è un
  `TMainWindow`; pattern già usato in `floatingpanelcommand.cpp`, `mainwindow.cpp:1584`);
  su `showEvent`/cambio room il Board nasconde i bottoni condivisi se c'è un Animatic.
- **Complementare:** overflow "»" sulla toolbar Board (QToolBar extension o menu More) così
  da sola ristretta non perde mai bottoni.
- **Da decidere a inizio task:** lista canonica dei bottoni condivisi (un posto solo) +
  identificare i file room `.ini` di default (bundle + ~/Library). Logica shot già in
  `ztoryshotops` (dedup di logica fatto); questo è SOLO la parte UI/toolbar.

### ⌨️ Keys-cels — residuo aperto (resto della feature: FATTO, vedi DONE/archivio)

5. **Selezione combinata governata dal link "Keyframes Follow Exposure" [DESIGN, NUOVO].**
   Richiesta utente: con pref ON, *qualsiasi* selezione (incluso il drag sui diamanti)
   deve produrre una `TCellKeyframeSelection` (chiavi + celle sottostanti); con pref OFF
   selezione indipendente. Oggi metà già funziona (selezione CELLE → combinata); manca
   il verso selezione DIAMANTI → combinata quando pref ON. Cambio trasversale alla logica
   di selezione dell'xsheet (xsheetviewer/cell viewer mouse handling), impatta TUTTI i
   comandi combinati → da testare sull'intero repertorio. Priorità: valutare dopo dedup.

### 🔧 Aperti — investigare / bassa priorità

47. ✅ RISOLTO (verificato da Franco 2026-07-21) — Audio scrub meno reattivo dopo il merge.
    Lo scrub del viewer/xsheet normale e' tornato reattivo sul singolo frame; nessun
    intervento ulteriore necessario. (L'indagine A/B pre/post merge non serve piu'.)
41. NEW Cache RAM threshold configurabile (BASSA) — ora a 14.3% shipped in `tsystempd.cpp` (il tentativo di alzarlo al 25% è stato revertito perché l'eviction aggressiva crashava il Save All su scene pesanti, raster liberato durante `TRasterCodecLZO::compress`). Rifarlo in modo MIRATO: non toccare l'eviction globale durante i save; semmai rilevamento per classe di macchina (≤8GB→più aggressivo) + opzione utente. ⚠️ Collegato: cache-leak post-render (frame restano in cache, ~17GB su scena pesante; fix upstream `be20f9512` da portare).

Milestone:
- M2: In/Out Marker, Roll, Slide, Doppio Viewer, Export render
- M3: Quick-shot selector, Export PDF migliorato
- M4: Room REFERENCE (canvas PureRef-style)
- M5: Kitsu Integration (kitsu.ztoryc.org su Mac mini M4)

---
---

---

## 🦴 ZtoRig / personaggio

### 🆕 2026-09-25 — Edit mesh: taglio per percorso e cancellazione di un pezzo (richiesta di Franco)

- **Taglio**: clic sul vertice di partenza e su quello d'arrivo, con ANTEPRIMA
  del percorso di segmenti, poi Cut Mesh. Oggi la multi-selezione c'e' (⌘+clic,
  e dal 25/09 anche ⇧+clic; Cut Mesh su una catena) ma e' un clic per segmento.
  Franco (25/09): il Cut Mesh con la multi-selezione gli va bene cosi'; il taglio
  da vertice a vertice resta un'idea, non urgente.
- **Cancellare un insieme di vertici** = togliere un PEZZO di mesh lasciando il
  buco: serve per le sporcature del disegno finite nella mesh (Franco).
  Nell'edit mesh oggi non esiste nessuna cancellazione.
- Crash in `closestVertex` dopo un Cut Mesh su DOTTO (25/09): NON riprodotto
  sotto lldb; messa una protezione per le mesh vuote (`5cbf0265c`), non una
  correzione dimostrata. Se ricapita: il log nuovo.

> ⚠️ Il blocco «ORDINE DI PRIORITA'» qui sotto e' del 2026-08-16: l'ORDINE
> e' superato dalla ROADMAP; restano validi i progetti (libreria di pose,
> template di scheletro e retargeting).

### Aperti al 2026-08-02 (travasati dalla lista di sessione)

> ⭐ **QUESTA E' L'UNICA LISTA DI STATO.** Unificata il 2026-08-03: le stesse voci
> ZtoRig vivevano anche dentro Priority Order e le due copie avevano gia' cominciato a
> divergere (una diceva aperto cio' che l'altra dava per chiuso). Li' sono rimaste solo
> **le prove e le misure**, che e' a cosa serve quella sezione; qui c'e' cosa e' aperto.
> Chi chiude una voce la chiude **qui**.
>
> Diagnosi raccolte nelle sessioni del 27/7 e del 2/8. Scritte qui perche' la
> lista di lavoro vive nella sessione e sparisce con la chat.

**ZtoRig / IK**

- ⬜ **Il personaggio «parte» manipolando le anche — solo su animazioni VECCHIE.**
  Su chiavi fresche non succede (Franco, 2/8). Sposta il sospetto dal solver al
  DATO GIA' IN SCENA: bersagli dei pin catturati con regole diverse, o chiavi
  scritte quando il write-back aveva un'altra semantica. **Primo test**: stessa
  scena vecchia su 0.11.0 contro branch — se il sintomo c'e' su entrambe, la
  ri-cattura dei pin (`fbafaeee5`) e' innocente. Misurato in precedenza su scene
  nuove: non e' la bisezione (`accepted` medio 0.995) ne' i limiti d'angolo;
  l'amplificazione sta fra bersaglio e posa risolta, firma di piu' bacini di
  convergenza dentro `solveMultiAnchor`.
- ⬜ **Pin legati allo scheletro.** I parametri PIN sono condivisi per nome fra
  gli scheletri della colonna. Serve un campo esplicito in `SkVD` (come
  `m_skelIds` per le pose), con «pin vecchi = ovunque». Tentativo di dedurre lo
  scheletro dal frame di attivazione: ritirato, sbagliato.
- ⬜ **Il personaggio scivola registrando pose con i PIN.** Da indagare, zona
  dell'autorita' del planting. Diagnostica: `ZTORYC_PIN_DIAG`.
- ⬜ **Residuo multi-pin ~1.5%.** La bisezione del drag giudica con
  `solveMultiAnchor`, ma l'ultima parola ce l'ha il solve di personaggio in
  `TStageObject`. Esporre anche quello come query, come `plantPins()` /
  `pinResidualForPose()` per la singola colonna.
- ✅ **Chiudere un loop di camminata — RISOLTO il 2026-08-14, senza scrivere
  codice.** Franco: «ha funzionato con **Part**». La prova che era annotata come
  «da fare prima» ha chiuso la voce da sola: la modalita' `Part` richiama **solo
  i parametri registrati**, quindi copiando una posa non si porta dietro il
  PIAZZAMENTO e il personaggio non torna indietro.
  **Quindi il comando nuovo NON serve.** Il difetto di fondo resta vero e va
  conosciuto — `SkVD::POSE_PARAMS` (`plasticskeletondeformation.cpp:111`)
  mescola FORMA (ANGLE, DISTANCE, SO) e PIAZZAMENTO (ROOTX/Y, TRANS, ROT, SCALE,
  PIVOT, SHEAR), e con i pin il piazzamento non andrebbe copiato affatto perche'
  il pin porta la posizione e gli angoli la forma — ma in pratica `Part` lo
  aggira. Se un giorno servisse copiare pose **senza** passare da Part, il
  rimedio e' un comando che copi solo ANGLE/DISTANCE/SO fra due frame.
  ⚠️ **Da mettere nel manuale**: «per chiudere un ciclo, richiama la posa in
  modalita' Part» e' conoscenza d'uso, non di codice, e senza scriverla si
  riperde.
- ⬜ **Template di scheletri riusabili + registrazione di animazioni.**
  ✅ **CONFERMATO da Franco il 2026-08-03** — era ricostruito a memoria, ora e' una voce
  vera. Restano aperte le domande di progetto: cosa contiene il template (topologia?
  limiti d'angolo? rigidity? SO?); le correttive **NON sono trasferibili** (sono delta
  per indice di vertice della MAGLIA, che cambia da disegno a disegno); riapplicazione
  **per nome** (unica chiave stabile fra scheletri diversi) o per indice; dove vivono i
  template (accanto alla scena? nella libreria di progetto?). Le azioni di posa ZtoRig
  sono il precedente piu' vicino e vanno guardate per prime.
- 💡 **Colore della linea come chiave di corrispondenza** (idea di Franco,
  2026-08-03, da valutare). Se l'animatore disegna la linea del naso con uno
  STILE dedicato, quella linea si riconosce in tutte le viste senza pickerare
  niente e senza indovinare dalla geometria. Il suo esempio e' preciso: la linea
  del naso a volte cade a sinistra e a volte a destra, e nessun criterio
  posizionale la segue.

  **Perche' e' forte**: lo stile e' **gia' nel formato file** — ogni `TStroke`
  porta il suo `getStyle()` e il PLI lo salva, a differenza di `getId()` che e'
  un contatore a runtime (`++maxStrokeId`) e si riassegna al caricamento. Quindi
  non chiede niente al formato, che era il vincolo peggiore. Ed e' un gesto che
  gli animatori gia' fanno: le palette di lavorazione esistono.

  **Limiti da guardare in faccia**: uno stile identifica una CLASSE, non
  un'istanza — con due occhi dello stesso colore non si distingue destro da
  sinistro (o due stili, o una regola sul lato). Ed e' opt-in: vale solo se
  qualcuno ha colorato apposta. Come **ripiego** invece che come obbligo e'
  perfetto: dove il colore c'e' lo si usa, dove non c'e' si ricade sull'indice
  e sul picker.

- ⬜ **Correttive di giuntura, milestone 3 (UI).** La milestone 2 (authoring, il
  pennello) e' entrata su master il 2026-08-03.
  **2026-08-14**: su branch `feature/ztorig-correttive-ui` (`785b0860d`) c'e' il
  pannello ZtoRig a schede + la scheda Correttive come **traccia in gradi** (una
  corsia per giunto, una chiave per correttiva, clic per andare a quella piega,
  trascinamento per spostarla, tasto destro per cancellare). **Da collaudare.**
  L'idea della traccia e' di Franco, ed e' come funzionano gli Smart Bones di
  Moho. Il dato non e' cambiato per ottenerla: le correttive nascono gia'
  incatenate, quindi erano gia' chiavi su una traccia scritte come tabella.
  Resta fuori: creare una correttiva NUOVA dal modo di rigging, e la
  separazione di Sculpt/Order fuori da Animate (deciso con Franco: modellare e
  riggare non e' animare).

- ⏸️ **Disco rigido di articolazione — PARCHEGGIATO il 2026-08-14.**
  Branch `feature/ztorig-joint-disc` (`50eb4cd95`), preferenza spenta.
  Il gomito pizzica perche' un giunto e' **UN** punto di comando e l'ARAP deve
  far coesistere li' due rotazioni. Il disco (corona di punti sintetici, raggio
  = meta' larghezza dell'arto, rotazione sulla bisettrice) toglie il
  pizzicamento, si vede nel tool e si tara.
  ⚠️ **Ma non puo' dare il bersaglio**: piegando, all'interno i due segmenti si
  SOVRAPPONGONO, e una maglia unica non puo' sovrapporsi a se stessa — puo' solo
  accartocciarsi o aprire un buco. Provato a Joint Blend 0 e 100: cambia solo
  quale difetto prevale.
  **La strada giusta e' il TAGLIO automatico**, che era la prima idea di Franco:
  dove il disco incontra l'arto la maglia si sdoppia in due meta' con calotta
  circolare, i vertici della calotta restano condivisi (quindi non si separano
  mai) e le due meta' si sovrappongono. Tocca la topologia: sessione a se'.
  Dettagli e i sei errori da non ripetere: memoria `project_ztorig_joint_disc`.

**Altro**

- ⬜ **Save and Render fa partire DUE render** — candidato upstream. ESCLUSI: la
  tavoletta (succede anche col mouse) e l'handler (`onSaveAndRender` fa un solo
  `doRender`). **Biforcazione da risolvere**: il comando parte due volte, oppure
  una esecuzione produce due render? Un contatore all'ingresso di
  `RenderCommand::onSaveAndRender` e uno in `doRender` lo dicono in un clic.
  A parte: i bottoni di `outputsettingspopup.cpp` usano `pressed()` invece di
  `clicked()` — sbagliato comunque, non e' la causa qui.
- ⬜ **CRASH 0.11.0 chiudendo la finestra di cattura — NON riproducibile.**
  Log `Crash-20260727-222814.log`. Backtrace (simbolicazione approssimata, i
  frame 1 e 2 sono identici): `onSelectionChanged` → `storeDeformation` →
  `onColumnSwitched` → `onXsheetChanged` → `saveSceneIfNeeded` → `closeEvent`.
  GIA' ESCLUSI leggendo, tutti guardati: `storeDeformation`, `onSelectionChanged`,
  `rootVd_animate`, `skeletonId()`. **Discrepanza da tirare**: il backtrace dice
  `MainWindow::closeEvent` ma Franco aveva chiuso solo la finestra di cattura.
- ✅ **FATTO (blocco «Special thanks» in `aboutpopup.cpp`, aggiornato a ogni rilascio)** — ~~**Ringraziamento sponsor DENTRO l'app**~~ — generico, senza nomi (deciso da
  Franco 2/8; vuole prima vedere come lo fa Tahoma2D). Se un giorno si passa ai
  nomi, il consenso esplicito va chiesto: essere sponsor pubblico su GitHub non
  e' consenso a comparire nell'About.
- ⬜ **Script di shake camera** con lo scripting di Toonz — chiesto e mai fatto.

---

### 🔝 ORDINE DI PRIORITA' — rifatto da Franco il 2026-08-16

> *«La mia priorità è chiudere il discorso character, mi serve poter usare il
> nuovo lipsync, e vediamo se si riesce a creare un template di scheletro per
> riutilizzare le animazioni. Il resto a questo punto è secondario mi pare.»*

**A. CHIUDERE IL PERSONAGGIO** — e' questo il lavoro, il resto viene dopo.
   1. **MouthSet**, perche' e' cio' che rende il lipsync USABILE: la catena
      fonemi→colonne e' finita e collaudata, ma senza l'associazione
      bocche↔fonemi salvata sul personaggio resta un elenco di sigle. Progetto
      pronto nella sezione «IL PERSONAGGIO COME OGGETTO»: sidecar `.ztoryc` con
      `role="character"`, riferimento per NOME DI LIVELLO + fotogramma.
   2. **Libreria di pose + ritorno in libreria** — «pubblica in libreria» /
      «prendi dalla libreria», con rifiuto se incompatibile e v2 se la struttura
      diverge (decisione di Franco, vedi sezione «IL RITORNO IN LIBRERIA»).
   3. **Template di scheletro con proporzioni standard, e compensazione**
      (idea di Franco, 2026-08-16): *«creare un template di scheletro con delle
      proporzioni standard; quando viene riadattato sul personaggio, facendo la
      differenza dall'originale si dovrebbe riuscire a compensare le modifiche
      in modo che le animazioni funzionino comunque»*. E' il **retargeting**, ed
      e' la strada giusta.

      💡 **E' economico, e si vede dalla struttura dei dati.** I delta di una
      posa sono salvati per NOME DI VERTICE (`plasticskeletondeformation.h:289`)
      e in **coordinate polari rispetto al padre** (`:139`), non in posizioni
      assolute. Quindi si spezzano in tre nature con destini opposti:
      - **ANGLE** — un angolo. 30° sono 30° su qualunque personaggio:
        **si trasferisce invariato**, nessuna compensazione.
      - **DISTANCE** — una lunghezza. E' **l'UNICO** che rompe cambiando
        proporzioni, ed e' esattamente il limite dichiarato alla riga 330.
      - **SO, PIN, PINTX/PINTY** — passano invariati.

      La compensazione non e' quindi un motore di retargeting, e' **una
      moltiplicazione su un parametro solo**:
      ```
      distanza_bersaglio = distanza_template × (riposo_bersaglio ÷ riposo_template)
      ```

      **Tre verifiche prima di progettare in grande**, in ordine di rischio:
      1. i **nomi dei vertici** devono coincidere — il template e' prima di
         tutto un ELENCO DI NOMI;
      2. le **lunghezze a riposo** devono essere leggibili da entrambi gli
         scheletri: cercando `restLength` non ho trovato un accessore pronto, va
         verificato nel codice vero (potrebbe esserci con altro nome, o si
         ricava dalle posizioni a riposo);
      3. il rapporto va preso **PER OSSO, non globale**: un personaggio con
         gambe lunghe e braccia corte con un fattore unico verrebbe peggio che
         non compensando affatto.

      ⚠️ **Limite onesto**: questo non salva una posa in cui la mano TOCCA il
      fianco. Cambiando proporzioni il contatto si perde comunque, perche' il
      vincolo era geometrico e non angolare. La scala completa sarebbe: angoli
      gratis → distanze in proporzione → **contatti tenuti dai PIN** (che
      esistono gia': `PIN/PINTX/PINTY`, «l'ancoraggio resta piantato anche sugli
      intermedi»). I pin pero' sono la parte collaudata e messa in pausa: e' il
      terzo pezzo naturale, non un lavoro da aprire adesso.

**B. SECONDARIO da qui in poi** (era il Priority Order precedente):
   - **Deformatori** (punto 3 sotto). Stimato il 2026-08-16 in **2-3 sessioni**
     per il primo (Liquify, che serve a misurare l'impalcatura) e 1-2 per
     ciascuno degli altri tre. ⚠️ Scoperta del 2026-08-16: il Plastic e' gia'
     `bind(TTool::AllImages)`, quindi un deformatore ANIMABILE su tutti i tipi
     di livello **esiste gia'** — il buco e' il ritocco DIRETTO, senza mesh ne'
     rig. Nessuna sovrapposizione, l'inquadramento di Claudio era giusto.
   - **Assistenti al disegno da OpenToonz** — Franco li ha ricordati il
     2026-08-16 come ancora in sospeso. Restano sotto al blocco A.
   - 2.5D Cartoon Models (ricerca), auto-shadow (per ultimo, sua indicazione).

---

**Cosa e' invece VIVO dal 2026-08-14 (ordine dato da Franco)**:
1. ~~**Kitsu — legare il Production Tracker al singolo EPISODIO**~~ ✅ **FATTO
   il 2026-08-14** (`3775c1144`). Legame = coppia (progetto, episodio) per ID;
   filtro su shot e asset; team NON filtrato di proposito. Due bug chiusi per
   strada: 156 task su 591 scartati in silenzio, e i nomi del team invisibili
   *perche'* Franco e' admin (Zou non serializza `full_name` per gli admin).
   ⚠️ **Resta da verificare**: il conteggio shot per episodio e' stato provato
   con **un solo shot**. Gli asset entrati per errore invece sono **chiusi**:
   rimossi a mano da Franco, e il filtro impedisce che ne arrivino altri —
   **non proporre un ripulitore automatico.**
2. ✅ **LIPSYNC — L'ALLINEATORE FORZATO E' FATTO (2026-08-16).**
   **Vosk** ha sostituito Whisper nel ruolo di cronometro. Whisper resta per le
   lingue senza modello e per il rilevamento automatico. Misurato sull'audio
   vero di sh020 con riscontro sulle **fricative dello spettro** (la /s/ di
   «que*s*to», la /f/ di «fa»: eventi fisici, non l'opinione di un altro
   modello):

   | | scarto medio | peggiore |
   |---|---|---|
   | **Vosk small it (48 MB)** | **10 ms — 0,2 fotogrammi** | 30 ms |
   | Whisper base-q5_1 | 30 ms — 0,8 | 110 ms |
   | Whisper + DTW | 191 ms — 4,8 | 400 ms |

   Su 14 battute: Whisper 86 parole a durata ZERO su 263 e un'allucinazione
   (108 parole, coda a 22 s oltre la fine); Vosk zero degenerazioni su 158.

   ⚠️ **Correzione a quanto scritto il 2026-08-15**: `-dtw` non «non ha avuto
   effetto» per la quantizzazione — **non era mai stato eseguito**, perche' il
   flash attention e' attivo di default e lo disabilita stampando una riga nel
   log. Acceso davvero (`-nfa`), PEGGIORA.

   **La catena completa, com'e' adesso**:
   `Vosk` dice QUANDO cade la parola (10 ms) → `espeak-ng --ipa` dice DI COSA e'
   fatta (processo separato, GPL, mai linkato) → l'**ONDA** dice dove cade il
   suono dentro la parola.

   Fatto, in ordine di quanto e' costato scoprirlo:
   - **Il centro delle vocali si aggancia ai massimi di energia** (idea di
     Franco). Non pesare i fonemi con l'energia — provato e MISURATO inutile,
     perche' campionava l'onda nella posizione indovinata, ed e' la posizione a
     essere sbagliata. Vocali sul picco: da 4/13 a **10/13**.
   - **L'accento di espeak e' durata, non punteggiatura.** `fˈatʃile`: la ˈ
     sulla /a/. Lo scartavo coi separatori. Ora moltiplica il peso (×2,5
     primario, ×1,5 secondario).
   - **L'avanzo va un fotogramma alla volta a chi e' piu' sotto il proprio
     peso**, non per arrotondamento: con 6 fonemi e 3 fotogrammi d'avanzo
     l'arrotondamento ne dava al massimo uno a testa e la vocale accentata
     restava al minimo come le altre.
   - **Durata minima 2 fotogrammi imposta sulla COLONNA**, non parola per
     parola, con prelievo **a cascata** (il vicino immediato spesso e' gia' al
     minimo mentre c'e' spazio cinque celle piu' in la').
   - **Mai fondere due bocche uguali e visibili**: togliere la /e/ fra la P di
     «per» e la M di «me» sembra innocuo e fa leggere UNA tenuta dove devono
     esserci due colpi.
   - **Anticipo** 2 fotogrammi regolabile (Preferenze > Import/Export > Lip
     Sync) **+1 sulle labiali**, solo sull'attacco.
   - **Rest esplicito** nelle pause ≥ 2 fotogrammi; buchi piu' corti assorbiti
     dalla cella precedente (una cella vuota TIENE il disegno prima, non chiude
     la bocca).
   - **Due colonne** per personaggio: prima le parole, poi le bocche.

   🧪 **Il banco di prova sta in `reference/forced-align/`** e la verita' di
   riscontro sono le **fricative dello spettro**, non un altro modello:
   `check_align.py`, `sibilance.py`, `robustness.py`, `visemes.py`.

   **Resta da fare**: spostare la generazione delle colonne all'EXPORT, e
   imballare espeak-ng (oggi viene da Homebrew, come whisper-cli — se manca, le
   colonne tornano a contenere le parole intere invece di fallire).

   💡 **POSSIBILE SVILUPPO FUTURO — codici dei fonemi personalizzabili**
   (Franco, 2026-08-16: *«i fonemi potremmo anche farli customizzabili, molto
   spesso le produzioni hanno delle tavole con dei codici»* — poi: *«per adesso
   lasciamo cosi', segniamocela come possibile implementazione futura»*).
   Quando si riapre: i codici sono della **PRODUZIONE**, quindi vanno nel
   progetto come gli alias dei personaggi, non nelle preferenze
   dell'applicazione. Resta da chiarire se basta rinominare le dieci caselle o
   se le tavole vere hanno un numero diverso di bocche — Franco si era offerto
   di mostrarne una.

   Recap originale del 2026-08-14 piu' sotto.
   ✅ **WHISPER E' DECISO**, non e' piu' un'opzione da valutare. Franco: *«whisper
   lo voglio assolutamente visto che sara' utile anche per l'inglese, credo fara'
   la differenza in ogni situazione»*. Ha ragione anche sull'inglese: PocketSphinx
   e' tecnologia dei primi anni 2000, Whisper e' migliore in assoluto, non solo
   sulle lingue che l'altro non copre.

   Vincoli gia' accertati, da non riscoprire:
   - **Usare `whisper.cpp`** (C/C++, licenza **MIT**, nessuna dipendenza Python,
     Metal su Apple Silicon). La versione originale in Python non e'
     distribuibile dentro il bundle.
   - ⚠️ **espeak-ng e' GPL-3.0.** Serve per testo→fonemi, ma NON va linkato:
     va invocato come **processo separato**, esattamente come gia' si fa con
     Rhubarb e ffmpeg. Linkarlo contaminerebbe la BSD di Ztoryc — stessa
     trappola gia' incontrata con Krita e AnimeEffects.
   - I **timestamp per parola** non sono nativi in Whisper: si ricavano
     allineando i pesi di attenzione, ed e' la parte meno solida della catena.
   - Whisper **inventa testo** su silenzio e rumore. In un tool di lipsync, dove
     le pause contano, va gestito esplicitamente.
   - ✅ **DECISO (Franco, 2026-08-14): il modello NON va nel bundle.** *«Se
     pesano tanto dobbiamo prevedere che sia una scelta dell'utente scaricarlo o
     meno e in che versione»*. Quindi serve una UI di gestione modelli: scegli
     quale, scarichi, vedi quanto pesa, lo puoi togliere. Ztoryc deve funzionare
     senza nessun modello scaricato (lipsync col solo Rhubarb, come oggi).
   - ⭐ **Osservazione che puo' ridurre di molto il modello necessario**: se il
     testo lo diamo noi (`PanelData::dialog`), il lavoro non e' piu'
     *riconoscimento* ma **allineamento forzato** — sappiamo gia' cosa e' detto,
     serve solo sapere quando. E' un compito molto piu' facile, su cui un modello
     piccolo se la cava. Il modello grande serve solo quando il copione NON c'e'.
     Da misurare prima di imporre a tutti un download da qualche GB.
   - **WhisperX — l'idea si prende, il codice no.** Franco l'ha segnalato il
     2026-08-14 («esiste una versione whisper X che supporta i fonemi»). Non e'
     un modello diverso: e' una *pipeline* attorno a Whisper (gruppo di Oxford)
     con VAD prima (taglia il silenzio -> meno allucinazioni + piu' veloce),
     **allineamento forzato con un modello CTC** per lingua, e diarizzazione
     opzionale.
     ⚠️ Due precisazioni che cambiano le conclusioni:
     - il modello di allineamento serve a **datare**, non a produrre fonemi da
       mappare sui viseme: l'uscita sono parole/caratteri con tempi precisi. Il
       pezzo testo->fonemi resta comunque da fare.
     - **e' Python e tira dentro PyTorch**: svariati GB, impraticabile in un
       bundle .app firmato. NON e' la versione leggera — quella e' whisper.cpp,
       che e' un'altra cosa.
     **Cosa prendere**: l'idea che i timestamp nativi di Whisper sono la parte
     fragile e un allineatore forzato li batte. In C++: o l'opzione **DTW per
     token gia' presente in whisper.cpp**, o un allineatore CTC invocato come
     processo separato (schema Rhubarb/ffmpeg).
     ❓ **Da verificare prima di adottarlo**: licenza di WhisperX e dei modelli
     di allineamento (quelli di diarizzazione stanno dietro condizioni d'uso su
     HuggingFace). In questo progetto la licenza e' stata la sorpresa finale
     troppe volte — Krita, AnimeEffects, espeak-ng.
   - 👥 **I PERSONAGGI — chi parla, e con quale bocca** (Franco, 2026-08-14).
     Il problema vero non e' il lipsync di una battuta: e' la catena
     **chi parla -> quale audio -> quale testo -> quale livello animare**.

     **Due anelli su tre ci sono gia', senza indovinare niente:**
     - *Chi parla*: una **colonna audio per personaggio**. E' come lavora il
       montaggio del suono, e Ztoryc le colonne sonore le scorre gia'. Se
       l'audio arriva separato, chi parla e' un DATO, non una deduzione.
     - *Chi esiste*: i personaggi sono **gia' asset di tipo Character** (63 nel
       progetto di Franco) e da oggi si sincronizzano da Kitsu. La lista non va
       inventata.

     **L'anello mancante**: `PanelData` ha `dialog`, `action`, `notes` ma **non
     ha un campo personaggio**. Il testo c'e' ed e' anonimo. Aggiungere un
     riferimento all'asset Character e' la modifica che chiude la catena.

     **La diarizzazione e' un ripiego**, non il progetto: serve solo con traccia
     unica mixata e nessuna annotazione, e comunque restituisce SPEAKER_00 /
     SPEAKER_01 — qualcuno deve poi dire chi sono.

   - 👄 **MOUTH SET — assegnare i disegni ai fonemi UNA VOLTA SOLA** (idea di
     Franco, 2026-08-14, ed e' il pezzo di maggior valore pratico).
     *«I fonemi vanno associati a due set diversi di bocche (bocca in su o in
     giu') per ogni posizione. Potremmo fare in modo che tutto questo venga
     registrato e associato al personaggio, cosi' e' un'operazione veloce invece
     di stare ogni volta a riassegnare i disegni ai fonemi: lo si fa una volta e
     basta e poi si sceglie quale set usare (frontale o profilo, felice o
     triste).»*

     Struttura dati che ne segue:
     - un **MouthSet** = mappa viseme (A-H, Preston Blair — gia' quelli che
       usiamo, `--datUsePrestonBlair`) -> disegno specifico (livello + frame);
     - il MouthSet ha i suoi attributi: **vista** (frontale / profilo / 3-4),
       **espressione** (felice / triste), **variante** (bocca in su / in giu');
     - un **personaggio possiede PIU' MouthSet**, e al momento del lipsync si
       sceglie solo quale usare: tutto il resto e' gia' noto.

     **Dove va salvato**: sul PERSONAGGIO, non sullo shot — cosi' viaggia fra
     shot, episodi e produzioni. Il che lo rende **lo stesso problema della
     «libreria di rig riusabili»** (punto 4 delle priorita'): un personaggio
     porta con se' il suo rig, i suoi mouth set, i suoi asset. E' una
     *definizione di personaggio* che esiste una volta sola. Progettarli
     separati sarebbe farlo due volte.

     ⚠️ **Vincolo per il futuro**: oggi il lipsync scambia disegni in un
     livello. Con ZtoRig un personaggio riggato ha la bocca DENTRO il rig,
     quindi prima o poi dovra' scrivere **pose** e non scambi di disegno. Non
     inchiodare il bersaglio al «livello di bocche»: il MouthSet deve poter
     puntare a un disegno OPPURE a una posa.

   - ✅ **FATTO il 2026-08-15 — CHI PARLA, ricavato dal testo.** Era il perno di
     tutta la catena: `PanelData::dialog` era una stringa ANONIMA.

     **Strada scelta da Franco: la convenzione, non un campo strutturato.**
     *«Va bene la B perche' tanto facciamo copia e incolla dallo script, e in
     alcuni formati come l'FDX e il Fountain il character e' riconoscibilissimo
     nel testo.»* Decisiva: la strada strutturata avrebbe toccato **18 punti**
     fra Board, animatic e due serializzatori, e avrebbe fatto compilare due
     campi dove se ne incolla uno solo — perdendo l'unica cosa che rende il
     lipsync automatico, cioe' che il testo c'e' gia'.

     `ZtoryModel::parseDialogue()` riconosce le due forme vere:
     `MARIO: battuta` e la forma sceneggiatura (nome da solo in maiuscolo,
     battuta sotto). Toglie le estensioni `(V.O.)` `(O.S.)`, scarta le
     didascalie fra parentesi, ricompone le battute su piu' righe.

     **Il nome si colora DENTRO il campo** (idea di Franco: *«non potrebbe
     bastare evidenziare in verde il nome?»* — si', ed e' meglio: il riscontro
     va dove sta la causa). Verde = personaggio del progetto, arancione = no.
     Resta una riga di avviso SOLO per i non riconosciuti, che in un campo lungo
     e scrollato resterebbero fuori vista. Nel Board **e** nello Shot Board.

     **ALIAS**: si seleziona un nome e lo si forza su un personaggio (tasto
     destro). Serve davvero — negli script i nomi non coincidono mai del tutto
     con quelli del tracker («PRINCIPESSA» nel copione, «PRINCENERENTOLA» fra
     gli asset) e le alternative erano correggere il copione o rinominare
     l'asset. L'alias e' di PROGETTO, non di pannello.

     ⚠️ **La regola sta in UN posto solo** (`ZtoryModel::speakerAt()`), usata sia
     dal parser sia dall'evidenziatore. Due copie divergono al primo caso
     limite, e in questa feature e' gia' successo.

     🧪 **13 casi di test** sul codice VERO (estratto testualmente, non
     riscritto): `scratchpad/test_parser.cpp`. Ne hanno presi due, e valgono
     piu' del codice:
     1. la prima versione rifiutava un nome sconosciuto, e cosi' «GIOVANNI»
        finiva inghiottito nella battuta — la funzione che deve SEGNALARE i
        personaggi mancanti non poteva vederne nemmeno uno;
     2. correggendo, avevo escluso le didascalie dal «seguito da»: ma
        `MARIO / (sottovoce) / Non ci credo` e' normalissimo e la parentesi
        CONFERMA l'intestazione. Facevo sparire Mario. A negarla e' la riga
        VUOTA, non la parentesi.

     📌 Nel progetto di Franco **51 personaggi su 63 hanno nomi veri e gia' in
     maiuscolo** (BRONTOLO, FATINA, LUPO, SOFIA…), quindi la convenzione morde
     da subito. I 12 chiamati «1».."12" non si riconoscono nella forma
     sceneggiatura — il parser pretende almeno una lettera, o una riga di numeri
     diventerebbe un personaggio — ma funzionano con i due punti.

   - 🎯 **DECISO 2026-08-15: serve un ALLINEATORE FORZATO. I tempi di
     whisper.cpp NON bastano — misurato.** Franco: *«dobbiamo arrivare a un
     sistema preciso, deve funzionare subito senza doverci rimettere le mani,
     altrimenti non ha senso»*. Quindi NON si fa la pezza di ridistribuzione:
     si fa la cosa giusta.

     **La misura che chiude la questione** (audio vero di Franco, 3,24 s, una
     battuta di SOFIA). I tempi per parola di `-ml 1 -sow` non sono un
     allineamento: sono una segmentazione approssimata dei token, ed **erano
     ERRATICI fra un modello e l'altro**:

     | parola | base-q5_1 (57 MB) | base intero (141 MB) |
     |---|---|---|
     | facile? | 680–1430 | 1190–2000 |
     | fa | 2350–2500 | **3240–3240** (durata zero) |
     | me! | 2720–2920 | 3240–**4660** (oltre la fine dell'audio!) |

     ⚠️ **Il modello INTERO da' tempi PEGGIORI del quantizzato**: parole a
     durata zero e coda oltre l'audio. Quindi il problema **non si risolve con
     un modello piu' grande**, e non e' un difetto del nostro codice.
     `-dtw base` provato: nessun effetto (il modello quantizzato non ha le teste
     di allineamento).
     Verificato anche che l'inviluppo complessivo e' giusto: il parlato finisce
     a 2886 ms (silencedetect) e Whisper dice 2920. E' la distribuzione DENTRO
     a sbagliare.

     **La strada**: un modello **CTC di allineamento forzato** che riascolta
     l'audio SAPENDO gia' le parole e dice dove cadono — il secondo stadio di
     WhisperX, e la ragione per cui WhisperX esiste. Precisione a decine di
     millisecondi invece che centinaia. A 25 fps, 200 ms sono 5 fotogrammi:
     nello scrub si vedono.
     Da valutare: wav2vec2 CTC per lingua (quelli usati da WhisperX), oppure un
     allineatore separato invocato come processo (schema Rhubarb/ffmpeg).
     ⚠️ Licenza e peso dei modelli di allineamento **da verificare**, come si e'
     fatto per whisper.cpp.

   - 🔇 **IL SILENZIO E' UN DATO — una traccia audio sola basta**
     (ragionato con Franco il 2026-08-15). Sua domanda: *«come fa a capire che a
     un certo punto quel personaggio deve restare muto e parla un altro? Con
     Rhubarb colleghi il livello delle bocche alla colonna dell'audio.»*

     **Non serve dividere l'audio per personaggio.** Separarlo servirebbe a
     DEDURRE chi parla — ma non lo dobbiamo dedurre, **c'e' scritto** nel testo
     del pannello. Dall'allineamento esce (personaggio, parola, inizio, fine),
     quindi per ogni personaggio si sanno DUE cose: dove parla → viseme, e
     **tutto il resto** → Rest. Il silenzio si ricava per complemento.

     ⚠️ **Il Rest va SCRITTO, non lasciato vuoto.** Una cella vuota tiene
     l'ultimo disegno: il personaggio resterebbe con la bocca aperta a meta'
     parola per tutta la battuta dell'altro. La colonna consegnata all'animatore
     e' una **linea temporale completa**: fonemi dove parla, Rest dove tace.

     **E' MEGLIO della separazione audio, non un aggiramento.** Il legame
     Rhubarb livello-bocche ↔ colonna audio presuppone che quell'audio sia di
     quel personaggio: dandogli un mix, Rhubarb fa muovere la bocca su TUTTE le
     battute, anche quelle degli altri — il difetto che Franco aveva gia'
     individuato chiedendo una colonna per personaggio. Con l'attribuzione dal
     testo sappiamo anche chi NON sta parlando, informazione che nemmeno una
     pista pulita per personaggio da' (li' il silenzio non distingue fra pausa e
     battuta altrui).

     **Cosa creare in automatico e cosa no:**
     - ✅ le **colonne di TESTO** (`TXshSoundTextColumn`), una per personaggio
       che parla in quello shot: dati derivati, si rigenerano, e finiscono
       nell'exposure sheet stampato.
     - ❌ **NON** le tracce audio. L'audio vive nel main xsheet ed e' di tutta la
       scena, i personaggi compaiono shot per shot: uno che parla in un pannello
       solo si porterebbe una traccia per l'intero progetto. E una traccia vuota
       «da riempire correttamente» e' un compito assegnato all'utente senza
       dargli niente in cambio.
     - 🔧 il legame **traccia → personaggio** resta come **RIFINITURA**: quando
       l'audio arriva gia' separato (doppiaggio, una pista per attore) e'
       guadagno netto — Whisper sente una voce sola. Ma e' un di piu' quando
       c'e', non un requisito da soddisfare.

     **Due limiti da tenere presenti:**
     - il testo dev'essere **ragionevolmente completo**: qui Whisper fa
       allineamento forzato, e una BATTUTA INTERA mancante puo' far slittare
       tutto il seguito (un refuso invece non fa danno);
     - il **parlato sovrapposto** e' l'unico caso in cui la traccia separata
       vince davvero — da trattare come eccezione, non come regola che detta
       l'architettura.

   - Primo passo comunque indipendente da Whisper: collegare `PanelData::dialog`
     all'argomento `-d` di Rhubarb. Il copione ce l'abbiamo gia' scritto, e
     nessun riconoscitore batte il testo vero.

---

### 🧍 IL PERSONAGGIO COME OGGETTO — com'e' fatto DAVVERO (rilevato 2026-08-16)

> Franco ha fermato la progettazione del MouthSet: *«prima definiamo il
> PERSONAGGIO»*, perche' e' lo stesso problema della libreria di rig riusabili e
> farlo due volte non ha senso. Poi ha spiegato com'e' fatto oggi e ha indicato
> gli esempi veri. Questa sezione e' OSSERVATA sul suo progetto
> `2604_grottazzolina`, non immaginata.

**Com'e' fatto un personaggio oggi (parole sue):** una **scena** che contiene il
personaggio riggato; la mesh applicata a una **sotto-scena** con le varie parti;
le **bocche di solito sono un livello a parte dentro la sotto-scena della
testa** (che a volte sta insieme al corpo). Quando serve, si **importa come
sotto-scena**.

**Verificato nel progetto reale** (`scenes/LIB_*.tnz`, sette personaggi):
- un personaggio = `scenes/LIB_NOME.tnz` + un PSD in `extras/LIB_NOME/`;
- il PSD e' caricato **a gruppi**: ogni gruppo diventa un livello chiamato
  `CH_nome#@N#group`. ⚠️ **I nomi sono NUMERATI, non descrittivi**: non esiste
  convenzione che permetta di indovinare quale livello sia la bocca. Va indicato
  a mano una volta, e ricordato. E' esattamente il sidecar che dice Franco.
- accanto al PSD stanno i `.mesh` del rig (`sub.0007.mesh`…).

⚠️ **IL PSD E' UN CASO, NON LA REGOLA** (correzione di Franco, 2026-08-16):
*«il sidecar e' legato al livello della scena, non al psd, potrebbe essere anche
un personaggio disegnato direttamente in ztoryc, vettoriale, smart raster o
raster che sia»*. Quindi il MouthSet punta a **un LIVELLO della scena
personaggio** — qualunque tipo, qualunque provenienza — e non va legato ne' al
formato ne' al file d'origine. Il PSD a gruppi e' solo il modo in cui e' fatta
questa produzione.

🎯 **IL FATTO CHE DECIDE L'ARCHITETTURA**: importare il personaggio in uno shot
**COPIA** i suoi file dentro `extras/<shot>/LIB_NOME/` (23 MB di PSD per ogni
shot). Quindi:

```
percorso   +extras/LIB_GIORNALISTA/…   →  +extras/scsh010/LIB_GIORNALISTA/…   CAMBIA
nome       CH_giornalista#@7#group     →  CH_giornalista#@7#group             RESTA
```

**Quindi il MouthSet NON va agganciato al percorso del file, ma al NOME DEL
LIVELLO** (piu' il numero di fotogramma). Il nome sopravvive alla copia, il
percorso no. Un sidecar messo accanto al PSD si romperebbe al primo import — o
costringerebbe a inseguire le copie.

🏠 **DOVE METTERLO: il sidecar `.ztoryc` ESISTE GIA'** accanto alle scene
personaggio (`LIB_GIORNALISTA.ztoryc` c'e' davvero, scritto perche' la scena e'
stata aperta in Ztoryc) e ha gia' un attributo **`role`**. Oggi i ruoli sono due,
`"storyboard"` e `"shot"` (`ztorymodel.cpp:151, 1445`). Un terzo ruolo
**`"character"`** e' il posto naturale per mouth set, pose registrate e
correttive: viaggia col file della scena, e' gia' letto e scritto, e non serve
inventare un formato nuovo.

**Struttura che ne segue** (da implementare, non ancora fatto):
- il MouthSet vive nel sidecar della scena personaggio (`role="character"`),
  con un riferimento incrociato all'**Asset** di progetto via `uuid`;
- una voce = `viseme -> (nome livello, fotogramma)`, dieci voci. **Il nome del
  livello, non il percorso**: e' l'unica cosa che sopravvive alla copia
  nell'import, e non dipende dal tipo di livello;
- un personaggio ne possiede **piu' d'uno**, con attributi vista / espressione /
  variante, e al lip sync si sceglie solo quale;
- ⚠️ deve poter puntare a un disegno **OPPURE a una posa**: con ZtoRig la bocca
  sta dentro il rig, e prima o poi si scrivono pose, non scambi di disegno.
- il personaggio deve conservare anche **pose registrate e correttive** (Franco,
  2026-08-16): il MouthSet e' UNA delle cose che gli appartengono, non un
  oggetto a se'.

**Il pezzo di interfaccia c'e' gia'**: `LipSyncPopup` ha le dieci caselle con
anteprima e frecce per scorrere i disegni del livello. Manca **salvare
quell'assegnazione sul personaggio e ripescarla** — non l'assegnazione in se'.

---

### ♻️ IL RITORNO IN LIBRERIA — le pose create animando (Franco, 2026-08-16)

> *«Creo il personaggio e lo importo come sottoscena per animarlo nei vari shot,
> animandolo però creo nuove pose e animazioni che potrebbero tornarmi utili, la
> sua libreria si arricchisce: come aggiorniamo il file sorgente?»*

🎯 **LA PARTE DIFFICILE E' GIA' RISOLTA, e non l'avevamo notato.** Da
`include/ext/plasticskeletondeformation.h:289`:
> «Deltas are stored **BY VERTEX NAME**, like keyframes are, and never by vertex
> index: that is what lets an action be **copied to a skeleton whose internal
> vertex numbering differs**.»

Cioe' una `PoseAction` e' **portabile per costruzione**: si trapianta da una
copia del personaggio a un'altra. E' esattamente il caso del ritorno in
libreria, ed e' il problema che di solito costa caro. Manca solo il TRASPORTO.

C'e' anche gia' il controllo di compatibilita': `m_skelIds` dice su quali
scheletri una posa e' lecita (riga 330). Una posa registrata sul frontale
replicata sul profilo «lands somewhere nobody authored» — quindi il travaso non
puo' essere cieco.

**Come farlo, quando si riapre:**
- ⚠️ **Esplicito e a senso unico, MAI automatico.** Il personaggio nello shot e'
  una COPIA (l'import copia i file, verificato: 23 MB di PSD per shot), quindi
  e' un fork. Una sincronizzazione automatica propagherebbe anche gli errori e
  le pose sbagliate a tutta la produzione.
- Due comandi simmetrici: **«pubblica in libreria»** dallo shot (scegli quali
  azioni, finiscono nel personaggio) e **«prendi dalla libreria»** nello shot.
  E' il modello dei template di Harmony, ed e' come ragiona un animatore:
  «questa me la salvo».
- Le pose vanno nel sidecar `role="character"` (vedi la sezione sopra), non
  dentro la scena: cosi' si leggono senza aprire il personaggio.
- **I CICLI di animazione sono un'altra cosa**, piu' grossa: non un insieme di
  valori ma curve nel tempo su piu' parametri. Da progettare a parte — una posa
  e' uno stato, un ciclo e' una clip. Non trattarli come lo stesso oggetto.
✅ **DECISO da Franco (2026-08-16) — divergenza strutturale**: *«il salvataggio
sul character sorgente viene RIFIUTATO se non e' compatibile; se e' diverso
strutturalmente potremmo esportarlo come una v2 dello stesso personaggio»*.
Buona regola: trasforma un caso d'errore in un atto deliberato, invece di
lasciare all'utente una fusione a meta'.

Il controllo di compatibilita' ha una definizione CONCRETA, e la si ha gratis
perche' i delta sono per nome di vertice:
- lo scheletro di destinazione contiene **tutti** i nomi citati dalla posa →
  compatibile, si accetta (se ne ha altri in piu' restano fermi: la posa e'
  PART, tocca solo i suoi);
- ne manca anche uno → **rifiuto**, e si propone la **v2 del personaggio**.

**Il flusso, come lo ha descritto Franco:** animi nello shot → comando
«pubblica in libreria» → scegli quali azioni registrate → controllo → finiscono
nella libreria del personaggio sorgente, disponibili da li' in poi in ogni shot.

---

### 🔴 APERTO — il controller funziona nel viewer e non nel render

Franco, 2026-08-05: «forse e' il nostro controller (quella specie di animate tool
legato allo skeleton) che se lo uso per riposizionare un elemento funziona nel
viewer ma non nel render». **Primo sospetto da verificare** quando si riprende il
render plastico.
Appiglio misurato: `getSquashControllerAffine` in un caso valeva
`[1, 0, 462.308, 0, 1, -2.17253]`, cioe' una **traslazione di 462 unita'**, non
l'identita'. Il codice lo descrive come «un affine SOPRA il risultato deformato»,
e viene composto in **due punti diversi** nelle due strade: `stagevisitor.cpp`
(viewer) fa `... * worldMeshToMeshAff * ctrl * meshToWorldMeshAff`,
`plasticdeformerfx.cpp` (render) fa `... * meshToWorldMeshAff * worldMeshToMeshAff
* squashCtrl * meshToWorldMeshAff`. **Verifica diretta**: stampare le due matrici
finali sullo stesso frame e confrontarle.

### 🟠 APERTO — doppio render occasionale

Franco: «succede ogni tanto». Parte due volte lo stesso lavoro. Da capire se
succede lanciando dal Task panel o dal menu — e se due processi scrivono lo
stesso file di output, e' un difetto a se'. Non e' la causa degli artefatti di
oggi (troppo ripetibili per una corsa fra processi).

### 🟠 APERTO — uno zombie si smonta in alcuni frame

Dopo aver reimportato la scena e' rimasto **un solo** caso: un personaggio i cui
pezzi appaiono staccati in un momento. Ipotesi di Franco: il **pin**, che li' sta
su piu' livelli. Ora e' isolato a un caso solo, quindi trattabile.

### 🔴 BUG — sussulto fra due chiavi sui vertici plastici (2026-08-04)

Segnalato da Franco, **ricorrente** («ogni tanto succede»). Fra due chiavi il
valore fa un'escursione che non dovrebbe esserci: visto su
`MaggiolataZombie/sh330` → sotto-scena **lib_armando** → sotto-scena **col1**,
sulle curve dei **vertici dello skeleton plastico** (colonna `Angle`), frames
9-21. A schermo: due chiavi a 0 e in mezzo l'angolo sale a **132** e torna.

**CAUSA (confermata dal rimedio)**: tangenti **non clampate** sulle chiavi dei
vertici. Chiave trovata nel `.tnz`:
```
S  frame 27   valore 55.017   maniglia.x 2.667   maniglia.y +21.69      (VD > Angle)
```
`maniglia.x = 2.667` e' un terzo di 8 frame, cioe' la maniglia standard: la
lunghezza e' giusta, e' la **pendenza** a essere fuori scala — oltre 8 unita'
per frame su un tratto che non deve muoversi.

✅ **Franco conferma che applicando Auto Bezier il problema sparisce**, il che
CONFERMA la diagnosi: il clamp di `KeyframeSetter::setAutoBezier`
(Fritsch-Carlson, tangente limitata a 3x la pendenza del segmento piu' piatto,
piatta sugli estremi locali) e' esattamente cio' che manca.

**Dove si scrive quella chiave** — catena tracciata:
`plastictool_animate.cpp` → `::setKeyframe(vd->m_params[SkVD::ANGLE], ...)` →
`plastictool.cpp:196 setKeyframe(TDoubleParamP&, double)` → `createKeyframe()`.
⚠️ Ma `createKeyframe` NON puo' produrre quella pendenza: con Auto Bezier acceso
clampa, spento assegna maniglie **orizzontali** (`segmentWidth/3, 0`). **Quindi
qualcosa RUOTA le maniglie dopo la creazione.** Due candidati, nessuno
verificato: le **maniglie linkate** (forzano i due lati di una chiave a restare
allineati e possono importare la pendenza del segmento vicino) oppure una
riscrittura successiva dello stamping delle pose.

**DA QUI SI RIPARTE**: trovare chi ruota le maniglie, e applicare li' il clamp
gia' esistente. Rimedio nel frattempo: Auto Bezier sul segmento (o Linear).

⚠️ **QUATTRO misure sbagliate prima di arrivarci, non rifarle**: (1) maniglie vs
lunghezza del segmento; (2) salti di valore; (3) maniglie vs dislivello **con
una soglia assoluta** che scartava i valori piccoli dei vertici plastici;
(4) segmenti **piatti** con tangenti non nulle — e il caso vero non e' piatto
(la chiave vale 55, non 0), quindi cadeva fuori da ogni filtro. Il parser dei
`.tnz` copriva tutto (7349 tag su 7349): non era un problema di copertura ma di
**criterio**. Vedi [[feedback_instrument_before_optimizing]].


---

## 🧩 Puppetoonz (lato Ztoryc)

> Il TODO di Puppetoonz vive nel SUO repo (`puppetoonz/TODO.md`). Qui solo
> cio' che riguarda Ztoryc: il porting come pannello.

**1. Scansioni di personaggio → livelli. ORA E' UN'APP CON UN REPO SUO:
`matitanimata/puppetoonz` (privato).** Si chiama **Puppetoonz**, sta in
`/Volumes/ZioSam/tahoma2d-workspace/puppetoonz/`, si avvia con
`Puppetoonz.command` e ha il suo `TODO.md`. Le voci sotto restano valide come
storia e come indicazioni per il porting in C++, ma il lavoro quotidiano si fa
li'. Il vecchio laboratorio (`reference/ch-layers/`) non e' stato toccato.

La pipeline FUNZIONA gia' in Python: `/Volumes/ZioSam/tahoma2d-workspace/reference/ch-layers/`.
Toglie la carta, separa gli elementi in componenti connesse, raggruppa le
sequenze e scrive un PSD con i gruppi che Ztoryc importa come fotogrammi.
Provata sulla tavola `princenerentolo.tiff`: 19 livelli, 3 gruppi, fotogrammi
registrati fra loro.

Deciso con Franco che **non sara' un'app separata ma una funzione di Ztoryc**:
OpenCV 4.1 e' gia' dipendenza obbligatoria (`sources/CMakeLists.txt:371`) e tutto
cio' che serve e' OpenCV di base, quindi il porting in C++ non aggiunge
dipendenze — mentre un'app a se' vorrebbe un secondo giro di build, firma e
distribuzione su tre sistemi.

**Ordine dei tempi, deciso: NON portare in C++ adesso.** Prima il Python fa da
laboratorio su altre tavole vere, poi il template, e solo dopo il pannello.
Portarlo prima congelerebbe una ricetta ancora in movimento.

**2026-08-22 — il porting si spezza in due, approvato da Franco.** Domanda sua:
«e' pensabile inserirlo dentro Ztoryc e fare tutto li', saltando il passaggio in
Photoshop?». Risposta emersa guardando il codice: **Photoshop non e' nel giro**
— il PSD lo scrive un writer nostro in `puppetoonz_core.py`, quindi togliendolo
si toglie un file intermedio, non un'applicazione. Il passaggio che costa
davvero e' un altro, e si toglie **subito e senza C++**:

- ✅ **PRIMO PASSO — FATTO il 2026-08-27: Puppetoonz scrive il `.zmouth`.**
  Esportando nascono `CH_nome#51#group.zmouth` accanto al PSD, con i viseme
  gia' assegnati: la tavola sa che `01 AI` e' `AI`. Le venti caselle si
  dividono in due `<mouthSet>` sulla regola del **ripetersi** di un viseme, non
  su un conteggio fisso. Le palpebre non ne prendono uno.
  **La domanda aperta aveva una risposta migliore del previsto:** il numero in
  `#7#group` non lo assegna il caricatore, e' la posizione del record nel file —
  scriviamo `lsct` ma non `lyid`, e senza `lyid` il lettore ripiega su `i + 1`.
  Quindi lo decidiamo noi, e arriva da `mappa_out` di `scrivi_psd` invece che da
  un secondo conteggio che divergerebbe al primo fotogramma saltato.
  Trovato con una **sonda compilata contro le librerie vere di Ztoryc**, che ha
  anche scoperto che i fotogrammi di un gruppo arrivavano **all'incontrario**
  (`01 AI` era il fotogramma 14) — raddrizzato.
  La tavola ora ha **dieci viseme per espressione** (aggiunti `WQ` e `REST`,
  raddoppiati `O` e `U`): venti caselle, 12,9 x 16,9 mm l'una.
  ⚠️ **Da guardare prima di stampare:** sono piu' alte che larghe, mentre una
  bocca e' il contrario. Due righe da dieci le porterebbero a 27,5 mm.

- **SECONDO PASSO (C++, quando l'interfaccia smette di muoversi): il pannello.**
  Il guadagno vero non e' saltare il PSD, e' poter correggere un pezzo con gli
  **strumenti di disegno di Ztoryc** invece dei tre pulsanti di Puppetoonz.
  Il porting non e' un salto nel buio: `ztorypapersheet.cpp` sono gia' 710 righe
  di OpenCV in C++ dentro Ztoryc, e fanno **gia' il riconoscimento dei crocini**
  che a Puppetoonz manca (voce aperta nel suo TODO).

  Il rischio se si porta prima e' preciso: due interfacce di correzione, e gli
  esperimenti si continuano a fare in quella che si modifica in dieci minuti,
  non in quella che chiede una compilazione. Cioe' si paga il porting e non si
  smette di usare il Python. Vale ancora di piu' oggi: la modifica delle caselle
  e' di ieri, e restano aperte due voci che toccano l'ossatura dei dati (piu'
  scansioni sullo stesso personaggio rompe l'assunto «un elemento = un'etichetta»;
  i crocini cambiano come si assegnano le caselle).

✅ **Il `.zmouth` VIAGGIA con il personaggio esportato — NON era aperto**
(verificato il 2026-08-29). La voce precedente diceva il contrario ed era
sbagliata **gia' quando e' stata scritta**.

Le prove:
- l'export dello storyboard chiama `TXshSimpleLevel::copyFiles()` in due punti
  (`storyboardpanel.cpp:6630` e `:7159`), nati il **2026-07-06** (`48cb39150`);
- `copyFiles()` copia il `.zmouth` dal **2026-08-16** (`e0ac9527b`), per
  qualsiasi tipo di livello, non solo tlv;
- `storyboardpanel.cpp` non ha commit dal 26 agosto, quindi il 27 era gia' cosi'.
- Le altre due `TSystem::copyFile` (righe 6632 e 7162) sono ripieghi che
  scattano solo se `copyFiles` e' fallita, e la 7207 e' il sidecar `.ztoryc`.

> ⚠️ **Perche' era stato dato per aperto: `grep zmouth storyboardpanel.cpp`
> non trova niente.** Ed e' vero — ma non deve trovare niente, perche' il
> pannello **delega** a `copyFiles()`. Cercare una stringa in un file non vede
> una delega, e una verifica cosi' produce un falso positivo che poi vive nella
> lista per giorni. Se il sospetto e' «questo percorso non porta il file X»,
> si guarda **quale funzione di copia chiama**, non se il nome di X compare.
>
> Costo reale: il 2026-08-29 questa voce e' stata riaperta e richiusa due volte
> nella stessa sessione prima di arrivare alla prova. Se ricompare il dubbio,
> la risposta e' qui — non ricontrollare.

Il resto della catena era gia' a posto: `getFiles()` e `copyFiles()` lo
conoscevano, e il 2026-08-27 sono stati sistemati anche `renameFiles()` e
`removeFiles()` — rinominare un livello di bocche perdeva la mappa in silenzio.
I posti da tenere allineati sono **quattro**, non due come diceva il commento.

⚠️ **Il prodotto non e' l'algoritmo** (sono ~200 righe) ma l'interfaccia di
correzione: il riconoscimento sbagliera' sempre qualcosa, e oggi gli scarti li
ha tolti Claude a mano compilando una tabella. Serve poter riassegnare un nome,
unire due pezzi o buttare una sbavatura in dieci secondi.

Template consegnato a Franco il 2026-08-18: `tavola_personaggio_A4.pdf`, tutto
in ciano chiaro (si filtra dal canale rosso), crocini di registro agli angoli e
un puntino di registro in ogni cella di sequenza. I "due livelli" su carta sono
due colori: grafite = disegno, matita BLU = crocino di destinazione (dove va la
bocca sul viso), che riempie in automatico il `DESTINAZIONE` dello script.


---

## 🔧 Infrastruttura, rilasci, crash dormienti

### BUG (da investigare) — Crash palette-switch su click/apertura shot (macOS)

**Priorità: MEDIA-ALTA | Tipo: BUG | Segnalato: 2026-05-31 (SB_APPENNINGERS)**

Famiglia di SIGSEGV quando si seleziona/apre uno shot dall'animatic: lo switch di
colonna/xsheet aggiorna la palette del livello corrente → oggetti che ascoltano la
palette crashano mid-switch (palette/livello stale). Due varianti osservate:

- `shotClicked → onColumnIndexSwitched → updateXshLevel → setPalette →
  editLevelPalette → setPalette → StyleEditor::onStyleSwitched()` → SIGSEGV
  (Crash-20260531-015855). Core Tahoma (libtoonzqt).
- `onShotDoubleClicked → openSubXsheet → onXsheetSwitched → updateXshLevel →
  setPalette → ToonzRasterBrushTool::onColorStyleChanged()` re-entrante → SIGSEGV
  (Crash-20260530-231701). **GIÀ FIXATO** in `8f8740628` (guardia re-entrancy).

Lo StyleEditor variant NON è coperto dal fix del brush. Pattern comune: il rapido
cambio colonna/xsheet dall'animatic emette un cascata di setPalette mentre il
livello/palette puntano a dati transitori. Possibile fix Ztoryc-side: deferire
(QTimer 0) il cambio colonna/palette in `shotClicked`/`onShotDoubleClicked`, o
guardia anti-reentrancy lato chiamante. Da indagare con SB_APPENNINGERS.

**Crashlog:** `Crash-20260531-015855.log` (StyleEditor), `Crash-20260530-231701.log` (brush, fixato).

---

### BUG (da investigare) — Stack overflow ricorsione layout QScrollArea (Windows)

**Priorità: MEDIA | Tipo: BUG | Segnalato: crash Windows 2026-05-28 (build 0.3.4)**

Crash `EXCEPTION_STACK_OVERFLOW` su Windows 10. Backtrace = ricorsione infinita
`QScrollArea::eventFilter → QWidget::resize → setGeometry_sys → QScrollArea::eventFilter`
ripetuta ~480+ volte. In cima allo stack: `QLabel::setWordWrap / sizeHint /
QTextEngine::itemize` → un **QLabel word-wrap dentro una QScrollArea** il cui
height-for-width oscilla (la larghezza dipende dalla scroll area che si ridimensiona
in base al sizeHint del label → loop).

**Contesto:** l'utente aveva installato 0.3.4 SOPRA un'installazione precedente
(file stale nella stessa cartella + config/layout `rooms` per-utente in AppData).
Possibile trigger: layout salvato incompatibile. **Workaround utente: clean install.**

**Da fare:** cercare nei pannelli Ztory una `QScrollArea` con dentro un `QLabel`
word-wrap senza larghezza fissata (candidati: StoryboardPanel grid, Script panel,
PanelWidget con label). Fix tipico: `label->setMinimumWidth()` o
`setWordWrap` + size policy fissa, oppure rompere il feedback con
`QScrollArea::setWidgetResizable` / gestione esplicita del resize.
Non riproducibile finora su Mac (probabilmente layout-file-specifico).

**Crashlog:** `SamDrive/Ztoryc/crash_windows/Crash-20260528-191004.log`

---
---

### ⚠️ DA NON PERDERE — la trappola del pin Vosk

**`ZTORYC_VOSK_RELEASE=0.3.42` non va alzato senza controllare gli asset.**
La 0.3.42 e' l'ULTIMA release che pubblica un binario macOS
(`vosk-osx-0.3.42.zip`). Dalla 0.3.45 in poi ci sono solo Linux, Windows e i
wheel Python. Alzando il pin **nessuna build fallirebbe**: macOS resterebbe
senza libvosk e il lip sync si ripiegherebbe su Whisper in silenzio.
Verificato interrogando l'API delle release il 2026-08-16.


