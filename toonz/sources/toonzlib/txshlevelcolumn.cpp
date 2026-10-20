

#include "toonz/txshlevelcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/tcolumnfx.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txsheet.h"

#include "tstream.h"

// Qt includes
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QSet>
#include <QUuid>
#include <algorithm>
#include <iterator>
#include <limits>

//-----------------------------------------------------------------------------

namespace {
bool placementDigest(const TXshLevelColumn &column,
                     const FlashPlacementData &placement,
                     quint64 maxCellsToCheck, QString &digest) {
  if (placement.start < 0 || placement.duration < 1 ||
      quint64(placement.duration) > maxCellsToCheck ||
      qint64(placement.start) + placement.duration >
          std::numeric_limits<int>::max())
    return false;
  QCryptographicHash hash(QCryptographicHash::Sha256);
  for (int row = placement.start; row < placement.start + placement.duration;
       ++row) {
    const TXshCell &cell = column.getCell(row);
    if (cell.m_level != placement.level ||
        !cell.m_frameId.getLetter().isEmpty() || cell.m_frameId.getNumber() < 1)
      return false;
    const quint32 frame = quint32(cell.m_frameId.getNumber());
    const char bytes[4] = {char(frame), char(frame >> 8), char(frame >> 16),
                           char(frame >> 24)};
    hash.addData(bytes, 4);
  }
  digest = QString::fromLatin1(hash.result().toHex());
  return true;
}

TFrameId qstringToFrameId(QString str) {
  if (str.isEmpty() || str == "-1")
    return TFrameId::EMPTY_FRAME;
  else if (str == "-" || str == "-2")
    return TFrameId::NO_FRAME;
  else if (str == "x" || str == "-3")
    return TFrameId::STOP_FRAME;

  QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
  QRegExp rx(regExpStr);
  int pos = rx.indexIn(str);
  if (pos < 0) return TFrameId();
  if (rx.cap(2).isEmpty())
    return TFrameId(rx.cap(1).toInt());
  else
    return TFrameId(rx.cap(1).toInt(), rx.cap(2));
}
}  // namespace

//-----------------------------------------------------------------------------

//=============================================================================
// TXshLevelColumn

TXshLevelColumn::TXshLevelColumn()
    : m_fx(new TLevelColumnFx())
    //, m_iconId("")
    , m_iconVisible(false) {
  // updateIcon();
  m_fx->addRef();
  m_fx->setColumn(this);
}

//-----------------------------------------------------------------------------

TXshLevelColumn::~TXshLevelColumn() {
  m_fx->setColumn(0);
  m_fx->release();
  m_fx = 0;
}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshLevelColumn::getColumnType() const {
  return eLevelType;
}

bool TXshLevelColumn::isEmpty() const {
  if (!TXshCellColumn::isEmpty()) return false;
  for (const auto &placement : m_flashPlacements)
    if (placement.detached) return false;
  return true;
}

//----------------------------------------------------------------------------

bool TXshLevelColumn::canSetCell(const TXshCell &cell) const {
  if (cell.isEmpty()) return true;

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (sl) return (sl->getType() & LEVELCOLUMN_XSHLEVEL);

  return cell.getChildLevel();
}

void TXshLevelColumn::flashCellsChanged() {
  if (++m_flashRevision == 0) m_flashChecks.clear();
}

bool TXshLevelColumn::setCell(int row, const TXshCell &cell) {
  const bool result = TXshCellColumn::setCell(row, cell);
  if (result) flashCellsChanged();
  return result;
}

bool TXshLevelColumn::setCells(int row, int count, const TXshCell cells[]) {
  const bool result = TXshCellColumn::setCells(row, count, cells);
  if (result) flashCellsChanged();
  return result;
}

void TXshLevelColumn::insertEmptyCells(int row, int count) {
  TXshCellColumn::insertEmptyCells(row, count);
  flashCellsChanged();
}

