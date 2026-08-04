#pragma once

#ifndef FUNCTIONSELECTION_H
#define FUNCTIONSELECTION_H

#include "tcommon.h"
#include "functiontreeviewer.h"
#include "toonzqt/selection.h"
#include "toonzqt/dvmimedata.h"

#include "toonz/txsheethandle.h"

#include "tdoublekeyframe.h"

#include <QWidget>
#include <QList>
#include <QSet>

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
class TFrameHandle;
class FunctionSheet;
struct EasePreset;

//-----------------------------------------------------------------------------

class ColumnToCurveMapper {
public:
  virtual TDoubleParam *getCurve(int columnIndex) const = 0;
};

//-----------------------------------------------------------------------------

class FunctionSelection final : public QObject, public TSelection {
  Q_OBJECT
  QRect m_selectedCells;  // yrange = rowrange of the selected keyframes;
                          // xrange = columnrange (functionsheet only)
  QList<QPair<TDoubleParam *, QSet<int>>> m_selectedKeyframes;
  // first = curve, second = set of selected keyframes index

  int m_selectedSegment;  // index of the first keyframe of the segment; -1 if
                          // no segment selected
                          // (functionpanel only)
  // assert(m_selectedSegment<0 || m_selectedKeyframes.size()==1)

  //! Every segment picked in the graph, as (curve, index of its first
  //! keyframe). Shift-clicking segments accumulates here so that one
  //! interpolation command can retype a whole run of them. Kept in step with
  //! m_selectedSegment, which stays the SINGLE-segment answer the spreadsheet
  //! and the segment editor were written against: with several picked it goes
  //! to -1, since there is no one segment to show.
  QList<QPair<TDoubleParam *, int>> m_selectedSegments;

  TFrameHandle *m_frameHandle;
  TXsheetHandle *m_xsheetHandle;
  ColumnToCurveMapper *m_columnToCurveMapper;
  FunctionSheet *m_sheet;

  void applyTangentsToSelection(bool flat);

  //! One keyframe's tangents, kept between a copy and a paste.
  struct TangentClip {
    bool m_full = false;
    //! Handle length as a fraction of the segment width, and rise as a
    //! fraction of the segment's value change. Zero segments keep zero.
    double m_outXFrac = 0, m_outYFrac = 0;
    double m_inXFrac = 0, m_inYFrac = 0;
    bool m_hasOut = false, m_hasIn = false;
  };
  static TangentClip m_tangentClip;
  int getCurveIndex(TDoubleParam *curve) const;
  // finds i : m_selectedKeyframes[i].first == curve
  //-1 if curve not found
  int touchCurveIndex(TDoubleParam *curve);
  // as getCurve(); if curve not found then add it

public:
  FunctionSelection();
  ~FunctionSelection();

