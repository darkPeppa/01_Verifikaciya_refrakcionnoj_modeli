#include "ParabolicLensSolid.hh"

#include "G4AffineTransform.hh"
#include "G4BoundingEnvelope.hh"
#include "G4ExtrudedSolid.hh"
#include "G4Polyhedron.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4Transform3D.hh"
#include "G4VGraphicsScene.hh"
#include "Randomize.hh"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace xrcl {

namespace {

// Numerically stable real roots of a t^2 + b t + c = 0 (a may be 0).
G4int SolveQuadratic(G4double a, G4double b, G4double c, G4double* r)
{
  if (a == 0.) {
    if (b == 0.) return 0;
    r[0] = -c / b;
    return 1;
  }
  const G4double disc = b * b - 4. * a * c;
  if (disc < 0.) return 0;
  const G4double q = -0.5 * (b + std::copysign(std::sqrt(disc), b));
  if (q == 0.) {  // b == 0 and c == 0
    r[0] = 0.;
    return 1;
  }
  r[0] = q / a;
  r[1] = c / q;
  return 2;
}

G4double ArcLength(G4double a, G4double R)
{
  // integral_{-a}^{a} sqrt(1 + x^2/R^2) dx
  const G4double s = a / R;
  return a * std::sqrt(1. + s * s) + R * std::asinh(s);
}

}  // namespace

G4double ParabolicLensSolid::ParabolicHalfAperture(const Parameters& par)
{
  return std::sqrt(par.radius * (par.length - par.web));
}

ParabolicLensSolid::ParabolicLensSolid(const G4String& name, const Parameters& par)
  : G4VSolid(name), fPar(par)
{
  if (par.radius <= 0 || par.web <= 0 || par.length <= par.web || par.halfHeight <= 0 || par.frame < 0) {
    std::ostringstream os;
    os << "ParabolicLensSolid " << name << ": invalid parameters R=" << par.radius << " d=" << par.web
       << " p=" << par.length << " frame=" << par.frame << " halfHeight=" << par.halfHeight;
    throw std::invalid_argument(os.str());
  }
  fR = par.radius;
  fWeb = par.web;
  fP2 = 0.5 * par.length;
  fA = ParabolicHalfAperture(par);
  fX = fA + par.frame;
  fY = par.halfHeight;
  fHalfTol = 0.5 * kCarTolerance;
}

ParabolicLensSolid::ParabolicLensSolid(const ParabolicLensSolid& rhs)
  : G4VSolid(rhs), fPar(rhs.fPar), fR(rhs.fR), fWeb(rhs.fWeb), fP2(rhs.fP2), fA(rhs.fA), fX(rhs.fX), fY(rhs.fY),
    fHalfTol(rhs.fHalfTol)
{}

ParabolicLensSolid::~ParabolicLensSolid() { delete fPolyhedron; }

G4double ParabolicLensSolid::MaxConstraint(const G4ThreeVector& p) const
{
  const G4double x = p.x();
  const G4double gx = std::abs(x) - fX;
  const G4double gy = std::abs(p.y()) - fY;
  const G4double gz = std::abs(p.z()) - fP2;
  const G4double slope = x / fR;
  const G4double gp = (std::abs(p.z()) - Sag(x)) / std::sqrt(1. + slope * slope);
  return std::max({gx, gy, gz, gp});
}

EInside ParabolicLensSolid::Inside(const G4ThreeVector& p) const
{
  const G4double g = MaxConstraint(p);
  if (g > fHalfTol) return kOutside;
  if (g > -fHalfTol) return kSurface;
  return kInside;
}

G4ThreeVector ParabolicLensSolid::SurfaceNormal(const G4ThreeVector& p) const
{
  const G4double x = p.x(), y = p.y(), z = p.z();
  const G4double sx = (x >= 0) ? 1. : -1.;
  const G4double sy = (y >= 0) ? 1. : -1.;
  const G4double sz = (z >= 0) ? 1. : -1.;
  const G4double slope = x / fR;
  const G4double norm = std::sqrt(1. + slope * slope);

  const G4double g[4] = {std::abs(x) - fX, std::abs(y) - fY, std::abs(z) - fP2, (std::abs(z) - Sag(x)) / norm};
  const G4ThreeVector n[4] = {G4ThreeVector(sx, 0, 0), G4ThreeVector(0, sy, 0), G4ThreeVector(0, 0, sz),
                              G4ThreeVector(-slope / norm, 0, sz / norm)};
  G4ThreeVector sum;
  G4int count = 0;
  for (G4int i = 0; i < 4; ++i) {
    if (std::abs(g[i]) <= fHalfTol) {
      sum += n[i];
      ++count;
    }
  }
  if (count == 1) return sum;
  if (count > 1) return sum.unit();
  // Not on the surface: normal of the nearest (largest) constraint.
  G4int imax = 0;
  for (G4int i = 1; i < 4; ++i)
    if (g[i] > g[imax]) imax = i;
  return n[imax];
}

