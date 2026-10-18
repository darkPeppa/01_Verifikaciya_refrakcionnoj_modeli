// World, lens elements (exact analytic solids or polygonal comparison solids),
// SU-8 test slab, and vacuum registration planes.
#ifndef XRCL_DETECTOR_CONSTRUCTION_HH
#define XRCL_DETECTOR_CONSTRUCTION_HH

#include "LensLayout.hh"
#include "SU8MaterialBuilder.hh"

#include "G4VUserDetectorConstruction.hh"

#include <vector>

class G4LogicalVolume;
class G4Material;
class G4VPhysicalVolume;

namespace xrcl {

// Geometry facts shared with the user actions and the output (single-threaded).
struct GeometryInfo {
  static GeometryInfo& Instance();
  LensLayout layout;
  MaterialReport materialReport;
  const G4Material* lensMaterial = nullptr;
  const G4LogicalVolume* elementLV = nullptr;
  const G4LogicalVolume* slabLV = nullptr;
  const G4VPhysicalVolume* entrancePlane = nullptr;
  const G4VPhysicalVolume* exitPlane = nullptr;
  std::vector<const G4VPhysicalVolume*> screens;
  std::vector<double> screenZ;
  double zSource = 0, zEntrancePlane = 0, zExitPlane = 0;
  double worldHalfXY = 0, worldZMin = 0, worldZMax = 0;
  bool slabMode = false;
  double slabTilt = 0, slabThickness = 0;
};

class DetectorConstruction : public G4VUserDetectorConstruction {
 public:
  DetectorConstruction() = default;
  G4VPhysicalVolume* Construct() override;

 private:
  G4LogicalVolume* MakePlane(const char* name, double halfXY, G4LogicalVolume* world, double z, G4Material* vac,
                             bool check, const G4VPhysicalVolume** out);
};

}  // namespace xrcl

#endif
