#!/usr/bin/env python3
"""Impacchetta una cartella modello Vosk in un archivio che Ztoryc sa aprire.

PERCHE' NON UNO ZIP: leggere uno zip in C++ vorrebbe dire tirare dentro minizip
(che sta in thirdparty/zlib ma non e' compilato) o una dipendenza nuova, e la
build su tre sistemi e' gia' il punto dove questo progetto ha perso piu' tempo.
I blocchi qui sono compressi nel formato di `qCompress`, cioe' quattro byte di
lunghezza in big-endian piu' uno stream zlib: `qUncompress()` li apre con la
sola Qt, che c'e' gia' ovunque.

Formato:
    "ZVOSK1\\n"                7 byte
    numero di file            quint32 big-endian
    per ogni file:
        lunghezza percorso    quint32 big-endian
        percorso (utf-8, relativo alla radice del modello)
        lunghezza blocco      quint32 big-endian
        blocco compresso      (4 byte di lunghezza originale + zlib)

Uso:  python3 pack_vosk_model.py <cartella-modello> <uscita.zvosk>
"""

import os
import struct
import sys
import zlib

MAGIC = b"ZVOSK1\n"


def pack(src, dst):
    files = []
    for root, _, names in os.walk(src):
        for n in sorted(names):
            if n.startswith("."):
                continue
            full = os.path.join(root, n)
            files.append((os.path.relpath(full, src), full))
    files.sort()

    raw_total = comp_total = 0
    with open(dst, "wb") as out:
        out.write(MAGIC)
        out.write(struct.pack(">I", len(files)))
        for rel, full in files:
            data = open(full, "rb").read()
            # formato qCompress: lunghezza originale big-endian + stream zlib
            blob = struct.pack(">I", len(data)) + zlib.compress(data, 9)
            enc = rel.replace(os.sep, "/").encode("utf-8")
            out.write(struct.pack(">I", len(enc)))
            out.write(enc)
            out.write(struct.pack(">I", len(blob)))
            out.write(blob)
            raw_total += len(data)
            comp_total += len(blob)
            print(f"  {rel:<40} {len(data)/1048576:7.1f} MB -> "
                  f"{len(blob)/1048576:6.1f} MB")

    print(f"\n{len(files)} file   {raw_total/1048576:.0f} MB -> "
          f"{comp_total/1048576:.0f} MB   ({dst})")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    if not os.path.isdir(sys.argv[1]):
        sys.exit(f"non e' una cartella: {sys.argv[1]}")
    pack(sys.argv[1], sys.argv[2])
