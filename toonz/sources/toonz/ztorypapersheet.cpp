#include "ztorypapersheet.h"

#include "opencv2/opencv.hpp"

#include <QPdfWriter>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPixmap>
#include <QRectF>
#include <QPointF>
#include <QMarginsF>
#include <QVector>
#include <QHash>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace {

// A4 in millimetres (long × short). Both printing and import assume A4.
constexpr double kA4Long = 297.0, kA4Short = 210.0;

// Light cyan for the grid lines: high red so a red-channel read on import drops
// it while the black pencil survives, yet visible to the artist on paper.
const QColor kCyanFrame(150, 220, 235);

// ── Page geometry shared by printing and import ─────────────────────────────
// PageFrame is everything that depends only on the page (markers, header, code
// position) — NOT the grid — so the importer can compute the marker warp targets
// and locate the page code BEFORE it knows the grid shape (which the code holds).
struct PageFrame {
  double pageW = 0, pageH = 0, dpi = 0;
  double mIn = 0, mSz = 0, mSzTL = 0, mHalf = 0;
  QPointF mc[4];  // marker centres: 0 = TL, 1 = TR, 2 = BL, 3 = BR
  double contentLeft = 0, contentTop = 0, contentRight = 0, contentBottom = 0;
  double gridAreaW = 0, gridAreaH = 0;
  QPointF codeOrigin;  // top-left of the page-code strip (start square)
  double codeCell = 0;
  double headerH = 0, headerLeft = 0, headerW = 0, titleBandH = 0;
};

PageFrame computePageFrame(double pageW, double pageH, double dpi) {
  auto mm2px = [dpi](double mm) { return mm * dpi / 25.4; };
  PageFrame f;
  f.pageW = pageW;
  f.pageH = pageH;
  f.dpi   = dpi;
  f.mIn   = mm2px(5.0);
  f.mSz   = mm2px(8.0);
  // The TOP-LEFT marker is larger than the other three so it is identified as
  // the origin purely by AREA — far more robust than counting rings (a rotated
  // or flipped photo re-orients reliably even when detection is imperfect).
  f.mSzTL = f.mSz * 1.4;
  f.mHalf = f.mSz / 2.0;
  f.mc[0] = QPointF(f.mIn + f.mSzTL / 2.0, f.mIn + f.mSzTL / 2.0);       // TL
  f.mc[1] = QPointF(pageW - f.mIn - f.mHalf, f.mIn + f.mHalf);           // TR
  f.mc[2] = QPointF(f.mIn + f.mHalf, pageH - f.mIn - f.mHalf);           // BL
  f.mc[3] = QPointF(pageW - f.mIn - f.mHalf, pageH - f.mIn - f.mHalf);   // BR

  const double inset = mm2px(2.0);
  f.headerH      = f.mSzTL;
  f.contentLeft  = f.mIn + f.mHalf;
  f.contentRight = pageW - f.mIn - f.mHalf;
  // Grid + header clear the (larger) TL marker on the top / left.
  f.contentTop   = f.mc[0].y() + f.mSzTL / 2.0 + mm2px(2.5);
  f.contentBottom = pageH - f.mIn - f.mHalf;
  f.gridAreaW    = f.contentRight - f.contentLeft;
  f.gridAreaH    = f.contentBottom - f.contentTop;
  f.headerLeft   = f.mc[0].x() + f.mSzTL / 2.0 + inset;
  f.headerW      = (pageW - f.mIn - f.mSz - inset) - f.headerLeft;
  // Header rows, in explicit millimetres so the title can NEVER touch the code
  // strip: any ink of the title landing on a code square flips a bit and the
  // checksum fails, which reads to the user as "the sheet cannot be imported".
  f.titleBandH   = mm2px(5.5);
  f.codeCell     = mm2px(1.8);
  f.codeOrigin   = QPointF(f.headerLeft, f.mIn + f.titleBandH + mm2px(1.2));
  return f;
}

// GridFit is the cell size + pagination for a given grid on a PageFrame.
struct GridFit {
  double cellW = 0, cellH = 0;
  int perRows = 1, totalPages = 1;
};

