#pragma once

#ifndef REPLICAMAP_H
#define REPLICAMAP_H

// The OpenToonz replicators (mirror, star, grid, affine, jitter) on the
// Tahoma2D brushes (experiment/ot-assistants).
//
// The Tahoma brushes already paint several strokes at once for the Symmetry
// tool: they ask SymmetryTool::getSymmetryPoints for the copies of every
// point, and saving and undo handle the extra strokes.  While a ReplicaMap
// is active that function answers with the replicators' copies instead.
//
// The map is a snapshot taken when the stroke starts (the replicators of the
// current frame, cloned with their placement), so the two Smart Raster undos
// that repaint the stroke on redo get the same copies again.

#include "tgeometry.h"
#include "tmetaimage.h"

#include <memory>
#include <utility>
#include <vector>

class TTool;

class ReplicaMap {
  std::vector<std::pair<TMetaObjectP, TAffine>> m_replicators;
  int m_count = 1;

public:
  // Copies of one point, the point itself first.
  int count() const { return m_count; }
  void apply(std::vector<TPointD> &points) const;

  // Snapshot of the replicators visible to `tool`; null when there are none
  // or they make a single copy.
  static std::shared_ptr<const ReplicaMap> capture(TTool *tool);

  // The map getSymmetryPoints uses, null when none is.
  static const ReplicaMap *active();
  static const std::shared_ptr<const ReplicaMap> &activeShared();

  // A brush starts a stroke: returns the number of strokes to paint (0 when
  // no replicator applies).  end() when the stroke is done.
  static int begin(TTool *tool);
  static void end();

  // Ends the stroke's map when the scope closes (leftButtonUp).
  class Guard {
  public:
    ~Guard() { end(); }
  };

  // Makes `map` active for a scope (an undo repainting its stroke).
  class Scope {
    std::shared_ptr<const ReplicaMap> m_previous;

  public:
    explicit Scope(const std::shared_ptr<const ReplicaMap> &map);
    ~Scope();
  };
};

#endif
