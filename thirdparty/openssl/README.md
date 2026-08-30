# OpenSSL 1.1.1w — DLL Windows a 64 bit

Servono a **Qt 5.15.2 per fare HTTPS su Windows**. Senza, qualunque connessione
cifrata fallisce con `TLS initialization failed` — e questo comprende
l'integrazione con **Kitsu**, il controllo aggiornamenti, e ogni altra chiamata
a un indirizzo `https://`.

**Non le copia `windeployqt`**: le librerie TLS non fanno parte di Qt e vanno
aggiunte a mano. Per questo sono versionate qui, come `freeglut` e `glew`.

## Perche' proprio la 1.1.1

Qt 5.15.2 e' del 2020 e cerca **OpenSSL 1.1.1**, con questi nomi esatti:

```
libssl-1_1-x64.dll
libcrypto-1_1-x64.dll
```

La serie **3.x non va**: ha nomi diversi (`libssl-3-x64.dll`) e un'altra ABI.

⚠️ La 1.1.1 e' **fuori supporto dal settembre 2023**, e la `1.1.1w` e' l'ultima
mai rilasciata. E' un debito noto: si chiude solo aggiornando Qt, non
cambiando queste DLL.

## Provenienza

Pacchetto **conda-forge** `openssl-1.1.1w-hcfcfb64_0.conda` (win-64), scaricato
il 2026-08-30 da `conda.anaconda.org` e verificato:

```
sha256 del pacchetto  6d46986fab161cb1b62f019c06dfc27f2e15caccd3943b3282d9e59872fa4ad2
libcrypto-1_1-x64.dll 67a17e76bebdf64f8a43909a254483ce7631989a8caaaac74057cfedde61e2f9
libssl-1_1-x64.dll    1dfdf13bef337ca9a9ff8fba4c59e137c5c52a251eddfedef0e5ffbda09ef4bb
```

Verificate come `PE32+ executable (DLL) x86-64, for MS Windows`.

## Licenza

OpenSSL 1.1.1 e' sotto **doppia licenza OpenSSL / SSLeay**, che permette la
ridistribuzione binaria mantenendo le note di copyright. Il testo completo sta
in `LICENSE`.

## Come finiscono nel pacchetto

`ci-scripts/windows/tahoma-buildpkg.bat` le copia accanto a `Ztoryc.exe` dopo
`windeployqt`. Devono stare **nella stessa cartella dell'eseguibile**: Qt le
cerca li' e nel PATH, e la cartella dell'eseguibile e' l'unico posto su cui
abbiamo controllo.

## Per correggere un'installazione gia' fatta

Copiare i due file in `Ztoryc\` accanto a `Ztoryc.exe` e riavviare. Nessuna
installazione, nessuna variabile d'ambiente.
