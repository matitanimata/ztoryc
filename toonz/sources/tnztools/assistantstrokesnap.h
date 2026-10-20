#pragma once

#ifndef ASSISTANTSTROKESNAP_H
#define ASSISTANTSTROKESNAP_H

// Snaps a brush stroke to the drawing assistants (OpenToonz assistants,
// phase 2 of experiment/ot-assistants).
//
// OpenToonz feeds its brushes through TInputManager, a modifier chain the
// Tahoma2D brushes do not have.  This is the part of TModifierAssistants
// that matters, without the chain: at the first point the guidelines of the
// enabled assistants are collected; while the stroke is short the one the
// gesture follows best is chosen (TGuideline::findBest), then it is locked;
// every raw point is projected on it before reaching the brush.

#include "tools/assistant.h"
#include "tools/track.h"

#include "tgeometry.h"
#include "tproperty.h"

#include <memory>
#include <vector>

class TTool;

class AssistantStrokeSnap {
  TTool *m_tool = nullptr;
  TGuidelineList m_guidelines;
  TGuidelineP m_current;
  std::unique_ptr<TTrack> m_track;  // TTrack cannot be reassigned
  std::vector<TThickPoint> m_raw;
  bool m_locked = false;

public:
  // Collects the guidelines through pos; false when no assistant is near.
  bool begin(TTool *tool, const TThickPoint &pos);
  // Adds a raw point.  Returns true when the chosen guideline changed: the
  // points given so far are then stale and the stroke must be rebuilt from
  // rawPoints().
  bool add(const TThickPoint &raw);
  TThickPoint transform(const TThickPoint &raw) const;
  const std::vector<TThickPoint> &rawPoints() const { return m_raw; }
  bool isActive() const { return m_tool && !m_guidelines.empty(); }
  // The guideline is final: points can be painted for good (raster).
  bool isLocked() const { return m_locked; }
  void end();

  // The guidelines of the current stroke, the chosen one highlighted.
  void drawGuidelines() const;
  // The guidelines through a hover position, before the stroke starts.
  static void drawHoverGuidelines(TTool *tool, const TPointD &pos);

private:
  TAffine trackToScreen() const;
};

// One menu chooses what a brush stroke snaps to, the Tahoma perspective grid
// or the OpenToonz assistants, never both (Franco, 20/10).  The two flags
// stay the ones the brush code reads; the menu sets them.
namespace GuideSnap {

inline void init(TEnumProperty &guides) {
  guides.addValue(L"Off");
  guides.addValue(L"Grid");
  guides.addValue(L"Assistants");
  guides.setIndex(2);  // matches the flags' defaults
}

inline void apply(const TEnumProperty &guides, TBoolProperty &grid,
                  TBoolProperty &assistants) {
  grid.setValue(guides.getIndex() == 1);
  assistants.setValue(guides.getIndex() == 2);
}

// a Grid saved by an older build is kept
inline void load(TEnumProperty &guides, bool grid, bool assistants) {
  guides.setIndex(grid ? 1 : assistants ? 2 : 0);
}

}  // namespace GuideSnap

#endif