G4int ParabolicLensSolid::Candidates(const G4ThreeVector& p, const G4ThreeVector& v, G4double* t) const
{
  G4int n = 0;
  auto add = [&](G4double tt) {
    if (tt > fHalfTol) t[n++] = tt;
  };
  if (v.x() != 0.) {
    add((fX - p.x()) / v.x());
    add((-fX - p.x()) / v.x());
  }
  if (v.y() != 0.) {
    add((fY - p.y()) / v.y());
    add((-fY - p.y()) / v.y());
  }
  if (v.z() != 0.) {
    add((fP2 - p.z()) / v.z());
    add((-fP2 - p.z()) / v.z());
  }
  G4double r[2];
  const G4double qa = 0.5 * v.x() * v.x() / fR;
  const G4double qbx = p.x() * v.x() / fR;
  const G4double qc = 0.5 * fWeb + 0.5 * p.x() * p.x() / fR;
  // exit surface  z - s(x) = 0
  G4int nr = SolveQuadratic(-qa, v.z() - qbx, p.z() - qc, r);
  for (G4int i = 0; i < nr; ++i) add(r[i]);
  // entrance surface  z + s(x) = 0
  nr = SolveQuadratic(qa, v.z() + qbx, p.z() + qc, r);
  for (G4int i = 0; i < nr; ++i) add(r[i]);
  std::sort(t, t + n);
  return n;
}

G4double ParabolicLensSolid::DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v) const
{
  G4double t[10];
  const G4int n = Candidates(p, v, t);
  G4double prev = 0.;
  for (G4int i = 0; i < n; ++i) {
    if (Inside(p + 0.5 * (prev + t[i]) * v) == kInside) return prev;
    prev = t[i];
  }
  return kInfinity;
}

G4double ParabolicLensSolid::DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v, const G4bool calcNorm,
                                           G4bool* validNorm, G4ThreeVector* n) const
{
  G4double t[10];
  const G4int nc = Candidates(p, v, t);
  G4double prev = 0.;
  G4double exitDist = 0.;
  G4bool found = false;
  for (G4int i = 0; i < nc; ++i) {
    if (Inside(p + 0.5 * (prev + t[i]) * v) != kInside) {
      exitDist = prev;
      found = true;
      break;
    }
    prev = t[i];
  }
  if (!found) exitDist = prev;
  if (calcNorm) {
    if (validNorm) *validNorm = false;  // non-convex solid
    if (n) *n = SurfaceNormal(p + exitDist * v);
  }
  return exitDist;
}

G4double ParabolicLensSolid::DistanceToIn(const G4ThreeVector& p) const
{
  // Lower bound 1: distance to the bounding box (which contains the solid).
  const G4double dx = std::max(0., std::abs(p.x()) - fX);
  const G4double dy = std::max(0., std::abs(p.y()) - fY);
  const G4double dz = std::max(0., std::abs(p.z()) - fP2);
  G4double safety = std::sqrt(dx * dx + dy * dy + dz * dz);
  // Lower bound 2: inside the cavity above a parabola, |s'| <= X/R on the solid.
  if (std::abs(p.x()) <= fX) {
    const G4double h = std::abs(p.z()) - Sag(p.x());
    if (h > 0) {
      const G4double smax = fX / fR;
      safety = std::max(safety, h / std::sqrt(1. + smax * smax));
    }
  }
  return safety;
}

G4double ParabolicLensSolid::DistanceToOut(const G4ThreeVector& p) const
{
  const G4double slope = p.x() / fR;
  // The convex sag function lies above its tangent: tangent-line bound is exact lower bound.
  const G4double dp = (Sag(p.x()) - std::abs(p.z())) / std::sqrt(1. + slope * slope);
  const G4double s = std::min({fX - std::abs(p.x()), fY - std::abs(p.y()), fP2 - std::abs(p.z()), dp});
  return std::max(0., s);
}

void ParabolicLensSolid::BoundingLimits(G4ThreeVector& pMin, G4ThreeVector& pMax) const
{
  pMin.set(-fX, -fY, -fP2);
  pMax.set(fX, fY, fP2);
}

G4bool ParabolicLensSolid::CalculateExtent(const EAxis pAxis, const G4VoxelLimits& pVoxelLimit,
                                           const G4AffineTransform& pTransform, G4double& pMin,
                                           G4double& pMax) const
{
  G4ThreeVector bmin, bmax;
  BoundingLimits(bmin, bmax);
  G4BoundingEnvelope bbox(bmin, bmax);
  const G4Transform3D transform3D(pTransform.NetRotation().inverse(), pTransform.NetTranslation());
  return bbox.CalculateExtent(pAxis, pVoxelLimit, transform3D, pMin, pMax);
}

G4double ParabolicLensSolid::CrossSectionArea() const
{
  return 2. * fWeb * fA + 2. * fA * fA * fA / (3. * fR) + 4. * fP2 * (fX - fA);
}

