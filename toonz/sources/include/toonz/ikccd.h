#pragma once

#ifndef IKCCD_H
#define IKCCD_H

#include "tgeometry.h"

#include <optional>
#include <vector>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// SuperPlastic shared IK core (task 58).
//
// Solver-only module: no dependency on stage objects, levels or meshes.
// The Skeleton Tool adapter (level transform chains) and the Plastic Tool
// adapter (mesh vertex chains) both build an IKChain, call SolveIK_CCD and
// write the resulting joint positions back into their own data model.
//
// The chain is always re-solved from scratch against the current target:
// a pin is just a target that has no keyframes of its own in a frame range,
// NOT a placement inherited/interpolated from the previous frame (which is
// the source of the historical pin drift bug in skeletonsubtools.cpp).
//=============================================================================

struct IKBone {
  double length;

  // Rotation limits relative to the parent bone direction, in radians.
  // NaN on either side means "no limit" on that side.
  double angleMin, angleMax;

  // -1 for the root bone. NOTE: the CCD solver only handles serial chains,
  // so it requires parentIndex == boneIndex - 1; the field exists so that
  // adapters can carry sub-chains extracted from branched skeletons without
  // losing the original topology.
  int parentIndex;
};

struct IKChain {
  std::vector<IKBone> bones;

  // Joint positions, size() == bones.size() + 1; [0] is the chain root,
  // which stays fixed during the solve. Used both as the solve starting
  // pose (CCD converges to the solution nearest to it) and to derive the
  // initial bone angles.
  std::vector<TPointD> currentPositions;
};

struct IKTarget {
  TPointD position;

  // Optional disambiguation point: when the chain admits two mirrored
  // solutions (e.g. an elbow that can bend either way), the solution whose
  // bend lies on the same side as the pole vector is preferred.
  std::optional<TPointD> poleVector;

  // Semantic flag for the adapters (a pinned target must stay fixed until
  // the next pin keyframe); the solver itself treats every target the same.
  bool isPinned = false;
};

// Diagnostics returned to the caller: the no-pole ambiguous case must be
// surfaced in the UI/log, not silently resolved (see SUPERPLASTIC.md).
struct IKSolveInfo {
  bool converged            = false;
  int iterationsUsed        = 0;
  bool ambiguousWithoutPole = false;
  double effectorError      = 0.0;
};

// Solves the chain from scratch against the target with Cyclic Coordinate
// Descent, clamping each bone to its angular limits after every rotation.
// Returns the new joint positions (same layout as chain.currentPositions).
DVAPI std::vector<TPointD> SolveIK_CCD(const IKChain &chain,
                                       const IKTarget &target,
                                       int maxIterations   = 10,
                                       double tolerance    = 0.01,
                                       IKSolveInfo *info   = nullptr);

#endif  // IKCCD_H
