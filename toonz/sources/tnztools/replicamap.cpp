#include "replicamap.h"

// TnzTools includes
#include "tools/replicator.h"
#include "tools/tool.h"
#include "symmetrytool.h"

// TnzLib includes
#include "toonz/tapplication.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tframehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshcolumn.h"
#include "toonz/dpiscale.h"

#include <algorithm>

namespace {
std::shared_ptr<const ReplicaMap> s_active;
}

//-----------------------------------------------------------------------------

void ReplicaMap::apply(std::vector<TPointD> &points) const {
  for (const auto &item : m_replicators)
    if (const TReplicator *replicator =
            item.first->getHandler<TReplicator>())
      replicator->getPoints(item.second, points);
}

//-----------------------------------------------------------------------------

// Same walk as TReplicator::scanReplicators, keeping clones instead of
// applying them on the spot.
std::shared_ptr<const ReplicaMap> ReplicaMap::capture(TTool *tool) {
  if (!tool) return nullptr;
  TTool::Application *application = tool->getApplication();
  if (!application) return nullptr;
  TXshSimpleLevel *simpleLevel =
      application->getCurrentLevel()->getSimpleLevel();
  TXsheet *xsheet = application->getCurrentXsheet()->getXsheet();
  if (!simpleLevel || !xsheet) return nullptr;

  TPointD dpiScale = getCurrentDpiScale(simpleLevel, tool->getCurrentFid());
  int frame        = application->getCurrentFrame()->getFrame();
  TAffine worldToTrack;
  if (tool->getToolType() & TTool::LevelTool &&
      !application->getCurrentObject()->isSpline()) {
    worldToTrack.a11 /= dpiScale.x;
    worldToTrack.a22 /= dpiScale.y;
  }

  std::shared_ptr<ReplicaMap> map(new ReplicaMap());
  long long multiplier = 1;
  for (int col = 0; col < xsheet->getColumnCount(); ++col) {
    TXshColumn *column = xsheet->getColumn(col);
    if (!column || !column->isCamstandVisible() || !column->isPreviewVisible())
      continue;
    TImageP image = xsheet->getCell(frame, col).getImage(false);
    TMetaImage *metaImage =
        image ? dynamic_cast<TMetaImage *>(image.getPointer()) : nullptr;
    if (!metaImage) continue;
    TAffine imageToTrack = worldToTrack * tool->getColumnMatrix(col);

    TMetaImage::Reader reader(*metaImage);
    for (TMetaObjectListCW::iterator i = reader->begin(); i != reader->end();
         ++i) {
      if (!*i) continue;
      const TReplicator *replicator = (*i)->getHandler<TReplicator>();
      if (!replicator || !replicator->getEnabled()) continue;
      int m = std::max(1, replicator->getMultipler());
      if (multiplier * m > TReplicator::multiplierLimit) continue;
      multiplier *= m;
      map->m_replicators.push_back(
          std::make_pair(TMetaObjectP((*i)->clone()), imageToTrack));
    }
  }
  if (map->m_replicators.empty()) return nullptr;

  // the brushes hold at most MAX_SYMMETRY strokes
  std::vector<TPointD> probe(1, TPointD());
  map->apply(probe);
  map->m_count = std::min((int)probe.size(), MAX_SYMMETRY);
  if (map->m_count <= 1) return nullptr;
  return map;
}

//-----------------------------------------------------------------------------

const ReplicaMap *ReplicaMap::active() { return s_active.get(); }

const std::shared_ptr<const ReplicaMap> &ReplicaMap::activeShared() {
  return s_active;
}

//-----------------------------------------------------------------------------

int ReplicaMap::begin(TTool *tool) {
  s_active = capture(tool);
  return s_active ? s_active->count() : 0;
}

void ReplicaMap::end() { s_active.reset(); }

//-----------------------------------------------------------------------------

ReplicaMap::Scope::Scope(const std::shared_ptr<const ReplicaMap> &map)
    : m_previous(s_active) {
  s_active = map;
}

ReplicaMap::Scope::~Scope() { s_active = m_previous; }
