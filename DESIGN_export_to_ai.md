# Export-to-AI — pipeline di produzione

Impostato con Franco il **2026-08-29**. E' la Fase E di
`DESIGN_production_tracker.md`, che qui prende una forma eseguibile: chi fa
cosa, **su quale macchina**, e cosa e' gia' verificato contro cosa e' ancora
un'ipotesi.

> **Come leggere questo documento.** Ogni affermazione tecnica porta la fonte:
> ✅ misurata sull'installazione vera, 🔵 proposta da validare. La distinzione
> non e' pedanteria — una pipeline che mescola le due cose fa preventivare
> tempo su cose che non esistono.

-----

## 1. Le macchine — sono DUE, non tre

| | cos'e' | ruolo |
|---|---|---|
| **Mac** | Apple M4, **16 GB** unificati ✅ | authoring e regia, prove singole |
| **RunPod** | GPU a noleggio, a minuti | **tutto il calcolo pesante**, LoRA compresa |

Il **Mac** non e' una macchina da render — 16 GB unificati sono meno del solo
`flux_schnell` (17 GB ✅). Ci si fanno Ztoryc, Puppetoonz, l'animatic, Kitsu, e
si pilota Anymatix. Una prova alla volta, per decidere.

**RunPod e' dove si ESEGUE una ricetta gia' decisa.** Arrivarci senza sapere
prompt, modello e parametri significa pagare per pensare.

### ❌ Il Dell e' fuori — muro hardware, verificato il 2026-08-29

Era previsto come «il mulino gratuito»: passate ControlNet notturne e
addestramento LoRA, lente ma senza contatore. **Non e' possibile.**

Il bootstrap remoto di Anymatix e' filato liscio (11 GB, CUDA verificata sulla
RTX 2060 ✅), ma ComfyUI muore a ogni avvio:

```
comfyui.service: Main process exited, code=dumped, status=4/ILL
flags del processore: sse4_1 sse4_2      AVX: no      AVX2: no
```

**`SIGILL`.** Lo Xeon X5670 e' del 2010 e non ha **AVX**, arrivato con Sandy
Bridge nel 2011. Le ruote Python attuali — numpy, scipy, torch — lo danno per
scontato: il processo arriva agli import di scipy e muore. **La GPU non viene
mai nemmeno sfiorata.** Non c'e' configurazione che aggiri l'assenza di
un'istruzione nel processore.

> Spiega anche perche' ComfyUI ci funzionava a gennaio 2026 e poi non e' piu'
> stato usato: allora le ruote installate erano ancora compatibili.
>
> Il Dell resta la macchina delle **build Windows e Linux di Ztoryc**, che e' il
> mestiere per cui lo si tiene. Ha 672 GB liberi e un kit LTX-2 da 88 GB
> scaricato a gennaio, inutilizzabile li' ma intatto.

-----


## 2. I sette passi

```
1. script                                          →  fuori
2. breakdown            Kitsu                      →  Mac
3. production design    modelli, mano umana + AI   →  Mac (prova) / RunPod (serie)
4. LoRA di stile        TrainLoraNode, in-app      →  RunPod
5. storyboard+animatic  Ztoryc, a mano             →  Mac
6. export-to-AI
   6a. control          ControlNet per fotogramma  →  RunPod
   6b. video            LTX / Wan                  →  RunPod
7. montaggio            FCPXML → DaVinci           →  Mac
```

### 2 — Breakdown su Kitsu ✅ funziona oggi
MCP Kitsu attivo, istanza locale. **filorosso** e' il progetto vero;
**ZTORYC-AI** e' la scena sacrificabile per i test distruttivi (Franco,
2026-08-29). Status e task type si leggono dal server, mai scritti a mano.

### 3 — Production design ✅ fattibile
Immagini fisse: flux o SDXL bastano. In locale una alla volta, per decidere;
le serie su RunPod. **E per le prove di stile Gemini via Kling batte tutto**
(33 secondi, 20 crediti, risultato usabile al primo colpo — misurato il
2026-08-29): serve a decidere, non a produrre.

### 4 — LoRA di stile ✅ possibile IN-APP
`TrainLoraNode`, `SaveLoRA`, `LossGraphNode`, `LoraModelLoader` sono in
`ComfyUI/comfy_extras/nodes_train.py` ✅. **Non serve kohya fuori dall'app.**

⚠️ Gira su **RunPod**, non piu' sul Dell (§ 1). Cambia il conto: l'addestramento
non e' piu' gratuito, si paga a minuti di GPU.

🔵 Da validare: che l'addestramento SD 1.5 stia dentro 6 GB con i parametri che
useremo, e quante immagini servano davvero.

