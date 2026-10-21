#pragma once

#ifndef KEYFRAMEDATA_INCLUDED
#define KEYFRAMEDATA_INCLUDED

#include "toonzqt/dvmimedata.h"

#include "toonz/tstageobject.h"
#include "keyframeselection.h"

// forward declarations
class TXsheet;

//=============================================================================
// TKeyframeData
//-----------------------------------------------------------------------------

#include "ext/plasticskeletondeformation.h"

class TKeyframeData final : public DvMimeData {
public:
  typedef TKeyframeSelection::Position Position;
  typedef std::map<Position, TStageObject::Keyframe> KeyData;
  typedef std::map<Position, TStageObject::Keyframe>::const_iterator Iterator;
  typedef std::pair<TPointD, TPointD> CenterInfo;
  typedef std::map<Position, CenterInfo> CenterData;
  typedef std::map<Position, CenterInfo>::const_iterator Iterator2;

  KeyData m_keyData;
  CenterData m_centerData;
  int m_columnSpanCount;
  int m_rowSpanCount;

  Position m_offset = std::make_pair(0, 0);

  // Numero di colonna della pegbar associato al booleano isCycleEnabled della
  // pegbar
  std::map<int, bool> m_isPegbarsCycleEnabled;

  // Ztoryc: the Plastic skeleton of each copied column (by column offset),
  // without keys: pasted on a column with no skeleton, the keys bring it
  // along — the skeleton goes with the key (Franco, 2026-10-21).
  std::map<int, PlasticSkeletonDeformationP> m_skeletons;

  // Undo of a paste: drops from columns c0..c1 a skeleton that a paste put
  // there and that has no key left. Never a skeleton made by hand.
  static void releasePastedSkeletons(TXsheet *xsh, int c0, int c1);

  TKeyframeData();
  TKeyframeData(const TKeyframeData *src);
  ~TKeyframeData();

  TKeyframeData *clone() const override { return new TKeyframeData(this); }

  // data <- xsh
  void setKeyframes(std::set<Position> positions, TXsheet *xsh);
  void setKeyframes(std::set<Position> positions, TXsheet *xsh,
                    Position startPos);

  // data -> xsh
  bool getKeyframes(std::set<Position> &positions, TXsheet *xsh) const;

  // Ztoryc: true when pasting at `dest` (top-left = its min, as getKeyframes)
  // would put a key holding a Plastic pose on a column with no Plastic
  // skeleton. setKeyframeWithoutUndo drops that half of the key in silence.
  bool losesPlasticPose(const std::set<Position> &dest, TXsheet *xsh) const;

  // Reads data keyframes and fills in positions set. The first element
  // passed with the set is automatically added to stored positions.
  void getKeyframes(std::set<Position> &positions) const;

  TKeyframeData *clone() {
    TKeyframeData *data = new TKeyframeData(this);
    return data;
  }

  int getColumnSpanCount() const { return m_columnSpanCount; }
  int getRowSpanCount() const { return m_rowSpanCount; }

  void setKeyframesOffset(int row, int col);
  Position getKeyframesOffset() const { return m_offset; }
};

#endif  // KEYFRAMEDATA_INCLUDED