GridFit computeGridFit(const PageFrame &f, int gridCols, int gridRows,
                       double aspect) {
  GridFit g;
  // Cells are ALWAYS as large as the page allows: full width divided by the
  // column count. We never shrink them to squeeze more rows onto one sheet —
  // a sheet is for drawing on, and small boxes are both hard to draw in and
  // low-resolution once photographed. However many rows fit at that size is
  // the page capacity (4 rows for a 4-column 16:9 grid); the rest spill onto
  // further pages. So a room grid that has grown to 4x8 still prints 4x4 twice,
  // not one cramped 4x8 sheet.
  g.cellW = f.gridAreaW / gridCols;
  double ch = g.cellW / aspect;
  if (ch > f.gridAreaH) {  // one row already taller than the page: fit height
    ch      = f.gridAreaH;
    g.cellW = ch * aspect;
  }
  g.cellH      = ch;
  g.perRows    = std::max(1, (int)std::floor(f.gridAreaH / ch));
  g.perRows    = std::min(g.perRows, std::max(1, gridRows));
  g.totalPages = (gridRows + g.perRows - 1) / g.perRows;
  return g;
}

// Top-left corner of the (centred) grid block on a given page.
QPointF gridOrigin(const PageFrame &f, const GridFit &g, int gridCols,
                   int gridRows, int pageIndex) {
  const int startRow     = pageIndex * g.perRows;
  const int rowsThisPage = std::min(g.perRows, gridRows - startRow);
  const double gridW = gridCols * g.cellW;
  const double gridH = rowsThisPage * g.cellH;
  return QPointF(f.contentLeft + std::max(0.0, (f.gridAreaW - gridW) / 2.0),
                 f.contentTop + std::max(0.0, (f.gridAreaH - gridH) / 2.0));
}

// ── Registration marker ─────────────────────────────────────────────────────
// `count` concentric filled squares, alternating black / white. The white rings
// carve holes so cv::findContours sees nested contours (a finder pattern). All
// four markers use the same 3 rings; the origin is fixed by SIZE (the TL marker
// is drawn larger — see computePageFrame), not by ring count.
void drawMarker(QPainter &p, double cx, double cy, double S, int count) {
  static const double kRatio[4] = {1.0, 0.64, 0.38, 0.18};
  count = std::min(count, 4);
  for (int i = 0; i < count; ++i) {
    double side  = S * kRatio[i];
    QColor color = (i % 2 == 0) ? Qt::black : Qt::white;
    p.fillRect(QRectF(cx - side / 2.0, cy - side / 2.0, side, side), color);
  }
}

// "Page [ ] / [ ]" — blank boxes the artist fills in by hand. The importer does
// not read them (the machine code carries the real page), they exist so a stack
// of printed or photocopied sheets can be kept in order on the desk.
// Returns the x where the group starts, so the caller can right-align it.
double drawPageBoxes(QPainter &p, double right, double topY, double bandH,
                     double dpi) {
  auto mm2px = [dpi](double mm) { return mm * dpi / 25.4; };
  const double box = std::min(mm2px(4.5), bandH * 0.8);
  const double gap = mm2px(1.2);

  QFont f = p.font();
  f.setBold(false);
  f.setPixelSize((int)std::round(dpi * 8.0 / 72.0));
  p.setFont(f);
  QFontMetrics fm(f);
  const double labelW = fm.horizontalAdvance(QStringLiteral("Page"));
  const double slashW = fm.horizontalAdvance(QStringLiteral("/"));
  const double totalW = labelW + gap + box + gap + slashW + gap + box;

  double x = right - totalW;
  const double yc = topY + bandH / 2.0;
  p.setPen(QColor(120, 120, 120));
  p.drawText(QRectF(x, topY, labelW, bandH), Qt::AlignVCenter | Qt::AlignLeft,
             QStringLiteral("Page"));
  x += labelW + gap;
  p.setBrush(Qt::NoBrush);
  p.setPen(QPen(QColor(120, 120, 120), std::max(1.0, mm2px(0.25))));
  p.drawRect(QRectF(x, yc - box / 2.0, box, box));
  x += box + gap;
  p.setPen(QColor(120, 120, 120));
  p.drawText(QRectF(x, topY, slashW, bandH), Qt::AlignVCenter | Qt::AlignHCenter,
             QStringLiteral("/"));
  x += slashW + gap;
  p.setPen(QPen(QColor(120, 120, 120), std::max(1.0, mm2px(0.25))));
  p.drawRect(QRectF(x, yc - box / 2.0, box, box));
  return right - totalW;
}

