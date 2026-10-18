// Unit tests of the optical kernel: vector Snell law, exact lens solid
// (intersections, normals, safeties), paraxial matrices, layout and material.
#include "mini_test.hh"

#include "Config.hh"
#include "FocusAnalysis.hh"
#include "LensLayout.hh"
#include "ParabolicLensSolid.hh"
#include "RefractiveIndex.hh"
#include "SU8MaterialBuilder.hh"
#include "XrayRefraction.hh"

#include "G4SystemOfUnits.hh"

#include <cmath>
#include <random>
#include <sstream>

using namespace xrcl;

namespace {

std::string Fmt(double a, double b)
{
  std::ostringstream os;
  os.precision(17);
  os << a << " vs " << b;
  return os.str();
}

// independent membership test (exact, no tolerance)
struct Exact {
  ParabolicLensSolid::Parameters par;
  double a, X, P;
  explicit Exact(const ParabolicLensSolid::Parameters& p) : par(p)
  {
    a = std::sqrt(p.radius * (p.length - p.web));
    X = a + p.frame;
    P = 0.5 * p.length;
  }
  double sag(double x) const { return 0.5 * par.web + 0.5 * x * x / par.radius; }
  bool inside(const G4ThreeVector& q) const
  {
    return std::abs(q.x()) <= X && std::abs(q.y()) <= par.halfHeight && std::abs(q.z()) <= P &&
           std::abs(q.z()) <= sag(q.x());
  }
};

ParabolicLensSolid::Parameters Objective1(double frame)
{
  ParabolicLensSolid::Parameters p;
  p.radius = 9 * um;
  p.web = 10 * um;
  p.length = 235 * um;
  p.frame = frame;
  p.halfHeight = 100 * um;
  return p;
}

}  // namespace

// ------------------------------------------------------------------ Snell law
TEST_CASE("Snell: deflection vs quad-precision scalar Snell law, all angles and media")
{
  const double pairs[][2] = {{0, 4.32e-7}, {4.32e-7, 0}, {0, 1e-3}, {1e-3, 0}, {1e-5, 3e-6}, {2.6e-7, 9.1e-7}};
  int nTir = 0;
  for (const auto& dd : pairs) {
    for (int i = 0; i <= 900; ++i) {
      const double th = i * 0.1 * deg * 0.99999;  // 0 .. ~89.999 deg
      const G4ThreeVector k(std::sin(th), 0, std::cos(th));
      const G4ThreeVector n(0, 0, 1);
      const RefractionResult r = Refract(k, n, dd[0], dd[1]);
      const long double ratio = (1.0L - (long double)dd[0]) / (1.0L - (long double)dd[1]);
      const long double s2 = ratio * std::sin((long double)th);
      if (s2 > 1.0L) {
        ++nTir;
        CHECK(r.totalReflection);
        CHECK(std::abs(r.direction.x() - k.x()) < 1e-15 && std::abs(r.direction.z() + k.z()) < 1e-15);
        continue;
      }
      CHECK(!r.totalReflection);
      const long double dref = std::asin(s2) - (long double)th;
      const double d = std::atan2(r.direction.x(), r.direction.z()) - th;
      CHECK_MSG(std::abs(d - (double)dref) <= 1e-15 + 1e-8 * std::abs((double)dref), Fmt(d, (double)dref));
      CHECK(std::abs(r.direction.mag() - 1.) < 5e-16);  // ~2 ulp
      CHECK(std::abs(r.direction.y()) == 0.);  // stays in the plane of incidence
    }
  }
  CHECK(nTir > 0);  // grazing angles beyond the critical angle were exercised
}

