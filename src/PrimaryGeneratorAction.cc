#include "PrimaryGeneratorAction.hh"

#include "DetectorConstruction.hh"
#include "RayStore.hh"

#include "G4Event.hh"
#include "G4Gamma.hh"
#include "G4ParticleGun.hh"
#include "G4SystemOfUnits.hh"

#include <cmath>
#include <cstdint>
#include <random>
#include <stdexcept>

namespace xrcl {

RayGenerator::RayGenerator(const SourceSpec& s, double a)
{
  const double ox = s.offsetUm[0] * um, oy = s.offsetUm[1] * um;
  const G4ThreeVector dir = G4ThreeVector(std::tan(s.tiltUrad[0] * 1e-6), std::tan(s.tiltUrad[1] * 1e-6), 1.).unit();
  // independent, reproducible generator for the angular spread
  std::mt19937_64 rngDiv(static_cast<std::uint64_t>(s.seed) ^ 0x9E3779B97F4A7C15ULL);
  std::normal_distribution<double> gaus(0., 1.);
  const bool spread = s.divergenceUrad[0] > 0 || s.divergenceUrad[1] > 0;
  auto push = [&](double x, double y) {
    SourceRay r;
    r.x = x + ox;
    r.y = y + oy;
    r.direction = dir;
    if (spread) {
      const double tx = s.tiltUrad[0] * 1e-6 + s.divergenceUrad[0] * 1e-6 * gaus(rngDiv);
      const double ty = s.tiltUrad[1] * 1e-6 + s.divergenceUrad[1] * 1e-6 * gaus(rngDiv);
      r.direction = G4ThreeVector(std::tan(tx), std::tan(ty), 1.).unit();
    }
    fRays.push_back(r);
  };
  const double ext = s.apertureFraction * a;
  if (s.mode == "axis" || s.mode == "offaxis") {
    push(0, 0);
  }
  else if (s.mode == "paraxial") {
    // symmetric pairs at heights h and 2h in x and in y (Richardson extrapolation h -> 0)
    const double h = s.paraxialFraction * a;
    for (double m : {1., 2.}) {
      push(m * h, 0);
      push(-m * h, 0);
      push(0, m * h);
      push(0, -m * h);
    }
  }
  else if (s.mode == "grid") {
    if (s.gridN < 2) throw std::runtime_error("source.grid_n must be >= 2");
    for (int i = 0; i < s.gridN; ++i)
      for (int j = 0; j < s.gridN; ++j)
        push(-ext + 2. * ext * i / (s.gridN - 1), -ext + 2. * ext * j / (s.gridN - 1));
  }
  else if (s.mode == "fan_x" || s.mode == "fan_y") {
    if (s.fanN < 2) throw std::runtime_error("source.fan_n must be >= 2");
    for (int i = 0; i < s.fanN; ++i) {
      const double u = -ext + 2. * ext * i / (s.fanN - 1);
      if (s.mode == "fan_x") push(u, 0);
      else push(0, u);
    }
  }
  else if (s.mode == "random") {
    std::mt19937_64 rng(static_cast<std::uint64_t>(s.seed));
    std::uniform_real_distribution<double> uni(-ext, ext);
    for (int i = 0; i < s.nRandom; ++i) {
      const double x = uni(rng);
      const double y = uni(rng);
      push(x, y);
    }
  }
  else {
    throw std::runtime_error("Unknown source.mode " + s.mode);
  }
}

PrimaryGeneratorAction::PrimaryGeneratorAction() : fGun(new G4ParticleGun(1))
{
  fGun->SetParticleDefinition(G4Gamma::Definition());
}

PrimaryGeneratorAction::~PrimaryGeneratorAction() { delete fGun; }

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  RayStore& store = RayStore::Instance();
  const auto id = static_cast<std::size_t>(event->GetEventID());
  if (!store.generator || id >= store.generator->Count()) {
    throw std::runtime_error("More events than rays: use /xrcl/run/beamOnAll or the default batch run");
  }
  const SourceRay& r = store.generator->Ray(id);
  const double energy = store.energy;
  fGun->SetParticleEnergy(energy);
  fGun->SetParticlePosition(G4ThreeVector(r.x, r.y, GeometryInfo::Instance().zSource));
  fGun->SetParticleMomentumDirection(r.direction);
  fGun->SetParticlePolarization(G4ThreeVector(1, 0, 0));
  fGun->GeneratePrimaryVertex(event);
  store.BeginRay(id, r, energy);
}

}  // namespace xrcl