// Attribution + repo, centred in the strip BELOW the grid (between the two
// bottom markers) so it costs the drawing area nothing. Mirrors the Board's PDF
// export footer.
void drawFooter(QPainter &p, const PageFrame &f, double dpi) {
  auto mm2px = [dpi](double mm) { return mm * dpi / 25.4; };
  const double top = f.contentBottom + mm2px(1.2);
  const double bandH = f.pageH - mm2px(1.5) - top;
  if (bandH < mm2px(2.0)) return;

  QPixmap logo = QPixmap(":Resources/ztoryc_about.png")
                     .scaled((int)mm2px(3.5), (int)mm2px(3.5),
                             Qt::KeepAspectRatio, Qt::SmoothTransformation);
  // Size in DEVICE pixels (not points): QFontMetrics resolves point sizes
  // against the screen, which at 300 dpi under-measures and clips the text.
  QFont f5;
  f5.setPixelSize((int)std::round(dpi * 6.0 / 72.0));
  p.setFont(f5);
  QFontMetricsF fm(f5);
  const QString made = QStringLiteral("Made with Ztoryc");
  const QString repo = QStringLiteral("github.com/matitanimata/ztoryc");
  const QString sep  = QStringLiteral("  ·  ");
  const double gap = mm2px(1.5);
  const double mW = fm.horizontalAdvance(made), sW = fm.horizontalAdvance(sep),
               rW = fm.horizontalAdvance(repo);
  double total = mW + sW + rW + (logo.isNull() ? 0.0 : logo.width() + gap);
  double x = (f.pageW - total) / 2.0;

  if (!logo.isNull()) {
    p.drawPixmap(QPointF(x, top + (bandH - logo.height()) / 2.0), logo);
    x += logo.width() + gap;
  }
  p.setPen(QColor(160, 160, 160));
  p.drawText(QRectF(x, top, mW, bandH), Qt::AlignVCenter | Qt::AlignLeft, made);
  x += mW;
  p.drawText(QRectF(x, top, sW, bandH), Qt::AlignVCenter | Qt::AlignHCenter, sep);
  x += sW;
  // Readable text, not a clickable annotation: QPdfWriter+QPainter has no
  // simple hyperlink API (same as the Board export).
  p.setPen(QColor(120, 120, 200));
  p.drawText(QRectF(x, top, rW + mm2px(1), bandH),
             Qt::AlignVCenter | Qt::AlignLeft, repo);
}

// ── Page code ───────────────────────────────────────────────────────────────
constexpr int kCodeBytes = 9;
constexpr int kCodeBits  = kCodeBytes * 8;

std::array<unsigned char, kCodeBytes> codeBytes(const ZtoryPaperSheet::PageCode &c) {
  std::array<unsigned char, kCodeBytes> b{};
  b[0] = (unsigned char)(ZtoryPaperSheet::PageCode::kVersion & 0xFF);
  b[1] = (unsigned char)((c.sceneHash >> 8) & 0xFF);
  b[2] = (unsigned char)(c.sceneHash & 0xFF);
  b[3] = (unsigned char)(c.gridCols & 0xFF);
  b[4] = (unsigned char)(c.gridRows & 0xFF);
  b[5] = (unsigned char)std::min(255, std::max(1, (int)std::lround(c.aspect * 100.0)));
  b[6] = (unsigned char)(c.pageIndex & 0xFF);
  b[7] = (unsigned char)(c.startRow & 0xFF);
  b[8] = (unsigned char)(b[0] ^ b[1] ^ b[2] ^ b[3] ^ b[4] ^ b[5] ^ b[6] ^ b[7]);
  return b;
}

// Deterministic bit-cell stepping, shared by draw + read.
double codeGap(double cell) { return cell * 0.35; }
double codeStartAdvance(double cell) { return cell + codeGap(cell) * 2.0; }
double codeStep(double cell) { return cell + codeGap(cell); }