  void setFrameHandle(TFrameHandle *frameHandle) {
    m_frameHandle = frameHandle;
  }
  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }
  void setFunctionSheet(FunctionSheet *sheet) { m_sheet = sheet; }

  // function graph
  void selectCurve(TDoubleParam *curve);
  void deselectAllKeyframes();

  // function sheet
  QRect getSelectedCells() const { return m_selectedCells; }
  void selectCells(const QRect &selectedCells,
                   const QList<TDoubleParam *> &curves);
  void selectCells(const QRect &selectedCells);
  void deselectAllCells();

  bool isEmpty() const override { return m_selectedKeyframes.empty(); }
  void selectNone() override;
  void select(TDoubleParam *curve, int k);
  bool isSelected(TDoubleParam *curve, int k) const;
  void selectSegment(TDoubleParam *, int k,
                     QRect selectedCells = QRect());  // note: if a segment is
                                                      // selected then also the
                                                      // segment ends are
                                                      // selected
  //! Adds one segment to the picked ones, keeping those already there.
  void addSegment(TDoubleParam *curve, int k);
  int getSelectedSegmentCount() const { return m_selectedSegments.size(); }
  //! Gives every selected keyframe an auto bezier tangent. Acts on the
  //! SELECTION only: applied to a whole curve it would flatten the held frames
  //! and hard stops that were put there on purpose.
  void setSelectedKeyframesAutoBezier();
  //! Flattens the tangent of every selected keyframe.
  void setSelectedKeyframesFlat();

  //! Copies the SHAPE of a keyframe's tangents -- not its value. Stored as
  //! fractions of the segment it came from (of its width, and of its value
  //! rise), so pasting it onto a segment of another length or another height
  //! reproduces the same character rather than the same numbers.
  void copyTangents();
  //! Applies the copied shape to every selected keyframe.
  void pasteTangents();
  bool hasCopiedTangents() const { return m_tangentClip.m_full; }

  //! Gives \p preset's shape to every segment the selection covers.
  //!
  //! An ease is a segment's shape, but a selection can be made of either, so
  //! both readings are answered: SEGMENTS picked say it outright and are taken
  //! at their word; KEYS picked are read the way Auto Bezier and Flat already
  //! read them -- both sides of every key chosen -- so no two commands in the
  //! same menu can disagree about what "the selection" is. One key selected
  //! therefore eases the movement into it and out of it, which is what asking
  //! for an ease at a key means.
  void applyEasePreset(const EasePreset &preset);

  //! Spreads the selected keys evenly in time so the value runs at constant
  //! speed between the keys bracketing them -- roving, on the channel where it
  //! has an exact meaning.
  void distributeSelectedEvenly();
  //! Whether the selection is all on the "posPath" channel, the one where
  //! constant speed in the value is constant speed along the path.
  bool isSelectionOnPosPath() const;
  //! Retypes every picked segment, in one undo.
  void setSelectedSegmentsType(TDoubleKeyframe::Type type);
  //! The interpolation ALL the picked segments already share, or -1 when they
  //! differ. With mixed types no entry may be greyed out: every choice is a
  //! real change for at least one segment.
  int getCommonSelectedSegmentsType() const;
  int getSelectedKeyframeCount() const;
  QPair<TDoubleParam *, int> getSelectedKeyframe(int index)
      const;  // if index<0 || index>=getSelectedKeyframeCount() returns (0,-1)

  QPair<TDoubleParam *, int> getSelectedSegment()
      const;  // if no segment is selected returns (0,-1)
  bool isSegmentSelected(TDoubleParam *, int k) const;

  void setColumnToCurveMapper(ColumnToCurveMapper *mapper);  // gets ownership

  TDoubleParam *getCurveFromColumn(int columnIndex) const {
    return m_columnToCurveMapper ? m_columnToCurveMapper->getCurve(columnIndex)
                                 : 0;
  }

  void enableCommands() override;

  void doCopy();
  void doPaste();
  void doCut();
  void doDelete();
  void insertCells();

  // if inclusive == true, consider all segments overlapping the selection
  void setStep(int, bool inclusive = true);
  void setStep1() { setStep(1); }
  void setStep2() { setStep(2); }
  void setStep3() { setStep(3); }
  void setStep4() { setStep(4); }

  // return step if all the selected segments has the same value.
  // return -1 if the selection does not overlap any segments
  // return 0 if the step value is uneven in the selection
  // if inclusive == true, consider all segments overlapping the selection
  int getCommonStep(bool inclusive = true);

  void setSegmentType(TDoubleKeyframe::Type type, bool inclusive = true);

  // return TDoubleKeyframe::Type value if the selected segments has the same
  // interpolation type. return -1 if the selection does not overlap any
  // segments return 0 (TDoubleKeyframe::None) if the interpolation is not
  // identical in the selection if inclusive == true, consider all segments
  // overlapping the selection
  int getCommonSegmentType(bool inclusive = true);

  QList<int> getSelectedKeyIndices(TDoubleParam *curve);

  //! Curves carrying at least one selected keyframe. More than one of them
  //! means the selection spans several parameters, which the graph has to
  //! move as one block.
  QList<TDoubleParam *> getSelectedCurves() const;
signals:
  void selectionChanged();
};

//-----------------------------------------------------------------------------

class FunctionKeyframesData final : public DvMimeData {
public:
  FunctionKeyframesData();
  ~FunctionKeyframesData();

  typedef std::vector<TDoubleKeyframe> Keyframes;

  void setColumnCount(int columnCount);
  int getColumnCount() const { return (int)m_keyframes.size(); }

  int getRowCount() const;

  void getData(int columnIndex, TDoubleParam *curve, double frame,
               const QSet<int> &kIndices);
  void setData(int columnIndex, TDoubleParam *curve, double frame) const;

  const Keyframes &getKeyframes(int columnIndex) const;

  DvMimeData *clone() const override;

  bool isCircularReferenceFree(int columnIndex, TDoubleParam *curve) const;

private:
  std::vector<Keyframes> m_keyframes;
};

#endif
