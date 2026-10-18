// Run-level storage of per-ray results and of detailed refraction traces
// (sequential run manager: one photon = one event = one ray).
#ifndef XRCL_RAY_STORE_HH
#define XRCL_RAY_STORE_HH

#include "PrimaryGeneratorAction.hh"
#include "XrayRefractionProcess.hh"

#include "G4ThreeVector.hh"

#include <memory>
#include <string>
#include <vector>

namespace xrcl {

struct ScreenHit {
  int screen = -1;
  G4ThreeVector position, direction;
};

struct RayRecord {
  std::size_t id = 0;
  double x0 = 0, y0 = 0;
  G4ThreeVector dir0;
  double energy = 0;
  bool reachedEntrance = false, reachedExit = false;
  G4ThreeVector entrancePos, exitPos, exitDir;
  int nRefractions = 0;
  int nTotalReflections = 0;
  int nElementsEntered = 0;
  bool outsideAperture = false;
  bool interacted = false;   // a non-transport process acted (only with EM physics on)
  // ---- interactions of the primary photon (EM physics on) ----
  int nPhot = 0, nCompt = 0, nRayl = 0, nOther = 0;
  bool absorbed = false;          // primary killed by an EM process
  std::string absorbedBy;         // process that killed it
  int absorbedElement = -1;       // lens element where it was absorbed (-1: not in a lens element)
  G4ThreeVector absorbedPos;
  std::string firstProcess;       // first interaction of the primary
  int firstElement = -1;
  G4ThreeVector firstPos;
  double exitEnergy = 0;          // primary energy at the exit plane
  double edepLens = 0;            // energy deposited in the lens material by all particles of the event
  int nSecGamma = 0, nSecElectron = 0;  // secondaries created by the primary
  std::string Fate() const
  {
    if (absorbed) return "absorbed";
    if (reachedExit && !interacted && nTotalReflections == 0 && !outsideAperture) return "unscattered";
    if (reachedExit && interacted) return "scattered_transmitted";
    if (reachedExit) return "outside_aperture";
    return "lost";
  }
  double pathInLens = 0;    // geometric path inside lens material
  double deltaPath = 0;      // sum of delta * path, i.e. geometric minus optical path
  double trackLength = 0;    // geometric path from the source to the exit plane
  std::vector<ScreenHit> screens;
  bool Valid() const { return reachedExit && !outsideAperture && !interacted && nTotalReflections == 0; }
};

struct TraceRow {
  std::size_t event = 0;
  int seq = 0;
  int element = -1;
  int surface = 0;  // 0 = entrance surface (entering), 1 = exit surface (leaving)
  G4ThreeVector position, dirBefore, dirAfter, normal;
  double energy = 0;
  std::string matBefore, matAfter;
  double trackLength = 0;
  double deltaPath = 0;
  std::string status;  // refracted | total_reflection
};

struct InteractionRow {
  std::size_t event = 0;
  int index = 0;            // n-th interaction of this primary
  std::string process;      // phot | compt | Rayl | ...
  int element = -1;         // lens element copy number (-1: outside lens elements)
  G4ThreeVector position;
  double energyBefore = 0, energyAfter = 0;  // energyAfter = 0 if the photon was absorbed
  double cosTheta = 1;      // cosine of the scattering angle
  int nSecGamma = 0, nSecElectron = 0;
  double secGammaEnergy = 0, secElectronEnergy = 0;
};

struct SecondaryExitRow {
  std::size_t event = 0;
  std::string particle, creator;
  int parentId = 0;
  double energy = 0;
  G4ThreeVector position, direction;
};

class RayStore : public RefractionObserver {
 public:
  static RayStore& Instance();

  void ResetRun();
  void BeginRay(std::size_t id, const SourceRay& ray, double energy);
  RayRecord& Current() { return fRays.back(); }
  void OnRefraction(const G4Track& track, const RefractionRecord& rec) override;

  const std::vector<RayRecord>& Rays() const { return fRays; }
  const std::vector<TraceRow>& Trace() const { return fTrace; }
  std::vector<TraceRow>& TraceMutable() { return fTrace; }
  std::vector<InteractionRow>& Interactions() { return fInteractions; }
  std::vector<SecondaryExitRow>& Secondaries() { return fSecondaries; }
  int NextSeq() { return fSeq++; }

  std::unique_ptr<RayGenerator> generator;
  double energy = 0;   // photon energy (Geant4 units)
  int traceEvents = 0;

 private:
  RayStore() = default;
  std::vector<RayRecord> fRays;
  std::vector<TraceRow> fTrace;
  std::vector<InteractionRow> fInteractions;
  std::vector<SecondaryExitRow> fSecondaries;
  int fSeq = 0;
};

}  // namespace xrcl

#endif