G4ThreeVector ParabolicLensSolid::GetPointOnSurface() const
{
  const G4double areaX = 4. * fY * fP2;               // each of x = +-X
  const G4double areaY = CrossSectionArea();          // each of y = +-Y
  const G4double areaZ = 4. * fY * (fX - fA);          // flat part of each z = +-p/2 face
  const G4double areaP = 2. * fY * ArcLength(fA, fR);  // each parabolic surface
  const G4double total = 2. * (areaX + areaY + areaZ + areaP);
  G4double r = G4UniformRand() * total;
  const G4double sgn = (G4UniformRand() < 0.5) ? -1. : 1.;
  const G4double y = (2. * G4UniformRand() - 1.) * fY;

  if ((r -= 2. * areaX) < 0) return {sgn * fX, y, (2. * G4UniformRand() - 1.) * fP2};
  if ((r -= 2. * areaY) < 0) {
    for (;;) {  // uniform point of the cross-section by rejection
      const G4double x = (2. * G4UniformRand() - 1.) * fX;
      const G4double z = (2. * G4UniformRand() - 1.) * fP2;
      if (std::abs(z) <= std::min(fP2, Sag(x))) return {x, sgn * fY, z};
    }
  }
  if ((r -= 2. * areaZ) < 0) {
    const G4double x = (fA + G4UniformRand() * (fX - fA)) * ((G4UniformRand() < 0.5) ? -1. : 1.);
    return {x, y, sgn * fP2};
  }
  // parabolic surface, x distributed with the arc-length density (rejection)
  const G4double wmax = std::sqrt(1. + (fA / fR) * (fA / fR));
  for (;;) {
    const G4double x = (2. * G4UniformRand() - 1.) * fA;
    if (G4UniformRand() * wmax <= std::sqrt(1. + (x / fR) * (x / fR))) return {x, y, sgn * Sag(x)};
  }
}

std::vector<G4TwoVector> ParabolicLensSolid::CrossSection(const Parameters& par, G4int nseg)
{
  if (nseg < 1) throw std::invalid_argument("CrossSection: nseg must be >= 1");
  const G4double a = ParabolicHalfAperture(par);
  const G4double X = a + par.frame;
  const G4double P = 0.5 * par.length;
  auto sag = [&](G4double x) { return 0.5 * par.web + 0.5 * x * x / par.radius; };
  std::vector<G4TwoVector> poly;
  // lower boundary (entrance surface), left to right
  if (X > a) poly.emplace_back(-X, -P);
  poly.emplace_back(-a, -P);
  for (G4int k = 1; k < nseg; ++k) {
    const G4double x = -a + 2. * a * k / nseg;
    poly.emplace_back(x, -sag(x));
  }
  poly.emplace_back(a, -P);
  if (X > a) poly.emplace_back(X, -P);
  // upper boundary (exit surface), right to left
  if (X > a) poly.emplace_back(X, P);
  poly.emplace_back(a, P);
  for (G4int k = nseg - 1; k >= 1; --k) {
    const G4double x = -a + 2. * a * k / nseg;
    poly.emplace_back(x, sag(x));
  }
  poly.emplace_back(-a, P);
  if (X > a) poly.emplace_back(-X, P);
  return poly;
}

void ParabolicLensSolid::DescribeYourselfTo(G4VGraphicsScene& scene) const { scene.AddSolid(*this); }

G4Polyhedron* ParabolicLensSolid::CreatePolyhedron() const
{
  // Visualisation only: polygonal cross-section extruded along y.
  G4ExtrudedSolid tmp(GetName() + "_vis", CrossSection(fPar, 48), fY);
  G4Polyhedron* ph = tmp.CreatePolyhedron();
  G4RotationMatrix rot;
  rot.rotateX(90. * CLHEP::deg);  // extrusion frame (X, Y=z, Z=-y) -> element frame
  if (ph) ph->HepPolyhedron::Transform(G4Transform3D(rot, G4ThreeVector()));
  return ph;
}

G4Polyhedron* ParabolicLensSolid::GetPolyhedron() const
{
  if (!fPolyhedron) fPolyhedron = CreatePolyhedron();
  return fPolyhedron;
}

std::ostream& ParabolicLensSolid::StreamInfo(std::ostream& os) const
{
  os << "-----------------------------------------------------------\n"
     << "    *** Dump for solid - " << GetName() << " ***\n"
     << "    ===================================================\n"
     << " Solid type: ParabolicLensSolid (linear biconcave parabolic element)\n"
     << " Parameters: R = " << fR << " mm, d = " << fWeb << " mm, p = " << 2 * fP2
     << " mm, half-aperture a = " << fA << " mm, half-width X = " << fX << " mm, half-height = " << fY << " mm\n"
     << "-----------------------------------------------------------\n";
  return os;
}

}  // namespace xrcl
