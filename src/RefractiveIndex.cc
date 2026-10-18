#include "RefractiveIndex.hh"

#include "G4Material.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace xrcl {

std::string ConstantDelta::Describe() const
{
  std::ostringstream os;
  os << "constant delta = " << fDelta;
  return os.str();
}

G4double PowerLawDelta::Delta(G4double energy) const
{
  return fDeltaRef * std::pow(energy / fEnergyRef, fExponent);
}

std::string PowerLawDelta::Describe() const
{
  std::ostringstream os;
  os << "power law delta = " << fDeltaRef << " * (E / " << fEnergyRef / keV << " keV)^" << fExponent;
  return os.str();
}

TableDelta::TableDelta(const std::string& file) : fFile(file)
{
  std::ifstream in(file);
  if (!in) throw std::runtime_error("Cannot open delta table " + file);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::replace(line.begin(), line.end(), ',', ' ');
    std::istringstream ss(line);
    double e, d;
    if (!(ss >> e >> d)) continue;  // header line
    fE.push_back(e * keV);
    fD.push_back(d);
  }
  if (fE.size() < 2) throw std::runtime_error("Delta table needs >= 2 rows: " + file);
  for (std::size_t i = 1; i < fE.size(); ++i) {
    if (!(fE[i] > fE[i - 1])) throw std::runtime_error("Delta table energies must increase: " + file);
    if (fD[i] <= 0 || fD[i - 1] <= 0) throw std::runtime_error("Delta table values must be > 0: " + file);
  }
}

G4double TableDelta::Delta(G4double energy) const
{
  if (energy < fE.front() * (1 - 1e-12) || energy > fE.back() * (1 + 1e-12)) {
    std::ostringstream os;
    os << "Energy " << energy / keV << " keV outside delta table " << fFile;
    throw std::runtime_error(os.str());
  }
  auto it = std::upper_bound(fE.begin(), fE.end(), energy);
  std::size_t i = std::min<std::size_t>(std::max<std::ptrdiff_t>(it - fE.begin(), 1), fE.size() - 1);
  const double t = std::log(energy / fE[i - 1]) / std::log(fE[i] / fE[i - 1]);
  return std::exp(std::log(fD[i - 1]) + t * std::log(fD[i] / fD[i - 1]));
}

std::string TableDelta::Describe() const { return "table delta(E) from " + fFile; }

G4double FreeElectronDelta::Delta(G4double energy) const
{
  const G4double lambda = twopi * hbarc / energy;
  return classic_electr_radius * lambda * lambda * fMaterial->GetElectronDensity() / twopi;
}

std::string FreeElectronDelta::Describe() const
{
  return "free-electron delta = r_e lambda^2 n_e / (2 pi), n_e of " + fMaterial->GetName();
}

std::unique_ptr<DeltaModel> MakeDeltaModel(const DeltaSpec& spec, const G4Material* mat)
{
  if (spec.model == "constant") return std::make_unique<ConstantDelta>(spec.value);
  if (spec.model == "power_law") {
    if (spec.deltaRef <= 0) throw std::runtime_error("power_law delta model needs delta_ref > 0");
    return std::make_unique<PowerLawDelta>(spec.deltaRef, spec.energyRefKeV * keV, spec.exponent);
  }
  if (spec.model == "table") return std::make_unique<TableDelta>(spec.tableFile);
  if (spec.model == "free_electron") return std::make_unique<FreeElectronDelta>(mat);
  throw std::runtime_error("Unknown delta model: " + spec.model);
}

RefractiveIndexRegistry& RefractiveIndexRegistry::Instance()
{
  static RefractiveIndexRegistry instance;
  return instance;
}

void RefractiveIndexRegistry::Register(const G4Material* mat, std::unique_ptr<DeltaModel> model)
{
  fModels[mat] = std::move(model);
}

G4double RefractiveIndexRegistry::Delta(const G4Material* mat, G4double energy) const
{
  auto it = fModels.find(mat);
  return it == fModels.end() ? 0. : it->second->Delta(energy);
}

const DeltaModel* RefractiveIndexRegistry::Model(const G4Material* mat) const
{
  auto it = fModels.find(mat);
  return it == fModels.end() ? nullptr : it->second.get();
}

}  // namespace xrcl
