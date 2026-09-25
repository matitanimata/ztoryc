# Registro dei port da OpenToonz

Ztoryc e' un fork di **Tahoma2D** (base, merge regolari) che integra **selettivamente**
funzionalita' e fix da **OpenToonz**. Da OpenToonz non si fa merge (le storie git sono
separate da anni): si fanno **port mirati**, uno alla volta, adattati alla base Tahoma2D.

Questo file registra cio' che e' **effettivamente entrato** in Ztoryc.
I candidati da valutare stanno in `OPENTOONZ_PORT_CANDIDATES.md`.

## Perche' esiste

Tahoma2D importa periodicamente modifiche da OpenToonz. Se Ztoryc ha gia' portato una
modifica e poi la porta anche Tahoma2D, al merge successivo le due versioni vanno in
conflitto. Il registro permette di riconoscere subito questi casi.

**Regola di conflitto:** se al merge da Tahoma2D arriva una modifica gia' portata da
OpenToonz, di norma si **tiene la versione di Tahoma2D** (riduce la divergenza), poi si
aggiorna la voce qui sotto ("Stato in Tahoma2D").

## Regole

- Licenza: OpenToonz e' BSD 3-Clause come Tahoma2D e Ztoryc, quindi compatibile.
- **Mantenere gli header di copyright originali** nei file portati.
- Verificare la licenza di eventuali parti di `thirdparty/` coinvolte.
- Ogni port aggiorna questo file **nello stesso commit** del codice.
- `license-guard` controlla che ogni port abbia la sua voce.

## Formato di una voce

```
## [AAAA-MM-GG] Titolo del port
- Origine: opentoonz PR #NNNN / commit <hash>
- Autore originale:
- File toccati in Ztoryc:
- Adattamenti rispetto all'originale:
- Stato in Tahoma2D: non presente / importato in Tahoma2D (versione X)
- Note:
```

---

## Port registrati

_Da compilare: registrazione retroattiva dei port gia' fatti prima del 2026-09-25._
