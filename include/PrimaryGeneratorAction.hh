// Monochromatic parallel X-ray beam along +z; one photon (= one ray) per event.
// The ray pattern is selected by source.mode (see RayGenerator).
#ifndef XRCL_PRIMARY_GENERATOR_ACTION_HH
#define XRCL_PRIMARY_GENERATOR_ACTION_HH

#include "Config.hh"

#include "G4ThreeVector.hh"
#include "G4VUserPrimaryGeneratorAction.hh"

#include <vector>

class G4ParticleGun;

namespace xrcl {

struct SourceRay {
  double x = 0, y = 0;  // transverse start position at the source plane
  G4ThreeVector direction{0, 0, 1};
};

class RayGenerator {
 public:
  // halfAperture: parabolic half aperture a; all extents are fractions of it.
  RayGenerator(const SourceSpec& spec, double halfAperture);
  std::size_t Count() const { return fRays.size(); }
  const SourceRay& Ray(std::size_t i) const { return fRays.at(i); }
  const std::vector<SourceRay>& Rays() const { return fRays; }

 private:
  std::vector<SourceRay> fRays;
};

class PrimaryGeneratorAction : public G4VUserPrimaryGeneratorAction {
 public:
  PrimaryGeneratorAction();
  ~PrimaryGeneratorAction() override;
  void GeneratePrimaries(G4Event* event) override;

 private:
  G4ParticleGun* fGun;
};

}  // namespace xrcl

#endif
