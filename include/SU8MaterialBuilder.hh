// Builds the lens polymer (SU-8 by default) as a real G4Material from the configured
// density and mass composition, and registers its delta(E) model.
#ifndef XRCL_SU8_MATERIAL_BUILDER_HH
#define XRCL_SU8_MATERIAL_BUILDER_HH

#include "Config.hh"

#include <string>
#include <utility>
#include <vector>

class G4Material;

namespace xrcl {

struct MaterialReport {
  std::vector<std::pair<std::string, double>> sourcePercent;  // as published
  std::vector<std::pair<std::string, double>> usedFraction;   // as given to G4Material
  double sourceSum = 0.;
  double electronDensityPerCm3 = 0.;
};

class SU8MaterialBuilder {
 public:
  // Returns an existing material of that name if already built.
  static G4Material* Build(const MaterialSpec& spec, MaterialReport* report = nullptr);
};

}  // namespace xrcl

#endif