void TXshLevelColumn::removeCells(int row, int count, bool keepCellMarks) {
  TXshCellColumn::removeCells(row, count, keepCellMarks);
  flashCellsChanged();
}

void TXshLevelColumn::clearCells(int row, int count) {
  TXshCellColumn::clearCells(row, count);
  flashCellsChanged();
}

void TXshLevelColumn::addFlashInstance(const FlashInstanceData &instance) {
  instance.toJson();
  if (m_flashInstances.size() >= 100000)
    throw Flash::Error("Flash instance metadata budget exceeded.");
  const int end       = instance.timing.start + instance.timing.duration;
  const auto position = std::lower_bound(
      m_flashInstances.begin(), m_flashInstances.end(), instance.timing.start,
      [](const FlashInstanceData &existing, int start) {
        return existing.timing.start < start;
      });
  if ((position != m_flashInstances.end() && position->timing.start < end) ||
      (position != m_flashInstances.begin() &&
       std::prev(position)->timing.start +
               std::prev(position)->timing.duration >
           instance.timing.start))
    throw Flash::Error("Overlapping Flash instance metadata spans.");
  FlashInstanceData inserted = instance;
  // Adding a placement is a native copy/new-object operation. Retain its
  // source reference, but never reuse the identity of an existing placement.
  inserted.nativeInstanceId =
      QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_flashInstances.insert(position, std::move(inserted));
  m_flashChecks.clear();
}

void TXshLevelColumn::replaceFlashInstances(
    std::vector<FlashInstanceData> instances) {
  if (instances.size() > 100000)
    throw Flash::Error("Flash instance metadata budget exceeded.");
  int end = 0;
  QSet<QString> identities;
  for (auto &instance : instances) {
    if (instance.nativeInstanceId.isEmpty())
      instance.nativeInstanceId =
          QUuid::createUuid().toString(QUuid::WithoutBraces);
    instance.toJson();
    if (identities.contains(instance.nativeInstanceId))
      throw Flash::Error("Duplicate native Flash instance identifier.");
    identities.insert(instance.nativeInstanceId);
    if (instance.timing.start < end)
      throw Flash::Error(
          "Unsorted or overlapping Flash instance metadata spans.");
    end = instance.timing.start + instance.timing.duration;
  }
  m_flashInstances = std::move(instances);
  m_flashChecks.clear();
}

QString TXshLevelColumn::addFlashPlacement(
    const FlashPlacementData &placement) {
  if (m_flashPlacements.size() >= 1000000 || placement.start < 0 ||
      placement.detached || placement.duration < 1 ||
      placement.duration > 20000000 ||
      qint64(placement.start) + placement.duration >
          std::numeric_limits<int>::max())
    throw Flash::Error("Invalid or excessive Flash placement metadata.");
  FlashPlacementData inserted = placement;
  inserted.nativePlacementId =
      QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!placementDigest(*this, inserted, 20000000, inserted.framesSha256))
    throw Flash::Error("Flash placement does not match native exposures.");
  inserted.toJson();
  const int end       = inserted.start + inserted.duration;
  const auto position = std::lower_bound(
      m_flashPlacements.begin(), m_flashPlacements.end(), inserted.start,
      [](const FlashPlacementData &existing, int start) {
        return existing.start < start;
      });
  auto next = position;
  while (next != m_flashPlacements.end() && next->detached) ++next;
  auto previous    = position;
  bool hasPrevious = false;
  while (previous != m_flashPlacements.begin()) {
    --previous;
    if (!previous->detached) {
      hasPrevious = true;
      break;
    }
  }
  if ((next != m_flashPlacements.end() && next->start < end) ||
      (hasPrevious && previous->start + previous->duration > inserted.start))
    throw Flash::Error("Overlapping Flash placement metadata spans.");
  const QString id = inserted.nativePlacementId;
  m_flashPlacements.insert(position, std::move(inserted));
  return id;
}

