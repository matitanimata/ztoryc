#pragma once

#ifndef PARAMCMD_INCLUDED
#define PARAMCMD_INCLUDED

#include "tdoubleparam.h"
#include "tdoublekeyframe.h"
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class KeyframesUndo;
class TSceneHandle;
class TXsheetHandle;
class TObjectHandle;
class TStageObject;

class DVAPI KeyframeSetter {
  TDoubleParamP m_param;
  int m_kIndex;
  std::set<int> m_indices;
  int m_extraDFrame;  // used by moveKeyframes
  bool m_enableUndo;
  TDoubleKeyframe m_keyframe;
  KeyframesUndo *m_undo;
  bool m_changed;
  double m_pixelRatio;  // frame pixel size / value pixel size

  double getNorm(const TPointD &p) const {
    double y = p.y * m_pixelRatio;
    return sqrt(p.x * p.x + y * y);
  }
  void getRotatingSpeedHandles(
      std::vector<std::pair<double, int>> &rotatingSpeeds, TDoubleParam *param,
      int kIndex) const;

public:
  KeyframeSetter(TDoubleParam *param, int kIndex = -1, bool enableUndo = true);
  KeyframeSetter(TDoubleParam *param, TXsheetHandle *xsheetHandle,
                 int kIndex = -1, bool enableUndo = true);
  ~KeyframeSetter();

  TDoubleParam *getCurve() const { return m_param.getPointer(); }

  // pixel ratio refers to graph panel. It is necessary to move along a circular
  // arc
  void setPixelRatio(double pixelRatio) { m_pixelRatio = pixelRatio; }
  double getPixelRatio() const { return m_pixelRatio; }

  void selectKeyframe(int kIndex);

  // create a new keyframe, select it and returns its k-index
  // (if a keyframe already exists at frame then it is equivalent to
  // selectKeyframe)
  // note: call createKeyframe() when no other keyframes are selected
  int createKeyframe(double frame);

  bool isSelected(int index) const { return m_indices.count(index) > 0; }

  void moveKeyframes(int dFrame, double dValue);

  bool isSpeedInOut(int segmentIndex) const;
  bool isEaseInOut(int segmentIndex) const;  // true also if EaseInOutPercentage

  // the following methods apply if only a single keyframe has been selected
  void setType(TDoubleKeyframe::Type type);
  void setType(int kIndex, TDoubleKeyframe::Type type);
  void setStep(int step);
  void setExpression(std::string expression);
  void setSimilarShape(std::string expression, double offset);
  void setFile(const TDoubleKeyframe::FileParams &params);
  void setUnitName(std::string unitName);

  void setValue(double value);

  void linkHandles();
  void unlinkHandles();

  void setSpeedIn(const TPointD &speedIn);
  void setSpeedOut(const TPointD &speedOut);
  void setEaseIn(double easeIn);
  void setEaseOut(double easeOut);

  // set the curve params adaptively by clicking apply button
  void setAllParams(int step, TDoubleKeyframe::Type comboType,
                    const TPointD &speedIn, const TPointD &speedOut,
                    std::string expressionText, std::string unitName,
                    const TDoubleKeyframe::FileParams &fileParam,
                    double similarShapeOffset);

  // addUndo is called automatically (if needed) in the dtor.
  // it is also possible to call it explicitly.
  void addUndo();

  static void setValue(TDoubleParam *curve, double frame, double value) {
    KeyframeSetter setter(curve);
    setter.createKeyframe(frame);
    setter.setValue(value);
  }
  static void removeKeyframeAt(TDoubleParam *curve, double frame,
                               TObjectHandle *objectHandle,
                               TXsheetHandle *xsheetHandle = nullptr);
  static void removeKeyframeAt(TDoubleParam *curve, double frame,
                               TStageObject *stageObj,
                               TXsheetHandle *xsheetHandle = nullptr);

  static void enableCycle(TDoubleParam *curve, bool enabled,
                          TSceneHandle *sceneHandle = nullptr);

  //! Gives \p kIndices AUTO BEZIER tangents: the slope at each key is decided
  //! from its neighbours instead of segment by segment, so the curve runs
  //! smoothly through them instead of wandering between them.
  //!
  //! Named as After Effects names it. Deliberately NOT "smooth": in Blender
  //! that word is a filter that averages VALUES and moves the animation, which
  //! is not this.
  //!
  //! Two things go wrong when breakdowns are added between main keys, and this
  //! addresses both. The handles of each segment are worked out independently
  //! (segmentWidth/3), so at a key the incoming and outgoing slopes differ and
  //! the curve arrives one way and leaves another -- barely visible in value,
  //! a spike in speed. And handles longer than the local rise make the curve
  //! leave the interval between two keys altogether: it climbs, falls back,
  //! climbs again.
  //!
  //! So: the tangent at a key comes from its NEIGHBOURS (Catmull-Rom), which
  //! makes it continuous by construction, and is then clamped so the curve
  //! cannot overshoot between two keys (Fritsch-Carlson -- a key that is a
  //! local extremum gets a flat tangent). The affected segments become
  //! SpeedInOut, the only type that can carry a slope at all: EaseInOut stores
  //! an ease AMOUNT with an implicit direction.
  //!
  //! Keyframes are never moved: the timing is the animator's.
  static void setAutoBezier(TDoubleParam *curve, const std::set<int> &kIndices,
                            bool enableUndo = true);

  //! Flattens the tangent at each of \p kIndices: the curve arrives and leaves
  //! horizontally. That is how an EXTREME is marked -- the contact, the apex,
  //! the point where the movement stops and starts again -- and it is also the
  //! way to undo an auto bezier that guessed wrong.
  static void setFlatTangents(TDoubleParam *curve,
                              const std::set<int> &kIndices,
                              bool enableUndo = true);

  //! Repositions \p kIndices IN TIME so the value runs at constant speed
  //! between the keys bracketing them. Their values are untouched: only when
  //! each one is reached changes.
  //!
  //! Meant for the "posPath" channel, which holds a position along a spline as
  //! a PERCENTAGE OF ITS LENGTH -- so on that channel constant speed in the
  //! value really is constant speed along the path, exactly and not by
  //! approximation. It is the same idea as After Effects' roving keys, done as
  //! a one-shot command: no flag on the keyframe, so nothing enters the file
  //! format and nothing has to recompute itself later. Move an end key and you
  //! run it again.
  //!
  //! Does nothing unless the keys are bracketed on BOTH sides: without two
  //! fixed ends there is no span to spread them over.
  //! Returns kIndex -> target frame; empty when the request makes no sense.
  //! It only WORKS OUT the frames: the caller moves the keys, because on a
  //! stage object a keyframe belongs to every channel at once and moving the
  //! posPath one alone splits it in two -- the xsheet then shows a key at the
  //! old frame (the other channels) and one at the new (posPath).
  //!
  //! The caller must also make the roved span LINEAR. These frames come from a
  //! straight line between the bracketing keys, so anything else leaves the
  //! keys in the right places with the movement between them still eased --
  //! the promise kept at the keys and broken everywhere else.
  static std::map<int, int> computeRovedFrames(TDoubleParam *curve,
                                               const std::set<int> &kIndices);

private:
  //! The two above: identical except for where the slope comes from.
  static void setTangents(TDoubleParam *curve, const std::set<int> &kIndices,
                          bool flat, bool enableUndo);
};

#endif
