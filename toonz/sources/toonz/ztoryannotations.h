#pragma once

#include "pane.h"
#include <QWidget>
#include <functional>
#include <vector>

class TStroke;

// Pannello "Camera Moves" — FASE 1
// Inserisce frecce vettoriali (Pan) nella colonna "Annotazioni" della sub-scena corrente.
// La colonna viene creata automaticamente se non esiste.
// Editing con i tool nativi (selezione, scala, ruota, colore).

class ZtoryCameraMovesPanel final : public TPanel {
  Q_OBJECT
public:
  explicit ZtoryCameraMovesPanel(QWidget *parent = nullptr);

private:
  void insertArrow(double dx, double dy);
  int  ensureAnnotationColumn();  // returns col index, -1 on failure
};
