#pragma once

#ifndef FULLCOLORBRUSHTOOL_H
#define FULLCOLORBRUSHTOOL_H

#include <ctime>

#include "toonzrasterbrushtool.h"
#include "assistantstrokesnap.h"
#include "replicamap.h"
#include "mypainttoonzbrush.h"
#include "toonz/mypaintbrushstyle.h"
#include <QElapsedTimer>

//==============================================================

//  Forward declarations

class TTileSetFullColor;
class TTileSaverFullColor;
class MyPaintToonzBrush;
class FullColorBrushToolNotifier;
namespace mypaint {
class Brush;
}

//==============================================================

//************************************************************************
//    FullColor Brush Tool declaration
//************************************************************************

class FullColorBrushTool final : public TTool, public RasterController, public BrushPresetClient {
  Q_DECLARE_TR_FUNCTIONS(FullColorBrushTool)

  void updateCurrentStyle();
  double restartBrushTimer();
  void applyToonzBrushSettings(mypaint::Brush &mypaintBrush);

public:
  FullColorBrushTool(std::string name);

  ToolType getToolType() const override { return TTool::LevelWriteTool; }
  // The guidelines through the pointer are drawn by the brush itself, with
  // a whole-viewer refresh (see ToonzRasterBrushTool).
  unsigned int getToolHints() const override {
    return GuideSnap::hints(m_guideSnap,
                            TTool::getToolHints() & ~HintAssistantsGuidelines);
  }
  void paintDragPoint(const TPointD &usePos, const TPointD &pos,
                      const TMouseEvent &e, TRectD invalidateRect);
  void paintPendingAssistedPoints(const TPointD &pos, const TMouseEvent &e);

  ToolOptionsBox *createOptionsBox() override;

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  bool askRead(const TRect &rect) override;
  bool askWrite(const TRect &rect) override;

  bool preLeftButtonDown() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override;

  void onEnter() override;
  void onLeave() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override;
  bool onPropertyChanged(std::string propertyName) override;

  void onImageChanged() override;
  void setWorkAndBackupImages();
  void updateWorkAndBackupRasters(const TRect &rect);

  void initPresets();
  void loadPreset();
  void addPreset(QString name) override;
  void removePreset();

  void loadLastBrush();

  // Ztoryc: each style remembers the brush it was drawn with
  // (BrushStyleMemory); these read and write the settings apart from presets.
  void loadPresetData(const BrushData &preset);
  BrushData currentBrushData(const std::wstring &name);
  void storeStyleBrush() override;

  // BrushPresetClient
  std::string brushKind() const override { return "raster"; }
  QString brushName() const override { return tr("Raster Brush"); }
  QStringList presetNames() override;
  QString currentPresetName() override;
  void selectPreset(const QString &name) override;
  void removeNamedPreset(const QString &name) override;
  std::shared_ptr<TPersist> presetData(const QString &name) override;
  void applyStyleBrush(const TPersist *data) override;

  void onCanvasSizeChanged();
  void onColorStyleChanged();

  TMyPaintBrushStyle *getBrushStyle();

  // Auto Fill "Fill Style:" — rebuild the enum from the palette (safe any
  // time: emits no Qt signals).
  void rebuildAutoFillStyleCombo(TPalette *pal);

private:
  // Fills the regions closed by the stroke just drawn (Auto Fill), through the
  // stock full-colour fill. Adds its own undo; call inside the stroke's block.
  void applyAutoFill(const TRasterImageP &ri, TXshSimpleLevel *sl,
                     const TFrameId &fid, const TRect &strokeBox);

protected:
  TPropertyGroup m_prop;

  TDoublePairProperty m_thickness;
  TDoubleProperty m_smooth;
  TBoolProperty m_mypaintPressure;
  TDoublePairProperty m_opacity;
  TDoubleProperty m_hardness;
  TDoubleProperty m_modifierSize;
  TDoubleProperty m_modifierOpacity;
  TBoolProperty m_modifierEraser;
  TBoolProperty m_modifierLockAlpha;
  TBoolProperty m_modifierPaintBehind;
  TEnumProperty m_preset;
  TBoolProperty m_snapGrid;
  TBoolProperty m_assistants;
  TEnumProperty m_guideSnap;
  TBoolProperty m_mypaintTilt;
  TBoolProperty m_autoFill;
  // Auto Fill "Fill Depth": the Fill tool's FullColorMinFillDepth, shown here
  // too (grainy MyPaint lines need it lower to count as closed).
  TIntProperty m_autoFillDepth;
  TEnumProperty m_autoFillStyle;  // see ToolUtils::resolveAutoFillStyle
  TStylusProperty m_sizeStylusProperty, m_opacityStylusProperty;
  TBrushTipProperty m_brushTip;

  TPixel32 m_currentColor;
  bool m_enabledPressure, m_enabledOPressure;
  bool m_enabledTilt, m_enabledOTilt;
  double m_minCursorThick, m_maxCursorThick;
  bool m_isMyPaintStyleSelected;

  TPointD m_mousePos,    //!< Current mouse position, in world coordinates.
      m_brushPos,        //!< World position the brush will be painted at.
      m_windowMousePos;  //!< Current mouse position, in window coordinates.

  TRasterP m_backUpRas;
  TRaster32P m_workRaster;

  TRect m_strokeRect, m_strokeSegmentRect, m_lastRect;

  SmoothStroke m_smoothStroke;
  AssistantStrokeSnap m_assistantSnap;
  size_t m_assistPainted = 0;  // raw points of m_assistantSnap already painted

  MyPaintToonzBrush *m_toonz_brush;
  QElapsedTimer m_brushTimer;
  double m_highFreqBrushTimer;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  BrushPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance
  FullColorBrushToolNotifier *m_notifier;

  bool m_presetsLoaded;
  bool m_firstTime;
  bool m_mousePressed = false;
  TMouseEvent m_mouseEvent;

  bool m_isStraight = false;
  TPointD m_firstPoint;
  TPointD m_lastPoint;
  double m_oldPressure = -1.0;
  double m_tiltAngle   = 0;

  bool m_propertyUpdating = false;

  int m_perspectiveIndex = -1;

  double m_minThick, m_maxThick;
  double m_oldOpacity;
  std::vector<TThickPoint> m_points;
  RasterBlurredBrush *m_bluredBrush;
  QRadialGradient m_brushPad;

  bool m_enabled;
  
  bool m_active;
};

//------------------------------------------------------------

class FullColorBrushToolNotifier final : public QObject {
  Q_OBJECT

  FullColorBrushTool *m_tool;

public:
  FullColorBrushToolNotifier(FullColorBrushTool *tool);

protected slots:
  void onCanvasSizeChanged() { m_tool->onCanvasSizeChanged(); }
  void onColorStyleChanged() { m_tool->onColorStyleChanged(); }
};

#endif  // FULLCOLORBRUSHTOOL_H
