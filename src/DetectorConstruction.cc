#include "DetectorConstruction.hh"

#include "Config.hh"
#include "ParabolicLensSolid.hh"
#include "RefractiveIndex.hh"

#include "G4Box.hh"
#include "G4ExtrudedSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace xrcl {

namespace {
constexpr double kPlaneHalfThickness = 0.05 * um;
}

GeometryInfo& GeometryInfo::Instance()
{
  static GeometryInfo info;
  return info;
}

G4LogicalVolume* DetectorConstruction::MakePlane(const char* name, double halfXY, G4LogicalVolume* world, double z,
                                                 G4Material* vac, bool check, const G4VPhysicalVolume** out)
{
  auto* box = new G4Box(name, halfXY, halfXY, kPlaneHalfThickness);
  auto* lv = new G4LogicalVolume(box, vac, name);
  lv->SetVisAttributes(G4VisAttributes::GetInvisible());
  // the plane is centred so that its upstream face lies at z
  *out = new G4PVPlacement(nullptr, G4ThreeVector(0, 0, z + kPlaneHalfThickness), lv, name, world, false, 0, check);
  return lv;
}

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  const Config& cfg = Config::Instance();
  GeometryInfo& info = GeometryInfo::Instance();
  info = GeometryInfo{};

  auto* nist = G4NistManager::Instance();
  G4Material* vacuum = nist->FindOrBuildMaterial("G4_Galactic");
  G4Material* lensMat = SU8MaterialBuilder::Build(cfg.material, &info.materialReport);
  info.lensMaterial = lensMat;

  auto& registry = RefractiveIndexRegistry::Instance();
  registry.Clear();
  if (cfg.refraction.hasDeltaOverride) registry.Register(lensMat, std::make_unique<ConstantDelta>(cfg.refraction.deltaOverride));
  else registry.Register(lensMat, MakeDeltaModel(cfg.material.delta, lensMat));

  const bool check = cfg.geometry.checkOverlaps;
  const double planeOffset = cfg.geometry.planeOffsetUm * um;

  // ---------------- longitudinal extent ----------------
  info.layout = LensLayout::Build(cfg.lens, cfg.system);
  const LensLayout& L = info.layout;
  double zFirst = 0, zLast = 0, lateral = 0;
  if (cfg.system.mode == "slab") {
    info.slabMode = true;
    info.slabTilt = cfg.system.slabTiltDeg * deg;
    info.slabThickness = cfg.system.slabThicknessUm * um;
    const double slabHalf = 5. * mm;
    const double zExt = std::abs(0.5 * info.slabThickness * std::cos(info.slabTilt)) +
                        std::abs(slabHalf * std::sin(info.slabTilt));
    zFirst = -zExt;
    zLast = zExt;
    lateral = 2. * slabHalf;
  }
  else {
    zFirst = L.zStart;
    zLast = L.zEnd;
    lateral = std::max(L.halfHeight, L.a + L.frame);
  }
  lateral = std::max({lateral, std::abs(cfg.source.offsetUm[0]) * um, std::abs(cfg.source.offsetUm[1]) * um});
  info.zEntrancePlane = zFirst - planeOffset;
  info.zExitPlane = zLast + planeOffset;
  info.zSource = zFirst - cfg.source.distanceMm * mm;
  if (info.zSource >= info.zEntrancePlane - 1. * um) {
    throw std::runtime_error("source.distance_mm must exceed the registration plane offset");
  }
  double zMax = info.zExitPlane;
  for (double s : cfg.screensMm) {
    const double z = zLast + s * mm;
    if (z <= info.zExitPlane + 1. * um) throw std::runtime_error("screens_mm must lie beyond the exit plane");
    zMax = std::max(zMax, z);
  }
  // Rays may diverge after the lens; the world is made wide enough for screens
  // within a few focal lengths, rays leaving it laterally are simply lost.
  info.worldHalfXY = 3. * lateral + 1. * mm;
  info.worldZMin = info.zSource - 1. * mm;
  info.worldZMax = zMax + 1. * mm;
  // The world is centred on the origin so that lens coordinates are global
  // coordinates (z = 0 at the entrance face of the first element).
  const double halfZ = std::max(std::abs(info.worldZMin), std::abs(info.worldZMax));
  info.worldZMin = -halfZ;
  info.worldZMax = halfZ;

  // ---------------- world ----------------
  auto* worldBox = new G4Box("World", info.worldHalfXY, info.worldHalfXY, halfZ);
  auto* worldLV = new G4LogicalVolume(worldBox, vacuum, "World");
  worldLV->SetVisAttributes(G4VisAttributes::GetInvisible());
  auto* worldPV = new G4PVPlacement(nullptr, G4ThreeVector(), worldLV, "World", nullptr, false, 0, false);

  auto* lensVis = new G4VisAttributes(G4Colour(0.2, 0.5, 0.9, 0.6));
  lensVis->SetForceSolid(true);

  if (info.slabMode) {
    auto* slab = new G4Box("Slab", 5. * mm, 5. * mm, 0.5 * info.slabThickness);
    auto* slabLV = new G4LogicalVolume(slab, lensMat, "Slab");
    slabLV->SetVisAttributes(lensVis);
    G4RotationMatrix rot;
    rot.rotateY(info.slabTilt);
    new G4PVPlacement(G4Transform3D(rot, G4ThreeVector()), slabLV, "Slab", worldLV, false, 0, check);
    info.slabLV = slabLV;
  }
  else {
    ParabolicLensSolid::Parameters par;
    par.radius = L.R;
    par.web = L.d;
    par.length = L.p;
    par.frame = L.frame;
    par.halfHeight = L.halfHeight;

    G4VSolid* solid = nullptr;
    G4RotationMatrix frameRot;  // solid frame -> element frame
    if (cfg.geometry.surface == "analytic") {
      solid = new ParabolicLensSolid("CRLElement", par);
    }
    else if (cfg.geometry.surface == "tessellated") {
      // Polygonal cross-section (x, z) extruded along the element y axis.
      solid = new G4ExtrudedSolid("CRLElement", ParabolicLensSolid::CrossSection(par, cfg.geometry.segments),
                                  L.halfHeight);
      frameRot.rotateX(90. * deg);
    }
    else {
      throw std::runtime_error("geometry.surface must be analytic or tessellated");
    }
    auto* elementLV = new G4LogicalVolume(solid, lensMat, "CRLElement");
    elementLV->SetVisAttributes(lensVis);
    info.elementLV = elementLV;

    for (const auto& e : L.elements) {
      G4RotationMatrix rot;
      if (e.axis == FocusAxis::Y) rot.rotateZ(90. * deg);  // local x (focusing) -> global y
      rot = rot * frameRot;
      new G4PVPlacement(G4Transform3D(rot, G4ThreeVector(0, 0, e.zCenter)), elementLV, "CRLElement", worldLV, false,
                        e.index, check);
    }
  }

  const double planeHalf = info.worldHalfXY * 0.999;
  MakePlane("EntrancePlane", planeHalf, worldLV, info.zEntrancePlane, vacuum, check, &info.entrancePlane);
  MakePlane("ExitPlane", planeHalf, worldLV, info.zExitPlane, vacuum, check, &info.exitPlane);
  for (std::size_t i = 0; i < cfg.screensMm.size(); ++i) {
    const double z = zLast + cfg.screensMm[i] * mm;
    const G4VPhysicalVolume* pv = nullptr;
    std::ostringstream name;
    name << "Screen" << i;
    MakePlane(name.str().c_str(), planeHalf, worldLV, z, vacuum, check, &pv);
    info.screens.push_back(pv);
    info.screenZ.push_back(z);
  }
  return worldPV;
}

}  // namespace xrcl