void drawPageCode(QPainter &p, const QPointF &origin, double cell,
                  const ZtoryPaperSheet::PageCode &code) {
  auto b = codeBytes(code);
  // Start marker: solid square.
  p.fillRect(QRectF(origin.x(), origin.y(), cell, cell), Qt::black);
  const double x0 = origin.x() + codeStartAdvance(cell);
  QPen hollow(Qt::black, std::max(1.0, cell * 0.12));
  for (int i = 0; i < kCodeBits; ++i) {
    bool bit = (b[i / 8] >> (7 - (i % 8))) & 1;
    QRectF r(x0 + i * codeStep(cell), origin.y(), cell, cell);
    if (bit) {
      p.fillRect(r, Qt::black);
    } else {
      p.setPen(hollow);
      p.setBrush(Qt::NoBrush);
      p.drawRect(r);
    }
  }
}

// Read the page code from a de-warped grayscale image at the known geometry.
// The expected position can drift by a pixel or two (marker centroids, paper
// stretch, print scaling), so a small offset grid is searched and the first
// offset whose checksum validates wins — self-verifying, no tuning needed.
bool readPageCode(const cv::Mat &gray, const PageFrame &f,
                  ZtoryPaperSheet::PageCode &out) {
  auto sample = [&](double x, double y) -> int {
    int xi = (int)std::lround(x), yi = (int)std::lround(y);
    if (xi < 1 || yi < 1 || xi >= gray.cols - 1 || yi >= gray.rows - 1) return 255;
    int s = 0;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        s += gray.at<unsigned char>(yi + dy, xi + dx);
    return s / 9;
  };
  const double cell = f.codeCell;
  const double step = codeStep(cell);

  for (int oy = -4; oy <= 4; ++oy) {
    for (int ox = -4; ox <= 4; ++ox) {
      const double yc = f.codeOrigin.y() + cell / 2.0 + oy * (cell / 4.0);
      const double x0 =
          f.codeOrigin.x() + codeStartAdvance(cell) + cell / 2.0 + ox * (cell / 4.0);
      std::vector<int> vals(kCodeBits);
      int lo = 255, hi = 0;
      for (int i = 0; i < kCodeBits; ++i) {
        vals[i] = sample(x0 + i * step, yc);
        lo = std::min(lo, vals[i]);
        hi = std::max(hi, vals[i]);
      }
      if (hi - lo < 40) continue;  // no black/white contrast here at all
      const int thr = (lo + hi) / 2;
      std::array<unsigned char, kCodeBytes> b{};
      for (int i = 0; i < kCodeBits; ++i)
        if (vals[i] < thr) b[i / 8] |= (1 << (7 - (i % 8)));

      if (b[0] != ZtoryPaperSheet::PageCode::kVersion) continue;
      unsigned char chk = 0;
      for (int i = 0; i < kCodeBytes - 1; ++i) chk ^= b[i];
      if (chk != b[kCodeBytes - 1]) continue;

      ZtoryPaperSheet::PageCode c;
      c.sceneHash = (b[1] << 8) | b[2];
      c.gridCols  = b[3];
      c.gridRows  = b[4];
      c.aspect    = b[5] / 100.0;
      c.pageIndex = b[6];
      c.startRow  = b[7];
      if (c.gridCols < 1 || c.gridRows < 1 || c.aspect < 0.1) continue;
      out = c;
      return true;
    }
  }
  return false;
}

