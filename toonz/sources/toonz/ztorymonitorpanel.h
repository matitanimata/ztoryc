#pragma once

// ZtoryMonitorPanel — floating viewer + timeline for second-monitor workflow.
//
// Combines a ZtoryAnimaticViewer (always main xsheet, no shot-toggle) with a
// compact ruler + video track below.  Designed to be floated onto a second
// monitor and left permanently visible regardless of the active room.
//
// Key difference from ZtoryAnimaticViewerPanel:
//   • No stacked-widget / shot toggle: the viewer is always in animatic mode.
//   • Double-click on a shot block seeks to that shot's first frame instead of
//     opening the sub-scene, so the panel never switches context.

#include "pane.h"
#include "ztoryanimatic.h"

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>

class ZtoryMonitorPanel : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryMonitorPanel(QWidget *parent = nullptr);

protected:
  void showEvent(QShowEvent *e) override;

private slots:
  void onShotClicked(int col);
  void onShotDoubleClicked(int col);  // seek only — no sub-scene open
  void onFrameChanged(int frame);
  void onZoomChanged(double ppf);
  void onModelChanged();

private:
  ZtoryAnimaticViewer *m_viewer = nullptr;
  ZtoryAnimaticRuler  *m_ruler  = nullptr;
  ZtoryAnimaticTrack  *m_track  = nullptr;
  double               m_ppf    = 8.0;
};
