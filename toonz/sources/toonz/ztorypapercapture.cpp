#include "ztorypapercapture.h"

#include "ztorypapersheet.h"  // findSheetCorners

#include "../stopmotion/webcam.h"
#include "toonzqt/gutil.h"  // rasterToQImage

#include <QCameraInfo>
#include <QSignalBlocker>
#include <QWidget>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

//=============================================================================
// ZtoryPaperPreview
//=============================================================================

ZtoryPaperPreview::ZtoryPaperPreview(QWidget *parent) : QWidget(parent) {
  setMinimumSize(480, 300);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ZtoryPaperPreview::setFrame(const QImage &frame, const QPolygonF &corners,
                                 bool found) {
  m_frame   = frame;
  m_corners = corners;
  m_found   = found;
  update();
}

void ZtoryPaperPreview::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(30, 30, 30));
  if (m_frame.isNull()) {
    p.setPen(QColor(150, 150, 150));
    p.drawText(rect(), Qt::AlignCenter, tr("No camera"));
    return;
  }

  // Letterbox the frame, keeping its aspect.
  const QSize scaled = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
  const QRect target(QPoint((width() - scaled.width()) / 2,
                            (height() - scaled.height()) / 2),
                     scaled);
  p.setRenderHint(QPainter::SmoothPixmapTransform, true);
  p.drawImage(target, m_frame);

  if (m_corners.size() != 4) return;
  // Map the detected quad from frame space into the letterboxed rectangle.
  const double sx = (double)target.width() / m_frame.width();
  const double sy = (double)target.height() / m_frame.height();
  QPolygonF poly;
  for (const QPointF &c : m_corners)
    poly << QPointF(target.x() + c.x() * sx, target.y() + c.y() * sy);

  QPen pen(m_found ? QColor(80, 220, 120) : QColor(200, 200, 200),
           m_found ? 3.0 : 1.5);
  if (!m_found) pen.setStyle(Qt::DashLine);
  p.setPen(pen);
  p.setBrush(Qt::NoBrush);
  p.drawPolygon(poly);
}

//=============================================================================
// ZtoryPaperCaptureDialog
//=============================================================================

ZtoryPaperCaptureDialog::ZtoryPaperCaptureDialog(QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(tr("Capture Sheet from Camera"));
  setModal(true);
  resize(760, 620);

  auto *root = new QVBoxLayout(this);

  auto *top = new QHBoxLayout();
  top->addWidget(new QLabel(tr("Camera:"), this));
  m_cameraBox = new QComboBox(this);
  top->addWidget(m_cameraBox, 1);
  root->addLayout(top);

  m_preview = new ZtoryPaperPreview(this);
  root->addWidget(m_preview, 1);

  m_status = new QLabel(this);
  m_status->setAlignment(Qt::AlignCenter);
  root->addWidget(m_status);

  auto *bottom = new QHBoxLayout();
  m_shootBtn   = new QPushButton(tr("Capture sheet"), this);
  m_shootBtn->setEnabled(false);
  m_shootBtn->setDefault(true);
  bottom->addWidget(m_shootBtn);
  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  m_importBtn = buttons->button(QDialogButtonBox::Ok);
  m_importBtn->setText(tr("Import captured (0)"));
  // Nothing to import until at least one shot is taken: leaving it enabled made
  // the button look broken (it closed the dialog and silently imported nothing).
  m_importBtn->setEnabled(false);
  bottom->addWidget(buttons, 1);
  root->addLayout(bottom);

  m_webcam = new Webcam();
  m_timer  = new QTimer(this);
  m_timer->setInterval(40);  // ~25 fps, same cadence as the Stop Motion room

  connect(m_timer, &QTimer::timeout, this, [this] { grabFrame(); });
  connect(m_shootBtn, &QPushButton::clicked, this, [this] { shoot(); });
  connect(m_cameraBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this](int i) { startCamera(i); });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  populateCameras();
}

ZtoryPaperCaptureDialog::~ZtoryPaperCaptureDialog() {
  stopCamera();
  delete m_webcam;
}

void ZtoryPaperCaptureDialog::populateCameras() {
  const QList<QCameraInfo> cams = QCameraInfo::availableCameras();
  if (cams.isEmpty()) {
    m_status->setText(tr("No camera found."));
    m_cameraBox->setEnabled(false);
    return;
  }
  QSignalBlocker blocker(m_cameraBox);
  for (const QCameraInfo &c : cams) m_cameraBox->addItem(c.description());
  blocker.unblock();
  startCamera(0);
}

void ZtoryPaperCaptureDialog::startCamera(int comboIndex) {
  stopCamera();
  const QList<QCameraInfo> cams = QCameraInfo::availableCameras();
  if (comboIndex < 0 || comboIndex >= cams.size()) return;

  // Description + device name must be set BEFORE translateIndex: on macOS it
  // matches the device UID to the right capture device (see stopmotion.cpp).
  m_webcam->setWebcamDescription(cams.at(comboIndex).description());
  m_webcam->setWebcamDeviceName(cams.at(comboIndex).deviceName());
  m_webcam->translateIndex(comboIndex);

  if (!m_webcam->initWebcam(comboIndex)) {
    m_status->setText(tr("Could not open this camera."));
    return;
  }
  m_status->setText(tr("Point the camera at the whole sheet."));
  m_timer->start();
}

void ZtoryPaperCaptureDialog::stopCamera() {
  if (m_timer) m_timer->stop();
  if (m_webcam) m_webcam->releaseWebcam();
}

void ZtoryPaperCaptureDialog::grabFrame() {
  TRaster32P ras;
  if (!m_webcam->getWebcamImage(ras) || !ras) return;
  QImage frame = rasterToQImage(ras, /*premultiplied=*/false).copy();
  if (frame.isNull()) return;
  m_lastFrame = frame;

  // Marker detection is much heavier than a frame grab, so only run it every
  // few frames — the preview stays fluid and the feedback is still immediate.
  if (--m_detectSkip <= 0) {
    m_detectSkip = 2;
    QPolygonF corners;
    m_sheetFound = ZtoryPaperSheet::findSheetCorners(frame, corners);
    m_preview->setFrame(frame, corners, m_sheetFound);
    m_shootBtn->setEnabled(m_sheetFound);
    m_status->setText(m_sheetFound
                          ? tr("Sheet detected — ready to capture.")
                          : tr("Point the camera at the whole sheet: all four "
                               "corner markers must be visible."));
  } else {
    m_preview->setFrame(frame, QPolygonF(), m_sheetFound);
  }
}

void ZtoryPaperCaptureDialog::shoot() {
  if (m_lastFrame.isNull()) return;
  m_captured.push_back(m_lastFrame);
  if (m_importBtn) {
    m_importBtn->setEnabled(true);
    m_importBtn->setText(tr("Import captured (%1)").arg(m_captured.size()));
  }
  m_status->setText(tr("Captured %1 sheet(s). Turn the page and capture again, "
                       "or press Import captured.")
                        .arg(m_captured.size()));
}