// ── Marker detection (OpenCV, imgproc only) ─────────────────────────────────
// Returns the four marker centres in IMAGE-space clockwise order — [0] nearest
// the image's top-left, then TR, BR, BL — WITHOUT deciding which one is the
// sheet's origin. Orientation is resolved later by trying all four rotations
// and keeping the one whose page-code checksum validates: far more robust than
// relying on a distinctive marker, which detection can easily mis-rank.
bool detectMarkerQuad(const cv::Mat &gray, cv::Point2f out[4]) {
  cv::Mat bin;
  cv::adaptiveThreshold(gray, bin, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                        cv::THRESH_BINARY_INV, 41, 6);
  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(bin, contours, hierarchy, cv::RETR_TREE,
                   cv::CHAIN_APPROX_SIMPLE);
  if (contours.empty()) return false;

  const double imgDiag = std::hypot(gray.cols, gray.rows);
  const double minArea = std::pow(imgDiag * 0.004, 2.0);  // ignore tiny specks
  // A marker is ~8 mm on A4: anything much larger is a drawing blob, not a
  // finder pattern (hatched sketches do produce big nested quads).
  const double maxArea = std::pow(imgDiag * 0.08, 2.0);

  struct Quad { cv::Point2f c; double area; };
  std::vector<Quad> quads;
  for (const auto &c : contours) {
    double area = cv::contourArea(c);
    if (area < minArea || area > maxArea) continue;
    std::vector<cv::Point> ap;
    cv::approxPolyDP(c, ap, 0.05 * cv::arcLength(c, true), true);
    if (ap.size() != 4 || !cv::isContourConvex(ap)) continue;
    cv::Moments m = cv::moments(c);
    if (m.m00 == 0) continue;
    quads.push_back({cv::Point2f((float)(m.m10 / m.m00), (float)(m.m01 / m.m00)),
                     area});
  }
  if (quads.size() < 3) return false;

  // Cluster concentric quads: nested squares share a centre, distinct markers
  // sit in far-apart corners, so a small radius separates them cleanly.
  const double mergeR = imgDiag * 0.02;
  struct Cluster { cv::Point2f sum; int count; double maxArea; };
  std::vector<Cluster> clusters;
  for (const auto &q : quads) {
    int hit = -1;
    for (int k = 0; k < (int)clusters.size(); ++k) {
      cv::Point2f cen = clusters[k].sum / (float)clusters[k].count;
      if (cv::norm(cen - q.c) < mergeR) { hit = k; break; }
    }
    if (hit < 0)
      clusters.push_back({q.c, 1, q.area});
    else {
      clusters[hit].sum += q.c;
      clusters[hit].count++;
      clusters[hit].maxArea = std::max(clusters[hit].maxArea, q.area);
    }
  }

  // Candidate markers = clusters of >= 3 concentric quads. The printed page-code
  // squares also nest and sit close together, so they can form such clusters —
  // picking by CORNER PROXIMITY (below) rejects them, since the real markers are
  // always the outermost things on the sheet.
  std::vector<cv::Point2f> cand;
  for (const auto &c : clusters)
    if (c.count >= 3) cand.push_back(c.sum / (float)c.count);
  if (cand.size() < 4) return false;

  // One marker per image corner, clockwise from the image's top-left.
  const cv::Point2f corners[4] = {
      {0.f, 0.f},
      {(float)gray.cols, 0.f},
      {(float)gray.cols, (float)gray.rows},
      {0.f, (float)gray.rows}};
  int pick[4] = {-1, -1, -1, -1};
  for (int k = 0; k < 4; ++k) {
    double best = -1;
    for (int i = 0; i < (int)cand.size(); ++i) {
      double d = cv::norm(cand[i] - corners[k]);
      if (best < 0 || d < best) { best = d; pick[k] = i; }
    }
  }
  for (int a = 0; a < 4; ++a)
    for (int b = a + 1; b < 4; ++b)
      if (pick[a] == pick[b]) return false;  // degenerate: not four corners

  for (int k = 0; k < 4; ++k) out[k] = cand[pick[k]];

  // Sanity: the four markers must enclose most of the frame, or we latched onto
  // something that is not the sheet.
  std::vector<cv::Point2f> quad(out, out + 4);
  const double quadArea = std::fabs(cv::contourArea(quad));
  if (quadArea < 0.15 * gray.cols * gray.rows) return false;
  return true;
}

// QImage → owning BGR cv::Mat.
cv::Mat qimageToBgr(const QImage &photo) {
  QImage im = photo.convertToFormat(QImage::Format_RGB888);
  cv::Mat rgb(im.height(), im.width(), CV_8UC3, (void *)im.constBits(),
             (size_t)im.bytesPerLine());
  cv::Mat bgr;
  cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
  return bgr;
}

}  // namespace

