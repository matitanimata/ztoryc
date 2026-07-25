#pragma once

// ZtoryPaperCaptureDialog — photograph a printed thumbnail sheet with a webcam
// (or capture card) from inside Ztoryc, instead of shooting with a phone and
// importing the file (task 63, phase 2b).
//
// It reuses the Stop Motion room's Webcam wrapper (device enumeration, macOS
// AVCaptureSession, frame grabbing) and feeds the captured frame to the very
// same ZtoryPaperSheet::importSheet pipeline the file import uses — one code
// path, so both sources behave identically.
//
// While previewing, each frame is run through the cheap marker detector so the
// user can see whether the sheet is framed BEFORE shooting: the outline turns
// solid and the shutter is enabled only when all four markers are visible.

#include <QDialog>
#include <QImage>
#include <QPolygonF>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;
class Webcam;

// Live preview + detected-sheet overlay.
class ZtoryPaperPreview final : public QWidget {
  Q_OBJECT
public:
  explicit ZtoryPaperPreview(QWidget *parent = nullptr);
  void setFrame(const QImage &frame, const QPolygonF &corners, bool found);
  QSize sizeHint() const override { return QSize(640, 400); }

protected:
  void paintEvent(QPaintEvent *) override;

private:
  QImage m_frame;
  QPolygonF m_corners;  // in frame coordinates
  bool m_found = false;
};

class ZtoryPaperCaptureDialog final : public QDialog {
  Q_OBJECT

public:
  explicit ZtoryPaperCaptureDialog(QWidget *parent = nullptr);
  ~ZtoryPaperCaptureDialog() override;

  // Frames captured with the shutter, in capture order — the order they will be
  // placed in the grid. Empty if the user shot nothing.
  const QList<QImage> &captured() const { return m_captured; }

private:
  void populateCameras();
  void startCamera(int comboIndex);
  void stopCamera();
  void grabFrame();     // timer tick: pull a frame, detect, repaint
  void shoot();         // keep the current frame

  Webcam *m_webcam       = nullptr;
  QTimer *m_timer        = nullptr;
  QComboBox *m_cameraBox = nullptr;
  ZtoryPaperPreview *m_preview = nullptr;
  QLabel *m_status       = nullptr;
  QPushButton *m_shootBtn  = nullptr;
  QPushButton *m_importBtn = nullptr;  // the button box's OK, relabelled

  QImage m_lastFrame;      // most recent preview frame (full resolution)
  bool m_sheetFound = false;
  QList<QImage> m_captured;
  int m_detectSkip = 0;    // detect every Nth frame to keep the preview fluid
};
