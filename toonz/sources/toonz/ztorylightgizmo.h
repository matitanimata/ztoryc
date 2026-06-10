#pragma once

// Light-direction gizmo drawing (task 40 FASE 3) — shared between the Board
// thumbnails/PDF (storyboardpanel.cpp, where the implementation lives) and the
// Shot Board large preview (ztoryanimatic.cpp).

#include <QPixmap>

class QPainter;
class QColor;
struct PanelData;
class TXsheet;

// Draws the 3D conic arrow over the (W, H) logical-pixel target.
// Coordinates are normalized 0-1; `depth` -1..+1 (background..camera);
// `spreadDeg` = full beam opening angle; `editing` adds the sun glyph,
// the translucent beam and the angle/depth/spread readout (drag feedback).
void ztoryDrawLightGizmo(QPainter &p, double W, double H, double tailX,
                         double tailY, double tipX, double tipY, double depth,
                         double spreadDeg, const QColor &color,
                         bool editing = false);

// Bakes the gizmo onto a thumbnail/preview pixmap when pd.hasLight.
// The pixmap's devicePixelRatio tag is honoured (drawing in logical coords).
void ztoryApplyLightOverlay(QPixmap &px, const PanelData &pd);

// Renders panel `pd` of `subXsh` at (physW × physH) with the full camera-move
// treatment (task 40 FASE 2): backed-out framing covering START and STOP and
// the classic rectangles/arrows/letters overlay.  `moveOrdinal` = how many
// camera-move panels precede this one in the shot (A→B lettering);
// `showCamLabel` toggles the "Trk In/Pan…" type label.  Shared by the Board
// thumbnails and the Shot Board large preview so the two stay identical.
QPixmap ztoryRenderPanelPreview(TXsheet *subXsh, const PanelData &pd,
                                int physW, int physH, int moveOrdinal,
                                bool showCamLabel, double labelPxSize = 0);