namespace ZtoryPaperSheet {

bool printSheet(const QString &pdfPath, const SheetParams &params) {
  const int gridCols  = std::max(1, params.gridCols);
  const int gridRows  = std::max(1, params.gridRows);
  const double aspect = params.cameraAspect > 0.01 ? params.cameraAspect
                                                    : 16.0 / 9.0;

  QPdfWriter writer(pdfPath);
  writer.setPageLayout(QPageLayout(
      QPageSize(QPageSize::A4),
      aspect >= 1.0 ? QPageLayout::Landscape : QPageLayout::Portrait,
      QMarginsF(0, 0, 0, 0)));
  writer.setResolution(300);

  QPainter painter(&writer);
  if (!painter.isActive()) return false;
  painter.setRenderHint(QPainter::Antialiasing, true);

  const double dpi = writer.resolution();
  const PageFrame f = computePageFrame(writer.width(), writer.height(), dpi);

  // A BLANK sheet is a template to photocopy, so it is always exactly ONE page
  // holding a full page's worth of rows — independent of how many rows the room
  // grid happens to have. A sheet printed WITH the thumbnails, instead, has to
  // cover them all, so it paginates over the real row count.
  const int pageCapacityRows =
      computeGridFit(f, gridCols, 1 << 20, aspect).perRows;
  const int effectiveRows =
      params.content.isNull() ? pageCapacityRows : gridRows;

  const GridFit g   = computeGridFit(f, gridCols, effectiveRows, aspect);
  const int sceneHash = (int)(qHash(params.sceneName) & 0xFFFF);

  auto pt2px = [dpi](double pt) { return pt * dpi / 72.0; };
  QFont titleFont;
  titleFont.setPixelSize((int)std::round(pt2px(12.0)));
  titleFont.setBold(true);
  QFont subFont;
  subFont.setPixelSize((int)std::round(pt2px(8.0)));

  for (int pageIndex = 0; pageIndex < g.totalPages; ++pageIndex) {
    if (pageIndex > 0) writer.newPage();
    const int startRow     = pageIndex * g.perRows;
    const int rowsThisPage = std::min(g.perRows, effectiveRows - startRow);

    drawMarker(painter, f.mc[0].x(), f.mc[0].y(), f.mSzTL, 3);  // TL = larger
    drawMarker(painter, f.mc[1].x(), f.mc[1].y(), f.mSz, 3);
    drawMarker(painter, f.mc[2].x(), f.mc[2].y(), f.mSz, 3);
    drawMarker(painter, f.mc[3].x(), f.mc[3].y(), f.mSz, 3);

    painter.setPen(Qt::black);
    painter.setFont(titleFont);
    QString title = params.sceneName.isEmpty() ? QStringLiteral("Ztoryc sheet")
                                               : params.sceneName;
    painter.drawText(QRectF(f.headerLeft, f.mIn, f.headerW * 0.6, f.titleBandH),
                     Qt::AlignVCenter | Qt::AlignLeft, title);
    // Blank boxes to number the sheet by hand (see drawPageBoxes). Kept a few
    // mm clear of the top-right marker so nothing crowds its finder pattern.
    drawPageBoxes(painter, f.headerLeft + f.headerW - dpi * 3.0 / 25.4, f.mIn,
                  f.titleBandH, dpi);

    PageCode code;
    code.sceneHash = sceneHash;
    code.gridCols  = gridCols;
    code.gridRows  = effectiveRows;
    code.aspect    = aspect;
    code.pageIndex = pageIndex;
    code.startRow  = startRow;
    drawPageCode(painter, f.codeOrigin, f.codeCell, code);

    const QPointF org = gridOrigin(f, g, gridCols, effectiveRows, pageIndex);
    const double gridW = gridCols * g.cellW;
    const double gridH = rowsThisPage * g.cellH;

    // Contact-sheet mode: blit this page's slice of the canvas into the cells
    // before the grid lines, so the printed rules stay visible on top.
    if (!params.content.isNull()) {
      const double srcCellW = (double)params.content.width() / gridCols;
      const double srcCellH = (double)params.content.height() / gridRows;
      for (int r = 0; r < rowsThisPage; ++r) {
        QRectF srcRect(0.0, (startRow + r) * srcCellH,
                       gridCols * srcCellW, srcCellH);
        QRectF dstRect(org.x(), org.y() + r * g.cellH, gridW, g.cellH);
        painter.drawImage(dstRect, params.content, srcRect);
      }
    }

    painter.setPen(QPen(kCyanFrame, std::max(1.0, dpi * 0.35 / 25.4)));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(org.x(), org.y(), gridW, gridH));
    for (int c = 1; c < gridCols; ++c)
      painter.drawLine(QPointF(org.x() + c * g.cellW, org.y()),
                       QPointF(org.x() + c * g.cellW, org.y() + gridH));
    for (int r = 1; r < rowsThisPage; ++r)
      painter.drawLine(QPointF(org.x(), org.y() + r * g.cellH),
                       QPointF(org.x() + gridW, org.y() + r * g.cellH));

    drawFooter(painter, f, dpi);
  }