TEST_CASE("Snell: reversibility and normal orientation independence")
{
  std::mt19937_64 rng(7);
  std::uniform_real_distribution<double> u(-1, 1);
  for (int i = 0; i < 20000; ++i) {
    G4ThreeVector k(u(rng), u(rng), 3 + u(rng));
    k = k.unit();
    G4ThreeVector n(u(rng), u(rng), u(rng) + 1.5);
    n = n.unit();
    const RefractionResult a = Refract(k, n, 0, 4.32e-7);
    const RefractionResult b = Refract(k, -n, 0, 4.32e-7);
    CHECK((a.direction - b.direction).mag() < 1e-16);
    if (a.totalReflection) continue;
    const RefractionResult back = Refract(a.direction, n, 4.32e-7, 0);
    CHECK((back.direction - k).mag() < 1e-15);
    CHECK(std::abs(k.cross(n).dot(a.direction)) < 1e-15);  // coplanar
  }
  const G4ThreeVector k(0.1, 0.2, 0.97);
  CHECK((Refract(k.unit(), G4ThreeVector(0, 1, 0), 3e-7, 3e-7).direction - k.unit()).mag() == 0);
}

// ------------------------------------------------------------------ solid
TEST_CASE("Solid: Inside agrees with the exact membership away from the surface")
{
  for (double frame : {0., 20 * um}) {
    const auto par = Objective1(frame);
    ParabolicLensSolid s("t", par);
    Exact ex(par);
    std::mt19937_64 rng(1);
    std::uniform_real_distribution<double> ux(-1.2 * ex.X, 1.2 * ex.X), uy(-1.2 * par.halfHeight, 1.2 * par.halfHeight),
        uz(-1.2 * ex.P, 1.2 * ex.P);
    int nIn = 0;
    for (int i = 0; i < 200000; ++i) {
      const G4ThreeVector q(ux(rng), uy(rng), uz(rng));
      const EInside in = s.Inside(q);
      if (in == kSurface) continue;
      CHECK(ex.inside(q) == (in == kInside));
      nIn += ex.inside(q);
    }
    CHECK(nIn > 1000);
  }
}

TEST_CASE("Solid: DistanceToIn/Out land on the surface and skip no material (brute force)")
{
  for (double frame : {0., 20 * um}) {
    const auto par = Objective1(frame);
    ParabolicLensSolid s("t", par);
    Exact ex(par);
    std::mt19937_64 rng(2);
    std::uniform_real_distribution<double> u(-1, 1);
    int hits = 0;
    for (int i = 0; i < 1000; ++i) {
      // start outside the bounding box, aim at a random point of the box
      G4ThreeVector start(u(rng) * 3 * ex.X, u(rng) * 3 * par.halfHeight, -2 * ex.P + 0.5 * ex.P * u(rng));
      if (i % 3 == 1) start.setZ(2 * ex.P);
      G4ThreeVector target(u(rng) * ex.X, u(rng) * par.halfHeight, u(rng) * ex.P);
      const G4ThreeVector v = (target - start).unit();
      const double t = s.DistanceToIn(start, v);
      // brute-force first entry
      const double tmax = (target - start).mag() * 2;
      const int nstep = 100000;
      double tb = kInfinity;
      for (int k = 0; k <= nstep; ++k) {
        const double tt = tmax * k / nstep;
        if (ex.inside(start + tt * v)) {
          tb = tt;
          break;
        }
      }
      if (tb == kInfinity) {
        CHECK_MSG(t == kInfinity || t > tmax * (1 - 1e-6), Fmt(t, tb));
        continue;
      }
      ++hits;
      CHECK_MSG(t <= tb + 1e-12 && t >= tb - tmax / nstep - 1e-12, Fmt(t, tb));
      const G4ThreeVector e = start + t * v;
      CHECK(s.Inside(e) == kSurface);
      // from just inside, DistanceToOut must give a surface point with material in between
      const G4ThreeVector pin = e + 1e-7 * v;
      if (s.Inside(pin) != kInside) continue;
      G4bool validNorm;
      G4ThreeVector nrm;
      const double to = s.DistanceToOut(pin, v, true, &validNorm, &nrm);
      const G4ThreeVector x = pin + to * v;
      CHECK(s.Inside(x) == kSurface);
      CHECK(std::abs(nrm.mag() - 1) < 1e-12);
      CHECK(nrm.dot(v) > -1e-12);  // leaving: normal not opposed to motion
      for (int k = 1; k < 1000; ++k) CHECK(ex.inside(pin + (to * k / 1000.) * v) || s.Inside(pin + (to * k / 1000.) * v) != kOutside);
    }
    CHECK(hits > 150);
  }
}

