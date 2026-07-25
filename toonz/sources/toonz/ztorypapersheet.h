#pragma once

// ZtoryPaperSheet — "import from paper" for the Thumbnail room (task 63).
//
// Round trip: printSheet() renders the room's grid to an A4 PDF the artist draws
// on by hand; importSheet() takes a photo/scan of that sheet and returns each
// cell's drawing, de-warped and cropped, ready to blit back into the canvas.
//
// The sheet is SELF-DESCRIBING: a machine page code (printed as a row of squares)
// carries the grid shape (cols/rows/aspect) and the page's position, so import
// needs no prior knowledge of how the sheet was printed. Four concentric-square
// registration markers (finder-pattern style, located with cv::findContours +
// hierarchy — no cv::aruco, which the CI can't build) give the four sheet corners
// for the perspective warp. Orientation is NOT read off a special marker (too
// easy to mis-rank): all four rotations are warped and the one whose page-code
// checksum validates wins. Box frames are printed light CYAN so a red-channel
// read on import drops them and leaves only the black pencil.

#include <QString>
#include <QImage>
#include <QPolygonF>
#include <QVector>

class ToonzScene;

namespace ZtoryPaperSheet {

// Machine-readable page code (printed as a row of squares, MSB-first, framed by
// a solid start square). Byte layout — keep stable or bump kVersion:
//   [version][hashHi][hashLo][gridCols][gridRows][aspect*100][pageIndex]
//   [startRow][checksum = XOR of the preceding bytes].
// startCol is always 0 (columns are never split across pages), so it is omitted.
// Bump kVersion if the layout ever changes after release, so a sheet printed by
// an older build is rejected instead of silently misread.
struct PageCode {
  static constexpr int kVersion = 1;
  int sceneHash  = 0;   // 16-bit qHash of the scene name (identity / mismatch warn)
  int gridCols   = 4;
  int gridRows   = 4;
  double aspect  = 16.0 / 9.0;  // camera aspect (box width / height)
  int pageIndex  = 0;   // 0-based page within the print job
  int startRow   = 0;   // grid row of this page's top-left box (0-based)
};

// Parameters describing the canvas grid to print. Derived by the panel from the
// canvas geometry and the scene camera.
struct SheetParams {
  int gridCols        = 4;
  int gridRows        = 4;
  double cameraAspect = 16.0 / 9.0;  // ZtoryShotOps::cameraAspect
  QString sceneName;                 // printed in the header + hashed into the code
  // Optional: the room's whole contiguous canvas (top-down), spanning
  // gridCols × gridRows cells. When set, each cell is printed with its drawing
  // instead of blank — a contact sheet of what is on the canvas. Panoramas stay
  // seamless because both grids are contiguous. Leave null for the blank sheet
  // meant to be drawn on (and photocopied).
  QImage content;
};

// One imported cell: its grid position and its upright, cropped drawing.
struct ImportedCell {
  int gridRow = 0;
  int gridCol = 0;
  QImage image;        // upright (top-down), background-normalized, grayscale-RGB
  bool empty = true;   // below the ink threshold → caller should skip it
  // Empty, yet carrying a fair amount of very light marks: most likely a sketch
  // drawn too faintly to clear the threshold. Worth telling the user about,
  // because such a cell is dropped silently and the drawing would just be gone.
  bool faint = false;
};

// Outcome of importing one photographed sheet.
struct ImportResult {
  bool ok = false;
  QString error;
  int sceneHash = 0;
  int gridCols  = 0;
  int gridRows  = 0;
  int pageIndex = 0;
  int startRow  = 0;
  QVector<ImportedCell> cells;   // only the cells on this page
};

// Render the whole grid to a multi-page A4 PDF at pdfPath. Returns false if the
// file could not be written.
bool printSheet(const QString &pdfPath, const SheetParams &params);

// De-warp and crop a photographed sheet. `photo` is any Qt-loadable image
// (JPG/PNG); the four markers and the page code do the rest. On success returns
// ok = true with the page's cells; on failure ok = false and `error` set.
ImportResult importSheet(const QImage &photo);

// Locate just the four registration markers, clockwise from the image's
// top-left, without de-warping or decoding anything. Cheap enough to run on a
// live camera preview: the capture dialog uses it to tell the user whether the
// sheet is framed before they shoot.
bool findSheetCorners(const QImage &photo, QPolygonF &corners);

}  // namespace ZtoryPaperSheet
