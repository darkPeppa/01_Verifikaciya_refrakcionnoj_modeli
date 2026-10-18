#include "RayStore.hh"

#include "G4Material.hh"
#include "G4Track.hh"

namespace xrcl {

RayStore& RayStore::Instance()
{
  static RayStore store;
  return store;
}

void RayStore::ResetRun()
{
  fRays.clear();
  fTrace.clear();
  fInteractions.clear();
  fSecondaries.clear();
}

void RayStore::BeginRay(std::size_t id, const SourceRay& ray, double e)
{
  RayRecord r;
  r.id = id;
  r.x0 = ray.x;
  r.y0 = ray.y;
  r.dir0 = ray.direction;
  r.energy = e;
  fRays.push_back(r);
  fSeq = 0;
}

void RayStore::OnRefraction(const G4Track& track, const RefractionRecord& rec)
{
  if (fRays.empty()) return;
  RayRecord& r = fRays.back();
  ++r.nRefractions;
  if (rec.totalReflection) ++r.nTotalReflections;
  if (static_cast<int>(r.id) >= traceEvents) return;
  TraceRow t;
  t.event = r.id;
  t.seq = fSeq++;
  t.element = rec.copyNumber;
  t.surface = rec.entering ? 0 : 1;
  t.position = rec.position;
  t.dirBefore = rec.directionBefore;
  t.dirAfter = rec.directionAfter;
  t.normal = rec.normal;
  t.energy = rec.energy;
  t.matBefore = rec.materialBefore ? rec.materialBefore->GetName() : "";
  t.matAfter = rec.materialAfter ? rec.materialAfter->GetName() : "";
  t.trackLength = track.GetTrackLength();
  t.deltaPath = r.deltaPath;
  t.status = rec.totalReflection ? "total_reflection" : "refracted";
  fTrace.push_back(t);
}

}  // namespace xrcl
