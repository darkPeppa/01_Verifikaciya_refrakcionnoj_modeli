// Focus determination: three independent methods.
//  A  intersection of symmetric paraxial ray pairs at heights h and 2h, with
//     Richardson extrapolation h -> 0 (removes the O(h^2) aberration term);
//  B  minimum of the RMS beam width (closed form for straight rays after the lens);
//  C  paraxial transfer-matrix model of the real element sequence.
// Rays after the exit plane travel in vacuum, so all downstream quantities are
// obtained by exact straight-line extrapolation (no need for a world of hundreds
// of metres for long-focus single elements).
#ifndef XRCL_FOCUS_ANALYSIS_HH
#define XRCL_FOCUS_ANALYSIS_HH

#include "LensLayout.hh"
#include "RayStore.hh"

#include <limits>
#include <vector>

namespace xrcl {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct ParaxialAxis {
  bool valid = false;
  int nFocusing = 0;
  double A = 1, B = 0, C = 0, D = 1;  // (u, theta) matrix, first entrance vertex -> last exit vertex
  double zFirstVertex = 0, zLastVertex = 0;
  double bflFromLastVertex = kNaN;    // back focal distance from the last surface vertex
  double efl = kNaN;                  // effective focal length -1/C
  double zFocus = kNaN, zPrincipal = kNaN;
};

// Paraxial model with refraction at every surface (incl. the n = 1 - delta inside
// the polymer and the non-focusing surfaces) and free propagation between them.
ParaxialAxis ComputeParaxial(const LensLayout& layout, FocusAxis axis, double delta);

struct PairFocus {
  bool valid = false;
  double h = 0;            // ray height at the source plane
  double zCross = kNaN;    // global z where the pair intersects
  double lateral = kNaN;   // transverse coordinate of the intersection
  double efl = kNaN;       // -2h / (t+ - t-), t = slope after the lens
};

struct AxisFocus {
  // Method A
  std::vector<PairFocus> pairs;
  double zParaxial = kNaN;    // Richardson-extrapolated pair intersection
  double eflParaxial = kNaN;
  // Method B
  int nRms = 0;
  double zRmsMin = kNaN, rmsMin = kNaN, rmsAtExit = kNaN;
  // non-focusing check: max change of the slope in this axis over all valid rays
  double maxSlopeChange = 0.;
};

struct FocusResult {
  AxisFocus x, y;
  double zExit = 0;
};

FocusResult AnalyseFocus(const std::vector<RayRecord>& rays, double zExitPlane, double sourceOffsetX,
                         double sourceOffsetY);

// RMS width of the valid rays at plane z (straight-line extrapolation from the exit plane).
void RmsAt(const std::vector<RayRecord>& rays, double z, double& rmsX, double& rmsY);

}  // namespace xrcl

#endif
