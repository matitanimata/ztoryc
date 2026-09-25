# AI in Ztoryc development · Sviluppo con AI in Ztoryc

_Version of 2026-09-25, approved by the maintainer ·
Versione del 2026-09-25, approvata dal maintainer._

---

## 🇬🇧 English

Ztoryc is developed with the help of AI tools (mainly Claude Code). This document
explains how, and which safeguards we apply. **For Ztoryc it replaces Tahoma2D's
`AI_POLICY.md`**, which is not kept in this repository.

### Principle

AI is a development partner. **Responsibility for the code stays human**: every change
that enters the repository is tested and approved by the project's maintainer.

### Process

1. **Important features start from written specifications** (architecture, workflows,
   use cases) before they are implemented.
2. **Every change is tested and approved** by the maintainer, in real production use.
3. **Automated review** by two read-only agents (`.claude/agents/`):
   - `ztoryc-reviewer` — correctness, consistency with the codebase, performance;
   - `license-guard` — licences and code provenance.
4. **Builds on every supported platform** (macOS, Windows, Linux) through CI.
5. **Licence guard before every push**: a SCANOSS scan of the modified code files
   (`.githooks/pre-push`, installed by `scripts/install-git-safety.sh`).

### Licences — our particular care

Ztoryc is released under the BSD 3-Clause licence. Keeping it that way is a priority,
and we treat the provenance of code with particular care:

- **From GPL projects we take ideas, never code** — not even "translated" or rewritten
  while looking at the original. Ideas, algorithms described in papers and interface
  choices are free; code is not. AI tools are not given GPL sources as a basis to
  derive code from.
- **GPL programs only as separate processes**, never linked or included. Today the only
  one is espeak-ng, and its source is distributed together with its binary. (Rhubarb
  Lip Sync is MIT; ffmpeg is used in its LGPL configuration.)
- **Dependencies** only under compatible licences (MIT, BSD, Apache-2.0, zlib, ISC, …),
  each with its licence text in `stuff/doc/LICENSE/`.
- **Tools, then people.** Code is checked with SCANOSS and ScanCode; when a licence or
  provenance question cannot be settled by the tools and our own review, **we ask
  experienced external developers for advice** before the code is merged.
- **Periodic audit**: see `LICENSE_AUDIT.md`.

### Traceability

- Commits with an AI contribution carry a `Co-Authored-By` trailer.
- Design documents record the human design work behind each feature.
- Code ported from OpenToonz is listed in `OPENTOONZ_IMPORTS.md`.

### Contributions to Ztoryc

We welcome contributions written with or without AI. For AI-assisted pull requests we
adopt **the criteria set by Shun Iwasawa for OpenToonz** (OpenToonz discussion #6937,
June 2026):

- **small, separate pull requests, one per topic** — no large, multi-purpose changes;
- each pull request **describes the changes and the reason for them**;
- it states **the name and version of the AI model** that generated the changes, and
  includes **the prompt (or design document)** given to it; prompts may be shared under
  CC-BY, and their authors credited as `Co-authored-by`;
- it goes through **the same review as human-written code**, which can take time and
  several rounds of revisions;
- the contributor **understands the code** and can explain, change and maintain it —
  the goal is that people, not only the AI, understand the source.

The licence rules above apply to every contribution.

### Upstream

Fixes and improvements that are useful to the shared codebase are proposed to
**OpenToonz**, following its AI-assisted pull-request policy, after a first review with
members of the Tahoma2D/OpenToonz community.

---

## 🇮🇹 Italiano

Ztoryc è sviluppato con l'aiuto di strumenti AI (soprattutto Claude Code). Questo
documento spiega come, e quali garanzie applichiamo. **Per Ztoryc sostituisce
l'`AI_POLICY.md` di Tahoma2D**, che non è mantenuto in questo repository.

### Principio

L'AI è un partner di sviluppo. **La responsabilità del codice resta umana**: ogni
modifica che entra nel repository è collaudata e approvata da chi mantiene il progetto.

### Processo

1. **Le funzionalità importanti nascono da specifiche scritte** (architettura, flussi di
   lavoro, casi d'uso) prima dell'implementazione.
2. **Ogni modifica è collaudata e approvata** da chi mantiene il progetto, nell'uso
   reale in produzione.
3. **Revisione automatica** con due agenti in sola lettura (`.claude/agents/`):
   - `ztoryc-reviewer` — correttezza, coerenza con la codebase, prestazioni;
   - `license-guard` — licenze e provenienza del codice.
4. **Build su tutte le piattaforme supportate** (macOS, Windows, Linux) tramite CI.
5. **Guardia licenze prima di ogni push**: scansione SCANOSS dei file di codice
   modificati (`.githooks/pre-push`, installato da `scripts/install-git-safety.sh`).

### Licenze — la nostra particolare attenzione

Ztoryc è distribuito con licenza BSD 3-Clause. Mantenerla tale è una priorità, e la
provenienza del codice la trattiamo con particolare attenzione:

- **Dai progetti GPL si prendono idee, mai codice** — neanche «tradotto» o riscritto
  guardando l'originale. Le idee, gli algoritmi descritti nei paper e le scelte di
  interfaccia sono liberi; il codice no. Gli strumenti AI non ricevono sorgenti GPL come
  base da cui derivare codice.
- **Programmi GPL solo come processi separati**, mai linkati né inclusi. Oggi l'unico è
  espeak-ng, e il suo sorgente è distribuito insieme al binario. (Rhubarb Lip Sync è MIT;
  ffmpeg è usato nella configurazione LGPL.)
- **Dipendenze** solo con licenze compatibili (MIT, BSD, Apache-2.0, zlib, ISC, …),
  ognuna con il suo testo di licenza in `stuff/doc/LICENSE/`.
- **Prima gli strumenti, poi le persone.** Il codice è controllato con SCANOSS e
  ScanCode; quando una questione di licenza o di provenienza non si risolve con gli
  strumenti e la nostra revisione, **chiediamo il parere di sviluppatori esterni
  esperti** prima di integrare il codice.
- **Audit periodico**: vedi `LICENSE_AUDIT.md`.

### Tracciabilità

- I commit con contributo AI portano il trailer `Co-Authored-By`.
- I documenti di progetto registrano il lavoro di progettazione umano dietro ogni funzione.
- Il codice portato da OpenToonz è registrato in `OPENTOONZ_IMPORTS.md`.

### Contributi a Ztoryc

Accogliamo contributi scritti con o senza AI. Per le pull request assistite dall'AI
adottiamo **i criteri fissati da Shun Iwasawa per OpenToonz** (discussione OpenToonz
#6937, giugno 2026):

- **pull request piccole e separate, una per argomento** — niente modifiche grandi e
  con più scopi;
- ogni pull request **descrive le modifiche e il loro motivo**;
- indica **nome e versione del modello AI** che ha generato le modifiche, e riporta
  **il prompt (o il documento di progetto)** che gli è stato dato; i prompt possono essere
  condivisi con licenza CC-BY, e i loro autori citati come `Co-authored-by`;
- passa **la stessa revisione del codice scritto a mano**, che può richiedere tempo e
  diversi giri di correzioni;
- chi la propone **capisce il codice** e sa spiegarlo, modificarlo e mantenerlo — lo
  scopo è che il sorgente lo capiscano le persone, non solo l'AI.

A ogni contributo si applicano le regole sulle licenze qui sopra.

### Verso i progetti upstream

Le correzioni e i miglioramenti utili al codice comune si propongono a **OpenToonz**,
secondo la sua policy sulle pull request assistite dall'AI, dopo una prima revisione con
membri della comunità Tahoma2D/OpenToonz.
