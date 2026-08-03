

#include "keyframedata.h"
#include "tapp.h"
#include "toonzqt/tselectionhandle.h"
#include "keyframeselection.h"
#include "xsheetviewer.h"

#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txshcolumn.h"
#include "toonz/preferences.h"
#include "toonz/doubleparamcmd.h"

#include <assert.h>
#include <QDebug>

//=============================================================================
// TKeyframeData
//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData() {}

//-----------------------------------------------------------------------------

TKeyframeData::TKeyframeData(const TKeyframeData *src)
    : m_keyData(src->m_keyData)
    , m_isPegbarsCycleEnabled(src->m_isPegbarsCycleEnabled)
    , m_offset(src->m_offset) {}

//-----------------------------------------------------------------------------

TKeyframeData::~TKeyframeData() {}

//-----------------------------------------------------------------------------
// data <- xsheet
void TKeyframeData::setKeyframes(std::set<Position> positions, TXsheet *xsh) {
  Position startPos(-1, -1);
  setKeyframes(positions, xsh, startPos);
}

void TKeyframeData::setKeyframes(std::set<Position> positions, TXsheet *xsh,
                                 Position startPos) {
  if (positions.empty()) return;

  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());

  std::set<Position>::iterator it = positions.begin();
  int r0                          = it->first;
  int c0                          = it->second;
  int r1                          = it->first;
  int c1                          = it->second;
  for (++it; it != positions.end(); ++it) {
    r0 = std::min(r0, it->first);
    c0 = std::min(c0, it->second);
    r1 = std::max(r1, it->first);
    c1 = std::max(c1, it->second);
  }

  m_columnSpanCount = c1 - c0 + 1;
  m_rowSpanCount    = r1 - r0 + 1;

  if (startPos.first >= 0 && startPos.second >= 0)
    m_offset = std::make_pair(r0 - startPos.first, c0 - startPos.second);

  for (it = positions.begin(); it != positions.end(); ++it) {
    int row              = it->first;
    int col              = it->second;
    TStageObject *pegbar = xsh->getStageObject(col >= 0 ? xsh->getColumnObjectId(col) : cameraId);
    assert(pegbar);
    m_isPegbarsCycleEnabled[col] = pegbar->isCycleEnabled();
    if (pegbar->isKeyframe(row)) {
      Position p(row - r0, col - c0);
      TStageObject::Keyframe k = pegbar->getKeyframe(row);
      TPointD center, offset;
      pegbar->getCenterAndOffset(center, offset);
      m_keyData[p]    = k;
      m_centerData[p] = CenterInfo(center, offset);
    }
  }
}

//-----------------------------------------------------------------------------

