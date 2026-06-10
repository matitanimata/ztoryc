#pragma once

// Light-direction gizmo drawing (task 40 FASE 3) — shared between the Board
// thumbnails/PDF (storyboardpanel.cpp, where the implementation lives) and the
// Shot Board large preview (ztoryanimatic.cpp).

class QPainter;
class QPixmap;
class QColor;
struct PanelData;

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
