#pragma once

#ifndef FUNCTIONPANEL_H
#define FUNCTIONPANEL_H

#include "tcommon.h"
#include "functiontreeviewer.h"

#include "toonz/txsheethandle.h"

#include <QDialog>
#include <set>
#include <cmath>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class TDoubleParam;
class TDoubleKeyframe;
class TFrameHandle;
class TObjectHandle;
class FunctionSelection;
class FunctionSheet;

//-----------------------------------------------------------------------------

//! Channel edit panel (numcols + graph popup)
class FunctionPanel final : public QDialog {
  Q_OBJECT

  QColor m_bgColor;
  QColor m_valueLineColor;
  QColor m_frameLineColor;
  QColor m_otherCurvesColor;
  QColor m_rulerBackground;

  Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)
  Q_PROPERTY(
      QColor ValueLineColor READ getValueLineColor WRITE setValueLineColor)
  Q_PROPERTY(
      QColor FrameLineColor READ getFrameLineColor WRITE setFrameLineColor)
  Q_PROPERTY(QColor OtherCurvesColor READ getOtherCurvesColor WRITE
                 setOtherCurvesColor)
  Q_PROPERTY(
      QColor RulerBackground READ getRulerBackground WRITE setRulerBackground)

  QColor m_textColor;      // black
  QColor m_subColor;       // white
  QColor m_selectedColor;  // blue
  Q_PROPERTY(QColor TextColor READ getTextColor WRITE setTextColor)
  Q_PROPERTY(QColor SubColor READ getSubColor WRITE setSubColor)
  Q_PROPERTY(QColor SelectedColor READ getSelectedColor WRITE setSelectedColor)

public:
  class DragTool;
  enum Handle {
    None,
    Curve,
    Point,
    SpeedIn,
    SpeedOut,
    EaseIn,
    EaseOut,
    EaseInPercentage,
    EaseOutPercentage
  };

private:
  QTransform m_viewTransform;  // world -> window
  int m_valueAxisX, m_frameAxisY, m_graphViewportY;
  QPoint m_origin;              // axes origin (window coords)
  QPoint m_startPos, m_oldPos;  // mouse click position, last mouse click/drag
                                // position (window coords)
  bool m_isFloating   = true;
  bool m_panningArmed = false;
  struct Gadget {
    Handle m_handle;
    int m_kIndex;
    QRect m_hitRegion;
    QPointF m_pos, m_pointPos;
    double m_keyframePosition;
    FunctionTreeModel::Channel *m_channel;
    Gadget(Handle handle, int kIndex, const QPointF &p, int rx, int ry,
           const QPointF &pointPos = QPointF());
  };
  QList<Gadget> m_gadgets;

  DragTool *m_dragTool;
  FunctionSelection *m_selection;
  TFrameHandle *m_frameHandle;
  TXsheetHandle *m_xsheetHandle;
  TObjectHandle *m_objectHandle;
  FunctionTreeModel *m_functionTreeModel;
  FunctionSheet *m_sheet;

  int m_currentFrameStatus;

  struct Highlighted {
    Handle handle;
    int gIndex;
  } m_highlighted;

  struct {
    bool visible;
    double frame, value;
  } m_cursor;

  struct {
    QPoint curvePos, labelPos;
    std::string text;
    TDoubleParam *curve;
  } m_curveLabel;

  enum CURVE_SHAPE {
    SMOOTH = 0,
    FRAME_BASED  // curves with the connected polylines of integer frames
  } m_curveShape;

  //! The read-only speed graph: a strip along the bottom showing the DERIVATIVE
  //! of the curves, sharing the time axis with the value graph above it.
  //!
  //! Spacing errors live in the speed, not in the value: a curve can pass
  //! through every key correctly and still lurch between them, and that is
  //! invisible in the value plot. Read-only on purpose -- moving a point in
  //! speed space has to be integrated back into value space, which is a
  //! different project; reading it is where nearly all the benefit is.
  bool m_speedGraphVisible;
  int m_speedGraphHeight;

