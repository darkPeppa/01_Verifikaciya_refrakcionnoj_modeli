#include "XrayRefractionProcess.hh"

#include "RefractiveIndex.hh"
#include "XrayRefraction.hh"

#include "G4AffineTransform.hh"
#include "G4Gamma.hh"
#include "G4GeometryTolerance.hh"
#include "G4NavigationHistory.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4Track.hh"
#include "G4VSolid.hh"
#include "G4VTouchable.hh"

#include <cfloat>

namespace xrcl {

G4bool XrayRefractionProcess::fEnabled = true;
RefractionObserver* XrayRefractionProcess::fObserver = nullptr;

XrayRefractionProcess::XrayRefractionProcess(const G4String& name) : G4VDiscreteProcess(name, fUserDefined)
{
  fTolerance = G4GeometryTolerance::GetInstance()->GetSurfaceTolerance();
}

G4bool XrayRefractionProcess::IsApplicable(const G4ParticleDefinition& particle)
{
  return &particle == G4Gamma::Definition();
}

G4double XrayRefractionProcess::GetMeanFreePath(const G4Track&, G4double, G4ForceCondition* condition)
{
  *condition = Forced;
  return DBL_MAX;
}

G4VParticleChange* XrayRefractionProcess::PostStepDoIt(const G4Track& track, const G4Step& step)
{
  aParticleChange.Initialize(track);
  if (!fEnabled) return &aParticleChange;

  const G4StepPoint* pre = step.GetPreStepPoint();
  const G4StepPoint* post = step.GetPostStepPoint();
  if (post->GetStepStatus() != fGeomBoundary) return &aParticleChange;
  // A step of zero length re-crosses the surface just handled (e.g. after total
  // reflection the navigator moves the photon back): never refract it twice.
  if (step.GetStepLength() <= fTolerance) return &aParticleChange;

  const G4Material* m1 = pre->GetMaterial();
  const G4Material* m2 = post->GetMaterial();
  if (m1 == nullptr || m2 == nullptr || m1 == m2) return &aParticleChange;

  const G4double energy = track.GetKineticEnergy();
  const auto& registry = RefractiveIndexRegistry::Instance();
  const G4double d1 = registry.Delta(m1, energy);
  const G4double d2 = registry.Delta(m2, energy);
  if (d1 == d2) return &aParticleChange;

  // The surface belongs to the deeper volume of the two (the daughter that is
  // entered or left); its solid provides the exact local normal.
  const G4VTouchable* tPre = pre->GetTouchable();
  const G4VTouchable* tPost = post->GetTouchable();
  const G4bool usePost = tPost != nullptr && (tPre == nullptr || tPost->GetHistoryDepth() >= tPre->GetHistoryDepth());
  const G4VTouchable* owner = usePost ? tPost : tPre;
  const G4AffineTransform& toLocal = owner->GetHistory()->GetTopTransform();
  const G4ThreeVector localPoint = toLocal.TransformPoint(post->GetPosition());
  const G4ThreeVector localNormal = owner->GetSolid()->SurfaceNormal(localPoint);
  const G4ThreeVector normal = toLocal.InverseTransformAxis(localNormal);

  const G4ThreeVector k = track.GetMomentumDirection();
  const RefractionResult r = Refract(k, normal, d1, d2);
  aParticleChange.ProposeMomentumDirection(r.direction);

  if (fObserver) {
    RefractionRecord rec;
    rec.position = post->GetPosition();
    rec.directionBefore = k;
    rec.directionAfter = r.direction;
    rec.normal = (normal.dot(k) < 0) ? -normal.unit() : normal.unit();
    rec.energy = energy;
    rec.materialBefore = m1;
    rec.materialAfter = m2;
    rec.copyNumber = owner->GetCopyNumber();
    rec.entering = usePost;
    rec.totalReflection = r.totalReflection;
    rec.delta1 = d1;
    rec.delta2 = d2;
    fObserver->OnRefraction(track, rec);
  }
  return &aParticleChange;
}

}  // namespace xrcl