  painter.end();
  return true;
}

bool findSheetCorners(const QImage &photo, QPolygonF &corners) {
  if (photo.isNull()) return false;
  // Run on a reduced copy so a live preview can afford this every frame, but
  // NOT too reduced: measured on a hard shot (dim light, sheet small in frame,
  // one marker half covered) only 3 of the 4 markers survive at 640-1000 px,
  // while all 4 are found from 1280 up. At 1280 the pass costs ~4 ms, nothing
  // against a 40 ms frame, so that is the floor worth keeping.
  const int kWork = 1280;
  QImage small = photo.width() > kWork
                     ? photo.scaledToWidth(kWork, Qt::FastTransformation)
                     : photo;
  const double scale = (double)photo.width() / std::max(1, small.width());

  cv::Mat bgr = qimageToBgr(small);
  cv::Mat gray;
  cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
  cv::Point2f quad[4];
  if (!detectMarkerQuad(gray, quad)) return false;

  corners.clear();
  for (int i = 0; i < 4; ++i)
    corners << QPointF(quad[i].x * scale, quad[i].y * scale);
  return true;
}

ImportResult importSheet(const QImage &photo) {
  ImportResult res;
  if (photo.isNull()) {
    res.error = "Empty image.";
    return res;
  }

  cv::Mat bgr = qimageToBgr(photo);
  cv::Mat gray;
  cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

  cv::Point2f quad[4];  // image-space clockwise from the image's top-left
  if (!detectMarkerQuad(gray, quad)) {
    res.error = "Could not find the four registration markers. Make sure the "
                "whole sheet is in frame, well lit and reasonably flat.";
    return res;
  }

  // Try every rotation of the marker quad: the sheet may have been shot upside
  // down or sideways. The page code's checksum tells us which one is right, so
  // orientation never depends on recognising a special marker.
  const double dpi = 200.0;
  PageFrame f;
  PageCode code;
  cv::Mat warped;
  bool solved = false;

  for (int k = 0; k < 4 && !solved; ++k) {
    cv::Point2f src[4] = {quad[k], quad[(k + 1) % 4], quad[(k + 3) % 4],
                          quad[(k + 2) % 4]};  // TL, TR, BL, BR
    const double wTop  = cv::norm(src[1] - src[0]);
    const double hLeft = cv::norm(src[2] - src[0]);
    const bool landscape = wTop >= hLeft;
    const double canonW =
        (landscape ? kA4Long : kA4Short) * dpi / 25.4;
    const double canonH =
        (landscape ? kA4Short : kA4Long) * dpi / 25.4;

    PageFrame cf = computePageFrame(canonW, canonH, dpi);
    cv::Point2f dst[4] = {
        cv::Point2f((float)cf.mc[0].x(), (float)cf.mc[0].y()),
        cv::Point2f((float)cf.mc[1].x(), (float)cf.mc[1].y()),
        cv::Point2f((float)cf.mc[2].x(), (float)cf.mc[2].y()),
        cv::Point2f((float)cf.mc[3].x(), (float)cf.mc[3].y())};
    cv::Mat M = cv::getPerspectiveTransform(src, dst);
    cv::Mat cand;
    cv::warpPerspective(bgr, cand, M, cv::Size((int)canonW, (int)canonH),
                        cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                        cv::Scalar(255, 255, 255));
    cv::Mat cgray;
    cv::cvtColor(cand, cgray, cv::COLOR_BGR2GRAY);

    PageCode c;
    if (readPageCode(cgray, cf, c)) {
      f      = cf;
      code   = c;
      warped = cand;
      solved = true;
    }
  }

  if (!solved) {
    res.error = "The page code in the sheet header could not be read. Make sure "
                "the whole page — including the row of small squares next to the "
                "title — is in frame, in focus and evenly lit.";
    return res;
  }

  const double canonW = f.pageW;

  // BLUE channel + background division.
  //
  // What actually removes the printed rules is the INSET CROP further down —
  // measured on a real sheet printed in black and white, where the cyan turns
  // into a neutral grey 201 that no channel choice can lighten. The channel only
  // helps on a COLOUR print, where a cyan rule reads 235-249 in blue (already
  // white) against 150-224 in red: there it erases whatever rule the crop does
  // not catch (a stroke drawn across a rule, a slightly-off warp). Graphite is
  // neutral, so it stays dark in every channel either way. Red was simply the
  // wrong pick — it made the rules darker.
  std::vector<cv::Mat> ch;
  cv::split(warped, ch);  // OpenCV channel order is B, G, R
  cv::Mat blue = ch[0];
  cv::Mat bg;
  cv::GaussianBlur(blue, bg, cv::Size(0, 0), canonW * 0.03);
  cv::Mat norm;
  cv::divide(blue, bg, norm, 255.0, CV_8U);
  // Keep the pre-lift image: the white-point lift below erases the very marks
  // the "faint" check is meant to notice, so that check has to measure here.
  cv::Mat preLift = norm.clone();
  // Lift the white point so paper noise and any residual rule saturate to pure
  // white while the darker pencil greys survive (no hard binarisation).
  norm.convertTo(norm, CV_8U, 255.0 / 238.0);

  const GridFit g = computeGridFit(f, code.gridCols, code.gridRows, code.aspect);
  const QPointF org = gridOrigin(f, g, code.gridCols, code.gridRows, code.pageIndex);
  const int rowsThisPage =
      std::min(g.perRows, code.gridRows - code.startRow);

  res.ok        = true;
  res.sceneHash = code.sceneHash;
  res.gridCols  = code.gridCols;
  res.gridRows  = code.gridRows;
  res.pageIndex = code.pageIndex;
  res.startRow  = code.startRow;

  // Crop each cell slightly INSIDE its rectangle so the printed rule (whose
  // exact position we know) is excluded from the drawing. This — not the colour
  // channel — is what keeps the grid out of the imported panels, and it is why
  // a black-and-white print works just as well. The cropped content is then
  // stretched back to the full box: measured on a stroke spanning four cells,
  // the resulting misalignment at the seams is 1-2 px out of 287, so panoramas
  // drawn across cells stay visually continuous.
  const int inset = std::max(3, (int)std::lround(g.cellW * 0.02));
  for (int r = 0; r < rowsThisPage; ++r) {
    for (int c = 0; c < code.gridCols; ++c) {
      int x0 = (int)std::lround(org.x() + c * g.cellW) + inset;
      int y0 = (int)std::lround(org.y() + r * g.cellH) + inset;
      int w  = (int)std::lround(g.cellW) - 2 * inset;
      int h  = (int)std::lround(g.cellH) - 2 * inset;
      cv::Rect roi(x0, y0, w, h);
      roi &= cv::Rect(0, 0, norm.cols, norm.rows);
      if (roi.width < 2 || roi.height < 2) continue;

      cv::Mat cell = norm(roi).clone();
      // Empty = almost no dark pixels (robust to JPEG noise / a stray speck).
      const int dark  = cv::countNonZero(cell < 235);
      const bool empty = dark < (int)(roi.area() * 0.001);
      // A cell with no "dark" pixels can still be covered in very light marks:
      // that is a real drawing pressed too lightly, not a blank box. Measured on
      // the PRE-LIFT image, since the white-point lift is exactly what wipes
      // those marks out — so the caller can warn instead of dropping it silently.
      const int light  = cv::countNonZero(preLift(roi) < 250);
      const bool faint = empty && light > (int)(roi.area() * 0.01);

      cv::Mat rgb;
      cv::cvtColor(cell, rgb, cv::COLOR_GRAY2RGB);
      QImage qi((const uchar *)rgb.data, rgb.cols, rgb.rows, (int)rgb.step,
                QImage::Format_RGB888);

      ImportedCell ic;
      ic.gridRow = code.startRow + r;
      ic.gridCol = c;
      ic.image   = qi.copy();   // own the pixels
      ic.empty   = empty;
      ic.faint   = faint;
      res.cells.push_back(ic);
    }
  }
  return res;
}

}  // namespace ZtoryPaperSheet
