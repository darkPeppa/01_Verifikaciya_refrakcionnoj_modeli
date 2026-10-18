#include "SU8MaterialBuilder.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <cmath>
#include <stdexcept>

namespace xrcl {

G4Material* SU8MaterialBuilder::Build(const MaterialSpec& spec, MaterialReport* report)
{
  if (spec.massPercentSource.empty()) throw std::runtime_error("Material " + spec.name + ": empty composition");
  double sum = 0.;
  for (const auto& [sym, pc] : spec.massPercentSource) {
    if (pc <= 0) throw std::runtime_error("Material " + spec.name + ": non-positive fraction for " + sym);
    sum += pc;
  }
  if (!spec.normalize && std::abs(sum - 100.) > 1e-9) {
    throw std::runtime_error("Material " + spec.name + ": mass percentages sum to " + std::to_string(sum) +
                             " and normalisation is disabled");
  }

  G4Material* mat = G4Material::GetMaterial(spec.name, /*warning=*/false);
  auto* nist = G4NistManager::Instance();
  if (!mat) {
    mat = new G4Material(spec.name, spec.densityGcm3 * g / cm3, static_cast<G4int>(spec.massPercentSource.size()),
                         kStateSolid);
    for (const auto& [sym, pc] : spec.massPercentSource) {
      G4Element* el = nist->FindOrBuildElement(sym);
      if (!el) throw std::runtime_error("Unknown element symbol " + sym);
      mat->AddElement(el, pc / sum);  // normalised mass fraction
    }
    if (std::abs(sum - 100.) > 1e-9) {
      G4cout << "[SU8MaterialBuilder] published mass percentages of " << spec.name << " sum to " << sum
             << " %; fractions normalised by this sum (relative change " << (sum - 100.) / 100. << ")" << G4endl;
    }
  }
  if (report) {
    report->sourcePercent = spec.massPercentSource;
    report->sourceSum = sum;
    report->usedFraction.clear();
    for (const auto& [sym, pc] : spec.massPercentSource) report->usedFraction.emplace_back(sym, pc / sum);
    report->electronDensityPerCm3 = mat->GetElectronDensity() * cm3;
  }
  return mat;
}

}  // namespace xrcl