TEST_CASE("Solid: surface normals are outward unit vectors equal to the analytic gradient")
{
  const auto par = Objective1(15 * um);
  ParabolicLensSolid s("t", par);
  Exact ex(par);
  int nPar = 0;
  for (int i = 0; i < 20000; ++i) {
    const G4ThreeVector q = s.GetPointOnSurface();
    CHECK(s.Inside(q) == kSurface);
    const G4ThreeVector n = s.SurfaceNormal(q);
    CHECK(std::abs(n.mag() - 1) < 1e-12);
    const double eps = 1e-6 * mm;
    CHECK(s.Inside(q + eps * n) == kOutside);
    // a point just inside along -n exists unless q is on an edge
    if (std::abs(std::abs(q.z()) - ex.sag(q.x())) < 1e-15 && std::abs(q.x()) < ex.a * 0.999 &&
        std::abs(q.y()) < par.halfHeight * 0.999) {
      ++nPar;
      const double sz = q.z() > 0 ? 1 : -1;
      const G4ThreeVector g = G4ThreeVector(-q.x() / par.radius, 0, sz).unit();
      CHECK((n - g).mag() < 1e-14);
      CHECK(s.Inside(q - eps * n) == kInside);
    }
  }
  CHECK(nPar > 1000);
}

TEST_CASE("Solid: safety distances are lower bounds")
{
  const auto par = Objective1(10 * um);
  ParabolicLensSolid s("t", par);
  Exact ex(par);
  std::mt19937_64 rng(3);
  std::uniform_real_distribution<double> u(-1, 1);
  for (int i = 0; i < 20000; ++i) {
    const G4ThreeVector q(u(rng) * 1.5 * ex.X, u(rng) * 1.5 * par.halfHeight, u(rng) * 1.5 * ex.P);
    const G4ThreeVector v = G4ThreeVector(u(rng), u(rng), u(rng)).unit();
    const EInside in = s.Inside(q);
    if (in == kOutside) CHECK(s.DistanceToIn(q) <= s.DistanceToIn(q, v) + 1e-12);
    if (in == kInside) CHECK(s.DistanceToOut(q) <= s.DistanceToOut(q, v) + 1e-12);
  }
}

TEST_CASE("Solid: aperture, cross-section and polygon are consistent")
{
  const auto par = Objective1(0);
  ParabolicLensSolid s("t", par);
  CHECK(std::abs(2 * s.HalfAperture() - 90 * um) < 1e-12);  // 2 sqrt(9 * 225) = 90 um
  const auto poly = ParabolicLensSolid::CrossSection(par, 64);
  double area = 0;  // shoelace
  for (std::size_t i = 0; i < poly.size(); ++i) {
    const auto& p = poly[i];
    const auto& q = poly[(i + 1) % poly.size()];
    area += p.x() * q.y() - q.x() * p.y();
  }
  area = std::abs(area) / 2;
  CHECK_MSG(std::abs(area - s.CrossSectionArea()) / s.CrossSectionArea() < 1e-3, Fmt(area, s.CrossSectionArea()));
}

// ------------------------------------------------------------------ paraxial
TEST_CASE("Paraxial: single element and thin/continuous limits")
{
  LensLayout L;
  L.R = 9 * um;
  L.d = 10 * um;
  L.p = 235 * um;
  L.elements.push_back({0, FocusAxis::X, 0.5 * L.p});
  const double delta = 4.32e-7;
  const ParaxialAxis P = ComputeParaxial(L, FocusAxis::X, delta);
  const double f1 = L.R / (2 * delta);
  CHECK_MSG(std::abs(P.efl - f1) / f1 < 1e-6, Fmt(P.efl, f1));
  const ParaxialAxis Q = ComputeParaxial(L, FocusAxis::Y, delta);
  CHECK(!Q.valid);  // no focusing in the orthogonal direction

  // N closely spaced elements: thin limit and continuous thick-lens limit
  LensLayout M = L;
  M.elements.clear();
  const int N = 400;
  for (int i = 0; i < N; ++i) M.elements.push_back({i, FocusAxis::X, 0.5 * M.p + i * M.p});
  const ParaxialAxis PN = ComputeParaxial(M, FocusAxis::X, delta);
  const double f0 = M.R / (2. * N * delta);
  const double Ls = N * M.p;
  const double x = std::sqrt(Ls / f0);
  const double eflCont = f0 * x / std::sin(x);
  const double bflCont = f0 * x / std::tan(x);
  CHECK_MSG(std::abs(PN.efl - eflCont) / eflCont < 2e-3, Fmt(PN.efl, eflCont));
  CHECK_MSG(std::abs((PN.zFocus - N * M.p) - bflCont) / eflCont < 2e-3, Fmt(PN.zFocus - N * M.p, bflCont));
}

