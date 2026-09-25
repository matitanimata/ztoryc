# Sviluppo con AI in Ztoryc

_Bozza del 2026-09-25 — da rivedere con Franco._

Ztoryc e' sviluppato con l'aiuto di strumenti AI (in particolare Claude Code).
Questo documento descrive come, e quali garanzie applichiamo. Sostituisce, per Ztoryc,
l'`AI_POLICY.md` di Tahoma2D.

## Principio

L'AI e' un partner di sviluppo. **La responsabilita' del codice resta umana**: ogni
modifica che entra nel repository e' stata discussa, revisionata e accettata da chi
mantiene il progetto.

## Processo

1. **Design prima del codice.** Le funzionalita' nascono da specifiche scritte
   (architettura, flussi di lavoro, casi d'uso) prima dell'implementazione.
2. **Revisione umana** di ogni modifica.
3. **Review automatica** con due agenti in sola lettura (`.claude/agents/`):
   - `ztoryc-reviewer` — correttezza, coerenza con la codebase, ottimizzazione;
   - `license-guard` — licenze e provenienza del codice.
4. **Test** e build su tutte le piattaforme supportate (CI).
5. **Guardia licenze nel pre-push**: scansione SCANOSS dei file di codice modificati
   (`.githooks/pre-push`, installato da `scripts/install-git-safety.sh`).

## Licenze

Ztoryc e' distribuito con licenza BSD 3-Clause. Per non comprometterla:

- **Dai progetti GPL si prendono idee, mai codice** — neanche "tradotto" o riscritto
  guardando l'originale. Le idee, gli algoritmi descritti nei paper e le scelte di
  interfaccia sono liberi; il codice no. Gli strumenti AI non ricevono sorgenti GPL
  come base da cui derivare codice.
- **Strumenti GPL solo come processi separati**, mai linkati ne' inclusi
  (oggi: espeak-ng, Rhubarb, ffmpeg in configurazione LGPL). Se ne distribuiamo il
  binario, distribuiamo anche il sorgente.
- **Dipendenze** solo con licenze compatibili (MIT, BSD, Apache-2.0, zlib, ISC, ...),
  con il testo di licenza in `stuff/doc/LICENSE/`.
- **Audit periodico**: vedi `LICENSE_AUDIT.md`.

## Tracciabilita'

- I commit con contributo AI portano il trailer `Co-Authored-By`.
- Le specifiche di progetto documentano il contributo progettuale umano.
- Il codice portato da OpenToonz e' registrato in `OPENTOONZ_IMPORTS.md`.

## Contributi esterni

Accogliamo contributi scritti con o senza AI, alle stesse condizioni:

- **Dichiarare l'uso di AI** nella descrizione della PR, con una breve nota su approccio
  e prompt (come richiesto da OpenToonz);
- **PR piccole, una per argomento**, perche' possano essere seguite e capite;
- chi invia la PR **comprende il codice** e sa spiegarlo, modificarlo e mantenerlo;
- rispetto delle regole sulle licenze qui sopra.

## Rapporto con i progetti upstream

- **OpenToonz**: seguiamo le sue indicazioni sulle PR assistite da AI (stesso processo di
  revisione del codice scritto a mano, PR piccole e separate, uso dell'AI dichiarato).
- **Tahoma2D**: rispettiamo la sua policy (`AI_POLICY.md` nel loro repository). Le PR
  verso Tahoma2D contengono solo codice riscritto e compreso a fondo dall'autore.