> **Perche' questo passo conta piu' di come suona.** Senza LoRA di stile ogni
> shot e' una trattativa col prompt e l'aria del corto cambia da inquadratura a
> inquadratura. Con la LoRA lo stile smette di essere un'opinione. E' il passo
> che decide se filorosso avra' un'aria **sua**.

### 5 — Storyboard e animatic ✅ e' Ztoryc
A mano. **E' qui che si dirige il film**: l'animatic non e' un riferimento
approssimativo da dare in pasto all'AI, e' la verita' a cui l'AI deve obbedire.

### 6a — Control ✅ i pezzi ci sono
`comfyui_controlnet_aux` (depth, pose, lineart, canny) e
`ComfyUI-Advanced-ControlNet` (coerenza temporale) sono installati ✅. Nella
libreria ci sono gia' `SD 1.5 ControlNet` e `SDXL ControlNet` ✅.

> ⚠️ **WanMove NON e' questa cosa.** Sembra fatto apposta e non lo e':
> `GenerateTracks` ✅ prende coordinate — inizio, fine, Bezier, interpolazione —
> e **sintetizza** un percorso. E' «descrivimi un movimento», non «guarda questo
> video e rifallo». Serve per i movimenti di macchina. Verificato leggendo lo
> schema del nodo, non dedotto dal nome.
>
> **WanDancer** ✅ invece e' reale ed e' un'altra cosa ancora: analizza l'audio
> (tempo, chroma, pitch, mel) per far muovere l'immagine **a tempo di musica**.
> Non serve a filorosso — serve a **«un anno di te»**, dove vale molto.

### 6b — Video: due strade, e vanno scelte

**Via API** ✅ — Anymatix spedisce i nodi API di ComfyUI: **Seedance** di
ByteDance (`ByteDance2ReferenceNode`, «Seedance 2.5 Reference to Video —
genera, modifica o estende un video usando immagini di riferimento, video e
audio»), piu' **Veo, Sora, Kling, Minimax, Hailuo**. Girano sui server di chi
li fa, si pagano a **crediti comfy.org** (`auth_token_comfy_org`), e **non
serve nessuna GPU**.

**Via aperta** ✅ — **Wan 2.2**, **LTX-2**, Hunyuan, Cosmos, Mochi, SVD,
AnimateDiff. Girano su ferro nostro, cioe' RunPod.

> **Il bivio non e' di potenza, e' di controllo.** A Seedance **non puoi dare la
> tua LoRA**: gli mandi riferimenti e prompt, e lui interpreta. Tutta la
> costruzione del passo 4 — set di venti immagini, LoRA di stile, quel segno
> tenuto identico per tutto il film — **funziona solo sui modelli aperti**.
>
> Per un corto in stile «disegno a matita», dove il punto e' che sia **quel**
> segno, la via aperta e' quella coerente. Ma 🔵 Seedance con i riferimenti
> potrebbe tenere lo stile meglio del previsto, e si scopre provando.
>
> Uso ibrido sensato: via aperta per gli shot con i personaggi, dove identita' e
> segno contano; Seedance per gli inserti — un paesaggio, un dettaglio, una
> transizione — dove serve solo che sia bello.


### 7 — Montaggio ✅ funziona oggi
Ztoryc esporta gia' FCPXML con le dissolvenze ✅ (v0.8.0).

-----

## 3. Il lipsync — due strade, e una da provare

In Anymatix **non c'e' nessun nodo di lipsync** ✅. Chatterbox e' sintesi vocale
(fa la voce, non la bocca); LTX-2 ha i nodi audio ma e' generazione, non
allineamento.

### Strada A — il labiale e' nostro (l'impostazione di partenza)

Il lipsync **ce l'abbiamo gia'**: Puppetoonz produce le bocche, il `.zmouth`
dice quale disegno e' quale viseme, Vosk allinea a 10 ms.

Quindi non si chiede all'AI di **inventare** il labiale — la cosa che le riesce
peggio e che non si puo' correggere — ma di **preservarlo**: la bocca giusta e'
gia' nell'animatic, fotogramma per fotogramma, ed entra nel control.

> **E' anche l'unica versione in cui il regista resta Franco.** Un lipsync
> generato non lo ritocchi; uno tuo si'. Lo stesso vale per il movimento:
> l'animatic e' la verita', l'AI e' la rifinitura.

### 🔵 Strada B — LongCat-Video-Avatar 1.5 (DA PROVARE)

Segnalato da Franco il 2026-08-29. `github.com/meituan-longcat/LongCat-Video`.

**Confermato leggendo repo e scheda del modello:**

- licenza **MIT** — libera davvero, nessuna clausola
- lip sync guidato dall'audio, encoder **Whisper-large-v3**
- *«Robustly generalizes to **anime**, animals, and complex real-world
  conditions such as multi-person interactions»* — **lo stilizzato e'
  dichiarato, non un effetto collaterale**
