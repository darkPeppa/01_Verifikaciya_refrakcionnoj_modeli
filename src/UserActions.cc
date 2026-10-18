#include "UserActions.hh"

#include "Config.hh"
#include "DetectorConstruction.hh"
#include "OutputWriter.hh"
#include "PrimaryGeneratorAction.hh"
#include "RayStore.hh"
#include "RefractiveIndex.hh"
#include "XrayRefractionProcess.hh"

#include "G4Electron.hh"
#include "G4Gamma.hh"
#include "G4GeometryTolerance.hh"
#include "G4Material.hh"
#include "G4LogicalVolume.hh"
#include "G4Run.hh"
#include "G4Step.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4VProcess.hh"
#include "G4ios.hh"

#include <cmath>

namespace xrcl {

namespace {

int ElementOf(const G4StepPoint* pt, const GeometryInfo& info)
{
  const G4VPhysicalVolume* pv = pt->GetPhysicalVolume();
  if (pv && info.elementLV && pv->GetLogicalVolume() == info.elementLV) return pv->GetCopyNo();
  return -1;
}

}  // namespace

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  const GeometryInfo& info = GeometryInfo::Instance();
  RayStore& store = RayStore::Instance();
  RayRecord& r = store.Current();
  G4Track* track = step->GetTrack();
  const G4StepPoint* pre = step->GetPreStepPoint();
  const G4StepPoint* post = step->GetPostStepPoint();
  const G4VPhysicalVolume* postPV = post->GetPhysicalVolume();

  // energy deposited in the lens polymer by every particle of the event
  if (pre->GetMaterial() == info.lensMaterial) r.edepLens += step->GetTotalEnergyDeposit();

  if (track->GetParentID() != 0) {
    // secondary photons (fluorescence, scattered secondaries) crossing the exit plane
    if (postPV && postPV == info.exitPlane && post->GetStepStatus() == fGeomBoundary &&
        track->GetDefinition() == G4Gamma::Definition()) {
      if (Config::Instance().output.writeSecondaries) {
        SecondaryExitRow sr;
        sr.event = r.id;
        sr.particle = "gamma";
        sr.creator = track->GetCreatorProcess() ? track->GetCreatorProcess()->GetProcessName() : "";
        sr.parentId = track->GetParentID();
        sr.energy = post->GetKineticEnergy();
        sr.position = post->GetPosition();
        sr.direction = post->GetMomentumDirection();
        store.Secondaries().push_back(sr);
      }
      if (info.screens.empty()) track->SetTrackStatus(fStopAndKill);
    }
    return;
  }

  const double len = step->GetStepLength();
  const G4Material* mat = pre->GetMaterial();
  if (mat == info.lensMaterial) r.pathInLens += len;
  r.deltaPath += RefractiveIndexRegistry::Instance().Delta(mat, track->GetKineticEnergy()) * len;