public:
  FunctionPanel(QWidget *parent, bool isFloating = true);
  ~FunctionPanel();

  void setModel(FunctionTreeModel *model) { m_functionTreeModel = model; };
  FunctionTreeModel *getModel() const { return m_functionTreeModel; }

  FunctionSelection *getSelection() const { return m_selection; }
  void setSelection(FunctionSelection *selection) {
    m_selection = selection;
  }  // does NOT get ownership

  void setFrameHandle(TFrameHandle *frameHandle);
  TFrameHandle *getFrameHandle() const { return m_frameHandle; }
  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  TXsheetHandle *getXsheetHandle() const { return m_xsheetHandle; }
  void setObjectHandle(TObjectHandle* objectHandle) {
    m_objectHandle = objectHandle;
  }
  TObjectHandle *getObjectHandle() const { return m_objectHandle; }

  void setFunctionSheet(FunctionSheet *sheet) { m_sheet = sheet; }
  FunctionSheet *getFunctionSheet() const { return m_sheet; }

  QTransform getViewTransform() const { return m_viewTransform; }
  void setViewTransform(const QTransform &viewTransform) {
    m_viewTransform = viewTransform;
  }

  // frame pixel size / value pixel size
  double getPixelRatio(TDoubleParam *curve) const;

  double xToFrame(double x) const;
  double frameToX(double frame) const;

  // note: the y-unit depends on the unit (e.g. degrees, inches,..) and
  // therefore on the curve
  double valueToY(TDoubleParam *curve, double value) const;
  double yToValue(TDoubleParam *curve, double y) const;

  void pan(int dx, int dy);
  void pan(const QPoint &delta) { pan(delta.x(), delta.y()); }

  void zoom(double sx, double sy, const QPoint &center);
  void fitSelectedPoints();
  void fitCurve();
  void fitGraphToWindow(bool currentCurveOnly = false);
  void fitRegion(double f0, double v0, double f1, double v1);

  QPointF getWinPos(TDoubleParam *curve, double frame, double value) const;
  QPointF getWinPos(TDoubleParam *curve, const TPointD &frameValuePos) const {
    return getWinPos(curve, frameValuePos.x, frameValuePos.y);
  }
  QPointF getWinPos(TDoubleParam *curve, double frame) const;
  QPointF getWinPos(TDoubleParam *curve, const TDoubleKeyframe &kf) const;

  int getCurveDistance(TDoubleParam *curve, const QPoint &winPos);
  TDoubleParam *findClosestCurve(const QPoint &winPos, int maxWinDistance);
  FunctionTreeModel::Channel *findClosestChannel(const QPoint &winPos,
                                                 int maxWinDistance);

  // returns the keyframe index (-1 if no keyframe found)
  // int findClosestKeyframe(TDoubleParam* curve, const QPoint &winPos, Handle
  // &handle, int maxWinDistance);

  int findClosestGadget(const QPoint &winPos, Handle &handle,
                        int maxWinDistance);

  //! The active channel having a KEYFRAME under \p winPos, nearest first, or
  //! null. Distinct from findClosestChannel, which answers "whose line passes
  //! closest": when curves cross, the line nearest the cursor is often not the
  //! one whose keyframe was aimed at.
  FunctionTreeModel::Channel *findChannelWithKeyframeAt(const QPoint &winPos,
                                                        int maxWinDistance);

  //! Dash pattern for the column \p channel belongs to; empty -- a solid line
  //! -- for the CURRENT column, whose curves are all drawn solid so they read
  //! as one group. Colour tells channels apart, the dash tells the other
  //! columns apart, weight marks the current curve.
  QVector<qreal> columnDashPattern(FunctionTreeModel::Channel *channel) const;

  // creates a QPainterPath representing a curve segment, limited in [x0,x1]
  // segmentIndex = -1 => ]-inf,first keyframe]
  // segmentIndex = segmentCount => [last keyframe, inf[
  QPainterPath getSegmentPainterPath(TDoubleParam *curve, int segmentIndex,
                                     int x0, int x1);

  TDoubleParam *getCurrentCurve() const;

  void emitKeyframeSelected(double frame) { emit keyframeSelected(frame); }

signals:
  //! What the modifiers do where the pointer is; empty when the pointer
  //! leaves. Connected to the hint bar by the application layer.
  void hintChanged(const QString &hint);

public:

  void setBGColor(const QColor &color) { m_bgColor = color; }
  QColor getBGColor() const { return m_bgColor; }
  void setValueLineColor(const QColor &color) { m_valueLineColor = color; }
  QColor getValueLineColor() const { return m_valueLineColor; }
  void setFrameLineColor(const QColor &color) { m_frameLineColor = color; }
  QColor getFrameLineColor() const { return m_frameLineColor; }
  void setOtherCurvesColor(const QColor &color) { m_otherCurvesColor = color; }
  QColor getOtherCurvesColor() const { return m_otherCurvesColor; }
  void setRulerBackground(const QColor &color) { m_rulerBackground = color; }
  QColor getRulerBackground() const { return m_rulerBackground; }
  void setTextColor(const QColor &color) { m_textColor = color; }
  QColor getTextColor() const { return m_textColor; }
  void setSubColor(const QColor &color) { m_subColor = color; }
  QColor getSubColor() const { return m_subColor; }
  void setSelectedColor(const QColor &color) { m_selectedColor = color; }
  QColor getSelectedColor() const { return m_selectedColor; }

protected:
  void updateGadgets(TDoubleParam *curve);

  void drawCurrentFrame(QPainter &);
  void drawFrameGrid(QPainter &);
  void drawValueGrid(QPainter &);
  void drawOtherCurves(QPainter &);
  void drawCurrentCurve(QPainter &);
  void drawGroupKeyframes(QPainter &);
  //! Makes the selected curves follow another one, via an Expression segment.
  //! The animator never types the expression: a dialog composes it from delay,
  //! multiplier and offset, and what comes out stays an ordinary expression,
  //! editable by hand afterwards.
  void linkSelectedCurves();
  //! Draws the speed strip. Called LAST, opaque, over the bottom of the value
  //! graph: a split view without having to rescale the value transform.
  void drawSpeedGraph(QPainter &);
  //! Top edge of the speed strip, or height() when it is hidden. Anything that
  //! must not be reachable under the strip compares against this.
  int speedGraphTop() const;
  //! The curves the speed strip draws: the SELECTED ones, as Blender's "only
  //! show selected" does. With nothing selected it falls back to the current
  //! curve, so the strip is never inexplicably blank.
  QList<TDoubleParam *> speedGraphCurves() const;

  bool event(QEvent *e) override;
  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void openContextMenu(QMouseEvent *e);

  void keyPressEvent(QKeyEvent *e) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;

  //! Shows in the hint bar what the modifiers do where the pointer is.
  void updateHint(const QPoint &winPos);
  QString m_lastHint;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  QColor getChannelColor(QString name, bool active);

  //! getChannelColor() plus the fade that marks a curve driving nothing in the
  //! object's present state -- x and y while it is on a motion path, posPath
  //! while it is not (FunctionTreeModel::Channel::isInert). Drawing such a
  //! curve at full strength is how one ends up editing a trajectory that
  //! stopped meaning anything.
  QColor getChannelColor(FunctionTreeModel::Channel *channel, bool active);

public slots:
  void onFrameSwitched();
  void onFitCalled();

signals:
  // void segmentSelected(TDoubleParam *curve, int segmentIndex);
  void keyframeSelected(double frame);
};

#endif
