## [2026-08-15b] — Whisper dentro Ztoryc, e la misura che dice di non fidarsene

Sessione lunga, tutta sul lipsync. Il filo: **ogni volta che ho misurato invece
di dedurre, ho trovato qualcosa che il ragionamento non prevedeva** — e due
volte era colpa mia.

### Added — chi parla, dal testo (`f7482e1f2`, `a721a4d77`)
`PanelData::dialog` era una stringa ANONIMA: il perno di tutta la catena.
Scelta di Franco: **convenzione nel testo, non campo strutturato** — «tanto
facciamo copia e incolla dallo script, e in FDX e Fountain il character e'
riconoscibilissimo». La strada strutturata avrebbe toccato 18 punti e fatto
compilare due campi dove se ne incolla uno.
Il nome si colora **dentro il campo** (idea sua: «non potrebbe bastare
evidenziare in verde?»), con **alias** per forzare un nome sul personaggio
giusto. Interruttore nelle preferenze, e si spegne da solo su un progetto senza
personaggi. **15 test** sul codice vero: ne hanno presi due.

### Added — whisper.cpp (`476a96306`, `4439834a2`, `bd6d81e23`, `257db25ea`)
Cablato come Rhubarb, modello `base-q5_1` (57 MB) nel bundle, licenze verificate
alla fonte (tutte MIT), riallineamento tempi↔copione con **9 test sui dati
reali**, e il comando nel menu Scene.

### ⚠️ I quattro difetti trovati facendolo girare davvero (`deaa616fa`)
1. **Il comando non era in nessun menu.** Il menu di Ztoryc non viene da
   `menubar.xml`: e' costruito in `TopBar::loadMenubar()`. Avevo modificato due
   file che non legge nessuno. **Non esiste un menu «Xsheet»** — quelle voci
   stanno in **Scene**.
2. **Lo shot si cercava alla riga 0.** In Ztoryc gli shot stanno in fila nel
   tempo: solo il primo ha una cella alla riga 0. Da ogni altro il comando
   diceva «apri una sotto-scena» stando gia' dentro una sotto-scena.
3. **`-nt` distruggeva i tempi.** L'avevo aggiunto per zittire la console:
   significa *no timestamps*. Avevo spento l'unica cosa per cui chiamiamo
   Whisper. Le parole centrali uscivano a durata ZERO e l'ultima a 30000 ms —
   la finestra da 30 s con cui whisper.cpp riempie l'audio corto: a 25 fps il
   fotogramma **751**, in uno shot da 81.
4. **Sfasamento di un fotogramma** (`createSoundTextLevel` mette lista[i] alla
   riga i, e la riga 0 e' il fotogramma 1) e **parole orfane buttate intere** in
   silenzio.

Su (3) la lezione non e' l'opzione sbagliata — capitano — ma che un dato assurdo
sia arrivato **fino alla colonna** senza che nulla lo fermasse. Aggiunta la
difesa a valle.

### 🎯 Deciso — serve un ALLINEATORE FORZATO
Franco: *«dobbiamo arrivare a un sistema preciso, deve funzionare subito senza
doverci rimettere le mani, altrimenti non ha senso»*. Niente pezze.

**La misura che chiude la questione**: i tempi per parola di whisper.cpp non
sono un allineamento ma una segmentazione approssimata, ed **erraticI fra
modelli**. Il modello **intero da' tempi PEGGIORI del quantizzato** — «fa» e
«per» a durata zero, «me!» fino a 4660 ms su un audio di 3240. Quindi **non si
risolve con un modello piu' grande**. `-dtw` provato: nessun effetto.
L'inviluppo complessivo e' invece giusto (parlato fino a 2886 ms, Whisper dice
2920): sbaglia la distribuzione dentro.

Strada: modello **CTC di allineamento forzato** che riascolta l'audio sapendo
gia' le parole — il secondo stadio di WhisperX, e la ragione per cui esiste.

### Note — non toccato di proposito
- **Crash** al cambio workflow (doppia liberazione fra i figli di `ColumnArea`
  in `clearRooms()`): **non riproducibile**, e `xshcolumnviewer.cpp` non l'ho
  mai toccato. Annotato, non inseguito.
- **Riordino shot**: Franco ha segnalato dialoghi che sembravano spostarsi, poi
  ha verificato che funziona — erano shot vuoti, indistinguibili fra loro.
  **Nessuna modifica**, come da sua regola. Resta annotato che `refreshFromScene`
  salta le colonne senza child level mentre `onMoveShot`/`onAddShot` usano
  l'indice dello shot come indice di colonna: oggi coincidono, e il campo giusto
  (`xsheetColumn`) esiste ed e' ignorato.
- **Mark out** che si aggiorna solo muovendo il confine: voce separata, da fare.
- **Colonne al momento dell'EXPORT** invece che a comando: idea di Franco, e ha
  ragione — a meta' lavorazione la colonna diventa stantia al primo cambio di
  timing. Da spostare li'.

## [2026-08-15] — Il breakdown, e il rifiuto di indovinare

Sessione lunga di **progettazione** con Franco su lipsync ed export, con due
implementazioni in mezzo. Il filo: quasi ogni volta il pezzo mancante non era
codice nuovo ma **un campo anonimo** — un dato che c'e' e non sa a chi
appartiene.

### Added — il selettore di lingua del lipsync (`f81d39338`)
L'etichetta diceva «Recognizer» e le due voci erano «PocketSphinx (English)» e
«Phonetic»: non diceva la cosa che conta, cioe' che una capisce PAROLE INGLESI e
l'altra non capisce parole affatto. Chi sincronizza italiano non poteva sapere
che il default cercava l'inglese nel suo audio. Ora e' «Dialogue language», e'
**sempre visibile quando Rhubarb gira** (prima spariva proprio quando carichi un
file audio, cioe' dove serve) e la scelta si ricorda.
Corretto un difetto latente: il confronto era su `currentText()`, cioe' il testo
**tradotto** — rendendo le voci traducibili si sarebbe rotto in ogni build
localizzata.

### Added — breakdown e catena degli asset (`d48f46b0e`)
Il breakdown **non e' stato inventato**: e' il *casting* di Kitsu, contratto
verificato sull'istanza vera prima di scrivere (tabella `entity_link`, 53 link
reali). Pull in **una chiamata** per tutto l'episodio; Kitsu e' autorevole
quindi ogni shot si **sostituisce**, non si fonde; solo lettura di proposito,
perche' il PUT di Kitsu sostituisce l'intero casting di uno shot e una spinta
parziale cancellerebbe il lavoro altrui.

Piu': cartelle asset **per categoria** nei parametri di progetto (con 145 asset
un percorso per ciascuno non lo compila nessuno), `resolveAssetFile()`, e la
scelta **Load vs Import** con default di progetto e scostamento per asset.

**La decisione di cui vado piu' fiero: non indovinare.** `resolveAssetFile()`
accetta solo il nome base UGUALE — «macchina» non pesca «macchina_v03» — e con
zero o piu' di un risultato restituisce vuoto col motivo. Un file non trovato si
risolve in dieci secondi; un file **sbagliato** scelto in silenzio si scopre in
render, giorni dopo. La colonna File della scheda Breakdown mostra cosa l'export
troverebbe ADESSO, ed e' cio' che la rende utile invece che decorativa.

### 🎬 Progettato con Franco — lipsync, e dove vive il dato
Franco mi ha fermato mentre mettevo un pulsante «prendi il copione dallo
storyboard»: *«aspetta aspetta [...] qui c'e' da pensarla bene»*. Aveva ragione,
e il pulsante e' stato **tolto lo stesso giorno** — leggeva il dialogo senza
sapere chi parla, cioe' gli mancava il perno.

Tutto in ANIMATIC_TASKS. In sintesi:
- **Meta' dei pezzi esistono gia'**: `TXshSoundTextColumn` e' la colonna
  dialoghi classica (una stringa per fotogramma) ed e' **gia' stampata
  nell'exposure sheet** da `exportxsheetpdf`; i personaggi sono gia' asset.
- **Mancano DUE campi, e sono i perni**: `PanelData::dialog` e' una stringa
  ANONIMA (deve diventare una lista di battute con un personaggio ciascuna — in
  un pannello parlano in due), e `Asset` non aveva un percorso file. Lo stesso
  identico schema due volte: il dato c'e' ed e' anonimo.
- **Decisioni di Franco**: il lipsync si prepara in Ztoryc e si applica
  dall'animatore nello shot esportato → il **MouthSet deve viaggiare col
  personaggio**; nelle celle i **fonemi**, come nell'x-sheet tradizionale (io
  avevo proposto le parole: ha ragione lui, quella colonna e' l'istruzione
  operativa, non documentazione); **una colonna per personaggio**, o si fa il
  lipsync delle battute dette da altri.
- **MouthSet = sidecar accanto al LIVELLO**, non dentro il progetto: viaggia con
  cio' che l'animatore importa davvero. ⚠️ L'export dovra' copiarlo, o il
  personaggio arriva con le bocche e senza le istruzioni per usarle.
- **Scoperta che rimpicciolisce il lavoro**: il pannello lipsync ha **gia'** i
  dieci slot di Preston Blair e chiede gia' quell'associazione — poi la
  dimentica ogni volta. Il MouthSet non e' una struttura da inventare, e' dare
  una casa a un dato che il programma raccoglie e butta via.
- **Whisper + espeak-ng** entrano come processi separati: `thirdparty.cpp` ha
  gia' la macchina. Le finezze (minimo 2 frame, anticipo di M/B/P) sono spiegate
  col PERCHE' — percezione la prima, fisiologia la seconda (il suono E'
  l'apertura delle labbra) — e con l'avvertenza che **litigano**: prima
  anticipare, poi imporre il minimo, con la chiusura protetta.

### Fixed — 50 artefatti di build tolti dal versionamento (`4e7cadaaa`)
`qxlsx/` era la directory di BUILD del target QXlsx (27 MB di `libQXlsx.a` + 35
`.o`), piu' `ztoryc_generated/`. Non una classe nuova di problema: **un buco in
un elenco che esiste gia'** — il `.gitignore` ha una riga per ogni target,
perche' la build dir E' la radice del repo. NON toccati i 24 `.o` di
`thirdparty/lzo` e i 20 di `Lz4`: sono di upstream, identici ai loro.
E AGENTS.md **prescriveva `git add -A`** (`baaf1759b`), la riga che ha prodotto
entrambi gli incidenti che il file stesso racconta come da evitare.

### Note — cosa resta da collaudare
- Il pull del breakdown sui 53 link veri e la risoluzione dei file: finora hanno
  girato solo nel compilatore.
- Il filtro shot per episodio: provato con **un solo shot**.
- Manca lo scostamento PSD **per singolo asset** nel senso completo? No: c'e'.
  Manca invece l'export che USA tutto questo — il piazzamento vero degli asset
  nello shot.

## [2026-08-14c] — Il tracker si lega a un episodio, e un elenco che si riempiva a mano

### Added — Kitsu: il legame diventa la COPPIA (progetto, episodio) — `3775c1144`
Franco: «devo poter connettere il production tracker al singolo episodio». Su
Kitsu «CARTOON SCHOOL 2026» e' **un** progetto tvshow con 6 episodi, e ogni
episodio e' un progetto Ztoryc a se': legandosi al solo progetto, ogni tracker
si tirava dentro la roba di tutti.

**Misurato sull'istanza vera PRIMA di scrivere** — ed e' la parte che vale:
106 shot su 3 episodi (30+32+44), 145 asset su 4 (40+39+38+28), zero sequenze
senza episodio padre. Numeri, non intuizioni.

- Nel dialogo un tvshow si espande nei suoi episodi; gli altri restano una riga.
- `m_kitsuEpisodeId` persistito: e' l'**id**, non il nome — rinominare su Kitsu
  non rompe il legame. Vuoto nei progetti vecchi = comportamento di prima.
- Il riaggancio confronta la **coppia**: col solo id di progetto, sei episodi
  della stessa serie fanno atterrare sempre sul primo.
- Filtro shot **e** asset. Sugli asset il predicato e' uno solo condiviso dalle
  DUE catene (import e stati): filtrarne una sola avrebbe fatto atterrare per
  nome gli stati degli asset altrui su quelli nostri.
- **Team NON filtrato**, di proposito: la troupe di una serie e' della serie.

### Due bug trovati per strada, che non c'entravano con gli episodi
1. **156 task su 591 sparivano in silenzio.** Lo stato si applicava solo se il
   task type esisteva gia' nella pipeline: `Modeling` e `Rigging` non hanno
   corrispondenza in Concept/Rough/Clean/Color e finivano nel nulla. Ora Kitsu
   e' la fonte di verita' e il tipo viene **adottato**. `reloadAssetTypesTab()`
   esisteva ma nessuno la chiamava, e `addAsset()` non creava il tipo mancante.
2. **I nomi del team non arrivavano PERCHE' Franco e' admin.** Zou serializza le
   persone con `serialize_safe` per gli amministratori, e li' `full_name` non
   c'e': e' una proprieta' Python, non una colonna. Solo i non-admin ricevono
   `present_minimal`, che la aggiunge a mano. Il codice inoltre **ingoiava**
   l'errore di rete e lo sostituiva con «persons endpoint restricted?» — una
   ipotesi travestita da diagnosi. Ora dice la causa vera.

### Fixed — 50 artefatti di build tolti dal versionamento — `4e7cadaaa`
`qxlsx/` non e' una libreria: e' la **directory di build** del target QXlsx
(`add_subdirectory(${SDKROOT}/QXlsx qxlsx)`, il secondo argomento e' la binary
dir). Dentro: `libQXlsx.a` da 27 MB, 35 `.o`, il moc. Piu' `ztoryc_generated/`,
che la ricerca per `.o`/`CMakeFiles`/`autogen` **non trovava**.

**Non e' una classe nuova di problema: e' un buco in un elenco che esiste gia'.**
Il `.gitignore` ha da tempo una riga per ogni target, perche' la directory di
build E' la radice del repo. Mancavano `/qxlsx/` e `/ztoryc_generated/`.

**NON toccati** benche' la stessa ricerca li peschi: i 24 `.o` assembly i386 di
`thirdparty/lzo/` e i 20 file di `thirdparty/Lz4/.../vs2010/CMakeFiles/` sono di
**upstream**, identici ai loro, mai toccati dalla nostra build e mai nominati da
un percorso di compilazione. Cancellarli sarebbe solo divergenza, col merge
1.6.2 in attesa. E i due `autogen.sh` di libpng/tiff **non sono artefatti**:
sono script di bootstrap autotools, li pescava il pattern per il nome.

### Modified — la regola, e il file che diceva il contrario — `baaf1759b`
AGENTS.md **prescriveva `git add -A`** nel passo «Commit and push»: la riga che
ha prodotto entrambi gli incidenti che il file stesso racconta come da evitare
(gli artefatti dentro `354d6f020`, i draft privati nel repo pubblico il
2026-07-04). Ora dice `git status` e poi i file uno a uno.
Aggiunta anche la mappa di `FromClaudioPaddei/` (sta su **SamDrive**, non sotto
`~/ZtorYc/`: il rimando a COMPETITIVE_ROADMAP.md era cieco), con l'avvertenza
che quei documenti hanno sezioni scritte come istruzioni («Cosa serve da Claude
Code») che sono **proposte di Claudio, non decisioni di Franco** — e il blocco
SOSPESI le sovrascrive.

### Recap lipsync — la miglioria che non era nella roadmap
Rhubarb ha gia' un selettore di riconoscitore, ma **il default e' l'inglese** e
in due dei tre percorsi la combo e' nascosta: su audio italiano PocketSphinx
cerca parole inglesi. Livello 0 = selettore di lingua e scelta ricordata.
Livello 1 (miglior rapporto sforzo/risultato, indipendente dalla lingua) = hold
minimo per viseme, fusione dei troppo brevi, anticipo di 1-2 frame sulle
bilabiali. Livello 2 = Whisper + espeak-ng al posto del riconoscitore, tenendo
la tabella dei viseme (`--datUsePrestonBlair` c'e' gia').

**La cosa che rende il lipsync davvero automatico non e' in COMPETITIVE_ROADMAP**
perche' e' stata scritta senza sapere che `PanelData` esiste: ogni pannello di
storyboard ha gia' `dialog`, `startFrame` e `duration`. Rhubarb accetta il testo
con `-d`, e col testo l'allineamento migliora enormemente **in qualunque
lingua**. Oggi quel testo lo si incolla a mano.

### Deformatori raster — rispondendo a Franco: valgono anche per il vettoriale
MLS e Mean Value Coordinates non sono algoritmi «raster»: sono **mappe di
punti**. Sul vettoriale si applicano ai punti di controllo — niente
ricampionamento, risultato ancora editabile. Due avvertenze: una Bezier
deformata da una mappa non lineare non e' piu' una Bezier (va **suddivisa** fino
a tolleranza), e lo **spessore** va scalato per lo Jacobiano locale o la linea
ingrassa dove non deve. Architettura: scrivere la deformazione come interfaccia
«mappa di punti» e due applicatori. E' gia' il pattern del Plastic.

### Deciso — ZtoRig in pausa
Franco, dopo il collaudo della traccia in gradi: «le correttive impostate cosi'
vanno bene [...] mi fermerei un attimo visto che e' piuttosto laboriosa, ma gia'
cosi' abbiamo degli strumenti utilissimi». **In pausa, non abbandonato.** Da
fare quando si riapre: evidenziare meglio il diamante della posa in edit, e la
**modalita' rig vs animazione** (in rig le operazioni non scrivono chiavi —
riferimento suo: anche Harmony ha una modalita' che non anima davvero). Resta il
taglio automatico sulla giuntura.

### Note — cosa resta aperto
- Conteggio shot per episodio: provato con **un solo shot**, quindi il filtro
  non e' ancora messo davvero alla prova.
- ~~Gli asset gia' scaricati per errore restano nel progetto~~ **CHIUSO**:
  Franco li ha rimossi a mano. *«Adesso che abbiamo fixato il problema non
  dovrebbe ricapitare»* — corretto. **Niente ripulitore automatico: non serve
  piu' e non va riproposto.** Unica condizione: il filtro agisce solo quando
  c'e' un episodio legato; su un progetto legato al solo tvshow, senza episodio
  scelto, `episodeScoped()` lascia passare tutto come prima.
- Ordine dato da Franco per il seguito: Kitsu (fatto), lipsync, deformatori
  raster, libreria di rig riusabili, 2.5D di Rivers, e **auto-shadow per ultimo**.

## [2026-08-14b] — Il disco di articolazione: sei correzioni, e la strada giusta era la prima idea di Franco

### Added — pannello ZtoRig a schede, correttive come TRACCIA IN GRADI
Branch `feature/ztorig-correttive-ui` (`785b0860d`), **da collaudare**.
Le correttive erano a senso unico: il pennello le crea e da li' in poi niente —
nessun elenco, `removeMeshCorrective()` esisteva **senza un solo chiamante**.
Prima ci ho messo una tabella; poi Franco ha detto la cosa giusta: *«una
timeline coi gradi al posto dei fotogrammi, su cui vediamo le correttive come
chiavi»*. E' esattamente come funzionano gli **Smart Bones di Moho**, a cui e'
arrivato da solo senza averli usati.
Il dato non e' cambiato per ottenerla: le correttive nascono gia' incatenate
(il riposo di una e' il pieno della precedente), quindi **erano gia' chiavi su
una traccia, scritte in forma di tabella**. Tabella buttata.

### ⏸️ PARCHEGGIATO — il disco rigido di articolazione
Branch `feature/ztorig-joint-disc` (`50eb4cd95`), preferenza spenta di default.

Il gomito pizzica per un motivo **strutturale**: `verticesToHandles()` mappa un
vertice di scheletro a **UN** punto di comando, e l'ARAP deve far coesistere in
quel punto le due rotazioni degli ossi. Franco l'ha verificato a mano
aggiungendo due vertici «collare» ai lati del gomito: il pizzicamento sparisce.

Il disco — corona di punti sintetici attorno al giunto, raggio = meta' larghezza
dell'arto, rotazione sulla bisettrice — toglie il pizzicamento, si vede nel tool
e si tara. **Ma non puo' dare il bersaglio che Franco ha disegnato**: piegando,
all'interno i due segmenti si SOVRAPPONGONO, e una maglia unica non puo'
sovrapporsi a se stessa. Puo' solo accartocciarsi o aprire un buco. Provato a
Joint Blend 0 e 100: cambia solo quale dei due difetti prevale.

**La strada giusta e' il taglio automatico — la PRIMA idea di Franco, che io
avevo scartato per un motivo sbagliato** (il costo di authoring). Dove il disco
incontra l'arto la maglia si sdoppia in due meta' con calotta circolare; i
vertici della calotta restano condivisi, quindi le due meta' non possono
allontanarsi e si sovrappongono ruotando. Il suo «taglio che non diventa mai
effettivo». Tocca la topologia: sessione a se'.

### I sei errori, che valgono piu' del codice
1. **Accodare a un array condiviso non e' un'aggiunta locale.** Avevo verificato
   UN lettore (il solver) e tirato dritto: `updateHandlesSO` camminava in
   lockstep coi vertici dello scheletro → **crash** (l'assert c'era, in release
   non esiste). Cercare TUTTI i lettori.
2. `compile()` gira **una volta per mesh con lo stesso array**: la corona
   inchiodava anche i pezzi sovrapposti (braccio sopra il corpo) e li distorceva.
3. **Spazio contato due volte**: i punti post-affine sono GIA' in spazio mesh.
   Il raggio usciva scalato per il DPI e il braccio si arrotolava a ferro di
   cavallo.
4. **Nel raccordo si fonde la ROTAZIONE, non il risultato**: mediare due
   posizioni ruotate taglia la corda dell'arco — lo stesso errore misurato
   nell'esperimento sul giro di testa. Si vedeva come un morso al gomito.
5. Tre dei sei erano **tentativi di indovinare a distanza un valore che Franco
   vede in due secondi**. Quando c'e' un numero da tarare a occhio va messo su
   uno slider subito, non dopo il quinto giro.
6. Ieri avevo detto che il file di configurazione in uso e' quello in `stuff/`:
   **falso**, l'app scrive in `ztorycstuff` dentro il bundle. Me l'ero anche
   annotato in passato e l'ho contraddetto.

### Deciso con Franco — direzione
Sculpt e Order **fuori da Animate**: modellare e correggere la forma di un
braccio che si piega e' **rigging, non animazione**. Nel modo di rigging la posa
non deve scrivere chiavi sulla timeline della scena — la chiave la scrive sulla
traccia della correttiva. Riferimenti utili: **Moho** (Smart Bones), **Blender**
(shape key correttive con driver sulla rotazione), **AnimeEffects** (strumenti
separati per dipingere/cancellare influenza, e Posa a parte). **VPaint/VAC non
serve** a questo problema, e dirlo evita di riaprirlo.

### Altro
Isolare i vertici per SO (per dipingere l'ownership quando un braccio sta sopra
il corpo): `getSOOwners()` ha gia' il dato e `m_mvSel` la selezione — manca solo
«seleziona i vertici di questo giunto» e la maschera sui pennelli. Da fare.
## [2026-08-14] — sh110 diventa riproducibile a comando, e due voci si chiudono senza scrivere codice

Sessione lunga, con un filo conduttore involontario: **tre volte su quattro il
lavoro utile e' stato smontare una mia ipotesi con una misura**, non costruire.

### Added — una Quick Toolbar per workflow (`5f42d48e1`)
Leggeva un solo file per utente, uguale in tutte le room. Ora cerca prima
`quicktoolbars/quicktoolbar_<workflow>.xml` e **ricade** sulla barra comune:
finche' non esiste un file per quel workflow il comportamento e' identico a
prima, quindi la novita' non tocca nessuno finche' non la si usa. Il meccanismo
non e' nuovo — le Command Bar con id usano gia' `commandbars/` con la stessa
ricaduta.
Preferenza `ztoryPerWorkflowQuickToolbar` (Xsheet → Scene Tools). Il menu
contestuale e il titolo dicono **su quale barra** si sta per agire: senza, si
personalizza un workflow credendo di toccarli tutti. Reset azzera cio' che si
sta guardando.
`MI_ZtoryShowMesh` e `MI_ToggleKeyframesFollowExposure` nel default, piu' un
**travaso una-tantum versionato**: il file personale vince sul template e non
c'e' nessuna fusione, quindi da soli non sarebbero mai arrivati a chi la barra
se l'era gia' personalizzata.

### 🔴 sh110 — IL DIFETTO E' RIPRODUCIBILE A COMANDO
La cosa piu' importante della sessione. `tcomposer` (nel bundle) renderizza la
scena **headless**, ~5 s a fotogramma. Sul **fotogramma 110** — quello dove
Franco vede cane e gatto ridotti a frammenti — esce lo stesso identico difetto,
e **quattro render danno quattro MD5 uguali** (`69cb901d18ac29f347fc9d2a867413dc`).
Deterministico, da cache fredda, ciclo di prova da sei secondi.

> ⚠️ **Ribalta la direzione dell'indagine.** Valeva «stesso binario, a volte bene
> a volte no → non e' il codice», ed e' la premessa che aveva spinto la caccia
> fuori dal codice per quindici cause. Sul 110 sbaglia **sempre**.

**Escluse CON MISURA su questo caso**, non per ragionamento:
- l'inversione `.inv()` in `doDryCompute` — applicata e ricompilata: l'uscita
  cambia di un **sub-pixel**, il difetto resta identico;
- le tessere — `-maxtilesize 100000` (tessera unica): identico.

**Misurato con la diagnostica nuova** (`ZTORYC_PLASTIC_DIAG=1`, `9f6c85eea`,
verificata **byte-neutra**): sul 110, di 49 colonne, 10 ricevono il deformatore
plastico e **39 no** — fra cui **tutti** i 9 pezzi di PINCO e i 10 di PALLINO,
che hanno padre `Table`. Confermato leggendo il `.tnz`: zombie_01 ha
`Col1→Col2`, `Col3→Col4` e hook; Pinco ha `Col1…Col10 → Table`, tutte.

**⚠️ Ma la spiegazione NON regge, ed e' li' che ci si e' fermati.** Franco ha
verificato che **nel viewer e nella preview il 110 e' corretto**, e il viewer usa
la condizione **identica** (`stage.cpp:339`, `stagevisitor.cpp:1518`). Se la
parentela fosse davvero `Table`, sbaglierebbero entrambi.
Ipotesi non verificata: le due strade percorrono il legame in **versi opposti**
(il render sale dal pezzo al padre mesh, il viewer forse scende dalla mesh ai
figli). Oppure ci sono piu' copie annidate di Pinco e le due strade ne risolvono
una diversa. **Sospeso per decisione di Franco**: «vedremo se ricapita».

**Falso allarme da non ripetere**: dal fotogramma ~145 in poi un solo
personaggio su sfondo bianco **e' corretto** (le colonne finiscono a 141/142/144,
solo la colonna 8 arriva a 277). In un contact sheet un personaggio intero
sembra un frammento: guardare a grandezza naturale prima di dire «riprodotto».

### Added — due candidati upstream nuovi
In `UPSTREAM_PR_CANDIDATES.md`, `plasticdeformerfx.cpp`: `doDryCompute` diverge
da `doCompute` su **due assi indipendenti** — l'affine col primo fattore
invertito (presente identica in **OpenToonz e Tahoma2D**, blame prima del 2016) e
i **flag di maschera** che `doCompute` imposta e `doDryCompute` no (**solo
Tahoma2D**, da `3488987d9`). Quei flag stanno in `TRenderSettings::operator==`,
cioe' l'identita' con cui il render riconosce un risultato in cache: la prova
generale mette in scena uno spettacolo diverso da quello che va in scena.
Restano validi come difetti; **non** sono la causa di sh110.

### Added — `OPENTOONZ_PORT_CANDIDATES.md`
Nato dal metodo di Claudio, con il primo candidato vero: gli **assistenti al
disegno** di OpenToonz. Non «piu' completi», ma **presenti contro assenti**:
~8.400 righe su 42 file (framework, punto di fuga, prospettiva, ellisse,
fisheye, replicatori) che noi non abbiamo affatto. Il costo vero e' che si
agganciano a `TInputManager`, una catena di modificatori fra tavoletta e
pennello, pilotata da tutti e tre i pennelli — proprio i file dove Tahoma ha
divergiuto di piu' (solo sul vettoriale il diff e' ~2957 righe). Piano in tre
tempi, rischio tutto nella fase 2.

### ✅ Loop di camminata — RISOLTO senza scrivere codice
La prova annotata come «da fare prima» ha chiuso la voce da sola: la modalita'
**Part** richiama solo i parametri registrati, quindi copiando una posa non si
porta dietro il piazzamento e il personaggio non torna indietro. Il comando
nuovo **non serve**. `POSE_PARAMS` mescola davvero forma e piazzamento, ma
`Part` lo aggira.
⚠️ Da mettere nel manuale: e' conoscenza d'uso, non di codice.

### IK — misurato l'annealing, NON adottato; poi il pole vector, esperimento fallito
Da `DRAGONBONES_IK_NOTES.md`. L'annealing (tetto che si stringe verso la radice)
misurato su una simulazione fedele del ciclo: sposta lavoro dalla radice alla
punta come promesso (quota radice 36% → 26%) ma sulla convergenza **non e'
monotono** — meglio a meta' corsa, peggio sugli spostamenti ampi. Non adottato;
resta il commento che registra la misura.
Poi Franco: «a volte mettere in posa e' ancora difficoltoso», e alla domanda su
cosa succede: «**scatta o si ribalta**» — cioe' l'ambiguita' di piega, non
l'annealing. Branch `feature/ik-pole-vector` con un rilevatore di ribaltamento
e l'annealing come manopola.
**L'esperimento non ha prodotto prove**, in nessuna direzione: il log non e' mai
stato scritto e **non e' mai stato dimostrato che quel logger sappia scrivere**.
Due giri persi per lo stesso errore di progetto — la diagnostica era armata da
una casella che l'utente doveva ricordarsi di spuntare. Franco ha poi giudicato
l'IK «piu' stabile e controllabile di quel che ricordavo» e ha deciso di
lasciarla com'era. **Branch non mergiato.**

### Removed — `ikccd`, il core IK mai cablato (`354d6f020`)
304 righe che non chiamava nessuno. Non e' stato cablato perche' risolve una
geometria diversa: `assert(parentIndex == i-1)`, **solo catene seriali**, mentre
l'IK vera lavora su un albero di pin con piu' ancore. L'unico argomento per
tenerlo era estrarne il pole vector, e quella strada e' stata chiusa.

### Fixed — il commento che diceva l'opposto del codice (`9f6c85eea`)
`plastictool_animate.cpp`: il commento sopra il cap dell'IK descriveva un tetto
«in DISTANZA» e sosteneva che uno fisso in gradi avrebbe fatto l'opposto di quel
che serve — mentre la riga sotto fa esattamente un tetto fisso in gradi. Residuo
di prima della correzione: avrebbe spinto il prossimo lettore a «rimettere a
posto» proprio la cosa giusta.

### WIP (branch) — pannello ZtoRig a schede + correttive gestibili
`feature/ztorig-correttive-ui`, `785b0860d`. **Da collaudare: Franco ha visto
anomalie non ancora descritte.** Su branch di proposito.
Le correttive di giuntura erano a **senso unico**: il pennello le crea da solo e
da li' in poi niente — nessun elenco, nessun modo di spostare la dissolvenza, e
`removeMeshCorrective()` esisteva **senza un solo chiamante**. Ora una scheda con
una riga per correttiva: giunto guida, riposo→pieno modificabili, **quanto pesa
adesso**, e cancella. A schede (idea di Franco) perche' e' una tabella e ne
arriveranno altre.

### Note — Rhubarb chiama il riconoscitore INGLESE per default
Il selettore esiste (`lipsyncpopup.cpp:245`) ma parte su «PocketSphinx (English)»
ed e' **nascosto** se la sorgente audio e' un file invece di una colonna. Su
audio italiano, di default, Rhubarb prova a riconoscere parole inglesi. Codice
upstream: candidato PR.
## [2026-08-08] — Il merge 1.6.2 entra in master, e una notte a escludere quindici cause senza trovarne una

### Merged — Tahoma2D 1.6.2 su master (`1bfd1c3c1`)
Il branch `merge/upstream-1.6.2` e' stato prima **riallineato a master**
(`a1468a00b`, zero conflitti) e poi portato su master, di nuovo **senza un
conflitto**: il grosso del lavoro era gia' stato pagato il 2026-08-05.
Collaudato da Franco su tutti i punti a rischio — testa di colonna, apertura e
salvataggio di MaggiolataZombie, zona plastica, preferenze, file browser,
pannello Script. Tutto a posto.
Punto di ritorno nel branch **`backup/pre-merge-162`** (`91efad9dd`): se emerge
qualcosa, `git reset --hard backup/pre-merge-162` e si torna esattamente
com'era.

> ⚠️ La build dir del worktree `merge-1.6.2` era configurata **esattamente come
> quella rotta del 2026-08-07** (Qt 5.9.2 del 2017, libtiff44, `Release`,
> deployment target vuoto). Riconfigurata come la CI e ricompilata da zero.
> Questo **invalida** un dato del 2026-08-07: «anche il merge 1.6.2 sbagliava
> uguale» non era un secondo esperimento, era lo stesso esperimento con codice
> diverso.

### Fixed — l'interruttore unificato di visibilita' non parte piu' acceso (`20e967339`)
Franco: «e' sparita l'icona della visibilita' in testa alla colonna». Non era il
merge: upstream 1.6.2 introduce `unifyColumnVisibilityToggles` **con default
`true`**, che sostituisce i due interruttori (occhio preview + camstand) con uno
solo. Peggio del cosmetico: all'attivazione,
`ColumnCmd::unifyColumnVisibilityToggles()` scorre tutte le colonne **sotto-scene
comprese**, forza preview = camstand e marca la scena modificata — e spegnere
la preferenza **non annulla** la riscrittura. Default portato a `false`; la voce
resta in Preferenze → **Scene** (non «Xsheet»), fra `Show Column Parents` e
`Show Column Parent's Color`.
I due interruttori separati servono proprio a chi lavora in produzione: vedere
in tavola cio' che non deve andare in render.

### Fixed — il pannello Script si allinea al modello anche all'apertura (`e058a5d2b`)
Aprendo uno Script da un'altra room, a scena gia' caricata, restava vuoto pur
essendoci uno script importato. Il singleton c'era gia' (`ZtoryModel::scriptFile()`,
persistito nel `.ztoryc`): mancava che il **costruttore** leggesse lo stato
corrente. I due segnali a cui era collegato — `scriptFileChanged` e
`sceneSwitched` — arrivano solo quando qualcosa *cambia*, e un pannello creato
dopo il caricamento non ne riceve nessuno.
Guardia necessaria: se il modello e' gia' pieno si rilegge e basta; solo se e'
vuoto si va a leggere il `.ztoryc`. Su una scena non salvata quella lettura
rende stringa vuota, e scriverla avrebbe **cancellato lo script anche negli
altri pannelli aperti**.

### Changed — `build_and_deploy.sh`, tre reti di sicurezza (`e058a5d2b`)
- **Controllo della configurazione**: confronta la `CMakeCache.txt` con
  `ci-scripts/osx/tahoma-build.sh` e si lamenta forte quando divergono, col
  comando di riallineamento gia' scritto. Nasce dai due giorni persi il
  2026-08-07 su una build dir configurata a mano. `WITH_GPHOTO2` spento e
  CMake 4.x sono segnalati come differenze **note e accettate**, non allarmi.
- **Bundle di destinazione** cercato come `Ztoryc-*.app` invece del solo
  `Ztoryc-SP.app`: un worktree nuovo e' protetto da subito. Con piu' di un
  candidato si ferma invece di indovinare.
- **`ZTORYC_NO_OPEN=1`** per aggiornare il bundle senza lanciare l'app. Serviva:
  due volte in una sera l'app e' partita da sola dopo che avevo detto il
  contrario.
- **`ztorycstuff` viene fuso** al ripristino invece che annidato. Se l'app in
  esecuzione ricrea la cartella, `mv sorgente destinazione` la infila *dentro*:
  si erano formate tre cartelle `ztorycstuff_deploy_*` annidate, che facevano
  fallire la firma con «unsealed contents present in the bundle root».

### Notes — render sbagliato su sh110: SOSPESO, quindici cause escluse
Franco: «lasciamo perdere, vedremo con i prossimi progetti se risuccede».
**Nessuna causa trovata.** Il valore della nottata e' l'elenco di cosa e'
escluso **con misura** — dettaglio completo in `ANIMATIC_TASKS.md`, in cima al
Priority Order:
configurazione della build, il codice (e quindi il merge 1.6.2), cloud,
ffmpeg e formato di uscita, multithreading, tiling, flag di visibilita' colonna,
file mancanti, salti in `doCompute`, allocazione texture, istante di
valutazione, valore del controller squash, mesh caricate,
`addPlasticDeformerFx`, deformazione e stacking order (`process` contro
`processOnce`: scarto **0.000** su 40 pezzi).

**Il dato che regge tutto, ed e' di Franco:** *lo stesso identico binario
scaricato dal repo prima rendeva bene e poi no*, con preview calda in entrambi i
casi. Un binario non cambia da solo → il codice e' escluso per costruzione.

**Due mie conclusioni annunciate e poi ritirate**, entrambe smontate da Franco:
1. *«E' `DefLevelType` nelle preferenze»* — la bisezione scriveva in
   `merge-1.6.2/stuff/...`, dove punta il `SystemVar.ini`, mentre l'app legge e
   scrive in `Ztoryc-162.app/ztorycstuff/...`, **dentro il bundle**. Sei render
   sullo stesso file inerte. Segnale ignorato: **in una bisezione valida deve
   uscire almeno un «buono»**, e uscivano solo «cattivo».
2. *«Le texture non sono ancora caricate»* — e' il normale comportamento a
   quattro tessere del render: un pezzo che sta in un altro quadrante ha
   legittimamente zero pixel. Franco l'ha demolita in una riga: la preview del
   fotogramma la fa sempre prima del render.

Trappole di lettura di quella scena, se si riprende: `+extras` =
`scenes/$scenepath/extras`; le sequenze sono `nome..ext`; nei PSD il `#` separa
il sotto-livello dal file; i bit di visibilita' colonna sono **invertiti**
(bit acceso = nascosto, `txshcolumn.cpp:780` e `:828`).
Prossima misura utile: confrontare il log di un render **buono** con uno
**cattivo** dello stesso fotogramma — serve prima ottenere un render buono a
comando.

### Notes — direzione di prodotto: OTTER
Franco, dopo il default invasivo di upstream: tenere il fork come **software
professionale pensato per le produzioni reali**, prendendo i loro fix e
ignorando le loro decisioni di prodotto — che e' esattamente quello che si e'
fatto stanotte con l'interruttore unificato.
Nome e mascotte gia' pronti: **OTTER**, doppio senso fra «chi usa OT» e la
lontra, con Ryc come personaggio. Divisione ipotizzata: **Ztoryc = la parte di
storyboard** (che continua a esportare verso Tahoma e OpenToonz, e i cui file
`.ztoryc` restano tali), **OTTER = la parte di animazione**. Da decidere ancora
se il rinominare tocca anche l'identita' dell'applicazione (`ztorycproject.xml`,
`ztorycstuff`, chiavi `ZTORYC*`, bundle id) — in quel caso serve **lettura
all'indietro dei nomi vecchi**, o si rompono le produzioni in corso.
Primo riscontro esterno entusiasta arrivato lo stesso giorno, e riguardava la
**parte di animazione** (Plastic tool, Root animabile) — non lo storyboard.

## [2026-08-05] — I DMG che non c'erano, il merge 1.6.2, e mezza giornata a cercare nel codice un difetto che era nella scena

### Fixed — i DMG macOS mancanti dalla 0.12.0 (`52ff4c16e`)
Letto dal log della run, non dedotto: `make` di libgphoto2 si ferma su `ax203.la`
con «Undefined symbols: `_libintl_dgettext`», identico su ARM64 e x86_64.
- `camlibs/Makefile.am` del fork linka ogni camlib solo contro `libgphoto2.la` e
  `libgphoto2_port.la`, **mai contro `$(INTLLIBS)`** che invece
  `libgphoto2/Makefile.am` aggiunge. Su glibc non si vede (`dgettext` sta nella
  libc); su macOS il `libintl.h` di Homebrew lo riscrive in `libintl_dgettext`.
- **Le intestazioni pubbliche si installano in un SUBDIR successivo a `camlibs`**:
  ecco perche' il sintomo era `'gphoto2/gphoto2.h' file not found`, dieci minuti
  dopo e lontano dalla causa.
- Lo script **non aveva `set -e`**: dopo il `make` fallito partiva comunque
  `sudo make install` e il passo tornava zero. Il difetto c'era da mesi,
  mascherato dalla cache di Actions che si rinnovava a ogni rilascio; la 0.12.0
  e' arrivata otto giorni dopo la precedente, la cache era scaduta e lo script e'
  stato eseguito davvero per la prima volta.
- Fix: `--disable-nls` (i18n.h rende identita' le chiamate gettext), `set -euo
  pipefail`, controllo dell'header dopo l'install, clone idempotente.
  **`WITH_GPHOTO2` resta ON.** Verificato: entrambi i DMG sulla release.

### Changed — nomi degli asset con la piattaforma per prima (`d9dc1dd99`)
Sulla v0.12.0 i due pacchetti Windows finivano ai due lati di quelli Linux.
**GitHub ordina gli asset alfabeticamente e non offre ordinamento manuale** —
verificato sui dati (i DMG caricati per ultimi comparivano per primi). Quindi il
nome E' l'ordine: `Ztoryc-<ver>-{linux,osx,win}-*`. Linux e Windows non avevano
la versione nel nome e ora la leggono da `ZtorycVersion.cmake`. Validato su tutte
e tre le piattaforme. Gli asset della 0.12.0 non sono stati rinominati (scelta di
Franco: niente link rotti).

### Merged — Tahoma2D 1.6.2 (`0a430ad42`, branch `merge/upstream-1.6.2`)
61 commit, 249 file, +6689/-3828. Ventuno conflitti, 38 hunk. **Compila a freddo,
ninja rc=0, NON ancora collaudato dentro l'app.** Non portato su master.
Risolti quasi tutti tenendo la nostra versione; tre eccezioni e una fusione:
- `xshcolumnviewer.cpp`: **obbligatoriamente la loro** — il corpo che usa
  `shiftLeft`/`shiftRight` era gia' entrato dall'auto-merge e le dichiarazioni
  stavano solo nel loro lato. Tenendo la nostra non compilava.
- `filebrowsermodel.cpp`: la loro. Upstream ha ristrutturato la funzione e quella
  struttura era gia' entrata: il nostro lato era un doppione che avrebbe
  ridichiarato `repositories`. In piu' aggiunge il nodo Scene Folder.
- `stageobjectutil.cpp` e `functionselection.cpp`: presa la loro **guardia null**,
  tenuta la NOSTRA correzione al centro (`6876cf4e5`). Le due sono concorrenti
  sullo stesso difetto: la loro tocca un solo sito e non copre il percorso del
  toggle di chiave (`RemoveKeyframeUndo::redo()`), la nostra ne copre nove.
- `preferencespopup.cpp`: fusione a mano — l'import ora cerca lo stuff come
  `ztorycstuff` e `tahomastuff`, sia alla radice sia in `Contents/Resources`.
- `tiio_tzl.cpp` sembrava il peggiore (conflitto su 4969 righe) ed era il piu'
  banale: **upstream ha il file in CRLF e noi in LF**, quindi ogni riga risulta
  diversa. La loro unica modifica reale era un refuso in un commento.

### Added — toggle «Show Mesh» globale e persistente
Il flag `m_drawMeshesWireframe` viveva dentro le impostazioni del Plastic tool e
finiva nel viewer solo mentre quel tool era attivo: irraggiungibile altrove, e
perso al riavvio (stava in `viewer->visualSettings()`, per-viewer e mai salvato).
Ora c'e' **una sorgente unica** — `PlasticVisualSettings::s_showMeshWireframe`,
statico in `include/ext` cosi' lo vedono sia tnztools sia toonz — persistita con
`TEnv` e riletta dal viewer a ogni disegno. Comando `MI_ZtoryShowMesh` (menu
Xsheet, aggiungibile alla Quick Toolbar), icona `ztoryc_show_mesh`. Il menu del
Plastic tool scrive lo stesso flag: due viste, un valore.
> Trappola pagata al primo link: la variabile globale era finita **dentro il
> namespace anonimo** di `plasticdeformerstorage.cpp` — collegamento interno,
> invisibile da tnztools. Un simbolo che attraversa un confine di modulo va
> esportato **e** deve stare a livello di file.

### Notes — la caccia al render plastic: la causa era nella SCENA
Segnalato da Franco: renderizzando `sh110` spariscono pezzi di personaggio e
alcune parti sembrano spostate. Viewer corretto, render no. **Mezza giornata di
indagine nel codice, e il codice non c'entrava**: reimportare la scena in una
nuova come sotto-scena ha ricostruito tutto e i pezzi sono tornati.

**Nove ipotesi cadute, tutte su misure e non su ripensamenti** — vale la pena
averle scritte, per non rifarle:
1. chiave di render/cache — smentita: 337 alias distinti, **zero** collisioni;
2. clustering di fotogrammi — i pezzi mancanti erano di un solo personaggio;
3. concorrenza fra task di render — gia' a CPU singola;
4. corsa in `PlasticDeformerStorage` — e' protetto da mutex;
5. sfratto delle texture dalla QCache — con tile Small identico, e la differenza
   fra tile grandi e piccoli non e' netta;
6. RAM/scena pesante — **artefatti identici fra render diversi**, quindi
   deterministico: la pressione di memoria non si ripete al fotogramma;
7. stacking order (commit `a9263e0a2`) — le facce spariscono, non finiscono
   dietro; e la 0.11.0, che quel codice non ce l'ha, falliva identica;
8. pin/IK — spegnendoli il difetto resta;
9. ramo a 64 bit di `doCompute` — il formato di output era a 8 bit.

**La misura che ha chiuso la caccia, ed e' la lezione**: salvare su PNG la
**texture in ingresso** alla fx. Dal main arrivava un ritaglio 174x131 con
**quattro zampette e nient'altro**; da dentro la sotto-scena, stesso personaggio
e stesso frame, 1114x833 con il cane intero. Il plastic disegnava fedelmente cio'
che riceveva. **Andava fatta per prima, non per nona.**

Sospetto rimasto, da verificare: il **controller** (l'affine «sopra» il risultato
deformato, `getSquashControllerAffine`) misurato una volta a
`[1,0,462.3,0,1,-2.17]`, cioe' una traslazione. Franco: «se lo uso per
riposizionare un elemento funziona nel viewer ma non nel render».

### Upstream candidates
Due difetti veri trovati strada facendo, **entrambi di Tahoma2D**, annotati in
`UPSTREAM_PR_CANDIDATES.md` e **non** applicati (nessuno dei due e' il difetto di
oggi, e vanno collaudati con calma):
- `PlasticDeformerFx::getAlias` ignora `m_texPlacement`, la cella di mesh e il
  dpi: frame diversi possono finire nello stesso `RenderTask`;
- la texture caricata a ogni tile non viene mai scaricata (`unloadTexture`
  commentato con un punto interrogativo).
Piu' quello trovato leggendo i conflitti del merge: `setCenter()` che riceve il
centro **grezzo** e fa derivare il pivot quando l'handle non e' `B`.

## [2026-08-04] — Tre crash, quindici curve, e due Tahoma puliti da cui guardare

### Fixed — il crash dell'Explode, per davvero (`80097adef`)
Un fix del mattino (`c9cd8d137`, altra sessione) era compilato e dentro il
binario, e **il crash restava identico**. Ripreso da capo sotto lldb: SIGSEGV in
`TStageObject::setParent` su `ldr x0, [x19, #0x70]`, `address=0x70` — cioe'
**`this` nullo**, non l'argomento.
- La catena: il ciclo che porta fuori le colonne **salta le colonne pegbar**,
  quindi per quelle `ids` non ha voce; il ciclo del parenting scorre TUTTE le
  colonne e `QMap::operator[]` su chiave assente **inserisce in silenzio** un
  `TStageObjectId` di default, che e' `NoneId`. `setStageObjectParent` ha
  `assert(id != NoneId)` e subito dopo un deref **senza guardia**: l'assert non
  e' compilato in release. L'albero rifiuta `NoneId` (guardia anti-BadPegbar del
  21/7) e restituisce 0.
- Fix alla causa, **un file solo**: `ids.contains()` + `ids.value()` invece di
  `operator[]`.
- **Tre diagnosi sbagliate prima di questa**, tutte per lettura del codice. Le
  ha smontate lldb. La piu' istruttiva: «`setDagNodePos` avrebbe crashato prima»
  non regge, perche' in build ottimizzata uno store inline su `this` nullo e' UB
  e il compilatore lo puo' spostare o eliminare — **il crash affiora alla prima
  chiamata VERA, non al primo deref**.

### Fixed — l'oggetto che si spostava cancellando una chiave (`6876cf4e5`)
Segnalato da Franco: si cancellano le prime chiavi per ripartire dalla posizione
dell'ultima, e l'oggetto si sposta — **con i valori alle chiavi identici**.
- Causa: dopo ogni chiave rimossa si azzeravano e ricalcolavano `m_frameCenter`
  e `m_offset`. Nessuno dei due e' un canale chiavato, ed **entrambi entrano nel
  piazzamento** (`position = puntoSpline - m_frameCenter`, `pos = m_offset +
  position`): ecco perche' si vedeva con la spline **e senza**.
- Il blocco era copiaincollato in **otto** punti — motivo per cui il primo
  tentativo sembrava non funzionare: il toggle della chiave non passa da
  `keyframeselection.cpp` ma da `UndoRemoveKeyFrame::redo()`. Gli `undo()` che
  ripristinano il centro salvato sono corretti e non sono stati toccati.
- Fix: **un metodo solo**, `TStageObject::resetFrameCenterIfUnanimated(frame)`,
  che azzera solo quando non resta nessuna chiave — il caso per cui il reset era
  stato scritto. Collaudato da Franco su **entrambi** i gesti.

### Added — preset di easing (`07e6528f7`)
Le quindici curve nominate del vocabolario motion design (Sine/Quad/Cubic/Quart/
Expo x In/Out/In-Out) nel menu contestuale del grafico, in submenu per famiglia.
Numeri **pubblicati** da easings.net/CSS, non riderivati. Maniglie come frazioni
del segmento, quindi stesso carattere su sei frame e su sessanta; il conto
combacia con la costruzione dell'evaluator, percio' e' la definizione CSS esatta.
- «In» e «Out» vogliono dire **il contrario** nelle due tradizioni: le voci
  dicono l'effetto accanto al nome — *In (slow start)*, *Out (slow end)*.
- Overshoot e bounce **fuori di proposito**: richiedono di generare keyframe.
- Collaudato da Franco su Expo In.

### Added — infrastruttura per le PR upstream
- Worktree **`tahoma-stock`** (da `upstream/master`) e **`opentoonz-stock`**
  (da `opentoonz/master`), remote `opentoonz` con push DISABLED.
- **Tahoma2D stock NON si compila** su Apple Silicon con toolchain corrente:
  quattro aggiramenti (CMake 4 x2, `libsuperlu_4.1.a` senza arm64,
  `TIFFReadRGBATile_64`) piu' `SystemVar.ini` completo o crasha all'avvio in
  `StartupPopup::loadPresetList`. Ora compila. Candidato PR a se'.
- Workflow scritto in `UPSTREAM_PR_CANDIDATES.md`: bersaglio verificato con
  `git show <remote>/master:<file>`, poi `/code-review ultra`, poi **revisione
  umana di Rodney** (manutentore OpenToonz) aprendo la PR **sul fork**, poi invio.

### Notes — bersagli verificati
Il difetto delle chiavi e' **solo Tahoma2D** (nasce da `baecf7504` del 19/4/2026;
OT non ce l'ha). Crash Explode, corruzione heap di `xshcellmover` e l'assert di
`getEaseHandles` sono invece in **entrambi**.

### Aperti
- 🔴 **Crash incollando effetti da una sotto-scena al main, al SECONDO paste** —
  `FxSelection::replacePasteSelection()`, file **identico a upstream**.
  Replicabile. Cattura lldb gia' presa in
  `/Volumes/ZioSam/tahoma2d-workspace/lldb_paste_crash.txt`: **da li' si riparte**.
- Verifica su `tahoma-stock` del fix chiavi (l'app stock ora parte).
- Feature request In/Out per sotto-scena — taglio deciso: serve **animando**,
  non solo per l'animatic; isolare la parte generale dal fallback Ztoryc.
- Speed graph: deciso riquadro sotto ad asse condiviso, **solo curve
  selezionate** (come *Only Show Selected* di Blender). Non iniziato.

## [2026-08-03d] — Il percorso dalle chiavi, e una testa che gira attorno a un ovale

Giornata lunga e in tre tempi: chiuso il Function Editor, mergiato ZtoRig su
master, e poi una deviazione che è diventata la cosa più interessante.

### Added — Function Editor: il percorso dalle chiavi
- **Generate Path from Keys** (menu della colonna keyframe dell'xsheet, non del
  grafico: prende un OGGETTO e ne cambia il modo di muoversi). Tre chiavi x/y
  fanno un movimento con un ANGOLO alla chiave centrale quando si voleva un
  arco; il comando costruisce la spline che passa per le posizioni delle chiavi,
  ci aggancia l'oggetto e converte le chiavi in `posPath` alla percentuale di
  lunghezza di ciascun punto. Stessa matematica dell'Auto Bezier, **nello spazio
  invece che nel tempo**.
- x e y **restano intatte**: su un percorso non vengono lette affatto
  (`computeLocalPlacement` fa uno switch sullo stato), quindi staccare la spline
  riporta il movimento originale. Verificato leggendo il codice, non dedotto.
- Uno stroke Toonz è a QUADRATICHE e un punto di controllo utente è ogni quarto
  punto grezzo: rispettare quel layout è ciò che rende la spline generata
  **modificabile a mano** col Control Point Editor, che era il requisito.

### Modified — due comportamenti di ieri, cambiati con motivo
- **Auto Bezier: prima e ultima chiave ora PIATTE.** Fuori da esse il valore è
  tenuto, quindi la pendenza in arrivo alla prima è zero: darle quella del
  segmento metteva uno scalino di velocità — da fermo a pieno regime — proprio
  dove è più grosso, cioè il difetto per cui l'Auto Bezier esiste. Effetto
  collaterale voluto: su **due sole chiavi** ora dà un ease invece di una retta.
- **Even Speed Along Path porta a Linear** i segmenti dello span rovato. Metteva
  le chiavi nei frame giusti e lasciava l'interpolazione com'era: manteneva la
  promessa **sulle chiavi e non fra le chiavi**. Linear e non auto bezier di
  proposito, così restano raggiungibili entrambe le letture — Rove da solo =
  velocità costante netta, Rove *poi* Auto Bezier = costante con ease alle due
  estremità (il roving-con-easy-ease di After Effects).

### Added — canali INERTI segnalati nel Function Editor
Nato da un incaglio reale: Franco ha guardato la curva X di un oggetto appena
messo su un percorso, l'ha trovata malfatta e ha segnalato un difetto delle
tangenti **che non c'era**. Su un percorso x e y non vengono lette, ma le loro
curve restano in lista identiche alle vive. Ora sono sbiadite nell'albero (col
perché nel tooltip) e nel grafico. Il criterio chiede `isPathEnabled()`
all'oggetto — lo stesso test di `computeLocalPlacement` e `updateKeyframes` —
così albero e motore non possono divergere.

### Merged — ZtoRig su master (`f67e807c7`)
Scoperto che **ZtoRig era già su master** da `3a26c2562`: l'appunto che diceva
il contrario era vecchio e l'ho ripetuto senza verificarlo. Il branch conteneva
solo l'ultimo scaglione, 5 commit e 8 file. Merge pulito, zero conflitti.
- pennello delle correttive di giuntura (milestone 2): distanza **lungo la
  maglia**, non a schermo
- stacking order su selezione multipla di giunti, un solo undo
- **riaccendere l'IK non fa più saltare il personaggio**
- canvas dei thumbnail per SCENA e non per progetto
- lock di istanza singola **per bundle**, così master e SP girano affiancate

### Fixed — bear: due scene che non si aprivano
Scene AI ricevute da terzi. I `.tnz` chiedono `$scenefolder/../levels/`, cioè
una cartella sorella mai consegnata; e **le due scene puntano alla stessa
cartella** chiedendo `torso`/`head` da viste diverse, che non possono
coesistere. Ricostruito il layout in `scenes_fixed/`, una cartella per scena,
senza toccare gli originali.

### Notes — cosa hanno fatto davvero con l'AI
**Non hanno generato immagini: hanno generato codice.** Sei viste di turnaround
più cinque pose di gesto sono l'unico ingresso disegnato; tutto il resto è
Python. Due strade: ritagliare i pezzi dalle tavole (12.000 colori = inchiostro
vero) oppure **ridisegnare l'orso a colpi di ellissi** (4 colori, zero
antialiasing, coordinate battute a mano validate per IoU contro il model sheet).
L'animazione è `math.sin(...)` valutato a ogni frame e **bakeato**: 64 chiavi su
64 frame, tutte Linear. Su `bear_run`, dieci chiavi ne riproducono la curva
entro il 2%.

### Notes — studio: VPaint/VAC, LayerInbetween, e cosa ha già Tahoma
- **`TInbetween` non è un lerp**: 1574 righe con individuazione degli spigoli,
  accoppiamento del sottoinsieme migliore, **affine propria per sotto-tratto** e
  reiezione degli outlier a 2.5σ. Ma **accoppia i tratti per INDICE e tronca al
  più corto** → candidato upstream.
- **Il disegno guidato lo aggira già**: passa a `TInbetween` due immagini da un
  tratto ciascuna, con la coppia scelta dall'utente via stroke picker. Ma la
  scelta è transitoria (due `int` sul viewer) e il risultato è bakeato.
- **Il VAC** (Apache 2.0) dice che il nostro piano era sbagliato: chiama
  *sequential keyframing* il modello a ID persistenti sui punti e mostra che
  fallisce per costruzione quando i tratti nascono, muoiono, si uniscono. La
  corrispondenza è **un oggetto fra due chiavi**. Codice non montabile (28k
  righe, `Cell` si disegna da solo in OpenGL, geometria a polilinea contro le
  nostre Bézier).
- **LayerInbetween** fuori portata: GPL, PyTorch + SAM2 + due reti esterne, e
  fallisce sui cicli di camminata e i giri di testa. La sua interpolazione, però,
  è **lineare**: il contributo è tutto nel trovare la corrispondenza.

### Notes — proxy 3D per il giro di testa (esperimenti, su ZioSam)
Idea di Franco: un ovale 3D che guidi l'intercalazione. È il limite che
**entrambi i paper dichiarano** e non sanno superare.
- errore del 2D contro la geometria: 1.6% a 30°, 5.5% a 60°, **13.2% a 90°**
- **l'occlusione esce dalla sola normale** e a angoli sensati
- le **correttive** vanno espresse in coordinate di SUPERFICIE, non di schermo:
  a beccheggio 0 coincidono, a ±28 divergono del 5.2%
- e lì la **corrispondenza sparisce**: una correttiva è un *edit* della base,
  stessi punti spostati — come le blend shape in 3D
- **sul disegno vero di Franco** (tre viste, comando di dump nuovo):
  **occlusione 5 su 5** — ogni tratto che ha tolto, la geometria lo dichiara
  nascosto da sola; **piazzamento sotto il 5%** con un ovale piazzato a occhio;
  il **naso al 27%** perché di profilo non è il naso ruotato, è un tratto NUOVO
- l'imbardata stimata come unico parametro libero è uscita **44.8°** e **80.6°**:
  il turnaround è geometricamente coerente
- la ricostruzione completa del controller è **buona da −45 a +45 e sporca agli
  estremi**: tre bug in fila, tutti trovati guardando l'immagine e nessuno
  visibile dai numeri. Segnale che la cosa va vista dal vivo, dentro Tahoma.

### Added — comando di dump dei tratti (strumento, non feature)
`File > Export > Ztoryc > Dump Vector Level (debug)`. Trappola dei **due
menubar.xml** ignorata benché annotata in memoria: costata un giro a vuoto.

## [2026-08-03c] — Le curve ondivaghe: Auto Bezier, Flat, tangenti, Rove

Partiti da un problema di Franco: si mettono le chiavi principali, poi i
breakdown per drag/lead/follow, e le curve diventano «ondivaghe». Sono **due**
difetti che arrivano insieme — le maniglie di ogni segmento sono calcolate da
sole (`segmentWidth/3`), quindi a una chiave la pendenza in entrata e quella in
uscita non coincidono e la curva ci arriva in un modo e riparte in un altro; e
maniglie piu' lunghe del dislivello locale fanno **uscire** la curva
dall'intervallo fra due chiavi.

### Added — strumenti sulle tangenti (menu tasto destro del grafico)
- **Auto Bezier** — tangente dai **vicini** (Catmull-Rom), quindi continua per
  costruzione, poi clampata perche' la curva non possa sorpassare
  (Fritsch-Carlson). E' l'Auto Clamped di Blender / l'Auto di Maya.
- **Flat** — tangente orizzontale: marca un estremo, e disfa un Auto Bezier che
  ha indovinato male.
- **Copy / Paste Tangents** — copia la **forma**, non i numeri: maniglia come
  frazione della larghezza del segmento e della sua salita, quindi incollata
  altrove da lo stesso carattere con ampiezza diversa.
- **Rove Keys** — sposta le chiavi **nel tempo** per velocita' costante fra le
  due che le racchiudono. Solo su `posPath`, documentato come «percentuale
  della lunghezza della spline»: li' velocita' costante del valore **e'**
  velocita' costante lungo il percorso, esatta.
- Opzione **Auto Bezier Tangents While Setting Keys** (Preferences → Animation),
  agganciata a `createKeyframe`, che ricalcola anche le **due chiavi vicine**.

### Notes — il nome, che e' costato la collisione giusta
Franco ha chiesto se «smooth» fosse il termine corretto. Non lo era: in Blender
`Smooth Keys` e' un filtro che media i **valori** e sposta l'animazione — e
c'era **gia' una voce "Smooth"** in quello stesso menu contestuale, a cinque
righe di distanza (la Curve Shape di OpenToonz, dal primo commit del 2016).
Il suo dubbio ha evitato due voci uguali con significati opposti.

### Notes — due decisioni di Franco migliori delle mie
- **Rove sposta la chiave INTERA** dello stage object, non il solo posPath.
  La mia motivazione era debole («altrimenti nell'xsheet sembra sdoppiata»);
  la sua e' sostanziale: **lo zoom e la rotazione a quella chiave esistono
  perche' la camera e' in quel punto del percorso**. Sono una cosa sola.
- **Path e Roving sono due comandi separati**, non uno. Rispondono a due
  domande diverse (forma / tempo), si compongono meglio, e **meta' era gia'
  fatta**.

### Fixed — il Linear, chiuso per davvero
Seguito di `d2dc6bd88`, che correggeva il tipo ma non l'easing. Due difetti in
fila, trovati **con una diagnostica** e non per deduzione: (1) sostituivo
`m_type` ma non le maniglie, a zero perche' la chiave copiata da ultima non
governava niente — un ease di ampiezza zero e' indistinguibile da una retta;
(2) l'incolla chiedeva la chiave precedente allo **stage object**, ma una chiave
puo' essere **parziale**: con solo Y chiavato alla riga prima, ogni altro canale
veniva scartato e teneva il Linear degli appunti. Ora la domanda va al
**canale**. La diagnostica e' stata **tolta** a fine giornata.

### Fixed — Rove Keys non registrava alcun undo (SIGSEGV)
`TStageObject::moveKeyframe` e' della famiglia *WithoutUndo*. L'avevo avvolta in
`beginBlock`/`endBlock`, ma un blocco e' solo un contenitore: dentro non c'era
niente. Stack e scena divergevano, e il Ctrl+Z successivo rigiocava
`UndoStageObjectMove` contro uno stato che non si aspettava. Aggiunto
`RoveKeysUndo`, fotografia della tabella chiavi prima/dopo.

> **Lo schema, terza volta in due giorni:** una convenzione che sta **un livello
> piu' sotto del nome che leggi**. `moveKeyframe` non dice che non fa l'undo: lo
> dicono i metodi che chiama. Come `getStageObject(id, false)` non dice che puo'
> tornare null: lo dice l'argomento.

### Notes — DA FARE, progetto pronto in ANIMATIC_TASKS
**Generate Path from Keys**: crea la spline dalle chiavi selezionate. Feasibility
verificata (`createSpline()`, `setSpline()`, l'arc-length di `TStroke`), i passi
elencati, e in cima la cosa da controllare **prima** di scrivere una riga: cosa
fanno X e Y quando l'oggetto ha gia' una spline.

## [2026-08-03b] — Il Function Editor sta in piedi da solo, senza una curva corrente

Coda della giornata: due ricadute del «click nel vuoto deseleziona» chiesto da
Franco, piu' la risposta tecnica alla community. Commit `387d67fe7`.

### Fixed — due percorsi davano per scontata una curva corrente
Da quel gesto in poi una curva corrente **non c'e'**, ed era il presupposto di:
- **`openContextMenu`**, che usciva subito su `if (!curve) return;` — prima del
  blocco che sceglie la curva fra quelle coi segmenti selezionati. Tasto destro
  su un gruppo di segmenti preso col rettangolo: non compariva **l'intero
  menu**, non solo la voce dell'interpolazione. Ora la curva si decide prima di
  rinunciare: la corrente, poi quella con un segmento selezionato sotto il
  cursore, poi la piu' vicina al puntatore.
- **`mousePressEvent`**, dove tutta la gestione del click sta dentro
  `if (currentChannel)`: senza corrente non veniva eseguito niente, ne' la
  deselezione ne' l'avvio del rettangolo. **Si auto-alimentava**, perche' e'
  proprio il click a vuoto ad azzerare la corrente — l'unico modo per uscirne
  era selezionare una curva.

Collaudato da Franco: «ora è perfetto».

### Notes — la lezione, che e' la stessa di ieri con un vestito nuovo
Tre giri di correzioni sulla **stessa** modifica, ogni volta sul sintomo appena
segnalato. Cercare **tutti i dipendenti di `currentChannel`** al primo giro li
avrebbe chiusi insieme, e sarebbe costato una grep. Ieri era «dedurre invece di
misurare»; oggi e' «correggere dove me lo dicono invece di dove sta».

### Notes — risposta alla community (OpenToonz/Tahoma2D)
Hanno chiesto i riferimenti al codice «spento» e hanno segnalato che la
scalatura nel tempo somigliava a lavoro recente di Shun. **Hanno ragione**:
`StretchPointDragTool` e' `fcfdb2c85` di shun-iwasawa (2023-02-10), ed e'
**gia' in Tahoma2D** — verificato con `git merge-base --is-ancestor` contro
`upstream/master`, quindi non c'e' nessun port da fare. Il nostro contributo e'
solo il **multi-curva**: il suo impacchettamento sui frame interi non e' stato
riscritto, solo separato in "decidi l'intervallo" / "applicalo", con un pivot e
un rapporto unici presi dagli estremi dell'intera selezione.

Riferimenti dati (righe **prima** delle nostre modifiche, cosi' corrispondono al
loro albero): `functiontreeviewer.cpp:1401` (`setSelectionMode(NoSelection)`)
contro `treemodel.cpp:391` e `studiopaletteviewer.cpp:159`;
`functiontreeviewer.h:248` (`setCurrentStageObject`, salva e basta);
`functionselection.h:47` (keyframe gia' per-curva) contro
`functionpanel.cpp:1269` (`selectNone()` incondizionata);
`functionselection.h:50` (`m_selectedSegment` singolo int) e
`functionpanel.cpp:1570-1572` (il menu **azzera** la selezione prima di
applicare).

### Notes — stato
Franco considera il Function Editor **completo** per questa fase. Le tre voci
escluse sono ora in `ANIMATIC_TASKS.md` con la valutazione di fattibilita':
speed graph, funzioni linkate, preset di easing. Nessuna e' bloccata dal
modello dati — la nota iniziale che diceva il contrario per le funzioni linkate
era **sbagliata** e va corretta ovunque compaia: le espressioni ci sono gia'.

## [2026-08-03] — Giro sistematico sui crash: 19 dereferenziamenti non guardati

Rilettura dei crash log utente del 28-31 luglio, e poi il giro su **tutti** i
chiamanti invece di aspettare che si presentassero uno alla volta. Commit
`6bbc75d25`, 19 siti corretti su 27 esaminati, 10 file.

### Fixed — dai crash log
- **`KeyframesDeleteUndo`** (SIGSEGV, 30 lug, **due volte in 20 minuti** sulla
  stessa scena): `doDelete()` **compatta** il vettore delle colonne saltando
  quelle inutilizzabili, e quell'indice veniva poi passato a
  `FunctionSheet::getStageObject()`, che si aspetta un indice di colonna del
  foglio. Nel caso buono leggeva l'oggetto sbagliato; oltre il limite tornava
  null, dereferenziato. In **tre punti** — costruttore, `undo()`, `redo()` —
  piu' un `release()` su un `m_param` che il costruttore poteva aver lasciato
  null. Ora lo stage object si risolve **dalla curva** e al momento in cui
  serve, cosi' sopravvive alla colonna eliminata mentre l'undo e' nello stack.
- **`TKeyframeData::setKeyframes`** (SIGSEGV, 29 lug, incollando keyframe):
  `assert(pegbar)` seguito dal dereferenziamento, e **in release l'assert non
  esiste**. Puo' essere null davvero: con `col` negativo l'id ripiega sulla
  camera, e `CameraId(-1)` su un xsheet senza colonna camera e' invalido.

### Fixed — il giro
- **Plastic tool, 15 siti su 23.** Invece di quindici guardie sparse, cinque
  wrapper null-safe accanto a `sdFrame()`, dove la guardia gia' viveva.
  Fra i corretti, `PasteDeformationUndo` chiamava `stageObject()` nel
  costruttore **e** in `undo()`/`redo()`: bastava cancellare la colonna e
  premere Ctrl+Z.
- **Function Editor, 10 siti.** Tutti sul percorso `W_DrawingNumber`, dove una
  colonna sembra scontata: non lo e', perche' `getColumnIndexByCurve` risponde
  **-1** per una curva che non e' fra i canali attivi.
- **Schematic, 2 siti.** `getStageObject(id, false)` — l'argomento significa
  *"non creare, torna null se non c'e'"* — dereferenziato lo stesso.

### Notes — il filo comune, da ricordare
Quando a luglio abbiamo insegnato all'albero degli stage object a **rifiutare**
gli id invalidi invece di creare zombie `BadPegbar`, ogni chiamante che non
controllava e' passato da corruzione silenziosa a **crash immediato**. Era il
prezzo giusto — ma andava pagato **subito su tutti i chiamanti**, non uno per
crash. Tre crash in tre settimane erano lo stesso difetto visto da tre lati.

Upstream ha ancora il comportamento vecchio: li' questi siti producono
**zombie invece di crash**. E' l'argomento per la PR — non «ci crasha», ma
«vi sta scrivendo oggetti spazzatura nelle scene, in silenzio».

### Notes — crash ancora aperti
- **Gomma sul vettoriale** (29 lug 10:42, `TVectorImage::Imp::computeRegions`
  durante `EraserTool::erase`). Non toccato di proposito: su quel codice i
  rattoppi locali sono gia' falliti cinque volte. Serve la repro.
- **Render con TimeShuffleFx** (29 lug 11:33, `doGetBBox`). Serve sapere se in
  `sh100` c'e' un Time Shuffle e com'e' cablato.
- Il crash Windows di **Pietro** (31 lug, Plastic tool) era **gia' corretto** in
  v0.11.0 (`ccdd2ca89`, 25 lug): la sua 0.10.1 e' di tre giorni prima del fix.

## [2026-08-02b] — Il Function Editor sapeva gia' quasi tutto: mancava accenderlo

Sessione intera sul Function Editor, piu' due bug fix nel core. Tutto su
**master**, mergiato e pushato. Il filo che tiene insieme la giornata: quasi
niente qui e' codice nuovo. `setSelectionMode(NoSelection)` in un costruttore
mentre la classe base ha gia' il ramo per `ExtendedSelection`;
`setCurrentStageObject()` che salva un puntatore e non lo usa;
`FunctionSelection` che tiene gia' i keyframe di **piu' curve** mentre il
grafico azzerava la selezione a ogni cambio di curva; `m_selectedSegment` come
singolo `int`, col menu che per giunta chiama `selectSegment()` — cioe'
**azzera la selezione** — prima di applicare l'interpolazione.

### Added — albero
Aggancio allo xsheet: scegliendo una colonna, il nodo si apre, ci si scrolla
sopra e la riga viene selezionata. Casella di ricerca per nome (`Ctrl+Shift+F`;
`Ctrl+F` e' l'FX Browser) che passa dallo **stesso** `applyShowFilter` del
filtro animati, cosi' i due si compongono, e che **nasconde senza spegnere** —
il filtro animati chiama `setIsActive(false)`, e riusare quella strada avrebbe
restituito il grafico vuoto a fine ricerca. Filtro "Animated only" globale.
Selezione multipla (Shift/Cmd e trascinamento sui nomi; il trascinamento che
parte sulle **icone** resta quello che accende e spegne una fila). Menu di
visibilita' come quello delle colonne dello xsheet, con l'ambito scritto
nell'etichetta. Sottomenu Interpolation che ricambia ogni segmento di tutte le
curve selezionate in un solo undo. Toggle "Open Selected Column Only".

### Added — grafico
Selezione di keyframe **e di segmenti** su piu' curve. Il rettangolo prende
quello che ci sta dentro. Trascinamento in blocco in tempo e in valore, ogni
curva convertendo lo stesso spostamento in pixel attraverso la propria scala.
Scalatura nel tempo su piu' curve con **un solo pivot e un solo rapporto**,
presi dagli estremi dell'intera selezione: l'impacchettamento sui frame interi
non e' stato riscritto, solo separato in "decidi l'intervallo" e "applicalo".
Il tasto destro segue il segmento selezionato sotto il cursore invece della
curva corrente. Hint contestuali nella barra di stato, coi nomi dei tasti della
piattaforma — emessi come segnale e cablati in `tpanels.cpp`, perche' toonzqt
**non puo' linkare TApp** (scoperto sbattendoci: `flipconsole.cpp` include
quell'header ma non ne usa i simboli).

### Added — leggere un grafico con piu' colonne dentro
Una regola sola: **il colore e' il canale, il tratto e' una colonna che non e'
quella corrente, lo spessore e' la curva su cui stai lavorando.** La colonna
corrente e' tutta continua. Prima versione mia sbagliata (continua = prima
colonna disegnata) e **rifatta da Franco**: quel che serve e' vedere subito
quali curve sono di quella colonna, non distinguere fra loro le altre.

### Fixed — il Linear che ricompariva al posto della Default Interpolation
Il tipo di una chiave descrive il segmento che la **segue**, quindi l'ultima
chiave non ne governa nessuno: `TDoubleParam` le mette `Linear` come
**segnaposto**, e lo riscrive **a ogni caricamento di scena** — per questo il
difetto sembrava capriccioso. Diventa un segmento vero quando quella chiave
smette di essere l'ultima passando da una strada che la **copia** invece di
crearla: `moveKeyframe` e l'incolla in mezzo. La creazione normale era gia'
corretta. Corretto per entrambe, toccando solo i canali in cui la chiave
**era** l'ultima. Non verificato su repro: Franco lo collaudera'.

### Fixed — SIGSEGV trascinando keyframe su piu' curve
Il ripristino dei drawing number era guardato dalla **colonna**, mentre
`click()` riempie `m_startFrames[i]` solo per `W_DrawingNumber`: per ogni altra
curva di quella colonna il set e' **vuoto**, e il ciclo lo percorreva `kCount`
volte senza confrontare l'iteratore con `end()`. Latente da prima (la modalita'
group lo espone gia'), reso immediato dalla selezione multi-curva. Stessa
famiglia in `release()`, dove `getStageObject()` puo' tornare null.

> **Lezione sul crash log:** il backtrace della release nominava solo
> `MovePointDragTool::drag`. L'offset `+1356` e' l'indirizzo di **ritorno di una
> `bl`**, non l'istruzione che fallisce: e' stato il **disassemblato** a quell'
> offset a dare il punto vero. Cfr. la nota su lldb vs crash log.

### Notes — due volte ho corretto senza avere la repro
Il primo fix allo Shift-click e' stato **incompleto** (tolta la distruzione
della selezione, ma l'aggiunta non avveniva comunque: i gadget descrivono solo
la curva corrente), e il primo fix al "riclicca la curva e torna continua" era
**nella funzione giusta ma mai chiamata** in quel percorso. Entrambe le volte
la diagnosi era dedotta invece che misurata. Cfr.
`feedback_instrument_before_optimizing`.

### Upstream
Registrati in `UPSTREAM_PR_CANDIDATES.md`: la feature request (da proporre
**sia a Tahoma2D sia a OpenToonz**, decisione di Franco) piu' i due bug fix,
proponibili da soli. Testo inglese pronto in **Drive → Ztoryc →
`FUNCTION_EDITOR_UPSTREAM_EN.md`**, con la traccia del video in coda.

## [2026-08-02] — I thumbnail smettono di sconfinare fra scene. E ZtoRig: pennello delle correttive e stacking order

> Il fix dei thumbnail e' su **master** (`a23416446`). Tutto il resto e' sul branch
> `feature/ztorig-pose-blend`, **non collaudato**: scritto e compilato, mai visto
> funzionare se non dove indicato.

### Fixed — il canvas dei thumbnail e' della scena, non del progetto (master)
Aprendo una scena nuova si trovavano griglia, disegni e riquadri uniti della
scena precedente. Il canvas veniva salvato risolvendo la cartella dei **livelli**
(`+drawings`, che appartiene al **progetto**) e il nome del file portava solo la
dimensione della griglia: tutte le scene di un progetto leggevano e scrivevano
lo stesso `_ztorythumbs_<cols>x<rows>.png`, con le fusioni per le panoramiche
nello stesso posto.

Ora sta in **`+extras/<scena>/thumbs`**, con la stessa risoluzione dell'import
della sceneggiatura: rispetta `useScenePath` dei project settings invece di
indovinare. I canvas salvati col percorso vecchio non vengono piu' trovati —
verificato con Franco che erano tutti di prova.

**Terza volta** che compare questa forma di difetto (stato o file condivisi fra
scene, dopo la contaminazione dei testi e quella cross-progetto): da trattare
come categoria da controllare ogni volta che si aggiunge qualcosa che persiste.

### Added — il pennello delle correttive di giuntura (branch, milestone 2/3)
Selezioni il giunto, lo pieghi fin dove la maglia si strozza, accendi `Sculpt` e
trascini: i vertici sotto il cerchio si spostano con ricaduta smoothstep e la
maglia si aggiorna mentre disegni. Un tratto un undo. **Approvato da Franco**
(«e' meraviglioso»).

Le correttive **multiple** vengono da se': il nome se lo prende dall'angolo
(`gomito_dx_95`), quindi lo stesso giunto scolpito a 45 ne crea un'altra che
copre 0-45, e quella a 95 riparte da 45. Gli strati si sommano invece di
contarsi due volte perche' il pennello registra il delta rispetto a **cio' che e'
a schermo**, che include gia' le correttive piu' basse.

**Il pennello sente la superficie, non lo schermo.** Col gomito piegato,
avambraccio e braccio si sovrappongono nello spazio ma sono lontani sulla maglia:
il filtro usa `buildDistances` (BFS sul grafo) e prende solo cio' che tocchi.

### Added — stacking order assegnabile, e la sua riprogettazione
Prima versione: un valore di SO per vertice di maglia. **Franco l'ha rifatta
meglio** — l'SO resta **uno solo**, animabile sui vertici dello scheletro, e
quello che si modifica e' **quanto ogni vertice influenza ogni punto**. Non un
secondo dato accanto al primo (due numeri sulla stessa cosa prima o poi si
contraddicono, e un SO chiavato scavalcherebbe in silenzio un valore congelato),
ma rendere editabile cio' che `buildSO` gia' calcola. Progetto completo nel task;
quello che c'e' ora (appartenenza per-vertice) e' un passaggio, non la meta.

Piu': Shift aggiunge alla selezione dei giunti (Animate e sculpt), e l'SO scritto
nel campo va su **tutti** i giunti selezionati in un solo undo — con la trappola
che il campo, con piu' selezionati, era **vuoto**, perche' il relay si legava solo
con `hasSingleObject()`.

### Fixed — riaccendendo l'IK il personaggio non salta piu' (branch)
Uscire dall'IK fa il bake in FK e molla i pin, ma i target di scena catturati
prima restavano indietro: rientrando, il primo solve trascinava il personaggio su
un bersaglio che il bake aveva gia' assorbito. Ora rientrando si ri-piantano i
pin **attivi** dove il personaggio sta in quel momento, con lo stesso calcolo che
usa il pin quando lo pianti.

### Notes — il bug che e' costato due diagnosi sbagliate
`buildDistances` scrive **solo** i vertici che la sua BFS visita. Con l'array
inizializzato a **zero**, le isole staccate della maglia — braccia, gambe —
restavano a distanza 0, cioe' piu' vicine di qualunque cosa: pennellando la testa
si assegnavano le gambe. Non raggiunto deve voler dire **lontano**, non vicino.

Ha prodotto due diagnosi sbagliate, **entrambe sostenute da misure vere** — prese
pero' dove le isole non c'erano. L'ha trovato Franco selezionando il vertice in
cima alla testa.

### Notes — aperto, con il discriminatore
Il personaggio che «parte» manipolando le anche succede su animazioni
**rimaneggiate** e non su **chiavi fresche**. Sposta il sospetto dal solver al
dato gia' in scena. Primo test quando si riprende: stessa scena vecchia su 0.11.0
contro branch — se il sintomo c'e' su entrambe, la ri-cattura dei pin e' innocente.

## [2026-07-27b] — ZtoRig su master, 0.11.0. E i limiti d'angolo che finalmente seguono il corpo

> Sessione lunga, quasi tutta di **misura**. Delle sette ipotesi formulate ne sono
> cadute sei, e ogni volta a ucciderle è stato un numero, non un ragionamento.
> Le trappole sono annotate perché sono la parte riutilizzabile.

### Added — ZtoRig entra in master (merge di `feature/ztorig-pose-blend`)
Nessun conflitto. Entrano pannello ZtoRig, pose di personaggio, posa Base, modi
Pose/Add/Part, stamping delle chiavi plastic, IK multi-pin con «il corpo
resiste», slider IK Max Step, motore delle correttive di giuntura (1/3).
Versione **0.11.0**: MINOR e non PATCH perché chi aggiorna deve sapere che ci
trova dentro un sistema di rigging, non una correzione.

### Fixed — i limiti d'angolo seguono la rotazione del corpo anche fra colonne
Su rig multi-colonna il bound di una spalla restava fermo mentre il busto si
piegava: un giunto appeso alla **radice** del proprio scheletro non ha un osso
padre lì, e il riferimento ripiegava sull'**asse X del mondo** — una costante.
Nel single level non succede perché l'osso padre è nello stesso scheletro.

Ora il limite viene spostato di quanto ha ruotato l'osso di attacco sulla colonna
**padre** (`parentColumnRefDirs_animate`, che era già scritto e misurato da giorni
ma non collegato). Lo scostamento serve in **quattro** posti, e se anche uno solo
resta indietro si vede a schermo:
`updateAngle`/`updatePosition`, `writeBackAnglesFor_animate` (il gemello del clamp,
quello che governa il caso cross-colonna e che era sfuggito), il ventaglio azzurro
e l'overlay a linee lunghe `1e4` — le «due righe lunghissime» disallineate.
`ZTORYC_BOUND_REF=0` torna indietro, `-1` inverte il verso.

### Fixed — il figlio eredita l'orientamento dell'osso-hook, ma solo a IK spenta
Il parenting a un hook portava la **posizione** del vertice e non l'orientamento
del suo osso: piegando il busto la colonna del braccio non ruotava. Aggiungerlo
sembrava perfetto — e **ha peggiorato molto la cinematica**: posare una gamba col
pin passava da «identico al single level» a quasi ingestibile, col giunto che
percorreva **un quarto** di quanto richiesto (`gain=0.24`).

Causa: con l'IK attiva l'arto ruota **già**, perché `solvePlasticCharacter` cuce la
radice della colonna figlia sul vertice di attacco del padre e la tratta come un
osso vero. Farlo anche nel piazzamento applica la rotazione **due volte**, e la
seconda finisce dove il write-back cross-level non può vederla: calcola gli angoli
nel piazzamento **congelato al press**. I due casi non si sovrappongono mai —
`crossLevelIK` gira solo con l'IK accesa — quindi **la condizione è l'intero fix**.

### Changed — la posa vettoriale non compare più di default
Due bottoni che sovrascrivono il disegno corrente senza undo e senza persistenza
non vanno in una release pubblica. Si riattiva con `ZTORYC_VECPOSE`.

### Notes — cosa NON era un bug (chiuso con misure)
- **Le due anche che si comportano diversamente**: pin su entrambi i talloni,
  gamba sinistra accorciata quando è stata riposizionata → meno gioco, meno
  discesa. Comportamento corretto. Cadute per strada tre ipotesi: la chiave sul
  Distance (tutti i 22 vertici ce l'hanno identica), il bound semiaperto che
  bloccherebbe a zero (il codice ripiega sul limite statico, `plastictool_animate.cpp:2034`),
  e i limiti d'angolo (v1, quella ferma, non ne ha; v5, che scende bene, ne ha di
  stretti — l'opposto di quanto predirebbe l'ipotesi).
- **Il «single pin regredito» rispetto alla 0.10.1**: non riprodotto. Controllato
  l'unica differenza reale nel percorso (il cap `ikMaxStep`, che su master non
  esiste proprio): misurato inattivo, 9 eventi su 461, rotazione media richiesta
  2.0° contro un cap di 15°. Nessun intervento.

### Notes — lo scatto in ginocchio, ricaratterizzato
Il controllo delle anche è **buono** e va conservato: la riscrittura «leva =
cursore» è annullata. Resta che «a volte basta poco e il personaggio scatta».
**Non** è la bisezione di fattibilità né i limiti: `accepted` medio 0.995, 45
eventi su 46 al valore pieno. L'amplificazione è **fra il bersaglio e la posa
risolta**: bersagli vicini danno pose lontane (90 → 16, 87 → 359). Firma di un
solver con più bacini di convergenza dentro `solveMultiAnchor`. Prossimo passo
scritto nel task.

### Lezioni di metodo
1. **«Mi pare perfetto» non è un collaudo.** La rotazione del piazzamento è stata
   approvata a caldo e un'ora dopo si è scoperto che rovinava la gamba. È emerso
   solo perché Franco ha chiesto un A/B su un caso diverso da quello guardato.
2. **Un log che cresce è un bersaglio in movimento.** Percentuali calcolate a
   3576 eventi confrontate con numeri presi a 9719: l'app era ancora aperta.
   Marcare la posizione nel log prima di ogni misura.
3. **Il valore identico può essere il bug, non la prova.** Bound uguali a busto
   eretto e piegato letti come «funziona»; erano il sintomo.
4. **Chiedere quale interruttore spegne cosa PRIMA di dedurlo.** «Esploso» per
   Franco è il single level; per me era il multi-colonna. Un test intero fatto
   sulla domanda sbagliata.

### Upstream candidates
Aggiornato `UPSTREAM_PR_CANDIDATES.md`: il parenting a hook che non eredita
l'orientamento dell'osso, insieme al fix gemello sui limiti d'angolo. Non è un
bug di Tahoma2D ma comportamento storico, quindi va proposto come **opzione**.

## [2026-07-27] — Posa vettoriale: il primo test funziona. E il crash della 0.10.1 portato su master

> Codice su branch `feature/ztorig-pose-blend` (`139f43082`), bundle Ztoryc-SP.app.

### Fixed — crash della 0.10.1 rilasciata, ora su MASTER (`dd33b5865`)
Tre crash di Franco la sera del 25 (`~/Library/Application Support/Ztoryc/Ztoryc/crash/`),
backtrace identico: `PlasticTool::storeDeformation` → `onColumnSwitched` →
`XsheetViewer::setCurrentColumn` → clic su una cella. SIGSEGV.

Il fix esisteva (`ccdd2ca89`, famiglia pegbar-zombie) ma **solo sul branch SP**:
su master `stageObject()->getPlasticSkeletonDeformation()` era ancora senza
guardia. Cioe' **la 0.10.1 in mano agli utenti ha questo crash**: Plastic tool
attivo + clic su una colonna senza stage object (camera, o xsheet scambiato sotto
il tool). Cherry-pick su master. Da solo giustifica una 0.10.2.

Deciso di NON mergiare il branch SP su master: ZtoRig e' a meta' (anche che
partono, angle bounds cross-colonna, correttive 2/3) e master resta releasabile.

### Added — posa vettoriale, primo test: FUNZIONA
Riquadro `Vec A / Vec B / slider` nel pannello ZtoRig: cattura due disegni
vettoriali come estremi e li interpola con `TInbetween` (il motore gia' dietro
Cells > Inbetween). Verificato da Franco: **«è una bocca eccome»**.

Distruttivo per scelta (sovrascrive il disegno corrente, niente undo, niente
persistenza), si rifiuta di scrivere sui frame di A e B. Diagnostica opt-in:
`ZTORYC_VECPOSE_DIAG`.

**Tre trappole trovate, tutte annotate nel codice:**
1. Il disegno si legge dalla **CELLA** quando sei nell'xsheet;
   `getCurrentFrame()->getFid()` vale solo con la level strip davanti — un
   livello vettoriale valido veniva riportato come "not a vector level".
2. `clone()` non porta la **palette**, che appartiene al LIVELLO: l'interpolazione
   usciva con indici di stile che non puntavano a niente (stroke presenti, nulla
   a schermo).
3. `invalidateFrame` / `ImageManager::invalidate` significano «ricostruisci dalla
   sorgente», e la sorgente non sa nulla della modifica in memoria: buttavano via
   il risultato, svuotavano il disegno e rompevano il salvataggio del `.pli`. La
   notifica giusta e' quella di `TTool::notifyImageChanged`: **`touchFrame`**
   (= «modificato») piu' il rinfresco icone. Un errore, tre sintomi scollegati.

### Architettura decisa (discussa con Franco, da un doc ChatGPT come spunto)
Il «oggetto bocca» che si chiavia sull'xsheet **non richiede un nuovo tipo di
livello**. Il modello esiste gia' due volte: una colonna con deformazione Plastic
espone UN disegno e cambia forma via parametri chiaviabili applicati al render.
Serve quindi una **deformazione nuova sullo stage object**, non un livello nuovo
(che significherebbe mesi di plumbing Toonz).

E il sistema di pose costruito il 26 **e' gia' un sistema di blend shape**: azioni
con curva di forza, modi Add/Pose/Part, piu' azioni che si sommano. Manca solo un
**secondo tipo di bersaglio**: delta di PUNTI per id di stroke accanto ai delta
dei parametri di posa.

Ordine dei mattoni:
1. **ID persistenti sui punti** (idea di Franco) — primo, senza quello ogni
   ritocco del disegno rompe la corrispondenza. Inserire un punto a forma
   invariata e' esatto (suddivisione di de Casteljau, stesso *t* su tutte le pose).
2. **Delta vettoriale dentro l'azione di posa** — stesso slider, stesse chiavi.
3. **Sostituzione a render-time**, come le dissolvenze dell'animatic (v0.8.0).

Nota tecnica: il vettoriale e' **PLI**, non TLV (che e' raster colormappato) — il
doc di partenza li confondeva.
## [2026-07-26d] — Multi-pin: i limiti d'angolo, il corpo che resiste, lo slider IK. E tre lezioni di metodo pagate care

> Worktree SP, branch `feature/ztorig-pose-blend`, bundle **Ztoryc-SP.app**.
> Sessione lunga e con parecchi passi falsi: sono annotati tutti, perche' sono
> la parte piu' utile.

### Fixed — perche' il multi-pin cedeva: due solver in disaccordo sui limiti
`solveMultiAnchor` (drag) **non clampa** ai limiti d'angolo; `plant()`
(valutazione) **sì**. Il drag inchiodava entrambi i pin, la valutazione
ri-risolveva sotto un vincolo che il drag non aveva mai visto, e il pin
secondario mollava. Il primario non lo mostrava mai: e' una traslazione rigida,
senza giunti da clampare.

Ora il CCD prova dentro i limiti e li fa cedere solo se il pin resterebbe
indietro. **Misurato**: bilanciamento sul 47% dei frame, residuo peggiore 2.3%
della diagonale — contro 81% e 7.0% con i limiti duri. Confermato da Franco
(«con gli angle bound larghi regge molto di piu'»).

### Refactor — un solo posto che sa piantare
`storeDeformedSkeleton` spezzato: `plantPins()` e' l'unico punto che trasforma
un pin in vincolo per il solver, e `pinResidualForPose()` espone la domanda che
serve al drag — *«se posassi cosi', i pin reggerebbero?»* — facendola decidere
allo stesso `plant()` che poi giudica davvero.

### Added — «il corpo resiste» + slider IK Max Step
Bisezione/scansione di fattibilita' nel percorso multi-anchor: il drag si
irrigidisce a fine corsa invece di trascinare via i pin. Caso peggiore da
**10.4% a ~1.5%** della diagonale nell'arco della giornata.

Nuovo slider **IK Max Step** nella toolbar del Plastic tool (Animate), 1-90
**gradi per evento del mouse**, default 15 (valore scelto da Franco provando).
Applicato in tre punti: rotazione diretta, CCD multi-anchor, `replantOtherPins`.

Diagnostiche opt-in aggiunte, sullo schema di `ZTORYC_SUSPEND_PLANT`:
`ZTORYC_PIN_DIAG` (residuo per pin + passate di bilanciamento + percorso di drag
+ guadagno) e `ZTORYC_PIN_NORESIST`.

### Fixed — il personaggio che «camminava in avanti» (regressione mia, stessa sera)
Quando il bersaglio era infattibile ripiegavo su `poseAt(from)`, che **non e'**
la posa corrente: rifa' girare il solver, ri-inchioda i pin e molla le ossa
verso il riposo. Con la baseline ricatturata a ogni gesto quella spinta si
sommava drag dopo drag. Ora se niente e' fattibile la risposta e' `curPos`:
**non si muove nulla**.

### Aperto — anche e spalle «partono», e non e' smorzabile
Diagnosi condivisa con Franco: trascinare significa oggi *«porta questo giunto
sotto il cursore»*, un obiettivo di **posizione**. Per l'anca, a due centimetri
dal bacino, seguire un cursore lontano richiede rotazioni enormi: la risoluzione
del controllo e' proporzionale alla distanza del giunto dal suo pivot, quindi i
giunti vicini al pivot sono **incontrollabili per costruzione**.

Lo smorzamento non risolve, **converte**: a 1 il personaggio scatta in ginocchio
(limite raggiunto subito), a 15 e' usabile ma nervoso. Franco: «non so se questa
cosa ha soluzione».

**Ce l'ha, ma non nel solver**: la leva dev'essere il **cursore**, non il
giunto — interpretare il drag come l'angolo che il cursore spazza attorno al
pivot (il ramo `rotateAboutPin` lo fa gia'; e' `multiAnchor`, dove passano le
anche, a usare un obiettivo posizionale). Forma completa: un **controller** con
leva vera, cioe' il master controller gia' in piano.

Nota: con tallone **e** punta pinnati su entrambi i piedi il piede e' bloccato
in posizione *e* orientamento — il sistema e' sovravincolato e «inginocchiarsi»
e' spesso l'unica soluzione geometrica disponibile. I rig veri lo gestiscono
rendendo il pin della punta vincolante solo in orientamento, o con priorita' fra
i pin.

### Aperto — angle bounds che risentono della rotazione del padre (multi-colonna)
Segnalato da Franco: un braccio non ha lo stesso range col corpo piegato avanti
o indietro. Succede **solo quando il padre sta su un'altra colonna**.

Causa: `limitDisplay_animate` (`dirFromGrand(1,0)`) e `writeBackAnglesFor_animate`
(`if (pp < 0) return 0.0;`) ripiegano sull'**asse X del mondo** quando il vertice
non ha un nonno nel proprio scheletro — che e' il caso di ogni giuntura vicina
alla radice di una colonna figlia. Il riferimento resta inchiodato allo schermo
mentre il corpo ruota.

Scritto `parentColumnRefDirs_animate()`: via `crossLevelLinks_animate()` trova
colonna e vertice di aggancio, prende l'osso del padre (riposo + deformato) e lo
porta in questa colonna con la parte lineare di `toCur`. **Misurato funzionante**:
trovato su 890/997 campioni, ruota da -4.5° a 51° piegando il corpo.

**NON collegato**: cablarlo nel clamp non ha cambiato il range effettivo, e non
lascio una modifica non verificata sul codice che decide quanto si muove ogni
giuntura. L'helper resta, documentato.

**Decisione presa da Franco**: il limite va ancorato al **padre**, come nel
single level. **Prossimo passo**: strumentare `writeBackAnglesFor_animate` PRIMA
di ritoccare il riferimento — `columnOfDeformation_animate` risolve la colonna?
il ramo del clamp viene raggiunto per quel giunto? Il sintomo si giudica sul
range, e il range si decide li'.

### Lezioni di metodo (le tre che sono costate di piu')
1. **Non dedurre quale codice gira: misurarlo.** Ho passato mezza serata a
   correggere e rimisurare `moveVertexMultiAnchor_animate`, che sul rig di Franco
   **non viene mai eseguito** — il drag passa da `crossLevelIK`. Una riga di
   `qDebug` all'inizio sarebbe costata una build. Stessa forma, tre volte in un
   giorno: il clock che «diverge sul multilevel» (dipendeva da Cycle), i pin
   dedotti dal frame di attivazione, e questo.
2. **Misurare la cosa che l'utente giudica.** Ho misurato `limitDisplay_animate`
   (il ventaglio) mentre Franco giudicava il range effettivo — che e' deciso
   altrove. Il ventaglio non lo aveva nemmeno acceso.
3. **Diffidare di ogni baseline non congelata.** Tre bug distinti oggi, stessa
   radice: l'Offset che si risommava, il personaggio che camminava in avanti, e
   il sospetto sulla root. Se una cosa «parte», cercare cosa rilegge il proprio
   risultato precedente.

Piu' una quarta, gia' in memoria: **niente percentuali di contesto inventate**
(annunciato 60-70% quando il pannello segnava 31%).

### Upstream candidates
Nessuno. I file core toccati (`plastictool*.cpp`, `plasticskeletondeformation.cpp`)
lo sono per funzionalita' SuperPlastic/ZtoRig, che upstream non ha.

## [2026-07-26c] — ZtoRig: azioni di personaggio, posa Base, modalita' Part, IK che spegne davvero i pin

> ⚠️ Tutto su **worktree SP**, branch `feature/ztorig-pose-blend`, bundle
> **`Ztoryc-SP.app`**. Master non e' toccato dal codice (solo i doc).
> Verificato da Franco a runtime tranne dove indicato.

### Fixed — gli altri slider non si azzeravano (il ⬜ n.1 della lista)
`poseStrengthAt` **deduceva** la forza da `(valore−riposo)/delta` sul parametro piu'
mosso: corretto solo se le azioni sono disgiunte, spurio appena due ne condividono uno.
Ora la forza e' **registrata** nella curva guida (`m_guide`, gia' nel dato e serializzata)
e riletta da li'.

**La trappola:** `m_guide` era ancora il campo che alimentava il *blend* a runtime, quindi
scriverci la forza avrebbe applicato la posa una seconda volta sopra le chiavi gia'
stampate. Il blend era vestigiale dal rework dello slider (nessuno scriveva piu' una guida
≠ 0, `applyPoseAction`/`poseBlendOffset` senza chiamanti): **rimosso** insieme al fix
(~90 righe, comprese le 5 iniezioni nel percorso di valutazione).

Corollario: l'azzeramento delle altre azioni segue quanto e' stato **rivendicato**
davvero — `Pose` azzera tutte quelle della sua skeleton, `Part` solo quelle con cui
collide su almeno un (vertice, parametro), `Add` nessuna.

Chiuso anche un buco aperto strada facendo: `UndoPoseActions` copiava il vettore di
azioni, ma `m_guide` e' uno **smart pointer** → prima e dopo condividevano la curva e la
forza scritta al Record sopravviveva all'undo. Ora l'undo porta anche il `PoseKeyState`.

### Added — le pose sono del PERSONAGGIO, non della colonna
Sintomo: «le registra ma spariscono appena cambio colonna». Il pannello guardava
`currentDeformation()`; su un rig esploso l'azione viveva su una colonna sola.

`ZtoRigPanel::characterParts()` fa **la stessa risalita di `PlasticTool::characterColumns()`**
(su al padre piu' alto per parentela di colonna, poi giu'). Deve essere la stessa
definizione o l'app avrebbe due idee diverse di «personaggio».
- **Record** registra su tutte le colonne, in un blocco di undo unico. Una colonna che la
  posa non muove registra zero delta — giusto: come Posa assoluta poi la porta alla base.
- **Slider / modo / Base / rimozione** si propagano alle sorelle **per nome** (gli indici
  non coincidono tra colonne).
- Ogni colonna usa il **proprio orologio**: `paramsTime` e' per stage object, quindi
  `CharPart` porta il frame calcolato sulla sua colonna.
- Restano per-colonna, di proposito: la chiave di **transform** e la **lista scheletri
  spuntati** (ogni colonna ha la sua numerazione).

### Added — posa Base (il problema del rig esploso)
Su un rig esploso il riposo vero e' il **disassemblato**, quindi lo 0 dello slider
smontava il personaggio. Ora un'azione si marca **Base** e diventa lo zero della sua
skeleton: lo stamping assoluto interpola `base + forza*(posa − base)`.
I delta restano misurati dal riposo vero (un'unica origine assoluta), la Base si applica
allo stamp → si puo' cambiare quale azione e' Base senza riscrivere nulla.
Una sola Base per skeleton; due Base coesistono se i loro insiemi non si intersecano.

### Added — terza modalita' `Part` + rinomina `Offset` → `Add`
L'asse che conta e' **richiamo vs spinta**, incrociato con quanto scheletro si rivendica:
- **Add** — spinge da dove sei, sui suoi parametri. Il risultato dipende dalla posa di
  partenza: una "A" dopo una "O" da' "O+A", mai la "A" registrata.
- **Pose** — richiama esatto su **tutto** lo scheletro (→ esclusiva).
- **Part** — richiama esatto sui **suoi** parametri. Per fonemi e pose per-arto: la bocca
  cade identica dopo qualsiasi altra forma e non tocca gli occhi.

Pulsante a 3 stati che cicla al clic. Enum rinominato `PoseAction::ADD` per non portarsi
due nomi. Serializzazione: nuovo tag `Mode` scritto solo se ≠ Add; il vecchio `Absolute`
si continua a **leggere** e mappa su `Pose`.

### Added — pose vincolate agli scheletri (insiemi)
`m_skelIds` (`std::set<int>`), **vuoto = nessuna restrizione**. Motivo: i nomi di vertice
sono condivisi tra le viste, ma `DISTANCE` e' una lunghezza nello spazio di *quello*
scheletro, non un rapporto — la posa "si trasferisce" ma non ha senso.
- Il **Record apre un dialogo**: nome + caselle degli scheletri (corrente gia' spuntato).
- Menu a tendina sulla riga per modificarlo dopo, «All skeletons» in cima.
- Vista **filtrata sullo scheletro corrente** di default; spunta «Show all skeletons» per
  la vista completa raggruppata, dove **la stessa azione si ripete sotto ogni scheletro**
  su cui e' attiva (le copie si sincronizzano da sole: le righe sono per indice azione).
- Il rifiuto sta nel **modello**: `applyPoseStrength` esce se l'azione non appartiene
  allo scheletro attivo. Il pannello disabilita la riga, ma non e' lui l'autorita'.
- Il cambio frame ricostruisce la lista **solo se cambia lo scheletro attivo**, non a ogni
  frame (altrimenti resetta lo scroll in play e combatte il drag).

### Fixed — clock unico nel pannello
Il pannello parlava con la deformazione su **due tempi**: scritture a
`paramsTime(frame)`, letture (Record, `poseStrengthAt`, skeleton attivo) al frame xsheet
grezzo. Ora tutto passa da `ZtoRigPanel::paramsFrame()`; il frame xsheet resta solo per la
chiave di transform.

> ⚠️ **Correzione a caldo, annotata perche' e' una lezione**: avevo dichiarato che i due
> tempi divergono sul multilevel. **Falso.** `TStageObject::paramsTime`
> (`tstageobject.cpp:569`) rimappa solo con **Cycle attivo** e ≥2 keyframe, altrimenti
> ritorna `t`. Avevo dedotto il collegamento invece di leggerlo. Il fix e' giusto a
> prescindere (leggere e scrivere sullo stesso orologio), ma non c'entrava col multilevel.

### Fixed — spegnere l'IK ora spegne davvero i pin
`pinsEnabled` **non era guardato da nessuna parte nella valutazione**: `grep` non lo
trovava affatto in `tstageobject.cpp`, e nel planting per-colonna c'era una nota che
diceva esplicitamente di non filtrarlo. Il flag addormentava solo la UI del tool.
- Guardia nel **planting per-colonna** e nel **solve unificato di personaggio** — questa
  **dopo** la passata di pulizia, altrimenti restano gli scheletri risolti sotto il vecchio
  stato dei pin, che e' lo stato in cui il rig sembrava rotto.
- `enablePins()` ora butta via `clearSolvedSkeleton()` + `clearSecondaryPinTargets()`: e'
  questo che fa valere il toggle **subito** (era la cache sopravvissuta a costringere al
  clic per «farlo riassestare»).
- La posa non si sposta perche' **uscire dalla modalita' IK fa gia' il bake** in FK +
  controller (`plastictool.cpp`, ramo `inverseKinematics`).

### Fixed — diamanti ciano delle colonne connesse
Disegnavano i pin **anche con IK spento**: erano l'unico percorso che leggeva i parametri
`PIN` direttamente invece di passare da `pinnedVerticesAtFrame`. Verificato da Franco.

### Fixed — angle bounds: il gizmo non creava mai la prima chiave
`plastictool_animate.cpp` — *se gia' chiaviato metti una chiave, altrimenti scrivi il
limite statico sul vertice*. Ma niente creava mai la prima chiave: il ramo animato era
**irraggiungibile** e nel function editor non compariva nulla.
Ora **chiavia sempre** (gizmo **e** campo Angle Bounds della toolbar; svuotare il campo
cancella le chiavi invece di chiaviare un infinito), tenendo allineato il valore statico.
Effetto collaterale voluto: i bound ora **seguono i livelli**, perche' `MINANGLE/MAXANGLE`
stanno nei parametri condivisi per nome invece che sul vertice del singolo scheletro —
era questo il «come se non li mantenesse».

### Tentato e RITIRATO — legare i pin allo scheletro
Avevo dedotto «su quale scheletro e' stato messo il pin» dallo **scheletro attivo al frame
di attivazione**. Sbagliato su due fronti, e i due fallimenti hanno la stessa radice:
- **rompeva il multi-pin** — su un multilevel lo scheletro cambia a ogni cambio di
  disegno, quindi ogni pin tenuto attraverso un cambio livello veniva rilasciato;
- **non spegneva il ciano** — cambiando disegno *allo stesso frame* il frame di
  attivazione non cambia, quindi il confronto dava «stesso scheletro».

Ho inferito un dato che **non esiste** invece di ammettere che va memorizzato. Revert dei
tre punti (planting, solve di personaggio, `pinnedVerticesAtFrame`).
**Da fare davvero:** un campo esplicito in `SkVD`, stessa logica di `m_skelIds` delle pose,
con retrocompatibilita' «pin vecchi = valgono ovunque». Zona delicata (ha gia' avuto il
bug di oscillazione multi-pin, `7dac71339`): da affrontare da fresco.

### Aperto a fine sessione
- **Multi-pin «non regge granche'»** (Franco, dopo il revert). Con IK acceso il percorso e'
  identico a stamattina, quindi **sulla carta** e' preesistente — ma va verificato con un
  A/B vero: compilare `422461463` e riprovare lo stesso rig. Non dedurlo.
- Pin legati allo scheletro (sopra).

### Upstream candidates
**Nessuno.** I file core toccati (`tstageobject.cpp`, `plastictool*.cpp`,
`plasticskeletondeformation.cpp`) lo sono solo per funzionalita' **SuperPlastic/ZtoRig**,
che upstream non ha: pin, `MINANGLE/MAXANGLE`, pose actions sono aggiunte nostre.

### Note di metodo (in memoria)
Due volte in questa sessione ho annunciato percentuali di contesto inventate (60-65%, poi
70%) mentre il pannello di Franco segnava **31%**. Non ho una lettura diretta del consumo:
**non annunciare piu' percentuali**. Aggiornata `feedback_context_window_1m`.

## [2026-07-26b] — ZtoRig: Offset risolto, due difetti aperti (LAVORO IN CORSO)

> ⚠️ **ZtoRig vive nel worktree `tahoma2d-superplastic`, branch
> `feature/ztorig-pose-blend`, bundle `Ztoryc-SP.app`** — NON `Ztoryc.app`, che è
> master e non contiene nulla di ZtoRig. Se gli slider "fanno cose strane",
> **primo controllo: quale bundle stai aprendo.**

### Fixed
- **L'Offset non si accumula più su se stesso** (`422461463`, verificato da Franco).
  `applyPoseStrength` faceva `valore corrente + forza*delta`, ma lo slider la chiama
  a OGNI movimento: ogni passo rileggeva il proprio risultato e ci risommava il
  delta (0.3 in tre passi = base + 0.6*delta) → il personaggio partiva per la
  tangente. Ora la base è congelata a inizio gesto (`beginPoseDrag`/`endPoseDrag`),
  stessa idea della baseline press-time dei pin.

### Da fare — diagnosticato, non ancora corretto
- **Gli altri slider non si azzerano** quando una Posa esclusiva prende il
  controllo. `poseStrengthAt` non memorizza la forza: la *deduce* dal parametro più
  mosso con `(valore−riposo)/delta`, corretto solo se le azioni sono disgiunte.
  Fix proposto: memorizzare la forza nel campo *guida* per azione (già nel dato,
  inutilizzato dopo il rework) e azzerare le guide delle altre azioni.
- **Il personaggio scivola registrando pose con i pin** — autorità del planting.
- Correttive di giuntura: milestone 2 (authoring) e 3 (UI).

### Nota di metodo
Il sospetto iniziale "ho perso una versione" era infondato: il commit `98e99b3bc`
è delle 16:18, lo stesso minuto del binario SP, e **si compila dal working tree**,
quindi quella build lo conteneva già. Prima di incolpare il bundle, confrontare
l'orario dei commit col timestamp del binario.

## [2026-07-26] — Task 63: import da carta (stampa + import file + cattura webcam)

> **Riepilogo**: feature completa sul branch `feature/paper-import`. Nuovi moduli
> `ztorypapersheet` (stampa PDF + pipeline OpenCV di raddrizzamento/ritaglio) e
> `ztorypapercapture` (dialog di cattura da webcam). Ciclo chiuso: stampi la
> griglia → disegni a matita → fotografi o scansioni → i pannelli rientrano nella
> Thumbnail room raddrizzati e ritagliati. Verificato da Franco sul campo.

### Added — Fase 1: stampa del foglio
- `Print` nella toolbar della Thumbnail room, PDF A4 via `QPdfWriter` (stesso
  schema del PDF del Board). Menu a due voci: **foglio vuoto da disegnare**
  (fotocopiabile) e **foglio con i thumbs correnti** (contact sheet).
- Griglia **continua** come il canvas della room (celle adiacenti, nessun gap) →
  si possono disegnare panoramiche a cavallo di più celle. Nessuna etichetta R/C
  dentro la griglia: la posizione la porta il codice macchina.
- **4 marker di registro** a quadrati concentrici (finder pattern), rilevabili con
  il solo `imgproc` — niente `cv::aruco`, che la CI non compila.
- **Codice-pagina** auto-descrittivo (9 byte + checksum XOR): versione, hash
  scena, colonne, righe, aspect, pagina, riga iniziale.
- Cornici in ciano chiaro, footer con logo + link repo, caselle `Page ☐/☐` da
  compilare a mano.
- Il **foglio vuoto è sempre UNA pagina** e sempre a piena capienza (4×4 con 4
  colonne), svincolato dalle righe della room: è un template da fotocopiare.

### Added — Fase 2: import da file
- `importSheet()`: marker → `warpPerspective` → lettura codice → normalizzazione
  carta → ritaglio per geometria → blit nel canvas come **un solo undo**.
- Selezione **multi-file**; i fogli si accodano **nell'ordine di scansione**.
- `ZtoryThumbnailCanvas::applyImportedCells()` (duale di `panelRaster`), con
  crescita automatica delle righe per la pagina successiva.

### Added — Fase 2b: cattura da webcam
- `ZtoryPaperCaptureDialog`: selettore camera, preview live e **feedback di
  inquadratura** (contorno verde quando i 4 marker sono visibili, scatto abilitato
  solo allora). Scatti multipli, importati nell'ordine di scatto.
- Riusa `Webcam` della Stop Motion room; stessa pipeline dell'import da file
  (helper `importOneSheet` condiviso).

### Fixed (durante lo sviluppo, tutti trovati provando sul campo)
- **Orientamento**: non si deduce più da un marker "speciale" (due tentativi
  falliti: l'anello extra sparisce in scansione, il marker più grande veniva
  mis-classificato su scan reale → foglio importato capovolto). Ora si provano
  **tutte e 4 le rotazioni** e vince quella il cui **checksum del codice valida**.
- **Titolo sopra il codice-pagina**: l'inchiostro del titolo alterava i primi bit
  → checksum KO → import impossibile anche con foglio appena stampato. Header
  ora impaginato in millimetri espliciti.
- **Canale colore sbagliato**: usavo il ROSSO per far sparire il ciano, ma su
  stampa reale una linea ciano legge 150-224 in rosso (scura) e 235-249 in blu
  (bianca). Passato al canale **blu**. Nota emersa provando: a togliere le righe
  e' in realta' il **ritaglio interno** della cella, non il canale — infatti
  funziona anche stampando in **bianco e nero** (li' il ciano diventa un grigio
  201 che nessun canale schiarisce). Il canale blu resta utile sulle stampe a
  colori, per le righe che il ritaglio non prende.
- **Capienza di stampa**: la griglia non viene più rimpicciolita per farla stare
  in una pagina (dopo un import la room a 8 righe stampava un 4×8 illeggibile).
  Celle sempre grandi quanto la pagina consente → 4 colonne = 4 righe/foglio.
- **Uscite silenziose nell'import da webcam** ("clicco Import e non succede
  niente"): pulsante ora disabilitato finché non c'è uno scatto e con contatore;
  e `revealRow()` porta la vista sulle righe importate, che atterrano sotto il
  disegno esistente e restavano fuori schermo.
- **Celle troppo chiare**: venivano scartate in silenzio. Ora vengono contate
  (misurando *prima* del white-point, che è ciò che le cancella) e segnalate.
- Rilevamento marker: filtro per area + un marker per angolo immagine (i
  quadratini del codice formavano falsi marker), risoluzione di lavoro della
  preview alzata a 1280 px (a 640 si perdeva un marker su 4; costo 4 ms/frame).

### Fixed — ZtoRig (branch `feature/ztorig-pose-blend`, bundle Ztoryc-SP.app)
- **L'Offset non si accumula piu' su se stesso** (`422461463`, verificato da
  Franco). `applyPoseStrength` calcolava l'offset come `valore corrente +
  forza*delta`, ma lo slider la chiama a OGNI movimento: ogni passo rileggeva il
  proprio risultato precedente e ci risommava il delta (0.3 in tre passi =
  base + 0.6*delta), e il personaggio partiva per la tangente. Ora la base e'
  congelata a inizio gesto (`beginPoseDrag`/`endPoseDrag`), stessa idea della
  baseline press-time gia' usata per i pin.
- Restano due difetti diagnosticati ma non corretti, dal collaudo del rework
  dello slider (l'unica parte mai verificata a mano): gli **altri slider non si
  azzerano** (poseStrengthAt *deduce* la forza dal parametro piu' mosso invece di
  memorizzarla → valore spurio quando due azioni condividono un parametro) e il
  personaggio **scivola registrando pose con i pin** (autorita' del planting).
  Dettagli e fix proposti in memoria.

### Fixed — Board, dopo "Send to Board" dalla Thumbnail room
- **Pannelli e anteprime non comparivano** finche' non si entrava nello shot:
  `addShotFromRasters` costruisce gia' un `PanelData` per ogni raster (start
  frame + hold), ma `StoryboardPanel::onShotInserted` li scartava e ne fabbricava
  UNO solo dalla colonna xsheet. Ora prende la lista dal modello quando c'e',
  crea un widget per pannello e renderizza le anteprime subito (deferred a
  evento zero, cosi' l'inserimento resta immediato).
- **L'ultimo shot aveva dimensione diversa** finche' non si forzava un resize:
  la passata delle larghezze saltava i pannelli con `!pw->isVisible()`, che e'
  falso per TUTTI i pannelli mentre il Board sta in un'altra room — ed e'
  esattamente il caso del Send to Board, fatto dalla Thumbnail room. Ora il test
  e' `pw->isHidden()` (solo i pannelli nascosti apposta dalla vista compatta) e
  le tre copie del calcolo sono unificate in `applyPanelWidths()`, chiamata
  anche da `showEvent`.

### Notes
- **Colonne**: si resta a 4×4. Per lavorare più in grande si stampa lo stesso PDF
  su **A3** (stesso rapporto √2 dell'A4, l'import lavora in proporzione → nessun
  codice da toccare; usare "adatta al foglio"). Analisi per un eventuale
  selettore di colonne in memoria: la parte delicata è solo il reflow dei disegni
  esistenti, il resto (persistenza, undo, stampa, import) è già cols-agnostico.
- **Non fatto** (Fase 4 della scheda): dialog di anteprima con spunte per-pannello
  e slider di soglia. Valutato e rimandato: duplica quello che la room già fa, e
  la rete di sicurezza è l'undo singolo.
- Nessun candidato PR upstream: tutti i file toccati sono Ztoryc.

## [2026-07-25] — ZtoRig: pose assolute/offset, correttive di giuntura (motore), stamping su xsheet

> **Riepilogo**: sessione lunga tutta su ZtoRig (branch `feature/ztorig-pose-blend`,
> worktree SP, bundle **Ztoryc-SP.app** — NON master). 10 commit oggi (+ i 6 di ieri).
> Il modello delle pose e' stato ribaltato due volte seguendo il feedback di Franco,
> fino a quello giusto: **lo slider e' l'animazione, scritta sulle chiavi vere
> dell'xsheet**. Tutto verificato a mano da Franco tranne l'ultimo rework grosso.

### Pose: da blend nascosto a stamping sull'xsheet
- **Assoluta vs additiva, per azione** (toggle Pose/Offset): Posa = richiamo esatto da
  rest; Offset = layer additivo che si somma (controller di arti/pupille/bocche).
- **Stamping**: applicare una posa scrive **chiavi plastic vere** nell'xsheet (non piu'
  un dial nascosto). Mancavano `paramsTime()` + `updateKeyframes()` perche' il diamante
  comparisse.
- **Esclusivita' completa**: una Posa ripristina TUTTO lo scheletro (i param che non
  muove scrivono il riposo), cosi' alternando due pose ognuna riporta il personaggio
  esattamente nella sua — verificato da Franco (`98e99b3bc`).
- **Slider auto-keying (rework finale)**: muovere lo slider scrive le chiavi in diretta,
  `0 = rest`, `1 = posa`, e al cambio frame LEGGE la forza dalle chiavi (`poseStrengthAt`)
  → dialabile in entrambe le direzioni (entra/esce dalla posa). Undo per gesto
  (begin/commit). La guida `m_guide` resta nel dato ma inutilizzata (ripulibile).
- **Rispetta il Global Key scope**: con Stage/All la posa chiavia anche il **transform**.
- Fix lungo la strada: due Pose insieme non ruotano piu' il personaggio (normalizzazione);
  Set Global Rest Key azzera anche i dial (con undo); il DISEGNO segue lo slider (la
  guida non era osservata dalla deformazione → deformer non invalidato).

### Correttive di giuntura (mesh PSD / SmartSkin) — MOTORE (milestone 1/3, ieri)
`MeshCorrective`: delta per-vertice-mesh guidati dall'angolo del giunto, iniettati DOPO
il solve ARAP in `plasticdeformerstorage`. Risolve cio' che rigid/flex non puo' (la
forma dell'arto in piega). Mancano authoring (scultura sul posato) e UI.

### Multi-level: gia' corretto per costruzione
Le pose sono per NOME di vertice, condivise tra tutti gli scheletri della colonna → una
posa vale su ogni scheletro del turnaround (la disciplina di corrispondenza di Franco).

### Fixed (core condiviso, famiglia pegbar-zombie)
`PlasticTool::storeDeformation`/`touchDeformation` deferenziavano `stageObject()` senza
guardia → crash cliccando una colonna senza stage object (camera/vuota/transitorio).
Guardati. Restano scoperti altri `stageObject()->` (undo set-deformation, paramsTime in
animate) — da fare se emergono.

### Roadmap emersa (in memoria) — library + mocap = stesso motore
Pose singole → clip/library (camminate/corse cross-personaggio via **template a id
fissi**; ANGLE rest-relativo si trasferisce tra corpi di taglia diversa) → import mocap
AI (MediaPipe → clip). Canali di taglia (DISTANCE/ROOT/TRANS) da escludere/scalare.

### Da fare alla ripresa
- Verificare il rework finale dello slider (auto-keying, 0=rest, scope, undo).
- **Disallineamento residuo**: se emerge ancora "svariati clic per assestarsi" nel posing
  plastic normale (non ZtoRig), serve diagnosi dal vivo (sample/lldb).
- Correttive milestone 2 (authoring) + 3 (UI), poi il morph vettoriale, poi library/mocap.

### Note
Anche i DMG macOS 0.10.1 sono stati ripubblicati e verificati (Silicon+Intel) — vedi
2026-07-24. Tutto il lavoro ZtoRig e' su branch: master resta releasabile.

## [2026-07-24] — DMG macOS ripubblicati (packaging rotto da 0.10.0) + trappola pennello vettoriale + ZtoRig pose-blend avviato

> **Riepilogo**: sessione lunga su tre fronti. (1) I binari macOS di 0.10.0 e
> 0.10.1 non partivano affatto — libreria mancante nel bundle — ora ricostruiti e
> verificati. (2) Un "bug" grave del pennello vettoriale che era in realtà
> un'opzione. (3) Avviato per davvero ZtoRig pose-blend sul branch SP: motore
> dial + pannello + correttive di giuntura (motore).

### Fixed — i DMG macOS non partivano (packaging, `2a594e63e` + `b02721f22` + `86b2de167`, su master)
I binari di **0.10.0 e 0.10.1** morivano in dyld all'avvio, su **entrambe** le
architetture:

```
Library not loaded: @executable_path/../Frameworks/libprotobuf.34.1.0.dylib
Referenced from: libopencv_dnn.411.dylib
```

Causa: **brew**, non il nostro codice. La formula `protobuf` semplice è ormai
alla 35.x; quella che entra come dipendenza di OpenCV è **versionata**
(`protobuf@33`), e le formule versionate sono **keg-only** → non linkate in
`$BREW_PREFIX/lib`. Il difetto vero: in `tahoma-buildpkg.sh` il `cp` di una
dipendenza non trovata non era controllato, ma l'`install_name_tool -change`
girava lo stesso → bundle che punta a un file mai copiato, **build verde, app
morta**. **Upstream Tahoma2D la guardia ce l'ha; la nostra copia dello script
l'aveva persa** — regressione nostra, non candidato upstream.

Fix in tre livelli: (1) guardia ripristinata (cerca in `$BREW_PREFIX/opt`, /usr,
/opt; se non trova ferma la build); (2) verifica dei riferimenti pendenti
`@executable_path`/`@loader_path` nel bundle; (3) `-DBUILD_opencv_dnn=OFF
-DWITH_PROTOBUF=OFF` — `dnn` non è usato da nessuna parte ed è l'unico che tira
dentro protobuf. DMG rifatti via CI, **Silicon e Intel verificati da Franco**.

> ⚠️ **Correzione alla nota del 2026-07-22**: il packaging era rotto **già dalla
> 0.10.0** (21 luglio), non solo dalla 0.10.1. La nota precedente attribuiva il
> problema alla sola 0.10.1.

**Trappola del verificatore** (due giri di CI persi): `@executable_path` è la
cartella dell'eseguibile che CARICA, non della libreria; gli helper in
`Resources/ffmpeg/libs/` risolvono rispetto alla cartella padre. Lezione: provare
la logica di packaging in locale su un bundle sintetico prima di rilanciare la CI.

### Not-a-bug — pennello vettoriale che disegna rosso e perde i tratti
Segnalato come bug grave (tratto rosso al rilascio, il precedente sparisce, i
raster funzionano, persiste ai riavvii). **Non è un bug**: è l'opzione **Range:
Linear** della barra del pennello vettoriale (`VectorBrushFrameRange "1"` in
`env.ini`). In modalità Frame Range la stroke resta in attesa (anteprima
**rossa** apposta), e un secondo tratto sullo stesso frame rimpiazza il primo.
Serve a fare gli intermedi tra due frame. Soluzione: **Range → Off**. Nessuna
modifica al codice. Lezioni di metodo salvate in memoria (diagnostica PRIMA delle
uscite anticipate; misurare la decisione, non l'effetto).

### Added — ZtoRig pose-blend (branch `feature/ztorig-pose-blend`, worktree SP, bundle Ztoryc-SP.app)
Avviato per davvero il motore di pose-blend (Task 59 + correttive). **NON su
master**: tutto sul branch. Nove commit:
- **Motore dial**: `POSE_PARAMS` unificata (una sola definizione, via il
  duplicato in `tstageobject.cpp`); `PoseAction` (nome + guida `TDoubleParam` +
  delta per nome-vertice) con serializzazione retrocompatibile nei due versi;
  blend iniettato in `updateBranchPositions` + controller squash&stretch (mai nel
  solver, che i parametri li scrive); `recordPoseAction` (delta dai param BASE).
- **Pannello ZtoRig** nel menu Windows: riga per azione (nome, dial, Rest/Full,
  rimozione) + Record. **Cablaggio in 5 file** — factory + comando non bastano,
  serve `createMenuWindowsAction` in `mainwindow.cpp` o la voce viene saltata.
- **Fix del dial che riponeva tutta la timeline**: `setDefaultValue` rendeva la
  guida costante → il blend cambiava anche i frame già animati (chiavi ferme, ma
  disegno diverso). Ora auto-key al frame corrente + **azione OFF prima della sua
  prima chiave** (niente bleed all'indietro). Più flush dei placement collegati
  (il disegno seguiva lo scheletro solo al click successivo).
- **Undo** su Record/Remove (snapshot del vettore azioni).
- **Correttive di giuntura — motore (milestone 1/3)**: `MeshCorrective`
  (pose-space deformation / SmartSkin) risolve ciò che rigid/flex non può, la
  forma dell'arto in piega. Delta per-vertice-mesh guidati dall'angolo del giunto,
  iniettati **dopo** il solve ARAP in `plasticdeformerstorage`. Diagnosi: la piega
  è decisa da un solo handle puntiforme + rigidity, l'ARAP non ha termine di
  volume → strozza. Mancano authoring (scultura sul posato) e UI.

### Da risolvere (annotato in memoria, non toccato)
- **Global Key su All non chiavia il transform**: il drag del Plastic tool ignora
  `GlobalKeyScope`, scrive solo le chiavi plastic. Il diamante lo mostra
  correttamente. Zona write-back delicata (pin/undo cross-colonna) → sessione
  dedicata. Feature Ztoryc, non candidato upstream.

### Note di metodo (in memoria)
Nuovo pannello Ztory = 5 punti di cablaggio. I file di stuff dopo la build vanno
ri-bundlati (`SystemVar.ini` punta allo stuff sorgente del worktree). La
diagnostica va PRIMA delle uscite anticipate.

## [2026-07-22] — Il qDebug che rallentava Windows + una cache di anteprime sola per tutti i Board (v0.10.1)

> **Nota**: entry scritta a posteriori il 2026-07-24 — la sessione del 22 si era chiusa
> senza passare dal CHANGELOG. Ricostruita dai commit `f023e6050`, `f6e47899e`,
> `1d946b496`, `c6aecd4e1`.

### Fixed — la lentezza su Windows veniva da una riga di log (`f023e6050`)
`hasScreensWithDifferentDevPixRatio()` in `toonzqt/gutil.cpp` stampava un `qDebug` per
**ogni monitor a ogni chiamata**, e la funzione è interrogata da `getDevicePixelRatio()`
sui percorsi di **disegno** — quindi in continuazione. Su una macchina a due schermi il
log di un tester erano centinaia di righe `Screen: DPR` in pochi secondi.

Non era solo rumore: `qDebug` formatta la stringa e la scrive, e su Windows la scrittura
su console/`OutputDebugString` è **sincrona e serializzata**, moltiplicata per il numero
di schermi. L'indizio decisivo è stato un tester che ha visto una differenza enorme
**scollegando un monitor**. Era una riga lasciata indietro dal lavoro sul DPR di macOS.
Gli altri `qDebug` del file restano: stanno tutti su percorsi d'errore.

### Release v0.10.1 (bump in `f6e47899e`, tag su `c6aecd4e1`, pubblicata il 2026-07-22)
Bump di `ZtorycVersion.cmake` e rigenerati `BundleInfo.plist` / `tversion.h`. Il tag
`v0.10.1` è stato creato dal workflow sul **merge della cache condivisa** (`c6aecd4e1`),
quindi la release contiene **sia** il fix del qDebug **sia** la cache di anteprime
condivisa descritta qui sotto.

⚠️ **I binari 0.10.1 pubblicati il 22 sono però inavviabili su entrambe le architetture
Mac** (libreria mancante nel bundle) — vedi l'entry del 2026-07-24.

### Performance — una cache di anteprime sola per tutti i Board (`1d946b496`, merge `c6aecd4e1`)
Ogni pannello Board teneva la **propria** cache privata: con un Board per room la STESSA
anteprima veniva renderizzata una volta per pannello. E renderizzare un frame di
sotto-scena è di gran lunga la cosa più cara del Board — molto più della scansione delle
colonne. Su una scena pesante era il lavoro moltiplicato per il numero di room.

Ora la cache è unica e vive in **`ZtoryModel`** (dove sta lo stato condiviso, come da
regola in AGENTS.md): il primo che chiede un'anteprima paga, gli altri leggono. Due
scelte tengono insieme la cosa:
- **In cache va il render NUDO, non l'anteprima finita.** L'overlay della camera dipende
  da dati che non stanno nella chiave (ordinale della lettera A→B, etichette del tipo di
  movimento) ed è economico: si riapplica a ogni uso, su una **copia**, così la pixmap
  condivisa non viene mai dipinta da chi la legge.
- **La chiave è il NOME della sotto-scena** (più frame, dimensioni fisiche e regione di
  camera). Il nome sopravvive ai riordini di colonna, a differenza del puntatore o
  dell'indice — stessa scelta già fatta per l'animatic e per la thumb cache.

Invalidazione **mirata** per sotto-scena quando uno shot è stato modificato (prefisso
`"nome|"`, col separatore perché `sh01` non cancelli `sh010`) e **totale** al cambio
scena. Senza la parte mirata l'anteprima sarebbe rimasta indietro rispetto al disegno:
il vero rischio di una cache condivisa non è la memoria, è la staleness.

File toccati: `storyboardpanel.cpp`, `ztorymodel.h/.cpp`, `ztoryanimatic.cpp`,
`ztorylightgizmo.h`.

Verificato su macOS: anteprime aggiornate dopo aver disegnato, in tutte le room; cambio
room più rapido. **Il guadagno vero è su Windows, dove il render costa di più — non
ancora misurato.**

## [2026-07-21] — Grammatica diamante nel viewer + la famiglia di bug "pegbar zombie" (v0.10.0)

> **Riepilogo**: partiti dal KeyframeNavigator del viewer (l'ultimo pezzo rimandato ieri),
> la sessione ha scoperchiato una famiglia intera di bug legati a un `TStageObjectId`
> corrotto — tre crash diversi, la corruzione silenziosa dei file di scena e le scene
> reali gia' infette. Tutti chiusi. Piu' due fix di rigging riportati da Franco.

### KeyframeNavigator del viewer — grammatica del diamante (chiude il blocco di ieri)
- **Sorgente unica**: nuovo header condiviso `include/toonzqt/ztorykeydiamond.h` con la
  grammatica dei colori (`keyDiamond`) **e** il rilevamento dello stato
  (`plasticPoseState`). Xsheet e navigator dipingono dalla stessa funzione: due superfici
  che descrivono lo stesso frame non possono piu' divergere. Il rilevamento sta li' apposta
  — il bug storico "ogni chiave di posa sembra parziale" nasceva proprio da quello.
  `xshcellviewer.cpp` perde ~45 righe, `drawTriPartPredefinedPath` delega a `fillKeyRegions`.
- `ztorytheme.h` spostato in `include/toonzqt/` (serviva a due librerie) + `keyBackground()`.
- **Icone dipinte a codice**: i .qss di tutti i temi forzano `image: transparent` sui
  bottoni chiave, quindi le icone su file non si sono **mai** viste (parziale e completa
  erano lo stesso quadrato arancione, e "nessuna chiave" era un bottone invisibile).
  Neutralizzate con foglio di stile inline sul widget; fondo magenta `#B01E9A` al posto
  dell'arancio, che divorava l'oro della posa. Sei stati leggibili invece di tre.
- **Ciclo del click con lo scope come tetto**: qualunque stato incompleto → il click
  avvicina al completo-nello-scope; solo da li' rimuove. Prima "transform pieno, posa
  assente" cancellava tutto invece di aggiungere la posa. Con scope Stage la colonna
  riggata si comporta come una liscia. Hint in hover che nominano l'asse mancante.

### La famiglia "pegbar zombie" — un id corrotto, tre crash e i file infetti
Radice comune: `PlasticTool::stageObject()` chiamava `getStageObject(ColumnId(column()))`
senza guardia, ma `column()` vale **-1** con la colonna camera corrente (e nei cambi di
xsheet). In release l'`assert` di `ColumnId` non c'e', `-1` sborda dai bit dell'indice a
quelli del **tipo** → id invalido; e `getStageObject(..., create=true)` **crea** l'oggetto
richiesto → zombie nella pegbar table, che `saveData` **scrive nella scena** come
`<pegbar id="BadPegbar">`. Al load l'id non riparsabile torna NoneId e ne genera un altro:
il file si re-infettava da solo.
- **CRASH 1** — `PlasticTool::onFrameSwitched` (SIGSEGV) entrando in uno shot col Plastic
  tool attivo: `sdFrame()` deferenziava a nudo. Guardie null in
  `stageObject()`/`sdFrame()`/`skeletonId()`/`invalidateXsheet()`.
- **CRASH 2** — assert `checkIntegrity` all'apertura della scena (debug), zombie in tabella.
  `TStageObjectTree::getStageObject` ora **rifiuta di creare** oggetti con id di tipo
  invalido: protegge ogni chiamante presente e futuro. `loadData`/`saveData` saltano i
  pegbar non riparsabili → **guariscono** le scene infette al salvataggio.
- **CRASH 3** — `assert(a <= b)` in `CellArea::getEaseHandles` su segmenti di 3 righe: i
  rami asimmetrici degenerano e l'intervallo si inverte. Ora clampa invece di asserire.
- **Bonifica dati**: trovate e ripulite 3 scene reali infette (`sh040`, `sh050` del progetto
  MaggiolataZombie; backup `.prezombiefix.bak` accanto). Nessuna scena infetta rimasta.

### Rigging — due difetti riportati da Franco
- **Scheletro disallineato dal disegno** dopo aver mosso vertici o toccato le chiavi, che
  "si riparava" a un click qualsiasi: `PlasticTool::onChange` ora flusha i placement delle
  colonne collegate (le cache per-frame non sanno di dipendere dai parametri plastic).
  I percorsi di drag lo facevano gia'; mancava tutto il resto.
- **Global Key ignorato dal drag cross-colonna**: il personaggio cucito metteva le chiavi solo sugli
  ANGLE della catena trascinata → posa parziale, e il diamante mostrava il doppio-parziale
  (letto come "ci sono chiavi anche sul transform", che infatti non era vero). Ora la posa si
  completa su **tutte** le colonne collegate, ciascuna al suo param-time, prima dello
  snapshot undo.
- **Limiti angolari (MINANGLE/MAXANGLE) continuity-first**: il valore memorizzato puo'
  stare legittimamente fuori dai limiti (il bake di unpin scrive la posa non clampata; un
  writer con wrapping salva 185 come -175). Il clamp secco strattonava il giunto al limite
  numericamente vicino al primo click — o lo faceva saltare dall'altro lato dell'arco oltre
  i 180 gradi. Ora il range e' `[min(lo,corrente), max(hi,corrente)]`: da fuori si puo' solo
  rientrare, mai essere teletrasportati.

### Risolti (confermati da Franco, tolti dai task attivi)
- Scrub audio del viewer/xsheet normale: tornato reattivo sul singolo frame (task 47).
- Frame handle condiviso animatic ↔ viewer: risolto (era in Known Bugs di AGENTS.md).

### Candidati PR upstream (in `UPSTREAM_PR_CANDIDATES.md`)
Tre nuovi, tutti in file core condivisi: il crash+corruzione di `stageObject()` con la
colonna camera (alto impatto: corrompe i file), l'assert di `getEaseHandles`, e la
segnalazione dei .qss che rendono indistinguibili gli stati del KeyframeNavigator.

### Note di metodo
Il crash log della release non bastava (simbolicazione approssimata): la diagnosi e' venuta
dalla build debug sotto lldb con `MallocScribble`, piu' un `DYLD_INSERT_LIBRARIES` che
interpone `__assert_rtn` per **loggare** gli assert invece di abortire — cosi' la scena
arriva a caricarsi e si vede il vero difetto a valle.

### Aggiunta di fine sessione (2026-07-21)

**Release v0.10.0 pubblicata** — build macOS (Intel + Silicon) e Windows (installer +
portable) verdi, note bilingui EN/IT con la legenda del diamante embeddata e il link
Discord. Le note coprono l'intero range v0.9.0..HEAD (47 commit), non solo la sessione:
titolo sui diamanti + Global Key scope (la novita' piu' rilevante secondo Franco),
rendering GPU su Windows, performance Board, rigging marcato *work in progress*.

**Community** — aperto il server Discord (invito permanente `ZP2gqQwmDb`, il primo era a
scadenza 7 giorni ed e' stato sostituito prima di andare online). Messo in README (badge
in cima + sezione Community bilingue), `docs/MANUAL.md`, `CONTRIBUTING.md` e nelle note.

**Bonifica dati** — ripulite `sh040.tnz` e `sh050.tnz` del progetto MaggiolataZombie dai
pegbar zombie (backup `.prezombiefix.bak` accanto). `lib_gino.tnz` risultava un falso
positivo: il suo `id="None"` e' un `<parent>`, che e' legittimo.

**Chiusi e tolti dai task attivi** (confermati da Franco): scrub audio del viewer normale
e frame handle condiviso animatic/viewer.

### Prossimo passo — ZtoRig pose-blend (Task 59 + correttive di giuntura)
Design concordato e scritto in memoria; branch **`feature/ztorig-pose-blend`** creato da
master nel worktree `tahoma2d-superplastic` (bundle **Ztoryc-SP.app**). Un solo motore,
due sorgenti di guida (dial keyframabile / angolo del giunto), additivo; UI a pannello
ZtoRig con toggle di link sulle coppie simmetriche; drag IK in opzione C (`base = target
− blend`). Trappola nota: una correttiva non puo' scrivere il parametro che la guida.
Esiste un mockup di anteprima, ma resta **sul branch e non pubblicato**: Franco preferisce
mostrarlo solo quando la cosa funziona davvero, invece di annunciare qualcosa che non c'e'
ancora. Master resta releasabile: il lavoro sta sul branch.


## [2026-07-20] — SUPERPLASTIC su master + fix Windows GPU + performance Board + Global Key scope + diamanti a due assi

> **Riepilogo**: sessione lunga e densa. Merge SUPERPLASTIC su master; risolto il rendering
> offline Windows che usava il rasterizzatore software (utente sbloccato); quattro fix di
> performance del Board tutti da profiling reale (`sample`); Global Key con portata
> Stage/Plastic/All condivisa Animate+Plastic tool; grammatica diamante keyframe a due assi
> (trasformazione + posa plastic) nello xsheet. Viewer navigator + legenda manuale rimandati
> a sessione dedicata (grammatica gia' bloccata, fondo magenta #B01E9A deciso).

### Recupero sessione persa
La chat SUPERPLASTIC di stamattina era stata cancellata dall'indice ma il transcript era
intatto su disco (7d98de6f...): contesto ricostruito e lavoro ripreso senza perdite.

### Rigging (merge `a9d98dc68`)
- Drag IK cross-colonna DETERMINISTICO: baseline press-time (il grafo unificato non viene
  piu' ricostruito dalla posa deformata a ogni mossa → rotto l'anello FABRIK↔CCD che faceva
  oscillare i giunti dentro le catene pinnate). Commit `7dac71339`.
- Multi-anchor per vertici TRA i pin (base del collo con piedi+mano piantati): passa dal
  solveMultiAnchor del single-level via scheletro sintetico.
- Undo multi-colonna raggruppato (ensureCrossLevelBaselines_animate ricablata) + flush
  placement (overlay non piu' disallineato dal disegno).
- Pin visibili su TUTTE le colonne collegate (`14bc4c012`).
- Interruttore diagnostico ZTORYC_SUSPEND_PLANT (`5a55895e8`, inerte senza env).

### Windows — rendering offline (`344ec9d6f`)
WIN32Implementation di TOfflineGL disegna in DIB section GDI col pixel format
PFD_DRAW_TO_BITMAP|PFD_SUPPORT_GDI, che NESSUN driver OpenGL hardware espone → ChoosePixelFormat
ripiega SEMPRE sul rasterizzatore software Microsoft. Ogni thumbnail/icona/renderFrame girava
in software, qualunque GPU. Ora Windows usa QtOfflineGL come macOS/Linux (gia' compilato ovunque).
Fallback ZTORYC_LEGACY_OFFLINEGL. Log diagnostico backend GL (`c0f8788ef`). Candidato PR upstream forte.

### Performance Board (tutto da profiling `sample`)
- Selezione clip: `setStyleSheet` a ogni cambio selezione (68% del tempo in QCss) → stile base
  una volta, evidenziazione in paintEvent (`59e7ffe83`).
- PanelWidget costruiti staccati dalla gerarchia: reparentFocusWidgets O(widget totali) per
  figlio → add shot 20s→12s (`bba1ff405`).
- Aggiornamento incrementale add/delete shot: Shot::childLevel come identita' lato scena,
  onModelResequenced riconosce inserimento/cancellazione e aggiorna solo cio' che cambia →
  12s→istantaneo. Ricostruzione totale resta rete di sicurezza (`merge 1681ae7e7`).

### Global Key scope + diamanti
- Preferenza GlobalKeyScope (Stage/Plastic/All, default All) al posto del booleano
  GlobalKeyIncludesPlastic; combo "Key:" condiviso in Animate e Plastic tool; menu che cicla
  (MI_CycleGlobalKeyScope). setGlobalKeyframe applica la portata. Fix: Animate tool ora chiama
  setGlobalKeyframe (prima solo lo Skeleton tool lo faceva). Commit `78d3a613b`, `e6889b39f`.
- Diamante xsheet a due assi: drawTriPartPredefinedPath (3 regioni), destra vuota=parziale,
  sinistra=quali sistemi (bianco trasf/oro posa/bianco-sopra-oro-sotto entrambi). Nuovo il
  doppio-parziale. Fix detection: iterare i vertex deformation, non isFullKeyframe (pretende
  skelIdsParam su cui nessuno mette mai chiavi). Commit `2ea4a5a44`.

### Rimandato (memoria: project_keyframe_diamond_grammar)
Viewer KeyframeNavigator con la stessa grammatica (icone a codice, fondo magenta #B01E9A,
hint hover) + immagine-legenda inglese per il manuale.

### Aperto — Windows heap corruption
Crash utente `0xc0000374` (STATUS_HEAP_CORRUPTION), pre-esistente ai fix di oggi, prima traccia
solida catturata. Da inseguire col page heap (gflags /p /enable ... /full) in sessione dedicata.

## [2026-07-19] — STEP C: il personaggio cucito diventa UN solo scheletro (+ global key, pin domain)

> **Riepilogo**: 10 commit su `feature/superplastic`. C.1 e C.2 chiuse, solve unificato,
> switch d'appoggio, dominio param-time dei pin, Set Key con posa plastic. Restano aperti il
> controllo del multi-pin sotto stress (due esperimenti falliti, diagnosi ristretta) e 8
> segnalazioni minori raccolte a fine giornata (3 con causa gia' individuata) in ANIMATIC_TASKS.md.

### STEP C.1: autorita' unica del planting + IK di personaggio + global key con posa plastic

Sessione su `feature/superplastic` (commit `3a035bd4a`).

### ✅ STEP C.1 — un solo proprietario del planting
L'autorita' sulla traslazione rigida che pianta un pin si sceglieva **per-pin dal ruolo della
colonna** (`isChildColumn`): figlia -> target scena PINWX/PINWY piantato dallo stage; radice ->
PINTX/PINTY piantato dentro `storeDeformedSkeleton`. Su un rig cucito erano attive **entrambe** e
si contro-traslavano a ogni switch d'appoggio. Ora e' una proprieta' del **personaggio**: nel rig
cucito tutti i pin sono stage-owned, `storeDeformedSkeleton` sopprime la propria traslazione
rigida (e il ciclo di bilanciamento damped), e il DFS stage copre anche la colonna radice.
Rig a colonna singola: percorso invariato.

**Verificato da Franco:** la regressione dello switch non c'e' piu' e i pin reggono sugli
intercalati.

### ✅ Fix — scelta deterministica del pin primario (commit `edeac9f05`)
Sintomo: pinnando il **secondo** piede cedeva il **primo**. Causa: il primario si sceglieva per sola
anzianita' con `if (found && since >= bestSince) continue;`, ma due pin piantati allo **stesso
frame** (cioe' un doppio appoggio) hanno lo stesso `activationFrame` e vinceva quello raggiunto per
primo dal DFS — scelta arbitraria su uno stack LIFO. Se usciva il pin nuovo, il vecchio restava
senza planter. Ora l'ordine e' la chiave totale `(since, colonna, vertice)`. Il solver a colonna
singola gia' evitava il problema con `std::stable_sort`.
**Verificato:** con due pin ora regge il primo e cede il secondo.

### ✅ C.2 — il personaggio cucito e' UN solo scheletro (commit `7ecaf6f11`, `960b9a8d8`)
Le colonne vengono posate, mappate in spazio scena e cucite in **un unico joint tree** (la radice
di ogni figlia legata al vertice-hook del padre: un osso normale, non un caso speciale), poi ci
gira sopra il **solver del livello singolo**. Quel solver e' stato prima **estratto alla lettera**
da `storeDeformedSkeleton` in `PlasticPinSolver::plant`, col percorso a colonna singola migrato e
verificato invariato PRIMA di costruirci sopra il multi-colonna. Un solo solver, quindi nessuna
seconda implementazione che possa contraddire la prima.

Conseguenza: il piantaggio vive **negli scheletri**, mai nella placement.
`computePlasticPinCorrection` e' sparita, e con lei l'ultimo residuo del disegno a due meccanismi.
Il bilanciamento damped arriva gratis, quindi un pin fuori portata non si stacca piu'.

**Ciclo di vita della cache** (imparato a caro prezzo, quattro giri di correzioni):
- il solve azzera tutti gli scheletri risolti prima di comporre, o un re-solve sullo stesso frame
  (= ogni movimento del mouse in un drag) costruirebbe sul proprio output precedente e accumula;
- un cambio parametro scarta lo scheletro risolto, che altrimenti sopravvive ai suoi input (un
  braccio trascinato scriveva gli ANGLE e si vedeva restituire la risposta vecchia: si bloccava);
- invalidare una colonna-arto deve far scadere la placement della colonna **top**, che e' cio' che
  innesca il solve — e va fatto azzerando **direttamente** il timestamp: passare da `invalidate()`
  ricorre sui figli, che risalgono, ciclo infinito (stack esaurito **aprendo una scena**).

**Verificato:** braccia mobili, un pin solido, due pin reggono.
**Aperto:** con due pin la posa "schizza" e il controllo e' grossolano — l'albero unificato da' al
CCD molti piu' gradi di liberta' della catena di una singola colonna. Mitigazioni: **limiti
angolari sui giunti** (prima leva, da mettere comunque nel rig) e smorzamento del CCD per
profondita'.

### ✅ Switch d'appoggio: un solo frame, cross-colonna, undo corretto (commit `cbb2ec4ff`)
Lo switch fa tutto al frame CORRENTE: pianta il vertice selezionato e rilascia ogni altro pin del
personaggio, comprese le colonne diverse da quella corrente — scritte direttamente nella loro
deformazione con coordinate `(riga, colonna)` esplicite per l'undo, **mai** attivando quelle
colonne (un primo tentativo via `TemporaryActivation` aveva peggiorato: nessuna chiave, piede
precedente spinnato, selezione lasciata su una colonna estranea). Scelta di Franco e scelta giusta:
scambiare l'appoggio su una sola chiave e' quello che vuole l'animazione, ed e' cio' che rende
l'operazione semplice — niente frame handle spostato dentro il blocco di undo, tutti gli undo sulla
stessa riga. Identico per rig a livello singolo e cuciti.

Due bug di dominio delle coordinate risolti: il rilascio cross-colonna scriveva a
`paramsTime(frame)` mentre `togglePinAtCurrentFrame` scrive al frame **grezzo**; e l'undo riceveva
il FRAME dove `AnimateValuesUndo` vuole la RIGA (riga = frame+1), quindi agiva un frame prima.

⚠️ **Disallineamento latente da sistemare:** i pin vengono **scritti** al frame grezzo ma **letti**
via `paramsTime` dal solve. Invisibile finche' i due domini coincidono; su una colonna riggata con
repeat/cycling i pin si comporterebbero male.

### 🛠️ Difesa di processo: istanza gia' aperta
Tre volte in questa sessione si e' testato un binario **vecchio** perche' l'app era rimasta aperta
da prima del deploy — una delle quali ha quasi fatto scartare un fix corretto, e un'altra ha
prodotto tre ipotesi diagnostiche sbagliate di fila su un bug che **non esisteva piu'**.
`build_and_deploy.sh` ora stampa un avviso ben visibile quando trova un'istanza precedente al
deploy. (Lo stesso script preferisce ora `Ztoryc-SP.app` quando esiste, e stampa sempre il bundle
di destinazione.)

### ✅ Pin nel dominio param-time (commit `5cf729696`) — [in autonomia, DA TESTARE]
I pin erano SCRITTI al frame xsheet grezzo ma LETTI via `paramsTime` (valutazione, solve
unificato e query del tool). `paramsTime` e' identita' tranne oltre l'ultima chiave stage con
**Cycle** attivo — li' una chiave pin finiva a un tempo che nessuna lettura campiona. Ora tutte le
scritture pin vivono nel dominio param, convertite per colonna: `togglePinAtCurrentFrame` a
`::sdFrame()` (il frame grezzo sopravvive solo per `getPlacement`), lo switch rilascia a
`obj->paramsTime(rawFrame)`, `crossColumns_animate` da' anche alla colonna corrente un paramFrame
convertito, `pinnedVerticesAtFrame` converte in ingresso (idempotente), e ogni `AnimateValuesUndo`
dei percorsi pin riceve riga esplicita = frame scritto + 1. Bit-identico senza Cycle; le scritture
ANGLE dei drag restano alla convenzione upstream (oltre il cycle erano gia' morte).

### ✅ Global key: Set Key include la posa + oro = chiave piena (commit `d08319068`) — [in autonomia, DA TESTARE]
- **Diamante bianco su Set Key**: solo il global key dello stage era stato istruito; lo Z
  dell'xsheet e il KeyframeNavigator passavano da `UndoSetKeyFrame`/`UndoRemoveKeyFrame` intonsi.
  La gestione posa ora sta in QUELLE classi undo — key e unkey, dietro la preferenza, con snapshot
  dello stato plastic alla costruzione. Il navigator instrada entrambe le branch da `undo->redo()`
  invece di operare inline. Deliberatamente NON dentro `setKeyframeWithoutUndo`: serve decine di
  percorsi copy/move/paste i cui undo non sanno nulla di plastic.
- **Oro letto come parziale**: l'oro scattava su QUALSIASI chiave stage che coincidesse con chiavi
  plastic — comprese le parziali (un drag Animate chiavizza solo X/Y). Ora richiede
  `isFullKeyframe`: trasformazione intera + posa intera, o niente oro.
- Nuovo `removePlasticPoseKeyframe` (inverso esatto, pin e limiti intoccati): Z-Z su colonna
  riggata fa round-trip pulito senza chiavi plastic orfane.

### ✅ Damped CCD sull'albero unificato (commit `d80cba2da`) — [in autonomia, DA TESTARE A/B]
Il multi-pin era meno solido del single-level per una ragione strutturale: stesso solver, albero
diverso — catene 2-3x piu' lunghe (attraversano le colonne) e giunti di cucitura sintetici senza
limiti autorali. Il CCD classico ruota il pivot piu' vicino al pin completamente verso il target a
ogni sweep: su quella catena, target vicini trovano configurazioni selvaggiamente diverse →
controllo nervoso, blocco cedevole. Rimedio da manuale: `plant()` accetta `maxStepDegrees` (opt-in)
che clampa la rotazione per giunto per sweep — la portata resta piena (24 sweep x 15° = 360° per
giunto) ma la piega si distribuisce lungo la catena e la soluzione varia con continuita'. Il solve
unificato usa 15°/sweep; il percorso a colonna singola resta a 0 = bit-identico. La costante e' un
solo numero da tarare se 15 risulta troppo rigido o troppo lasco.

### ❌ Esperimento fallito e revertato: plant primary-only durante il drag (`e090f8859` → revert `8d8d58367`)
Ipotesi: la vibrazione/accartocciamento con 2 pin nasce dal tool che cattura nei parametri le
pieghe CCD della valutazione (fit assoluto alla geometria mostrata). Esperimento: durante il drag
la valutazione pianta solo il primario (traslazione, invisibile al write-back). **Attivazione
verificata dal log** (secondo giro — il primo aggancio era in `ensureCrossLevelBaselines_animate`,
che si è scoperto essere CODICE MORTO, zero chiamanti: l'esperimento non girava). Esito con
esperimento attivo: **vibrazione identica** → diagnosi FALSIFICATA. Il loop non passa dal CCD di
valutazione: sta nel write-back stesso (fit-assoluto ↔ traslazione del plant che rimbalzano,
sospetto ping-pong della traslazione dentro/fuori ROOTX/Y). Il fix vero è ridisegnare il
write-back del drag in forma DELTA param-space (come il single-level, che scrive un solo vertice)
— sessione dedicata. Resta attivo il damped CCD (15°/sweep, `d80cba2da`).

### 🐛 Scoperto: undo per-colonna del drag cross-level MAI attivo
`ensureCrossLevelBaselines_animate` (baseline undo + flag `m_ikCrossDragged`) non ha chiamanti —
il caller si è perso in un merge/rewrite. Conseguenza: `finishCrossLevelUndo_animate` non scatta
mai → i drag cross-level non producono gli undo per-colonna previsti. Da ricablare nella sessione
sul write-back (stessa area).

### 🔴 Residui aperti
**Il multi-pin regge un solo pin:** il secondo cede, come atteso. La correzione stage e' una
**traslazione pura** (soddisfa un punto solo) e i pin stage-owned sono esclusi dalla lista locale,
quindi `if (pins.empty()) return;` esce prima di ogni CCD e i secondari non hanno alcun planter.
C.2 deve portare alla valutazione per-colonna il mapping scena->locale che il passaggio stage gia'
compone (`parentP * baseLocal * acc`).

⚠️ **Ostacolo noto per C.2:** `storeDeformedSkeleton` non conosce l'affine world della propria
colonna, e calcolarlo passa dalla placement della colonna, che richiama `storeDeformedSkeleton` →
ricorsione. Se ne esce in **due fasi**: (1) scheletri senza CCD secondario → correzione primaria →
trasformazione di personaggio nota; (2) scheletri con CCD secondario che usa quella trasformazione.
Delicato: tocca codice condiviso viewer/render, attenzione a ordine e staleness.

### Fixed
- **IK di personaggio** — `pinsEnabled` era per-colonna: IK sulla gamba col corpo in FK lasciava la
  root del corpo bloccata e i pin apparentemente inerti. Nuovi helper `characterColumns()` /
  `characterDeformations()` / `enablePinsOnCharacter()`. ✅ verificato
- **Vertice pinnato non trascinabile** — il solver cross-level declinava il drag (il pin e' la sua
  base di re-root) e il fallback FK lo spostava, sganciando il pin senza causa visibile. ✅ verificato
- **`build_and_deploy.sh`** — deployava su `Ztoryc.app` mentre il bundle lanciato era
  `Ztoryc-SP.app`: ora preferisce il bundle rinominato e **stampa la destinazione**.

### Added — Global Key include la posa plastic
Il global key dello stage chiavizza anche la posa plastic sulle colonne riggate, dietro la
preferenza `GlobalKeyIncludesPlastic` (default ON) con toggle nel menu **Xsheet**. Lista di
parametri **curata**: mai i pin ne' gli override dei limiti di giunto, perche' entrambe le famiglie
usano la **presenza di chiavi come interruttore semantico** (PINW = autorita' stage,
MIN/MAXANGLE = override attivo). E' quasi certamente il motivo per cui upstream tiene commentato il
blocco plastic in `setKeyframeWithoutUndo`. Le chiavi con posa plastic hanno il **diamante oro**.

### Da rifinire (sessione separata, richiesta di Franco)
- "Set Key" col toggle attivo produce un diamante **bianco** invece che oro
- selezionando un diamante oro il viewer lo legge come chiave **parziale**, a volte globale
- lo **switch d'appoggio** non scrive ancora la chiave di rilascio sulla colonna padre: un primo
  tentativo via `TemporaryActivation` peggiorava le cose (nessuna chiave, piede precedente
  spinnato, selezione lasciata su una colonna estranea) ed e' stato revertato, con un TODO che
  spiega il vincolo: non spostare colonna/selezione dentro il blocco di undo

### Note di processo
Mezza sessione persa perche' il deploy scriveva su un bundle diverso da quello lanciato. Il primo
giro di test era valido, il secondo e il terzo no — e C.1 e' stata revertata e poi ripristinata
sulla base di un verdetto che non la riguardava. Da qui la riga `Bundle di destinazione` nello
script.

## [2026-07-18] — IK cross-colonna: il pin regge (STEP A+B) + rifiniture controller

Sessione lunga tutta su `feature/superplastic`. Obiettivo: far reggere il **foot-planting su rig
multi-colonna** (corpo+testa+una gamba su una colonna, braccia e gamba dietro su colonne figlie
parentate via handle). Due milestone raggiunte e verificate da Franco.

### Diagnosi — perché l'attachment-pin mirror falliva
Il mirror specchiava il pin del tallone sull'anca del genitore → **sovra-vincolo** della catena 3+
(anca inchiodata + tallone inchiodato, solo il tratto in mezzo libero). Muovendo il ginocchio il pin
cedeva, muovendo il busto pivotava sull'anca invece che sul tallone. È il limite previsto nel design.
Causa di fondo: il tallone è piantato nello spazio **locale** della gamba, agganciato rigidamente
all'anca → la figlia non può traslare contro il genitore → il tallone non resta in world se il corpo
si muove, a meno che la gamba **si pieghi** (IK vera).

### ✅ STEP A — solver IK sul grafo unificato (commit `ed2abbfdd`)
`crossLevelIK_animate`: re-root del grafo unificato **al pin**, rotazione single-joint del
sotto-albero trascinato, write-back ANGLE per colonna + **ROOTX/ROOTY sulla sola colonna radice**
(traslazione free-root). Nuovi param SkVD `ROOTX/ROOTY` applicati in `updateBranchPositions`, gated
da un flag così single-level e FK cross-livello restano invariati. `togglePin` su colonna figlia
setta **solo** il flag PIN (niente PINTX locale, niente mirror). Gestito il nodo "incollato"
(anca ≡ root gamba, osso lunghezza-0) e resa trascinabile la root del corpo quando il pin è su una
figlia. Verificato: punta, ginocchio, anca e busto reggono tutti il tallone.

### ✅ STEP B — hold sugli intercalati (commit `5d2671842`)
Il drift tra le chiavi è **inerente** all'interpolazione degli angoli: nessun keyframing lo risolve.
Fix per-frame a livello **stage-placement** (viewer e render condividono `getPlacement` → coerenti
per costruzione): nuovi param `PINWX/PINWY` = target del pin in spazio scena, e
`TStageObject::computePlasticPinCorrection` che pre-trasla il personaggio perché il vertice pinnato
torni sul target ad ogni frame. Catena composta a mano → nessuna ricorsione su `getPlacement`.
A differenza del `computeIkRootOffset` nativo **non c'è foot-chaining**: ogni attivazione ri-cattura
il target assoluto, quindi le correzioni non si accumulano.

### Rifiniture controller (commit WIP `221693bdb`)
- **Overlay che segue durante il drag**: `updateMatrix()` anche in move/scale (prima lo scheletro
  restava indietro e scattava al rilascio).
- **Fix feedback di coordinate**: i delta si calcolano nella matrice congelata al press
  (`m_ctrlPressMatrix`) — la matrice viva si muove coi valori scritti e faceva rimbalzare (padre) o
  vibrare (figlio) la maniglia.
- **Sway dei livelli figli**: la maniglia Move su una colonna figlia agganciata scrive ora l'**X/Y di
  colonna** (stesso canale dell'Animate tool) invece del TRANS del controller → mesh + scheletro +
  nipoti si spostano insieme restando agganciati. Hint contestuali sulle maniglie.
- **Provvisorio, non convince ancora**: col pin attivo il Move sul padre è lockato e Cmd+drag sposta
  tutto coi target al seguito. Manca l'undo dello spostamento dei target.

### 🔴 Regressione nota — da risolvere in STEP C
Con un pin sulla colonna **radice** (plant dentro lo scheletro) e uno su una **figlia** (hold a
livello placement), lo **switch d'appoggio della camminata** fa spostare il piede precedente: l'unpin
trasferisce il plant al TRANS del controller, questo muove l'aggancio della figlia, e la correzione
di placement contro-trasla tutto il personaggio. **I due meccanismi di planting si combattono.**
Workaround per lavorare: niente pin sulle colonne figlie → comportamento identico a v0.9.

### STEP C (prossima sessione, pezzo architetturale)
Unificare il planting in **un solo passaggio per-frame** che veda entrambi i tipi di pin. Ne
discendono anche: multi-pin cross-colonna (oggi regge un solo pin — la correzione è una traslazione
pura e può soddisfare un punto solo), la regressione dello switch, e il buco dell'undo.

### Note
- Squash & stretch con IK: sui pin **cross-colonna** il piede resta piantato (la correzione legge la
  posizione col controller già applicato). Sui pin **single-level** invece lo squash sposta il pin —
  rimedio attuale: mettere il pivot del controller sul vertice pinnato (snap ai vertici).
- Il worktree `feature/superplastic` è ancora a **0.8.1** (il bump a 0.9.0 è solo su master):
  cosmetico, il merge lo riallinea.
- ⚠️ `build_and_deploy.sh` ha `DEFAULT_WS` hardcoded al workspace master: per il worktree serve
  **sempre** `ZTORYC_WORKSPACE=.../tahoma2d-superplastic`, altrimenti compila e riapre il master.

## [2026-07-13] — v0.9.0 rilasciata (rigging IK) + fix drag colonna figlia + rifiniture UI

Merge di `feature/superplastic` su master e **release pubblica v0.9.0** (macOS + Windows), note
bilingui. Il branch resta vivo per continuare il rigging.

### Rilasciato in v0.9.0
- **Rigging/IK SUPERPLASTIC**: IK single-level (pin/foot-plant per-frame, limiti angolari, bake in
  FK), scheletri cross-level (vista+selezione unificata, posing cross-colonna via attachment-pin,
  FK unificata sul grafo combinato, pick redirect su root-figlia coincidente), **toggle gizmo
  controller**.
- Incluso anche: Kitsu (skip-unchanged/pull asset-status/team sync), Production Tracker asset types,
  Edit Cels/Keys, vector fill fix, **crash handler Windows** (`set_terminate` → `Crash-*.log`+`.dmp`
  simbolicati coi PDB già inclusi nel build RelWithDebInfo).

### Fix (post-v0.9.0, su master → nella release)
- **Drag colonna figlia**: lo scheletro non si disallinea più dalla mesh durante il drag (refresh
  xsheet in tempo reale quando la colonna corrente ha un genitore-colonna). Prima si correggeva solo
  al rilascio.

### Rifiniture UI (branch, non ancora rilasciate)
- Bottone **Pin** con **icona** (icon-only, da `design/pin.svg` ripulito); "Inverse Kinematics" → **"IK"**.
- RIMANDATO (ristrutturazione layout): IK a sinistra del Pin, Maintain dopo Scale V, Controller Gizmo
  prima di Scale H.

### Tester Windows (crash)
- Il tester era su 0.8 (senza crash handler). v0.9.0 lo include + PDB → al prossimo crash otterrà un
  `Crash-*.log` simbolicato. Nessuna build speciale necessaria.

### Note / prossimi passi
- **Compatibilità export OT/Tahoma** dei nuovi param Plastic: verifica RIMANDATA a rigging finito
  (formato ancora in evoluzione). Atteso: file si apre (tag-based skip), posa diversa se IK/controller
  live → bake IK prima di esportare; controller squash&stretch non bakabile.
- **Prossimo**: path B — IK cross-colonna per-frame nel Plastic (ispirato a `computeIkRootOffset` dello
  Skeleton nativo). Prima documentarsi su Harmony/Moho.

## [2026-07-12b] — SUPERPLASTIC: cross-level IK (saga) → attachment-pin checkpoint + modello unificato

Sessione lunga interamente su `feature/superplastic`, guidata dai test dal vivo di Franco su un rig
multi-colonna (corpo + mano/treccia parentate via handle `H<n>`). Obiettivo: posare il corpo con un
**pin end-effector** (es. la mano) che resta fermo nello spazio. Molte iterazioni, ognuna ha chiarito
il modello.

### Percorso (per memoria; dettaglio in project_superplastic_worktree.md)
- **v1/v2 re-root + PINTX live** → esplodeva: feedback dell'ancoraggio + matrice colonna stale
  (`getPlacement`/`computeLocalPlacement` cachano per-frame; serve `invalidate()`). Finding:
  la colonna figlia eredita dal bend del genitore **solo la traslazione** dell'handle, non la
  rotazione (tstageobject.cpp:1532 + xshhandlemanager.cpp).
- **v3 CCD reach** (piega il braccio per tenere la mano) → “spalla quasi ferma”, ma gomito/polso/pin
  su altra parte incompleti.
- **v4 re-root sul grafo unificato** → “root inchiodata”: il re-root appende il corpo al pin, il
  vertice trascinato è vincolato a un arco → poco controllabile. Diagnosi con log su file + marker
  magenta a schermo: nessun mismatch di spazio (solver=eval), era la reach del CCD.
- **Chiave (storeDeformedSkeleton:946)**: il single-level tiene il pin primario **traslando
  rigidamente l’intero scheletro** sul target PINTX/PINTY a eval-time → **root libera**, non piega
  niente. Continuavo a piegare (CCD) → sbagliato.

### Fatto e committato (checkpoint `af2fc6049`)
- **Attachment-pin mirroring**: un pin su una colonna figlia viene specchiato sul **vertice-aggancio
  del genitore**, passando dalla stessa `togglePinAtCurrentFrame` (quindi bake della posa + transfer
  della traslazione al controller all’unpin = **niente scatto**, e **ricorsione su per la
  gerarchia**). Il posing del corpo diventa **puro single-level** col pin-polso → root libera, pin
  esatto, controllabile. **Regge benissimo a 2 colonne** (foglia+radice). Undo unico (block).
- Limite: catene **3+ colonne** → le colonne intermedie si sovra-vincolano (entrambe le estremità
  pinnate) → “root avambraccio bloccata”.

### Modello DEFINITIVO per il prossimo giro (definito da Franco)
Nel cross-level **le root di colonna si annullano e diventano vertici normali** che linkano un
livello all’altro (vale per **FK e IK**). Uno **scheletro unico**, una sola root effettiva, l’IK
single-level gira su quello (pin primario = traslazione rigida dell’intero rig → root libera,
single-joint, multi-pin), write-back ANGLE per colonna (l’angolo del primo giunto della figlia
assorbe la rotazione ereditata → nessuna modifica all’eval). Sostituirà attachment-pin e il vecchio
path FK per personaggi multi-colonna. È il rework grosso, rimandato a sessione fresca per budget.

## [2026-07-12] — Kitsu “chiuso” (master) + Plastic multi-livello: vista/selezione unificata (feature/superplastic)

Sessione doppia guidata dai test di Franco. Kitsu completato su `master`; nuovo filone
SUPERPLASTIC multi-livello su `feature/superplastic`.

### Added / Fixed — Kitsu (master, `kitsuclient.cpp/.h`, `ztoryproductionpanel.cpp`, `kitsuconnectdialog.cpp`)
- **Push status skip-unchanged** (shot + asset): il push confronta lo status target con quello
  corrente in Kitsu (catturato dal GET tasks) e **salta gli invariati** — niente commenti/
  notifiche ridondanti sulla activity feed. Messaggio: “N changed, M unchanged”.
- **Pull asset task-status** — `pullAssetStatuses()` (gemello di `pullStatuses`): asset-types→
  assets→task-types(Asset)→tasks→`assetStatusesPulled`; agganciato al bottone “Pull assets
  from Kitsu” (prima entità, poi status, add-only con match kitsuAssetId poi type+name).
- **Team/assignee sync** — struct `KitsuPerson`; `pullTeam()` popola il roster `m_team` dal
  **team del progetto** (`GET projects/<id>?relations=true` — il campo `team` è m2m, senza
  `relations=true` è assente); pull assignee (union add-only su shot+asset); **push assignee
  add-only ristretto ai membri del team** (`PUT actions/tasks/<id>/assign`, chi è fuori team
  viene saltato e riportato). Helper condiviso `loadRosterThen(projectId,next)` (persone +
  team-ids). Roster tirato giù **automaticamente alla connessione** (onLink, loginFinished,
  apertura panel).

### Added — SUPERPLASTIC multi-livello (feature/superplastic, `plastictool.h/_animate.cpp`)
- Milestone ridefinito (scartato l'adapter del vecchio Skeleton tool): **Plastic tool che gestisce
  scheletri su più livelli connessi in gerarchia** (personaggio articolato su più drawing level).
  Finding: “vertice→vertice” tra mesh = parenting di colonna su handle `H<n>` (risolve al vertice
  deformato del genitore); la FK cross-livello già funziona in eval, nessun nuovo modello dati.
- **Vista unificata** (`connectedSkeletons_animate`): BFS sul parenting, disegna gli scheletri
  delle colonne connesse come contesto attenuato, piazzati via `A_C = getMatrix()⁻¹·getColumnMatrix(C)·ctrl_C`.
- **Selezione cross-livello**: click su un vertice di un'altra colonna → la rende attiva + seleziona.

### Fixed — SUPERPLASTIC bug latente (feature/superplastic, `toonzlib/xshhandlemanager.cpp`)
- La gerarchia multi-livello si **sganciava** muovendo un livello con controller squash attivo:
  `getHandlePos` risolveva l'handle-vertice del genitore in posizione **pre-controller** mentre la
  mesh renderizzata è post-controller. Fix: applica `getSquashControllerAffine` al vertice prima
  dello scale 1/inch. No-op sui rig senza controller (identità). Bug SP, non stock.

### Notes
- Prossimo SUPERPLASTIC: **IK/pin cross-livello** (grafo unificato in spazio comune + write-back
  dispatchato per colonna) — rimandato a sessione fresca. Rifiniture: hover cross-colonna.

## [2026-07-11c] — Thumbnail room, Production Tracker, Edit Cels/Keys, Kitsu asset-task push

Sessione lunga guidata dai test in parallelo di Franco. Tutto su `master`, non ancora sotto SUPERPLASTIC.

### Fixed — Thumbnail room (`ztorythumbnailcanvas.cpp/.h`)
- **Panel "affettati/compressi" al reopen**: il reopen ricostruiva `m_boxAspect` dall'altezza intera del PNG salvato → drift sub-pixel su camere non-16:9 (>1e-4) → `onSceneChanged` lanciava un reflow spurio che ritagliava i disegni cross-box e lasciava ghost-seam. Fix: `onSceneChanged` confronta l'**altezza raster in pixel interi** invece dell'aspect float; stesso layout → nessun reflow. Bug invisibile su 16:9 (per questo non si replicava su Mac).
- **Tasto Canc in Transform**: l'eventFilter intercettava solo KeyPress, ma Canc è una scorciatoia globale (cancella celle) → mai consegnato. Ora gestisce anche `ShortcutOverride` (predicato `wantsTransformKey`), reclamando Del/Backspace/Invio/Esc/Cmd+C/V prima che la QAction globale li mangi.
- **Undo del float**: lo stato del float (immagine + trasform + srcRect + wasMove) entra ora nello `Snapshot`; `deleteFloat` registra uno snapshot col float invece di scartarlo → Cmd+Z dopo Canc/incolla **fa riapparire** il disegno flottante. `cancel`/`delete` non lasciano più snapshot fantasma.

### Fixed — Crash handler Windows (`crashhandler.cpp`) — candidato PR upstream
- Le eccezioni C++ non catturate (TException & co.) e gli abort CRT morivano **senza** log (il VEH cattura solo structured exceptions). Aggiunti `std::set_terminate` (cross-platform, cattura anche il **messaggio** dell'eccezione via `TException::getMessage()`/`what()`) + `_set_invalid_parameter_handler`/`_set_purecall_handler` su Windows. Ora il tester ottiene `Crash-*.log` + `.dmp` anche in quei casi.

### Added — Production Tracker
- **Tipi asset custom + pipeline task per-tipo** (Kitsu-aligned): nuova struct `AssetType{name,taskTypes}`, persistita in `production.ztrack` (`<assetTypes>`), seedata dai canonici, tab **"Asset Types"** (editor a due pannelli come i Workflow). Tabella Assets: colonne = unione dei task dei tipi usati, celle attive solo per la pipeline del tipo dell'asset. Picker tipo usa i custom. (`ztorymodel.h/.cpp`, `ztoryproductionpanel.h/.cpp`)
- **Ordine task workflow → schermata shot**: `spreadsheetTaskColumns()` ordina per la pipeline del workflow, non più per l'ordine canonico → riordinare i task nel workflow si riflette subito negli shot.

### Added / Fixed — Edit Cels/Keys + export
- Menu celle "Edit Cell Numbers" → **"Edit Cels/Keys"**; aggiunto lo stesso submenu (Reverse/Swing/Rollup/Rolldown/TimeStretch) nel menu contestuale delle **chiavi** — operano sulle chiavi via le versioni key-only già cablate in `TKeyframeSelection::enableCommands`. (`xshcellviewer.cpp`)
- **Render Settings dall'export animatic**: popup dedicato **senza** bottoni Render/Save-and-Render (`OutputSettingsPopup::setRenderButtonsVisible`), parented al dialog export con `Qt::Window` così chiuderlo non termina l'export. Fix crash `EXC_BAD_ACCESS`: il lambda del bottone catturava un `QPointer` locale block-scoped che dangling-ava durante `loop.exec()` → ora il popup si trova via `findChild` su `dlg`. (`outputsettingspopup.h/.cpp`, `storyboardpanel.cpp`)

### Added — Kitsu: push asset-task + status
- `pushAssetTasks` (gemello di `pushTasks` per `for_entity=Asset`) + `buildAssetTasksFromModel` (asset × pipeline del suo tipo, con status), concatenato dopo `assetsPushed` come per gli shot. (`kitsuclient.h/.cpp`, `ztoryproductionpanel.cpp`)

### Notes / TODO
- **Kitsu ancora da fare** (rimandato dopo validazione del push contro istanza locale): **team/assignee sync bidirezionale** (pull persone → roster, push assignee sui task; il client non ha nulla per persone/assignee) e **pull asset-status** (gemello di `pullStatuses`).
- Rinominare un tipo asset non migra gli asset esistenti (parità con la rinomina workflow).
- Crash "cambio workflow storyboard→cutout" segnalato una volta, non riproducibile poi (nessun listener su `workflowChanged`).

## [2026-07-11b] — SUPERPLASTIC: pin robusti + limiti angolari visivi + undo bake (feature/superplastic)

Seconda parte della sessione (dopo il controller). Tutto su `feature/superplastic` (commit finale `965ebd3ed`, pushato). Guidato dai test/stress-test di Franco.

### Fixed / Added — pin & doppio appoggio
- **Limiti angolari applicati in modalità pin** (prima solo FK): clamp nel write-back unificato + nel CCD dell'eval (le pieghe "a cascata" ora rispettano i min/max; helper relAngleDeg).
- **Unpin dell'ultimo pin senza shift**: la traslazione rigida del planting va nei canali TransX/TransY del controller (chiave di confinamento a f-1, mappata con la parte lineare → esatta anche sotto squash).
- **IK off = bake completo dell'animazione pinnata** (`bakePinsToFK_animate`): itera TUTTI i keyframe con pin attivo, cattura la posa piantata via storeDeformedSkeleton a frame esplicito, elimina PIN/PINTX/PINTY, bakes forma negli ANGLE + traslazione nel controller → ogni chiave resta identica, rig FK puro e libero (risolve "spegnendo IK a metà si perde l'animazione successiva"). Confinamento del transfer alla prossima attivazione pin per le camminate a piedi alternati.
- **Constraint del doppio appoggio**: i pin secondari non si staccano più a fine corsa — ciclo di correzione (trasla verso il residuo medio + ri-pianta) che assesta il corpo dove tutti i piedi restano a terra; soglia RELATIVA alla scala del rig + damping + più sweep CCD → niente piccoli spostamenti con 3+ pin asimmetrici (2 e 4 già ok). No-op nel caso raggiungibile (primario esatto preservato).
- **Undo del bake IK-off** (`BakeToFKUndo`): snapshot SkDKey per-frame a tutti i frame coinvolti prima/dopo, ripristino wholesale; riporta su anche checkbox IK + visibilità pin. Il bake non è più distruttivo.

### Added — limiti angolari keyframabili + gizmo visivo
- Param SkVD **MINANGLE/MAXANGLE** = override keyframabile del limite statico del vertice (i limiti del giunto possono cambiare nel tempo); senza chiavi = statico di sempre (retrocompat). Nel function editor come canali MinAngle/MaxAngle.
- **Gizmo nel viewer**: due maniglie draggabili (min/max) su un arco attorno al genitore + cuneo del range consentito; drag = angolo mouse vs rest; hover arancio, hit-test prima della selezione. Commit = chiave se già keyato, altrimenti statico.
- **Toggle "Angle Bounds Gizmo"** (default OFF) per tenere pulito lo scheletro; campo toolbar **live** durante il drag; **undo** del drag (AngleLimitUndo: statico + snapshot SkVD).

### Notes
- Restano per SUPERPLASTIC: **adapter Skeleton Tool** (milestone grosso), pole vector opzionale, campi toolbar per trans/rot/shear, dedup ctrlContrastColor. Merge su master rimandato (regression pass sui file core + già fatto l'undo del bake).
- **Kitsu**: il flag `useKitsu` è creation-only (attributo su `<project>` in `production.ztrack`). Per attivarlo su un progetto esistente: aggiungere `useKitsu="1"` al tag `<project>` ad app chiusa (workaround dato a Franco, ha funzionato). Da fare: checkbox in-app "Enable Kitsu" nella Production room.

## [2026-07-11] — SUPERPLASTIC: controller "Animate tool sopra lo scheletro" + task 62 vector fill (master)

Sessione doppia: fix core su `master`, poi tutta l'evoluzione squash&stretch → controller su `feature/superplastic`.

### Fixed — master (task 62, candidato PR upstream, commit `021d6886d`)
- **Vector fill che si "ripara" solo ricaricando la scena**: nuovo `TVectorImage::forceRegionsRecompute()`
  (stesso rebuild del load, colori preservati) chiamato dal FillTool su attivazione e cambio frame
  (guardia isPlaying). **Maximum Gap che si resettava al cambio frame**: `m_lastUserGapValue` propaga
  l'ultimo valore utente alle immagini ancora a tolerance default. Verificato Franco: gap OK, fill in osservazione.

### Added — feature/superplastic: controller completo (commit finale `c7de1b20f`)
- **Architettura (design Franco, 3 iterazioni)**: lo squash&stretch NON entra mai nella catena dello
  scheletro. È un'affine controller T(trans)·T(C)·Rot·Shear·Scale·T(−C) composta SOPRA il risultato
  deformato (`getSquashControllerAffine`), iniettata nei 3 siti di draw/render (stagevisitor ×2 +
  plasticdeformerfx) e nella matrice del tool (`updateMatrix` override) → manipolazione, pin e IK
  lavorano in spazio pre-controller PER COSTRUZIONE (spariti i feedback loop che "esplodevano" la posa).
- **Pivot keyframabile che segue il personaggio**: offset PIVOTX/PIVOTY dalla root DEFORMATA (default 0).
- **Param**: SCALEX/SCALEY (fattori, 100%=neutro, measure "scale"), TRANSX/TRANSY, ROT, SHEARX/SHEARY —
  tutti sul vd della root, serializzazione tag-based retrocompatibile, esclusi dal Set Key (p<PIN).
- **Gizmo "modalità all"**: doppio ESAGONO al pivot (drag=sposta pivot, snap ai vertici) + raggi
  TRATTEGGIATI (identità visiva vs Animate tool di colonna, mockup approvato); disco=rotate,
  quadrati=scala uniforme/libera, parallelogramma=shear, rombo=move. Matematica replicata dai
  Drag*Tool di edittool (Shift/Alt/combo Maintain con Mass=1/v). **Colori dinamici** come l'Animate
  tool Ztoryc (sample framebuffer + contrasto complementare, highlight per-maniglia) + hint in hover
  (gotcha: testo GLUT da scalare ×devPixRatio o su retina è invisibile).
- **Global key** chiava anche i param del controller (PIN* sempre esclusi).
- **Pin dormienti**: IK off → diamanti nascosti, manipolazione pin-aware spenta, bottone Pin
  disabilitato; IK on → tornano identici (chiavi intatte). Il PLANTING all'eval NON è gated (il primo
  tentativo spostava la posa al toggle): flag `pinsEnabled` sulla deformazione (tag `PinsDisabled`),
  checkbox sincronizzata allo switch colonna se il rig ha chiavi PIN.

### Notes
- Trade-off accettato: i pin secondari non "tengono" sotto squash (niente stretch braccio-barra);
  pivot snappato sul pin d'appoggio copre il caso principale. `ctrlContrastColor` duplicata da
  edittool → da condividere prima di eventuale PR. Restano: cursori per-maniglia, campi toolbar
  trans/rot/shear, taratura stiffness, limiti angolari in pin mode, adapter Skeleton.
- Task 62: voce PR candidates aggiunta in AGENTS.md; nota implementazione in SUPERPLASTIC.md.

## [2026-07-10] — SUPERPLASTIC: multi-pin completo (eval 2-target, manipolazione simmetrica, cambio appoggio)

Branch `feature/superplastic` (Ztoryc-SP). Sessione interamente guidata dai test di Franco.

### Fixed — falsa regressione "root non draggabile"
- NESSUNA regressione nel codice: `Ztoryc-SP.app` era la build PRE-revert del 07-07 (rename
  mancato dopo l'ultima build) + scene di test inquinate da chiavi PIN della build rotta.
  Controprova su scena nuova = ok. Morale: rename dopo OGNI build; bonifica scene = unpin di
  tutti i diamanti e re-pin.

### Added — eval multi-pin (`storeDeformedSkeleton`)
- Pin PRIMARIO = il più anziano (attivazione, non indice) → traslazione rigida (root libera,
  nessun salto di posa quando si aggiunge il 2° pin); pin successivi → CCD confinato sotto la
  divergenza dalle catene già piantate. Robusto ai residui (pin senza target saltati).

### Added — manipolazione multi-pin (plastictool_animate)
- Re-root sul pin PIÙ VICINO al vertice trascinato; gli altri pin vengono ri-piantati DENTRO
  il drag (CCD sul solo loro arto) → tool e eval non si combattono più (fine dei vertici che
  "scappano" vicino ai pin).
- Pin DURI: θ-bisezione nel posing locale (il drag si irrigidisce a fondo corsa invece di
  strappare un pin); target del mouse clampato alla portata nel solve simmetrico.
- Vertice TRA i pin (sottoalbero spanning) → FABRIK multi-ancora: entrambe le catene si
  piegano (barra: entrambe le spalle salgono). Stiffness per profondità ELASTICA verso la posa
  di riposo (clavicole rigide che TORNANO, gomiti assorbono per primi — niente cricchetto);
  lunghezze ossa ripristinate post-solve + re-nail di ogni pin sul ramo esclusivo (la media
  FABRIK alle giunzioni violava le lunghezze → pin che si staccavano). Pivot extra = ultimo
  vertice condiviso, ruotando solo il ramo del pin (la clavicola cede se serve al planting).
- Mount del vertice trascinato (tratto v→top) esente da stiffness → clavicole e anche
  manipolabili direttamente senza resistenza.
- Ancore ai target assoluti PINTX/PINTY (niente deriva accumulata nei drag lunghi).
- Bake della posa piantata all'UNPIN → il vertice non scatta più al toggle-off. Caveat: unpin
  dell'ULTIMO pin può mostrare shift globale (traslazione non rappresentabile negli angoli) —
  si ricollega alla task squash/pivot.
- **"Switch Support Pin Here"** (context menu, cambio appoggio one-click): pinna il vertice
  selezionato al frame corrente e rilascia gli altri pin a f+1 (double support su una sola
  chiave), tutto in un unico undo. La chiave PIN=0 a f+1 è by-design.
- Pin selezionato = diamante ciano PIENO (feedback visivo).

### Fixed — CRASH switch colonna (candidato PR upstream)
- `PlasticTool::onSelectionChanged()` dereferenziava `m_sd->skeleton(skelId)->vertex(m_svSel)`
  senza guardie: su switch colonna skeleton può essere null e la selezione un indice stale →
  SIGSEGV (repro: click su altra colonna con tool attivo). Fix: guardia + drop della selezione
  stale. Codice STOCK Tahoma2D → candidato hardening upstream.

### Aperti (prossima sessione)
- Taratura fine pesi stiffness (0.8/0.5/0.2 per profondità) se serve.
- Limiti angolari in modalità pin; pole vector; adapter Skeleton Tool; task squash/pivot.

## [2026-07-07c] — SUPERPLASTIC: timing pin corretto (fix "shift al 2° passo")

Branch `feature/superplastic` (Ztoryc-SP, master intatto). Commit `ed8daf47e`. ✅ Franco "meraviglioso".

### Fixed — keyframing dei pin IK
- **Pin fantasma all'indietro**: una chiave PIN=1 messa a un frame > 1 si estrapolava costante
  all'indietro → il piede risultava pinnato anche sui frame precedenti. Fix: baseline PIN=0 al
  frame 1 alla prima chiave "on" (confina il pin a [frame, …)).
- **Target stale ("il 2° pin prende i valori di un frame passato" e sposta tutto)**: il target
  PINTX/PINTY veniva catturato DOPO aver attivato PIN → l'eval ripiantava il vertice sul suo
  target vecchio (residuo Constant di un pin precedente) e si registrava quella posizione stale.
  Fix: cattura del target PRIMA di attivare PIN (posizione reale corrente). Diagnosi via log su
  file dei keyframe reali.
- **Global Set Key** non keyframa più PIN/PINTX/PINTY: propagava in avanti il target vecchio
  (Constant), inquinando i frame successivi. I pin restano keyframati dal loro toggle.

### Aperti (prossima sessione)
Ri-abilitare la eval a 2 target (centroide + CCD per-pin, oggi rivertita) ora che i pin fantasma
sono risolti; anomalia braccio→gamba nella manipolazione; comando one-click "cambio appoggio".

## [2026-07-07b] — SUPERPLASTIC: manipolazione IK rifatta (root libera / pin=root provvisoria)

Branch `feature/superplastic` (build separata Ztoryc-SP, master NON toccato). Rifatta da capo
la manipolazione IK del Plastic Tool sul modello corretto emerso dal feedback diretto di Franco.

### Fixed / Reworked — manipolazione IK a 1 pin (✅ Franco: "MERAVIGLIOSO")
- **Posa sempre a giunto singolo locale**, mai chain-solve: il CCD nel drag distribuiva la
  rotazione su tutta la catena radice→handle riconfigurando il corpo in modo incontrollabile.
  Rimosso il CCD/`solveChainIK_animate` dal drag.
- **Senza pin**: giunto singolo sulla gerarchia originale (come manipolazione normale).
- **Con pin**: il pin è una **root provvisoria** (re-root BFS); il drag ruota solo il bone verso
  il genitore-re-rooted a lunghezza costante, il sottoalbero segue rigido, il lato-pin resta
  fermo; planting a eval-time via PINTX/PINTY (traslazione rigida singola).
- **La ROOT nativa è manipolabile** sotto IK con pin — era l'unico vertice il cui drag non era
  rappresentabile (niente ANGLE): ruota attorno al vicino verso il pin, l'angolo scritto è quello
  del vicino, conserva le lunghezze. Sbloccarla ha risolto l'ingestibilità della zona vicino alla
  root. (commit `13ca6724d`)

### WIP parcheggiato — due pin / IK a 2 target (NON attivo)
- Tentato foot-planting a 2 pin (centroide+traslazione, CCD per-pin per snappare ogni pin, draw
  multi-diamante, `pinnedVerticesAtFrame`, re-àncoraggio pin). Multipli bug nei test di Franco:
  shift al 2° passo (pin "fantasma" non rilasciati — PIN keyframe Constant persiste), il 2° pin
  non blocca il movimento della gamba del 1°, anomalia braccio→gamba. **L'eval a 2 pin è stato
  RIVERTITO al single-pin** (`git checkout` di `plasticskeletondeformation.cpp`) per non regredire
  la root libera a 1 pin. Lo scaffolding lato-tool (helper multi-pin + draw) resta committato come
  base. Da rifare la prossima sessione partendo dalla **semantica di rilascio/switch dei pin** (un
  pin vale solo nel suo intervallo) + comando one-click "cambio appoggio". Dettaglio dei 4 punti
  aperti nella memoria `project-superplastic-worktree`.

## [2026-07-07b] — Board/thumbnail room: hang export risolto + performance (disegno e timeline)

Sessione di debug su master, tutta guidata da evidenze (`sample` sul processo bloccato),
su segnalazioni di Franco sulla scena `SB_maggiolatazombie`.

### Fixed — hang dell'export-to-board
- **Causa (root)**: `StoryboardPanel::onDeleteShot` cancellava la colonna dell'xsheet ma
  lasciava nel cast la sotto-scena e i suoi livelli OVL. Il nome restava occupato → il Send
  to Board successivo riusava l'etichetta → `createNewLevel` riceveva un nome già nel
  levelSet → la sua disambiguazione `_N` viene riletta come **separatore di frame**
  (`sh150_1` → livello `sh150`) → **loop infinito**. Confermato dal sample: 100% dei campioni
  in `addShotFromRasters → createNewLevel → doesExistFileOrLevel`.
  Fix (`2bdb3d19e`): raccolta dei livelli esposti dagli shot cancellati (child level +
  `getUsedLevels()` della sotto-scena, ricorsivo) e rimozione dal cast dei soli livelli senza
  utilizzatori (`isLevelUsed` → un Copy Shot che condivide il livello lo mantiene). I livelli
  non vengono distrutti: `UndoBoardState` li possiede via `TXshLevelP` e li reinserisce in
  `undo()` prima di ripristinare le colonne.
- **Rete di sicurezza** (`be4856c69`): in `addShotFromRasters` il controllo di collisione ora
  guarda **levelSet in RAM + disco** (prima solo disco: i livelli restano in RAM fino al
  salvataggio) e fa rollback invece di passare a `createNewLevel` un nome occupato.

### Performance — thumbnail room (lag di disegno)
- `paintEvent` chiamava `rasterToQImage(..., mirrored=true)`: il wrapper è a costo zero ma
  `QImage::mirrored()` **deep-copia l'intera superficie** (~31 MB su griglia 4×15) a **ogni
  repaint**, cioè a ogni mouse move durante il tratto. Ora vista zero-copy + flip via
  trasformazione del painter.
- `pushUndo()` clonava tutto il raster a **ogni inizio tratto** (hitch) e con `kMaxUndo=16` la
  cronologia arrivava a ~500 MB (pressione di memoria → lag intermittenti che alteravano il
  segno). I tratti usano ora un undo **copy-on-write a tile 256×256** via `askWrite()` (che il
  pennello MyPaint chiama prima di scrivere). Snapshot pieno mantenuto per resize/paste/transform.
  Commit `91f167a2b`.

### Performance — timeline animatic (stallo ~2s a ogni trim)
- Sample: `onShotDurationChanged → resequenceXsheet → modelReset → clearThumbCache`, poi
  `refreshFromScene` rirenderizzava **ogni** shot con `IconGenerator::renderXsheetFrame` →
  `ToonzScene::renderFrame` → **`QtOfflineGL::createContext`** (un contesto GL offscreen nuovo
  per thumbnail, da solo ~40% dello stallo). Ma trimmare cambia la durata, non il disegno.
- La cache era chiavata sull'**indice di colonna**, quindi andava svuotata a ogni `modelReset`
  (i riordini fanno scalare gli indici). Ora è chiavata sul **nome della sotto-scena**, stabile
  a trim/riordini/cancellazioni. Clear rimosso da `modelReset`; aggiunto all'uscita da una
  sotto-scena (unico punto da cui un disegno può cambiare). Commit `2f20de7f2`.

### Upstream candidates
- **Loop infinito di `createNewLevel` con nome livello occupato** (`toonzlib/toonzscene.cpp`):
  il suffisso `_N` della disambiguazione viene riletto da `TFilePath` come separatore di frame,
  quindi il nome collassa su sé stesso e il ciclo non termina mai. Codice Tahoma/OpenToonz
  condiviso, innescabile da chiunque. Alta priorità.

### Performance — Board: rebuild completo a ogni resequence (drift falso positivo)
- Ogni trim ricostruiva da zero **ogni istanza di Board** (`clearShots` + rebuild widget +
  `loadZtoryc()`, che **rilegge il `.ztoryc` da disco**). Misurato sul log: **24 `full rebuild`**
  in pochi trim, con i conteggi coincidenti (42 shot = 42).
- Causa: la detection di riordino confronta il nome della colonna con l'etichetta dello shot,
  ma `TStageObject::getName()` per una colonna **senza nome esplicito** restituisce il default
  `"Col<N>"` — mai uguale a `"sh130"`. Drift a ogni resequence, per sempre.
- Colpevole a monte: `addShotFromRasters` (export-to-board) **non chiamava `updateColumnName()`**,
  a differenza di ogni altro percorso di creazione shot → tutte le colonne anonime.
- Fix (`5c245fd20`): (1) confronto solo se `obj->hasSpecifiedName()` — una colonna anonima non
  porta informazione d'ordine; (2) `addShotFromRasters` nomina la colonna, il che **ripristina
  anche la detection dei riordini** per gli shot da export-to-board, finora di fatto rotta.
- Verificato sul log, stessi trim: `full rebuild` **24 → 0**.

### Performance — StoryStrip (stesso pattern)
- `ZtoryStoryStrip` (room SHOTEDITOR) aveva la stessa cache chiavata per colonna, svuotata su
  `modelReset` → un `renderXsheetFrame` + contesto GL nuovo per shot a ogni trim/riordino.
  Stessa correzione: chiave = nome sotto-scena, clear su uscita da sotto-scena + `sceneSwitched`.
  Commit `68098976e`. Da verificare visivamente.

### Note
- **Falsa pista scagionata**: l'hang era stato attribuito a un incolla di thumb in uno shot
  nuovo; era solo un tentativo di workaround di Franco. Lezione: il primo stack ("main thread
  in `nextEventMatchingMask`") era stato campionato a processo non bloccato e portava fuori
  strada — il `sample` sul processo davvero bloccato ha dato la risposta in un colpo.
- **Tecnica**: lanciare il bundle del workspace da terminale con stderr su file e leggere i
  `qWarning` `[ZTORY]` già presenti nel codice — dicono chi ricostruisce e perché, senza
  aggiungere diagnostica. (NON `/Applications/Ztoryc.app`: è una copia vecchia.)
- `ZtoryModel::removeShot()` risulta **codice morto** (nessun chiamante: la cancellazione passa
  da `StoryboardPanel::onDeleteShot`). Non rimosso.

## [2026-07-07] — SUPERPLASTIC: adapter Plastic Tool con pin/foot-planting (re-rooting + vincolo per-frame)

Sessione lunga e iterativa sul branch `feature/superplastic` (build separata Ztoryc-SP,
master NON toccato). Costruito l'adapter del solver CCD sul Plastic Tool fino al
foot-planting reale, raffinato in più giri sul feedback diretto di Franco.

### Added — branch feature/superplastic (NON su master)
- **Adapter Plastic → SolveIK_CCD** (commit d528b8ad2): checkbox "Inverse Kinematics" nelle
  opzioni Animate (default off). Attiva, il drag di un vertice risolve la catena
  radice→vertice col solver CCD condiviso, riscrivendo solo gli ANGLE (distanze intatte →
  proporzioni preservate). Gli Angle Bounds per-vertice diventano veri limiti IK.
- **Pin IK keyframabile** (2016a2eb5): nuovo param `PIN` nel SkVD (step/Constant,
  retrocompatibile via serializzazione tag-based), bottone "Pin" checkable, diamante ciano
  sul vertice pinnato. Escluso da Set Key/Rest/isFullKeyframe.
- **Il pin diventa la radice della gerarchia** (ecd6746a0): re-rooting vero — il vertice
  pinnato è la base fissa, la maniglia raggiunge il mouse, ogni altro vertice (radice
  originale inclusa) segue liberamente. Modello chiarito da Franco ("altrimenti non è
  utilizzabile").
- **Pin = vincolo per-frame** (c7bcf22c5): risolto il drift sugli intercalati (IK-vs-
  interpolazione-FK). Il pin ora memorizza una posizione TARGET (`PINTX/PINTY`) e a OGNI
  frame lo scheletro è traslato rigidamente così il pin cade esatto sul target — piantato
  su tutti gli intercalati, non solo alle chiavi. Applicato sia in vista tool sia in
  deformazione mesh (render coerente).

### Notes / percorso
- Scartata dopo verifica l'idea di pilotare il transform di COLONNA per il free-root: nel
  Plastic tutto è in spazio locale e la colonna sposta tutto in blocco (piede+corpo) → non
  può piantare il piede mentre il corpo trasla. La libertà mancante è interna alla
  deformazione (prima offset di radice ROOTX/ROOTY, poi evoluto in PINTX/PINTY + shift a
  eval-time).
- Gotcha risolto: build_and_deploy fa `open Ztoryc.app` → istanza stale tiene il QLockFile
  single-instance dopo il rename → Ztoryc-SP "si chiude da sola" (NON un crash). Ricetta
  rename aggiornata (pkill + rm lock prima del rename, cp del plist completo generato).

### Limiti noti aperti (prossima sessione)
- Limiti angolari non applicati in modalità pin (re-root). Possibile ribaltamento del verso
  di piega senza pole vector (Franco: "un po' difficoltoso da controllare").
- Adapter Skeleton Tool ancora da fare (il pin drift stock è in `computeIkRootOffset`).

### Extra
- Trapiantati i set di pennelli MyPaint `aotz` e `slos_mpb` da OpenToonz a Ztoryc/Tahoma2D.

## [2026-07-06c] — SUPERPLASTIC avviato: branch separato + core solver IK CCD (task 58)

Avviato il filone SUPERPLASTIC (spec: `Drive/Ztoryc/SUPERPLASTIC.md` — solver IK
condiviso Skeleton/Plastic + task 59-62 collegate) su **branch separato con build
separata**: master NON viene toccato da questo lavoro.

### Added — branch feature/superplastic (commit 3f15802fa, NON su master)
- **Setup**: worktree git `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d-superplastic`
  (branch `feature/superplastic`), build Ninja separata configurata nella root del
  worktree (stessi flag della release). `thirdparty/` rsyncato dal workspace
  principale (le lib compilate, es. libtiff.a, non sono in git). Build:
  `ZTORYC_WORKSPACE=.../tahoma2d-superplastic ./build_and_deploy.sh`.
- **Core solver `SolveIK_CCD`** (`include/toonz/ikccd.h` + `toonzlib/ikccd.cpp`):
  CCD in spazio angolare relativo (limiti per-bone = clamp diretto, il motivo della
  scelta CCD vs FABRIK), pole vector con mirror sulla linea root→target, nudge
  progressivo della root per uscire dalle pose singolari collineari (CCD stalla se
  catena e target sono allineati), `IKSolveInfo` con ambiguità-senza-pole segnalata
  al chiamante (non risolta in silenzio). Tipi adattati a `TPointD` (nessun Point2D
  nel codebase). Test standalone 5/5 verdi: target raggiungibile, zero drift su
  solve ripetuti, clamp limiti, scelta lato con pole, target irraggiungibile.

### Notes — findings esplorazione (decisivi per i prossimi step)
- Esiste già un solver IK in toonzlib: **`IKEngine`** (Jacobiano DLS, `ikengine.cpp`),
  usato SOLO dal drag dello Skeleton Tool (`IKTool` in skeletonsubtools.cpp). Plastic
  non lo usa: fa FK ricorsivo (`updateBranchPositions`,
  plasticskeletondeformation.cpp:492). Scelto comunque il nuovo CCD come da spec
  (pulito, deterministico, limiti+pole nativi); IKEngine resta come riferimento.
- **Il pin per-frame NON è in IKTool** ma in `TStageObject::computeIkRootOffset()`
  (tstageobject.cpp:1429): tiene fermo il piede TRASLANDO la radice (compensazione
  con l'inversa del placement FK del piede), gli angoli tra keyframe restano
  interpolazione FK. Il drift nasce lì (catena changeFootAff tra pin successivi +
  caching lazyData/m_ikflag). Quindi il fix drift = modifica alla valutazione
  per-frame del placement, da progettare a mente fresca — non uno swap del solver
  nel drag. Il drift nel drag su nuovo pin è invece in `IKTool::computeIHateIK()`
  (skeletonsubtools.cpp:925).
- Prossimi step: (1) design del re-solve per-frame in/accanto a computeIkRootOffset,
  (2) adapter drag IKTool→SolveIK_CCD con parità senza pin, (3) adapter Plastic.

## [2026-07-06b] — Fix export (target OT/Tahoma/Ztoryc, audio trimmato) + timing transizioni + Board — release 0.8.1

Sessione dedicata ai tre problemi grossi segnalati da Franco: export (normale e a
progetto esterno), timing con transizioni, refresh durate parziali del Board.
Tutto verificato da Franco su casi reali. Rilasciata come **v0.8.1** (solo fix).

### Fixed — Export
- **Target sdoppiati Ztoryc / Tahoma2D / OpenToonz** (dialog Board + Export Scene
  nativo). Tahoma2D: strip dei marker In/Out per-xsheet (feature solo-Ztoryc,
  Tahoma stock rigettava la scena) + copia del project file come
  **`tahomaproject.xml`** (il rename a ztorycproject.xml aveva rotto il
  riconoscimento del progetto in Tahoma stock). OT invariato (explicit holds +
  `_otprj.xml`). Enum `ExportTarget`, `stripSceneInOutMarkers`,
  `writeTahomaProjectFile` in exportscenepopup.
- **Audio export riprogettato: WAV fisicamente trimmato per shot.** La vecchia
  iniezione clonava i ColumnLevel (file intero + trim in startFrame negativi e
  offset): OT non decodifica mp3 → colonna vuota; il trim a offset era fragile
  anche in Tahoma/Ztoryc. Ora `injectAudioForShot` renderizza la fetta netta
  dello shot (`getOverallSoundTrack`) in `+extras/<shot>_audio[N].wav` e
  inietta una colonna semplice senza offset. Con XD-in il piazzamento parte da
  `headHalf` (allineato al contenuto reale, righe head mute). I wav viaggiano
  col meccanismo capture/copy dei path `+`.
- **Ordine compat/copie**: il pass di compatibilità target (explicit holds /
  strip marker) faceva load+save PRIMA delle copie +extras/audio → sound level
  caricato vuoto e perso. Ora `applyTargetCompatibility()` separato, eseguito
  dal Board DOPO le copie asset.
- **Export normale allineato ai settaggi progetto**: il capture/copy dei livelli
  '+' (post-save, non invasivo) ora gira anche nel plain Export Shots — prima
  con layout "assets next to scene" gli asset non venivano trovati.
- **Logging diagnostico**: `ztoryc_export_log.txt` accanto alle scene esportate
  (capture / post-save / project-copy / audio-trim / compat).

### Fixed — Timing con transizioni (semantica NETTA unificata)
- `onShotDurationChanged`: teardown dissolvenze PRIMA di misurare (il drag del
  track emette durate nette, il confronto con getRange lorda faceva
  crescere/ridurre lo shot dei frame extra). Range In/Out ricalcolato per
  coprire head/tail extra (convenzione onTransitionChanged).
- `onMatchSubsceneDuration`: misura netta (skip colonne sound/note XD;
  net = contenuto − headHalf − tailHalf); no-op guard su shotTrueSpan (prima
  netto-vs-lordo → resize storm con auto-match).
- **Markout dentro lo shot**: il pin del play range in
  `ZtoryModel::resequenceXsheet` ora gira SOLO a livello main — modificando la
  transizione da dentro lo shot il markout saltava alla fine della timeline main.

### Fixed — Board
- **Primo panel = durata totale**: `onModelResequenced` sovrascriveva a ogni
  resequence la durata parziale del panel 0 con la durata totale (anche per
  shot multi-panel). Ora T: per tutti, D: solo single-panel, durate nette.
- `detectAndUpdatePanels` + `onXsheetChanged`: timelineDuration netta via
  shotTrueSpan; filtro panel head-aware (`f < net + headHalf`); primo panel
  assorbe le head-hold. Detection di tutti gli shot a fine `refreshFromScene`
  (prima girava solo da dentro lo shot).

### Release
- v0.8.1 (patch, solo fix): include anche i fix Ztoryc Monitor del 07-05d
  (layout tracce + Set Key nascosto) e i fix export/crash del 07-05/06.
## [2026-07-06] — Export shot→progetto: molti fix + WIP; rebrand project file/splash

Sessione maratona sull'export "Shots to New Project" (Tahoma e OpenToonz) + rifiniture
di branding. Diversi fix solidi; il transfer degli asset (+extras/audio) verso un NUOVO
progetto resta FRAGILE — dettagli e root cause in memoria `project_export_project_assets_wip`.
Post-0.8.0, per la prossima release.

### Fixed
- **Cartelle nidificate** nell'export a progetto: importScene salvava al path di staging
  (`+scenes/ztoryc_export_tmp/scenes/<shot>`) ricreando la nidificazione nel target. Ora
  salva piatto in `+scenes/<basename>.tnz` e ritorna il path assoluto.
- **Nome scena malformato** (`_01___sh010_V01`): l'export usava il pattern di produzione
  con token vuoti. Ora convenzione shot: `SEQ_SHOT` (o `SHOT` senza sequenza).
- **`inOutMarkers` unknown tag in OpenToonz**: convertSceneToExplicitHolds azzera i marker
  In/Out su main + sub-xsheet prima del save → OT non rigetta più la scena.
- **Timing audio con cross-dissolve**: l'audio usava getRange() (includeva l'overlap della
  dissolvenza → audio troppo lungo). Ora usa `ZtoryShotOps::shotTrueSpan()` (durata vera).
- **Crash file-browser Windows / crash export** già in 07-05c/precedenti.

### Changed
- **Project file `tahomaproject.xml` → `ztorycproject.xml`** (retrocompat: riconosce anche
  il legacy). Aggiornati getProjectFile/searchProjectPath/getLatestVersionProjectPath/
  isAProjectPath (tproject.cpp) + sandbox path in startuppopup.cpp.
- **Splash tagline**: "STORYBOARD · ANIMATIC · ANIMATION" → "OPEN STORYBOARD TOOL".
- Radio New/Existing e Target OT/Tahoma nell'export dialog in QButtonGroup separati.

### WIP / aperti (prossima sessione)
- Transfer +extras/audio verso NUOVO progetto: fragile/intermittente (i livelli
  project-folder non sono copiati dalla macchina di export; il self-containment indovina
  la risoluzione a tempo-apertura e la manca). Audio in OT non caricato/trimmato.
  → serve logging diagnostico su un export reale, non patch al buio.
- Immagine About (`ztoryc_about.png`): testo "storyboard animatic animation" disegnato nel
  PNG → va rigenerata.

## [2026-07-05d] — Fix Ztoryc Monitor (post-0.8.0, per la prossima release)

Due bug del Ztoryc Monitor segnalati da Franco dopo il rilascio della 0.8.0.
Committati su master (28172e59a), NON in 0.8.0 → prossima release.

### Fixed
- **Layout tracce del Monitor**: gap spurio tra traccia video e audio + traccia
  video "invisibile" all'apertura. Causa: in `ztorymonitorpanel.cpp` la
  `m_timelineLay` (QVBoxLayout della timeline, dentro uno splitter più alto delle
  righe fixed-height) **non aveva un `addStretch` finale** → lo spazio verticale
  in eccesso veniva distribuito TRA i widget. Fix: `addStretch(1)` dopo il track +
  inserimento delle tracce audio prima dello stretch (stesso pattern del pannello
  Animatic).
- **Set Key nascosto nei viewer always-main**: il rombo Set Key (keyframe navigator
  = custom widget del FlipConsole) era visibile nel viewer animatic e nel Monitor.
  Poiché questi viewer lavorano sul MAIN xsheet — dove mettere chiavi su una colonna
  shot non è permesso — il navigator ora è nascosto nel costruttore di
  `ZtoryAnimaticViewer` e ri-forzato a ogni `showEvent` (via
  `setCustomizemask(~eShowCustom)`, instance-local → il viewer normale resta intatto).
  Copre viewer standalone + Monitor. Follow-up possibile: bloccare anche lo shortcut.

## [2026-07-05c] — Kitsu: dual URL + asset bidirezionali + sync spostato nella tab Project

Sessione dedicata a completare l'integrazione Kitsu (M5) e a mettere ordine nella UI
di sync. Route asset Zou inchiodate dal sorgente live. Rilasciata come **v0.8.0**
(bundle con il lavoro dissolvenze del 07-05b, bumpato a 0.8.0 ma non ancora rilasciato).

### Added
- **Kitsu dual URL local/remote** — oltre all'URL primario (login + tutto il sync
  leggero) c'è un **Local URL** opzionale: stessa istanza Kitsu via LAN, usato SOLO per
  l'upload dei preview (salta il limite body del proxy remoto, es. i 100MB di Cloudflare).
  `uplProbeLocalThenRun()` fa un probe con `setTransferTimeout(4000)` prima di partire:
  se il LAN endpoint non risponde → fallback automatico al primario. Il JWT è
  instance-wide, vale su entrambi gli URL. Zero impatto su push/pull/login (param `base`
  opzionale sugli auth builder). Campo "Local URL:" nel Connect dialog.
- **Kitsu asset bidirezionali** — push/pull delle entità asset Ztoryc↔Kitsu.
  `pushAssets()` (upsert per type+name: crea i mancanti, non duplica), `pullAssets()`
  (importa nel tracker gli asset creati in Kitsu, match per id poi type+name),
  `kitsuAssetId` su `Asset` (link rename-proof, persistito nel .ztrack). Tassonomia
  **allineata a Kitsu**: Character/Prop/FX/Environment (`ZtoryModel::canonicalAssetTypes()`,
  sorgente unica UI + sync); legacy "BG" → "Environment" in load; "Audio" resta solo
  cartella asset, non asset-type. Route Zou verificate dal sorgente live:
  `GET /api/data/asset-types`, `GET /api/data/projects/<pid>/assets`,
  `POST /api/data/projects/<pid>/asset-types/<atid>/assets/new`. **Testato end-to-end.**

### Changed
- **FCPXML export: rebrand label DaVinci → NLE generico** — checkbox "Also export editing
  timeline", titolo "Export editing timeline", body "your NLE (DaVinci Resolve, Premiere
  Pro or Final Cut Pro)". DaVinci ora è solo uno dei tre target.
- **Comandi sync spostati dal Connect popup alla tab Project** — il Connect popup ora
  contiene SOLO connessione (URL/Local URL/email/password) + binding progetto (Link/Create).
  Tutti i push/pull (shots/statuses/asset) e l'upload preview vivono nella tab Project,
  dove si lavora. Eliminata anche una duplicazione latente (popup e tab ascoltavano gli
  stessi signal del singleton). Aggiunta la riga Push/Pull assets alla tab.
- **Create Project popup: nascosto il checkbox "Separate assets into scene sub-folders"** —
  il combo "Asset organization" lo supersede (il suo preset "Scene sub-folders" è la stessa
  identica impostazione). Evita il footgun del doppio-nesting per nome-scena quando era
  attivo insieme a "Assets folder next to each scene". Il checkbox resta nel Project
  Settings popup (che non ha il combo).

### Upstream candidates
- Nessuno nuovo.

### Notes
- Restano per Kitsu: sync dei **task-status degli asset** (concept/rough/clean/color) —
  oggi è bidirezionale solo l'ENTITÀ asset, non i suoi status. Route
  `/actions/projects/<pid>/task-types/<ttid>/assets/create-tasks` già verificata.
- La spec **struttura cartelle PRODUCTION/** (wizard Nuova produzione, ASSETS/PROJECT/OUTPUT)
  resta da implementare: è ciò che darebbe una "casa" ai file degli asset (oggi il sync
  scambia solo i metadati, non l'arte; `Asset` non ha ancora un campo path).

## [2026-07-05b] — Dissolvenze reali nell'animatic (render-time) + undo transizioni

Completato il filone "dissolve reale Parte 2": le cross-dissolve ora si vedono
DAVVERO nel play della timeline animatic E nell'export come filmato, non piu' solo
come marker (triangolo). Diagnosi guidata da crash log + ragionamento sul render tree.

### Added
- **Cross-dissolve reali renderizzate** nel viewer animatic e nell'export MOV. Il
  blend e' iniettato al RENDER (`scenefx.cpp`, `FxBuilder::makePF(TXsheetFx)`) come
  `blendFx` nativo transiente costruito PER-FRAME, MAI persistito nel fx dag. Gated
  sulle colonne note XD-in/XD-out -> zero impatto sul rendering Tahoma normale. Un solo
  meccanismo copre viewer + export. **Chaining A->B->C nativo** (per-frame e' attiva al
  massimo una dissolvenza). Interpolazione **ease-in-out** (smoothstep).
- **Undo/redo sulle transizioni** (`UndoTransition`, approccio replay QPointer-safe):
  in cronologia come "Change Cross-Dissolve".

### Fixed
- **Interpolazione a scatti (0->100 invece di rampa)**: root cause = il costruttore di
  `TDoubleKeyframe` NON inizializza `m_prevType` (garbage); `setKeyframe()` lo copia nel
  `m_type` del segmento (il tipo d'interpolazione letto da `getValue`) -> rampa resa come
  Constant step. Fix lato chiamante: `m_prevType` esplicito + `m_isKeyframe=true`.
- **Crescita cumulativa della durata shot** modificando/rimuovendo la dix (osservato:
  uno shot da 24 frame diventava 40): il teardown delle celle di overlap ora gira
  all'INIZIO di `onTransitionChanged`, quando le colonne XD tengono ancora il valore
  VECCHIO (coerente con l'overlap esposto) -> strip completo. Prima il teardown leggeva
  il valore NUOVO e strippava il conteggio sbagliato, lasciando celle stale.
- **Crash su undo e su cambio-workflow** (2D tradigital <-> storyboard) dopo una dix:
  eliminata la causa alla radice passando da fx persistiti nel dag a render-time
  injection -> non esiste piu' nessun oggetto fx da danglare in undo/selezione/room-load.

### Modified / Architettura
- `ZtoryShotOps::applyCrossDissolves` ora espone SOLO le celle di overlap (colA tail-extra
  continue senza buco + stop, colB head-extra); `teardownCrossDissolves`/`shotTrueSpan`
  riscritti **fx-free**, basati sulle colonne XD (geometrici).
- `ZtorycVersion.cmake`: bump 0.7.0 -> 0.8.0.

### Upstream candidates
- Nessuno nuovo. Il bug `m_prevType` non inizializzato e' in core Tahoma
  (`tdoublekeyframe.cpp`) ma il nostro fix e' lato chiamante; segnalabile come hardening
  del costruttore se si vuole contribuire.

### Note
- Restano da fare (prossima sessione): Kitsu asset bidirezionali + dual URL local/remote;
  rebrand label export FCPXML da "DaVinci" a generico (Resolve/FCP/Premiere).

## [2026-07-05] — Fix bug durata transizione (cross-dissolve) + ricerca dissolve reale

Sessione focalizzata sulle transizioni dell'animatic. Diagnosi guidata da tracing lldb
dal vivo (mai patch alla cieca), come da workflow crash-diagnosi.

### Fixed
- **Durata cross-dissolve: shot B teneva il primo frame dopo una riduzione della
  dissolvenza.** Repro (trace lldb): dissolvenza 0→12→22, poi ridotta a 12 → lo shot B
  ripartiva tenendo il frame 1 invece di riprendere dal frame corretto. Root cause: NON
  in `onTransitionChanged` (il suo shift head/tail è simmetrico e corretto), ma in
  `ZtoryModel::resequenceXsheet()` che ricostruiva ogni colonna shot con frameId `1,2,3…`
  puliti, **cancellando l'offset dell'head-hold** applicato per allineare lo shot B.
  Siccome resequence gira dopo qualsiasi cambio (e su molte interazioni UI), tra due edit
  l'offset veniva azzerato mentre l'head-hold nella sub-scena e la colonna XD-in
  sopravvivevano → desync → B teneva il frame 1. Fix: nuovo helper
  `ZtoryShotOps::xdInHeadOffset()` che legge la colonna XD-in (fonte di verità persistente);
  resequence ora espone il sub-scene frame `r+1+headOffset`, rendendo l'offset self-healing
  attraverso resequence / reload / undo. (ztorymodel.cpp, ztoryshotops.cpp/.h) Verificato
  dal vivo da Franco.

### In corso (non committato — prosegue nella prossima sessione)
- **Dissolve reale nel viewer animatic** (overlap sotto-scene + Cross Dissolve nativo
  `blendFx`): prima implementazione fatta e il blend RENDERIZZA una dissolvenza vera, ma
  con 3 problemi da risolvere (crash al workflow-switch per registrazione fx incompleta;
  interpolazione keyframe; e un problema strutturale: l'esposizione degli extra frame sulle
  colonne shot corrompe `getRange()` → triangolo marker sparito + crescita cumulativa
  durate). WIP revertito per tenere master sano; diagnosi completa + fix precisi salvati in
  memoria per riprendere a caldo. Decisione Franco: timeline invariata, gli extra frame
  dentro gli shot forniscono l'overlap.

### Upstream candidates
- Nessuno nuovo (il fix `xdInHeadOffset` è logica Ztoryc-specifica, non portabile upstream).

## [2026-07-04b] — Fix thumbnail (Board/Animatic/Thumbnails room), transizioni FCPXML per DaVinci, GitHub Sponsors

Seconda parte della sessione del 2026-07-04 (continua dopo l'export progetto esterno,
vedi entry precedente). Segnalazioni utente su thumbnail rotte + richiesta transizioni
DaVinci + compilazione profilo GitHub Sponsors.

### Fixed — thumbnail Board / Animatic / export
- **Bande grigie al cambio formato camera + thumbs deformati alla riapertura scena**:
  tutti i render di anteprima (Board panel preview, animatic track, export PDF/XLSX)
  dimensionavano il raster sulla camera del MAIN xsheet invece che su quella della
  **sub-xsheet renderizzata** — le camere sono per-xsheet, e `renderFrame` adatta la
  camera dell'xsheet passato al raster, lasciando bande del colore di sfondo se le due
  divergono. Nuovo `ZtoryShotOps::xsheetCameraRes/Aspect(TXsheet*)`, usato ovunque.
  (storyboardpanel.cpp, ztoryanimatic.cpp, ztoryshotops.cpp/.h)
- **Cache thumbnail dell'animatic track stale al cambio scena**: si puliva solo su
  `modelReset`, non su `sceneSwitched` — una scena il cui refresh non passava per
  quella via mostrava le thumb della scena precedente sulle colonne coincidenti.
- **Blocchi corti in timeline**: se il blocco era più stretto della thumb, la larghezza
  veniva schiacciata invece di rimpicciolire l'immagine uniformemente.
- **"Col 5" invece del numero shot**: se un blocco veniva costruito tra l'inserimento
  dello shot e il resequence del modello, appariva il nome colonna di default di Tahoma.
  Ora c'è un fallback posizionale con regex `^Col\d+$`.

### Fixed — Thumbnails room (cambio formato camera)
- **Riscrittura completa del reflow** (`reanchorRaster`, condiviso da `onSceneChanged`
  live e `persistLoad` in ricarica). Tre bug distinti segnalati dall'utente in un giro
  di test iterativo:
  1. Il vecchio reflow lavorava **per box**: una panoramica unita (merge) veniva tagliata
     al confine box e perdeva contenuto → ora lavora **per regione** (una merge = una
     regione unica).
  2. **Griglia fantasma accumulata** sui cambi ripetuti di formato: i margini bianchi di
     ogni box venivano ri-scalati insieme al contenuto, accumulando seam del resample →
     ora si estrae **solo il bounding box dell'inchiostro** (soglia 248/255) su bianco
     fresco ad ogni passaggio.
  3. **Rimpicciolimento cumulativo** (square→HD→ultrawide→square non tornava alla
     dimensione originale): ora la scala mantiene la frazione di altezza rispetto al
     frame (`newBoxH/oldBoxH`, sale E scende), riducendo ulteriormente solo se
     l'inchiostro scalato sborda la larghezza fissa del box — su richiesta esplicita
     di Franco.
  4. **Bordino scuro attorno al disegno** dopo il fix #2: `do_resample` riempie di nero
     i campioni fuori raster; ora l'inchiostro viene imbottito con un margine bianco di
     6px prima di scalare + pulizia di un frame di 2px sul risultato.
  5. **Undo del cambio formato schiacciava i disegni**: lo snapshot undo non salvava
     l'aspect ratio con cui era stato disposto il raster. Ora `Snapshot` include
     `boxAspect`; il reflow stesso è annullabile (`pushUndo()` prima del reshape).
  - **Griglia guida** ridisegnata in blu classico da animazione `#1D5C83` (tenue, bassa
    opacità) invece di grigio — una linea grigia residua è quindi inequivocabilmente un
    artefatto, non la guida. (ztorythumbnailcanvas.cpp/.h)
- Fix upstream candidato aggiunto in questa sessione: `convertToExplicitHolds`
  convertiva le sub-xsheet a IMPLICIT holds (copy-paste bug), vedi AGENTS.md.

### Added — Export animatic → DaVinci (transizioni)
- Le transizioni dell'animatic (colonne note XD-out/XD-in) sono ora scritte
  nell'FCPXML come elementi `<transition>` Cross Dissolve: i clip adiacenti si
  sovrappongono della durata della dissolvenza e gli offset audio vengono
  compensati per restare sincronizzati. **Verificato funzionante in DaVinci
  Resolve** (import reale delle dissolvenze). Bug corretto nello stesso giro:
  la durata veniva letta dal mirror `.ztoryc` (spesso stale/0, transizioni
  scritte a zero) invece che dalla colonna XD-out della sub-scena, fonte di
  verità reale. (storyboardpanel.cpp)
- **Hint scopribile**: il suggerimento per creare una transizione appariva solo
  tenendo già premuto Alt/Option (quindi introvabile). Ora l'hint sul bordo
  destro di uno shot con un successivo include "hold ⌥ Option/Alt and drag to
  add a cross-dissolve" (label per piattaforma). (ztoryanimatic.cpp)

### Chore
- Rimossi dal tracking pubblico 5 file di bozze private (sponsor pricing, manuale,
  corso) finiti nel repo per un `git add -A` incauto — ora in `.gitignore`, restano
  solo in locale. Nessuna riscrittura della history su richiesta di Franco (il
  contenuto non era abbastanza sensibile da giustificare un force-push).
- Fix minore: checkbox "Update Production Tracker" nell'export progetto esterno —
  il file `.ztoryc` di collegamento era legato a una regola automatica sul target
  OT/Tahoma; ora è governato solo dalla checkbox (un export mirato a OpenToonz può
  comunque essere riaperto in Ztoryc solo per aggiornare gli stati).

### Non fatto — Profilo GitHub Sponsors (compilato manualmente, non da codice)
- Bio, introduction, goal ($50/month), 7 tier (5 mensili + 2 one-time) compilati
  nella dashboard GitHub Sponsors con testi bilingui EN/IT — in attesa di
  approvazione GitHub. Sorgente di verità: `SPONSORS_DRAFT.md` (privato, gitignored).
  Posizionamento aggiornato: "any production — 2D, stop motion, 3D or live-action"
  (non solo 2D). Quando arriva l'approvazione: finalizzare blocco README +
  `SUPPORTERS.md` da `SPONSORS_DRAFT.md` sezioni C/E.

### Aperti / rimandati a prossima sessione
- **Bug transizione**: cambiando la durata di una dissolvenza (es. 12→24 frame) lo
  shot A non mostra gli ultimi 12 frame e lo shot B parte da 6 invece di 12 —
  desync tra i due rami (coda A / testa B) di `onTransitionChanged`
  (ztoryanimatic.cpp ~7252). Da riprodurre e tracciare passo-passo prima di
  qualsiasi fix.
- **Cross-dissolve reale nel viewer animatic** (non solo marker + export FCPXML):
  approccio indicato da Franco — sovrapporre le colonne sub-scene sul main xsheet
  durante la transizione e applicare l'effetto Cross Dissolve **nativo** di Tahoma
  (il calcolo coda/testa esiste già via XD-out/XD-in). Va fatto DOPO il fix del bug
  sopra. Vedi memoria project_animatic_real_dissolve_part2.

---

## [2026-07-04] — Export shot → progetto esterno OpenToonz/Tahoma (dialog unico)

Implementata la feature "Export progetto esterno" (memoria project_export_external_project_ot):
gli shot del Board diventano un progetto autonomo con asset copiati, compatibile
OpenToonz o Tahoma. Tre iterazioni con test di Franco nella stessa sessione.

### Added
- **File ▸ Export ▸ Ztoryc ▸ Shots to New Project...** (`MI_ZtoryExportShotsToProject`):
  dialog unico con: destinazione **New project** (nome/posizione) o **Existing project**
  (albero progetti — utile per raccogliere shot di più storyboard nello stesso progetto);
  **Target application** Tahoma/OpenToonz; combo **Asset organization**; sezione asset
  folders custom (ripiegata di default, prefilled dal progetto corrente); versione;
  shots **All/Selected/Range**; checkbox Production Tracker con dicitura chiara
  ("Update Production Tracker (writes a .ztoryc file next to each exported scene)" +
  tooltip sugli avanzamenti di stato). (storyboardpanel.cpp/.h, menubar.cpp,
  mainwindow.cpp, menubarcommandids.h)
- **Motore riusabile in ExportScenePopup**: statiche `createProjectFromSpec` (NewProjectSpec:
  nome, posizione, folder custom, useSubScenePath, targetOpenToonz) e
  `exportScenesToProject` (import + collectAssets + passata OT). Il popup MI_ExportScenes
  guadagna anche lui folder fields custom + toggle target. (exportscenepopup.h/.cpp)
- **Compatibilità OpenToonz**: conversione a explicit holds di tutte le scene esportate
  (ricorsiva nelle sub-xsheet) + copia del project file come `<nome>_otprj.xml`
  (coesiste con tahomaproject.xml: la cartella è valida per entrambe le app).
  Il companion `.ztoryc` è governato **solo** dalla checkbox tracker, indipendentemente
  dal target: un export mirato a OpenToonz può comunque essere riaperto in Ztoryc in
  seguito solo per aggiornare lo stato produzione, anche se disegno/animazione
  avvengono in OT (prima versione legava la scelta al target, rimosso dopo feedback
  di Franco — spunto "tool standalone per tracker cross-app" salvato per il futuro).
- **Combo "Asset organization"** (nell'export E in File ▸ New Project nativo):
  1) Project folders (default) · 2) Scene sub-folders (= flag nativo useSubScenePath)
  · 3) **Assets next to each scene** = `scenes/$scenepath/{drawings,extras,inputs,outputs,stopmotion}`
  (layout storico di Franco, comodo per spostare scene tra computer). Palettes e scripts
  restano a livello progetto (palette color design e script sono condivisi tra scene).
  (projectpopup.cpp/.h)
- Refactor: core di Export Shots estratto in `exportShotScenesToDir(indices, dir, ver,
  writeLink, fail)` — riusato da entrambi i flussi, supporta indici sparsi (Selected).

### Fixed
- **convertToExplicitHolds convertiva le child xsheet a IMPLICIT** (txsheet.cpp ~2732,
  copy-paste bug dalla funzione inversa): convertendo a explicit le sub-scene perdevano
  il timing. Ora ricorre con `convertToExplicitHolds(0)`. **Candidato PR upstream**
  (aggiunto alla lista in AGENTS.md).
- **Staging autopulente senza prompt "already exists"**: le copie asset del salvataggio
  sub-scene (takeCareSceneFolderItemsOnSaveSceneAs, scene subfolders) finivano fuori
  dalla cartella temporanea (`scenes/extras/<nome>/`) → dal secondo export appariva il
  prompt di overwrite. Ora lo staging è `+scenes/ztoryc_export_tmp/scenes/` così le
  copie cadono DENTRO ztoryc_export_tmp → un solo rmDirTree (fatto anche preventivamente).

### Notes
- Edge case noto: se importScene rinomina una scena per collisione nel progetto target,
  il companion .ztoryc non viene abbinato (raro).
- Residui del primo test in `demoztoryc/scenes/extras/matitanimata__*` eliminabili a mano.
- Il salvataggio del progetto forza sempre il nome `tahomaproject.xml`
  (TProject::save → getLatestVersionProjectPath): per OT si copia il file, non si rinomina.

---

## [2026-06-28b] — Production Tracker standalone: rifiniture + fix contaminazione → RELEASE v0.7.0

Sessione di rifinitura della room Production standalone. Branch
**feature/kitsu-m5-phase3b** mergiato in **master** e rilasciato come **v0.7.0**
(prima release con l'intera integrazione Kitsu M5 + export DaVinci/FCPXML +
Production Tracker di progetto).

### Fixed — Production Tracker standalone
- **Cambio status non sporca piu' la scena**: editare uno status/assignee/tecnica nel
  tracker non chiede piu' di salvare una scena. La registrazione undo marcava la scena
  untitled come "modificata"; ora la pulizia del dirty-flag e' centralizzata nelle
  funzioni di persist (persistViaBoard/persistProjectDb/persistAssets), quindi copre
  anche il context-menu batch e undo/redo. (ztoryproductionpanel.cpp)
- **Contaminazione cross-progetto degli shot** (in memoria E su disco): aprendo il
  tracker di un progetto senza production.ztrack, il modello veniva salvato con ancora
  dentro gli shot/metadati del progetto precedente -> finivano nel .ztrack dell'altro
  progetto. Ora loadProjectDb() resetta i dati di progetto prima di creare il DB vuoto,
  e loadProjectDbFromDevice() resetta prima di ripopolare dal file. (ztorymodel.cpp)
- **Tile "Production Tracker" non ricaricava il progetto corrente**: rientrando nel tile
  (es. dopo aver cambiato progetto) la room non si ricostruiva e il tracker restava sul
  progetto precedente. Ora forza loadProjectDb() + productionReloaded. (startuppopup.cpp)

### Added — Production Tracker standalone
- **Uscita dalla room**: pulsante "<- Open or Create Scene..." in cima al tracker (solo
  nella room Production standalone) che riapre la Startup page per caricare/creare una scena.
- **Chrome pulita**: nella room Production la menubar nativa (File/Edit/...), i tab room e la
  main toolbar sono nascosti - resta solo il tracker + il pulsante. Applicato anche al
  riavvio dell'app (helper applyRoomChrome chiamato da room-change e startup). (mainwindow.cpp)

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-28] — Fix export FCPXML, automazione status, sync multi-board, Kitsu opt-in, room Production (branch feature/kitsu-m5-phase3b)

Sessione lunghissima. Tutto su **feature/kitsu-m5-phase3b** (NON mergiato in master).

### Fixed — export / FCPXML
- Clip per-shot che contenevano tutti gli shot: render serializzati (segnale
  `ZtoryModel::renderFinished` + QEventLoop) — i render partivano concorrenti e si
  contaminavano. (`ee6a10fbd`)
- Full animatic più corto del montaggio: `shotFrameRange` escludeva la cella STOP_FRAME (SFH).
- Audio FCPXML: in-point sorgente (`getStartOffset`) e durata visibile, prima ignorati.

### Added — Kitsu (M5)
- Upload preview mp4 (`uploadPreviews`): comment WFA → add-preview → POST multipart;
  crea il task se manca; `set-main-preview` = thumbnail shot (primo frame). (`c1ac2d6ed`)
- Automazione status: export shot→DONE/Layout READY; apertura .tnz→primo task non-Done WIP;
  upload→WFA; pull DONE→successivo READY. Helper `firstProductionTaskType`/`nextTaskType`.
- Controlli sync (Push/Pull/Upload/handles) spostati nel tab Project del tracker (sempre
  disponibili); rimossa la tabella status mapping (Ztoryc↔Kitsu già 1:1). Helper condivisi
  `buildUploadsFromFolder`/`buildShotPushFromProject`. (`27fa7fc54`)
- Checkbox "Upload to Kitsu" nell'export (post-export auto-upload).
- **Opt-in**: checkbox "Use Kitsu" nella creazione progetto (flag `useKitsu` nel .ztrack);
  con off tutta la UI Kitsu è nascosta. `resetProjectLevelDefaults` azzera anche il binding
  Kitsu (un nuovo progetto non eredita il link del precedente). (`b6985e45c`)

### Fixed — numerazione Keep + multi-board
- Keep numbering era rotta (regressione, anche in 0.6.3): `m_autoRenumber` era per-pannello →
  spostato globale in `ZtoryModel`; e le label si riderivano per indice invece che dal nome
  colonna (identità) → fix in `refreshFromScene`. (`57b01f3fb`)
- Crash `addPanelWidget` su shot senza panel (guard). (`92a2bdfb3`)
- Sync multi-board: `onModelResequenced` ricostruisce su drift di conteggio/ordine/widget;
  bottoni Add sul board flottante; reorder cross-board in Keep. (`53c97c75b`, `d64fc458f`)
  RESTA: reorder cross-board in modalità AUTO (deferred). Vedi project_multiboard_sync.

### Added — Rooms / startup
- Room di default Storyboard: Production Tracker + Thumbs (ordine tracker, thumbs,
  ztoryc X, ztoryc T, browser). Tile PT azzurra nello startup → apre la room-set isolata
  Production (solo il tracker, non è una scena; si esce con load/create scene). (`1f5b8704b`)
- Production Tracker standalone: carica il .ztrack del progetto corrente su showEvent; gli edit
  non sporcano più la scena untitled.

### Note / RESTA
- Branch NON mergiato. Restano un paio di rifiniture (da definire con Franco) + reorder
  AUTO cross-board + transizioni vere FCPXML.

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-27c] — Kitsu M5 Fase 3b + fix contaminazione + sync bidirezionale + export DaVinci (branch feature/kitsu-m5-phase3b)

Sessione lunga. Branch **feature/kitsu-m5-phase3b** (da master dopo il merge di 1+2+3a).

### Fixed — bug critico tracker
- **Contaminazione cross-progetto** (`84cba915e`): gli shot di uno storyboard finivano nel
  `production.ztrack` di altri progetti/storyboard. Causa: `ZtoryModel::m_shots` cresceva ma non
  si troncava per scene più piccole → al publish scriveva shot residui. Fix: `setShotsFrom()`
  sincronizza il modello alla scena PRIMA di ogni publish; `clearShots()` sui rami early-return di
  `loadZtoryc`; firewall `saveProjectDb` meno aggressivo (blocca solo se sul disco ci sono davvero
  metadati) → ripristina l'aggregazione multi-storyboard. Trovato instrumentando (log su file).

### Added — Kitsu (completamento sync bidirezionale)
- **Fase 3b task+status push** (`201d73042`) + allineamento nomi task-type (`cd8693ccf`,
  Render↔Rendering/VFX↔FX, case-insensitive).
- **Frame in/out come timecode MM:SS:FR** (`065a52b0`/`e520b7e1`): colonna tracker 'Sec/Fr'→'In-Out'
  cumulativo per source; export XLSX; push manda frame_in/out cumulativi.
- **Pull status (review sync)** (`2e4e2eb5`): il supervisor mette WFA→Done/Retake su Kitsu e torna
  in Ztoryc; match per shot+task con alias.
- **Sessione persistente** (`caf3e561`): KitsuClient singleton, auto-connect con credenziali salvate,
  password Remember default ON, dropdown pre-seleziona la produzione linkata.
- **kitsuShotId per shot** (`6fc37c9e`) + **PUT-by-id nel push** (`afdb8eed`): link rename-proof.
- **Handles** (`4e91a7b4`): checkbox 'Push with handles +N fr' padda frame_in/out su Kitsu.

### Added — Export montaggio (DaVinci/NLE)
- **FCPXML** (`db617160e`/`baedde6e0`): checkbox 'Also export DaVinci timeline' (solo con 'one clip
  per shot'); clip per-shot in ordine + **audio multitraccia** (ogni colonna sonora → lane separata,
  dialoghi/musica/fx mixabili in DaVinci). Range combinabile con 'one clip per shot'.

### Notes / da fare
- **Upload preview mp4 su Kitsu** + task **Storyboard** (WIP→WFA): contratto API nailato in memoria.
- **FCPXML transizioni vere** (cross-dissolve da `transitionFrames`): da fare DOPO conferma import base.
- Branch NON ancora mergiato in master.

### Upstream candidates
- Nessuno (codice Ztoryc-specifico).

---

## [2026-06-27b] — Kitsu M5 Fase 1+2+3a (branch feature/kitsu-m5)

Avvio dell'integrazione Kitsu (CGWire/Zou). Lavoro su **branch `feature/kitsu-m5`**,
NON su master. Validato contro l'istanza locale docker (localhost:8012) interrogando
l'API reale per azzeccare i JSON shape e i contratti.

### Added — Fase 1 (login + pull)
- `kitsuclient.h/.cpp` — `KitsuClient` (QtNetwork, `Qt5::Network` già linkato): login JWT
  (`/api/auth/login`), pull progetti (`/api/data/projects/open`) e task-status
  (`/api/data/task-status`). Config in QSettings (`Ztoryc/Kitsu/*`).
- Mappa status Kitsu→`TaskStatus` via `short_name` + fallback flag (`is_done/is_retake/
  is_feedback_request`); colori già coincidenti con quelli Ztoryc.
- `kitsuconnectdialog.h/.cpp` — dialog di connessione (URL/email/password, Connect,
  dropdown progetti, tabella mapping status). Aperto da "Connect to Kitsu…" nel tab Project.

### Added — Fase 2 (binding bidirezionale role-aware)
- Campo progetto **`code`** (sigla breve, es. CS26) + nuovo token **`{CODE}`** nel naming
  (3 resolver: outputsettings + storyboard ×2). Modello: `kitsuProjectId/Name`,
  `productionType/Style`, `ratio`, `resolution`, `isKitsuLinked()`, tutto nel `.ztrack`.
- KitsuClient: legge `user.role`, `canManageProjects()` (admin/manager), `createProject()`
  (POST), `updateProject()` (PUT).
- Dialog: **Link selected** (pull → scrive il modello, bind) e **Create new in Kitsu**
  (push, role-gated). Tab Project: Production/Code read-only + label "🔗 Linked" quando legato.
- Verificato in-app: create di una produzione su Kitsu + link con pull dei parametri. ✓

### Added — Fase 3a (shot push, solo Ztoryc→Kitsu)
- `KitsuClient::pushShots()` — macchina a stati sequenziale async: ensure episode (tvshow,
  find-or-create) → sequences → shots; upsert per nome (POST nuovo / PUT esistente,
  `nb_frames` + `data{frame_in,frame_out}`). Bottone "Push shots to Kitsu →".
- Sorgente: `projectShots()` con fallback agli shot di scena; sequenza default **SQ01**
  quando assente. Verificato in-app: 3 shot creati su Kitsu. ✓

### Notes / da fare (prossima sessione)
- Mancano su Kitsu: **thumbnail** shot (upload preview) e **task + status** sugli shot.
- **Asset** bidirezionali (download/upload), route create da rifinire (non `/assets/new`).
- Contratti API e decisioni in memoria `project_kitsu_m5_integration.md`.

### Upstream candidates
- Nessuno (codice tutto Ztoryc-specifico in file nuovi).

---

## [2026-06-27] — Export progetto, naming render, status pipeline + fix crash/perdita-dati (v0.6.3)

Sessione lunga: dall'export completo del progetto al ciclo di stato della pipeline
(Modello A: un .tnz per shot riusato in tutte le fasi), più due fix pesanti
(crash su nuovo progetto, perdita dati Production Tracker).

### Added — Export & naming
- **Export Project Spreadsheet** (Production Tracker → Shots): un singolo `.xlsx`
  con TUTTI gli storyboard + TUTTI i tab → fogli `Project · Overview · <tecniche> ·
  Team · Assets · Workflows`. Thumbnail dalla cache su disco (nessuna scena da aprire).
- Board: l'export è rinominato **"Export Storyboard Spreadsheet"** (scena corrente);
  il Production Tracker usa la stessa icona (più larga) per l'export di progetto.
- **Riordino task** nel tab Workflows: drag&drop + frecce ▲▼ (QToolButton).
- **Output name dal pattern di progetto** (Output Settings → "Name from project
  pattern…"): compone il nome del render da PROD/SEASON/EP/SEQ/SHOT/TASK/VER con
  preview live (Opzione 1 — Modello A: task solo nel nome dei render).
- Tab `Project` spostato per primo (pannello + fogli export).

### Added/Changed — Pipeline Modello A
- **Export Shots**: rimosso il selettore Task stage; il `{TASK}` non finisce più nel
  nome del `.tnz` (un solo file per shot riusato in tutte le fasi).
- **Ciclo di stato del primo task** (di solito Layout): export → `READY`; primo
  caricamento dello shot → `WIP`. Tecnica risolta dal **project DB** (quella impostata
  nel tracker) → ogni shot apre nella room giusta e avanza il task corretto.
- **Auto workflow detection**: aprendo una scena si entra nel workflow del suo
  ruolo/tecnica (storyboard→Storyboard; shot→Cutout/StopMotion/Tradigital). Spunta
  **"Automatic"** nello startup (persistente in QSettings); switch manuale sempre
  possibile (auto-switch one-shot, deduplicato per scena).
- Auto-WIP reso **room-indipendente**: listener globale su `ZtoryModel::sceneSwitched`
  (prima viveva solo nello StoryboardPanel → non scattava nelle room senza Board).
- Badge "SB" nello startup solo per `role="storyboard"` (non per gli shot esportati).

### Fixed — Crash & perdita dati (alta priorità)
- **CRASH creando un nuovo progetto** (`dvdirtreeview.cpp`): `connect(...projectAdded,
  [=]{...})` senza receiver context → connessione dangling sul singleton `DvDirModel`;
  dopo la distruzione del tree (room switch) il successivo `projectAdded` sparava su
  `this` morto → SIGSEGV. Fix: passare `this` come context. **Candidato PR upstream.**
- **Perdita dati Production Tracker** (production/team/assets svuotati): firewall in
  `saveProjectDb()` — non sovrascrive un `.ztrack` esistente con meta+team+assets tutti
  vuoti (firma di un reset transitorio non ancora ripopolato da `loadProjectDb`).
- `saveZtoryc()` non riscrive più il `.ztoryc` companion delle scene `role="shot"`
  (riscriveva `role="storyboard"` → badge SB + workflow sbagliato).
- Colonne task del tracker non sparivano più aprendo uno shot: `spreadsheetTaskColumns()`
  ora copre anche `m_projectShots` (non solo gli `m_shots` della scena aperta).

### Upstream candidates
- `dvdirtreeview.cpp` — dangling lambda connection su `DvDirModel::projectAdded` (crash
  riproducibile su creazione progetto dopo distruzione del tree). Alto impatto, fix 1 riga.

---

## [2026-06-26b] — Thumbnail persistenti multi-storyboard + fix popup startup/progetti (B3b)

Sessione di hardening multi-scena del Production Tracker e fix UX della startup page.

### Fixed — Thumbnail Production Tracker (multi-storyboard)
- **Cache thumbnail su disco** (`<project>/thumbs/<uuid>.png`): caricata interamente
  all'avvio del Production Tracker, così le anteprime di TUTTI gli storyboard sono
  sempre visibili qualsiasi scena sia aperta (prima si vedevano solo quelle della
  scena corrente, e il pannello a tutta room non le caricava).
- **UUID v5 namespaced per file storyboard** (`makeSourcedUuid`): elimina le collisioni
  di uuid tra `.ztoryc` diversi (es. anteprima di "bugs" che appariva su "camera").
  `ensureShotUuids()` rileva e rigenera gli uuid in conflitto cross-storyboard.
- **Attribuzione per `source`** in `publishShotsToProjectDb`: upsert aggiorna solo gli
  shot di proprietà del file corrente, niente più ri-attribuzione errata.
- Refresh thumbnail **event-driven** via segnale `previewUpdated` (debounce 400ms),
  al posto dei timer di retry.
- Badge **"SB"** sulle scene storyboard nella schermata di avvio.

### Fixed — Startup page / cambio progetto (doppio popup)
- **Root cause**: selezionare un progetto nell'albero del browser chiama
  `DvDirModelNode::makeCurrent()` che creava SEMPRE una seconda `StartupPopup` →
  due finestre, una non si chiudeva al caricamento scena.
- Ora `makeCurrent()` riusa la popup di startup già visibile: chiude il browser
  (come "Choose") via `BrowserPopupController::closePopup()` (reject) e aggiorna la
  stessa finestra con `StartupPopup::refreshAfterProjectChange()`. Nessun duplicato.
- Tracking di tutte le istanze (`s_instances` + `visibleDefaultInstance()`), valido
  per **Open Project**, **Load Scene** e **New Scene** (tab corretto per modo).

### Upstream candidates (file core condivisi)
- `filefield.h` (+`closePopup()` su `BrowserPopupController`), `filebrowserpopup.cpp/.h`,
  `filebrowsermodel.cpp` — il doppio-popup al cambio progetto da browser è bug stock
  Tahoma2D potenziale (verificare su stock prima di proporre PR).

---

## [2026-06-26] — Production Tracker: room a tab, multiselezione, e file di progetto (B3a)

Grande arco sul Production Tracker. Studiata la Kitsu reale locale; il tracker e'
diventato un mini-Kitsu in-app (pannello a tab) e ora ha un **file di progetto**
(`production.ztrack`) per i dati project-level. Design B3 completo in
`DESIGN_production_tracker.md` (Modello B). Tutto su master.

### Added — Production Tracker a tab (pannello `ztoryproductionpanel`)
- **Tab Shots**: matrice shot x task con status+assignee; arricchita con Thumbnail
  (riuso anteprime del Board via `firstPanelThumbnail`), Frames, Sec/Fr, **Workflow
  editabile** (click sulla cella), **Done** (progress bar). Set Technique del Board
  disabilitato (icona tenuta per futuro breakdown tagging).
- **Tab Team**: roster di progetto (add/remove/rename) — spostato fuori da Storyboard
  Settings.
- **Tab Project**: production/season(nuovo)/episode/title/default technique inline.
- **Tab Assets**: entita' Asset (type/name/tasks/tags) con add/remove + matrice task.
- **Tab Workflows**: editor task-type custom alla Kitsu (workflow + lista task editabili).
- **Assignee multipli** per task; picker dal Team (checkbox) con fallback testo libero.
- **Multiselezione** celle task (shift=intervallo, Cmd=salti, drag) -> tasto destro =
  batch set status/assignee (un solo undo); doppio-click = edit singolo.

### Added — B3a: file di progetto `production.ztrack`
- Modello B (deciso): `production.ztrack` alla radice progetto e' la **fonte di verita'
  project-level**. Possiede meta (production/season/episode/title/defaultTechnique/
  namingPattern) + team + techniques + assets, **condivisi tra le scene del progetto**.
- `ZtoryModel::loadProjectDb/saveProjectDb/resetProjectLevelDefaults`; migrazione
  automatica dai `.ztoryc` vecchi; il `.ztoryc` resta scene-level (shot/sequenze/
  numbering/panel/pdfLogo).

### Fixed
- Bug latente: i task stavano in 2 copie non sincronizzate (StoryboardPanel vs
  ZtoryModel) -> editing tracker non persisteva/esportava. Ora ZtoryModel e' la fonte
  unica (bridge pull/push a load/save/export). Aggiunto `ShotData::uuid` stabile (Fase A).
- Refresh tab non-shot al riapri scena (`productionReloaded`).
- **Leak project-level tra scene/progetti**: azzeramento completo dei campi a ogni load
  scena (incl. techniques->reseed) su tutti i path, prima della migrazione. Ripuliti i
  file gia' contaminati (`2303v15.ztoryc`).
- Palette status allineata ai colori Kitsu reali (WIP blu / WFA viola) nel pannello e
  nell'export xlsx.

### Added (fuori repo)
- `~/Desktop/Production_Tracker_Kitsu_Template.xlsx`: template standalone Google-Sheets-ready
  ispirato a Kitsu (Shots/Assets/Team/Dashboard), costruito con la skill xlsx.

### Notes / prossimi passi
- **B3b** (prossima sessione): shot nel project file + pubblicazione multi-storyboard
  (per uuid+source), tab Shots raggruppata per file; nodo thumbnail per storyboard non
  aperti. Poi B3c (export-tnz + naming convention `AVIS_CS26_EP03_AMC_V01`) / B3d.
- Room dedicata "Production" + eventuale colonna "Due" opzionale = separati.
- Deciso vs Kitsu: qui status/assignee/workflow/progress/thumbnail; deadline+scheduling -> Kitsu (M5).

## [2026-06-25] — Production Tracker panel (status in-app) + palette Kitsu allineata

Nuova direzione export production: **status masterizzati in-app**, foglio = proiezione
(niente Google API; integrazione Kitsu rimandata a M5). Decisioni prese studiando la
Kitsu reale locale di Franco (docker, localhost:8012): task type/status e colori veri.

### Added — Production Tracker panel (`ztoryproductionpanel.h/.cpp`, nuovo)
- Pannello dockable **matrice Kitsu-style**: righe = shot, colonne = task-type della
  tecnica (Layout, Key Animation, Inbetweening, …). Celle status colorate; **N/A** grigio
  dove il task non si applica alla tecnica dello shot. Voce **Windows ▸ Ztoryc ▸ Production
  Tracker** (registrato come gli altri pannelli: factory + commandid + tpanels + mainwindow
  + menubar.cpp + le 2 menubar.xml).
- **Editing in-app**: click su una cella → menu con i 6 stati (pallino colorato) → scrive nel
  modello, salva il `.ztoryc` via `StoryboardPanel::saveZtoryc()`, **undo** (`StatusEditUndo`,
  keyed su `shotLabel` → sopravvive al riordino degli shot).
- Refresh via nuovo segnale leggero `ZtoryModel::taskStatusChanged` (NON `shotDataChanged`,
  così niente re-bake thumbnail nel Board/Animatic). Nuovo setter `setShotTaskStatus` /
  `setShotTaskStatusByLabel` nel modello.
- **Re-export non distruttivo "gratis"**: con gli status nel modello/`.ztoryc`, ri-esportare
  dopo aver aggiunto shot non azzera nulla.

### Modified — palette status allineata a Kitsu
- `statusColor` del pannello E dell'export xlsx (`storyboardpanel.cpp`) ora usano i colori
  **ufficiali Kitsu**: Todo grigio · Ready ambra `#FBC02D` · WIP blu `#3273DC` · WFA viola
  `#AB26FF` · Retake rosso `#FF3860` · Done verde `#22D160`. Prima WIP/WFA erano invertiti
  (viola/blu) rispetto a Kitsu. La formattazione condizionale dell'xlsx usa la stessa lambda
  → allineata automaticamente.

### Added (fuori repo) — template spreadsheet standalone
- `~/Desktop/Production_Tracker_Kitsu_Template.xlsx`: template riutilizzabile Google-Sheets-ready
  (Project Info / Shots / Assets / Todo / Dashboard) ispirato al modello Kitsu, con dropdown
  status + colori condizionali + frozen panes + legenda. Costruito con la skill xlsx.

### Notes / prossimi passi
- **Fase 3** (da fare): auto WIP→WFA al render (`RenderCommand::onRenderCompleted`, già
  individuato → stesso path `setShotTaskStatus`).
- Guida naming-convention della produzione (pattern in `.ztoryc` + lint) — idea separata, da fare.
- Integrazione Kitsu vera = M5 (via API, non file).

## [2026-06-23d] — Post-0.6.2: polish Thumbnail panel + fix build Windows

### Fixed (Windows release)
- **Build Windows 0.6.2 fallita in link** (LNK2001): il canvas usa `MyPaintToonzBrush`/
  `Raster32PMyPaintSurface` di tnztools dall'eseguibile, ma erano senza `DVAPI` (non
  esportate dalla DLL). Aggiunto export. Commit `401828950`. La 0.6.2 Windows e' poi uscita.
  → Gotcha: classi di lib usate dall'exe vanno marcate `DVAPI` o falliscono SOLO su Windows.

### Added — Thumbnail panel polish (commit `adaf8e2e0`)
- Griglia di default **4x4** (era 4x3) + reset alla default aprendo una scena senza canvas
  salvato (bug: prima ereditava le righe aggiunte con +Row).
- **Zoom con la rotella** (centrato sul cursore), niente piu' Ctrl.
- **Scrollbar laterali** (16px) che compaiono solo quando il contenuto sborda; sync col pan
  da tasto centrale.
- **Cursore pennello**: cerchio della dimensione reale (exp(RADIUS_LOGARITHMIC + sizeMod) *
  zoom) con mirino; system cursor blank in draw mode.

### Note
- Loop in `createNewLevel` con nome livello collidente (NameModifier `_N` letto come
  separatore frame): **NON confermato come bug upstream**. La helper `rfindFrameSep`
  (`common/tsystem/tfilepath.cpp`) che decide i separatori e' **codice nostro** (PR trattino,
  commit `d1f4e21ef`), quindi il comportamento puo' differire da Tahoma stock. Da verificare
  su stock prima di considerare qualsiasi PR — probabilmente NON e' un problema upstream.

## [2026-06-23c] — Release 0.6.2: Thumbnail room completa (export, persistenza, panoramiche, transform, undo)

Rilascio **v0.6.2** (Win + macOS). Completata la Thumbnail room (Fase 3) e fix vari.

### Added — Thumbnail room (branch `feature/thumbnail-room`, in master via FF)
- **Export-to-board + Shrink** (`2f3267add`): i pannelli selezionati diventano uno
  shot reale nel Board (livello OVL multi-frame in `extras/<scena>/`, sotto-scena,
  colonna main, resequence). Spinbox **Shrink** 1-8 (risoluzione disegni = cameraRes/shrink).
  - **Fix hang**: `createNewLevel` andava in loop infinito quando esistevano gia' i PNG
    di export precedenti. `NameModifier` disambigua con `_N`, ma Tahoma legge `_<cifre>`
    come separatore di frame → `thumb_1` collassa su livello `thumb` → collisione eterna.
    Ora il livello e' nominato col label dello shot (`SH0xx`) e la disambiguazione su
    disco usa un suffisso LETTERA (`SH040B`). **Bug core Tahoma → candidato PR upstream.**
- **Persistenza del canvas per scena** (`b93899885`): il raster contiguo viene salvato
  come PNG in `extras/<scena>/_ztorythumbs_<cols>x<rows>.png` (autosave debounced +
  flush a chiusura), ricaricato al cambio scena. Prima i disegni si perdevano a chiusura.
- **Panoramiche via merge** (`c94c775a6`): merge di un blocco rettangolare di pannelli
  in un unico panel-panorama (selezione diagonale auto-completa il rettangolo). In export
  tutti i selezionati = UN solo shot su UN livello dimensionato sul pannello piu' grande.
- **Transform tool** (`bb2cb24fc`): selezione raster (rettangolare o **lazo**) → sposta,
  copia/incolla (Cmd+C/V), scala (angoli), ruota (maniglia). Icone tool + cestino.
- **Undo/redo** (`5d3a9018f`): Cmd+Z / Cmd+Shift+Z (snapshot full-canvas, 16 step) su
  tratti, +Row, merge/split e operazioni Transform (uno step per operazione).

### Fixed (inclusi nella release, gia' su master/branch dalle sessioni precedenti)
- Camera unica di scena + anteprime camera-aware (`dbf0f5cc7`).
- Animatic: play prosegue se il video e' piu' lungo dell'audio; play non si ferma sulla
  1a traccia con piu' tracce audio; export non sporca la scena (`7bbadf0fe`, `f28b8f565`).
- Sync Export Animatic ↔ Render Settings (`24d91e076`, `ff456130d`).
- Board: undo del Delete Shot non duplica piu' gli shot vuoti (`aa3b16dea`).
- Persistenza toolbar dei viewer animatic/shot (`ebcf790d5`, `ed25d5df4`).

### Upstream candidates (PR Tahoma)
- **NameModifier vs separatore frame `_N`** in `createNewLevel`: loop infinito quando un
  livello con quel nome esiste gia' su disco (sequenza). Hardening: disambiguare con
  suffisso non-collassabile. Riproducibile su stock creando livelli con nome collidente.

### TODO prossima sessione
- Icone tool: sostituire le disegnate a mano con Lucide/Phosphor (path da fornire).
- Tasto Canc nudo non sempre cattura (focus): per ora cestino + Cmd+Backspace.

## [2026-06-23b] — Camera unica di scena + anteprime camera-aware; Thumbnail room Fase 3 (step 1-2)

### Fixed / Added (master, commit `dbf0f5cc7`)
- **Camera unica di scena** (regressione vs Tahoma). La camera (res+size) ora e' un
  parametro unico per main + tutte le sotto-scene, come in Tahoma nativo.
  - Nuovo `ZtoryShotOps::syncAllCamerasFrom(scene, srcXsh)`: propaga res+size della
    camera appena modificata a TUTTI gli altri xsheet (main + sotto-scene), in
    entrambi i versi. Hook in `CameraSettingsPopup::onChanged` (edit dal main O da
    dentro uno shot). Solo res+size: i keyframe di camera-move restano intatti.
  - `addShotNamed` (startup dialog) ora chiama `syncChildCameraToMain`: gli shot
    creati all'avvio non ereditano piu' una camera di default != camera Preferenze.
- **Anteprime camera-aware** (Board, PDF, track Animatic, story strip). Nuovo helper
  `ZtoryShotOps::cameraAspect(scene)` (= res.lx/res.ly, fallback 16:9) sostituisce
  tutti gli hardcode 16:9. Una camera quadrata mostra inquadrature quadrate ovunque.
  Board re-renderizza le anteprime al cambio di aspect (`onXsheetChanged` +
  `m_lastCameraAspect`); cache thumbnail animatic invalidata su cambio forma.
- **GUI Show/Hide nel viewer ANIMATIC main**: `ZtoryAnimaticViewer` sovrascriveva
  `addShowHideContextMenu`/`updateShowHide` con corpi VUOTI dal primo commit (non era
  `bvp` null come ipotizzato): ora deleghano alla base, il submenu compare. Rimosso
  il log diagnostico temporaneo da `onContextMenu`.

### Added — Thumbnail room, Fase 3 step 1-2 (branch `feature/thumbnail-room`)
- Merge di master nel branch (`5458b8f1e`): l'helper `cameraAspect` disponibile qui.
- **Griglia con aspect camera** (`e063dc724`): le box derivano l'altezza da
  `cameraAspect()` invece del 16:9 fisso (480x270). NB: letto alla creazione del
  canvas, non live (follow-up flaggato come task in background).
- **Step 1 — selezione pannelli** (`29452d1de`): modalita' Select in toolbar, click
  in ordine (= ordine export), overlay arancio + badge numerato, contatore + Clear.
- **Step 2 — rilevamento vuoti** (`70d865ab1`): `isPanelEmpty()` (raster non-bianco,
  soglia 250); i pannelli vuoti non sono selezionabili.

### TODO prossima sessione
- **Thumbnail room Fase 3 step 3 — export-to-board** (il grosso): ritaglio regione
  raster per pannello selezionato → child level (sotto-scena) con livello raster a N
  frame (1 per pannello, ordine di selezione) salvato in `drawings/` → shot reale in
  main xsheet + board + timeline (wiring tipo `addShotNamed`). Decisioni gia' prese:
  selezione → 1 shot multi-panel, pannelli come sequenza di frame, salta i vuoti.
- (opz.) refresh live della griglia thumbnail al cambio camera.

### Decisioni / scelte tecniche
- Cameras sono per-xsheet in Tahoma (`getCurrentCamera()` = camera dell'xsheet
  corrente); `TSceneProperties::m_cameras` e' solo un mirror del main per la
  serializzazione. La coerenza "camera unica" si ottiene sincronizzando gli stage
  object camera, non centralizzando il dato.

## [2026-06-23] — Batch fix segnalazioni utente (undo Delete Shot, persistenza GUI viewer)

### Fixed (su master, pushato)
- **Undo del Delete Shot duplicava tutti gli shot/sotto-scene** (`storyboardpanel.cpp`
  `restoreFromSnapshot`). Il conteggio delle colonne-shot da rimuovere le identificava
  come "colonna con una cella child-level" e si fermava alla prima senza: uno **shot vuoto**
  (solo celle vuote/rosse — stato valido) non ha celle child-level → stop anticipato →
  rimosse troppo poche colonne → il re-insert dello snapshot duplicava tutto dopo lo shot
  vuoto. Ora rimuove tutte le colonne iniziali fino alla prima traccia audio o al primo
  livello reale (non sub-scene). Regression-safe. Commit `aa3b16dea` (+ merge `f542b0830`).
  NB: bug Windows-only nei test utente ma codice identico cross-OS → era data-dependent.
- **Toolbar del viewer di disegno (shot) non persistevano** tra riavvii (a differenza di
  Tahoma). Il viewer è un `ComboViewerPanel` *embedded* (non pannello top-level della room)
  → la room non ne serializza i visible-parts. Ora salvati/ripristinati in QSettings
  (load alla creazione, save su uscita shot-mode + distruttore). **Confermato funzionante.**
  Commit `ed25d5df4`. Stessa persistenza aggiunta al viewer animatic (`ebcf790d5`).
- **`SceneViewer::onContextMenu`**: il pannello viewer ora si trova via match
  `p->sceneViewer()==this` su `QApplication::allWidgets` invece dell'annidatura fissa
  `parentWidget()->parentWidget()` → robusto ai viewer embedded. Candidato upstream. `ebcf790d5`.

### Aperti (prossima sessione — vedi ANIMATIC_TASKS #5/#6/#7)
- **"GUI Show / Hide" non compare nel viewer ANIMATIC main**: `bvp` resta null a runtime
  nonostante il match (binario verificato aggiornato). Serve log diagnostico in `onContextMenu`.
- **#2 Camera unica di scena** (regressione vs Tahoma): la camera deve essere un parametro
  unico per tutta la scena (main + tutte le sub-scene). Modello: `TSceneProperties::m_cameras`
  scena + camere per-sub-xsheet da allineare. Piano in task #6 (sync + hook cambio camera).
- **#1 CRASH new project → selezione cartella (Windows only, no crash log)**: serve stack trace.

## [2026-06-22] — Thumbnail room (Fasi 1-2): canvas MyPaint + griglia continua + palette  [branch `feature/thumbnail-room`]

> Lavoro su branch dedicato `feature/thumbnail-room` (NON master). Milestone "Thumbnail room":
> griglia per schizzare panel veloci, poi export-to-board (Fase 3 da fare).

### Added — Thumbnail room (nuova room/panel)
- **Panel `ZtoryThumbnailPanel`** registrato in `Windows ▸ Ztoryc ▸ Ztoryc Thumbnails`
  (factory + `OpenFloatingPanel` in `tpanels.cpp` + `MI_OpenZtoryThumbnail` + menubar + CMake).
- **Canvas di disegno custom `ZtoryThumbnailCanvas`** che pilota il **motore brush MyPaint
  vero di Tahoma** (`MyPaintToonzBrush` su `TRaster32P`), del tutto **disaccoppiato da
  SceneViewer/TTool**. Brush `.myb` reali caricati via `TMyPaintBrushStyle`; colore→HSV come
  il fullcolor tool; `dtime` reale per le dinamiche. Scelta architetturale chiave: NON
  embeddare un `ComboViewerPanel` (TPanel annidato in TPanel rompe il routing del viewer
  attivo → non si disegna). Decoupling raster = export futuro banale (ritaglio, no slicing).
- **Superficie continua** (un unico raster contiguo per tutta la griglia): gli stroke
  attraversano i confini dei pannelli → **panoramiche orizzontali/verticali** come canvas
  ad hoc. I bordi pannello sono solo overlay (linee sottili).
- **Pan** (tasto centrale / rotella, Shift = orizzontale) e **zoom** (Ctrl+rotella, centrato
  sul cursore). **`+ Row`** ingrandisce la griglia (raster ricopiato preservando il disegno).
- **Palette toolbar**: strumenti con **icone = preview MyPaint del brush** (`_prev.png`):
  Pencil/Brush/Airbrush/Kneaded/Eraser + **`+`** per aggiungere un brush dalla libreria.
  **Colore separato dal brush**: chip blu `#1D5C83`/nero/rosso + **swatch attivo** (più grande,
  bordo marcato, ▾) che apre il color picker. Le gomme dipingono **bianco** sulla carta opaca
  (normale = pieno, gomma pane = bianco a opacità 30% → schiarisce). Slider **Size**.

### Fixed (in-feature)
- `TPanel` è un `TDockWidget`: contenuto installato via `setWidget()` (col layout diretto il
  canvas restava invisibile).
- Link: l'app non linkava `libmypaint` (lo tirava solo `tnztools`) → aggiunto `${MYPAINT_LIB_LDFLAGS}`
  in `toonz/CMakeLists.txt`; aggiunto `tnztools` agli include dir dell'app.
- Brush aggiunto col **+** (path assoluto dal file dialog): `resolveBrushFile` ora passa i path
  assoluti → l'icona `_prev.png` si carica e il brush disegna col proprio stile.

### TODO (prossime sessioni)
- Fase 2 resto: selezione multi-pannello + riordino + aggiungi/rimuovi pannelli.
- Fase 3: **export to board** (riquadri/panoramiche → shot reali in `ZtoryModel` + timeline/board).
- Salvare la palette come **global/studio palette** Tahoma, default anche in modalità normale.
- Possibili "pagine" oltre alle righe per gestire decine di pannelli.

## [2026-06-21b] — Release 0.6.1 (macOS+Windows), fix CI ccache, fix post-release

### Release
- **Pubblicata v0.6.1** (macOS Intel+Silicon DMG + Windows installer/portable), note bilingui IT/EN.
- Bump `ZtorycVersion.cmake` 0.6.0 → 0.6.1. Trattino aggiunto ai **PR candidates** upstream (AGENTS.md).

### CI
- **macOS+Linux: `CC="ccache <compiler>"` rompeva Qt AutoMoc** (`moc_predefs.h`): la ccache
  aggiornata sui runner rifiuta `ccache -std=… clang++` (`invalid option -- t`). Fix: CC/CXX nudi +
  shim ccache nel PATH (macOS `brew --prefix ccache`/libexec; Linux `/usr/lib/ccache`). macOS OK.
  Linux: ccache risolto, resta QXlsx che richiede header privato Qt (`qzipreader_p.h` →
  `qtbase5-private-dev` + tweak CMake) — task a sé.
- Push dei file `.github/workflows/*` richiede scope `workflow` sul token (push manuale utente).

### Fixed
- **Monitor animatic: tracce che glitchavano/sparivano + gap verticale**. `refreshFromScene` usava
  `animaticFrameCount` (include l'audio piazzato) per la larghezza; dopo un razor un ColumnLevel
  audio con endOffset=0 fa restituire a `getVisibleEndFrame` la durata del file raw → minWidth
  abnorme → glitch/gap. Fix: larghezza dai **soli blocchi video** (come `updateTrackWidths`).
- **Rebrand popup render** (`dvdialog.cpp`): `ProgressDialog`/`RadioButtonDialog` usavano
  `tr("Tahoma2D")` come titolo → ora `getAppName()` ("Ztoryc"). Risolve il popup "Finalizing render".
- **Play si fermava alla fine della 1ª traccia audio** con due tracce di lunghezza diversa
  (`getMasterAudioUsecs`): leggeva il `processedUsecs` della PRIMA colonna, che si congela quando
  quella traccia finisce. Fix: usare il **massimo** tra tutte le colonne (il player ancora attivo).
- **Play si fermava a fine audio se il video è più lungo** (`onDrawFrame`): il playback è
  audio-clocked, quindi col video oltre l'audio il playhead si bloccava a fine audio invece di
  arrivare al mark-out. Fix: quando il clock audio è congelato (audio finito) ma il mark-out è
  ancora avanti, avanza sul **wall-clock della FlipConsole** (`m_lastMasterAudioUsecs` rileva se
  l'audio avanza ancora).
- **Export/render marcava la scena come modificata** (`onExportAnimatic`): il ripristino delle
  output properties post-render chiamava `notifySceneChanged()` (default `setDirty=true`) → asterisco
  spurio dopo ogni export. Fix: `notifySceneChanged(false)` (ripristino net-zero, niente dirty).
  NOTA: resta aperto un bug intermittente non riprodotto — asterisco che non si pulisce sul MAIN
  mentre si lavora sull'animatic (in sotto-scena ok). Da indagare con repro o log su setDirtyFlag.

### Changed
- **Export Animatic ↔ Render Settings: sync bidirezionale live** (`onExportAnimatic`).
  Output folder/Filename ↔ Save in/Name (poll non-distruttivo + write-back con `code/decodeFilePath`
  per l'alias `+outputs`); nota Format ed estensione filename aggiornate live; il "…" scrive in
  `prop` (il poll non riverte) + rialza il dialog (z-order macOS) e usa la label **"Choose"**;
  `notifySceneChanged()` rinfresca il popup nativo in tempo reale (Export→Render).



### Added
- **Riconoscimento sequenze con trattino** (`frame-0006.jpeg`). Aggiunto `-` come separatore
  frame alla pari di `.`/`_` nel parser core (`tfilepath.cpp`): helper `rfindFrameSep()` usato
  in read (getDots/getSepChar/getFrame/getWideName/getLevelNameW) e write (withName/withFrame,
  che preserva il `-` nel round-trip) + regex `analyzePath`. La guardia "solo cifre tra
  separatore ed estensione" rende sicuro `my-file.jpg` (resta singolo). **Candidato PR upstream.**
- **Modalità rilevamento sequenze (Automatic / Sequence / Individual frames)** stile DaVinci.
  Preferenza globale `numberedFilesImportMode` (tab Loading) + override per-caricamento nei popup
  **Load Level** e **Import Assets** (helper riusabile `FileBrowserPopup::createNumberedFilesModeCombo`).
  `FileBrowser::SequenceMode` pilota il grouping in `refreshCurrentFolderItems` (Automatic =
  raggruppa ma demota i singleton a still; Individual = ogni file separato).
- **Board "Compact view"** (toggle in toolbar, icona `ztoryc_compact_view` stacked-cards): una card
  per shot (mostra il panel corrente) con frecce ◀ ▶ per navigare i panel in place. Allevia il Board
  con scene animate (un panel per keyframe). `rebuildGrid` collassato + swap della singola cella su
  nav (no rebuild completo); `updateVisiblePreviews` salta i panel nascosti. Stato persistito
  (`TEnv ZtoryBoardCollapsePanels`).

### Fixed
- **Animatic: il play si fermava prima della fine / audio muto su shot vuoti** (`ztoryanimatic.cpp`).
  Due cause: (1) il playback è audio-clocked e la cache audio per-colonna è tagliata a
  `videoFrameCount`, ma `soundColumnsFingerprint` guardava solo i sound column → allungando il video
  (Load nelle sub-scene) la cache restava stale (play fermo a 828). Fix: includere la lunghezza nel
  fingerprint. (2) timeline = solo video → audio oltre l'ultimo shot non suonava. Fix: nuovo
  `animaticFrameCount = max(videoFrameCount, audioPlacedFrameCount)` (placement via `getRange` sui
  sound column, non il file raw) usato per cap audio, play range, FlipConsole range, stop/markOut,
  larghezza track. Montaggio audio-led ora riproduce tutto l'audio.
- **CRASH su Windows esportando lo Spreadsheet** (`storyboardpanel.cpp` `onExportSpreadsheet`).
  `xlsx.sheetNames().first()` su `QXlsx::Document` appena costruito: nessun foglio finché non si
  scrive → QList vuota → `.first()` UB → `EXCEPTION_ACCESS_VIOLATION` in `renameSheet`/`operator==`
  (innocuo su macOS per la QString nulla condivisa). Fix: guardia — rename del foglio default se
  presente, altrimenti `addSheet(overviewName)`.
- **Undo dello split shot perdeva i disegni della seconda metà** (`ztoryanimatic.cpp`
  `onRazorRequested`). Lo split muta `origCL` in place (`trimChildXsheetTo`), ma lo snapshot
  `UndoBoardState` tiene solo un PUNTATORE al child level → l'undo riesponeva `origCL` già trimmato.
  Fix: clonare `origCL` in un livello backup pristine PRIMA del trim (via `cloneChild`), orfanare la
  colonna (il `TXshLevelP` lo tiene vivo) e ripuntare lo snapshot `before` sul backup → undo lossless.
- **Compact view: anteprime non aggiornate dopo undo/redo** (`storyboardpanel.cpp`
  `restoreFromSnapshot`). Mancava il refresh deferito post-rebuild. Fix: `onRefreshPreviews` via
  `singleShot(0)` dopo `rebuildGrid()` (vale per qualunque undo: reorder/split/delete…).

### Changed
- **Razor audio dentro lo shot**: `onAudioRazorRequested` ora funziona anche con una sub-scena aperta
  (opera sempre su `mainXsheet()`; dentro una sub-scena usa `notifyCastChange` invece di
  `notifyXsheetChanged` per non innescare cascate dal contesto sbagliato). Comodo per gli spostamenti
  della guida audio senza uscire dallo shot. Il drag dei segmenti audio già funzionava. Razor degli
  shot (video) lasciato gated.

### Notes
- Build dir Ninja = ROOT del workspace (non `toonz/build`, che non esiste più e fa fallire ninja in
  silenzio). Per i check rapidi: `ninja -C /Volumes/ZioSam/tahoma2d-workspace/tahoma2d`.

## [2026-06-21] — Fix crash maniglia "Drawing #" Animate tool (bug upstream)

### Fixed
- **CRASH trascinando la maniglia "Drawing #" dell'Animate tool** (`tnztools/edittool.cpp`,
  `DragDrawingNumberTool::leftButtonDrag`). Root cause trovata con lldb sul debug build:
  il tool registra UN solo canale (`DragChannelTool(T_DrawingNumber, false)` -> `m_channels.size()==1`)
  ma il drag usava l'API a due canali `setValues(v0, getOldValue(1)+delta.y*factor)` (copiata dal tool
  di posizione), accedendo a `m_channels[1]` inesistente. Debug: `assert` in
  `TStageObjectValues::getValue` (stageobjectutil.cpp:147). Release: OOB read+write -> corruzione heap
  -> trap `malloc` (i crash log erano mal-simbolicati in overlay GL/Qt, fuorvianti). Scattava al primo
  micro-movimento del mouse sulla maniglia. Fix: usare l'API a canale singolo `setValue(v0)`
  (solo il drag orizzontale cambia il drawing number).
- **VERIFICATO su Tahoma2D stock** -> bug upstream (feature PR #2124, merge `67f0ef7f4`), NON regressione
  Ztoryc. Candidato PR upstream ad alto impatto (vedi AGENTS.md).

### Added / Changed (Animate tool, stessa sessione)
- **Drawing # — comportamento corretto**: il riferimento del numero di disegno viene letto dalla
  CELLA (non dal param `T_DrawingNumber`, che e' 0 senza keyframe). Un click secco o un micro-drag
  non cambia nulla e non aggiunge keyframe; la chiave nasce solo quando il drag passa davvero a un
  altro disegno. Clamp minimo a **1** (0 = cella vuota). Allineato al campo Drawing# di tool options.
- **Maniglia "Drawing #" nascosta di default** (`m_showDrawingNumber`, env `EditToolShowDrawingNumber=0`):
  gate su disegno **+** `glPushName` -> sparisce anche dal pick (non cliccabile per sbaglio).
  Riattivabile dalle opzioni tool. Nota: upstream non collegava affatto la flag al disegno.
- **Gizmo Animate adattivo allo sfondo (ripristinato)**: `sampleBgColor` (glReadPixels al centro) +
  `gizmoContrastColor` (complementare a luminanza forzata; b/n su sfondi neutri); highlight del device
  in hover sempre vivido a luminosita' media, leggibile sia su gizmo chiaro che scuro. Era stato tolto
  per errore col revert anti-crash; il crash era il Drawing#, non questo.
- **Animate tool disabilitato nel viewer animatic** (`ZtoryAnimaticViewer::eventFilter` sul SceneViewer):
  ingoia click/drag sinistro e tablet quando il tool attivo e' `T_Edit` -> niente trasformazioni nella
  vista timing/preview; pan, zoom e hover restano attivi. Hint al primo click.

## [2026-06-20c] — Tema chiaro "Abete" (WIP) + Animate tool: gizmo leggibile su ogni sfondo

### Added (WIP — tema Abete)
- **Tema chiaro "Abete" (legno caldo)** — `stuff/config/qss/Abete/Abete.qss`, generato da
  Light.qss: neutri tintati su rampa legno per luminanza, selezione magenta -> ambra/oro,
  semantici (rosso/arancio/blu/verde) invariati. Auto-scoperto (Preferences > Interface > Style).
- **`ztorytheme.h`** — palette centralizzata header-only per i pannelli custom: `isLight()`
  (legge il tema attivo), `wood(lum)` rampa abete, `g(v[,a])` grigio neutro che segue il tema
  (dark: grigio originale; light: inversione di luminanza sul legno), `activeShot()`/`accent()`.
- **Strato 2 (timeline animatic)** — `ztoryanimatic.cpp`: 32 grigi neutri ora via `ZtoryTheme::g()`,
  testo bianco -> testo-tema; scrim "muted"/chip marker restano scuri in entrambi i temi.
  Shot attivo: oro logo `#ECA61C` in Abete, **magenta `#E0249B` mantenuto nei temi scuri**.
- STATO: **WIP**. L'oro su legno e la resa generale vanno ancora rifiniti; il Board (storyboardpanel,
  stylesheet-based) non e' ancora tematizzato. Default invariato (Dark).

### Added (Animate tool — leggibilita' gizmo)
- **Colore gizmo adattivo allo sfondo** (`tnztools/edittool.cpp/.h`) — `sampleBgColor()` legge
  via `glReadPixels` il colore medio sotto il centro del gizmo; `gizmoContrastColor()` colora il
  manipolatore col **complementare a luminanza forzata** (fondo chiaro -> gizmo scuro e viceversa;
  fondo neutro -> bianco/nero). Si aggiorna mentre si sposta il gizmo. Risolve il gizmo invisibile
  su disegni di colore simile.
- **Highlight device sempre distinguibile** — l'elemento in hover (centro/estremi bracci) usa un
  colore **vivido a luminosita' media** con tinta nettamente diversa dal normale, leggibile sia
  quando il gizmo e' chiaro sia quando e' scuro.
- Implementata prima come "casing" a doppio tratto (alone), poi sostituita dalla versione a colore
  adattivo su richiesta utente; l'infrastruttura casing resta nel codice ma disabilitata.
- **REVERT (2026-06-21)**: `edittool.cpp/.h` ripristinati all'originale (gizmo fisso rosa/verde). La `glReadPixels` durante l'overlay corrompe il primitive-buffer immediate-mode su macOS (Metal-GL) -> crash non deterministico in `glEnd` (visto toccando il braccio Drawing Number, che disegna testo extra). Da rifare con sampling crash-safe (lettura dal raster dell'immagine, non dal framebuffer live, come fa lo style picker in un pass separato).

### Upstream candidates
- **Gizmo Animate tool adattivo allo sfondo** (`tnztools/edittool.cpp`) — codice core Tahoma2D,
  il gizmo invisibile su sfondi di colore simile affligge tutti gli utenti. Candidato PR upstream.

## [2026-06-20b] — Task 55: altezza tracce video/audio regolabile

### Added
- **Import audio dall'interno di uno shot** (`ztoryanimatic.cpp`, menu contestuale
  timeline). "Load Audio..." e "Add Audio Track" non chiedono piu' di tornare
  all'Animatic: se sei dentro una sub-scena chiudono lo shot (`while ancestorCount>0
  -> MI_CloseChild`) e operano sul main xsheet, coerente col pattern delle op
  strutturali del Task 53. Rispetta la regola "audio solo nel main xsheet".
- **Resize verticale delle tracce della timeline animatic** (`ztoryanimatic.h/.cpp`).
  Handle sul bordo inferiore di ogni traccia (striscia di `kZtoryResizeGrip=5px`,
  riservata), drag -> ridimensiona; cursore `SizeVerCursor` in hover. Limiti
  condivisi `kZtoryMinTrackH=24` / `kZtoryMaxTrackH=120`. Grip visivo (separatore +
  tratto centrato) disegnato in fondo a entrambe le tracce.
  - **Video track** (`ZtoryAnimaticTrack`): nuovo `m_trackHeight` (default 80) +
    `setTrackHeight()` con clamp; il `paintEvent` scalava gia' con `height()` ->
    thumbnail si adattano da sole. `DragMode::Resize`.
  - **Audio track** (`ZtoryAudioTrack`): `setTrackHeight()` ora fa clamp + no-op
    guard; la waveform (`m_waveformDirty`) si rigenera alla nuova altezza senza
    ricalcolo completo. `DragMode::Resize`.
- **Persistenza globale** via `QSettings` (`Ztoryc/VideoTrackHeight`,
  `Ztoryc/AudioTrackHeight`) - pattern coerente con `Ztoryc/LightColor`. Altezza
  audio **condivisa**: resize di una traccia le ridimensiona tutte e persiste.
- **Densita' label audio progressiva**: a scalare sparisce prima il nome file
  (`<kAudioShowNameMinH=45`), poi anche la barra del volume (`<kAudioShowVolMinH=36`);
  al minimo restano solo i bottoni L/M/S. Hit-test del volume disattivato quando la
  barra e' nascosta (no drag fantasma).

### Fixed
- **Cursore resize sull'audio track non appariva in hover**: l'audio gestisce il
  cursore via `QEvent::HoverMove` in `event()` (WA_Hover, piu' affidabile di
  `setMouseTracking` dentro `QScrollArea` su macOS), non via `mouseMoveEvent`.
  Aggiunto il check del grip li', con priorita' sul cursore di trim dei bordi.
- **Nome shot nella traccia video** ora centrato verticalmente (`AlignVCenter`
  invece di `AlignBottom`) -> resta leggibile a qualsiasi altezza traccia.
- **Label timecode che si ammucchiavano in zoom**: `kMinLabelPx` era fisso a 40px
  (ok per i numeri di frame, troppo stretto per `MM:SS:FF`/`H:MM:SS:FF`). Ora misurato
  con `QFontMetrics` sulla label piu' larga visibile (frame piu' a destra) + 14px gap ->
  l'intervallo tra label (serie 1/2/5/10/25/50...) si dirada da solo. Formatter `fmtFrame`
  spostato prima del calcolo dello spacing.

## [2026-06-20] — Export to Spreadsheet (production tracking Kitsu-aligned, XLSX+CSV)

### Added
- **Matrice tecnica→task nel modello** (`ztorymodel.h/.cpp`): `TaskStatus` (TODO/READY/WIP/WFA/
  RETAKE/DONE), `TaskState{status,assignee}`, `Technique{name,taskTypes}`. `ShotData` +=
  `technique`, `QMap<QString,TaskState> tasks`, `notes`, `vfxNotes`. ZtoryModel += `m_episode`,
  preset tecniche editabili + helper. Persistito nel `.ztoryc` (path attivo
  `StoryboardPanel::saveZtoryc/loadZtoryc`): `<project>` episode/defaultTechnique, blocco
  `<techniques>`, per-shot `technique` + `<task>` + `<shotNotes>`/`<shotVfxNotes>`.
- **Tecniche preset** (Kitsu-aligned, per-shot, editabili): Tradigital, Traditional (scan&clean/
  ink&paint/x-sheet), Cut-out, 3D/CGI, Stop-motion (Set-up→…→Rig Removal), Generic (tutti i task),
  Live (ridotto). Bottone toolbar Board **Set Technique** (multi-selezione) + colonna Workflow.
- **Export Spreadsheet XLSX** (`onExportSpreadsheet`) via **QXlsx 1.5.1 vendorato** (MIT,
  `thirdparty/QXlsx`, CMakeLists minimale Qt5): foglio **All Shots** (tutti gli shot, unione task,
  N/A grigio dove non pertinente) + **uno sheet per tecnica usata** (solo i suoi task). Per riga:
  thumbnail primo panel, seq, shot, frames, Sec/Fr (durata), Workflow, Notes (dal primo panel),
  VFX Notes; per ogni task colonna status (colore Kitsu via **conditional formatting** che segue
  il valore) + assegnatario. Dropdown status + **autofilter** (header).
- **Export Spreadsheet CSV** (`onExportSpreadsheetCsv`) per import in sistemi di production mgmt.
- **Menu File ▸ Export ▸ Ztoryc**: Storyboard PDF, Spreadsheet XLSX, Spreadsheet CSV, Shots/Scenes,
  Animatic (comandi globali che instradano al Board).
- **Campo Episode** in creazione scena (`startuppopup`) + dialog/testata PDF.

### Fixed (patch su QXlsx vendorato — cercare "Ztoryc:" nei commenti)
- Conditional formatting: le regole via API non avevano `priority` (tutte 0 → Excel/LibreOffice
  ne tenevano una sola). Ora `priority=i+1` nel saveToXml.
- Aggiunto `Worksheet::setAutoFilter()` (QXlsx non aveva autofilter) + emit `<autoFilter>` nell'ordine
  schema corretto, + defined-name `_xlnm._FilterDatabase` per LibreOffice.
- Thumbnail: `insertImage` è 0-based (vs `write()` 1-based) → immagini shiftate; fix `insertImage(row-1,0)`
  + DPI 96 pinnata (renderXsheetFrame può tornare immagine retina 2×).

### Notes
- Terminologia: rinominato "worksheet"→"spreadsheet" in tutto il codice Ztoryc (non nei token QXlsx).
- Limite: cambiare Workflow nel foglio non aggiorna gli N/A (export statico) → Workflow reso read-only;
  la tecnica si setta in-app. I colori status invece sono dinamici (CF).
- **DA FARE prossima sessione** (richiesto a fine 2026-06-20): (1) tecnica di default nelle opzioni
  di creazione scena; (2) voce "Storyboard Settings" nel menu per editare produzione/episodio/titolo/
  tecnica/numbering dopo la creazione. Dettagli in memory `project_worksheet_export`.
- Tecniche riviste = solo su scene NUOVE (le scene esistenti hanno i preset salvati nel .ztoryc).

## [2026-06-19b] — Task 53 shot ops in edit-shot mode + Task 54 logo custom PDF + footer branding

### Added
- **Task 53 — Shot ops dall'interno di una sub-scena (edit-shot mode)** (`ztoryanimatic.cpp`).
  Le 6 shot ops dell'Animatic (Copy/Clone/Cut/Paste/Delete/Merge) erano bloccate silenziosamente
  da `if (!ZtoryModel::assertMainXsheet(false)) return;` quando si era dentro uno shot. Rimosso il
  guard e garantito che ogni op operi sul main xsheet:
  - **Copy/Clone** (non distruttivi): leggono da `scene->getChildStack()->getTopXsheet()` senza
    chiudere la sub-scena → l'utente resta in edit-mode.
  - **Cut/Paste/Delete/Merge** (strutturali): chiudono la sub-scena (`while ancestorCount>0 →
    MI_CloseChild`) poi operano sul main, esattamente come fanno già le op del Board. Cut/Paste
    avevano già il loop di chiusura (era solo dopo l'assert, irraggiungibile); a Delete aggiunto
    il loop; Merge usava già `getTopXsheet()`, aggiunto il close-children + rimosso l'assert.
- **Task 54 — Logo custom + metadata nell'export PDF** (`storyboardpanel.cpp`, `ztorymodel.h`).
  Premendo Export PDF appare un dialog "Export Storyboard PDF" con:
  - **Production** e **Title** editabili (prima nel modello ma senza UI) → finiscono nell'header.
  - **Header logo**: campo path + Browse (PNG/SVG/JPG/BMP) + Clear, checkbox "No logo (clean
    export)", **preview live** con warning ⚠ se il file non esiste.
  - Scelte persistite per-progetto nel `.ztoryc` (`<project>` attr `pdfLogo`/`pdfNoLogo`),
    ricaricate alla riapertura scena. Nuovi campi modello `m_pdfLogoPath`/`m_pdfNoLogo` +
    accessor; helper `resolvePdfLogoFile()` (assoluto o relativo alla cartella scena).
  - Logo header: custom valido → custom; rotto → fallback al logo Ztoryc (export non si rompe mai);
    "No logo" → header pulito.
- **Footer PDF branding sempre presente** (richiesta utente): il footer "Made with Ztoryc" + logo
  Ztoryc resta **indipendente** dalla scelta header (logo footer hardcoded su `ztoryc_about.png`).
  Aggiunto link al repo: `Made with Ztoryc  ·  github.com/matitanimata/ztoryc` (URL in colore link;
  risulta cliccabile nei viewer PDF testati dall'utente).

### Fixed
- **Anteprime Board non rigenerate dopo shot ops dall'Animatic** finché non si scrollava (il fix
  del 2026-06-19 copriva solo le op nate dal Board). Causa: quando l'op nasce dall'Animatic il
  Board non riceve un ciclo di paint/layout, quindi al `singleShot(0)` il `QGridLayout` non era
  ancora ricalcolato → `mapTo()` su geometrie stale → test viewport-intersect fallisce → niente
  render. Fix in `updateVisiblePreviews()`: `m_grid->activate()` forza il flush del layout
  pendente prima del test geometrico. Punto unico, copre tutti i chiamanti.

### Notes
- **Prossimo**: task 55 (altezza tracce video/audio regolabile) → 56 (Thumbnail Room, milestone).

## [2026-06-19] — Toolbar dedup Board↔Animatic + overflow scroll + UI polish + ottimizzazione task list

### Added
- **Dedup toolbar Board↔Animatic a livello UI** (commit `d194149ad`, su master). Riuscito dove
  il tentativo del 2026-06-17 era fallito, rispettando i vincoli della memoria: SOLO `setVisible`,
  niente `QToolBar`, niente hook su `showEvent`, niente delega add/delete cross-panel.
  - `MainWindow::updateZtoryToolbarDedup()` nasconde sul Board i 6 shot-ops
    (add/delete/merge/copy/clone/paste) + i 2 export (Shots/Animatic) quando un `ZtoryAnimaticPanel`
    è nella stessa room. Pilotato da `onCurrentRoomChanged`/`switchToRoom`/costruttore via
    `QTimer::singleShot(0)`, MAI dal `showEvent` del Board. Room col solo Board → tutto visibile
    (self-sufficient).
  - Toolbar Animatic riordinata: shot-ops a SINISTRA (sotto il Board), tool editing
    (fit/select/trim/razor/link) a destra.
  - **Export spostati sulla timeline** (spazio libero): Export Shots/Scene + Export Animatic
    sull'Animatic a destra, delegano al Board sibling; PDF resta sul Board.
- **Overflow scroll sulla toolbar del Board** via `DvScrollWidget` nativo di Tahoma: frecce
  laterali quando le icone non c'entrano, invece di tagliarle/ammucchiarle (stessa UX dei pannelli
  nativi). Rimosso l'`addStretch()` interno che avrebbe assorbito lo spazio.
- **Icone nuove**: `ztoryc_fit_all` (arrows-maximize) e `ztoryc_zoom` (lente). Fit All riportato
  accanto allo zoom slider; etichetta "Zoom:" sostituita dalla lente.

### Modified
- **Tracce video/audio**: Lock con icona lucchetto aperto/chiuso (`ztoryc_lock`/`_on`); Mute con
  icona speaker nativa di Tahoma (`sound_on`/`sound`); Solo resta "S" (paint dei track, non più
  drawText "L"/"M").
- **Show light direction + Show camera movement**: ora hanno lo stato `:checked` (background #666)
  come razor/select, sia Board che Animatic.

### Fixed
- **Anteprime Board non rigenerate dopo add/copy/paste/delete/cut/merge** finché non si scrollava.
  `refreshFromScene()` ricostruisce con thumbnail vuote (lazy by design per non freezare al load);
  ora in `onModelResequenced` (ramo count-cambiato, solo path operazione) un `singleShot(0)` →
  `updateVisiblePreviews()` rigenera SOLO i pannelli nel viewport (salta quelli già renderizzati),
  istantaneo e senza appesantire. Il path di caricamento scena resta lazy.

### Docs
- **ANIMATIC_TASKS.md ottimizzato**: 1213 → 611 righe. Pregresso (614 righe, verbatim) spostato in
  `ANIMATIC_TASKS_ARCHIVE_2026-05.md` (sezione giugno 2026): spec dei task DONE rimaste tra gli
  "aperti" (39,40,42,43,45,48,49,50, mark-out, ffmpeg, pdf-thumb, audio/volume/transizioni/startup/
  nav-tags), task 35/36/37 (assorbiti in 40), keys-cels 1-4, icon migration, blocco FATTO/DECLASSATO
  (51,52,StudioPalette). Tabella DONE aggiornata con 11 voci giugno (incl. `ICON-MIGRATION`).
  Regola adottata: completato → riga in tabella DONE + spec in archivio (basta voci `✅ FATTO`
  accumulate nella sezione attiva).

### Notes
- Branch `feat/toolbar-dedup-board-animatic` mergiato in `master` (fast-forward) e pushato su origin.
- **Prossimo**: task 53 (shot ops in edit-shot mode, MEDIA-ALTA) → 54 → 55, e valutare 57. Task 56
  (Thumbnail Room) è lavoro a parte (milestone).

## [2026-06-17] — Wiring icone toolbar + fix add-shot xsheetColumn

### Added
- **Wiring icone ai 5 toggle** (commit `ef833e133`): snap (`m_snapBtn`, magnet/magnet-off —
  base = off, `_on` = on, `createQIcon` carica auto la variante `_on` per lo stato checked);
  light_arrow (bottone "Show light arrows" "L", sia Board che Animatic); cam_moves (Board "Trk");
  keys_follow (`MI_ToggleKeyframesFollowExposure`, era `segment_linked`); listen_audio
  (`MI_ToggleMainAudio`, era `sub_main_audio`). Rimosso il fondo colorato `:checked` da cam_moves
  e light_arrow.
- Rimossi file morti icone (`slip`, `onion`/`_on`, `shotedit`/`_on`, `snap_off`) + righe qrc;
  aggiunto `ztoryc_snap_on`.

### Fixed
- **`onAddShot` non aggiornava `xsheetColumn` degli shot successivi** (commit `ea1ab3490`).
  Inserendo uno shot in mezzo, la colonna inserita spostava a destra gli shot seguenti
  nell'xsheet ma `data.xsheetColumn` non veniva incrementato (a differenza di `onDeleteShot`
  che lo tiene sempre allineato) → cliccando uno shot dopo l'inserimento si apriva la
  sotto-scena sbagliata (es. ultimo → penultimo). **Da verificare:** l'utente conferma che la
  **v0.5 NON ha il baco**, pur avendo `onAddShot` byte-identico al tag → il trigger reale
  coinvolge un'interazione con una modifica post-v0.5 ancora **non tracciata**. Il fix è
  comunque corretto (allinea `onAddShot` a `onDeleteShot`). ⚠️ Indagare l'origine prima di
  considerarlo chiuso.

### Reverted / Notes
- **Dedup toolbar Board↔Animatic + delega add/delete dall'Animatic al Board: TENTATI e ANNULLATI**
  (regressioni: numero panel sballato). Tutto revertito. Lezione: il dedup deve toccare SOLO
  `setVisible` dei bottoni — mai convertire il container a `QToolBar`, mai agganciarsi a
  `showEvent`/timer. Vedi memoria `project_toolbar_dedup_ui`.
- **Trappola single-instance:** i sintomi "catastrofici" osservati durante le prove (disegni
  spariti, scene nuove rotte, 030→020) erano il **binario vecchio bacato ancora in esecuzione**
  (single-instance guard rifocalizza invece di riavviare); codice/binario erano già puliti
  (verificato con `nm`). Regola: dopo ogni deploy **Cmd+Q + riapri**. Vedi memoria
  `feedback_deploy_single_instance_restart`.
- `onMergeShots`/`onMergeWithNext` Animatic: NON toccati (regola AGENTS "già corrette").

## [2026-06-16b] — Increase/Decrease chiavi + refactor selezione combinata

### Fixed
- **Pannello-fantasma da 1 frame nel paste (e ricalcoli durata).** Lo Stop Frame Hold
  che `resequenceXsheet()` mette in coda a ogni colonna shot è una cella non vuota
  (`isEmpty()` controlla solo `m_level`) e veniva contato come frame: `refreshFromScene`,
  `detectAndUpdatePanels` (entrambi i rami), `onXsheetChanged`, `onShotInserted`
  gonfiavano la durata di +1 → il filtro di visibilità pannelli (`f < timelineDuration`)
  lasciava passare un boundary → pannello-fantasma. Fix: escludere la SFH dal conteggio
  (`getRange(..., ignoreLastStop=true)` / break sullo stop-frame), allineati a
  `onModelResequenced`. File `storyboardpanel.cpp`.

### Added
- **Increase/Decrease spaziatura chiavi (key-only)** su `TKeyframeSelection`
  (`keyframeselection.cpp`): aggiunge/toglie un frame per ogni intervallo tra chiavi
  consecutive; prima chiave ancorata, chiavi successive che **slittano** (ripple via
  `moveKeyframes`); Decrease no-op se un gap è già 1; blocco riselezionato dopo
  l'operazione (ripetibile). Agganciato a MI_IncreaseStep/MI_DecreaseStep. Undo
  `KeyframeSpaceUndo`.
- **Increase/Decrease combinato celle+chiavi** (`cellkeyframeselection.cpp`): in
  modalità combinata NON ripete i disegni — inserisce/rimuove un frame negli intervalli
  tra le chiavi via `insertCells`/`removeCells` (celle+chiavi slittano insieme,
  pref-independent). Single-point su una cella sola → un frame nel punto selezionato;
  sul primo frame del disegno il disegno cresce in testa (no frame vuoto). Undo
  `KeyframeGapResizeUndo` con snapshot completo per colonna.

### Refactor
- **`TCellKeyframeSelection` ora EREDITA `TCellSelection`** (tolto `final`). Con
  keys-follow ON ogni selezione celle diventava la selezione combinata che NON era una
  `TCellSelection` → gli ~82 `dynamic_cast<TCellSelection*>` fallivano e i comandi cella
  (drawing substitution W/Q, filtri, reframe…) ricadevano sulla singola cella. Ora il
  cast riesce → operano sull'intero blocco. La combinata È la cell selection (possiede il
  range); ctor prende solo `TKeyframeSelection*`. Ogni override (copy/cut/clear/paste,
  increase/decrease) ha fall-through al comportamento cella base quando non ci sono chiavi
  (o keys-follow OFF) → modalità normale invariata. Aggiornati `xsheetviewer.cpp` e
  `cellselectioncommand.cpp` (ctor).

### Notes / TODO
- Aggiunto **task PRIORITARIO** in ANIMATIC_TASKS.md: finalizzazione UI dedup toolbar
  Board↔Animatic (bottoni condivisi su timeline Animatic sinistra, Board liberato per
  auto/keep/renumber + numbering + light arrow + export; nascondere duplicati a runtime
  via `findChildren<TPanel*>`, panel sempre self-sufficient).
- Task 5 (drag sui diamanti → selezione combinata quando pref ON) ancora aperto; il
  refactor di oggi è infrastruttura utile in quella direzione.

## [2026-06-16] — Dedup comandi shot Board↔Timeline (ZtoryShotOps) + cleanup UI

### Added
- **`ztoryshotops.{h,cpp}` — modulo condiviso `ZtoryShotOps`** con la logica xsheet
  pura dei comandi shot: `syncChildCameraToMain`, `cloneChildToPosition`,
  `pasteSharedClip`, `colDuration`. Prima erano duplicate come static file-local in
  `storyboardpanel.cpp` e `ztoryanimatic.cpp` e si erano **disallineate** (il clone
  del Board usava `storeObjects(ids)`+`setDagNodePos`, quello dell'Animatic
  `storeColumns(indices)` senza dagNodePos). Canonico = versione Board (battle-tested).
  Entrambi i panel ora delegano. Net Step 1: -314/+245.

### Modified
- **Board: clipboard shot ora SOLO `ZtoryModel::sharedClip()`** — rimosso il membro
  locale `m_clipboard` + struct `ClipboardEntry` e il ramo "legacy" di `onPasteShot`
  (127 righe). `onCopy/onCut/onCloneShot` scrivono solo lo shared clip; `onPasteShot`
  delega a `ZtoryShotOps::pasteSharedClip` + resequence + refreshFromScene +
  `UndoBoardState`, identico a `onPasteShots` dell'Animatic. Net Step 2: -230/+63.
  **Cambio di consistenza voluto:** un secondo Paste di un Cut non duplica piu (Board
  allineato all'Animatic).
- **Rimosso il chip blu "ANIMATIC"** dalla toolbar della timeline (coerente con la
  pulizia dei chip decorativi; toolbar ora parte da "Zoom:").
- **Main Toolbar nascosta di default** (`ShowMainToolbarAction` 1->0) - la barra
  window-top (new level / Reframe 1's-2's-3's / audio) duplicava la QuickToolbar
  dell'xsheet e ingombrava le room storyboard. Resta riattivabile da
  View > Show Main Toolbar. La QuickToolbar dell'xsheet/timeline e invariata.
- **README**: rimosso "Brush feel" dalla Roadmap (declassato).

### Verified (a video)
- copy/cut/clone/paste same-panel (Board): Copy→Paste→Paste (copia persiste),
  Cut→Paste→Paste (2o no-op); cross-panel Board→Animatic e Animatic→Board (Cmd+C/Cmd+V);
  clone Animatic (chiavi camera + colonne preservate); undo; sync Board↔Animatic.

### Notes / TODO
- **Bug pre-esistente (NON il dedup):** un paste puo lasciare un pannello-fantasma da
  1 frame su uno shot vicino (`refreshFromScene`/`resequence` fa crescere la colonna).
  Verificato dal diff: non ho toccato quella logica e il paste normale gia passava da
  `refreshFromScene` su master → si riproduce anche su master. Da fixare a parte.
- **TODO UI:** togliere la toolbar finestra "nuovo livello/1's2's3's/repeat" in Animatic
  e de-doppiarla in Shot editing (file `room*.ini` in bundle + ~/Library; le room nel
  bundle hanno nomi default che non corrispondono a Ztoryc X/T → identificare i file
  giusti prima).

## [2026-06-15c] — Time Stretch combinato celle+chiavi + UI cleanup titoli panel

### Added
- **Time Stretch COMBINATO celle+chiavi (keys-follow ON)** — completa il gruppo
  ritempi rimandato. Selezionando un blocco di celle con keyframe (modalità
  "Keyframes Follow Exposure"), il Time Stretch ora ritempra **sia le celle sia le
  chiavi** in modo proporzionale (caso walk cycle 18↔24). Nuova
  `CellAndKeyframeStretchUndo` (`timestretchpopup.cpp`) che **compone**
  `TimeStretchUndo` per le celle e gestisce le chiavi esplicitamente: si lascia che
  `xsh->timeStretch` rippli tutto (corretto per le chiavi *sotto* il blocco), poi si
  sovrascrivono **solo le chiavi del blocco** `[r0,r1]` con il rimappamento
  proporzionale (estremi ancorati). Niente doppio-handling (classe BUG-2). Undo/redo
  atomici celle+chiavi. Snapshot del blocco preso PRIMA dello stretch.

### Fixed
- **`Old Range 0` nel Time Stretch con celle+chiavi selezionate.** Quando si
  selezionano celle che contengono keyframe la selezione corrente è una
  `TCellKeyframeSelection` (wrapper nativo Tahoma con dentro una cell-selection +
  una keyframe-selection), non una `TCellSelection`. Il `TimeStretchPopup` faceva un
  `dynamic_cast<TCellSelection>` che falliva → range 0 e nessuno stretch.
  **Fix**: helper `asCellSelection()` che spacchetta il wrapper alla cell-selection
  interna, usato in `updateValues` (display) e nella free function `timeStretch`
  (range + colonne + trigger del path combinato). Diagnosi con logging mirato su
  stderr (typeid della selezione), poi rimosso.

### Modified
- **UI cleanup — titoli ridondanti nei panel compositi.** Rimosso il chip verde
  decorativo "BOARD" (`storyboardpanel.cpp`) e azzerato il window title nei 3 panel
  con switcher — `ZtoryRightPanel` (SCRIPT|PALETTE), `ZtoryLeftPanel` (BOARD|XSHEET),
  `ZtoryDrawLeftPanel` (BOARD/SHOT) — sia nel costruttore che nelle factory
  (`ztoryanimatic.cpp`). Lo switcher row già etichetta il panel; la barra del titolo
  resta come drag-handle per il docking (vincolo autosufficienza room custom). I
  panel a scopo singolo (Animatic/Viewer/StoryStrip/Navigator) non toccati.

### Notes / TODO
- **Selezione combinata governata dal link "Keyframes Follow Exposure" [DESIGN, da fare].**
  Modello richiesto dall'utente: con pref ON, *qualsiasi* selezione (incluso il drag
  sui diamanti) deve produrre una `TCellKeyframeSelection` (chiavi + celle
  sottostanti); con pref OFF, selezione indipendente (chiavi isolabili). Oggi metà
  funziona già (selezione celle → combinata); manca l'altro verso (selezione dai
  diamanti → combinata quando pref ON). Cambio trasversale alla logica di selezione
  dell'xsheet (xsheetviewer/cell viewer), impatta TUTTI i comandi combinati → da fare
  e testare sull'intero repertorio.
- **Dedup comandi Board↔Timeline [refactor] ora sbloccato** (BUG-1/BUG-2 chiusi):
  estrarre la logica shot condivisa in un punto unico, panel thin che delegano.
  Primo passo: audit grep dei comandi duplicati.

## [2026-06-15b] — BUG-1 (drag cross-colonna keys+cels) + Gruppo A operazioni key-only

### Fixed
- **BUG-1 [keys+cels] — drag cross-colonna ora trasferisce i keyframe.** Trascinando
  un sotto-blocco di celle con keyframe su un'altra colonna (modalità "Keyframes
  Follow Exposure"), le chiavi seguono le celle al nuovo stage object. Implementazione
  **unificata** in `CellKeyframeMoverTool` (`xsheetdragtool.cpp`): rimosso del tutto il
  keyframe-mover *live* dal tool combinato — le celle si muovono live (LevelMover), le
  chiavi vengono trasferite **al rilascio** dove le celle sono atterrate (posizione già
  validata da `canMove`, quindi righe libere → atomico). Questo elimina la transizione
  fragile `revertMove` che corrompeva la cell-selection (stessa classe di BUG-2,
  diagnosticata con log su 3 giri). Collisione di riga esatta sulla destinazione →
  blocco dell'intero drag (celle incluse); convivenza permessa se le righe target sono
  libere. Nuova `CrossColumnKeyframeUndo` (snapshot via `TKeyframeData`). Il drag di
  colonna intera → dest vuota (path clone "Cells and Column Data") resta invariato,
  rilevato a runtime via `keysStillAtSource` per non fare doppio spostamento.

### Added
- **Gruppo A — operazioni timing su selezione di SOLE chiavi (`TKeyframeSelection`).**
  Routing via `enableCommands`, tutte con undo dedicato, in `keyframeselection.cpp`:
  - **Reverse** — specchia `r→r0+r1-r` (involuzione, undo==redo).
  - **Swing** — appende la coda specchiata (ping-pong); undo ripristina la coda.
  - **Roll Up / Roll Down** — rotazione ciclica entro `[r0,r1]`; undo = direzione opposta.
  - **Repeat** — appende il pattern N volte (aggancio a `DuplicatePopup` per la selezione
    chiavi). Nuova opzione **Loop** (visibile solo con chiavi selezionate): sovrappone la
    chiave di giunzione (passo `r1-r0`) per cicli seamless senza doppia posa.
  - **Time Stretch** — rimappamento **proporzionale** `r→r0+round((r-r0)·(N-1)/(old-1))`,
    estremi ancorati (caso walk cycle 18→24). Aggancio a `TimeStretchPopup` + abilitazione
    `MI_TimeStretch` in `enableCommands`. Undo con snapshot dell'intero span interessato.
- Step/Each/Reframe **archiviati** per le sole chiavi: non hanno semantica sensata su
  keyframe interpolati (decidono esposizione di disegni tenuti).

### Notes / TODO
- **Time Stretch su selezione combinata celle+chiavi RIMANDATO** a sessione dedicata con
  build di debug + lldb: lo stretch celle (`xsh->timeStretch`) con keys-follow ON shifta
  le chiavi in modo uniforme (non proporzionale) → doppio-handling sul path keys+undo
  (classe BUG-2). Serve undo-class dedicata (snapshot originale + stato intermedio).
- Reverse mantiene gli ease di ogni chiave (non flippa le tangenti in/out) — come il
  Reverse celle. Eventuale rifinitura futura.

## [2026-06-15] — BUG-2 (perdita chiavi su undo) + drag celle: modificatori nativi

### Fixed
- **BUG-2 [perdita dati] — l'undo del cut perdeva i keyframe (modalità keys+cels).**
  In `cutCellsKeyframes()`, con "Keyframes Follow Exposure" ON, `cutCells()` →
  `removeCells()` cancellava già i keyframe nello span tagliato; il successivo
  `deleteKeyframesWithShift()` faceva lo snapshot a chiavi già sparite
  (`data->m_keyData=0`) → `DeleteKeyframesUndo` non aveva nulla da ripristinare.
  **Fix** (`cellkeyframeselection.cpp`): cancellare i keyframe con
  `deleteKeyframes()` (senza shift) PRIMA di `cutCells()`; lo snapshot cattura le
  chiavi e l'ordine di undo si inverte correttamente (CutCellsUndo ripristina
  celle+shift, poi DeleteKeyframesUndo ri-incolla le chiavi). Rimuove anche un
  latente doppio-shift. Diagnosi con debug build + lldb (probe Python su
  `TKeyframeData::m_keyData`). Commit `73f1a5d64`.

### Changed / Added
- **Drag celle: ripristinata la convenzione modificatori nativa di Tahoma.**
  Rimossa l'intercettazione di Alt+drag che attivava il Cell/Block Swap: ora il
  drag celle è gestito da `LevelMoverTool` — drag = move, Shift = insert,
  Alt = overwrite, Ctrl = copy (comportamenti già presenti in upstream ma
  "coperti" dallo swap Ztoryc).
- **Cell/Block Swap spostato su Ctrl+Alt+drag** (Cmd+Option su macOS): non ruba
  più il modificatore overwrite; nessun conflitto con il rolling cel (regioni
  diverse — drag bar vs smart-tab extender). Il block swap ora funziona anche
  afferrando il corpo della cella (prima il fallback forzava una cella sola).
  Commit `c5095c6a7`.
- **Tooltip hint** sui modificatori del drag celle, mostrato sulla drag bar
  (nomi tasti per piattaforma: Cmd/Option su macOS, Ctrl/Alt altrove).

### Notes
- **BUG-1 rimandato** (drag cross-colonna in modalità combinata keys+cels →
  trasferimento keyframe al nuovo stage object). È una feature sostanziosa sullo
  stesso path delicato keys+undo di BUG-2: da fare a sessione fresca con verifica
  lldb. Accertato che la preference "Cell-dragging Behaviour: Cells and Column
  Data" già muove l'intero stage object (keyframe inclusi) per colonne intere su
  destinazione vuota; manca il caso del **sotto-blocco** in modalità combinata,
  bloccato da `KeyframeMoverTool::canMove` (col != startCol).
- Dead-code del CellSwapper NON rimosso (swap tenuto, su Ctrl+Alt).
- Pulizia repo: rimossi branch effimeri (`feature/keys-cels-modes` già mergiato,
  e 2 worktree `claude/*` con relativi branch).

## [2026-06-14] — Keys Follow Exposure: operazioni Cels portano i keyframe + toggle visibile

Branch `feature/keys-cels-modes` (NON ancora su master).

### Fixed
- **Undo della cancellazione keyframe nel Level Extender (shrink)** — accorciare il
  timing di un blocco cancellava i keyframe nel tail rimosso e l'undo non li
  ripristinava. Fix a livello di comando in `LevelExtenderUndo` (`xsheetdragtool.cpp`):
  snapshot dei keyframe del blocco all'onClick (`m_savedKeys`, gated su `m_followExposure`)
  + restore nel path undo (`insertCells()`) limitato al tail `[r0,r1]`. Testato OK.
- **Popup Repeat… grigio con selezione combinata** — `DuplicatePopup` faceva
  `dynamic_cast<TCellSelection>` che fallisce su `TCellKeyframeSelection` → campi/bottoni
  disabilitati. Nuovo helper `getCurrentCellSelection()` estrae la cell-selection interna
  (`duplicatepopup.cpp`). ⚠️ Stesso pattern da verificare su Time Stretch….

### Added
- **Keys-follow per le operazioni del menu Cels** (toggle "Keyframes Follow Exposure" ON) —
  i keyframe seguono il rimappamento delle celle. Tutto in `txsheet.cpp`, gated sulla
  preference, undo simmetrico via primitive. Testato OK:
  - **Reverse** — mirror involutivo `r→r0+r1-r` (`ReverseUndo` undo==redo).
  - **Roll Up/Down** — rotazione ciclica; chiave di bordo salvata e riposizionata
    (prima cancellata da `removeCells`).
  - **Swing** — duplica `[r0,r1-1]` specchiato nel tail (`s→2*r1-s`), pivot escluso.
  - **Repeat** — duplica il chunk di keyframe su ogni copia.
- **Toggle visibile in toolbar** — `MI_ToggleKeyframesFollowExposure` ora ha icona
  `segment_linked` (`mainwindow.cpp`): aggiungibile alla Quick Toolbar dell'xsheet via
  "Customize Quick Toolbar" → Misc, mostra stato premuto/rilasciato.

### Notes
- **Classificazione completa menu Cels** in `KEYS_CELS_MODES_DESIGN.md`: estendibili
  (rimappano il tempo) vs no (cambiano contenuto/marker playback). Loop Frames resta
  fuori (righe virtuali, no celle reali) → eventuale "loop transform" è feature a parte.
- **Gruppo ritempi TODO** (per sessione fresca): Step/Each/Time Stretch richiedono
  snapshot/restore keyframe *dentro le Undo class* (ripristinano celle wholesale, non via
  primitiva inversa). Reframe: semantica keyframe da decidere. Mappature già definite nel
  design.
- Toggle di default nella Quick Toolbar per tutti: eventuale, via `buildDefaultToolbar`.

## [2026-06-13] — task 52 declassato, autofill antialias indagato, brush-feel audit

### Added
- **BrushProfiler** (`toonz/sources/include/brushprofiler.h`) — strumentazione
  latenza header-only, zero-cost senza flag, attivabile con
  `ZTORYC_BRUSH_PROFILE=1`. Instrumenta `leftButtonDrag` (dab_compute),
  `paintGL` e la latenza end-to-end evento→paint; stampa min/med/max/avg ogni
  120 campioni su stderr. Punti in `sceneviewer.cpp`, `sceneviewerevents.cpp`,
  `toonzrasterbrushtool.cpp`.

### Investigated / Declassed
- **Task 51 — Brush feel → DECLASSATO** (da ALTA strategica a feature opzionale).
  La premessa "feel < TVPaint" era un'assunzione di Fable 5, mai misurata.
  Audit fase 1 col BrushProfiler su tavoletta reale (153 blocchi × 120 dab):
  dab_compute med **0.08 ms**, paintGL med **0.27 ms**, evt→paint med **2.18 ms**
  (sotto un frame @60Hz). La pipeline software NON è il collo di bottiglia:
  repaint già incrementale (invalidateRect→clipRect→glScissor), tablet events
  non compressi, nessun lavoro estraneo nel drag. Fasi 2-4 (stabilizzatore,
  preset, curve MyPaint) restano solo come feature UX on-demand.
- **Task 52 — crash palette Shift+N → DECLASSATO**: non riproducibile né su
  debug build + lldb + MallocScribble (path esercitato 70× senza crash, handle
  sempre azzerata correttamente) né sull'ESATTO binario release che crashò l'11
  (TUndoManager hardening già incluso). Heisenbug state-dependent, famiglia del
  crash StudioPalette declassato l'11. CrashHandler a presidio.

### Notes
- **AutoFill bordino bianco antialiased (smart raster) — IRRISOLTO, baseline safe
  ripristinata.** 5 approcci provati, tutti regrediti sui pennelli morbidi:
  (1) dipingere pixel ink adiacenti → sborda; (2) + guardia no-esterno → sborda;
  (3) delega al fill() nativo → leak totale (soft brush senza core tone=0);
  (4) tone-march `<=` → ricopre tutto; (5) tone-march `<` → colora tratti aperti.
  Causa radice: nessuna regola locale per-pixel distingue versante interno/esterno
  della linea sui soft brush (gradiente di tone esteso). Strada giusta (da fare
  OFFLINE, testata su raster sintetici): logica scanline-direzionale di
  `calcFillRow` confinata alla regione BFS. Lezione salvata in memoria Claude.
- Lezione trasversale: misurare/riprodurre prima di patchare ha evitato di
  inseguire problemi inesistenti (52, 51); l'errore è stato spedire pezze autofill
  in live invece di testarle offline.

## [2026-06-10b] — New Shot After Current, fix task 49/50, README, release v0.4.1

### Added
- **New Shot After Current (Shift+N)** — comando globale MI_ZtoryNewShotAfter:
  da dentro una sub-scene chiude, crea lo shot subito dopo quello in editing
  e ci entra direttamente (l'artista resta nello SHOTEDITOR col pennello in
  mano). Dal Board/Animatic aggiunge dopo la selezione senza entrare.
  Rimappabile da Configure Shortcuts. Bottone "+" anche sullo Shot Board
  (room T); il "+" della timeline animatic dentro una sub-scene ora delega
  al comando invece del warning "main xsheet only".
- Timeline animatic si ricostruisce su modelReset anche con sub-scene aperta
  (refreshFromScene legge il TOP xsheet) — prima restava stale dopo Shift+N.

### Fixed
- **Task 49 — scatto sul secondo tratto di disegno** (commit e7676d620):
  il detect timer (1s) partiva a ogni xsheetChanged in sub-scene e scattava
  a metà del tratto successivo (detect + render thumbnail sincroni sul thread
  UI corrompevano la linea). Ora xsheetChanged marca solo m_dirtyShotCol;
  detect+render solo su frameSwitched / ritorno al Board / showEvent, più
  guardia mouse-premuto sul timeout.
- **Task 50 — panel fantasma dopo undo che svuota uno shot**: verificato
  risolto (test utente) dopo i fix task 48 + TUndoManager hardening.

### Modified
- **README riscritto** (f8791f070, 4d3a625c8): sezione Download, feature per
  room (camera-move, light gizmo, Arrows, burn-in, import sceneggiatura…),
  4 screenshot nuovi (animatic+board, 2× shot editing, pagina PDF con camera
  move), roadmap ridotta a Kitsu.
- ANIMATIC_TASKS: chiusi 49/50; rimossi come già-fatti 21 (volume audio),
  24 (startup hub), frame handle separato (risolto dal Monitor); TRADITIONAL
  declassata; nuovo task 51 Brush feel (ALTA, strategica) con piano in 4 fasi
  (audit latenza → stabilizzatore → preset → MyPaint). NO codice da Krita
  (GPL); libmypaint già in casa.

### Release
- **v0.4.1** — bump ZtorycVersion.cmake, prima stabile della linea 0.4.
  Diff da v0.4.0-beta.2: light gizmo (fase 3) Board+Shot Board, burn-in
  export, New Shot After Current, fix task 48/49/50, TUndoManager hardening
  (candidato upstream), parità Board↔Shot Board, README nuovo.

### Notes
- gh CLI: risolti i 401 intermittenti — c'erano 4 voci keychain duplicate
  `gh:github.com`; pulite tutte + login fresco. Un solo token ora.

# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).
> Voci più vecchie di ~2 settimane → spostarle in `CHANGELOG_ARCHIVE.md`.

---

## [2026-06-11] — sessione stabilità: TUndoManager hardening, crash palette declassato

### Fixed
- **TUndoManager use-after-free (core, candidato upstream)**: se un comando
  eseguito DENTRO `TUndo::undo()` rientra nel manager (es. chiusura sub-scene
  → push di `CloseChildUndo`), `doAdd`/`beginBlock`/`reset` cancellavano il
  ramo redo che CONTIENE l'oggetto in esecuzione → l'oggetto proseguiva su
  memoria liberata (root cause a monte del wipe task 48). Fix in
  `tcore/tundo.cpp`: la cancellazione dell'entry in esecuzione è deferita alla
  fine di `undo()`/`redo()` (`m_executing`/`m_deferredDelete` +
  `safeDeleteUndo`); guard anche su `++it` in `redo()` dopo rientranza.

### Notes
- **Crash palette DECLASSATO**: non riproducibile su build debug sotto lldb
  nemmeno con MallocScribble (torture test: load studio palette dopo delete
  livello, undo spam, switch sub-scene). Teoria confermata dai fatti: era un
  derivato del bug undo-wipe (restore rotto → livelli distrutti → palette
  corrente dangling), fixato il 2026-06-10. Si tiene il workaround note e il
  CrashHandler a presidio; togliere dal radar salvo recidiva.
- Build di debug permanente in `/Volumes/ZioSam/tahoma2d-workspace/debug-build`
  (cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5, stesse
  dipendenze homebrew). Script lldb repro in /tmp/ztory_lldb_script.txt.
- Nuovo task minore 50: panel fantasma nel Board dopo undo che svuota uno shot
  (si riallinea a enter/exit della sub-scene).

---

## [2026-06-10] — light direction (task 40 fase 3), burn-in export, fix undo/lag

### Added
- **Gizmo Light Direction (task 40 FASE 3 — COMPLETA il task 40)**: freccia
  conica 3D per-panel nel Board. Bottone ☀ → drag sul thumbnail (coda =
  sorgente), rotella = inclinazione Z (a ±100% notazione ⊙ verso camera /
  ⊗ verso fondo), Shift+rotella = apertura fascio (12–90°, hard/soft light).
  Cono con silhouette tangente all'ellisse di base, occlusione corretta
  front/back, basi piatte di profilo (roundness = sin(tilt)), shading assiale
  (luce che cade lungo la freccia). Fascio conico traslucido + sole + readout
  gradi solo durante il drag. Right-click rimuove. Toggle visibilità (bottone
  L + shortcut L), swatch colore temperatura. Persistenza `.ztoryc`
  (lightTail/Tip/Depth/Spread/Color), undo via UndoBoardState, anche su PDF.
  Commit `d498c010b`.
- **Burn-in export animatic (stile Storyboard Pro)**: nel dialog Export
  Animatic gruppo "Burn-in" — timecode di sequenza (basso-dx), nome
  SQ_SH_P per-panel (alto-sx), checkbox Clapperboard (mirror live del Board
  setting nativo). Architettura: `TRasterImageUtils::addBurnIn` (toonzlib) +
  `MovieRenderer::setBurnIn` pinnato al setup (stesso pattern del fix audio
  task 43) + `ZtoryBurnInConfig` in ZtoryModel letta da rendercommand.cpp.
  Bottone "Render Settings…" nel dialog con riepilogo formato live (poll
  700ms); estensione file riletta alla conferma.

### Fixed
- **Dialog Export Animatic bloccava l'Output Settings popup**: era
  WindowModal (blocca tutta la catena del main window). Ora non-modale con
  QEventLoop locale.
- **Undo svuotava lo storyboard** (parziale — vedi Known issues):
  `captureSnapshot()` usava l'xsheet CORRENTE → dentro una sub-scene snapshot
  con level tutti nulli → `restoreFromSnapshot` rimuoveva tutte le colonne e
  salvava `.ztoryc` vuoto. Fix: snapshot sempre dal TOP xsheet + guardia che
  rifiuta snapshot senza alcun level valido.
- **Lag disegno con auto-match attivo**: ogni burst di tratti eseguiva resize
  colonna + resequence + refresh anche a durata invariata. Aggiunta guardia
  no-op in `onMatchSubsceneDuration`.

### Fixed (addendum sera — task 48 RISOLTO)
- **Undo-wipe definitivo (use-after-free)**: con Cmd+Z da DENTRO una sub-scene,
  `restoreFromSnapshot` eseguiva `MI_CloseChild` → `closeChild` pusha un
  `CloseChildUndo` → `TUndoManager::add()` durante un undo() attivo tronca il
  ramo redo e DISTRUGGE l'`UndoBoardState` in esecuzione → la reference al suo
  `m_before` (lo snapshot) diventava dangling → re-insert leggeva memoria
  liberata → tutti i level "nulli" → Board svuotato. Diagnosi via logging
  [ZTORY] su repro reale. Fix: deep-copy dello snapshot come prima istruzione
  (le TXshLevelP della copia tengono vivi anche i livelli). In più: firewall in
  saveZtoryc (mai persistere .ztoryc con 0 shot se l'xsheet ha colonne child) e
  logging diagnostico permanente su refreshFromScene/restore/resequence.

### Known issues (prossima sessione — DEBUG BUILD + LLDB)
1. **Crash palette** — `StudioPaletteCmd::loadIntoCurrentPalette →
   TPalette::assign → destroy mappa stili` SIGBUS su puntatore garbage
   (use-after-free, famiglia task 42 residuo). Report:
   `Ztoryc-2026-06-10-112817.ips`.
2. **TUndoManager UB residuo**: quando `add()` distrugge l'undo in esecuzione,
   il manager rientra in un oggetto liberato — oggi innocuo in release, ma è
   UB e possibile parente dei crash random. Da verificare sotto lldb.
3. **Lag sui primi tratti** (anche senza auto-match) — già risolto in versioni
   pre-merge, forse fix perso col merge 1.6.1: cercare nel CHANGELOG_ARCHIVE.
   Candidati: m_panelDetectTimer/refresh thumbnail su xsheetChanged in sub.

### Notes
- Burn-in testato e funzionante (raffinamenti rimandati); clapperboard da
  verificare con Board configurato nei render settings.

---

## [2026-06-09] — v0.4.0-beta.2: libreria frecce, overlay camera-move, fix crash

### Added
- **Pannello Arrows** (ex "Camera Moves"): libreria di frecce vettoriali `.pli`
  per indicazioni di movimento personaggi/camera. Picker a thumbnail da cartella
  bundled (`stuff/library/directional arrows`, 16 frecce) + cartella personale
  opzionale (QSettings). Click → stampa la freccia nella colonna "Annotazioni"
  della sub-scena, con **colori originali** preservati (mergeImage + palette dal
  livello PLI), strokes raggruppate, inserimento **frame-aware** (nuova cel su
  cella vuota o in hold con "Enable Creation in Hold Cells").
- **Overlay camera-move FASE 2** sui thumbnail Board + PDF: render "backed-out"
  che copre START e STOP (le pan non vengono più tagliate), rettangoli + frecce
  d'angolo (notazione classica), lettere **A→B continue** tra i panel dello shot,
  toggle "Trk" per le label tipo movimento (persistito), label PDF a ~6pt.

### Fixed
- **Crash Geometric tool** aprendo una sub-scene (doppio-click shot in Animatic):
  `GeometricTool::onDeactivate()` dereferenziava `m_viewer` nullo. Diagnosi lldb.
- **Disallineamento view-mode** Animatic↔Shot (Camera Stand vs Camera View): il
  viewer attivo ora segue sempre il button set condiviso, niente toggle manuale.
- **Trk In/Out** invertiti nell'overlay (scala su = Out, giù = In); le label si
  auto-correggono al caricamento di scene vecchie.

### Upstream candidates (Tahoma2D)
- Null deref in `GeometricTool::onDeactivate()` (bug core).

### Notes
- Workspace unificato (merge merge-1.6.1 in master). Release via workflow
  (publish_release + release_tag v0.4.0-beta.2), note bilingui poi `gh release edit`.

---

## [2026-06-08] — consolidamento workspace + fix crash Geometric tool (task 42)

### Modified
- **Workspace unificato**: mergiato il branch `merge/upstream-nightly` in `master`
  (task 40 fase 1, PSD fix, PDF template, hints + CrashHandler thread-safe ora tutti
  insieme). Worktree `merge-1.6.1` e branch `merge/upstream-nightly` dismessi.
  Risolto conflitto in `storyboardpanel.cpp` (frammento stale) e duplicazione
  `fps`/`framesToTC` nell'export PDF post-merge.

### Fixed
- **Task 42 — CRASH Geometric tool aprendo sub-scene** (`geometrictool.cpp`):
  doppio-click su uno shot nell'Animatic con il Geometric tool attivo →
  SIGSEGV. `GeometricTool::onDeactivate()` chiamava `m_viewer->getDevPixRatio()`
  in modo incondizionato; durante l'apertura sub-scene la catena
  `ToolHandle::onImageChanged → setTool → onDeactivate` disattiva il tool prima
  che fosse agganciato a un viewer → `m_viewer == NULL` → null deref
  (EXC_BAD_ACCESS @ 0x90 dentro getDevPixRatio). Fix: il viewer serve solo per
  il rendering nel ramo `m_isRotatingOrMoving`, ora è guardato `if (m_viewer)`.
  Diagnosi con **lldb sulla repro reale** (il crash log della release mostrava
  "due frame setTool+316" fuorvianti — simbolicazione approssimata).

### Notes
- Il caso "raster brush" del task 42 era già coperto dal guard `m_inColorStyleChanged`
  (commit `8f8740628`). Verificati non vulnerabili anche StyleEditor/PaletteViewer
  (caso C non crasha). Il task 42 resta da affrontare con debug+lldb solo se emergono
  altri percorsi (palette-switch ri-entrante puro).
- **Candidato upstream Tahoma2D**: il null deref in `GeometricTool::onDeactivate()`
  è un bug del core, riproducibile anche fuori da Ztoryc se onDeactivate scatta con
  viewer nullo. Valutare PR.

## [2026-06-06] — fix CrashHandler da render thread; verifica ImageManager leak

### Fixed
- `crashhandler.cpp`: `CrashHandler::trigger()` non mostra più la dialog quando
  chiamato da un thread non-main (render thread). Su macOS, creare `NSWindow`
  da un background thread lancia un'ObjC exception → abort secondario che
  mascherava il crash reale. Ora il log file viene sempre scritto; la dialog
  appare solo se si è sul main thread. Fix applicato in entrambi i repo
  (`tahoma2d/` e `merge-1.6.1/`).

### Notes
- Crash render analizzato: root cause `TLevelColumnFx::doCompute` → `TRasterFx::applyAffine`
  null ptr (0x14) dopo 2.5h render con 17.9 GB MALLOC (Plastic + PSD). Causa
  profonda: memoria esaurita durante render lungo. Fix `be20f9512`
  (invalidateAllCached post-render) e `b79ba7d32` (memoryShortage macOS 14.3%)
  sono già presenti e attivi in entrambi i branch — nessun ulteriore intervento
  necessario. Workaround: renderizzare in batch più piccoli su scene molto lunghe.
- Analizzato anche crash Tahoma2D 1.6.1 su macOS 26.5 (beta): EXC_CRASH SIGKILL
  Code Signature Invalid in dyld — non è un bug Ztoryc, è firma codice non valida
  su macOS 26.5 beta con enforcement più stretto. Fix per lo sviluppatore Tahoma:
  `codesign --deep --force --sign - Tahoma2D.app`.
- Task 43 (export animatic a/b/c/d), 45 (status bar hints) — confermati risolti.
- Task 46 (explode peg inutili) — chiuso, comportamento intenzionale Tahoma2D.

## [2026-06-04] — PDF storyboard template, hints, fix audio/crash

### Added
- **Task 45 — Status bar hints contestuali** (gold `#d4a017`):
  - Board: hover su panel → hint workflow Board vs Animatic; hover toolbar buttons → hint specifico per ogni bottone
  - Animatic: hint tool-aware in `mouseMoveEvent` (SelectTool, TrimTool roll/ripple, RazorTool); `leaveEvent` pulisce
  - XSheet: `setStatusTip` su Roll Up/Down, Autofill, Auto Fill checkbox (BrushToolOptionsBox)
  - Nuovi metodi `StatusBar::showZtoryHint` / `clearZtoryHint` + helper `TApp`
- **PDF Export — template professionale**:
  - Header: logo Ztoryc, Production, Title, Page X/Y
  - 3 panel per pagina (1 riga), celle full-height; sub-header con shot label + durate
  - Griglia senza gap: linea grigia tra panel stesso shot, nera+spessa tra shot diversi
  - Footer: logo piccolo + "Made with Ztoryc"
- **Metadati Production/Title**: campi in `ZtoryStartupDialog` + `StartupPopup`; persistiti in `.ztoryc`; fix race condition con `refreshFromScene`

### Fixed
- **PDF timecode fps**: leggeva fps fisso 24; ora da `scene->getProperties()->getOutputProperties()->getFrameRate()`
- **Camera keyframe singolo**: non crea più panel boundary in `detectAndUpdatePanels`
- **Audio +1 frame nelle scene esportate**: `getRange()` ora con `ignoreLastStop=true` in `onExportShots`
- **Crash al quit/workflow-switch (OpenGL static destructor)**: `signalHandler` tentava QDialog su Qt già distrutto. Fix: flag `s_appExiting` su `aboutToQuit` → `_Exit(0)` silenzioso
- **"sub-scene" → "shot"**: tooltip Auto Match Duration e Match Duration button

### Notes
- Plastic drawing invisibile (mesh visibile): intermittente, non riproducibile. Da investigare con debug build.

---

## [2026-06-03] — Merge upstream nightly ✅ SU MASTER + 🚀 RELEASE v0.4.0-beta.1

> 🚀 **Rilasciata `v0.4.0-beta.1`** (prima beta sulla base mergeata) — binari pubblicati su
> GitHub Releases per tutte le piattaforme: macOS Intel + Apple Silicon (DMG), Windows
> (install .exe + portable .zip). Note bilingui IT/EN applicate, marcata prerelease.
> Workflow: `gh workflow run macOS_build.yml/windows_build.yml -f publish_release=true -f release_tag=...`
> (NON via git tag). Note in `~/ZtorYc/v0.4.0-beta.1_notes.md`.
>
> **Fix/migliorie della giornata post-merge (tutti su master):**
> - 🔴 crash al salvataggio (`CleanupParameters`/`~TPalette`, `LastSavedParameters` morta) — risolto
> - crash Set Key Plastic Tool (`m_sd` null in `keyFunc_undo`) — risolto
> - default Ztoryc: Ease In/Out (era Linear), drag bars ON, implicit hold OFF
> - rebranding Preferences + shortcut dialog (Tahoma2D→Ztoryc, lasciati import + crediti)
> - confermati risolti DAL merge: asterisco/save dal main in storyboard; vari refresh
>
> **Task loggati per dopo (ANIMATIC_TASKS.md):** 45 status-bar hint contestuali; 46 explode
> sub-scene crea peg inutili (pre-esistente); 47 audio scrub meno reattivo nel viewer normale
> (il fix widen-scrub 150ms è SALVO in txsheet.cpp:2121 — regresso in native scrub/flipconsole timing).
> **Crash noti rari (beta known issues):** keyframe move durante save concomitante; Plastic storeDeformation.

> ✅ **Mergiato su `master`** (fast-forward `b4aff742f..9d88943cf`, push origin master).
> Lavorato su branch `merge/upstream-nightly` + worktree isolato, poi portato su master.
> Crash keyframe (Drawing Number) rimandato a **hotfix** futuro (raro, non ripresentato).

### Merged
- **Merge `upstream/master` (nightly, post-v1.6.1)** in `master`.
  - merge-base: `c8b768aa3` (10 mar 2026). Ztoryc 333 commit / upstream 154 (46 fix, 10 feat).
  - **Solo 17 conflitti**, hotspot (mainwindow, txsheet, xshcellviewer, tooloptions,
    flipconsole) **auto-mergeati**. Conflitti risolti: 15 infra/branding → `ours`;
    `CMakeLists.txt` → ours (scartato `set(VERSION 1.6.1)`, Ztoryc usa `ZtorycVersion.cmake`);
    `cellselectioncommand.cpp` → **upstream** (il loro fix "set key" su peg column rende
    ridondante il mio PR candidate).
  - **2 fix post-merge** (`b2ca7abb4`): (a) `onStageObjectChange` — upstream ha aggiunto
    `bool isDragging` alla virtuale, allineata la firma dell'override Ztoryc; (b) `maintoolbar.h/.cpp`
    riaggiunti al CMakeLists (persi col `--ours`, causavano undefined `MainToolbar::MainToolbar`).
  - **Build pulita** + deploy isolato funzionante (`merge-1.6.1/toonz/Ztoryc.app`, `ZTORYC_WORKSPACE`).
- Novità upstream verificate presenti e funzionanti: **Render Settings** (ex Output Settings),
  **Sync with Play Range**, Animate tool (Reset Center/Set Key/Pick mode), Master Toolbar,
  Drawing Number/Mark.

### Fixed (post-merge)
- ✅ **CRASH Set Key Plastic Tool** (`plastictool_animate.cpp`, commit `9d88943cf`) — le azioni
  Set Key / Set Rest Key (nuove di upstream per il Plastic) erano raggiungibili via shortcut
  anche senza deformazione attiva (`m_sd == nullptr`) → SIGSEGV in `keyFunc_undo →
  m_sd->getKeyframeAt()`. Aggiunta guardia `if (!m_sd) return;`. **Candidato fix upstream.**
  Era l'unico crash *introdotto* dal merge (gli altri sono pre-esistenti).

### Hotfix da fare (non bloccanti — merge già su master)
- 🔴 **CRASH muovendo keyframe nella xsheet** (`Crash-20260602-161810.log`) — SIGSEGV in
  `TStageObject::setKeyframeWithoutUndo → TDoubleParam::setKeyframe`, via `KeyframeMover::moveKeyframes`.
  **NON è regressione del merge**: `keyframemover.cpp` è identico a upstream puro (mai toccato da Ztoryc).
  È un bug upstream nightly: muovendo un keyframe su **colonna di livello** in una scena **esistente**
  (`scsh120.tnz`, salvata pre-Drawing-Number), il canale nuovo `T_DrawingNumber` viene copiato con dati
  non inizializzati → crash. Il fix noto upstream `cf52a1776` ("moving non-column based channel keys")
  è già incluso ma non copre questo path. **TODO**: (1) ripro su scsh120 vs scena nuova; (2) guard/init
  difensivo del canale Drawing Number al load o in `setKeyframeWithoutUndo`. File: `tstageobject.cpp`,
  `tparam.cpp` (TDoubleParam::setKeyframe).

### Notes
- Bug pre-esistenti (non da merge) osservati: (a) glitch refresh monitor panel (tracce video/audio,
  solo scene vecchie finora); (b) entrando in uno shot il viewer ignora il view mode corrente e parte
  in camera view invece di camera-stand (serve toggle per refresh). Entrambi da loggare/indagare.
- Integrazione fine "Sync with Play Range" ↔ marker In/Out dell'animatic: il sync si aggancia al drag
  via `xsheetdragtool`; nell'animatic i marker passano per `subscenecommand::setPlayRange`, quindi il
  sync potrebbe non scattare lì. Da collegare esplicitamente se serve.

### Altri crash/bug emersi durante l'uso (pre-esistenti, NON da merge)
- 🟡 **Crash Plastic Tool intermittente** (`Crash-20260602-172557.log`, sull'app PRE-merge) —
  SIGSEGV in `PlasticTool::onColumnSwitched → storeDeformation → onSelectionChanged` (ricorsivo)
  attivando il tool via shortcut. Raro ma l'utente usa il Plastic tantissimo. Le funzioni del crash
  NON sono toccate dal merge (upstream cambia solo la parte "animate Set Key" del Plastic, 315 righe),
  quindi il crash persiste anche nel merge. Ipotesi: deref durante cambio colonna/attivazione su stato
  transitorio. File: `plastictool.cpp`.
- 🟡 **Disallineamento mesh↔skeleton Plastic — TRANSITORIO** — animando una gamba la mesh non seguiva
  lo skeleton (bone "fuori" dalla mesh). ⚠️ CORREZIONE: **NON dipende dall'import come sotto-scena**
  (reimportando la libreria torna a funzionare) → i dati NON sono corrotti, è uno stato transitorio.
  Ipotesi più probabile: **glitch di refresh/invalidazione del viewer** — la mesh deformata non viene
  ridisegnata in sync con lo skeleton; reload forza il ridisegno. Coerente con gli altri problemi di
  refresh osservati (monitor panel; view mode all'ingresso shot che parte in camera view). Possibile
  pattern comune: **invalidazione viewer mancante entrando/lavorando nelle sotto-scene** in Ztoryc.
  Da indagare: refresh/invalidate del viewer Ztoryc su enter sub-xsheet, deformazione Plastic.

### Ripresa (sessione nuova)
1. Test riproducibilità crash keyframe (scsh120 vs scena nuova)
2. Fix crash Drawing Number su scene pre-esistenti
3. `git checkout master && git merge merge/upstream-nightly` → push origin master → rsync → rebuild app principale
4. (minori) crash Plastic intermittente + disallineamento mesh in sotto-scena (entrambi pre-esistenti)

---

## [2026-06-02] — CLONE shot: fix perdita keyframe camera (e pegbar)

### Fixed
- **CLONE shot perdeva i keyframe di camera** (`storyboardpanel.cpp`,
  `ztoryanimatic.cpp`). Causa radice in `restoreCamera()`
  (`stageobjectsdata.cpp`): ogni child xsheet creato da `createChild()` ha già
  una Camera 0 di default; ripristinando la camera via `StageObjectsData` il
  codice la trova "occupata" e crea una **Camera 1 fantasma** con i keyframe,
  lasciando la Camera 0 (usata per il rendering) vuota → chiavi apparentemente
  perse. Due path affetti:
  - **Board** (`cloneChildToPosition`): includeva `CameraId(0)` negli ids →
    camera fantasma.
  - **Animatic** (`animCloneChildToPosition`): usava `storeColumns()` che
    **omette del tutto** la camera → nessun keyframe copiato.
- **Fix**: dopo `restoreObjects`, copia manuale degli stage object non-colonna
  (camera + pegbar) sull'oggetto con lo **stesso id** già esistente nel clone
  (`assignParams` keyframe + copia `TCamera` + parent). Mirror della logica
  stock `cloneXsheetTStageObjectTree()` (in namespace anonimo, non richiamabile
  da fuori). Esteso ai **pegbar** così anche i rig cutout mantengono
  l'animazione.

### Notes
- **COPY shot** verificato OK: condivide lo stesso `TXshChildLevel`
  (sub-scene condivisa) → camera = stesso oggetto, nessuna perdita possibile.
- Il razor/split usa lo stock `ColumnCmd::cloneChild` (già corretto via
  `cloneXsheetTStageObjectTree`) — non toccato.
- Non coperto: camera su spline (motion path) — caso edge raro; i keyframe di
  posizione/rotazione/scala/zoom sono comunque coperti.

---

## [2026-06-02] — Mark In/Out persistenti per-xsheet (tutti i workflow)

### Fixed
- **Mark In/Out delle sotto-scene non persistevano dopo save+reload**
  (`txsheet.h/.cpp`, `subscenecommand.cpp`, `iocommand.cpp`). Causa: il play
  range "live" è in `scene preview properties`, ed è **globale e unico** —
  condiviso tra main xsheet e tutte le sotto-scene. La vecchia `s_frameRangeMap`
  era runtime-only (chiave `TXsheet*`, invalidata al reload). Inoltre il
  preview-range globale veniva serializzato così com'era: se l'ultimo range
  attivo era quello di una sotto-scena (o si salvava da dentro una sotto-scena),
  al reload il **main ereditava** quel range (bug nidificazione: con main/-1/-2
  il main si ritrovava i marker di -2).
- **Fix strutturale — marker In/Out per-xsheet persistiti nel `.tnz`:**
  - `TXsheet`: nuovi campi `m_markerIn`/`m_markerOut` (-1 = unset), API
    `get/setInOutMarkers()` + `hasInOutMarkers()`, serializzati in
    `saveData`/`loadData` con tag `<inOutMarkers>` (assente nei file vecchi →
    default unset, retrocompatibile).
  - `openSubXsheet` (scendendo): salva il range dell'xsheet che si lascia sia in
    cache sia nei marker persistenti (`prevXsh->setInOutMarkers`). Risalendo:
    priorità di ripristino = marker persistenti → cache di sessione → fallback
    auto (durata shot + cross-dissolve XD).
  - `closeSubXsheet`: scrive i marker dell'xsheet che si chiude nel TXsheet +
    dirty flag.
  - `IoCmd::saveScene`: sincronizza il play range live → marker dell'xsheet
    **corrente** prima di serializzare (copre il caso "imposto un range e salvo
    senza entrare/uscire da una sub").
  - `IoCmd::loadScene`: il **main xsheet diventa autoritativo** — il play range
    live viene inizializzato dai marker del main (o disabilitato se assenti),
    sovrascrivendo il preview-range stantio ereditato da una sotto-scena.
    Elimina il leak alla radice. Funziona in **ogni workflow** e a qualsiasi
    profondità di nidificazione.

### Upstream candidates
- **Per-xsheet In/Out markers** — la parte `txsheet.h/.cpp` (campi + API +
  serializzazione `<inOutMarkers>`) è pulita e proponibile a Tahoma2D così
  com'è. Gli agganci in `iocommand.cpp`/`subscenecommand.cpp` sono la logica di
  sync; la parte cross-dissolve (XD-in/XD-out) resta Ztoryc-specifica. Era già
  in lista come feature request — ora c'è un'implementazione di riferimento.

### Notes
- `ztorymodel.cpp` includeva una modifica **pre-esistente non committata**
  (pinning del play range a `[0, lastFrame]` in `resequenceXsheet`, + include
  `xsheetdragtool.h`) — inclusa in questo commit.
- **Bug aperto (bassa priorità)**: importando un `.psd` da Affinity (40 layer,
  blocco `Lr16` 16-bit, nomi layer vuoti) come libreria personaggio, e poi
  caricando quella scena come **sotto-scena** in un'altra, il layer più in basso
  risulta "not found". La scena originale aperta direttamente è OK; anche
  importare il PSD direttamente in una sotto-scena è OK. Da indagare
  (`tiio_psd.cpp` `REF_LAYER_BY_NAME`/`getLevelIdByName`, `psd.cpp` blocco
  `Lr16`). Workaround utente: aggiungere un layer sacrificale come primo (più in
  basso). Da loggare in `ANIMATIC_TASKS.md`.

---

## [2026-05-31] — Windows Storyboard startup crash fix + audio flicker + autoMatch perf + Render Tile default + workflow anti-flicker + Task 40 FASE 1

### Fixed
- **Flicker tracce audio durante operazioni** (`ztoryanimatic.cpp/.h`) —
  `refreshAudioTracks()` distruggeva e ricreava tutti i widget traccia ad ogni
  modello cambiato (anche aggiungendo/clonando shot, che NON tocca l'audio),
  causando lo sfarfallio. Fix: fast-path che matcha le tracce esistenti per
  puntatore `TXshSoundColumn*` (stabile agli shift di indice colonna) e aggiorna
  in-place (`setColumnIndex` + `invalidateWaveform`) senza ricreare i widget.
  Rebuild completo solo se la struttura delle colonne audio cambia davvero.
- **Lentezza con autoMatch attivo** (`ztoryanimatic.cpp`) — il loop che cerca
  la colonna corrispondente alla sub-scena aperta era O(col × righe): controllava
  ogni frame di ogni colonna ad ogni `xsheetChanged`. Fix: O(col) — controlla solo
  la prima cella per colonna (tutte le celle di un shot puntano allo stesso livello).
- **Crash avvio Windows entrando nel workflow Storyboard** (`storyboardpanel.cpp`)
  — `m_scrollArea` usava la policy default `ScrollBarAsNeeded`. L'altezza dei
  `PanelWidget` dipende dalla larghezza (preview con aspect-ratio): la scrollbar
  verticale che appare/sparisce cambia la larghezza del viewport → larghezza
  pannello → altezza pannello → ri-toggle scrollbar, oscillando. Su Windows
  l'eventFilter interno di `QScrollArea` (setWidgetResizable) gira sincrono e
  ricorsivo → stack overflow → uscita silenziosa + dump 0 byte. Si manifestava
  solo alla geometria transitoria d'avvio (Storyboard scelto dal popup iniziale;
  passare alla room dopo, a finestra dimensionata, non crashava). Fix: pinnare le
  policy (verticale AlwaysOn = larghezza costante, orizzontale AlwaysOff). Stessa
  classe del fix già presente sui QTextEdit del file. Diagnosi confermata via
  backtrace `QScrollArea::eventFilter ↔ resize` ×446 + repro tester. Commit `5af4a994f`.
- **Sfarfallio grafico al cambio workflow** (`mainwindow.cpp`) — `switchRoomChoice`
  fa `clearRooms()`+`readSettings()` e `Room::load()` chiama `processEvents()`,
  quindi ogni stato intermedio delle room veniva dipinto (le "ghost windows"
  ansiogene, evidenti su Windows). Fix: `setUpdatesEnabled(false/true)` attorno
  alla ricostruzione → si dipinge solo lo stato finale.
- **NON modificata la soglia RAM cache** (`tsystempd.cpp`) — un tentativo di
  alzarla 14.3%→25% per contenere l'uso RAM su scene pesanti rendeva l'eviction
  troppo aggressiva e **crashava il Save All** (raster liberato durante
  `TRasterCodecLZO::compress`). **Revertito** al 14.3% shipped. L'ottimizzazione
  RAM va rifatta in modo mirato (task 41) senza toccare l'eviction durante i save.
- **BUG-CAMERA — dati stale in SB_APPENNINGERS.tnz** — 4 sub-scene avevano
  `cameraSize: 12 6.75` (valore baked prima del fix). Corretto via script Python
  → tutte e 46 le camere ora `16 9`. Backup `.tnz.rtkcam` conservato.
- **BUG-CAMERA — confermato risolto nel codice** — test su scena nuova: camera
  main e sub-scena corrispondono perfettamente anche cambiando F e Z. Era un
  problema di dati stale, non di codice.

### Added / Changed
- **Render Tile default = Small** (`outputproperties.cpp`) — il default era
  `None` (frame intero), che su scene di minuti fa gonfiare la cache immagini
  oltre la RAM fisica fino allo swap (osservato: 17 GB su Mac da 16 GB durante
  un render full). Small tiene basso il footprint per-operazione. Solo per scene
  nuove; le esistenti mantengono il valore salvato nel `.tnz`.
- **Task 40 FASE 1 — Pannello Camera Moves** (`ztoryannotations.h/.cpp`) —
  Pannello "Ztoryc Camera Moves" nel menu Panels → Ztoryc con 8 pulsanti
  freccia Pan (4 ortogonali + 4 diagonali). Crea automaticamente una colonna
  PLI "Annotazioni" nella sub-scena corrente (con `sl->setScene(scene)` prima
  di `setFrame` per evitare crash). Frecce vettoriali centrate nel frame,
  editabili con tool di selezione nativo.

### Reverted (regressione introdotta e annullata in sessione)
- **Patch crash palette/style ritirate** — durante la sessione ho tentato di
  fixare il crash `StyleEditor::onStyleSwitched` (famiglia palette-switch
  ri-entrante) con modifiche a `TPaletteHandle` (riferimento forte), guard di
  re-entrancy in `onStyleSwitched`, `PaletteViewer::onFrameSwitched`,
  `toonzrasterbrushtool`, `tooloptions`. Queste patch hanno introdotto una
  **regressione** (crash al disegno anche su scene nuove) e sono state **tutte
  revertite** al baseline. Lezione: non patchare a scatola chiusa un cascade core
  complesso deployando build a metà.

### Notes
- **Crash StyleEditor (pre-esistente) ancora aperto** — re-entrancy nella cascata
  `updateXshLevel → setPalette → editLevelPalette → setPalette → onStyleSwitched`
  (palettecontroller.cpp:65). Da affrontare con build di debug + lldb, NON a
  tentativi. Il crash inseguito in sessione era in gran parte causato dalle mie
  patch parziali già deployate: al baseline la scena non crasha più.
- **Task 40 REDESIGN** — FASE 2 riprogettata: annotazioni camera-move automatiche
  leggendo i parametri del Camera1 pegbar (X/Y→Pan/Tilt, Z→Truck, Scale→Zoom).
  Overlay sul thumbnail BOARD, non colonna PLI separata. Vedi ANIMATIC_TASKS.
- **Regola memoria: aggiungere pannelli al menu Panels richiede 4 file** —
  `tpanels.cpp` (OpenFloatingPanel), `menubar.cpp` (sottomenu Ztoryc hardcoded),
  `stuff/profiles/layouts/menubar.xml`, e
  `~/Library/Application Support/Ztoryc/Ztoryc/profiles/layouts/menubar.xml`.
  Documentato in memory/feedback_ztoryc_panel_menu.md.

---

## [2026-05-31] — Release v0.3.5 + crash fix brush + diagnosi BUG-CAMERA

### Released
- **Ztoryc v0.3.5** pubblicata (Windows + macOS via CI) con tutto il lavoro sotto.
  Release note con sezione macOS `xattr` + raccomandazione clean install Windows.
  Bump `ZtorycVersion.cmake` 0.3.4 → 0.3.5 (`5fb3ea3e7`).

### Fixed
- **Crash SIGSEGV al doppio-click su uno shot col brush raster attivo** (`8f8740628`)
  — `onShotDoubleClicked → openSubXsheet → switch xsheet/palette →
  ToonzRasterBrushTool::onColorStyleChanged()` rientrante su stato di tratto pendente
  (`m_tileSaver` ancora set, immagine sbagliata post-switch) → use-after-free.
  Aggiunta guardia `m_inColorStyleChanged`.

### Notes (da riprendere)
- **BUG-CAMERA RIAPERTO (priorità ALTA)** — A/B test (baseline `e8d4a1466`) ha
  confermato che l'animatic-camera ≠ shot-camera **NON è una regressione recente**:
  è il design del fix `7d1746f3a` (animatic forzato sulla camera MAIN). Prima del fix
  l'animatic usava la camera SUB e combaciava (ma SH010 off-screen, offset x=13.4).
  Fix corretto da fare: usare la camera SUB risolvendo il compositing sub→parent di
  SH010, senza ri-ancorare al main. Diagnosi completa in ANIMATIC_TASKS.
- **Crash StyleEditor su click shot** (`Crash-20260531-015855`) — famiglia
  palette-switch, diversa da quella del brush (non coperta dal fix). Da indagare.
- **Bug QScrollArea layout-recursion (Windows)** e **UX camera-view editing** —
  segnalati in ANIMATIC_TASKS.

---

## [2026-05-31] — Transizioni cross-dissolve + fix timeline animatic + autofill undo/only-new

### Added
- **Transizioni cross-dissolve** (`7ccba5721`, `6c245442e`) — handle Alt+drag sul
  bordo destro di uno shot nella timeline animatic imposta la durata della
  dissolvenza; triangoli arancioni visualizzano l'overlap su entrambi i lati +
  etichetta frame. Inserisce fisicamente T/2 frame extra in coda alla sub-scena A
  e T/2 in testa a B (frame ID nel main xsheet shiftati di +T/2 così l'animatic
  mostra ancora il contenuto originale). Colonne note SoundText `XD-out`/`XD-in`
  marcano i frame extra in SHOTEDITOR e sono la **fonte di verità** persistente:
  triangolo e mark-out si derivano contandole, quindi sopravvivono al reload senza
  dipendere dal timing del `.ztoryc`. Mark-out esteso per coprire i frame della
  dissolvenza (mark-in resta a 0). Durata board sempre al netto degli extra.
  `detectAndUpdatePanels` salta le colonne Sound/SoundText (niente panel spurio).
- **Multi-select tracce audio + group move** (`9699c06a7`) — Ctrl/Cmd+click
  seleziona segmenti su più tracce; trascinando un segmento selezionato tutti si
  spostano dello stesso delta, in un unico step di undo (`TUndoScopedBlock`).
- **Snap (magnete)** (`8a6ad68c5`) — toggle in toolbar animatic (default ON):
  i bordi trascinati di audio e blocchi video si agganciano (entro 8px) a confini
  shot, playhead e bordi di altri segmenti audio. Bottone con glifo "U" segnaposto
  (icona vera rimandata — le icone sono un capitolo a parte).

### Fixed
- **Zoom-to-cursor timeline** (`546c4712a`) — forza la larghezza del content prima
  di riposizionare la scrollbar così `setValue` non viene clampato al massimo
  pre-zoom: il frame sotto il cursore resta fisso.
- **Razor — vista non salta** (`546c4712a`, `39c37bc4f`) — sia il razor video che
  quello audio mantengono la posizione di scroll; il path audio (`onAudioRazorRequested`)
  era quello che faceva saltare a ~1690 (estensione audio gonfiata post-taglio).
  `refreshFromScene` ora preserva sempre lo scroll (copre paste/cut/delete/drag);
  reset a 0 solo allo switch di scena.
- **Incolla audio al playhead** (`39c37bc4f`) — Ctrl+V incolla alla posizione del
  playhead invece che sulla selezione di copia persistente.
- **Autofill undo/redo + solo forme nuove** (`25ad78f53`) — l'autofill del brush
  smart-raster: (1) undo lasciava artefatti di riempimento e redo perdeva il fill →
  nuovo `AutoFillUndo` dedicato (tile prima/dopo) raggruppato col tratto; (2) riempiva
  TUTTE le regioni chiuse ad ogni tratto (anche forme preesistenti vuote) → ora
  riempie solo le regioni il cui contorno tocca l'inchiostro del tratto corrente,
  rilevato via flood-fill delle regioni + adiacenza all'inchiostro dentro il
  **footprint del tratto preso dai tile dell'undo** (coordinate raster affidabili;
  `m_strokeRect`/`m_points` erano vuoti coi brush hard/pencil).

### Notes
- **Design task 40 approvato** — Sistema Annotazioni Camera-Move + Light Direction
  (unifica 35/36/37): simboli parametrici + libreria PLI, colonna vettoriale per
  shot, toggle render. Piano a 3 fasi salvato in ANIMATIC_TASKS.md (prossima priorità).

### Upstream candidates
- **AutoFill undo/redo fix** (`toonzrasterbrushtool.cpp`) — il fix dell'undo che
  lascia artefatti e del redo che perde il fill è applicabile a qualsiasi build con
  l'autofill brush. Da valutare per PR se l'autofill è feature condivisa.

---

## [2026-05-31] — BUG-CAMERA fix + audio scrub + marker timeline + sync selezione

### Fixed
- **Preset camera vuoto + prevenzione camere sub non standard (`09dc82463`)** — il
  combo preset in Camera Settings restava vuoto anche con camera = preset (es. HD
  1920x1080): `updatePresetListOm()` faceva solo il reset a `<custom>`, mai il
  forward-match. Ora cerca e seleziona il preset corrispondente (estratto in
  `presetMatchesFields()`, con fx/fy init a -1 per i preset a 3 token). Stesso
  forward-match aggiunto alla Startup popup su nuova scena. **Prevenzione causa
  radice BUG-CAMERA**: ogni path che crea una sub-scena nuova/vuota ora forza la
  camera della sub = camera del main (res+size). Logica estratta in
  `syncChildCameraToMain`/`animSyncChildCameraToMain` e applicata ai paste-fallback
  prima scoperti. Clone NON toccato (reincornicerebbe i keyframe), sub esistenti
  non modificate. Candidato PR upstream: il forward-match in `camerasettingswidget.cpp`.
- **Sequenze/numerazione non persistite nel `.ztoryc` (`871aea839`)** — `saveZtoryc()`
  salvava solo numero/label dei singoli shot: riaprendo la scena le sequenze
  sparivano (3 sequenze → sequenza unica). Ora il `.ztoryc` salva anche
  `<numbering>` (NumberingConfig: stile Simple/Sequence, prefissi, step, padding,
  seqNumber, resetOnSeqChange), gli elementi `<sequence>` (uuid/label/order) e il
  `sequenceId` di ogni shot. `loadZtoryc()` azzera le sequenze a inizio caricamento
  (no leak tra scene) e ripristina config/sequenze/sequenceId nel board e nel model;
  `renumberAll()` post-load preserva il raggruppamento. File vecchi retro-compatibili.
  ⚠️ Le scene salvate PRIMA del fix vanno ricreate+risalvate una volta.
- **BUG-CAMERA risolto** (`7d1746f3a`) — il monitor (viewer always-main) ora resta
  ancorato alla camera del MAIN xsheet anche dentro uno shot, invece di usare la
  camera della sub-scena. 6 punti in `sceneviewer.cpp`: `drawBuildVars()` (camera
  top-xsheet), skip dell'affine ancestrale per always-main inside-sub,
  `getViewMatrix()` CAMERA_REFERENCE (inverte la camera main, non la sub —
  risolveva il contenuto fuori schermo su SH010 con offset x=13.4),
  `fitToCamera()`/`fitToCameraOutline()` (rect dalla camera main), `getCameraRect()`,
  e `drawOverlay()` con `cameraRectAff = m_drawCameraAff * TScale(main/sub size)`
  per maschera/contorno/safe-area che `ViewerDraw::getCameraRect()` calcolava ancora
  sulla sub.
- **Scrub audio main da dentro lo shot muto (regressione, `06423030b`)** —
  `onNativeFrameSwitched` usava `scrubDevice()`, un `TSoundOutputDevice` grezzo mai
  aperto col formato audio → nessun suono. Routing su `mainXsh->play()`, stesso path
  dello scrub del ruler animatic (che funziona). Il play funzionava perché usa il
  device gestito di ogni colonna.
- **Delete audio track non funzionava** — `ColumnCmd::deleteColumns` opera sulla
  current xsheet (la sub dentro uno shot). Guard al livello main + undo +
  `notifyXsheetSoundChanged` + refresh esplicito. Menu tasto-destro sull'area
  etichetta della traccia audio.
- **Cursore roll-edit nell'xsheet non compariva** — `CellArea::mouseMoveEvent`
  reimpostava ArrowCursor ad ogni movimento. Guard `!getDragTool()` + `setCursor`
  SplitV/SplitH ai call-site del `LevelRollingTool` (Alt sul confine tra due celle).
- **Nomi colonne sub-scene = "SH010"** — `StoryboardPanel::updateColumnName` usava
  `scene->getXsheet()` (la sub se dentro uno shot). Cambiato in `getTopXsheet()`.

### Added
- **Task 39 — highlight shot attivo** (`043b5020b`) — entrando in edit-shot, il blocco
  attivo nella timeline animatic riceve glow magenta (`#E0249B`) + bordo 2px. Colonna
  attiva da `ChildStack::getAncestorInfo(0)->m_col`; repaint su `xsheetSwitched`.
- **Marker / navigation tag nella timeline animatic** (`7182b5543`) — i navigation tag
  della main xsheet disegnati nel ruler con la forma nativa Tahoma
  (`PredefinedPath::NAVIGATION_TAG`, pin a goccia). Etichetta solo in hover, click
  sinistro posiziona il playhead, tasto-destro Add/Edit/Remove. L'edit riusa il popup
  nativo `NavTagEditorPopup` (testo + colore). Persistono con la scena.
- **Sync selezione Board↔Animatic** (`7182b5543`) — `ZtoryModel::setSharedSelection`
  emette `sharedSelectionChanged()`; selezionare una clip nella timeline evidenzia lo
  shot nel Board e viceversa. Guardie no-op anti-loop.

### Upstream candidates
- Il cursore roll-edit (SplitV/H) e il guard `!getDragTool()` in `xshcellviewer.cpp`
  sono fix puliti riproponibili upstream.

### Notes
- `build_and_deploy.sh` riapre automaticamente l'app dopo il deploy.

---

## [2026-05-30b] — Findings BUG-CAMERA (tentativo monitor-white revertato)

### Reverted
- **Tentativo fix monitor-bianco (`5f335a295`) revertato in `6901cd844`** — era
  basato su una premessa sbagliata: credevo che TUTTI gli shot fossero bianchi nel
  monitor entrando in sub-scena, ma l'utente ha confermato che **solo il primo shot
  (frame 0) è sempre stato bianco**, sia prima che dopo. Rimuovendo l'affine
  ancestrale in `sceneviewer.cpp` non ho risolto il bianco e ho introdotto una
  discrepanza di inquadratura monitor vs viewer nativo. Ripristinato il codice
  originale (l'affine ancestrale fa combaciare il monitor con la sub-scena).

### Notes
- **Confermato dall'utente:** solo il **primo shot (frame 0)** diventa bianco nel
  monitor, comportamento preesistente e indipendente dai fix di questa sessione.
- I valori camera divergenti (F/Z) sono lo **Stage transform della colonna camera**
  (N/S/E/W/Z) — è **BUG-CAMERA** (camera main ≠ camera sub), preesistente. Findings
  dettagliati in ANIMATIC_TASKS.md. Da affrontare in sessione dedicata con test
  interattivo. NON ritoccare l'affine ancestrale in sceneviewer.cpp.

### BUG-CAMERA — diagnosi completa (analisi .tnz SB_APPENNINGERS)
- **Root cause confermato:** mismatch di `cameraSize` tra sub-scena e main. Main =
  16×9; su 59 sub-scene, 4 sono anomale (3× `12 6.75`, 1× `12 9`). SH010 è `12×6.75`
  con offset x=13.4 → nel monitor finisce fuori frame → bianco. Spiega "solo il
  primo shot bianco".
- **Decisione utente:** NON toccare i dati camera (la camera piccola è legittima,
  l'animatic puro la mostra giusta). Il fix è SOLO nel rendering del monitor: deve
  restare ancorato alla camera del MAIN anche dentro lo shot (come animatic puro),
  invece di scendere in edit-in-place sulla camera della sub.
- **Piano implementazione** (3 punti in sceneviewer.cpp: affine ancestrale, camera
  di riferimento, re-fit on scene-switch) salvato in ANIMATIC_TASKS.md → BUG-CAMERA.
  Richiede sessione dedicata con iterazione a test visivi su SH010.

---

## [2026-05-30] — Script per-scena: fix binding scena↔sceneggiatura

### Fixed
- **Script importato condiviso tra scene (BUG-SCRIPT-CROSS)** — lo script restava
  caricato cambiando scena e si mescolava tra progetti diversi. Due cause:
  1. **Posizione errata** — l'import scriveva in `+extras/script/` (livello
     progetto, condiviso da TUTTE le scene). Ma il progetto ha
     `<folder name="extras" useScenePath="yes"/>`: gli extras sono per-scena
     come i drawings. Ora l'import va in `+extras/<scena>/script/` replicando
     `getDefaultLevelPath()` (`+extras + getSavePath() + "script"`).
  2. **Load fragile** — il caricamento/clear dello script era un side-effect di
     `StoryboardPanel::loadZtoryc()` (dipendeva dall'esistenza/refresh della
     Board). `ZtoryScriptView` ora si connette direttamente a
     `TSceneHandle::sceneSwitched` e legge il tag `scriptFile` dal `.ztoryc`
     della scena corrente — autoritativo e indipendente dalla Board. Scena senza
     script → panel vuoto.

### Notes
- **Migrazione "solo nuovo schema"**: le scene vecchie mantengono il path
  `+extras/script/...` (risolve finché il file esiste); reimportare lo script lo
  sposta nella cartella per-scena. Nessuna migrazione automatica dei file esistenti.
- I file `.ztoryc` esistenti potevano avere tag `scriptFile` contaminati (3 scene
  puntavano allo stesso "Il Palazzo Scomparso v7.fdx") — risolto al re-import.

---

## [2026-05-29] — Mark-out fix, Monitor sub-scene guard, Clone camera keyframes + task reconcile

### Fixed
- **Mark-out main blocca play animatic (BUG-MARKOUT)** — la timeline animatic
  usava `XsheetGUI::getPlayRange()` (mark-out del native xsheet) in 4 punti di
  playback/audio. Se stale (impostato dentro una sub-scena o da sessione
  precedente) il play si fermava al frame sbagliato. Sostituito con
  `ZtoryAnimaticController::getAnimaticPlayRange()` (range proprio dell'animatic)
  in tutti e 4 i punti. Aggiunto `ZtoryAnimaticRuler::clampPlayRangeToTimeline()`
  chiamato dopo ogni `resequenceXsheet()`: riduce il mark-out se oltre la nuova
  durata (shots cancellati/accorciati).
- **Monitor track si azzera entrando nello shot** — `refreshFromScene()` chiamava
  `getTopXsheet()` senza guard `ancestorCount`; dentro una sub-scena restituiva
  la sub-scena e svuotava i blocchi della track animatic. Aggiunto guard
  `ancestorCount == 0` nel timer callback e nello `showEvent`.
- **Clone non copiava i keyframe camera** — `cloneChildToPosition()` usava
  `storeColumns()` che serializza solo `ColumnId`; la camera (`CameraId(0)`) non
  veniva mai memorizzata. Fix: `storeObjects()` con IDs espliciti incluso
  `CameraId(0)` → `restoreObjects()` chiama `restoreCamera()` → `assignParams()`
  copia tutti i keyframe (posizione, rotazione, zoom).
- **Cross-scene text contamination (BUG-TEXT-CROSS)** — già committato a inizio
  sessione: `m_currentZtoryPath` lega il save path al ciclo di vita di `m_shots`.

### Modified
- **ANIMATIC_TASKS.md riconciliato** — c'erano 3 file con date diverse (2205, 2305,
  canonico) e numerazione task incoerente. Pulito il canonico: marcati DONE i task
  32/33/34/30/31/25/26/27/28 + ffmpeg/PDF/Windows-installer (già fixati/superati).
  Aggiunti task aperti: 39 (feedback visivo shot editing), BUG-CAMERA (discrepanza
  camera main vs sub-scene), e BUG-MARKOUT (poi fixato). Priority order ora inizia
  da task 39 + BUG-CAMERA, poi 35 (Arrow Tool), 38 (Room Traditional), Kitsu.

### Notes
- **Task 25 In/Out Marker** marcato superato: approccio semplificato con `inPoint`
  fisso a 1, Roll/Slide funzionano via trim su `outPoint` (durata).
- **Windows installer** — crash utente (`onViewerDestroyed` entry point not found)
  era da mixed install: Ztoryc installato in `C:\Program Files\Tahoma2D\` con vecchie
  DLL T2D in PATH. Bug installer già fixato; workaround utente: disinstallare T2D prima.
- **Rimangono aperti:** task 39 (feedback visivo shot editing), BUG-CAMERA.

---

## [2026-05-27e] — Release v0.3.4: Monitor keyboard, thumbnails, version bump

### Added
- **ZtoryMonitorPanel — keyboard shortcuts** — Del/Backspace, Cmd+C/X/V/D/N attivi
  quando il track del Monitor ha il focus; ShortcutOverride per prevenire che
  CommandManager intercetti i tasti prima del panel.
- **Release checklist in AGENTS.md** — sezione permanente con procedura per release,
  istruzioni macOS `xattr -cr`, e regola "diff dal tag precedente prima di scrivere note".

### Fixed
- **Monitor delete button** — implementazione diretta senza delegare a
  `findAnimaticPanel()` (null se la stanza Animatic non era mai aperta). Undo via `UndoBoardState`.
- **ZtoryStoryStrip thumbnails vuote** — `renderXsheetFrame()` + cache per colonna
  (sostituisce `ZtoryModel::preview()` che restituiva null su sub-scene columns).

### Modified
- **Versione bump → 0.3.4**

### Upstream candidates
- **`thirdparty.cpp` ffmpeg autodetect** — ❌ NON candidato PR: bug specifico del bundle Ztoryc (`Contents/Resources/ffmpeg`). In Tahoma2D l'ffmpeg è nella stessa cartella dell'eseguibile e veniva già trovato — non affligge gli utenti Tahoma2D.
- **`tcodec.cpp` signal deadlock** — ✅ Candidato PR: `sigprocmask` attorno a `QProcess::start()` in `lzoCompress`/`lzodecompress`. Interessa tutti gli utenti Mac/Linux di Tahoma2D. Alta priorità.

### Notes
- Release note v0.3.4 approvate dall'utente prima del commit (diff-based, categorizzate).
- Regola stabilita: prima di release note, sempre `git diff <last-tag>..HEAD` per
  separare bug user-reported da fix interni mai arrivati all'utente.
- PDF quality fix incluso in v0.3.4 (300 DPI + pre-render + re-render a risoluzione cella).

---

## [2026-05-27d] — Fix cross-scene text contamination in Board

### Fixed
- **Cross-scene text contamination** — aprendo la scena 1 dopo aver editato la scena 2
  si ritrovavano i testi della scena 2. Root cause: `saveZtoryc()` usava `ztoryPath()`
  (scena attiva) mentre `m_shots` apparteneva ancora alla scena precedente, durante il
  window tra scene-switch e `clearShots()`. Fix: aggiunto `m_currentZtoryPath` che viene
  azzerato da `clearShots()` e impostato solo a fine `refreshFromScene()` — qualsiasi
  `saveZtoryc()` con `m_shots` stale diventa un no-op.

### Notes
- Regressioni da verificare nella prossima sessione:
  - **ffmpeg non funziona** + formati video assenti tra gli output (era già stato fixato)
  - **Risoluzione thumbnail PDF pessima** nel Board
- Task 32, 34, 31 risultano già completati in sessioni precedenti (da verificare/aggiornare ANIMATIC_TASKS)

---

## [2026-05-27c] — ZtoryMonitorPanel full toolbar + context chips + single-instance guard

### Added
- **ZtoryMonitorPanel full toolbar** — zoom slider, fit-all, select/trim/razor,
  add/delete/merge/copy/clone/paste, TC toggle. Toolbar posizionata tra viewer e
  timeline (sopra la traccia video).
- **Double-click sul Monitor** — apre la sotto-scena nel contesto principale
  (seek + play range + activateShotForViewing). Return button chiude la sub-scene.
- **Audio tracks nel Monitor** — refresh con fingerprint per evitare rebuild inutili.
- **Context chips** — badge colorati nella toolbar: "BOARD" (verde), "ANIMATIC" (blu),
  "MONITOR" (viola).
- **Single-instance guard** (task 33) — QLockFile in
  `~/Library/Caches/ztoryc/ztoryc_<user>.lock`. QMessageBox se seconda istanza.

### Modified
- **ztoryanimatic.h** — slot di edit spostati a `public slots:` per forwarding dal Monitor.
- **onDeleteShots/onCopyShots/onCutShots/onCloneShots** — usano sharedSelection come fallback.
- **StoryboardPanel** — `m_dirtyShotCol` tracking + detectAndUpdatePanels in
  contesto main-xsheet (legge sub-xsheet da TXshChildLevel senza iterare celle main).

### Notes
- **BUG APERTO (PRIORITÀ 1 prossima sessione):** testi Board spariscono ad ogni
  riapertura (nei primi panel soprattutto). Root cause non trovata via code
  inspection. Richiede test interattivo con l'app aperta. Da riprendere subito.
- Commit: `140d790ac`

---

## [2026-05-27b] — ANIMATIC_TASKS: arrow tool feature requests

### Added (task list only)
- **Task 35** — Storyboard Arrow Tool: freccia vettoriale disegnabile su arco/curva
  Bézier con arrowhead auto-calcolato dalla tangente dell'endpoint; opzioni
  inizio/fine/entrambi; integrazione con tool arco Tahoma2D.
- **Task 36** — Frecce 3D / Prospettiva: estensione del task 35 con frecce
  foreshortened per comunicare movimenti sull'asse Z. Variante 1 (2D stilizzata)
  prioritaria, variante 2 (gizmo 3D con proiezione camera) come futura iterazione.
- **Task 37** — Indicatore Direzione Luce: overlay non distruttivo nel panel
  per posizionare la sorgente di luce in 3D (angleH + angleV). Salvato nel .ztoryc
  come metadato, disegnato in PanelWidget::paintEvent() sopra il thumbnail.

### Notes
- Nessuna modifica al codice sorgente in questa sessione.

---

## [2026-05-27] — ZtoryMonitorPanel + camera view fix + RAM/performance fixes

### Added
- **ZtoryMonitorPanel** — nuovo pannello "Ztoryc Monitor" (secondo monitor): combina
  `ZtoryAnimaticViewer` + `ZtoryAnimaticRuler` + `ZtoryAnimaticTrack` in un QSplitter
  verticale. Doppio click cerca il frame (seek only, non apre sub-scene).
  Registrato in CMakeLists, menubar (`MI_OpenZtoryMonitor`), tpanels.cpp, mainwindow.cpp.

### Fixed
- **Camera rect drift in Camera View** — `SceneViewer::getViewMatrix()` usava il frame
  handle globale invece di `m_customFrameHandle`, causando disallineamento nel viewer
  animatico. Fix: usa `m_customFrameHandle` quando impostato.
- **RAM 50GB su apertura scena** (root cause 1) — `ZtoryAnimaticTrack::refreshFromScene()`
  usava `found = !b.thumbnail.isNull()`: se l'icona non era in cache iterava ogni cella
  × ogni layer della sub-xsheet → fino a 240.000 `getIcon()` calls per scena complessa.
  Fix: `found = true` alla prima cella trovata.
- **RAM 50GB su apertura scena** (root cause 2) — `onRefreshPreviews()` renderizzava tutti
  i pannelli (631 per la scena messina HARMONYA). Fix: sostituito con `updateVisiblePreviews()`.
- **SFH explosion repair** — `loadZtoryc()` rileva e collassa automaticamente i pannelli
  SFH-esplosi (> 20 pannelli, durata media ≤ 5 frame) → collassa a 1 pannello e riscrive
  il `.ztoryc` su disco. Soglia aggiornata: usa media invece di "tutti a 1 frame".
- **Scroll lento nel Board** — connessione `scrollBar::valueChanged → updateVisiblePreviews`
  debounced a 250ms. Prima: `renderXsheetFrame()` sincrono ad ogni tick dello scroll.
- **Skip thumbnail esistenti** — `updateVisiblePreviews()` salta i pannelli che hanno già
  un pixmap, evitando re-render inutili durante lo scroll di ritorno.
- **Freeze al caricamento con scene dense** — placeholder usa CSS + emoji 🎥 invece di
  `QPixmap(640×360)`. Prima: 631 allocazioni QPixmap sincrone nel loop `rebuildGrid()`
  bloccavano il main thread per ~6 secondi.
- **No auto-render all'apertura** — rimosso `QTimer::singleShot(500ms, updateVisiblePreviews)`
  da `refreshFromScene()`. Thumbnails renderizzati solo su scroll stop o "Refresh Previews".

### Notes
- v0.3.3 CI triggerato su GitHub Actions (macOS + Windows)
- `build_and_deploy.sh` fix: rilevamento automatico directory di build

## [2026-05-26] — SFH pipeline + split/merge/undo fixes + startup popup fix

### Fixed
- **Stop Frame Hold (SFH) in main xsheet** — `resequenceXsheet()` piazza ora una
  cella `STOP_FRAME` alla fine di ogni colonna shot nel main xsheet, impedendo che
  l'ultimo disegno di uno shot faccia implicit hold sul frame successivo durante
  playback/render animatic.
- **Split (Razor) durata corretta** — `ignoreLastStop=true` su `srcColumn->getRange`
  esclude la SFH da `totalDuration`/`secondHalf`: Shot 2 aveva 1 frame in più.
- **Split Shot 2 partiva da frame 0 invece che dal punto di taglio** — `materializeCells`
  riempiva solo fino a `lastContent`; se il punto di split era in zona implicit hold,
  `shiftChildXsheetBy` calcolava `keep < 0` e non spostava nulla. Aggiunto parametro
  `fillToEnd=true` per il caso split che riempie hold fino a `duration-1`.
- **SFH in sub-scena Shot 1 dopo split** — piazza SFH a `splitRel` in ogni colonna
  della sub-scena di Shot 1 per terminare la catena di hold al punto di taglio.
- **materializeCells non propaga SFH come hold** — celle SFH azzerano `last`.
- **Merge durata corretta** — `dstColumn->getRange(ignoreLastStop=true)`;
  `appendAt` ora sovrascrive la SFH esistente invece di appendere dopo di essa.
- **onMatchSubsceneDuration +1 frame vuoto** — `ignoreLastStop=true` + skip SFH
  nel backward scan: mark-out al frame precedente la SFH.
- **captureSnapshot contava SFH come frame reale** — `s.duration` gonfiato di 1
  → undo ricostruiva con 1 frame extra → timeline vuota. Fix: skip `isStopFrame()`.
- **Popup "Unable to create a new document" all'avvio** — rimossa `NSDocumentClass`
  da `CFBundleDocumentTypes` in `BundleInfo.plist.in`.
- **openSubXsheet mark-out inflato** — `ignoreLastStop=true` in `subscenecommand.cpp`.

### Notes
- Build CI macOS + Windows lanciate su commit `ef7e934ea`.
- Undo del razor: Board/animatic si ripristinano correttamente; contenuto interno
  della sub-scena non viene undone (limitazione architetturale — da risolvere con
  undo dedicato in futuro).

## [2026-05-25d] — Xsheet: cell swap, block swap, rolling edit + animatic TC sync

### Added
- **Cell swap** (`xsheetdragtool.cpp`) — `⌥ Option + drag` su una singola cella
  in xsheet: scambia il contenuto della cella sorgente con la destinazione (stessa
  colonna). Highlight arancione sulla destinazione durante il drag. Undo/redo.
- **Block swap** — stessa gesture con una selezione multi-frame preesistente: il
  blocco intero viene scambiato con un range uguale alla destinazione. La selezione
  segue il blocco dopo il rilascio.
- **Rolling edit** (`RollingEditTool`) — `⌥ Option + smart tab inferiore`: sposta
  il confine tra due livelli adiacenti senza cambiare la durata totale. Drag giù →
  cel corrente si allunga, il successivo si accorcia dall'inizio (e viceversa).
  `⌘ Cmd + ⌥ Option + smart tab superiore`: rolling edit verso l'alto. Linea
  verde indica il confine durante il drag. Undo/redo con capture lazy pre-drag.

### Fixed
- **TC non si aggiornava al cambio FPS** (`ztoryanimatic.cpp`) — `refreshFromScene()`
  ora chiama `m_ruler->setFps()` sincronizzando il timecode quando si modifica
  il frame rate da Scene Settings.
- **Delete shot nella toolbar animatic** posizionato correttamente accanto ad Add (+/-).

### Notes
- `CellSwapUndo` range-aware: gestisce sia swap singola cella che blocchi N frame.
- `RollingEditUndo`: cattura lo stato pre-drag lazily per frame toccati; redo
  ri-applica da stored before/after vectors.
- Hook Alt in `mousePressEvent` (xshcellviewer.cpp) intercettato prima del blocco
  level-range-selection per evitare che la selezione venga allargata all'intero livello.


## [2026-05-25c] — Animatic: zoom Ctrl+Scroll, Fit All, ruler adattivo

### Added
- **Ctrl+Scroll zoom** su tutto l'animatic — funziona su video track, audio track
  e ruler (prima solo sul ruler). Ctrl+Scroll → zoom, scroll plain → pan.
- **Pulsante Fit All `[]`** nella toolbar animatic — calcola il ppf esatto per
  vedere tutta la timeline in un click. Anche shortcut **Ctrl+0**.
- **Limiti zoom estesi**: min `0.02 ppf` (supporta 26 minuti a 24fps in ~750px),
  max `200 ppf` (editing frame-per-frame). Slider ricalibrato ×100.
- **`ZtoryAudioTrack::zoomChanged` signal** — connesso a `onZoomChanged` del panel.

### Fixed
- **Ruler adattivo** — label spacing completamente adattivo in entrambe le
  direzioni con serie base-10/5 (fps-agnostica: 1,2,5,10,25,50,100,250,500…):
  - Zoom in alto: label su ogni frame
  - Zoom normale (~8ppf): label ogni 5-10 frame
  - Zoom out estremo (26min): label ogni 2500-5000 frame senza accavallamento
  - Font ripristinato al default app (era `QFont("",8)` → spaziatura anomala)
- **`assignKeepNumbers()` crash su board vuota** (Windows) — accesso
  `m_shots[-1]` con board vuota. Fix: `if (total <= 1) return` early.

### Notes
- Thumbnail quality Board + Navigator: render a risoluzione fisica (ppf × DPR),
  rescalePreview DPR-aware, auto-resize su ridimensionamento finestra (150ms debounce),
  re-render su resize con previewRerenderNeeded signal + 200ms debounce.
- Min cella Board: 200→150px.


## [2026-05-25b] — Fix crash Board + su scena vuota (Windows)

### Fixed
- **`assignKeepNumbers()` crash su board vuota** (`storyboardpanel.cpp`) — con il
  primo shot (total=1, insertAt=0), la condizione `insertAt >= total-1` (0>=0) era
  vera e accedeva a `m_shots[-1]` → segfault su Windows (UB silenzioso su macOS).
  Fix: early return `if (total <= 1)` — nessun vicino da ereditare, ci pensa
  `renumberAll()`. In uso normale non si manifesta perché Ztoryc parte con uno shot
  già creato dalla startup dialog.

### Notes
- Build locale aggiornata; CI non triggerata (fix non urgente per utenti normali).


## [2026-05-25] — Script panel: import multi-formato (.fdx, .fountain, .docx, .odt, .txt)

### Added
- **`parseFountain()`** (`ztoryscriptpanel.cpp`) — parser completo del formato Fountain
  (open standard per sceneggiature): scene heading, character, dialogue, parenthetical,
  action, transition, lyrics, boneyard, note inline. Title page saltata automaticamente.
- **`parseDocx()`** — parser cross-platform per `.docx` (Word 2007+): estrae
  `word/document.xml` dallo ZIP con un lettore zlib custom (nessuna dipendenza esterna),
  legge gli stili di paragrafo Word. Se nessuno stile è riconosciuto come screenplay,
  ricade sull'euristica di `parseTxt()`.
- **`parseOdt()`** — parser cross-platform per `.odt` (LibreOffice/OpenDocument): estrae
  `content.xml`, gestisce `<text:span>` annidati tramite `readElementText()`, stili ODF.
  Stesso fallback euristico di DOCX.
- **`parseTxt()` migliorato** — ora rileva automaticamente se il testo è una
  sceneggiatura (conta le scene heading SC\d+ / INT. / EXT.): se sì applica la
  formattazione visiva identica a FDX; se no restituisce il testo grezzo.
- **zlib linkata esplicitamente** (`toonz/sources/toonz/CMakeLists.txt`) al target
  Ztoryc via `find_package(ZLIB REQUIRED)` — necessaria per il decompressore ZIP dei
  parser DOCX/ODT.
- **Word wrap** nel panel script — `WidgetWidth` invece di `NoWrap`: le righe si
  adattano alla larghezza del panel mantenendo la formattazione (indentazioni, dialoghi).
- **Conversione .doc → .fdx** via script Python standalone (`/tmp/convert_fdx.py`):
  usato in sessione per convertire "Il Palazzo Scomparso v7.doc" → `.fdx`.

### Fixed
- **DOCX fallback**: paragrafi vuoti ora inclusi nel flat text → l'euristica riceve
  le righe vuote separatrici indispensabili per distinguere i blocchi dialogo/azione.
- **ODT**: il `continue` iniziale nel loop XML filtrava tutti gli `isCharacters()` →
  testo completamente vuoto. Fix: rimosso il filtro; `<text:span>` gestito con
  `readElementText(IncludeChildElements)`.
- **ODT style-name**: attributo ora letto correttamente via namespace URI esplicito
  (`urn:oasis:names:tc:opendocument:xmlns:text:1.0`) invece di `contains("text")`.

### Notes
- `.doc` (formato binario legacy) non supportato cross-platform: mostra messaggio
  che invita a riesportare come `.docx`.
- File dialog e drag & drop ora accettano: `.fdx`, `.fountain`, `.docx`, `.odt`,
  `.doc`, `.txt`.

---

## [2026-05-25] — Qualità anteprime Board e Navigator

### Fixed
- **ZtoryPanelNavigator preview sfocata** (`ztoryanimatic.cpp`) — rendering fisso a
  320×180 px indipendentemente dalla dimensione del label e dal DPR. Fix: render al
  `label_size × devicePixelRatio` (cappato a 1280×720), pixmap taggato con
  `setDevicePixelRatio`, display senza upscaling. Su Retina il navigator mostra
  immagini pixel-perfect.
- **Board thumbnails sfocate su Retina e su pannelli larghi** (`storyboardpanel.cpp`):
  - `updatePreview()`: render a `panel_width × DPR` fisici invece di 320×180 fisso.
  - `rescalePreview()`: ora DPR-aware — scala a pixel fisici e tagga con
    `setDevicePixelRatio` prima di passare al QLabel.
  - `PanelWidget::resizeEvent()`: se la larghezza fisica richiesta supera del 20%
    quella del pixmap memorizzato, emette `previewRerenderNeeded(si, pi)`.
  - `connectPanelWidget()`: debounce 200ms su `previewRerenderNeeded` — ri-renderizza
    in coda solo i pannelli che ne hanno bisogno, una sola volta dopo il resize.

### Added
- **Auto-resize Board su ridimensionamento finestra** (`storyboardpanel.cpp`) —
  `StoryboardPanel::resizeEvent` (nuovo) con debounce 150ms: ricalcola `colW` dal
  viewport e aggiorna `setFixedWidth` su tutte le celle. Le celle si adattano in
  tempo reale alla finestra senza toccare il numero di colonne.
- **Dimensione minima celle ridotta a 150 px** (era 200 px) — permette più colonne
  visibili su schermi stretti o con molti shot.

## [2026-05-24c] — Post-release fix: crash Cutout Digital + upstream candidates

### Fixed
- **TasksViewer crash on room switch** (`tasksviewer.cpp`) — `~TasksViewer()` vuoto
  lasciava puntatore dangling in `BatchesController::m_tasksTree`; switchando su
  Cutout Digital crashava in `QHeaderView::setModel()`. Fix: `setTasksTree(nullptr)`
  nel distruttore. 1 riga. Commit `1569cf2cc`. ✅ Pronto per PR upstream Tahoma2D.

### Notes
- Lista upstream PR candidates aggiornata in AGENTS.md con priorità e checkbox
  "da verificare su Tahoma2D". TasksViewer crash è il primo verificato e pronto.
- Nuovo build macOS in corso (deployment target 12.0 + tutti i fix del giorno).
  Sostituirà i DMG in v0.3.2 quando finisce (~80 min).

---
## [2026-05-24b] — Post-release fix: FFmpeg, Gatekeeper, deployment target

### Fixed
- **FFmpeg dylib path nel bundle**: `dylibbundler` scriveva `@executable_path/../libs/`
  aspettando layout `bin/`, ma i binari finiscono direttamente in `Resources/ffmpeg/`.
  Fix: `install_name_tool` in `tahoma-buildpkg.sh` riscrive tutti i riferimenti a
  `@executable_path/libs/` dopo la copia (sia binari che dylibs cross-ref).
  Risultato: formati video ora visibili nel render.
- **"Unable to create a new document" all'avvio**: macOS session-restore machinery
  attivata perché mancavano `NSQuitAlwaysKeepsWindows=false` e
  `NSApplicationSupportsSecureRestorableState=true` in `BundleInfo.plist.in`.
- **"Non puoi usare questa versione" su macOS 12-14**: nessun `CMAKE_OSX_DEPLOYMENT_TARGET`
  impostato → il runner macOS 15 embed `minos 15.0`. Aggiunto `-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0`
  in `tahoma-build.sh` (allineato a Tahoma2D upstream). Nuovo build macOS triggerato.
- **Windows CI**: fix `publish_release` input — rimossa dipendenza da
  `github.repository_owner == 'tahoma2d'`.

### Notes
- Percorso immagine DMG drag-and-drop: `ci-scripts/osx/assets/ztoryc-dmg-background.png`
  (generata da `render-dmg-background.py`)
- Fix FFmpeg applicato manualmente all'app installata (senza rebuild)
- Nuovo build macOS in corso per v0.3.2 con deployment target 12.0

---
## [2026-05-24] — Release v0.3.2: fix CI macOS + pacchetti Windows/macOS

### Fixed
- **CI macOS: build falliva su "Package portable app and DMG"**: Homebrew ha rimosso
  la formula `libtiff44`. `tahoma-install.sh` ora builda libtiff 4.4.0 da sorgenti
  in `/tmp/libtiff44-build`; `tahoma-buildpkg.sh` legge `libtiff.5.dylib` da lì.

### Added
- **Windows CI: `publish_release` workflow_dispatch input**: il workflow Windows ora
  ha lo stesso meccanismo di macOS — triggerando con `publish_release=true` carica
  installer + portable zip direttamente nella GitHub Release.

### Notes
- Release v0.3.2 pubblicata con tutti e 4 i pacchetti (2 DMG + exe + zip).
- Percorso QSS temi: `/Applications/Ztoryc.app/Contents/Resources/tahomastuff/config/qss/`
- Fix macOS Gatekeeper "file danneggiato": `xattr -cr /Applications/Ztoryc.app`

---
## [2026-05-25] — Shot Board: fix preview, label e titolo viewer

### Fixed
- **Shot Board preview sempre su shot 010**: `ZtoryModel::m_shots[si].xsheetColumn`
  era sempre 0 (default) perché `syncShotPanels` non lo propagava. `refreshPreview()`
  usa `shot.xsheetColumn` per scegliere da quale colonna rendere la thumbnail: con
  valore 0 fisso, tutti gli shot mostravano la thumbnail di SH010. Ora `syncShotPanels`
  accetta un parametro `xsheetCol` (opzionale) e lo scrive in `m_shots[si]`. Tutti e 4
  i call site nel Board ora passano `m_shots[i].data.xsheetColumn`.
- **Shot Board header "  -  1 panels"** (label vuoto): `syncShotPanels` non propagava
  il `shotLabel`. Aggiunto parametro `label` opzionale; ogni call site passa
  `m_shots[i].data.shotLabel`. Dopo `renumberAll()` in `refreshFromScene` viene
  eseguita una seconda bulk-sync per aggiornare ZtoryModel con i label finali
  (prima i label erano quelli pre-renumber del .ztoryc, o vuoti se nessun .ztoryc).
- **Titolo viewer sempre "Ztoryc Viewer"**: `ZtoryAnimaticViewerPanelFactory::createPanel()`
  chiamava `panel->setWindowTitle("Ztoryc Viewer")` dopo il costruttore, annullando
  `updateTitle()`. Spostato `updateTitle()` in un `QTimer::singleShot(0, ...)` così
  viene eseguito nell'event loop successivo, dopo il factory. Ora mostra
  "Animatic - scenename" in main e il label shot ("SH020" ecc.) in shot editing mode.

### Modified
- `ZtoryModel::syncShotPanels`: firma estesa con `label = {}` e `xsheetCol = -1`
  (parametri opzionali, backward-compatible con tutti i call site esistenti)
- `StoryboardPanel::refreshFromScene`: bulk sync aggiuntivo dopo `renumberAll()`
  per garantire che ZtoryModel abbia i label finali post-renumber

---
## [2026-05-24] — Sessione breve: sync task list da Drive

### Notes
- Sessione post-compact molto carica di contesto, nessuna modifica al codice.
- Letta la nuova `ANIMATIC_TASKS (1).md` (2026-05-23) preparata da Claudio su
  Drive. La nuova Priority Order mette in cima:
  1. MOD UI Headers (viewer contestuale, left `BOARD/SHOT`, right `SCRIPT/PALETTE`)
  2. NEW Single-instance guard (`QLockFile` in `main.cpp`)
  3. NEW Room "Ztoryc T" + Panel Navigator + rinomina "Ztoryc X" +
     rimozione Browser dal right panel di entrambe le room
  4. PERF Board thumbnail cache (lazy + thread + dirty + disco)
  5. PERF/BUG RAM causa 1 — sub-scene leak su scene lunghe
- Task in sospeso dalla sessione precedente (rimozione Browser dal right
  panel + zoom-out timeline illimitato) restano da fare: la rimozione
  Browser ora è formalizzata dentro il task "Ztoryc T" e va applicata a
  entrambe le room, non solo a quella corrente.
- Chiusura sessione per ripartire pulito al prossimo turno.

---
## [2026-05-22] — Main Audio toggle: riscrittura completa + Script panel persistente

### Contesto
Il toggle Main Audio (sovrappone la colonna sonora del main a uno shot, serve
per il lavoro di lipsync) aveva una serie di problemi: audio di scene
precedenti, tracce cancellate ancora udibili, scrub diverso dal play, scrub
impreciso. Riscrittura in 4 fasi A/B/C/D. In più, persistenza dello Script
panel (sceneggiatura importata).

### Fixed — audio cache (Fase A)
- `m_columnSoundTracks` era indicizzata per numero di colonna: dopo delete/
  riordino di una colonna audio l'indice puntava alla traccia sbagliata o
  cancellata. Ora chiave = puntatore `TXshSoundColumn*`.
- `preBuildSoundTrackAsync()` scriveva il risultato con `if (!m_soundTrack)`:
  dopo `invalidateSoundTrack()` (cambio scena) `m_soundTrack` è null, quindi
  una build della scena PRECEDENTE veniva scritta nella corrente — l'audio
  fantasma. Aggiunto generation counter (`m_soundGen`): l'async scarta il
  risultato se la generazione è cambiata.
- Aggiunto `validateSoundCache()`: fingerprint delle colonne audio (puntatori
  + lunghezze); `requireSoundTrack`/`startPerColumnAudio`/`preBuildSound
  TrackAsync` lo chiamano e auto-invalidano la cache su qualsiasi modifica.

### Fixed — comportamento toggle (Fasi B + C)
- `onFrameSwitched` riproduceva l'audio SOLO con toggle ON (`&& mainAudio
  Enabled`): con OFF lo shot era muto. Tolto il gate — il viewer nativo
  riproduce sempre l'audio della sua xsheet; `hasSoundtrack()` decide la
  sorgente.
- `ownsSubSceneAudio()` valeva solo a profondità 1. Ora: OFF → false (audio
  sub-scena), ON → true a qualsiasi profondità (audio main).
- `onNativeFrameSwitched()` usciva se profondità ≠ 1. `ChildStack::getAncestor()`
  chaina già tutti i livelli → limite rimosso, funziona a ogni profondità.
- `TXsheet::scrub()` aveva il gate invertito (suonava solo con ON). Ora cede
  solo quando il controller possiede l'audio (ON + sub-scena).
- Logica finale = come il toggle video: ON = solo audio main, OFF = audio
  della sotto-scena corrente.

### Fixed — precisione scrub (Fase D)
- Finestra di scrub estesa da 1 frame (~41 ms, impercettibile) a ~150 ms.
- Scrub gapless: non si interrompe più il segmento in corso (interromperlo
  tagliava la parte centrale — "articolo" → "art--olo"); il segmento
  successivo riparte da dove l'audio era arrivato. Applicato a entrambi i
  percorsi (toggle ON e OFF).

### Fixed — colori anteprime Board
- `IconGenerator::renderXsheetFrame` applicava `rgbSwapped()` → R/B scambiati,
  un rosa diventava azzurro. Rimosso lo swap (le funzioni canoniche di Tahoma
  non lo fanno).

### Added — Script panel persistente
- All'import, la sceneggiatura (.fdx/.txt) viene copiata in
  `<progetto>/extras/script/`; il path relativo è salvato nel `.ztoryc`
  (`<scriptFile>`).
- `StoryboardPanel::saveZtoryc/loadZtoryc` (i veri reader/writer del `.ztoryc`)
  gestiscono il tag; `loadZtoryc` chiama sempre `setScriptFile` → aprendo una
  scena senza sceneggiatura (o una scena nuova) il panel si svuota.
- `ZtoryModel::scriptFileChanged` pilota il reload del panel e il salvataggio
  del `.ztoryc` (un File>Save semplice non chiama saveZtoryc).

### Upstream candidates
- `onActiveViewerChanged` parentWidget-chain null guard, `BaseViewerPanel`
  preview button init — entrambi fixati per il crash Windows, validi upstream.

### Notes
- Build #11 Windows + DMG macOS rigenerati a fine sessione.



### Contesto
La build Windows era rotta da mesi (silenziosamente: lo script non propagava
gli errori msbuild e impacchettava uno zip senza `Ztoryc.exe`). Sono servite
10 iterazioni CI per arrivare a una build Windows funzionante e testata.

### Come riprodurre la build (per Vincenzo / chi fa le release)

**Windows** — GitHub Actions, workflow `Ztoryc Windows Build`
(`.github/workflows/windows_build.yml`), trigger manuale:
```
gh workflow run "Ztoryc Windows Build" --ref master
```
Il workflow (runner `windows-2022`) esegue in sequenza:
1. `ci-scripts/windows/tahoma-install.bat` — deps (Qt 5.15.2_wintab, Boost, OpenCV)
2. `ci-scripts/windows/tahoma-get3rdpartyapps.bat` — ffmpeg, rhubarb
3. `ci-scripts/windows/tahoma-build.bat` — `cmake` + `msbuild RelWithDebInfo ALL_BUILD.vcxproj`
4. `ci-scripts/windows/tahoma-buildpkg.bat` — assembla `Ztoryc\`, `windeployqt`,
   zip portable + installer Inno Setup
Output: artifact `Ztoryc-portable-win.zip` + `Ztoryc-install-win.exe`.
Build #10 funzionante = run CI `26226282757`, commit `8324c58ca`.

**macOS** — build locale (non CI):
1. CMake + `ninja` standard
2. `SKIP_PKG=1 SKIP_DSYM_IN_PACKAGE=1 bash ci-scripts/osx/tahoma-buildpkg.sh`
Output: `Ztoryc-portable-osx.dmg`. Il workflow `macOS_build.yml` fa lo stesso
su CI e può creare direttamente una GitHub Release (`workflow_dispatch` con
input `publish_release: true`).

### Fixed — compilazione Windows (MSVC)
- `TXsheet::setMasterVolume` chiamava `TSoundOutputDevice::setVolume`, che su
  Windows non esiste (dichiarato `#ifndef _WIN32` in tsound.h — il backend
  `tsound_nt.cpp` non ha API di volume per-device). Aggiunto guard `#ifndef _WIN32`.
- 48 occorrenze dell'operatore `not` → `!` (MSVC non accetta i token
  alternativi C++ senza `/permissive-`): storyboardpanel.cpp, subscenecommand.cpp,
  tcenterlinecolors.cpp, borders_extractor.h.
- Variabile locale `near` in ztoryanimatic.cpp rinominata `nearEdge` — `near`
  è una macro retaggio 16-bit in `windef.h`.
- Cherry-pick "Windows port: MSVC compatibility fixes" dal branch `windows-build`
  (DVAPI exports, altri `not`→`!`). Il branch `windows-build` è ora obsoleto.
- `tahoma-build.bat` ora propaga l'exit code di msbuild (`|| exit /b 1`) e
  verifica che `RelWithDebInfo\Ztoryc.exe` esista — prima impacchettava
  silenziosamente uno zip senza l'eseguibile principale.

### Fixed — crash runtime Windows
- Crash `EXCEPTION_ACCESS_VIOLATION` al doppio click su uno shot
  (`onShotDoubleClicked → enterShotMode → ... → getPreviewButtonStates`).
  Causa: il costruttore `BaseViewerPanel` inizializzava `m_symmetryButton` e
  `m_perspectiveButton` a nullptr ma NON `m_previewButton` /
  `m_subcameraPreviewButton`. Quelli vengono creati in `initializeTitleBar()`,
  invocato dal *container* del pannello (tpanels.cpp), non dal costruttore.
  `ZtoryAnimaticViewerPanel::enterShotMode` crea un `ComboViewerPanel` nudo
  che non passa mai da quel container → `m_previewButton` restava memoria
  garbage → su Windows non-null → `->isChecked()` crashava. Su macOS la
  memoria capitava a zero (guard regge) → bug Windows-only. Fix: init a
  nullptr nel costruttore.
- `MainWindow::onActiveViewerChanged`: catena `parentWidget()->parentWidget()`
  senza null-check — guard difensivo aggiunto.

### Fixed — installer / packaging Windows
- Registry root: l'app leggeva da `SOFTWARE\Tahoma2D\Ztoryc\ZTORYCROOT`
  (residuo pre-rebranding in tenv.cpp) ma l'installer scrive su
  `Software\Ztoryc\Ztoryc`. L'app installata trovava ZTORYCROOT vuoto e
  abortiva. Registry root → `SOFTWARE\Ztoryc\Ztoryc`.
- Icona: l'exe Windows aveva ancora `Tahoma2D.ico`. Aggiunto `Ztoryc.ico`
  (6 risoluzioni 16–256, generato da Ztoryc.icns) + `toonz.rc` aggiornato.
- Versione installer: `setup.iss` aveva `MyAppVersion "1.6"` hardcoded
  (versione base Tahoma2D). Ora `tahoma-buildpkg.bat` legge la semver da
  `ZtorycVersion.cmake` e la passa a ISCC via `/DMyAppVersion`.

### Modified
- Cartella stuff del bundle portable rinominata `tahomastuff` → `ztorycstuff`
  (tenv.cpp, CMakeLists.txt, build script mac/win/linux, preferencespopup.cpp).
  `tenv.cpp` prova prima `ztorycstuff`, poi fallback legacy `tahomastuff`.
- Window title: rimosso il suffisso "Tahoma2D <versione>".

### Notes
- I pacchetti delle build sono artifact CI temporanei (scadono 90 giorni,
  richiedono login). Per condividerli con tester serve una GitHub Pre-release
  (tag tipo `v0.3.2-beta1`, link pubblico permanente).
- `master` HEAD `9b804d8e0` ha il fix versione installer non incluso nella
  build #10 — per la pre-release serve una build #11 fresca.

---
## [2026-05-19] — Audio track: undo completo, fix RAM su scene lunghe

### Fixed
- Linked razor (video+audio): l'undo ora annulla sia il taglio video che quello audio
  (prima annullava solo il video). Usa `TUndoScopedBlock` + `UndoAudioEdit` per gruppo.
- Linked razor: fix index shift — dopo `cloneChild(col)` le colonne audio sono spostate
  di +1; ora l'indice viene corretto prima di `splitAudioColumn`.
- Waveform cache RAM: `QPixmap(trackW, trackH)` allocava l'intera larghezza (es. 80.000px
  su scene da 10.000 frame → 16 MB per traccia). Sostituito con cache viewport-aware:
  solo la zona visibile + 600px di overscan, indipendente dalla lunghezza della scena.
- Add Audio Track: ora supporta undo/redo (`UndoAddAudioTrack` con `insertColumn`/`removeColumn`).
- Delete segmento audio: ora funziona correttamente (focus esplicito con `setFocus()` in
  `mousePressEvent` garantisce che il widget riceva l'evento tastiera).
- Selezione audio cross-track: il click su un segmento di un'altra traccia (o sulla
  traccia video) deseleziona le altre tracce. Nuovo segnale `selectionCleared`.
- Undo drag/trim audio: `mouseReleaseEvent` ora fa snapshot prima/dopo con `UndoAudioEdit`
  per `SegmentDrag`, `TrimLeft`, `TrimRight`.
- Focus indicator traccia audio: bordo blu (#50A0FF) 2px intorno alla traccia attiva,
  così è chiaro quale traccia riceverà il Ctrl+V.

### Notes
- La waveform cache usa un "sliding window" da ~1200px. Al primo scroll fuori banda
  la cache si rigenera (solo la banda visibile). CPU e RAM costanti indipendentemente
  dalla lunghezza della scena.

## [2026-05-10] — Early Beta v0.2: overlay buttons shot viewer + symmetry guide fix

### Fixed
- **Crash Perspective Grid Tool (SIGSEV)**: `m_perspectiveButton` era un pointer non inizializzato in `ComboViewerPanel` (che non chiama `initializeTitleBar()`). Fix: inizializzati `m_symmetryButton` e `m_perspectiveButton` a `nullptr` nel costruttore di `BaseViewerPanel` + null-guard in `onToolSwitched()` prima di dereferenziare entrambi. (`viewerpane.cpp`)
- **Camera View e Render Preview non funzionavano in shot mode**: i bottoni nel title bar di `ZtoryAnimaticViewerPanel` (Camera Stand, Camera View, Preview) erano connessi permanentemente allo SceneViewer dell'animatic viewer (pagina 0 dello stack). In shot mode (pagina 1, ComboViewerPanel), i bottoni non avevano effetto sul viewer visibile. Fix: in `enterShotMode()` le connessioni vengono riinstradate al `ComboViewerPanel`; in `returnToAnimaticMode()` vengono riportate all'animatic viewer. Il shot viewer si apre di default in `CAMERA_REFERENCE`. (`ztoryanimatic.cpp`, `viewerpane.h`)
- **Room nere quando si tornava al workflow storyboard con simmetria attiva**: causato dalla lambda `xsheetSwitched` che non ripristinava il routing dei bottoni quando l'xsheet tornava al livello principale via percorso diverso dal back button. Fix: aggiunto `restoreAnimaticButtons()` nella lambda. (`ztoryanimatic.cpp`)
- **Symmetry Guide non funzionava in shot edit mode**: il bottone overlay triggherava `MI_ShowSymmetryGuide` via CommandManager, aggiornando la `TEnv` var ma senza mai chiamare `SymmetryTool::setGuideEnabled()` (che normalmente è in `onSymmetryGuideToggled()`, connessa solo nei viewer che chiamano `initializeTitleBar()`). Fix: aggiunta connessione diretta a `SymmetryTool`/`PerspectiveTool::setGuideEnabled()` nel costruttore di `ZtoryAnimaticViewerPanel`. (`ztoryanimatic.cpp`)

### Added
- **Overlay buttons in shot edit mode**: bottoni Symmetry Guide e Perspective Grid nel title bar di `ZtoryAnimaticViewerPanel`, visibili solo in shot edit mode (enterShotMode/restoreAnimaticButtons). Bottoni Safe Area e Field Guide sempre visibili sull'animatic viewer. (`ztoryanimatic.cpp`)
- `BaseViewerPanel::sceneViewer()`, `referenceModeButtonSet()`, `previewButton()` — accessor pubblici per permettere a `ZtoryAnimaticViewerPanel` di reindirizzare le connessioni dei bottoni senza accedere a membri protetti. (`viewerpane.h`)
- `ZtoryAnimaticViewerPanel::restoreAnimaticButtons()` — helper che nasconde gli overlay buttons e ripristina il routing dei bottoni verso l'animatic viewer; chiamato in `returnToAnimaticMode()` e nella lambda `xsheetSwitched`. (`ztoryanimatic.cpp`)

### Notes
- Task 21 (Volume per traccia audio) completato nella sessione precedente dello stesso giorno
- Early Beta (v0.2) milestone raggiunta: Undo/Redo ✅, audio toggle ✅, crash fix ✅, shot viewer camera view ✅, overlay buttons shot viewer ✅, symmetry guide fix ✅
- Fix version: `ZtorycVersion.cmake` era già a `0.3` (next milestone) — ripristinato a `0.2.0` per la release corrente

---
## [2026-05-09] — Audio sub-scene fix completo + cleanup SLIP/onion residui

### Fixed
- **Audio in sub-scene (workflow 2D Tradigital + Storyboard)**: risolto il "frammento + double-play". Catena di fix:
  - **Guard `m_frameHandle->isPlaying()` sostituito con `isContinuousPlaying()`** in `onNativePlayingStatusChanged()` e `onNativeFrameSwitched()`. Il guard precedente bloccava l'audio quando il frame handle dell'animatic restava "stuck" in playing dopo un cambio room senza stop esplicito → audio a volte non partiva o non ripartiva al loop
  - **Room nera durante play**: causata da flooding del `QAudioOutput::notify` ogni 50ms quando `onNativeFrameSwitched()` veniva chiamato per ogni frame durante il play (frame handle globale aggiornato anche durante play animatic). Fix con il guard preciso sopra
  - **Doppio play (frame 1 + frame 31)**: aggiunto `ZtoryAnimaticController::ownsSubSceneAudio()` chiamato da `BaseViewerPanel::hasSoundtrack()` per sopprimere il path nativo per-frame quando il controller streama già il main audio
  - **Frame senza mapping main → muti**: check `ancestor.first != mainXsh` in entrambe le funzioni — sub-frame fuori dal range mappato ora correttamente silenziosi
  - **Audio non parte se play comincia su frame muto**: `onNativeFrameSwitched()` ora ritenta `onNativePlayingStatusChanged()` quando entra nel mapped range
  - **Audio muto al loop**: aggiunto `m_lastNativePlayFrame` per rilevare il salto all'indietro del FlipConsole; al loop resetta `m_nativeAudioPlaying=false` e fa ripartire l'audio
- **`m_scrubDevice` dedicato** (`TSoundOutputDevice` separato dal `mainXsh->m_player`) per scrub audio sia in `onNativeFrameSwitched()` che in `playAnimaticAudioFrame()` — evita interferenze fra scrub e continuous play. Destructor in `~ZtoryAnimaticController()` per evitare leak a chiusura
- **`mainXsh->stopScrub()` esplicito prima di `play()`** in `onNativePlayingStatusChanged()` per scartare residui del ring buffer hardware

### Removed
- **SlipTool eliminato completamente** (decisione di scope dopo analisi onesta — implementazione corretta richiedeva 2-3 settimane su mobile mark-in nei sub-scene cell):
  - Toolbar `slipBtn` rimosso da `ZtoryAnimaticPanel`
  - Enum `Tool::SlipTool` e `DragMode::Slip` rimossi
  - Field `ShotData::slipOffset`, metodi `getSlipOffset`/`adjustSlipOffset` rimossi da `ZtoryModel`
  - Attributo `slipOffset` rimosso da save/load XML (read no-op per backward compat)
  - Signal `slipEdit`/slot `onSlipEdit` rimossi
  - Slip indicator (striscia arancione + label `+N`) rimosso dal paint dei block
  - Cell write in `resequenceXsheet()` semplificato a `TFrameId(r + 1)` (era `TFrameId(slipOff + r + 1)`)
  - In `ztoryOpenSubXsheet()` il play range ora copre l'intera durata animatic (non più clamp via slipOff)
- **Onion skin residui in `ZtoryAnimaticRuler`** (era stato deciso di rimuoverlo ma rimanevano frammenti che causavano malfunzionamenti):
  - Strip FOS (top) + MOS (bottom) rimosse → ruler ora alto 18px (era 36px)
  - `m_localMask`, `m_onionEnabled`, `setOnionEnabled()`, `syncOnionToGlobal()` rimossi
  - HoverZone (HoverFOS/HoverMOS), `m_hoverFrame`, `m_hoverZone` rimossi
  - Signal `onionEnabledChanged` rimosso
  - Connect `onionSkinMaskChanged` (sia su `m_sceneViewer` che su `m_ruler`) rimossi
  - Include `onionskinmask.h` e `tonionskinmaskhandle.h` rimossi dal `ztoryanimatic.cpp`/`.h`

### Modified
- `BaseViewerPanel::hasSoundtrack()` (viewerpane.cpp): aggiunto early return su `ZtoryAnimaticController::ownsSubSceneAudio()` analogo a `ownsAudioAtMainLevel()` — sopprime la path audio nativa quando il controller gestisce lo streaming continuo dal main

### Notes
- Lista feature/fix completa rivista in sessione → piano d'azione in 7 fasi:
  1. Audio finiture (icon toggle, waveform normalization, volume per traccia, audio keyboard shortcuts)
  2. Timeline QoL (peg column residue, PASTE behavior, default palette)
  3. Sync shot duration toggle + mesh extras path
  4. Camera sensitivity + waveform residui
  5. Layout per workflow (config-only, replicare struttura `Storyboard/`)
  6. Transizioni (formula `extra_per_lato = N/2`)
  7. Heavy a parte: CEL/KEYS modes (timeline only), PSD robustness + ORA importer, Autofill vector, Multi-select import
- **DROPPED**: SLIP, SLIDE (workaround manuale accettabile), navigation tag fissi per export range
- Toggle audio invertito (icona ON↔OFF) noto, da fixare in Fase 1 — è solo asset/QAction state

---
## [2026-05-05] — Trim/Roll + Slip + fix workflow Load Scene

### Added
- **TrimTool (T)** nella toolbar animatic: drag sul seam tra due shot → Roll edit (A±Δ, B∓Δ, durata totale invariata); drag sul bordo destro dell'ultimo shot → Ripple Trim classico. Cursore `SplitHCursor` sul seam, `SizeHorCursor` sul bordo isolato
- **SlipTool (Y)** nella toolbar animatic: drag dentro un blocco → sposta la finestra sub-scena senza cambiare durata o posizione nel timeline. Indicatore visivo: striscia arancione bordo sinistro + testo `+N` in basso a destra
- **`ShotData.slipOffset`** (int, 0-based): campo persistente in ZtoryModel, salvato/caricato nel `.ztoryc` (`slipOffset` attribute)
- **`ZtoryModel::resequenceXsheet()`** aggiornato: scrive `TFrameId(slipOff + r + 1)` per ogni colonna, preservando lo slip offset ad ogni resequence
- **`ZtoryModel::shotIndexForCol / getSlipOffset / adjustSlipOffset`**: metodi helper per gestione slip
- **Marker dentro lo shot con Slip**: `onShotDoubleClicked` ora imposta play range `[slipOff, slipOff+duration-1]` nella sub-scena; `onSlipEdit` aggiorna `ztorySetShotRange` con il range slippato
- **Icone SVG**: `ztoryc_trim.svg`, `ztoryc_slip.svg` + registrazione in `toonz.qrc`
- **`onRollEdit` / `onSlipEdit`** slot in `ZtoryAnimaticPanel`: implementazione completa con undo snapshot

### Fixed
- **Workflow non applicato su scene recenti** nel popup Load Scene: `onRecentSceneClicked` ora esegue il comando MI_Workflow* selezionato in `m_loadWorkflowCB` (condizione `m_mode != LoadSubSceneMode`)

### Notes
- Slide (spostamento shot con compensazione vicini) rimandato — complessità alta
- Slip funziona tecnicamente (frameIds cambiano, viewer mostra frame diversi) ma feedback visivo durante il drag è limitato: il blocco non cambia apparenza a schermo; il contenuto cambia solo alla riproduzione successiva. Miglioramento previsto

---
## [2026-05-05] — Fix crash + Add to Favorites funzionante

### Fixed
- **Crash "Refresh Folder"**: causato da vtable corruption dopo aggiunta di `virtual TFilePath getPath()` nella base class `DvDirModelNode`. Rimossa la virtual (non necessaria), rimosso `override` da `DvDirModelFileFolderNode::getPath()`, ripristinato cast-based approach in `dvdirtreeview.cpp`
- **Errori compile `TFilePath + QString`**: `TFilePath::operator+` non accetta `QString` — corretti tutti e 4 i siti (dvdirtreeview.cpp ×2, filebrowser.cpp ×2) usando `favFolder + srcFp.withoutParentDir()` (TFilePath + TFilePath)

### Added
- **Add to Favorites (pannello destra)**: right-click su spazio vuoto nella cartella corrente → "Add to Favorites"; right-click su una cartella → "Add to Favorites". Implementato in `filebrowser.cpp::getContextMenu()`
- **Remove from Favorites (pannello destra)**: right-click dentro Favorites → "Remove from Favorites"
- **Add/Remove Favorites (albero sinistro)**: right-click su nodo folder nell'albero → "Add to Favorites" / "Remove from Favorites". Funziona per tutti i nodi `DvDirModelFileFolderNode` (Desktop, Downloads, cartelle progetto, ecc.) — non per nodi virtuali root (My Computer, History) che non hanno path reale
- **Drag & drop su Favorites**: trascinare una cartella sul nodo Favorites nell'albero crea un symlink

### Notes
- Symlinks creati con `QFile::link()` nella cartella `ToonzFolder::getMyFavoritesFolder()`
- `ImportAssetsPopup`: browser multi-selezione, tutti i tipi file eccetto .tnz, chiama `IoCmd::loadResources()`

---
## [2026-05-04] — Task 24: Startup popup come hub scene management

### Added
- **StartupPopup `Mode` enum** (4 modalità): `DefaultMode` (avvio a freddo, entrambi i tab, blocco chiusura), `CreateMode` (File > New Scene, solo tab Create, niente recent panel), `LoadMode` (File > Load Scene, solo tab Load), `LoadSubSceneMode` (File > Load As Sub-Scene, solo tab Load, multi-selezione con Shift/Cmd + pulsante "Load Selected", niente recent panel)
- **`File > Import > Import Assets...`** — browser nativo macOS (vecchio comportamento Load Scene), aggiunto in `menubar.cpp` + `menubarcommandids.h` + `mainwindow.cpp/.h`
- **Multi-selezione in LoadSubSceneMode**: `ExtendedSelection` + `setMultiSelect(true)` su `StartupScenesList` che disabilita hover-clear in `leaveEvent` e hover-setCurrentItem in `mouseMoveEvent`

### Modified
- **`onNewScene()`**: non chiama più `IoCmd::newScene()` prima del popup — la scena corrente non viene chiusa se l'utente fa Cancel. `IoCmd::newScene()` spostato dentro `onCreateButton()` per `CreateMode` only
- **Startup a freddo**: popup mostrato da `main.cpp` (DefaultMode); File > New Scene usa sempre `CreateMode`
- **Titoli popup dinamici**: "Ztoryc Startup" / "Create New Scene" / "Load Scene" / "Load Scene as Sub-Scene"
- **Bottone Cancel**: `setMinimumSize(65,25)` + `setMaximumHeight(25)` (uguale ai pulsanti nativi DVGui); label "Quit Ztoryc" solo a freddo con scena untitled, "Cancel" altrimenti
- **`onProjectComboChanged` in LoadSubSceneMode**: carica le scene del progetto selezionato senza cambiare progetto attivo né chiudere la scena corrente (`TProject::load()` + `refreshExistingScenes(scenesFolder)`)
- **`refreshExistingScenes`**: accetta `TFilePath scenesFolder = TFilePath()` opzionale
- **`IoCmd::loadSubScene(path)` in `iocommand.cpp`**: invariato (usa ASK_USER default per tutte le scene)
- **`ResourceImportDialog::askImportQuestion`**: messaggio contestuale — "doesn't belong to the current project" solo per file effettivamente esterni; messaggio neutro "Do you want to import it or load it from its current location?" per file nella scenes folder del progetto corrente (check `isExternPath` + ancestor della `+scenes` folder)

### Notes
- Bug layout Cancel button: `addWidget(btn, Qt::AlignLeft)` passava AlignLeft (=1) come stretch factor → pulsante si espandeva. Fix: `addWidget(btn, 0, Qt::AlignLeft)`

---
## [2026-05-03] — Peg columns narrow (22px) + Set Key fix

### Fixed
- **Set Key (Z) su peg columns**: `TCellSelection::setKeyframes()` usava `ColumnId(col)` invece di `xsh->getColumnObjectId(col)` → il keyframe veniva impostato sullo stage object sbagliato, nessun diamante visibile. Fix: una riga in `cellselectioncommand.cpp`
- **`setKeyframeWithoutUndo(int frame)`**: aggiunto `invalidate()` alla fine così `isKeyframe()` riflette subito le modifiche (lazy cache `m_lazyData` non veniva refreshata)
- **Peg columns narrow in vertical timeline**: colonne peg ora larghe 22px come camera. Modifiche:
  - `ColumnFan`: per-column width support (`m_width`, `setColumnWidth()`, `getCameraColumnDim()`, `getColWidth()`) con `update()` che usa larghezze per-colonna
  - `TXsheet`: peg columns marchiate 22px su `insertColumn()` e al termine di `loadData()`
  - `xshcolumnviewer`: peg in vertical timeline usa `CAMERA_LAYER_HEADER`/`CAMERA_LAYER_NAME`, nome ruotato 90°, nessuna icona
  - `xshcellviewer`: celle/keyframe/selection/focus border usano `CAMERA_CELL`/`CAMERA_KEY_ICON`/`CAMERA_LOOP_ICON` per peg in vertical timeline

### Added
- **Task 24** in ANIMATIC_TASKS: Startup popup come hub scene management (New/Load/Subscene + Cancel contestuale + Import Assets in File > Import)

### Upstream candidates
- Set Key (Z) non mostra diamante su peg columns — `cellselectioncommand.cpp` una riga

---
## [2026-05-02d] — Fix writeRoomList/renameRoom crash + Storyboard layout template

### Fixed
- `writeRoomList` salta rooms con path vuota: le "fallback rooms" create durante
  un workflow switch prima che `makePrivate()` assegni i path scrivevano "." in
  layouts.txt corrompendolo → crash "room not found" al prossimo switch
- `renameRoom` guarded: salva il file INI solo se la room ha un path valido
  (preveniva crash al rename su room senza path)

### Modified
- Template Storyboard (`stuff/profiles/layouts/rooms/Storyboard/ztoryc.ini` e
  `browser.ini`) aggiornati con il layout produzione di francobianco: Board a
  sinistra, Viewer al centro, RightPanel a destra, Animatic in basso
  Gerarchia corretta: `-1 1 [ [ 0 1 2 ] 3 ]` (era `-1 1 [ [ [ 0 1 2 ] 3 ] ]`)

### Notes
- Il meccanismo di fallback `getRoomsFile()` (user dir → template dir) è già
  in produzione. Nuovi utenti e CI builds ora ricevono il layout corretto.
- Task 23 completato per Storyboard; StopMotion già aveva template DragonFrame.

---
## [2026-05-02c] — Fix workflow combo "Open Existing Scene" + rimossa regressione StopMotion

### Fixed
- **Bug workflow popup "Open Existing Scene"** (`startuppopup.cpp`): `CommandManager::execute(cmd)`
  spostato DOPO `IoCmd::loadScene` invece che prima — il `switchRoomChoice` (clearRooms +
  readSettings) avveniva mentre la scena era ancora in caricamento, causando schermata nera
  per StopMotion e potenziali interferenze per tutti i workflow.
- **Regressione StopMotion rimossa**: in un passaggio intermedio era stato erroneamente
  eliminato "Stop-Motion Mode" dai combo del popup — ripristinato correttamente.

### Notes
- Feedback utente ricevuto: chiedere conferma PRIMA di rimuovere funzionalità esistenti.

---
## [2026-05-02b] — Task 19 completato: cursore resize su video e audio track

### Fixed
- **SIGABRT dyld crash** (`build_and_deploy.sh`): le dylib nella root di `build/`
  sono stale; tutti i path aggiornati a usare le sottodirectory (`build/tnzcore/`,
  `build/toonzlib/`, ecc.) dove ninja deposita le build aggiornate.
- **Compile error `mx` undefined** (`ztoryanimatic.cpp`): `int mx = e->x() - kLabelW`
  spostato all'inizio di `ZtoryAnimaticTrack::mouseMoveEvent`; rimossa la ridefinizione
  duplicata nel blocco RazorTool.

### Added
- **Task 19 — Cursore resize audio track** (`ztoryanimatic.cpp`): `ZtoryAudioTrack`
  ora mostra `SizeHorCursor` quando il mouse si avvicina ai bordi di un segmento audio.
  Implementato via `setAttribute(Qt::WA_Hover)` + override di `event()` con
  `QEvent::HoverMove`/`HoverLeave`. Helper statico `nearSegmentEdge()` aggiunto.
- **Task 19 — Cursore resize video track** (`ztoryanimatic.cpp`): `ZtoryAnimaticTrack`
  mostra `SizeHorCursor` quando il mouse si avvicina al bordo destro di un blocco shot
  in `mouseMoveEvent` (zona ±6px). Cursore resettato in `leaveEvent`.

### Modified
- **`SystemVar.ini`** (`toonz/install/`): risolto conflict rebase; accettata versione
  remote con chiavi `ZTORYC*` e path `/Applications/Ztoryc/Ztoryc_stuff`.
- **`postinstall-script.sh`**: fallback per entrambi i nomi file (`ztorycstuffdirloc` /
  `tahoma2dstuffdirloc`) dall'AppleScript dell'installer.

---
## [2026-05-02] — Task 16/17/18 completati; Task 19 cursor ancora irrisolto
### Added
- **Task 16:** Workflow combo nel tab "Open Existing Scene" della StartupPopup —
  scelta workflow viene applicata prima di caricare la scena; entrambi i combo
  (Create + Load) si sincronizzano al workflow corrente all'apertura del dialog.
- **Task 16:** Voci workflow nel menu Windows ora sono checkable e mostrano
  spunta sul workflow attivo; `updateWorkflowMenuChecks()` chiamata ogni volta
  che `switchRoomChoice()` cambia il layout.
- **Task 17:** All'apertura di una sotto-scena (doppio-click shot nell'animatic),
  il play-range viene impostato automaticamente su `[0, subFrameCount-1]` via
  `XsheetGUI::setPlayRange()`.
- **Task 18:** Zoom con rotella del mouse spostato dal `ZtoryAnimaticTrack` al
  `ZtoryAnimaticRuler`; il track ignora ora la wheel (`e->ignore()`). Aggiunto
  signal `zoomChanged(double)` al ruler, connesso a `onZoomChanged` nel panel.
### Notes
- **Task 19 (cursor resize audio):** `setMouseTracking(true)` aggiunto al
  costruttore di `ZtoryAudioTrack`; logica hover aggiornata per cappare `xRight`
  alla larghezza del widget. Risultato non ancora visibile — da investigare.
- Reverted tutte le modifiche problematiche della sessione precedente (audio
  cut/paste/undo + `#include "ztoryanimatic.h"` in storyboardpanel.cpp che
  causava board panel regression) con `git restore .` prima di re-implementare.

---
## [2026-05-01d] — Fix audio toggle 12d + onion skin rimosso + mark-out default + nuovi task
### Fixed
- **Bug 12d — Audio toggle in sub-scena** (`sceneviewer.cpp`, `ztoryanimatic.h/.cpp`):
  `execute()` di `MI_ToggleMainAudio` ora chiama `stopNativeAudio()` sul controller
  per fermare lo streaming avviato da `onNativePlayingStatusChanged`. Aggiunto
  `restartNativeAudioIfPlaying()` per ri-abilitare l'audio durante il play quando
  si toglie il mute.
- **Mark-out a fine timeline all'avvio** (`ztoryanimatic.cpp`): aggiunto
  `resetPlayRangeToFull()` su `sceneSwitched` in `ZtoryAnimaticPanel` — il mark-out
  si posiziona automaticamente all'ultimo frame della scena ad ogni caricamento.
### Removed
- **Onion skin dalla toolbar animatic** (`ztoryanimatic.cpp`): rimosso `onionBtn`
  e i relativi connect dalla toolbar di `ZtoryAnimaticPanel`.
### Added (ANIMATIC_TASKS)
- Task 16–22: Workflow startup, stop marker immediato, zoom ruler, cursore resize,
  taglia/copia/incolla audio, volume per traccia, transizioni.

---
## [2026-05-01c] — Undo/Redo CRUD completo + fix refresh anteprime Board
### Added
- **Task 13 — Undo/Redo completo** (`ztoryundo.h`, `storyboardpanel.cpp`, `ztoryanimatic.cpp`):
  - Nuovo file `ztoryundo.h`: `ZtoryShotSnap {ShotData, TXshLevelP, int duration}` +
    classe `UndoBoardState` (before/after snapshot, chiama `restoreFromSnapshot` su undo/redo)
  - `StoryboardPanel::captureSnapshot()` / `restoreFromSnapshot()`: full rebuild xsheet+Board
    da un vettore di snapshot; `TXshLevelP` mantiene in vita livelli eliminati per undo-of-delete
  - Board: undo su Add, Delete, Move (drag&drop), Paste, Merge, Match Duration
  - Board: undo duration con timer di coalescenza 600ms (`m_durationCommitTimer`) — un solo
    item per "sessione di editing" invece di uno per ogni tick della spinbox
  - Animatic: undo su Delete, Cut, Paste, Duration resize, Merge, MergeWithNext, Razor
  - Fix anti-polluzione stack: `ColumnCmd::deleteColumns(..., withoutUndo=true)` per tutti i
    delete interni; `TUndoManager::manager()->popUndo(1)` dopo `ColumnCmd::cloneChild()` in Razor
  - `findBoardPanel()`: helper statico in ztoryanimatic.cpp tramite `QApplication::allWidgets()`
### Fixed
- **Bug `updatePreview` colonna errata** (`storyboardpanel.cpp:updatePreview`):
  usava `shotIdx` come indice colonna xsheet invece di `shot.data.xsheetColumn`.
  Poteva rendere la thumbnail dello shot sbagliato quando ordine shot ≠ ordine colonne.
- **Anteprime Board stantie dopo disegno** (`storyboardpanel.cpp`):
  - `showEvent`: aggiunto `onRefreshPreviews()` anche quando Board torna visibile con shots
    già caricate (prima refresh solo a primo caricamento). Fix caso: disegno in sub-scena →
    cambio room → Board mostra thumbnail aggiornate.
  - `xsheetSwitched` handler: quando si entra in una sub-scena, `xsheetChanged` viene ora
    connesso anche a `m_panelDetectTimer->start()` — ogni modifica (disegno/cancella) riavvia
    il timer da 1s; al timeout si ri-renderizza la thumbnail dello shot corrente.

---
## [2026-05-01b] — Fix testi Board persi al reload, cleanup Export Animatic
### Fixed
- **BUG critico: testi dialog/action/notes persi al salvataggio** (`storyboardpanel.cpp`):
  aggiunta `syncWidgetsToData()` chiamata all'inizio di `saveZtoryc()`. Il handler
  `dataChanged` aggiornava solo `shotLabel`, mai `data.panels[pi].dialog/action/notes`,
  quindi il salvataggio XML scriveva dati stale. Ora prima del write tutti i widget
  vengono copiati nel data model.
### Modified
- **Export Animatic dialog**: rimosso pulsante "Output Settings…" (bloccava l'UI con
  `ApplicationModal`). Sostituito con label read-only che mostra formato/fps/risoluzione
  correnti + nota "change via Render > Output Settings".

---
## [2026-05-01] — Branding Ztoryc, fix crash panel, fix reorder shot
### Added
- `DockWidget::setEmbedded()` in `docklayout.h`: setta `m_floating=false`,
  `m_parentLayout=nullptr`, rimuove margini floating — impedisce drag-to-float
  sui panel embedded in `ZtoryLeftPanel` / `ZtoryRightPanel`
- Doppio click su `ComboViewerPanel` in shot mode → esce dalla shot mode
  (eventFilter su `ZtoryAnimaticViewerPanel`)
- Cartella `Ztoryc` su Google Drive con copia CHANGELOG e ANIMATIC_TASKS
### Fixed
- **Crash QTextEdit stack overflow**: `ZtoryScriptView::m_textEdit` ora ha
  `setMinimumSize(80,60)` + `setLineWrapMode(NoWrap)`, evita layout ricorsivo
  a larghezza zero quando il panel è nascosto in QStackedWidget
- **Board "sganciato"**: panel embedded in ZtoryLeftPanel/ZtoryRightPanel ora
  usano `setEmbedded()` + `getTitleBar()->hide()` → non più draggabili come
  panel floating; null-guard in `mouseDoubleClickEvent` e `maximizeDock`
- **Room duplicate al Reset**: `layouts.txt` (template + utente) aggiornato
  a soli `ztoryc.ini` + `browser.ini`; `currentRoom.txt` → `ZTORYC`; rimossi
  vecchi file `animatic.ini`, `board.ini`, `room1-6.ini` da entrambe le dir
- **Reorder shot apriva lo shot sbagliato**: in `onMoveShot()`, dopo lo
  spostamento fisico delle celle xsheet, aggiornamento di
  `m_shots[i].data.xsheetColumn = i` per tutti gli slot
### Modified (altra istanza Claude)
- **About dialog** (`aboutpopup.cpp`, `toonz.qrc`): titolo "About Ztoryc",
  logo `ztoryc_about.png` (400×400, scalato 80×80), link GitHub
  `github.com/matitanimata/ztoryc`, licenza GPL v3, note FFmpeg (LGPLv2.1)
  + Rhubarb Lip Sync (MIT), ringraziamenti team Tahoma2D
- **Splash screen** (`Resources/tahoma2d_splash.svg`): versione corretta
  da `v1.0.0` a `v0.2.0`

---
## [2026-04-25b] — revert side-fix, mantenuto solo Homebrew SuperLU

### Modified
- Revert di `tlin_superlu_wrap.cpp` e `plasticdeformer.cpp` allo stato pre-sessione
  (commit `d3ac737e3`). Le modifiche aggiuntive (safety net sigsetjmp,
  inversa analitica 4×4, validazioni colptr) erano superflue una volta
  passato a Homebrew SuperLU 7 e creavano potenziale per bug subdoli.
- `BundleInfo.plist.in`: rimosso `LSRequiresCarbon=true` — bloccava
  l'AutoFill UI. Era stato aggiunto come tentativo, ma il vero fix del
  drag crash era già il cambio a Homebrew SuperLU.
- `storyboardpanel.cpp::updatePreview()`: ripristinato
  `IconGenerator::renderXsheetFrame()` (i preview thumbnail si aggiornano
  di nuovo al cambio xsheet — ora safe con Homebrew SuperLU).

### Notes
- Unica modifica essenziale del fix di oggi rimasta: `CMakeLists.txt`
  `WITH_SYSTEM_SUPERLU=ON` di default su macOS (commit `fc625e448`).
- Crash open: `TProjectManager::notifyListeners()` SIGBUS durante click
  in DvDirTreeView (dangling listener pointer). Da indagare in nuova
  sessione — chiunque chiama `addListener` su `TProjectManager::instance()`
  e non rimuove nel distruttore.
- Commit revert: `48b42a8d3 revert: keep only essential SuperLU fix (Homebrew)`

---
## [2026-04-25] — fix crash plastic deformer + drag (SuperLU bundled vs Homebrew)

### Fixed
- **PlasticDeformer SIGSEGV su arm64**: il bundled SuperLU 4.1 ha UB latente
  che su Apple Silicon nativo corrompe memoria — `dgstrf` crasha direttamente
  in `compileStep1`/`initializeStep2`, e (sorpresa) la stessa corruzione
  emerge come `BUG IN CLIENT OF LIBPLATFORM: recursive os_unfair_lock` nel
  drag-and-drop di scene nel cast (NSCoreDragManager).
- **Root cause**: dopo il rebranding Ztoryc, `WITH_SYSTEM_SUPERLU` di default
  su macOS è rimasto `OFF` → linker ha incluso libsuperlu_4.1.a bundled
  invece di `libsuperlu.7.dylib` di Homebrew (che la vecchia Tahoma2D.app
  funzionante usava dinamicamente).

### Modified
- `toonz/sources/CMakeLists.txt`: default `WITH_SYSTEM_SUPERLU=ON` su macOS.
  Richiede `brew install superlu` una sola volta.
- `toonz/cmake/BundleInfo.plist.in`: re-aggiunto `LSRequiresCarbon=true`
  (era nel vecchio Tahoma2D, dropped nel rename Ztoryc).
- `toonz/sources/tnzext/tlin/tlin_superlu_wrap.cpp`: guard difensivi
  permanenti in `factorize()` — validazione NaN/Inf valori, bounds-check
  rowind, monotonia colptr, safety net `sigsetjmp`/`siglongjmp` intorno
  a `dgstrf`, fix swap argomenti `relax`/`panel_size`.
- `toonz/sources/tnzext/plasticdeformer.cpp`: rimpiazzato SuperLU per la
  factorizzazione 4×4 per-faccia in `initializeStep2`/`deformStep2` con
  inversa analitica closed-form (Schur complement) — più veloce e azzera
  esposizione UB SuperLU per il sistema per-triangolo.
- `toonz/sources/toonz/storyboardpanel.cpp`: rimosso workaround mesh-column
  in `updatePreview()` (non più necessario), thumbnail sempre via `getIcon()`.

### Notes
- L'app è arm64 nativo (non Rosetta come sospettato inizialmente). Il vecchio
  Tahoma2D.app funzionante era anch'esso arm64 ma linkava dinamicamente la
  SuperLU 7 di Homebrew → da qui il diverso comportamento.
- Setup post-pull per dev macOS: `brew install superlu` (one-shot).
- Commit: `fc625e448 fix: PlasticDeformer + drag crashes — switch to system SuperLU on macOS`

---
## [2026-04-24] — resetOnSeqChange: riavvio contatore SH per sequenza

### Added
- **`NumberingConfig::resetOnSeqChange`** — nuovo campo bool (default `false`).
  Quando `true` (solo Sequence style): il contatore SH si azzera a `startNumber`
  ad ogni cambio di sequenza (SQ01→SH010, SQ02→SH010…). Quando `false`:
  numerazione globale continua tra tutte le sequenze.
- **`m_resetOnSeqChangeCB`** in `StartupPopup` — checkbox "Restart shot # at each
  new sequence", visibile solo in Sequence style; stato salvato in `NumberingConfig`.
- **`resetOnSeqCB`** in `StoryboardPanel::onNumberingConfig()` — stessa checkbox
  nel dialogo di configurazione numerazione del Board.

### Modified
- **`StoryboardPanel::renumberAll()`** — in Auto mode, le sequenze sopravvivono
  al renumber (solo SH cambia). I nuovi shot senza `sequenceId` ereditano la
  sequenza dello shot precedente. Con `resetOnSeqChange`, `shotIdx` è relativo
  alla sequenza (non globale).
- **`StartupPopup::onCreateButton()`** — in Sequence mode, crea una sequenza
  default "sq01" e vi assegna tutti gli shot iniziali (campo SQ pre-popolato).

### Fixed
- **Crash SIGABRT su import scena con Plastic Deformer** — `ZtoryAnimaticTrack::
  refreshFromScene()` e `ZtoryStoryStripPanel::refreshFromScene()` chiamavano
  `IconGenerator::getIcon()` sincronicamente durante `xsheetChanged`. Durante
  l'import di una scena, la xsheet non è ancora stabilizzata: il rendering
  triggerava `PlasticDeformerStorage::process()` → `PlasticDeformer::initialize()`
  → `tlin::factorize()` → `StatFree()` su SuperLU Matrix non inizializzata → crash.
  Fix: entrambi gli handler `xsheetChanged` wrappati con `QTimer::singleShot(0)`
  per differire l'esecuzione all'iterazione successiva dell'event loop.
  Rimossa anche la chiamata ridondante `updateAllPreviews()` da
  `ZtoryModel::onXsheetChanged()` (violava regola AGENTS.md).

---
## [2026-04-23] — Numerazione SQ/SH, rename app Ztoryc, fix firma bundle

### Added
- **Sequenze editabili nel Board** — campo SQ separato e editabile per ogni shot.
  Digitando un numero di sequenza (es. "020") viene assegnata la sequenza a quello
  shot e a tutti i seguenti fino al prossimo cambio manuale (`seqLabelEdited` cascade).
- **`ZtoryModel::findOrCreateSequence()`** — trova o crea `SequenceData` by label,
  usata sia dal cascade handler che dal renumber automatico.
- **`ZtoryModel::assignShotLabel()` (static)** — algoritmo midpoint condiviso tra
  `ZtoryModel` e `StoryboardPanel` per generare label senza duplicati al momento
  dell'inserimento (Keep mode → SH015 tra SH010 e SH020; Auto mode → rinumera tutto).

### Fixed
- **Doppio click entra e ritorna subito** — `PanelWidget::mouseDoubleClickEvent`
  chiamava `QFrame::mouseDoubleClickEvent(e)` che propagava l'evento a
  `StoryboardPanel::mouseDoubleClickEvent` il quale eseguiva `MI_CloseChild`.
  Fix: sostituito con `e->accept()`.
- **Shot duplicato al momento dell'inserimento** — in modalità Auto, `renumberAll()`
  usava `ZtoryModel::m_shots` come sorgente invece della lista locale del Board,
  ottenendo l'indice errato. Fix: algoritmo statico opera sulla lista locale del Board.
- **Campo SH mostrava "SH - sq01_sh010"** — `setShotNumber()` ora separa SQ e SH
  sul separatore `_`, mostra solo la parte numerica in ciascun campo e salva il
  prefisso in `m_storedShotPrefix`/`m_storedSeqPrefix` per la ricostruzione.
- **`renumberAll()` Auto + Sequence style** — `cfg.shotName(i)` restituisce
  "SQ001_SH010"; ora viene splittato correttamente: SH → `shotLabel`, SQ → `sequenceId`.

### Modified
- **Rename app: Tahoma2D → Ztoryc** — bundle ID `io.github.ztoryc.Ztoryc`,
  `CFBundleName/ExecutableName = Ztoryc`, versione 1.0.0.
  File cambiati: `CMakeLists.txt` (target), `BundleInfo.plist.in`, `main.cpp`,
  `Ztoryc.entitlements`, `build_and_deploy.sh`.
- **`build_and_deploy.sh`** — firma corretta senza `--deep` (dylib firmate
  singolarmente prima del bundle); `xattr -cr` prima della firma; `rm -rf profiles/`
  per evitare "unsealed contents in bundle root"; copia automatica `SystemVar.ini`
  se mancante; copia dylib secondarie dal build tree.

### Notes
- `Ztoryc.app/profiles/` viene ricreata dall'app ad ogni avvio — è normale,
  non invalida la firma al lancio (il seal è valido al momento di `open`).
- `SystemVar.ini` punta a `/Volumes/ZioSam/.../stuff` — path assoluto,
  non portabile; da parametrizzare per distribuzione.
- Per permessi TCC stabili: aggiungere Ztoryc.app al Full Disk Access in
  System Settings → Privacy & Security.

---
## [2026-04-23b] — Branding Ztoryc completato

### Modified
- **`tversion.h`** — `applicationName = "Ztoryc"`, versione 1.0 (era Tahoma2D 1.6).
  Propaga su titolo finestra, startup popup, about dialog, tutti i log.
- **`tahoma2d_splash.svg`** — icona Ztoryc (PNG embedded base64) + wordmark +
  tagline "STORYBOARD · ANIMATIC · ANIMATION" su sfondo scuro.
- **`tahoma2d_startup.svg`** — banner orizzontale: icona + "ZTORYC" in giallo `#F5B800`.
- **`tipspopup.cpp`** — titolo "Ztoryc Tips".
- **`mainwindow.cpp`** — update checker punta a github.com/matitanimata/ztoryc.
- **`main.cpp`** — tips popup disabilitato; update check automatico disabilitato
  (contenuti ancora riferiti a Tahoma2D).
- **`Ztoryc.icns`** — generato da `ztoryc_icon.png` con tutte le risoluzioni macOS
  (16×16 → 1024×1024).

### Notes
- `toonz.qrc` va touchato prima di ogni modifica SVG per forzare la ricompilazione
  delle risorse Qt: `touch toonz/sources/toonz/toonz.qrc && ./build_and_deploy.sh`

---
## [2026-04-20] — Fix: 7 crash + audio export + workflow switch lento

### Fixed
- **Crash FlipConsole::doButtonPressed (QThread::isRunning SIGSEGV)** — durante
  `clearRooms()` i widget venivano nascosti e `hideEvent` → `setActive(false)` →
  `pressButton(ePause)` → `doButtonPressed` iterava `m_visibleConsoles` con pointer
  potenzialmente stale. Fix: `setActive(false)` ora abortisce direttamente il
  `PlaybackExecutor` inline invece di passare per click→signal→slot chain.
  (`flipconsole.cpp`)

- **Crash ~FlipConsole dangling pointer** — `m_visibleConsoles` non veniva pulita
  nel distruttore. Aggiunto `~FlipConsole()` che rimuove `this` dalla lista.
  (`flipconsole.cpp`, `flipconsole.h`)

- **Crash SceneViewer/FxGadgetController (TTool::m_viewer dangling)** — al load di
  una scena il `SceneViewer` veniva distrutto ma `TTool::m_viewer` non veniva
  azzerato → crash in `onFxSwitched`. Fix: `SceneViewer::~SceneViewer()` chiama
  `TTool::onViewerDestroyed(this)` che azzera tutti i tool che puntano a quel viewer.
  (`sceneviewer.cpp`, `tool.cpp`, `tool.h`)

- **Crash PlasticDeformer SuperLU (dgstrf NaN)** — triangoli degeneri in una mesh
  producevano NaN/Inf da `ortCoords()` che venivano passati a SuperLU → crash.
  Fix: guard `isfinite()` in `initializeStep2()` salta la fattorizzazione per facce
  degeneri; `deformStep2()` usa posizione invariata quando `m_invF[f]` è null.
  (`plasticdeformer.cpp`)

- **Crash Room::save() da switchRoomChoice re-entrante** — `Room::load()` chiama
  `qApp->processEvents()` che fa scattare il `QTimer::singleShot(0)` che resettava
  `m_isHandlingWorkflow=false`, permettendo un secondo `switchRoomChoice` annidato
  che settava poi `m_isSwitchingRooms=false`. L'outer `readSettings` entrava in
  `makePrivate(rooms)` con pointer dangling → SIGSEGV. Fix: guard
  `if (m_isSwitchingRooms) return;` all'inizio di `switchRoomChoice`.
  (`mainwindow.cpp`)

- **Audio export oltre lunghezza shot** — `vsf - shotR0` usava `getVisibleStartFrame()`
  invece di `getStartFrame()` per calcolare la posizione nella colonna destinazione.
  Fix: usa `cl->getStartFrame() - shotR0`.
  (`storyboardpanel.cpp`)

- **"Load Audio" non apriva il dialog su macOS** — parent `this` invece di `nullptr`
  rendeva il dialog invisibile dietro la finestra principale. (`ztoryanimatic.cpp`)

- **Audio stale tra scene diverse** — `requireSoundTrack()` usava la cache della
  scena precedente al cambio scena. Fix: `invalidateSoundTrack()` chiamato nel
  handler `sceneSwitched`. (`ztoryanimatic.cpp`)

### Performance
- **Workflow switch verso Storyboard lento (1–3 s)** — `makeSound()` bloccava il
  main thread perché veniva chiamato da `singleShot(0)` che scattava dentro
  `qApp->processEvents()` di `Room::load()`. Fix: `preBuildSoundTrackAsync()` esegue
  `makeSound()` in un `std::thread` detached; il risultato è consegnato al main
  thread via `QMetaObject::invokeMethod(QueuedConnection)`. Zero blocking.
  (`ztoryanimatic.cpp`, `ztoryanimatic.h`)

### Modified
- `flipconsole.cpp` — `~FlipConsole()`, `setActive(false)` riscritta
- `flipconsole.h` — aggiunto `~FlipConsole()`
- `sceneviewer.cpp` — `~SceneViewer()` chiama `TTool::onViewerDestroyed`
- `tool.cpp` / `tool.h` — aggiunto `TTool::onViewerDestroyed(Viewer*)`
- `plasticdeformer.cpp` — guard triangoli degeneri in step2
- `mainwindow.cpp` — re-entrancy guard in `switchRoomChoice`
- `storyboardpanel.cpp` — fix audio export frame offset
- `ztoryanimatic.cpp` / `.h` — Load Audio fix, sceneSwitched invalidate, async sound build

### Notes
- Il ritardo al primo switch verso Storyboard è fisiologico: il Board carica le
  anteprime (500ms timer) e l'audio viene costruito in background. Non è un bug.

---
## [2026-04-19b] — Fix: double-update Board dopo operazioni Animatic

### Fixed
- **Razor, AddShot, MergeWithNext dall'Animatic aggiungevano uno shot vuoto extra nel Board**
  - Root cause: stessa classe di bug del merge double-removal. Dopo `resequenceXsheet()`
    → `modelReset()` → `onModelResequenced()` → `refreshFromScene()` il Board era già
    corretto (4 shot dopo razor), poi arrivava `emit shotAdded(newCol)` →
    `onShotInserted()` inseriva un altro shot vuoto (senza sub-scene) → Board a 5 shot.
  - Fix: rimossi `emit shotAdded()`/`emit shotRemovedAt()` da `onRazorRequested()`,
    `onAddShot()`, `onMergeWithNext()`. Il Board si sincronizza esclusivamente via
    `resequenceXsheet()` → `modelReset()` → `onModelResequenced()` (xsheet count check).

### Modified
- `ztoryanimatic.cpp` — rimossi 3 emit ridondanti post-resequenceXsheet

---
## [2026-04-19] — Shared clipboard e shared selection Board ↔ Animatic + fix merge double-removal

### Added
- **Shared clipboard Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `ZtoryClipEntry` struct e `m_sharedClip` in `ZtoryModel` — unica source of truth per clipboard
  - Board (`onCopyShot`, `onCutShot`, `onCloneShot`): scrive sempre su `ZtoryModel::setSharedClip()`
  - Animatic (`onCopyShots`, `onCutShots`, `onCloneShots`): usa già `ZtoryModel::sharedClip()`
  - `pasteSharedClipToBoard()` — helper statico in `storyboardpanel.cpp` che replica
    la logica di `pasteFromClip()` usando il `cloneChildToPosition()` locale
  - Board `onPasteShot()`: shared clip ha sempre priorità su `m_clipboard` locale
    (fix bug: `m_clipboard` stale con 3 shot causava incolla 3 invece di 1 dopo copy da Animatic)

- **Shared selection Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `m_sharedSelection` (set di xsheet columns) in `ZtoryModel` con getter/setter
  - Animatic: `selectionChanged` signal → `ZtoryModel::setSharedSelection()`
  - Board `onPanelClicked()`: converte `m_selectedIndices` → xsheet columns → `setSharedSelection()`
  - Merge cross-panel: seleziona in Animatic → merge button Board funziona (e viceversa)
  - Fallback "last panel wins": vince sempre l'ultima interazione utente

### Fixed
- **Bug merge cross-panel: double-removal nel Board** (`storyboardpanel.cpp`, `ztoryanimatic.cpp`)
  - Root cause: `onModelResequenced()` usava `ZtoryModel::m_shots.size()` come riferimento
    ma quella dimensione è stale dopo operazioni copy/paste/clone che bypassano
    `ZtoryModel::addShot()/removeShot()`. Se stale ≠ Board count → `refreshFromScene()` (Board → 5 shot)
    poi arrivava anche `emit shotRemovedAt(4)` → `onShotRemovedAt()` → rimozione extra (Board → 4 shot)
  - Fix 1: `onModelResequenced()` conta le colonne child-level direttamente dall'xsheet (ground truth),
    non da `ZtoryModel::m_shots.size()`
  - Fix 2: Animatic `onMergeShots()`: rimosso `emit shotRemovedAt()` — il Board si sincronizza già
    via `resequenceXsheet()` → `modelReset()` → `onModelResequenced()`
  - Fix 3: Board `onMergeShots()`: `m_updating=true` attorno all'emit di `shotRemovedAt()` per
    prevenire self-processing (double-removal anche per merge nativo del Board)

- **Bug clipboard priorità**: Board usava `m_clipboard` locale (stale) invece dello shared clip
  - Fix: in `onPasteShot()` lo shared clip ha sempre la precedenza; `m_clipboard` è solo fallback

### Modified
- `ztorymodel.h` — aggiunti `ZtoryClipEntry`, `m_sharedClip`, `m_sharedSelection` + `#include <set>`
- `ztoryanimatic.h` — rimossi `AnimClipEntry`/`m_animClip`; commento shared clipboard
- `ztoryanimatic.cpp` — riscritta gestione clipboard; merge fix; connect selectionChanged
- `storyboardpanel.cpp` — shared clipboard write in copy/cut/clone; paste fallback; merge fix;
  shared selection write in onPanelClicked; pasteSharedClipToBoard() helper

---
## [2026-04-17] — Fix: crash BrushToolOptionsBox + AutoFill restore

### Fixed
- **`tooloptions.cpp` — crash on sub-xsheet entry and app close (`BrushToolOptionsBox::updateStatus`)**
  - Root cause: `updateStatus()` era chiamata sincronamente durante la signal chain
    dell'xsheet switch (`openSubXsheet` / `saveSceneIfNeeded`); in quel momento
    `m_pltHandle->getPalette()` può restituire un puntatore temporaneamente invalido
    → SIGSEGV in `rebuildAutoFillStyleCombo`.
  - Fix: entrambe le chiamate critiche (`rebuildAutoFillStyleCombo` +
    `notifyToolComboBoxListChanged`) deferite con `QTimer::singleShot(0, this, lambda)`,
    così vengono eseguite solo dopo che la signal chain si è completamente disfatta.
  - Aggiunto change-detection (`m_lastPalette`, `m_lastPaletteStyles`) per evitare
    rebuild superflui.
  - `try-catch(...)` non era sufficiente: SIGSEGV è un segnale Unix, non un'eccezione C++.

### Modified
- **`tooloptions.cpp`** — `BrushToolOptionsBox::updateStatus()` con QTimer deferred rebuild
- **`tooloptions.h`** — aggiunti `m_lastPalette` / `m_lastPaletteStyles` a `BrushToolOptionsBox`
- **`toonzrasterbrushtool.cpp`** — `rebuildAutoFillStyleCombo` ripristinato con lista completa
  palette; fill code ripristinato al comportamento originale (`getPaint() == 0`)
- **`toonzrasterbrushtool.h`** — `rebuildAutoFillStyleCombo(TPaletteP pal)` dichiarazione ripristinata

### Notes
- AutoFill "Fill Style" combo ora mostra di nuovo tutti i colori della palette (non solo "+1")
- Fill con antialias ripristinato al comportamento originale (era stato rimosso per errore)
- Savebox fix mantenuto: `sb = sb + m_strokeRect` per evitare scan area 1×1 al primo stroke

---
## [2026-04-16] — Fix: render preview frame bianco/trasparente

### Fixed
- **`toonz/sources/tnzbase/trasterfx.cpp` — `enlargeToI()` UB con `TConsts::infiniteRectD`**
  - Root cause definitivo identificato e corretto.
  - `enlargeToI(TRectD &r)` applica `tfloor`/`tceil` (che fanno `(int)(x)`) a `TConsts::infiniteRectD = TRectD(-DBL_MAX,-DBL_MAX,DBL_MAX,DBL_MAX)`. Cast `(int)(±DBL_MAX)` è undefined behavior; su questo Mac produce `(int)(DBL_MAX)=-1` e `(int)(-DBL_MAX)=0`, corrompendo il rect a `(-1,-1)-(0,0)`.
  - `ColorCardFx::doGetBBox` ritorna `infiniteRectD` → dopo `enlargeToI` il bbox di `overFx` diventa `(-1,-1)-(0,0)` → `interestingRect` = 1×1 pixel → tutto il render è 1 pixel trasparente.
  - **Fix**: guard in `enlargeToI` che skippa la conversione se qualsiasi coordinata supera `INT_MAX/2`:
    ```cpp
    const double kMaxSafeInt = static_cast<double>(std::numeric_limits<int>::max() / 2);
    if (r.x0 < -kMaxSafeInt || r.x1 > kMaxSafeInt || r.y0 < -kMaxSafeInt || r.y1 > kMaxSafeInt)
        return;
    ```

### Modified
- Rimossi tutti i log diagnostici `std::cerr` aggiunti nelle sessioni precedenti da:
  - `trasterfx.cpp` (logger rimosso dall'agent)
  - `tcolumnfx.cpp`, `scenefx.cpp`, `previewer.cpp`, `sceneviewer.cpp` (rimossi con Python script)

### Notes — Diagnosi completa (path del bug)
```
ColorCardFx::doGetBBox → restituisce TConsts::infiniteRectD
  → TRasterFx::getBBox chiama enlargeToI(infiniteRectD)
    → (int)(DBL_MAX) = -1 [UB sul Mac]
    → temp = TRectD(-1,-1,0,0)
    → myIsEmpty(-1,-1,0,0) = false (getLx()=1 ≥ 1)
    → r corrotto a (-1,-1)-(0,0)
  → overFx.compute: interestingRect = tileRect * (-1,-1,0,0) = 1×1 pixel
  → tutta la chain renderizza 1 pixel trasparente → frame bianco in output
```
Confirmato con log14: `[compute_extract] fx=overFx tile=1920x1080 bbox=(-1,-1)-(0,0) interesting_tile=1x1`

### Nuovo bug da investigare (sessione successiva)
- Con 2+ livelli il render a volte produce frame **nero** (intermittente)
- Con 3+ livelli il terzo livello quasi mai viene renderizzato
- In visualizzazione normale il 3° livello appare **sotto** il 2° (z-order invertito)
- Probabile causa: `TImageCombinationFx::doCompute` gestisce il livello più alto come
  "background" (render diretto sulla tile) e quelli sotto con `allocateAndCompute`.
  Se l'ordering dei port è invertito rispetto all'atteso, l'ordine di compositing
  è sbagliato. Da verificare in `binaryFx.cpp` e `scenefx.cpp` (`makePF`).

### Upstream candidate
- Il fix di `enlargeToI` è pulito e applicabile a Tahoma2D upstream: il commento
  originale diceva "the rect may become empty" ma non lo proteggeva. Fix corretto
  e backward-compatible.

---
## [2026-04-15b] — Diagnosi: render preview produce raster TRASPARENTE (bug Ztoryc-specifico)

### Modified
- `toonz/sources/toonz/sceneviewer.cpp` — `drawPreview()`:
  - Camera usata per `rasterToStageRef` cambiata da `scene->getCurrentCamera()`
    → `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()` per
    allineare la camera a quella usata dal Previewer (in test erano già
    equivalenti 1920x1080, ma fix coerente con `Previewer::updateCamera()`).
  - Aggiunto logging diagnostico (ogni 60 frame): row, dimensioni camera
    root/sub, validità raster, pixel sample TL/Center/BR.
- `toonz/sources/toonz/previewer.cpp` — logging diagnostico in:
  - `updateCamera()`: cameraRes, renderArea, flag subcamera
  - `refreshFrame()`: previewRect, renderArea, motivi abort
  - callback render completed: dimensione raster + pixel centrale

### Notes — Scoperta chiave
Il raster **NON è bianco, è totalmente TRASPARENTE**:
```
[Previewer::renderCompleted] frame=0 rasSize=1920x1080 centerPix=(0,0,0,0)
[drawPreview] ras=valid rasSize=1920x1080 TL=(0,0,0,0) C=(0,0,0,0) BR=(0,0,0,0)
```

Tutti i pixel sono `RGBA=(0,0,0,0)` — alpha zero. Il viewer compone
il trasparente sopra `m_visualSettings.m_blankColor` (bianco di default),
**facendoci vedere bianco**.

Quindi il bug NON è:
- ❌ camera mismatch (root e sub entrambe 1920x1080)
- ❌ scheduling/trasporto (`refreshFrame` parte, `renderCompleted` firma,
  raster arriva valido al viewer con dim corretta)
- ❌ legato alle sub-scene (confermato dall'utente: succede anche
  renderizzando un disegno direttamente nel main xsheet)

**Il bug è Ztoryc-specifico**: la stessa scena aperta in Tahoma2D vanilla
renderizza correttamente. Una modifica di fork introdotta da Ztoryc rompe
il render preview → da bisettare rispetto a upstream `tahoma2d/tahoma2d`.

### Prossima sessione — piano concreto
1. **Diff con upstream Tahoma2D** — `git diff upstream/master -- toonz/sources/toonz/previewer.cpp toonz/sources/toonz/sceneviewer.cpp toonz/sources/common/tfx/` per vedere cosa Ztoryc ha toccato nel path render preview.
2. **Ricerca aree sospette**: `scenefx.cpp`, `trop.cpp`, `trasterfx.cpp`,
   qualsiasi modifica alla composizione `makeOver(bgCard, fx)`.
3. **Bisect**: se il diff è grande, `git bisect` partendo da un commit
   pre-animatic che funzionava. Candidati iniziali:
   - commit `ac5e46ca8` "Add storyboard/ztory sources" (potrebbe essere OK)
   - commit `35577720e` "ZtoryAnimaticController + dedicated TFrameHandle"
     (tocca TFrameHandle, area a rischio)
4. **Opus per analisi** — dato che il codice di rendering è denso, usare
   Claude Opus per leggere il diff upstream vs Ztoryc e identificare
   subito l'area rotta.

---
## [2026-04-15] — Indagine render preview bianco (bug ancora aperto)

### Modified
- `toonz/sources/toonz/previewer.cpp`:
  - `Previewer::Imp::buildSceneFx()`: cambiato `scene->getXsheet()` →
    `scene->getTopXsheet()` — il Previewer ora renderizza sempre dalla root
    xsheet anziché dalla sub-scene aperta. Fix corretto ma non sufficiente.
  - `Previewer::Imp::updateCamera()`: cambiato `scene->getCurrentCamera()`
    (che usava `getXsheet()`, tornando la camera della sub-scene aperta) →
    `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()`.
    Camera del Previewer ora sempre allineata alla root xsheet.
  - Aggiunti include: `toonz/fxdag.h`, `toonz/tcolumnfxset.h`,
    `toonz/tstageobjecttree.h` (necessari per i fix).

### Notes — Bug render preview ancora aperto
Il render preview mostra bianco sia nel viewer animatico che in quello nativo.

**Investigazione effettuata:**
- Debug confermato: il FX tree è valido end-to-end:
  - Root xsheet: `cols=6, frameCount=214, termFxs=4` → `fxA=non-null`
  - Sub-scene: `subCols=1, subTermFxs=1, outputConnected=1` → `buildFx=non-null`
- Il render completa e `ras=VALID` (raster non-null restituito al viewer).
- `buildSceneFx()` in `scenefx.cpp` fa sempre `makeOver(bgCard, fx)` — quindi
  `fxA=non-null` non garantisce contenuto visivo (potrebbe essere solo bgCard).
- GL error 1286 (`GL_INVALID_FRAMEBUFFER_OPERATION`) pre-esistente, non causa
  del bianco (LUT non attiva, `lutValid=0`).

**Ipotesi ancora da verificare:**
1. La `drawPreview()` in `sceneviewer.cpp` usa ancora
   `scene->getCurrentCamera()` per calcolare `rasterToStageRef` — se la camera
   della sub-scene ha dimensioni diverse dalla root, l'immagine potrebbe essere
   mappata fuori dal viewport.
2. Il raster renderizzato potrebbe contenere effettivamente solo il colore
   sfondo (bianco) perché le sub-scene, pur avendo `termFxs=1`, non producono
   pixel visibili per qualche ragione ancora ignota (palette? DPI? blend mode?).
3. Il Previewer singleton potrebbe condividere cache tra viewer diversi in modo
   conflittuale.

**Prossima sessione — cosa fare:**
- Fixare `drawPreview()` in `sceneviewer.cpp` per usare la root xsheet camera
  nel calcolo di `rasterToStageRef`.
- Aggiungere debug mirato al valore dei pixel del raster renderizzato (es.
  `ras->pixels(0)[0]`) per capire se il contenuto è bianco o trasparente.
- Considerare di usare Opus per analisi più profonda.

---
## [2026-04-09] — Camera mismatch parziale fix + design room unificata

### Fixed
- **`getViewMatrix()` rimossa logica errata `getTopXsheet()`** (`sceneviewer.cpp`): il branch `if (m_alwaysMainXsheet)` in `getViewMatrix()` usava `getTopXsheet()` (camera root = identity) rendendo il viewer animatic cieco alle camera delle sottoscene. Rimosso: ora usa sempre `getCurrentXsheet()` + `TApp::getCurrentFrame()` (comportamento originale Tahoma2D). Il `m_customFrameHandle` resta solo per `drawScene()` dove serve il frame animatic per renderizzare la root xsheet al frame corretto.

### Notes
- **Bug aperto — Camera mismatch inside shot**: il mismatch tra viewer animatic e ComboViewer quando si è dentro uno shot persiste. La causa root è che Stage NON applica la camera della sottoscena quando renderizza dalla root xsheet (la camera sub-scene è applicata solo quando si è *dentro* la sottoscena). Il viewer animatic renderizza sempre la root xsheet, quindi non può applicare la camera delle singole sottoscene via `getViewMatrix()`. Richiede investigazione approfondita su Stage::visit() o una soluzione alternativa (e.g. quando si è dentro uno shot, il viewer animatic usa getCurrentXsheet() come shot viewer).
- **Design room unificata discussa**: proposta utente di room SHOT+ANIMATIC con toggle (QStackedWidget Left: Board↔XSheet, Center: AnimaticViewer↔ComboViewer, Right: Script+Inspector↔Palette+SmallViewer). Fasi: sprint 1 = toggle Left+Center + highlight giallo shot corrente.

---
## [2026-04-08] — Animatic viewer: marker indipendenti, camera view, real-time update

### Fixed
- **TSoundTrackP dangling pointer** (`viewerpane.h`): `m_sound` era `TSoundTrack*` raw — diventava dangling quando `m_mixedSound` veniva liberato da `invalidateSound()`. Fix: cambiato a `TSoundTrackP` (smart pointer). Null check aggiornati da `!= NULL` a `if (m_sound)`.
- **AutoFill combo non si popolava** (`tooloptions.cpp`): `m_controls` è indicizzato per `getName()` = `"Fill Style:"`, non per `getId()` = `"AutoFillStyle"`. Fix: corretti 3 punti in `tooloptions.cpp` (lookup, filter set, notifyToolComboBoxListChanged).
- **Mute/solo interferisce con ComboViewer nativo** (`viewerpane.cpp`, `ztoryanimatic.cpp`): quando sia il ComboViewer nativo che l'animatic viewer erano aperti, i rispettivi `play()` competevano per lo stesso `TSoundOutputDevice`. Fix quickfix: `ownsAudioAtMainLevel()` in `ZtoryAnimaticController` — il viewer nativo cede il controllo audio quando siamo a main level e l'animatic è aperto (gated su `isStoryboardWorkflow()`).
- **Marker animatic si spostavano entrando in uno shot** (`ztoryanimatic.cpp`, `ztoryanimatic.h`): `openSubXsheet()` sovrascriveva `XsheetGUI::setPlayRange()` con il range della sottoscena — storage singolo globale. Fix: range indipendente `m_animaticR0/m_animaticR1` in `ZtoryAnimaticController`; `updateFrameMarkers()` virtuale overridato in `ZtoryAnimaticViewer` per leggere sempre dallo storage proprio.
- **Camera animatic viewer non si aggiornava in real-time** (`ztoryanimatic.cpp`): aggiunto `objectChanged → m_sceneViewer->update()` in `showEvent()` — `objectChanged` si emette durante il drag interattivo di camera/peg, che era già connesso in `SceneViewer::showEvent()` ma non sopravviveva ai cicli disconnect delle sottoscene.

### Added
- **CAMERA_REFERENCE come default** (`ztoryanimatic.cpp`): l'animatic viewer si avvia in camera view — mostra l'inquadratura della sottoscena corrente senza che l'utente debba cambiare modalità manualmente.
- **`getViewMatrix()` usa root xsheet per animatic** (`sceneviewer.cpp`): quando `m_alwaysMainXsheet` è true, `getViewMatrix()` usa `getTopXsheet()` (camera del main) invece di `getCurrentXsheet()` (camera della sottoscena), evitando doppia applicazione della camera. Usa `m_customFrameHandle` per il frame animatic invece di `getCurrentFrame()` (che punterebbe alla frame della sottoscena).

### Notes
- **Bug aperto**: real-time update della camera mentre si edita all'interno di uno shot ancora da verificare dopo l'aggiunta di `objectChanged`. Il build `4f48e4da5` include il fix.
- **Design session in sospeso**: toggle Animatic↔ComboViewer rooms (architettura rooms definitiva).

---
## [2026-04-08] — Fix crash mute scene vecchie + antialias autofill + palette picker

### Fixed
- **Crash SIGABRT mute su scene vecchie** (`txshsoundcolumn.cpp`): `mixingTogether()` aveva `assert(soundLevel)` attivo in RelWithDebInfo — se l'audio file di una scena vecchia ha un riferimento rotto, `l->getSoundLevel()` ritorna null → assert → crash. Fix: sostituito con `if (!soundLevel) return mix`. Stesso fix in `getOverallSoundTrack()`: `overallSoundTrack->blank()` crashava se `TSoundTrack::create()` aveva lanciato un'eccezione (overallSoundTrack null). Fix: guard `if (!overallSoundTrack) return`. Aggiunto anche null check per `soundLevel` nel loop degli sound levels. Upstream candidate fix per Tahoma2D.
- **Bordino bianco tra linea e fill (antialias autofill)**: il BFS usava `getInk() != 0` come barriera (corretto), ma la fill condition richiedeva `getInk() == 0` — i pixel antialiased interni (ink>0, tone>0) venivano esclusi → gap bianco. Fix: rimossa condizione `getInk() == 0` → fill su tutti i pixel interni con `getPaint() == 0`. Per pixel puramente inchiostrati (tone=0) il paint viene settato ma il canale ink domina visivamente — nessun impatto.

### Added
- **AutoFill palette picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.h/.cpp`): il combo "Fill Style" ora si popola dinamicamente con tutti gli stili della palette corrente (oltre a "Next Style (N+1)" e "Current Style"). Rebuild automatico quando cambia la palette o il livello. Ogni stile appare come `[N] NomeStile`. Selezione persistente tra un refresh e l'altro.

---
## [2026-04-08] — Fix crash audio mute + mute immediato durante play + AutoFill picker

### Fixed
- **Crash heap corruption su mute (scena con audio lungo)**: `makeSound()` con `fromFrame=-1, toFrame=-1` → `mixingTogether()` usava `getFrameCount()` inflato (durata file raw, potenzialmente ore) → buffer centinaia di MB → corruzione heap. Fix in `viewerpane.cpp`: bounded `prop->m_toFrame` al frame count delle sole colonne video (`maxFrame` da `col->getRange()`).
- **Crash heap corruption durante `refreshAudioTracks()`**: `restoreTrackStates()` chiamava `applyMuteSolo()` → `invalidateSound()` + restart audio device mentre ancora in play → corruzione. Fix: `restoreTrackStates()` ripristina solo stato UI (checked/unchecked), non tocca il device audio.
- **Null dereference in viewerpane.cpp**: `m_sound->getSampleRate()` chiamato prima del null check → spostato `if (!m_sound) return` prima del dereference.
- **Mute non ha effetto immediato durante play**: `applyMuteSolo()` chiamava `stopScrub()`/`play()` dal click handler, in race con i callback CoreAudio XPC → EXC_BAD_ACCESS. Fix: flag `m_pendingAudioRestart` settato da `applyMuteSolo()`, consumato in `onDrawFrame()` che viene chiamato dal Qt timer tra i callback XPC — contesto sicuro per `stopScrub()`/`play()`. Il mute ora è effettivo entro il prossimo frame (~40ms).
- **Mute/solo non persistente dopo `refreshAudioTracks()`**: stato salvato in `m_colMuted`/`m_colSolo`, ripristinato in `restoreTrackStates()`.

### Added
- **AutoFill fill style picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.cpp`): nuovo `TEnumProperty m_autoFillStyle` con valori "Next Style (N+1)" (default, comportamento precedente) e "Current Style" (riempie con lo stile attualmente selezionato in palette). Il combo appare nella toolbar del brush tool accanto al checkbox AutoFill. Aggiunto anche `invalidate()` dopo autofill per aggiornare il canvas subito dopo mouseUp senza aspettare il prossimo hover.

### Notes
- Pattern sicuro per restart audio durante play: flag `m_pendingAudioRestart` → `restartAudioIfPlaying()` da `onDrawFrame()`.
- `stopScrub()`/`play()` sono sicuri solo se chiamati tra i callback CoreAudio XPC (Qt timer) — non dai click handler UI.

---
## [2026-04-06] — Board desync fix (merge/cut/delete), edit shot fix, durate panel, match button

### Fixed

- **3-shot merge lascia uno shot in più nel Board** (`storyboardpanel.cpp`, `onShotRemovedAt`):
  quando il secondo `shotRemovedAt` non trova lo shot per `data.xsheetColumn` (tracking
  desynced da operazioni precedenti), ora cade back su `refreshFromScene()` invece di
  tornare silenziosamente.

- **Edit shot button non selezionava lo shot** (`storyboardpanel.cpp`, `onEditShot`):
  aggiunto `selectShot(shotIdx)` prima di aprire la sottoscena.

- **Edit shot button usava board index come colonna xsheet** (`storyboardpanel.cpp`, `onEditShot`):
  ora usa `m_shots[shotIdx].data.xsheetColumn` — fix critico dopo merge/cut che desincronizzano
  indice Board dall'indice xsheet.

- **T: (durata totale) aggiornava panels[0].duration invece del display** (`onXsheetChanged`):
  per shot multi-panel questo sovrascriveva la durata parziale del panel 0 con la durata
  totale. Ora `onXsheetChanged` aggiorna solo il display T: per tutti i panel; D: (parziale)
  viene aggiornata solo per shot a panel singolo (dove D: == T:).

- **D: (durata parziale) includeva frame nascosti** (`detectAndUpdatePanels`):
  l'ultimo panel usava `numFrames` (frame count completo della sottoscena, inclusi frame
  oltre la durata visibile in timeline). Ora legge la durata visibile dalla colonna del
  main xsheet ancestor e cappa l'ultimo panel al limite timeline.

- **Panel oltre l'area visibile in timeline venivano mostrati nel Board**
  (`detectAndUpdatePanels`): aggiunto filtro — i panel con `startFrame >= timelineDuration`
  vengono esclusi dal Board.

### Added

- **Bottone ⇔ (Match Duration)** (`storyboardpanel.h/.cpp`, `PanelWidget`):
  ogni shot nel Board ha un piccolo bottone ⇔ accanto al campo T:. Quando cliccato,
  legge il `getFrameCount()` reale della sottoscena e ridimensiona la colonna nel main
  xsheet di conseguenza, poi chiama `resequenceXsheet()`. Consente di allineare la durata
  timeline alla durata effettiva della sottoscena.

### Notes

- `detectAndUpdatePanels` è chiamato dal `m_panelDetectTimer` (1000ms debounce) mentre
  si è dentro una sottoscena. Ora richiede un AncestorNode valido per calcolare
  `timelineDuration`; se l'ancestor non è disponibile, usa `numFrames` come fallback.
- Il bottone ⇔ è visibile in tutti i panel dello shot ma opera sempre sulla colonna
  dell'intero shot nel main xsheet.

---
## [2026-04-05] — Icone toolbar QToolButton, SVG Ztoryc, camera init sottoscene

### Modified

- **QPushButton → QToolButton in toolbar** (`storyboardpanel.h/.cpp`, `ztoryanimatic.cpp`):
  tutti i bottoni toolbar convertiti da QPushButton con testo a QToolButton con icone SVG
  via `createQIcon()`. Stile uniforme: `setFixedSize(28,28)`, `setIconSize(20,20)`,
  background trasparente, hover `#555`, checked `#666`.
  Connect aggiornati da `&QPushButton::clicked` a `&QToolButton::clicked`.

### Added

- **21 icone SVG Ztoryc** (`toonz/sources/toonz/icons/dark/ztoryc/`, `toonz.qrc`):
  `ztoryc_add_shot`, `ztoryc_delete_shot`, `ztoryc_merge`, `ztoryc_edit_shot`,
  `ztoryc_numbering`, `ztoryc_export_pdf`, `ztoryc_export_animatic`, `ztoryc_export_shots`,
  `ztoryc_select`, `ztoryc_razor`, `ztoryc_av_link`, `ztoryc_av_link_on`, `ztoryc_onion`,
  `ztoryc_onion_on`, `ztoryc_lock`, `ztoryc_lock_on`, `ztoryc_copy`, `ztoryc_clone`,
  `ztoryc_paste`, `ztoryc_shotedit`, `ztoryc_shotedit_on`, `ztoryc_refresh_preview`.
  Embedded nel binario via qrc. Toggle on/off gestiti automaticamente da `createQIcon`.

- **Camera init sottoscene** (`storyboardpanel.cpp`, `onAddShot()`): copia res e size
  dalla camera del main xsheet alla nuova sottoscena, stesso comportamento di
  `subscenecommand.cpp`. Risolve la piccola differenza di inquadratura tra sottoscena
  e main su scene create con Ztoryc.

### Removed

- `m_refreshButton` — rimosso da header, cpp e layout Board (refresh automatico
  con debounce già attivo).
- `m_backButton` — rimosso da header, cpp e layout Board (doppio click per tornare
  al Board già implementato).

### Notes

- Per aggiornare un'icona: sostituire il file SVG in `icons/dark/ztoryc/` e ricompilare.
  Se l'icona non cambia dopo la modifica al qrc: `ninja -C toonz/build -t clean` poi rebuild.
- Il bottone merge nel Board (`m_mergeButton`) è presente ma disabilitato
  (`setEnabled(false)`) — implementazione pendente come task aperto.
- Edit In Place deve essere **spento** quando si lavora sulla camera dentro uno shot.
  Con Edit In Place spento la camera locale funziona correttamente.
  L'audio del main si sente anche con Edit In Place spento — comportamento corretto.

---
## [2026-04-03] — Audio track L/M/S buttons, mute/solo fix, crash fix, cursor jump fix

### Fixed
- **Crash on mute (memory corruption of free block)**: `m_sound` (raw ptr in base
  `viewerpane.h`) was dangling after controller released `m_soundTrack` ref. Fixed by
  giving `ZtoryAnimaticViewer` its own `TSoundTrackP m_soundTrackRef` to keep the
  object alive until `refreshAnimaticSound()` replaces it. Removed the fragile
  `soundTrackInvalidating` signal approach.
- **Mute/Solo not updating during playback**: Mute handler was calling `setVolume()`
  directly without going through `applyMuteSolo()`, so solo state was ignored and
  `restartAudioIfPlaying()` was never called. Now both M and S delegate entirely to
  `applyMuteSolo()` via signals. `applyMuteSolo()` invalidates both TXsheet internal
  cache (`xsh->invalidateSound()`) and controller cache, then calls
  `restartAudioIfPlaying()` synchronously.
- **Solo logic**: Fixed `effectiveMute = muted || (hasSolo && !solo)` — M wins over S.
  Previously used `hasSolo ? !solo : muted` which gave wrong result when M+S both active.
- **`applyMuteSolo()` corrupting m_muted state**: Was calling `at->setMuted()` to
  apply solo overrides, which destroyed the user's own mute flag. Now uses separate
  `m_effectiveMuted` bool (set by `setEffectiveMuted()`) for visual dim only.
- **Cursor jumps right after audio cut/move**: `segmentMoved` lambda was calling
  `xsh->updateFrameCount()` which included long audio columns (trailing ColumnLevel
  with `endOffset=0` after razor cut = raw file length). Removed the call; animatic
  length is driven by video shots, not audio.
- **Selection not clearing on razor cut**: `m_selSeg` was never reset when razor was
  active (selection logic gated on `!m_razorActive`). Now cleared when razor fires.

### Added
- **L/M/S painted buttons** on audio track headers (horizontal row, 22×16px each).
  Pure paint approach — no QToolButton children (they don't render in custom-painted
  QWidgets on macOS).
- **Lock painted button** on video track header.
- **Waveform dim overlay** when track is muted (M) or solo-silenced — semi-transparent
  black rect over waveform area.
- **`m_effectiveMuted` flag** on `ZtoryAudioTrack`: tracks solo-silenced state
  separately from user's `m_muted`, so applyMuteSolo never corrupts user state.
- **`restartAudioIfPlaying()`** on `ZtoryAnimaticViewer`: rebuilds merged track and
  calls `mainXsh->play()` in-place (no stopScrub) so QAudioOutput hot-swaps data.
- **`ZtoryAnimaticController::setViewer/viewer()`**: lets the panel call
  `restartAudioIfPlaying()` on the viewer without a direct reference.

### Notes
- Audio update during play has ~100ms latency (QAudioOutput hardware buffer drain
  time) — same as DaVinci Resolve. Acceptable.
- M + S both active on same track: M wins (track is muted). Both S active: both play.

---
## [2026-04-01] — NLE audio track: zoom, edge trim, overlap, add track, cross-track

### Fixed
- **Razor audio split**: `splitAudioColumn` ripristinata a `splitLevelAtFrame` (nessun frame perso). `findSegments()` ora itera `ColumnLevel` direttamente (non celle xsheet) → segmenti razor indipendentemente selezionabili e trascinabili
- **Zoom/scroll audio lungo**: `updateTrackWidths()` calcola la larghezza totale includendo sia i blocchi video che i range audio — i file audio lunghi non vengono più tagliati
- **Cut lines fantasma**: cut lines ora mostrate solo dove c'è audio nel punto di taglio; aggiornate dopo ogni `segmentMoved` e `shotDurationChanged`
- **Cursore hover edge**: `SizeHorCursor` su hover pixel-based ai bordi segmento (non solo al click)

### Added
- **Edge trim segmenti audio**: drag bordo sinistro/destro per accorciare o allungare il segmento; commit via `modifyCellRange` (nessun frame audio perso all'interno del ColumnLevel)
- **Overlap prevention**: durante `SegmentDrag` il movimento è clampato contro i segmenti adiacenti per evitare sovrapposizioni nella stessa traccia
- **Add Audio Track**: context menu panel → inserisce nuova colonna sound vuota nell'xsheet
- **Cross-track segment move**: drag segmento fuori dalla traccia → drop su altra traccia; posizionamento preciso con `dragOffset`; clamp anti-overlap sulla traccia destinazione

### Modified
- `TXshSoundColumn`: `getColumnLevel`/`getColumnLevelCount` spostati da `protected` a `public`; aggiunti `detachLevelByFrame` e `adoptLevel` come API pubbliche
- `refreshAudioTracks`: rimosso check `sc->isEmpty()` per mostrare tracce audio vuote (necessario per Add Audio Track)

### Notes
- Cross-track drop: se la traccia destinazione ha segmenti sovrapposti, il clamp li evita ma può posizionare il segmento in modo non intuitivo — da migliorare in sessione futura con feedback visivo durante il drag

---
## [2026-04-01] — Fix SIGSEGV salvataggio TLV (libimage ABI mismatch)

### Fixed
- **Crash SIGSEGV salvataggio TLV** (`build_and_deploy.sh`): `libimage.dylib` nel bundle era un residuo di una build Debug precedente. `TLevelWriterTzl` (in `libimage`) leggeva `m_creator` a `this+0x48`, ma il nuovo `libtnzcore` RelWithDebInfo lo scrive a `this+0x50` (8 byte di differenza per layout di `TSmartObject`). Fix: aggiunto deploy di `libimage` con `install_name_tool` che patcha il rpath `libtiff` da `/usr/local/lib/libtiff.5.dylib` → `@executable_path/libtiff.5.dylib` (il path `/usr/local/lib` non esiste su questo Mac).

### Notes
- Root cause: `libimage` e `libtnzcore` devono essere sempre della stessa build. Qualsiasi cambio di build type (Debug/RelWithDebInfo/Release) richiede di ri-deployare `libimage`.
- `libpng` e `libjpeg` linkati via `/opt/homebrew` — risolvono correttamente a runtime.
- `libcolorfx` e `libtnzstdfx` NON deployate: dipendono da `libimage` ma non cambiano → usano quella nel bundle già aggiornata.

---

## [2026-06-05] — PSD fix, crash fix, camera overlay Phase 1

### Fixed
- **PSD first layer "not found" as sub-scene** — root cause: `getLevelPathAndSetNameWithPsdLevelName` replaced `##` → `#` unconditionally, turning `file##group.psd` (empty name + group mode, common in Affinity Designer 16-bit PSDs) into `file#group.psd` where "group" was misread as a layer name. Fix: two-part — (1) skip replace for mode keywords; (2) fallback in TLevelReaderPsd reader. Commit `5b8eeb3c1`. PR candidate upstream.
- **Crash on quit / workflow switch (OpenGL static destructor)** — `signalHandler` tried to open QDialog during Qt static destructor teardown → abort. Fix: `s_appExiting` flag set on `aboutToQuit`. Commit `428f6c0d9`.
- **Audio +1 frame in exported shots** — `getRange()` without `ignoreLastStop=true` included stop-hold frame. Commit `976db07c4`.
- **PDF fps hardcoded to 24** — now reads from scene output properties. Camera single keyframe no longer creates panel boundary. `sub-scene` → `shot` in tooltips.

### Added
- **Camera move overlay on Board thumbnails (Task 40 Phase 1)**: PanelData stores camera affines at panel start/end; `computeCameraMove()` classifies Pan/Tilt/TrkIn/TrkOut; `applyCameraOverlay()` draws red IN/OUT rectangles + labels on thumbnails. Applied in Board view and PDF export. Persisted in `.ztoryc`. Commit `d19a84551`. Phase 2: backed-out wide render for Pan/Tilt.

### Notes
- PSD bug is PR candidate for Tahoma2D upstream (added to AGENTS.md).
- Camera overlay still needs tuning (Phase 2: Pan wide render, editable label).

---