  // ---------------- interactions of the primary photon ----------------
  const G4VProcess* proc = post->GetProcessDefinedStep();
  if (proc && proc->GetProcessType() != fTransportation && proc->GetProcessType() != fUserDefined) {
    r.interacted = true;
    const G4String& pname = proc->GetProcessName();
    if (pname == "phot") ++r.nPhot;
    else if (pname == "compt") ++r.nCompt;
    else if (pname == "Rayl") ++r.nRayl;
    else ++r.nOther;
    const int el = ElementOf(pre, info);
    const bool killed = track->GetTrackStatus() == fStopAndKill || track->GetTrackStatus() == fStopButAlive;
    if (r.firstProcess.empty()) {
      r.firstProcess = pname;
      r.firstElement = el;
      r.firstPos = post->GetPosition();
    }
    InteractionRow row;
    row.event = r.id;
    row.index = r.nPhot + r.nCompt + r.nRayl + r.nOther - 1;
    row.process = pname;
    row.element = el;
    row.position = post->GetPosition();
    row.energyBefore = pre->GetKineticEnergy();
    row.energyAfter = killed ? 0. : post->GetKineticEnergy();
    row.cosTheta = killed ? 1. : pre->GetMomentumDirection().dot(post->GetMomentumDirection());
    if (const auto* sec = step->GetSecondaryInCurrentStep()) {
      for (const G4Track* t : *sec) {
        if (t->GetDefinition() == G4Gamma::Definition()) {
          ++row.nSecGamma;
          row.secGammaEnergy += t->GetKineticEnergy();
        }
        else if (t->GetDefinition() == G4Electron::Definition()) {
          ++row.nSecElectron;
          row.secElectronEnergy += t->GetKineticEnergy();
        }
      }
    }
    r.nSecGamma += row.nSecGamma;
    r.nSecElectron += row.nSecElectron;
    if (killed) {
      r.absorbed = true;
      r.absorbedBy = pname;
      r.absorbedElement = el;
      r.absorbedPos = post->GetPosition();
    }
    if (Config::Instance().output.writeInteractions) store.Interactions().push_back(row);
    if (static_cast<int>(r.id) < store.traceEvents) {
      TraceRow t;
      t.event = r.id;
      t.seq = store.NextSeq();
      t.element = el;
      t.surface = -1;
      t.position = post->GetPosition();
      t.dirBefore = pre->GetMomentumDirection();
      t.dirAfter = killed ? G4ThreeVector() : post->GetMomentumDirection();
      t.energy = pre->GetKineticEnergy();
      t.matBefore = t.matAfter = mat ? mat->GetName() : "";
      t.trackLength = track->GetTrackLength();
      t.deltaPath = r.deltaPath;
      t.status = pname;
      store.TraceMutable().push_back(t);
    }
  }

  if (post->GetStepStatus() != fGeomBoundary) return;
  if (!postPV) return;  // leaving the world

  static const double tol = G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
  if (info.elementLV && postPV->GetLogicalVolume() == info.elementLV && len > tol) {
    ++r.nElementsEntered;
    const auto& el = info.layout.elements.at(static_cast<std::size_t>(postPV->GetCopyNo()));
    const double u = (el.axis == FocusAxis::X) ? post->GetPosition().x() : post->GetPosition().y();
    if (std::abs(u) > info.layout.a * (1. + 1e-9)) r.outsideAperture = true;  // entered through a flat face
  }
  if (postPV == info.entrancePlane) {
    r.reachedEntrance = true;
    r.entrancePos = post->GetPosition();
  }
  else if (postPV == info.exitPlane && !r.reachedExit) {
    r.reachedExit = true;
    r.exitPos = post->GetPosition();
    r.exitDir = post->GetMomentumDirection();
    r.exitEnergy = post->GetKineticEnergy();
    r.trackLength = track->GetTrackLength();
    // A ray inside the aperture enters each element exactly once.
    if (!info.slabMode && !r.interacted && r.nElementsEntered != static_cast<int>(info.layout.elements.size())) {
      r.outsideAperture = true;
    }
    if (info.screens.empty()) track->SetTrackStatus(fStopAndKill);
  }
  else {
    for (std::size_t i = 0; i < info.screens.size(); ++i) {
      if (postPV != info.screens[i]) continue;
      ScreenHit h;
      h.screen = static_cast<int>(i);
      h.position = post->GetPosition();
      h.direction = post->GetMomentumDirection();
      r.screens.push_back(h);
      if (i + 1 == info.screens.size()) track->SetTrackStatus(fStopAndKill);
    }
  }
}

void RunAction::BeginOfRunAction(const G4Run*)
{
  const Config& cfg = Config::Instance();
  RayStore& store = RayStore::Instance();
  store.ResetRun();
  store.energy = cfg.source.energyKeV * keV;
  store.traceEvents = cfg.output.traceEvents;
  const double a = GeometryInfo::Instance().slabMode ? 0. : GeometryInfo::Instance().layout.a;
  store.generator = std::make_unique<RayGenerator>(cfg.source, a);
  XrayRefractionProcess::SetEnabled(cfg.refraction.enabled);
  XrayRefractionProcess::SetObserver(&store);
}

void RunAction::EndOfRunAction(const G4Run* run)
{
  if (run->GetNumberOfEvent() == 0) return;
  WriteRunOutput();
}

void ActionInitialization::Build() const
{
  SetUserAction(new PrimaryGeneratorAction());
  SetUserAction(new RunAction());
  SetUserAction(new SteppingAction());
}

}  // namespace xrcl
