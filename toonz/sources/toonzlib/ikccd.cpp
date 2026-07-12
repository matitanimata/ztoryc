#include "toonz/ikccd.h"

#include <cassert>
#include <cmath>

//=============================================================================
// SuperPlastic shared IK core — CCD implementation (task 58).
//
// The solve works in relative angle space: theta[i] is the rotation of bone i
// with respect to its parent bone direction (bone 0 with respect to world +x).
// This makes the per-bone angular limits a plain clamp on theta[i], which is
// the reason CCD was chosen over FABRIK (limits are awkward there).
//=============================================================================

namespace {

const double IK_EPS = 1e-9;

double normalizeAngle(double a) {
  while (a > M_PI) a -= 2.0 * M_PI;
  while (a < -M_PI) a += 2.0 * M_PI;
  return a;
}

double cross2D(const TPointD &a, const TPointD &b) {
  return a.x * b.y - a.y * b.x;
}

// Relative bone angles derived from the joint positions; degenerate
// (zero-length) segments keep the parent direction so they don't inject
// garbage angles into the solve.
std::vector<double> anglesFromPositions(const std::vector<TPointD> &pos) {
  std::vector<double> theta(pos.size() - 1, 0.0);
  double prevAbs = 0.0;
  for (int i = 0; i < (int)theta.size(); ++i) {
    TPointD d  = pos[i + 1] - pos[i];
    double abs = (norm2(d) > IK_EPS * IK_EPS) ? atan2(d.y, d.x) : prevAbs;
    theta[i]   = (i == 0) ? abs : normalizeAngle(abs - prevAbs);
    prevAbs    = abs;
  }
  return theta;
}

std::vector<TPointD> positionsFromAngles(const TPointD &root,
                                         const std::vector<double> &theta,
                                         const std::vector<double> &length) {
  std::vector<TPointD> pos(theta.size() + 1);
  pos[0]     = root;
  double abs = 0.0;
  for (int i = 0; i < (int)theta.size(); ++i) {
    abs += theta[i];
    pos[i + 1] = pos[i] + TPointD(length[i] * cos(abs), length[i] * sin(abs));
  }
  return pos;
}

void clampToBoneLimits(double &theta, const IKBone &bone) {
  if (!std::isnan(bone.angleMin) && theta < bone.angleMin)
    theta = bone.angleMin;
  if (!std::isnan(bone.angleMax) && theta > bone.angleMax)
    theta = bone.angleMax;
}

// One full CCD sweep, tip to root: rotate each joint so the effector heads
// toward the target, clamping every bone right after its rotation.
void ccdSweep(std::vector<double> &theta, const std::vector<double> &length,
              const TPointD &root, const IKChain &chain,
              const TPointD &target) {
  int n = (int)theta.size();
  for (int j = n - 1; j >= 0; --j) {
    std::vector<TPointD> pos = positionsFromAngles(root, theta, length);
    TPointD toEff            = pos[n] - pos[j];
    TPointD toTarget         = target - pos[j];
    if (norm2(toEff) < IK_EPS * IK_EPS || norm2(toTarget) < IK_EPS * IK_EPS)
      continue;
    double delta =
        normalizeAngle(atan2(toTarget.y, toTarget.x) - atan2(toEff.y, toEff.x));
    theta[j] = normalizeAngle(theta[j] + delta);
    clampToBoneLimits(theta[j], chain.bones[j]);
  }
}

// Runs CCD to convergence from the given starting angles. preferredSide
// (sign of the pole vector side, 0 = none) picks the escape direction when
// the chain stalls on a singular pose: with root, joints and target all
// collinear every CCD rotation is locally optimal at zero, so the sweep
// can't bend the chain — kicking the root bone breaks the alignment.
std::vector<TPointD> solveFrom(std::vector<double> theta,
                               const std::vector<double> &length,
                               const TPointD &root, const IKChain &chain,
                               const TPointD &target, int maxIterations,
                               double tolerance, double preferredSide,
                               IKSolveInfo &outInfo) {
  std::vector<TPointD> pos = positionsFromAngles(root, theta, length);
  double prevError         = norm(pos.back() - target);
  int stallCount           = 0;
  int iter                 = 0;
  for (; iter < maxIterations; ++iter) {
    if (prevError <= tolerance) break;
    ccdSweep(theta, length, root, chain, target);
    pos          = positionsFromAngles(root, theta, length);
    double error = norm(pos.back() - target);
    if (error > tolerance && prevError - error < tolerance * 1e-3) {
      ++stallCount;
      double sign = (preferredSide < 0.0) ? -1.0 : 1.0;
      theta[0] = normalizeAngle(theta[0] + sign * 0.3 * stallCount);
      clampToBoneLimits(theta[0], chain.bones[0]);
      pos   = positionsFromAngles(root, theta, length);
      error = norm(pos.back() - target);
    }
    prevError = error;
  }
  outInfo.iterationsUsed = iter;
  outInfo.effectorError  = norm(pos.back() - target);
  outInfo.converged      = (outInfo.effectorError <= tolerance);
  return pos;
}

// Signed side (vs the root->target line) of the interior joint that deviates
// most from that line: this identifies which way the chain is bending.
double bendSide(const std::vector<TPointD> &pos, const TPointD &target) {
  TPointD axis = target - pos[0];
  double best = 0.0, bestAbs = 0.0;
  for (int i = 1; i < (int)pos.size() - 1; ++i) {
    double s = cross2D(axis, pos[i] - pos[0]);
    if (fabs(s) > bestAbs) bestAbs = fabs(s), best = s;
  }
  return best;
}

// Starting pose mirrored across the root->target line: re-solving from here
// makes CCD converge onto the opposite-bend solution.
std::vector<TPointD> mirrorAcrossAxis(const std::vector<TPointD> &pos,
                                      const TPointD &target) {
  TPointD axis = target - pos[0];
  double n2    = norm2(axis);
  if (n2 < IK_EPS * IK_EPS) return pos;
  std::vector<TPointD> out(pos.size());
  for (int i = 0; i < (int)pos.size(); ++i) {
    TPointD d = pos[i] - pos[0];
    double t  = (d.x * axis.x + d.y * axis.y) / n2;
    TPointD p = TPointD(axis.x * t, axis.y * t);  // projection on the axis
    out[i]    = pos[0] + p + (p - d);
  }
  return out;
}

}  // namespace

