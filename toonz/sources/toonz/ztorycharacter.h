#pragma once

//============================================================================
// ZtoryCharacter — il RUOLO DELLA SCENA: «questa scena e' un personaggio».
//
// ⚠️ La mappatura delle bocche NON sta qui. Franco, 2026-08-16: «il role
// character e' legato alla scena, i dati del mapping delle bocche sono
// collegati ai livelli delle bocche di quel character». Due oggetti diversi:
//
//   la SCENA   -> `role="character"` nel .ztoryc   (questo file)
//   il LIVELLO -> i set di bocche accanto ad esso  (ztorymouthmap.h)
//
// Qui restano il ruolo e i badge (SB / SH / CH). Le pose registrate e le
// correttive, quando si riapre ZtoRig, andranno decise allo stesso modo: sulla
// scena o sul livello a seconda di CHI le possiede davvero.
//
// ── PERCHE' E' SEPARATO DA ZtoryModel ──────────────────────────────────────
// ZtoryModel e' il singleton della scena CORRENTE. Un personaggio va invece
// letto quando NON e' aperto: si sta lavorando in uno shot e si vuole il set
// di bocche che sta nella scena di libreria. Legarlo al modello obbligherebbe
// ad aprire il personaggio per sapere com'e' fatta la sua bocca.
//
// ── DOVE VIVE IL DATO ──────────────────────────────────────────────────────
// Nel sidecar `.ztoryc` accanto alla SCENA del personaggio (`LIB_NOME.tnz` ->
// `LIB_NOME.ztoryc`), con `role="character"`. Il sidecar esiste gia' ed e' gia'
// letto e scritto: non serve inventare un formato nuovo.
//
// ⚠️ StoryboardPanel::saveZtoryc() scrive `role="storyboard"` per QUALSIASI
// scena non-shot, quindi riscriverebbe sopra al sidecar di un personaggio
// aperto in Ztoryc. Il guard sta li' (`m_currentSceneIsShot` ha il gemello per
// i personaggi): e' lo stesso bug gia' incontrato con i `role="shot"`.
//
//============================================================================

#include <QString>

namespace ZtoryCharacter {

//! Il sidecar che affianca una scena: `.../LIB_NOME.tnz` -> `.../LIB_NOME.ztoryc`.
//! Accetta gia' un percorso `.ztoryc` e lo restituisce invariato.
QString sidecarPathFor(const QString &scenePath);

//! Dichiara \p scenePath scena personaggio, scrivendo il suo sidecar.
//! E' l'ATTO: il ruolo non si deduce da dove sta il file, si scrive.
bool declareCharacterScene(const QString &scenePath, const QString &assetUuid,
                           const QString &assetName, QString *error = nullptr);

//! Il ruolo dichiarato dal sidecar di \p scenePath: "storyboard", "shot",
//! "character", o stringa vuota se non c'e' sidecar. Un sidecar SENZA
//! l'attributo e' storyboard — e' come si scrivevano prima che il ruolo
//! esistesse, e sono ancora in giro.
QString roleOf(const QString &scenePath);

//! Cambia il ruolo del sidecar LASCIANDO INTATTO TUTTO IL RESTO.
//!
//! E' per l'errore di scelta («l'ho creata come storyboard e invece e' un
//! personaggio»), quindi deve costare poco: riscrivere il file da capo con la
//! sola chiave nuova butterebbe via shot, dialoghi e set, cioe' punirebbe un
//! refuso con una perdita di dati. Si sostituisce l'attributo nel testo e basta.
bool setRole(const QString &scenePath, const QString &role,
             QString *error = nullptr);

//! Il personaggio a cui la scena si riferisce, dal blocco <character> del suo
//! sidecar. Entrambi possono restare vuoti: una scena puo' essere dichiarata
//! personaggio senza essere legata a un asset di progetto, e funziona lo
//! stesso — semplicemente non si trova partendo dal copione.
void characterRef(const QString &scenePath, QString *uuid, QString *name);

//! Vero se il sidecar di \p scenePath dichiara `role="character"`. Serve al
//! guard di StoryboardPanel::saveZtoryc(), che altrimenti ci scriverebbe sopra
//! `role="storyboard"` alla prima apertura, in silenzio.
bool isCharacterScene(const QString &scenePath);

}  // namespace ZtoryCharacter
