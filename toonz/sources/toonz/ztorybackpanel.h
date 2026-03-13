#pragma once
#include "pane.h"
#include <QPushButton>
#include <QLabel>

class ZtoryBackPanel final : public TPanel {
  Q_OBJECT
public:
  explicit ZtoryBackPanel(QWidget *parent = nullptr);
private slots:
  void onBackClicked();
};