void TXshLevelColumn::replaceFlashPlacements(
    std::vector<FlashPlacementData> placements) {
  if (placements.size() > 1000000)
    throw Flash::Error("Flash placement metadata budget exceeded.");
  QSet<QString> identities;
  int end           = 0;
  int previousStart = -1;
  for (const auto &placement : placements) {
    placement.toJson();
    if (placement.start < previousStart ||
        identities.contains(placement.nativePlacementId) ||
        (!placement.detached && placement.start < end))
      throw Flash::Error("Invalid Flash placement metadata ordering or ID.");
    if (!placement.detached) end = placement.start + placement.duration;
    previousStart = placement.start;
    identities.insert(placement.nativePlacementId);
  }
  m_flashPlacements = std::move(placements);
}

bool TXshLevelColumn::isFlashPlacementLinked(int index,
                                             quint64 maxCellsToCheck) const {
  if (index < 0 || index >= int(m_flashPlacements.size()) ||
      m_flashPlacements[index].detached)
    return false;
  QString digest;
  return placementDigest(*this, m_flashPlacements[index], maxCellsToCheck,
                         digest) &&
         digest == m_flashPlacements[index].framesSha256;
}

int TXshLevelColumn::flashInstanceIndexAt(int row) const {
  auto position =
      std::upper_bound(m_flashInstances.begin(), m_flashInstances.end(), row,
                       [](int frame, const FlashInstanceData &instance) {
                         return frame < instance.timing.start;
                       });
  if (position == m_flashInstances.begin()) return -1;
  --position;
  if (row >= position->timing.start + position->timing.duration) return -1;
  return int(position - m_flashInstances.begin());
}

bool TXshLevelColumn::flashCellsMatch(const FlashInstanceData &instance,
                                      int frameCount) const {
  if (frameCount < 0) frameCount = instance.symbolFrameCount;
  const auto &timing = instance.timing;
  for (int row = timing.start; row < timing.start + timing.duration; ++row) {
    const TXshCell &cell = TXshCellColumn::getCell(row);
    if (cell.m_level != instance.level ||
        cell.m_frameId != TFrameId(*timing.sourceFrameAt(row, frameCount) + 1))
      return false;
  }
  return true;
}

bool TXshLevelColumn::isFlashInstanceLinked(int index) const {
  if (index < 0 || index >= int(m_flashInstances.size())) return false;
  const auto &instance = m_flashInstances[index];
  auto *child          = instance.level->getChildLevel();
  const int count      = child ? child->getFrameCount() : 0;
  m_flashChecks.resize(m_flashInstances.size());
  auto &check = m_flashChecks[index];
  if (check.revision != m_flashRevision || check.frameCount != count) {
    check.linked =
        count > instance.timing.firstFrame && flashCellsMatch(instance, count);
    check.revision   = m_flashRevision;
    check.frameCount = count;
  }
  return check.linked;
}

bool TXshLevelColumn::materializeFlashInstance(
    int index, const Flash::GraphicTiming &timing, int count, bool honorLock) {
  if ((honorLock && isLocked()) || index < 0 ||
      index >= int(m_flashInstances.size()) || count <= 0)
    return false;
  auto candidate = m_flashInstances[index];
  if (timing.start != candidate.timing.start ||
      timing.duration != candidate.timing.duration)
    throw Flash::Error(
        "Changing a Flash instance span requires a native exposure edit.");
  candidate.timing           = timing;
  candidate.symbolFrameCount = count;
  candidate.toJson();
  std::vector<TXshCell> cells;
  cells.reserve(timing.duration);
  for (int row = timing.start; row < timing.start + timing.duration; ++row)
    cells.emplace_back(candidate.level.getPointer(),
                       TFrameId(*timing.sourceFrameAt(row, count) + 1));
  if (!setCells(timing.start, timing.duration, cells.data())) return false;
  m_flashInstances[index] = std::move(candidate);
  return true;
}

