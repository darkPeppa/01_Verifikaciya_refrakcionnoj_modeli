#include "FocusAnalysis.hh"

#include <algorithm>
#include <cmath>
#include <map>

namespace xrcl {

namespace {

struct M2 {
  double a = 1, b = 0, c = 0, d = 1;
};
M2 Mul(const M2& l, const M2& r)  // l * r
{
  return {l.a * r.a + l.b * r.c, l.a * r.b + l.b * r.d, l.c * r.a + l.d * r.c, l.c * r.b + l.d * r.d};
}
M2 Drift(double L) { return {1, L, 0, 1}; }

}  // namespace

ParaxialAxis ComputeParaxial(const LensLayout& layout, FocusAxis axis, double delta)
{
  ParaxialAxis out;
  if (layout.elements.empty()) return out;
  const double R = layout.R, d = layout.d;
  const double nIn = 1. - delta;
  M2 M;
  double z = layout.elements.front().zCenter - 0.5 * d;
  out.zFirstVertex = z;
  for (const auto& e : layout.elements) {
    const double zv1 = e.zCenter - 0.5 * d;
    M = Mul(Drift(zv1 - z), M);
    const bool focusing = (e.axis == axis);
    if (focusing) ++out.nFocusing;
    // entrance surface z = zv1 - u^2/(2R): n2 th2 = n1 th1 - (n2 - n1) kappa u, kappa = -1/R
    const double k1 = focusing ? -1. / R : 0.;
    M = Mul(M2{1, 0, -(nIn - 1.) * k1 / nIn, 1. / nIn}, M);
    M = Mul(Drift(d), M);
    // exit surface z = zv2 + u^2/(2R): kappa = +1/R, n1 = 1 - delta, n2 = 1
    const double k2 = focusing ? 1. / R : 0.;
    M = Mul(M2{1, 0, -(1. - nIn) * k2, nIn}, M);
    z = e.zCenter + 0.5 * d;
  }
  out.zLastVertex = z;
  out.A = M.a;
  out.B = M.b;
  out.C = M.c;
  out.D = M.d;
  if (M.c != 0.) {
    out.valid = true;
    out.efl = -1. / M.c;
    out.bflFromLastVertex = -M.a / M.c;
    out.zFocus = out.zLastVertex + out.bflFromLastVertex;
    out.zPrincipal = out.zFocus - out.efl;
  }
  return out;
}

void RmsAt(const std::vector<RayRecord>& rays, double z, double& rmsX, double& rmsY)
{
  double sx = 0, sy = 0, sxx = 0, syy = 0;
  int n = 0;
  for (const auto& r : rays) {
    if (!r.Valid()) continue;
    const double s = (z - r.exitPos.z()) / r.exitDir.z();
    const double x = r.exitPos.x() + s * r.exitDir.x();
    const double y = r.exitPos.y() + s * r.exitDir.y();
    sx += x;
    sy += y;
    sxx += x * x;
    syy += y * y;
    ++n;
  }
  if (n == 0) {
    rmsX = rmsY = kNaN;
    return;
  }
  const double mx = sx / n, my = sy / n;
  rmsX = std::sqrt(std::max(0., sxx / n - mx * mx));
  rmsY = std::sqrt(std::max(0., syy / n - my * my));
}

namespace {

void MethodB(const std::vector<RayRecord>& rays, bool xAxis, double zExit, AxisFocus& f)
{
  // positions at the exit plane and slopes t = k_u / k_z; x(z) = x0 + (z - zExit) t
  double n = 0, su = 0, st = 0;
  for (const auto& r : rays) {
    if (!r.Valid()) continue;
    const double u = xAxis ? r.exitPos.x() : r.exitPos.y();
    const double t = (xAxis ? r.exitDir.x() : r.exitDir.y()) / r.exitDir.z();
    su += u;
    st += t;
    n += 1;
  }
  f.nRms = static_cast<int>(n);
  if (n < 3) return;
  const double mu = su / n, mt = st / n;
  double vuu = 0, vtt = 0, vut = 0;
  for (const auto& r : rays) {
    if (!r.Valid()) continue;
    const double u = (xAxis ? r.exitPos.x() : r.exitPos.y()) - mu;
    const double t = (xAxis ? r.exitDir.x() : r.exitDir.y()) / r.exitDir.z() - mt;
    vuu += u * u;
    vtt += t * t;
    vut += u * t;
  }
  vuu /= n;
  vtt /= n;
  vut /= n;
  f.rmsAtExit = std::sqrt(vuu);
  if (vtt <= 0.) return;
  const double s = -vut / vtt;
  f.zRmsMin = zExit + s;
  f.rmsMin = std::sqrt(std::max(0., vuu + 2. * s * vut + s * s * vtt));
}

void MethodA(const std::vector<RayRecord>& rays, bool xAxis, double ox, double oy, double zExit, AxisFocus& f)
{
  // rays of the paraxial cross: displaced only along this axis
  const double eps = 1e-12;  // mm
  std::map<long long, std::pair<const RayRecord*, const RayRecord*>> byHeight;  // key: height in fm
  for (const auto& r : rays) {
    if (!r.Valid()) continue;
    const double du = xAxis ? r.x0 - ox : r.y0 - oy;
    const double dv = xAxis ? r.y0 - oy : r.x0 - ox;
    if (std::abs(dv) > eps || std::abs(du) < eps) continue;
    const long long key = std::llround(std::abs(du) * 1e12);
    auto& slot = byHeight[key];
    (du > 0 ? slot.first : slot.second) = &r;
  }
  for (const auto& [key, pr] : byHeight) {
    if (!pr.first || !pr.second) continue;
    const RayRecord& p = *pr.first;
    const RayRecord& m = *pr.second;
    PairFocus pf;
    pf.h = key * 1e-12;
    const double up = xAxis ? p.exitPos.x() : p.exitPos.y();
    const double um = xAxis ? m.exitPos.x() : m.exitPos.y();
    const double tp = (xAxis ? p.exitDir.x() : p.exitDir.y()) / p.exitDir.z();
    const double tm = (xAxis ? m.exitDir.x() : m.exitDir.y()) / m.exitDir.z();
    if (tp != tm) {
      pf.valid = true;
      const double s = -(up - um) / (tp - tm);
      pf.zCross = zExit + s;
      pf.lateral = up + s * tp;
      pf.efl = -2. * pf.h / (tp - tm);
    }
    f.pairs.push_back(pf);
  }
  std::vector<PairFocus> ok;
  for (const auto& p : f.pairs)
    if (p.valid) ok.push_back(p);
  if (ok.size() == 1) {
    f.zParaxial = ok[0].zCross;
    f.eflParaxial = ok[0].efl;
  }
  else if (ok.size() >= 2) {
    // Richardson extrapolation with the two smallest heights: q(h) = q0 + c h^2
    const double h1 = ok[0].h, h2 = ok[1].h;
    const double w = h2 * h2 - h1 * h1;
    f.zParaxial = (h2 * h2 * ok[0].zCross - h1 * h1 * ok[1].zCross) / w;
    f.eflParaxial = (h2 * h2 * ok[0].efl - h1 * h1 * ok[1].efl) / w;
  }
}

}  // namespace

FocusResult AnalyseFocus(const std::vector<RayRecord>& rays, double zExit, double ox, double oy)
{
  FocusResult res;
  res.zExit = zExit;
  for (int ax = 0; ax < 2; ++ax) {
    const bool xAxis = (ax == 0);
    AxisFocus& f = xAxis ? res.x : res.y;
    MethodA(rays, xAxis, ox, oy, zExit, f);
    MethodB(rays, xAxis, zExit, f);
    for (const auto& r : rays) {
      if (!r.Valid()) continue;
      const double t0 = (xAxis ? r.dir0.x() : r.dir0.y()) / r.dir0.z();
      const double t1 = (xAxis ? r.exitDir.x() : r.exitDir.y()) / r.exitDir.z();
      f.maxSlopeChange = std::max(f.maxSlopeChange, std::abs(t1 - t0));
    }
  }
  return res;
}

}  // namespace xrcl
