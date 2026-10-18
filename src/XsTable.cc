#include "XsTable.hh"

#include "DetectorConstruction.hh"

#include "G4Element.hh"
#include "G4EmCalculator.hh"
#include "G4Gamma.hh"
#include "G4Material.hh"
#include "G4SystemOfUnits.hh"
#include "G4ios.hh"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace xrcl {

void WriteXsTable(const std::string& file, const std::vector<double>& energiesKeV)
{
  const G4Material* m = GeometryInfo::Instance().lensMaterial;
  if (!m) throw std::runtime_error("xs table: no lens material");
  const auto parent = std::filesystem::path(file).parent_path();
  if (!parent.empty()) std::filesystem::create_directories(parent);
  std::ofstream out(file);
  if (!out) throw std::runtime_error("Cannot write " + file);
  out << std::setprecision(10);
  out << "# material " << m->GetName() << ", density " << m->GetDensity() / (g / cm3)
      << " g/cm3; linear attenuation coefficients in 1/cm from G4EmCalculator\n";
  out << "energy_keV,mu_phot_per_cm,mu_compt_per_cm,mu_rayl_per_cm,mu_total_per_cm,mu_over_rho_total_cm2_g";
  const G4ElementVector* els = m->GetElementVector();
  for (std::size_t i = 0; i < m->GetNumberOfElements(); ++i) out << ",mu_phot_" << (*els)[i]->GetSymbol() << "_per_cm";
  out << "\n";
  G4EmCalculator calc;
  const G4double* nAt = m->GetVecNbOfAtomsPerVolume();
  const double rho = m->GetDensity() / (g / cm3);
  for (double e : energiesKeV) {
    const double E = e * keV;
    const double ph = calc.ComputeCrossSectionPerVolume(E, G4Gamma::Definition(), "phot", m) * cm;
    const double co = calc.ComputeCrossSectionPerVolume(E, G4Gamma::Definition(), "compt", m) * cm;
    const double ra = calc.ComputeCrossSectionPerVolume(E, G4Gamma::Definition(), "Rayl", m) * cm;
    out << e << ',' << ph << ',' << co << ',' << ra << ',' << ph + co + ra << ',' << (ph + co + ra) / rho;
    for (std::size_t i = 0; i < m->GetNumberOfElements(); ++i) {
      const G4Element* el = (*els)[i];
      out << ',' << calc.ComputeCrossSectionPerAtom(E, G4Gamma::Definition(), "phot", el->GetZ(), el->GetA()) * nAt[i] * cm;
    }
    out << "\n";
  }
  G4cout << "attenuation table (" << energiesKeV.size() << " energies) written to " << file << G4endl;
}

}  // namespace xrcl