bool TXshLevelColumn::setFlashInstanceTiming(
    int index, const Flash::GraphicTiming &timing) {
  if (!canEditFlashInstance(index)) return false;
  const auto &instance = m_flashInstances[index];
  auto *child          = instance.level->getChildLevel();
  return child &&
         materializeFlashInstance(index, timing, child->getFrameCount());
}

bool TXshLevelColumn::canEditFlashInstance(int index) const {
  if (isLocked() || index < 0 || index >= int(m_flashInstances.size()))
    return false;
  const auto &instance = m_flashInstances[index];
  const auto *child    = instance.level->getChildLevel();
  if (!child || child->getFrameCount() <= 0) return false;
  m_flashChecks.resize(m_flashInstances.size());
  auto &check = m_flashChecks[index];
  if (check.compiledRevision != m_flashRevision ||
      check.compiledFrameCount != instance.symbolFrameCount) {
    check.compiledMatch      = flashCellsMatch(instance);
    check.compiledRevision   = m_flashRevision;
    check.compiledFrameCount = instance.symbolFrameCount;
  }
  return check.compiledMatch;
}

bool TXshLevelColumn::canRefreshFlashInstance(int index) const {
  if (!canEditFlashInstance(index)) return false;
  const auto &instance = m_flashInstances[index];
  const int count      = instance.level->getChildLevel()->getFrameCount();
  return count != instance.symbolFrameCount &&
         count > instance.timing.firstFrame;
}

bool TXshLevelColumn::refreshFlashInstance(int index) {
  if (index < 0 || index >= int(m_flashInstances.size())) return false;
  const auto &instance = m_flashInstances[index];
  auto *child          = instance.level->getChildLevel();
  if (!child || !flashCellsMatch(instance) ||
      child->getFrameCount() <= instance.timing.firstFrame)
    return false;
  return materializeFlashInstance(index, instance.timing,
                                  child->getFrameCount());
}

//-----------------------------------------------------------------------------

TLevelColumnFx *TXshLevelColumn::getLevelColumnFx() const { return m_fx; }

//-----------------------------------------------------------------------------

TFx *TXshLevelColumn::getFx() const { return m_fx; }

//-----------------------------------------------------------------------------

TXshColumn *TXshLevelColumn::clone() const {
  TXshLevelColumn *column = new TXshLevelColumn;
  column->setStatusWord(getStatusWord());
  column->setOpacity(getOpacity());
  column->setMaskScope(getMaskScope());
  column->getOpacityCurve()->copy(getOpacityCurve());
  column->m_cells = m_cells;
  column->m_first = m_first;
  column->m_flashInstances = m_flashInstances;
  for (auto &instance : column->m_flashInstances)
    instance.nativeInstanceId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
  column->m_flashPlacements = m_flashPlacements;
  for (auto &placement : column->m_flashPlacements)
    placement.nativePlacementId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
  column->setColorTag(getColorTag());
  column->setColorFilterId(getColorFilterId());
  column->setFolderIdStack(getFolderIdStack());
  column->setLoops(getLoops());

  // column->updateIcon();
  return column;
}

//-----------------------------------------------------------------------------

