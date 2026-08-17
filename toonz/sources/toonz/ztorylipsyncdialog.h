#pragma once

//============================================================================
// ZtoryLipSyncDialog — UN posto solo che dice DOVE SEI.
//
// Il lip sync sono tre operazioni in fila, e sono tre domande diverse:
//
//     Generate   quando ogni bocca      (i tempi, dal suono)
//     Map        quale disegno e' quella bocca   (una volta per livello)
//     Apply      quale set su quale tratto       (cambia quando si gira)
//
// Il difetto non erano gli strumenti — erano giusti — ma il fatto che l'utente
// dovesse INDOVINARE in quale delle quattro situazioni si trovava (colonne
// si/no × bocche mappate si/no) e scegliere il comando di conseguenza, da un
// menu che non gliene diceva niente. Franco, 2026-08-17: «abbiamo creato uno
// strumento potente ma ora il difficile e' fare in modo che sia semplice
// usarlo».
//
// Qui le quattro situazioni diventano LA STESSA FINESTRA con spunte diverse:
// si legge cosa c'e' e cosa manca, e i pulsanti che servono sono accesi. Il
// caso peggiore — personaggio disegnato adesso, nessun set salvato — smette di
// essere un vicolo cieco perche' la riga dice «manca» e il bottone porta dove
// si mappa.
//============================================================================

#include <QString>

// Apre la finestra sullo shot la cui sotto-scena e' aperta adesso.
// Se non si e' dentro nessuno shot lo dice e non fa niente.
void ztoryShowLipSyncDialog(class QWidget *parent);