// ------------------------------------------------------------------ layout
TEST_CASE("Layout: gaps, overlaps and alternating orientation from the design data")
{
  LensSpec obj1;
  obj1.name = "obj1";
  obj1.radiusUm = 9;
  obj1.apertureUm = 90;
  obj1.nPerAxis = 154;
  obj1.lengthUm = 235;
  obj1.webUm = 10;
  obj1.nominalLengthMm = 81;
  SystemSpec full;
  full.mode = "full";
  const LensLayout L = LensLayout::Build(obj1, full);
  CHECK(L.elements.size() == 308);
  CHECK(L.nX == 154 && L.nY == 154);
  CHECK(std::abs(L.gap / um - 28.0781759) < 1e-6);
  CHECK(std::abs(L.builtLength - 81 * mm) < 1e-9);
  CHECK(L.elements[0].axis == FocusAxis::X && L.elements[1].axis == FocusAxis::Y);
  for (std::size_t i = 1; i < L.elements.size(); ++i)
    CHECK(L.elements[i].zCenter - L.elements[i - 1].zCenter >= L.p);  // no overlap

  LensSpec cond1 = obj1;
  cond1.radiusUm = 89;
  cond1.apertureUm = 909;
  cond1.nPerAxis = 21;
  cond1.lengthUm = 2331;
  cond1.nominalLengthMm = 97;
  const LensLayout C = LensLayout::Build(cond1, full);
  CHECK(C.gap == 0);
  CHECK(C.gapFromNominal < 0);
  CHECK(!C.warnings.empty());
  CHECK(std::abs(C.builtLength / mm - 97.902) < 1e-9);
}

// ------------------------------------------------------------------ material / delta
TEST_CASE("Material: SU-8 normalisation and delta models")
{
  MaterialSpec m;
  m.name = "SU8_unit";
  m.densityGcm3 = 1.24;
  m.massPercentSource = {{"C", 72.4}, {"O", 17.9}, {"H", 6.9}, {"Sb", 1.326}, {"F", 1.242}, {"S", 0.3493}};
  MaterialReport rep;
  G4Material* mat = SU8MaterialBuilder::Build(m, &rep);
  CHECK(std::abs(rep.sourceSum - 100.1173) < 1e-9);
  double sum = 0;
  for (const auto& [el, w] : rep.usedFraction) sum += w;
  CHECK(std::abs(sum - 1) < 1e-12);
  PowerLawDelta pl(4.32e-7, 25 * keV, -2);
  CHECK(std::abs(pl.Delta(25 * keV) - 4.32e-7) < 1e-20);
  CHECK(std::abs(pl.Delta(20 * keV) / pl.Delta(30 * keV) - 2.25) < 1e-12);
  // thesis formula (1.7.4): 1.7564e-6 * lambda[A]^2 at 25 keV
  const double lambdaA = 12.39842 / 25.;
  CHECK(std::abs(1.7564e-6 * lambdaA * lambdaA - 4.32e-7) / 4.32e-7 < 1e-3);
  const double fe = FreeElectronDelta(mat).Delta(25 * keV);
  CHECK_MSG(std::abs(fe - 4.32e-7) / 4.32e-7 < 0.03, Fmt(fe, 4.32e-7));
}

int main() { return mt::RunAll(); }