void TXshLevelColumn::loadData(TIStream &is) {
  std::string tagName;
  bool readFlashInstances  = false;
  bool readFlashPlacements = false;
  std::vector<FlashInstanceData> loadedFlashInstances;
  std::vector<FlashPlacementData> loadedFlashPlacements;
  m_flashInstances.clear();
  m_flashPlacements.clear();
  m_flashChecks.clear();
  while (is.openChild(tagName)) {
    if (tagName == "status") {
      int status;
      is >> status;
      setStatusWord(status);
      if (status & eCamstandTransparent43) {
        setOpacity(128);
        status = status & ~eCamstandTransparent43;
      }
    } else if (tagName == "camerastand_opacity") {
      int opacity;
      is >> opacity;
      setOpacity((UCHAR)opacity);
    } else if (tagName == "filter_color_id") {
      int id;
      is >> id;
      setColorFilterId(id);
    } else if (tagName == "mask_scope") {
      int scope = 1;
      is >> scope;
      setMaskScope(scope);
    } else if (tagName == "opacity_curve") {
      is >> *getOpacityCurve();
    } else if (tagName == "cells") {
      while (is.openChild(tagName)) {
        if (tagName == "cell") {
          TPersist *p = 0;
          std::string str;
          int row = 1, rowCount = 1, increment = 0;
          TFilePath path;
          is >> row >> rowCount >> p >> str >> increment;
          TFrameId fid = qstringToFrameId(QString::fromStdString(str));
          assert((fid.getLetter().isEmpty() && rowCount >= 0) ||
                 (!fid.getLetter().isEmpty() && rowCount == 1));
          TXshLevel *xshLevel = dynamic_cast<TXshLevel *>(p);
          if (xshLevel) {
            int fidNumber = fid.getNumber();
            for (int i = 0; i < rowCount; i++) {
              TXshCell cell(xshLevel, fid);
              setCell(row++, cell);
              // rowCount>1 => fid has not letter.
              fidNumber += increment;
              fid = TFrameId(fidNumber);
            }
          }
        } else
          throw TException("TXshLevelColumn, unknown tag(2): " + tagName);
        is.closeChild();
      }
    } else if (tagName == "flashInstances") {
      if (readFlashInstances || is.getTagAttribute("version") != "1")
        throw TException("Unsupported or repeated Flash instance metadata.");
      readFlashInstances = true;
      while (is.openChild(tagName)) {
        if (tagName != "instance")
          throw TException("Unknown Flash instance metadata tag: " + tagName);
        // Preserve UTF-8 bytes: the native QString reader appends individual
        // bytes as QChars and cannot round-trip this Unicode JSON payload.
        std::string encoded;
        TPersist *persist = nullptr;
        is >> encoded >> persist;
        if (encoded.size() > 1024 * 1024)
          throw TException("Flash instance metadata record is too large.");
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(
            QByteArray::fromStdString(encoded), &parseError);
        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject())
          throw TException("Invalid Flash instance metadata JSON.");
        try {
          loadedFlashInstances.push_back(FlashInstanceData::fromJson(
              document.object(), dynamic_cast<TXshLevel *>(persist)));
        } catch (const Flash::Error &error) {
          throw TException(std::string("Invalid Flash instance metadata: ") +
                           error.what());
        }
        is.closeChild();
      }
      try {
        replaceFlashInstances(std::move(loadedFlashInstances));
      } catch (const Flash::Error &error) {
        throw TException(std::string("Invalid Flash instance metadata: ") +
                         error.what());
      }
    } else if (tagName == "flashPlacements") {
      if (readFlashPlacements || is.getTagAttribute("version") != "1")
        throw TException("Unsupported or repeated Flash placement metadata.");
      readFlashPlacements = true;
      while (is.openChild(tagName)) {
        if (tagName != "placement")
          throw TException("Unknown Flash placement metadata tag: " + tagName);
        std::string encoded;
        TPersist *persist = nullptr;
        is >> encoded >> persist;
        if (encoded.size() > 1024 * 1024)
          throw TException("Flash placement metadata record is too large.");
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(
            QByteArray::fromStdString(encoded), &parseError);
        if (parseError.error != QJsonParseError::NoError ||
            !document.isObject())
          throw TException("Invalid Flash placement metadata JSON.");
        try {
          loadedFlashPlacements.push_back(FlashPlacementData::fromJson(
              document.object(), dynamic_cast<TXshLevel *>(persist)));
        } catch (const Flash::Error &error) {
          throw TException(std::string("Invalid Flash placement metadata: ") +
                           error.what());
        }
        is.closeChild();
      }
      try {
        replaceFlashPlacements(std::move(loadedFlashPlacements));
      } catch (const Flash::Error &error) {
        throw TException(std::string("Invalid Flash placement metadata: ") +
                         error.what());
      }
    } else if (tagName == "fx") {
      TPersist *p = 0;
      is >> p;
      if (TLevelColumnFx *lcf = dynamic_cast<TLevelColumnFx *>(p)) {
        lcf->addRef();
        if (m_fx) m_fx->release();
        m_fx = lcf;
        lcf->setColumn(this);
      }
    } else if (tagName == "fxnodes")  // per compatibilita' con 1.x e precedenti
    {
      TFxSet fxSet;
      fxSet.loadData(is);
    } else if (loadCellMarks(tagName, is)) {
      // do nothing
    } else if (loadFolderInfo(tagName, is)) {
      // do nothing
    } else if (loadLoopInfo(tagName, is)) {
      // do nothing
    } else
      throw TException("TXshLevelColumn, unknown tag: " + tagName);
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshLevelColumn::saveData(TOStream &os) {
  os.child("status") << getStatusWord();
  if (getOpacity() < 255) os.child("camerastand_opacity") << (int)getOpacity();
  if (getColorFilterId() != 0)
    os.child("filter_color_id") << (int)getColorFilterId();
  // Written only when used, so ordinary scenes keep the stock format. Older
  // builds reject these tags ("unknown tag") instead of misreading them.
  if (getMaskScope() != 1) os.child("mask_scope") << getMaskScope();
  if (getOpacityCurve()->hasKeyframes() ||
      getOpacityCurve()->getDefaultValue() != 1.0)
    os.child("opacity_curve") << *getOpacityCurve();

  int r0, r1;
  if (getRange(r0, r1)) {
    os.openChild("cells");
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = getCell(r, false, false);
      if (cell.isEmpty()) continue;
      TFrameId fid = cell.m_frameId;
      int n = 1, inc = 0, dr = fid.getNumber();
      // If fid has not letter save more than one cell and its incrementation;
      // otherwise save one cell.
      if (r < r1 && fid.getLetter().isEmpty()) {
        TXshCell cell2 = getCell(r + 1, false, false);
        TFrameId fid2  = cell2.m_frameId;
        if (cell2.m_level.getPointer() == cell.m_level.getPointer() &&
            fid2.getLetter().isEmpty()) {
          inc = cell2.m_frameId.getNumber() - dr;
          n++;
          for (;;) {
            if (r + n > r1) break;
            cell2         = getCell(r + n, false, false);
            TFrameId fid2 = cell2.m_frameId;
            if (cell2.m_level.getPointer() != cell.m_level.getPointer() ||
                !fid2.getLetter().isEmpty())
              break;
            if (fid2.getNumber() != dr + n * inc) break;
            n++;
          }
        }
      }
      os.child("cell") << r << n << cell.m_level.getPointer() << fid.expand()
                       << inc;
      r += n - 1;
    }
    os.closeChild();
  }

  if (!m_flashInstances.empty()) {
    os.openChild("flashInstances", {{"version", "1"}});
    for (int index = 0; index < int(m_flashInstances.size()); ++index) {
      const auto &instance = m_flashInstances[index];
      auto json            = instance.toJson();
      json["linked"]       = isFlashInstanceLinked(index);
      os.child("instance")
          << QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString()
          << instance.level.getPointer();
    }
    os.closeChild();
  }
  if (!m_flashPlacements.empty()) {
    os.openChild("flashPlacements", {{"version", "1"}});
    for (const auto &placement : m_flashPlacements)
      os.child("placement") << QJsonDocument(placement.toJson())
                                   .toJson(QJsonDocument::Compact)
                                   .toStdString()
                            << placement.level.getPointer();
    os.closeChild();
  }
  os.child("fx") << m_fx;

  // cell marks
  saveCellMarks(os);
  // folder info
  saveFolderInfo(os);
  // Loop info
  saveLoopInfo(os);
}

