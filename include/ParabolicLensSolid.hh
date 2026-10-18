// Exact (non-tessellated) Geant4 solid of one linear biconcave parabolic X-ray lens element.
//
// Local frame: x = focusing coordinate (u), y = non-focusing coordinate (extrusion),
// z = optical axis; origin at the element centre (middle of the central web).
// The material occupies
//     |x| <= X,  |y| <= Yh,  |z| <= p/2,  |z| <= s(x),   s(x) = d/2 + x^2/(2R)
// i.e. the polymer lies between the entrance surface z = -s(x) and the exit surface
// z = +s(x) (thickness t(x) = d + x^2/R), truncated by the faces z = +-p/2.
// The parabolic (optically active) half-aperture is a = sqrt(R (p - d)), where the
// parabola meets the faces z = +-p/2; for |x| > a the element is a flat block of
// thickness p (frame), X = a + frame.
//
// All ray/surface intersections are solved analytically (stable quadratic roots),
// so surface positions and normals carry no discretisation error.
#ifndef XRCL_PARABOLIC_LENS_SOLID_HH
#define XRCL_PARABOLIC_LENS_SOLID_HH

#include "G4TwoVector.hh"
#include "G4VSolid.hh"

#include <vector>

class G4Polyhedron;

namespace xrcl {

class ParabolicLensSolid : public G4VSolid {
 public:
  struct Parameters {
    G4double radius = 0.;      // R
    G4double web = 0.;         // d
    G4double length = 0.;      // p
    G4double frame = 0.;       // frame width beyond the parabolic aperture
    G4double halfHeight = 0.;  // half extent along y
  };

  ParabolicLensSolid(const G4String& name, const Parameters& par);
  ParabolicLensSolid(const ParabolicLensSolid& rhs);
  ParabolicLensSolid& operator=(const ParabolicLensSolid&) = delete;
  ~ParabolicLensSolid() override;

  EInside Inside(const G4ThreeVector& p) const override;
  G4ThreeVector SurfaceNormal(const G4ThreeVector& p) const override;
  G4double DistanceToIn(const G4ThreeVector& p, const G4ThreeVector& v) const override;
  G4double DistanceToIn(const G4ThreeVector& p) const override;
  G4double DistanceToOut(const G4ThreeVector& p, const G4ThreeVector& v, const G4bool calcNorm = false,
                         G4bool* validNorm = nullptr, G4ThreeVector* n = nullptr) const override;
  G4double DistanceToOut(const G4ThreeVector& p) const override;

  void BoundingLimits(G4ThreeVector& pMin, G4ThreeVector& pMax) const override;
  G4bool CalculateExtent(const EAxis pAxis, const G4VoxelLimits& pVoxelLimit, const G4AffineTransform& pTransform,
                         G4double& pMin, G4double& pMax) const override;

  G4GeometryType GetEntityType() const override { return "ParabolicLensSolid"; }
  G4VSolid* Clone() const override { return new ParabolicLensSolid(*this); }
  std::ostream& StreamInfo(std::ostream& os) const override;
  G4ThreeVector GetPointOnSurface() const override;

  void DescribeYourselfTo(G4VGraphicsScene& scene) const override;
  G4Polyhedron* CreatePolyhedron() const override;
  G4Polyhedron* GetPolyhedron() const override;

  // --- geometry helpers ---
  G4double Sag(G4double x) const { return 0.5 * fWeb + 0.5 * x * x / fR; }
  G4double HalfAperture() const { return fA; }
  G4double HalfWidth() const { return fX; }
  G4double CrossSectionArea() const;
  const Parameters& GetParameters() const { return fPar; }

  // Polygonal (x, z) cross-section with nseg chords per parabola; used for the
  // tessellated comparison geometry and for visualisation.
  static std::vector<G4TwoVector> CrossSection(const Parameters& par, G4int nseg);
  static G4double ParabolicHalfAperture(const Parameters& par);

 private:
  // Signed constraint value: > 0 outside, < 0 inside (approximate distance).
  G4double MaxConstraint(const G4ThreeVector& p) const;
  G4int Candidates(const G4ThreeVector& p, const G4ThreeVector& v, G4double* t) const;

  Parameters fPar;
  G4double fR, fWeb, fP2, fA, fX, fY;
  G4double fHalfTol;
  mutable G4Polyhedron* fPolyhedron = nullptr;
};

}  // namespace xrcl

#endif
