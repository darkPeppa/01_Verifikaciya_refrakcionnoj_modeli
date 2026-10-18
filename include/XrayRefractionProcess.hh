// Geant4 process that refracts X-ray photons (G4Gamma) at material boundaries.
//
// Mechanics (same scheme as G4OpBoundaryProcess for optical photons):
//  * GetMeanFreePath returns DBL_MAX with condition Forced, so the process never
//    limits the step but its PostStepDoIt runs after every step, *after*
//    G4Transportation has relocated the track into the next volume.
//  * Refraction is applied only if the step ended on a geometric boundary
//    (fGeomBoundary) and the step is longer than the geometric tolerance; a
//    zero-length step is the navigator moving a reflected photon back across the
//    same surface and must not be refracted a second time.
//  * The surface normal is the analytic normal of the solid that owns the surface
//    (the deeper of the pre/post volumes), evaluated in its local frame.
//  * Only the momentum direction is changed; position, energy and polarisation are
//    untouched, so the step itself introduces no lateral displacement.
#ifndef XRCL_XRAY_REFRACTION_PROCESS_HH
#define XRCL_XRAY_REFRACTION_PROCESS_HH

#include "G4ThreeVector.hh"
#include "G4VDiscreteProcess.hh"

class G4Material;

namespace xrcl {

struct RefractionRecord {
  G4ThreeVector position;
  G4ThreeVector directionBefore;
  G4ThreeVector directionAfter;
  G4ThreeVector normal;  // oriented along the propagation
  G4double energy = 0.;
  const G4Material* materialBefore = nullptr;
  const G4Material* materialAfter = nullptr;
  G4int copyNumber = -1;  // copy number of the lens element owning the surface
  G4bool entering = false;  // photon enters the element (else leaves it)
  G4bool totalReflection = false;
  G4double delta1 = 0., delta2 = 0.;
};

class RefractionObserver {
 public:
  virtual ~RefractionObserver() = default;
  virtual void OnRefraction(const G4Track& track, const RefractionRecord& rec) = 0;
};

class XrayRefractionProcess : public G4VDiscreteProcess {
 public:
  explicit XrayRefractionProcess(const G4String& name = "XrayRefraction");
  ~XrayRefractionProcess() override = default;

  G4bool IsApplicable(const G4ParticleDefinition& particle) override;
  G4double GetMeanFreePath(const G4Track& track, G4double previousStepSize, G4ForceCondition* condition) override;
  G4VParticleChange* PostStepDoIt(const G4Track& track, const G4Step& step) override;

  static void SetEnabled(G4bool on) { fEnabled = on; }
  static G4bool IsEnabled() { return fEnabled; }
  static void SetObserver(RefractionObserver* obs) { fObserver = obs; }

 private:
  static G4bool fEnabled;
  static RefractionObserver* fObserver;
  G4double fTolerance;
};

}  // namespace xrcl

#endif
