#include "XrayRefraction.hh"

#include <cmath>

namespace xrcl {

RefractionResult Refract(const G4ThreeVector& k, const G4ThreeVector& normal, double delta1, double delta2)
{
  RefractionResult res;
  G4ThreeVector n = normal.unit();
  double c = n.dot(k);
  if (c < 0) {
    n = -n;
    c = -c;
  }
  res.cosIncidence = c;
  if (delta1 == delta2) {
    res.direction = k;
    return res;
  }
  const double eps = (delta2 - delta1) / (1. - delta2);  // mu - 1
  const double mu = 1. + eps;
  const double twoEps = eps * (2. + eps);  // mu^2 - 1
  const double s2 = c * c - twoEps * (1. - c * c);
  if (s2 < 0.) {
    res.totalReflection = true;
    res.direction = (k - 2. * c * n).unit();
    return res;
  }
  const double s = std::sqrt(s2);
  res.direction = (mu * k - (twoEps / (s + mu * c)) * n).unit();
  return res;
}

}  // namespace xrcl