std::vector<TPointD> SolveIK_CCD(const IKChain &chain, const IKTarget &target,
                                 int maxIterations, double tolerance,
                                 IKSolveInfo *info) {
  IKSolveInfo localInfo;
  IKSolveInfo &out = info ? *info : localInfo;
  out              = IKSolveInfo();

  int n = (int)chain.bones.size();
  if (n == 0 || (int)chain.currentPositions.size() != n + 1) {
    assert(n > 0 && (int)chain.currentPositions.size() == n + 1);
    return chain.currentPositions;
  }

  std::vector<double> length(n);
  double totalLength = 0.0;
  for (int i = 0; i < n; ++i) {
    assert(chain.bones[i].parentIndex == i - 1);  // serial chains only
    length[i] = (chain.bones[i].length > IK_EPS)
                    ? chain.bones[i].length
                    : norm(chain.currentPositions[i + 1] -
                           chain.currentPositions[i]);
    totalLength += length[i];
  }

  const TPointD root   = chain.currentPositions[0];
  double preferredSide =
      target.poleVector
          ? cross2D(target.position - root, *target.poleVector - root)
          : 0.0;
  std::vector<double> theta = anglesFromPositions(chain.currentPositions);
  std::vector<TPointD> solved =
      solveFrom(theta, length, root, chain, target.position, maxIterations,
                tolerance, preferredSide, out);

  // A reachable target with >= 2 bones admits mirrored solutions: without a
  // pole vector CCD keeps the bend nearest to the starting pose, but the
  // ambiguity must be reported so the UI can surface it.
  bool ambiguous = (n >= 2 && norm(target.position - root) <
                                  totalLength - tolerance);
  if (!target.poleVector) {
    out.ambiguousWithoutPole = ambiguous;
    return solved;
  }

  if (ambiguous) {
    double wantSide = preferredSide;
    double haveSide = bendSide(solved, target.position);
    if (wantSide * haveSide < 0.0) {
      // Re-solve from the mirrored pose; keep it only if the limits still
      // allow reaching the target at least as well as the original bend.
      IKSolveInfo mirroredInfo;
      std::vector<TPointD> mirroredStart =
          mirrorAcrossAxis(chain.currentPositions, target.position);
      std::vector<TPointD> mirroredSolved = solveFrom(
          anglesFromPositions(mirroredStart), length, root, chain,
          target.position, maxIterations, tolerance, preferredSide,
          mirroredInfo);
      if (mirroredInfo.effectorError <= out.effectorError + tolerance) {
        out    = mirroredInfo;
        solved = mirroredSolved;
      }
    }
  }
  return solved;
}
