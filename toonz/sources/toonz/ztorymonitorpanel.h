#pragma once

// ZtoryMonitorPanel — floating viewer + full timeline for second-monitor workflow.
//
// Combines a ZtoryAnimaticViewer (always main xsheet) with a complete toolbar,
// ruler, video track and dynamic audio tracks.  Designed to be floated onto a
// second monitor and left permanently visible regardless of the active room.
//
// Key differences from ZtoryAnimaticPanel:
//   • Viewer always shows the main xsheet (no stacked-widget / shot toggle).
//   • Double-click opens the shot sub-scene in the main application context
//     while the monitor viewer continues to show the main animatic.
//   • Shot edit operations (add, delete, merge, copy, paste) are forwarded to
//     ZtoryAnimaticPanel when it is open; they are no-ops otherwise.

#include "pane.h"
#include "ztoryanimatic.h"

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QSlider>
#include <QToolButton>

class ZtoryMonitorPanel : public TPanel {
  Q_OBJECT

public:
  explicit ZtoryMonitorPanel(QWidget *parent = nullptr);

protected:
  void showEvent(QShowEvent *e) override;

private slots:
  void onShotClicked(int col);
  void onShotDoubleClicked(int col);  // open sub-scene in main app
  void onReturnToMain();
  void onFrameChanged(int frame);
  void onZoomChanged(double ppf);
  void onModelChanged();
  void refreshAudioTracks();

private:
  ZtoryAnimaticViewer *m_viewer      = nullptr;
  ZtoryAnimaticRuler  *m_ruler       = nullptr;
  ZtoryAnimaticTrack  *m_track       = nullptr;
  QVBoxLayout         *m_timelineLay = nullptr;  // ruler + video + audio tracks
  QList<ZtoryAudioTrack *> m_audioTracks;
  double               m_ppf         = 8.0;
  size_t               m_audioFP     = 0;  // fingerprint to skip no-op rebuilds
  QTimer              *m_refreshTimer = nullptr;  // debounces rapid onModelChanged calls
  // Toolbar widgets (kept for zoom sync)
  QSlider      *m_zoomSlider  = nullptr;
  QToolButton  *m_selectBtn   = nullptr;
  QToolButton  *m_trimBtn     = nullptr;
  QToolButton  *m_razorBtn    = nullptr;
  QToolButton  *m_timecodeBtn = nullptr;
};