//-----------------------------------------------------------------------------
// Used in TCellData::getNumbers
bool TXshLevelColumn::setNumbers(int row, int rowCount,
                                 const TXshCell cells[]) {
  if (m_cells.empty()) return false;
  // Check availability.
  // - if source cells are all empty, do nothing
  // - also, if source or target cells contain NO_FRAME, do nothing
  bool isSrcAllEmpty = true;
  for (int i = 0; i < rowCount; i++) {
    // checking target cells
    int currentTgtIndex = row + i - m_first;
    if (currentTgtIndex < m_cells.size()) {
      TXshCell tgtCell = m_cells[currentTgtIndex];
      if (!tgtCell.isEmpty() && tgtCell.m_frameId == TFrameId::NO_FRAME)
        return false;
    }
    // checking source cells
    TXshCell srcCell = cells[i];
    if (!srcCell.isEmpty()) {
      if (srcCell.m_frameId == TFrameId::NO_FRAME) return false;
      isSrcAllEmpty = false;
    }
  }
  if (isSrcAllEmpty) return false;

  // Find a level to input.
  // If the first target cell is empty, search the upper cells, and lower cells
  // and use a level of firsty-found occupied neighbor cell.
  TXshLevelP currentLevel;
  int tmpIndex = std::min(row - m_first, (int)m_cells.size() - 1);
  // search upper cells
  while (tmpIndex >= 0) {
    TXshCell tmpCell = m_cells[tmpIndex];
    if (!tmpCell.isEmpty() && tmpCell.m_frameId != TFrameId::NO_FRAME) {
      currentLevel = tmpCell.m_level;
      break;
    }
    tmpIndex--;
  }
  // if not found any level in upper cells, then search the lower cells
  if (!currentLevel) {
    tmpIndex = std::max(row - m_first, 0);
    while (tmpIndex < (int)m_cells.size()) {
      TXshCell tmpCell = m_cells[tmpIndex];
      if (!tmpCell.isEmpty() && tmpCell.m_frameId != TFrameId::NO_FRAME) {
        currentLevel = tmpCell.m_level;
        break;
      }
      tmpIndex++;
    }
    // in the case any level for input could not be found
    if (!currentLevel) return false;
  }

  // Resize the cell container
  int ra   = row;
  int rb   = row + rowCount - 1;
  int c_rb = m_first + m_cells.size() - 1;
  if (row > c_rb) {
    int newCellCount = row - m_first + rowCount;
    m_cells.resize(newCellCount);
  } else if (row < m_first) {
    int delta = m_first - row;
    m_cells.insert(m_cells.begin(), delta, TXshCell());
    m_first = row;
  }
  if (rb > c_rb) {
    for (int i = 0; i < rb - c_rb; ++i) m_cells.push_back(TXshCell());
  }

  // Paste numbers.
  for (int i = 0; i < rowCount; i++) {
    int dstIndex     = row - m_first + i;
    TXshCell dstCell = m_cells[dstIndex];
    TXshCell srcCell = cells[i];
    if (srcCell.isEmpty()) {
      m_cells[dstIndex] = TXshCell();
    } else {
      if (!dstCell.isEmpty()) currentLevel = dstCell.m_level;
      m_cells[dstIndex] = TXshCell(currentLevel, srcCell.m_frameId);
    }
  }

  flashCellsChanged();
  // Update the cell container
  while (!m_cells.empty() && m_cells.back().isEmpty()) {
    m_cells.pop_back();
  }
  while (!m_cells.empty() && m_cells.front().isEmpty()) {
    m_cells.erase(m_cells.begin());
    m_first++;
  }
  if (m_cells.empty()) {
    m_first = 0;
  }
  return true;
}

//-----------------------------------------------------------------------------

std::vector<TXshColumn *> TXshLevelColumn::getColumnMasks(int frame) {
  std::vector<TXshColumn *> masks;

  if (m_index < 0) return masks;

  TXsheet *xsh    = getXsheet();
  TXshColumn *col = xsh->getColumn(m_index);

  if (!col) return masks;

  return col->getColumnMasks(frame);
}

//-----------------------------------------------------------------------------

PERSIST_IDENTIFIER(TXshLevelColumn, "levelColumn")
