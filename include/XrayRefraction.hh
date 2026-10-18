// Vector Snell law for X-rays at an interface between media with refractive
// indices n1 = 1 - delta1 (incident side) and n2 = 1 - delta2 (transmitted side).
//
// With the unit normal oriented along the propagation (c = n.k > 0) and mu = n1/n2:
//     k' = mu k + (s - mu c) n,    s = sqrt(1 - mu^2 (1 - c^2))
// Because mu = 1 + eps with |eps| ~ 1e-7, the form above loses ~7 digits through
// cancellation. It is evaluated instead as
//     eps = (delta2 - delta1) / (1 - delta2)                (exact, no 1 - delta)
//     s^2 = c^2 - (2 eps + eps^2)(1 - c^2)
//     s - mu c = -(2 eps + eps^2) / (s + mu c)
//     k' = (1 + eps) k - (2 eps + eps^2) / (s + mu c) n
// s^2 < 0 means no transmitted solution: total external reflection, k' = k - 2 c n.
#ifndef XRCL_XRAY_REFRACTION_HH
#define XRCL_XRAY_REFRACTION_HH

#include "G4ThreeVector.hh"

namespace xrcl {

struct RefractionResult {
  G4ThreeVector direction;
  bool totalReflection = false;
  double cosIncidence = 0.;  // c = |n.k|
};

RefractionResult Refract(const G4ThreeVector& k, const G4ThreeVector& normal, double delta1, double delta2);

}  // namespace xrcl

#endif
