#pragma once
#include "viewerpane.h"
#include "tapp.h"
#include "pane.h"
#ifndef ZTORYANIMATIC_H
#define ZTORYANIMATIC_H

#include "pane.h"
#include <QWidget>
#include <QScrollArea>

class ZtoryAnimaticRuler : public QWidget {
  Q_OBJECT
public:
  ZtoryAnimaticRuler(QWidget *parent = nullptr);
  void setFps(double fps) { m_fps = fps; update(); }
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  int currentFrame() const { return m_currentFrame; }
protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
signals:
  void frameChanged(int frame);
private:
  double m_fps = 24.0;
  double m_ppf = 8.0; // pixels per frame
  int m_currentFrame = 0;
};

class ZtoryAnimaticTrack : public QWidget {
  Q_OBJECT
public:
  struct ShotBlock {
    int col;
    int startFrameInMain; // frame di inizio nel main xsheet
    int f0, f1;           // marker In/Out della sottoscena
    QString shotNumber;
    QPixmap thumbnail;
  };

  ZtoryAnimaticTrack(QWidget *parent = nullptr);
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  void refreshFromScene();
protected:

protected:
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

signals:
  void shotClicked(int col);
  void shotDurationChanged(int col, int newF1);
  void shotMoved(int col, int newStartFrame);
  void zoomChanged(double ppf);
  void matchSubsceneDuration(int col);

private:
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  std::vector<ShotBlock> m_blocks;
  int m_draggingCol = -1;
  int m_dragStartX = 0;
  int m_dragOrigF1 = 0;
  QMap<int, int> m_origStarts;
  QMap<int, int> m_origDurations;
};

class ZtoryAnimaticViewer : public BaseViewerPanel {
  Q_OBJECT
public:
  ZtoryAnimaticViewer(QWidget *parent = nullptr)
    : BaseViewerPanel(parent) {
    m_sceneViewer->setAlwaysMainXsheet(true);
    // Disconnetti il segnale activeViewerChanged — non vogliamo che questo viewer
    // diventi il viewer attivo globale
    disconnect(TApp::instance(), SIGNAL(activeViewerChanged()),
               this, SLOT(onActiveViewerChanged()));
  }
  void updateShowHide() override {}
  void addShowHideContextMenu(QMenu *) override {}
  void checkOldVersionVisblePartsFlags(QSettings &) override {}
};

class ZtoryAnimaticPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryAnimaticPanel(QWidget *parent = nullptr);
  void refreshFromScene();
protected:
  void showEvent(QShowEvent *e) override;

private slots:
  void onShotClicked(int col);
  void onShotDurationChanged(int col, int newF1);
  void resequenceXsheet();
  void onZoomChanged(double ppf);
  void onMatchSubsceneDuration(int col);
  void onFrameChanged(int frame);

private:
  ZtoryAnimaticRuler *m_ruler;
  ZtoryAnimaticTrack *m_track;
  ZtoryAnimaticViewer *m_animViewer;
  double m_ppf = 8.0;
};

#endif

