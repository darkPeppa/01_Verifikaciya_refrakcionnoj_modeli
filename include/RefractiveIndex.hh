// Real part of the X-ray refractive index, n = 1 - delta.
// Convention: complex index n = 1 - delta + i*beta for fields ~ exp(-i*omega*t)
// (beta > 0 means attenuation). Only delta is used in stage 1.
// delta is stored and propagated as such (never as 1 - delta) to keep full precision.
#ifndef XRCL_REFRACTIVE_INDEX_HH
#define XRCL_REFRACTIVE_INDEX_HH

#include "Config.hh"

#include "globals.hh"

#include <map>
#include <memory>
#include <string>
#include <vector>

class G4Material;

namespace xrcl {

class DeltaModel {
 public:
  virtual ~DeltaModel() = default;
  // energy in Geant4 internal units
  virtual G4double Delta(G4double energy) const = 0;
  virtual std::string Describe() const = 0;
};

class ConstantDelta : public DeltaModel {
 public:
  explicit ConstantDelta(G4double d) : fDelta(d) {}
  G4double Delta(G4double) const override { return fDelta; }
  std::string Describe() const override;

 private:
  G4double fDelta;
};

// delta(E) = deltaRef * (E/Eref)^exponent ; exponent = -2 is the free-electron
// asymptote valid far from absorption edges.
class PowerLawDelta : public DeltaModel {
 public:
  PowerLawDelta(G4double deltaRef, G4double energyRef, G4double exponent)
    : fDeltaRef(deltaRef), fEnergyRef(energyRef), fExponent(exponent) {}
  G4double Delta(G4double energy) const override;
  std::string Describe() const override;

 private:
  G4double fDeltaRef, fEnergyRef, fExponent;
};

// Tabulated delta(E) read from CSV (energy_keV,delta); log-log interpolation,
// no extrapolation outside the table (throws).
class TableDelta : public DeltaModel {
 public:
  explicit TableDelta(const std::string& file);
  G4double Delta(G4double energy) const override;
  std::string Describe() const override;

 private:
  std::string fFile;
  std::vector<G4double> fE, fD;
};

// Free-electron (Thomson) limit: delta = r_e * lambda^2 * n_e / (2 pi),
// n_e taken from the G4Material. Ignores anomalous dispersion f'(E): not valid
// near absorption edges (Sb K edge 30.49 keV for SU-8).
class FreeElectronDelta : public DeltaModel {
 public:
  explicit FreeElectronDelta(const G4Material* mat) : fMaterial(mat) {}
  G4double Delta(G4double energy) const override;
  std::string Describe() const override;

 private:
  const G4Material* fMaterial;
};

std::unique_ptr<DeltaModel> MakeDeltaModel(const DeltaSpec& spec, const G4Material* mat);

// Maps materials to delta models. Materials without a model are treated as
// vacuum (delta = 0); the world is G4_Galactic.
class RefractiveIndexRegistry {
 public:
  static RefractiveIndexRegistry& Instance();
  void Register(const G4Material* mat, std::unique_ptr<DeltaModel> model);
  void Clear() { fModels.clear(); }
  G4double Delta(const G4Material* mat, G4double energy) const;
  const DeltaModel* Model(const G4Material* mat) const;

 private:
  RefractiveIndexRegistry() = default;
  std::map<const G4Material*, std::unique_ptr<DeltaModel>> fModels;
};

}  // namespace xrcl

#endif