- **immagini di riferimento** supportate (`--ref_img_index`)
- piu' personaggi, audio singolo o doppio; uscita **480p e 720p**
- quantizzazione **INT8** e distillazione a **8 passi** disponibili
- il modello base LongCat-Video e' **13,6B** ed e' addestrato sulla
  continuazione video, da cui i «video di minuti»

**NON dichiarato da nessuna parte:** la **VRAM**, il numero di parametri
dell'Avatar, la velocita', la durata per generazione. Sono i numeri che
decidono il costo, e non ci sono.

**Non fa:** nessun ControlNet — prende un'immagine e un audio e **inventa** il
movimento, non lo copia da un video. Nessuna LoRA: lo stile viene
dall'immagine di riferimento.

⚠️ **Attenzione a una confusione che gira:** i «video lunghi minuti» sono del
modello **base**; l'**Avatar** nello Space fa **~5 secondi**. Sono due modelli
diversi.

**La tensione con la strada A e' reale:** un labiale generato **non si
ritocca**, e questo anima tutta la faccia, non solo la bocca. Su un corto
disegnato a mano puo' essere un dono o un'invasione — non si decide a tavolino.

**Come si prova, gratis e in dieci minuti:** lo Space
`huggingface.co/spaces/victor/LongCat-Video-Avatar-1.5` accetta immagine +
audio dal browser. Immagine da usare: `FR_STYLE_gumball_v01.png`, o meglio una
nello stile «disegno a matita» se e' quello scelto. Con dieci secondi di voce si
sanno due cose che nessuna scheda tecnica dira': **se il labiale regge su un
disegno piatto**, e **se il movimento inventato rispetta il personaggio o lo
tradisce**.

Se convince: misurare la VRAM su RunPod e il costo di un minuto di parlato.
Se non convince: resta la strada A, e non si e' perso niente.


## 4. Il pacchetto — da § 6 del production tracker

Unita' di export = **Panel**. Struttura 🔵, da rifinire **sull'uso**:

```
export_SH020/
├── manifest.json
├── panels/p001/sketch.png
└── modelpack/Hero/ref01.png
```

Il `manifest.json` porta per ogni panel: `sketch`, `durationFrames`, `prompt`
(dai campi **action** e **dialogue** di Ztoryc), `rawText`, e gli `assets` con
il rimando al modelpack.

**Chi decide se il formato e' giusto e' filorosso**, non Ztoryc (Franco,
2026-08-29). Il primo consumatore reale detta i requisiti: cio' che manca torna
indietro come richiesta sull'app, **non si adatta il corto al formato**.

> **Da NON confondere con una collection BYOM.** Una collection contiene
> `anymatix/*.json` — i **workflow**, cioe' la ricetta — e basta: *"Only
> `anymatix/*.json` is imported"* ✅. Il pacchetto di export e' **dato di uno
> shot**: prodotto, consumato, mai pubblicato. Metterlo in una collection
> vorrebbe dire pubblicare su GitHub gli sketch di ogni scena.
>
> BYOM serve invece a una domanda che il design non si era posto: **come arriva
> la ricetta all'utente**. Ed e' un fork del loro template, zero codice.

-----

## 5. Il prossimo passo, ed e' piccolo

**Uno shot di filorosso: sketch → ControlNet, sul Mac.** Non per fare qualcosa
di bello — per rispondere alla sola domanda che conta adesso:

> *dal mio disegno esce la mia inquadratura?*

Si giudica in dieci secondi. Se si', il resto e' ingegneria e si sa dove va. Se
no, si e' risparmiato l'affitto di una GPU.

Solo **dopo** ha senso: il dumper del pacchetto in Ztoryc (Fase E), la LoRA di
stile su RunPod, e il video su RunPod.

-----

## 6. Aperti

- 🔵 **Provare LongCat-Video-Avatar nello Space** (§ 3, strada B) — gratis,
  dieci minuti, e risponde alla domanda piu' grossa che abbiamo sul labiale
- 🔵 LoRA di stile su RunPod: parametri, numero di immagini, costo per giro
  (era «sul Dell, di notte, gratis» — quella strada e' chiusa, vedi § 1)
- 🔵 Formato del manifest: emerge dai test, iterativo
- 🔵 Come arriva il pacchetto ad Anymatix. Un agente via MCP sa aprire un
  workflow, impostare ogni ingresso e attaccare un'immagine (anche con URL
  `file://` ✅) — ma **non sa accendere la macchina di calcolo**: verificato tre
  volte il 2026-08-29, `compute.route.choose` registra la scelta e basta.
  Quindi il collante non puo' essere *solo* un agente: il motore dev'essere
  gia' acceso da una persona, o avviato per altra via.
- 🔵 Coerenza dei personaggi: il modelpack come reference, oppure una LoRA per
  personaggio oltre a quella di stile