// data -> xsh
bool TKeyframeData::getKeyframes(std::set<Position> &positions,
                                 TXsheet *xsh) const {
  std::set<TKeyframeSelection::Position>::iterator it2 = positions.begin();
  int r0                                               = it2->first;
  int c0                                               = it2->second;
  std::map<int, int> firstRowCol, lastRowCol;
  firstRowCol.insert(std::pair<int, int>(c0, r0));
  lastRowCol.insert(std::pair<int, int>(c0, r0));
  for (++it2; it2 != positions.end(); ++it2) {
    r0 = std::min(r0, it2->first);
    c0 = std::min(c0, it2->second);
    std::map<int, int>::iterator itF = firstRowCol.find(it2->second);
    std::map<int, int>::iterator itL = lastRowCol.find(it2->second);
    if (itF == firstRowCol.end())
      firstRowCol.insert(std::pair<int, int>(it2->second, it2->first));
    if (itL == lastRowCol.end())
      lastRowCol.insert(std::pair<int, int>(it2->second, it2->first));
    else
      itL->second = c0;
  }

  XsheetViewer *viewer = TApp::instance()->getCurrentXsheetViewer();

  positions.clear();
  TStageObjectId cameraId =
      TStageObjectId::CameraId(xsh->getCameraColumnIndex());
  Iterator it;
  Iterator2 it3 = m_centerData.begin();
  bool keyFrameChanged = false;
  for (it = m_keyData.begin(); it != m_keyData.end(); ++it, ++it3) {
    Position pos = it->first;
    int row      = r0 + pos.first;
    int col      = c0 + pos.second;
    positions.insert(std::make_pair(row, col));
    TXshColumn *column = xsh->getColumn(col);
    if (column && (column->getSoundColumn() || column->getFolderColumn()))
      continue;
    TStageObject *pegbar = xsh->getStageObject(col >= 0 ? xsh->getColumnObjectId(col) : cameraId);
    if (xsh->getColumn(col) && xsh->getColumn(col)->isLocked()) continue;

    // A real guard, not an assert: asserts are compiled out of the release
    // build, so a null pegbar went straight into getKeyframeRange() below.
    // It CAN be null -- the id above falls back to the camera when col is
    // negative, and CameraId(-1) on an xsheet with no camera column is an
    // invalid id that the stage object tree refuses to create (deliberately:
    // creating it is how the "BadPegbar" zombies got written into scenes).
    assert(pegbar);
    if (!pegbar) continue;

    keyFrameChanged = true;

    int kF, kL, kP, kN;
    double e0, e1;
    pegbar->getKeyframeRange(kF, kL);
    pegbar->setKeyframeWithoutUndo(row, it->second);
    CenterInfo centerInfo = it3->second ;
    pegbar->setCenterAndOffset(centerInfo.first, centerInfo.second);

    std::map<int, int>::iterator itF = firstRowCol.find(col);
    std::map<int, int>::iterator itL = lastRowCol.find(col);
    TStageObject::Keyframe newKey = pegbar->getKeyframe(row);

    TDoubleKeyframe::Type wantType[TStageObject::T_ChannelCount];
    bool wantTypeSet[TStageObject::T_ChannelCount] = {false};

    // Diagnostic, ZTORYC_KEYPASTE_DIAG=1. Which branch a paste takes decides
    // whether the pasted key's interpolation gets fixed up; deducing it from
    // the code has already been wrong once.
    const bool keyPasteDiag = ::getenv("ZTORYC_KEYPASTE_DIAG") != 0;
    // Every KEYED channel, not one picked in advance: reading only T_Angle
    // said "prev.isKey=N" on a column where the rotation simply is not
    // animated, which answers nothing.
    static const char *chName[] = {"Angle", "X",      "Y",      "Z",
                                   "SO",    "ScaleX", "ScaleY", "Scale",
                                   "Path",  "ShearX", "ShearY", "?"};
    if (keyPasteDiag) {
      QString keyed;
      for (int i = 0; i < TStageObject::T_ChannelCount; i++)
        if (newKey.m_channels[i].m_isKeyframe)
          keyed += QString(" %1=t%2/p%3")
                       .arg(chName[i < 11 ? i : 11])
                       .arg((int)newKey.m_channels[i].m_type)
                       .arg((int)newKey.m_channels[i].m_prevType);
      qDebug().noquote()
          << QString("[KEYPASTE] row=%1 col=%2 kF=%3 kL=%4 | firstBlock=%5 "
                     "lastBlock=%6 | keyed:%7")
                 .arg(row).arg(col).arg(kF).arg(kL)
                 .arg(itF != firstRowCol.end() && itF->second == row ? "Y" : "N")
                 .arg(itL != lastRowCol.end() && itL->second == row ? "Y" : "N")
                 .arg(keyed.isEmpty() ? QString(" NONE") : keyed);
    }
    // Process 1st key added in column
    if (itF != firstRowCol.end() && itF->second == row) {
      if (row > kL) {
        // If new key was added after the existing last one, create new
        // interpolation between them using preference setting
        TStageObject::Keyframe prevKey = pegbar->getKeyframe(kL);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              prevKey.m_channels[i].m_isKeyframe) {
            prevKey.m_channels[i].m_type = TDoubleKeyframe::Type(
                Preferences::instance()->getKeyframeType());
            newKey.m_channels[i].m_prevType = prevKey.m_channels[i].m_type;
          }
        }
        pegbar->setKeyframeWithoutUndo(kL, prevKey);
        pegbar->setKeyframeWithoutUndo(row, newKey);
      } else if (row > kF) {
        // If new key was added between existing keys, sync new key's previous
        // interpolation to key just before it
        if (!pegbar->getKeyframeSpan(row - 1, kP, e0, kN, e1)) kP = row - 1;
        TStageObject::Keyframe prevKey = pegbar->getKeyframe(kP);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              prevKey.m_channels[i].m_isKeyframe) {
            newKey.m_channels[i].m_prevType = prevKey.m_channels[i].m_type;
          }

          // The segment AFTER the pasted key wants the type of the span it was
          // dropped into. Asked of the CHANNEL, not of the stage object: a
          // keyframe can be partial, and getKeyframeSpan answers at object
          // level. With only Y keyed at the previous row, every other channel
          // read prevKey.m_isKeyframe == false and kept the clipboard's type
          // -- the Linear placeholder of a key copied from the end of a curve.
          // Their real previous key simply lies further back.
          //
          // Recorded here, applied below through KeyframeSetter: the key still
          // carries the clipboard's ease handles, at zero when it came from
          // the end of a curve, and a segment of the right NAME with no easing
          // is drawn perfectly straight.
          if (!newKey.m_channels[i].m_isKeyframe) continue;
          TDoubleParam *chParam = pegbar->getParam((TStageObject::Channel)i);
          if (!chParam) continue;
          const int prevIdx = chParam->getPrevKeyframe((double)row);
          if (prevIdx >= 0)
            wantType[i] = chParam->getKeyframe(prevIdx).m_type;
          else
            // First key of this channel: no span to continue, so the
            // preference decides -- same as upstream's "before the 1st" case.
            wantType[i] =
                TDoubleKeyframe::Type(Preferences::instance()->getKeyframeType());
          wantTypeSet[i] = true;
        }
        if (keyPasteDiag) {
          QString per;
          for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
            if (!newKey.m_channels[i].m_isKeyframe) continue;
            per += QString(" %1:objPrevKey=%2 want=%3")
                       .arg(chName[i < 11 ? i : 11])
                       .arg(prevKey.m_channels[i].m_isKeyframe ? "Y" : "N")
                       .arg(wantTypeSet[i] ? (int)wantType[i] : -1);
          }
          qDebug().noquote()
              << QString("[KEYPASTE]   between-first: kP=%1 |%2").arg(kP).arg(per);
        }
        pegbar->setKeyframeWithoutUndo(row, newKey);
      }
    }
    // Process last key added in column
    if (itL != lastRowCol.end() && itL->second == row) {
      if (row < kF) {
        // If new key was added before the existing 1st one, create new
        // interpolation between them using preference setting
        TStageObject::Keyframe nextKey = pegbar->getKeyframe(kF);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              nextKey.m_channels[i].m_isKeyframe) {
            newKey.m_channels[i].m_type = TDoubleKeyframe::Type(
                Preferences::instance()->getKeyframeType());
            nextKey.m_channels[i].m_prevType = newKey.m_channels[i].m_type;
          }
        }
        pegbar->setKeyframeWithoutUndo(row, newKey);
        pegbar->setKeyframeWithoutUndo(kF, nextKey);
      } else if (row < kL) {
        // If new key was added between existing keys, sync new key to the next
        // key's previous interpolation
        if (!pegbar->getKeyframeSpan(row + 1, kP, e0, kN, e1)) kN = row + 1;
        TStageObject::Keyframe nextKey = pegbar->getKeyframe(kN);
        for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
          if (newKey.m_channels[i].m_isKeyframe &&
              nextKey.m_channels[i].m_isKeyframe) {
            nextKey.m_channels[i].m_prevType = newKey.m_channels[i].m_type;
          }
        }
        if (keyPasteDiag)
          qDebug().noquote()
              << QString("[KEYPASTE]   between-last: kN=%1").arg(kN);
        pegbar->setKeyframeWithoutUndo(row, newKey);
      }
    }
    // Now that the key is on the pegbar, give the segment its handles.
    // setType writes speedOut here and speedIn on the FOLLOWING keyframe, both
    // from segmentWidth/3 -- and it returns early when the type already
    // matches, which is why nothing above assigns m_type.
    for (int i = 0; i < TStageObject::T_ChannelCount; i++) {
      if (!wantTypeSet[i]) continue;
      TDoubleParam *param = pegbar->getParam((TStageObject::Channel)i);
      if (!param) continue;
      const int kIndex = param->getClosestKeyframe((double)row);
      if (kIndex < 0 || param->keyframeIndexToFrame(kIndex) != (double)row)
        continue;
      if (kIndex >= param->getKeyframeCount() - 1) continue;  // still last
      KeyframeSetter(param, kIndex, false).setType(kIndex, wantType[i]);
    }

    if (keyPasteDiag) {
      TStageObject::Keyframe after = pegbar->getKeyframe(row);
      QString per;
      for (int i = 0; i < TStageObject::T_ChannelCount; i++)
        if (after.m_channels[i].m_isKeyframe)
          per += QString(" %1=t%2/p%3 out=%4 in=%5")
                     .arg(chName[i < 11 ? i : 11])
                     .arg((int)after.m_channels[i].m_type)
                     .arg((int)after.m_channels[i].m_prevType)
                     .arg(after.m_channels[i].m_speedOut.x, 0, 'f', 2)
                     .arg(after.m_channels[i].m_speedIn.x, 0, 'f', 2);
      qDebug().noquote() << QString("[KEYPASTE]   AFTER:%1")
                                .arg(per.isEmpty() ? QString(" NONE") : per);
    }
  }
  if (!keyFrameChanged) return false;

  for (auto const pegbar : m_isPegbarsCycleEnabled) {
    int const col = pegbar.first;
    TStageObjectId objectId =
        (col >= 0) ? xsh->getColumnObjectId(col) : cameraId;
    xsh->getStageObject(objectId)->enableCycle(pegbar.second);
  }
  return true;
}

//-----------------------------------------------------------------------------

void TKeyframeData::getKeyframes(std::set<Position> &positions) const {
  int r0 = positions.begin()->first;
  int c0 = positions.begin()->second;
  positions.clear();

  TKeyframeData::KeyData::const_iterator it;
  for (it = m_keyData.begin(); it != m_keyData.end(); ++it) {
    TKeyframeData::Position pos(it->first);
    positions.insert(
        TKeyframeSelection::Position(pos.first + r0, pos.second + c0));
  }
}

void TKeyframeData::setKeyframesOffset(int row, int col) {
  m_offset = std::make_pair(row, col);
}
