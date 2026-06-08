#pragma once

#include "pane.h"
#include "tfilepath.h"
#include <QWidget>
#include <vector>

class QGridLayout;

// Pannello "Annotations" — FASE 1 (task 40)
// Libreria di frecce vettoriali (.pli) per indicazioni di movimento personaggi /
// camera. Le frecce vengono lette da una cartella bundled (ztorycstuff/library/
// directional arrows) più una cartella utente opzionale, mostrate come thumbnail
// e, al click, stampate nella colonna "Annotazioni" della sub-scena corrente.
// La colonna viene creata automaticamente; editing con i tool nativi.

class ZtoryCameraMovesPanel final : public TPanel {
  Q_OBJECT
public:
  explicit ZtoryCameraMovesPanel(QWidget *parent = nullptr);

private:
  int  ensureAnnotationColumn();            // returns col index, -1 on failure
  void insertArrowFromFile(const TFilePath &pliPath);
  void rebuildLibrary();                     // scan folders → populate grid
  void chooseUserFolder();

  QGridLayout *m_grid = nullptr;
};
