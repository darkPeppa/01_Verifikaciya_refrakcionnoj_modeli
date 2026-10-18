#include "Messenger.hh"

#include "Config.hh"
#include "DetectorConstruction.hh"
#include "PrimaryGeneratorAction.hh"

#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIdirectory.hh"

namespace xrcl {

std::size_t RaysForCurrentConfig()
{
  const GeometryInfo& info = GeometryInfo::Instance();
  return RayGenerator(Config::Instance().source, info.slabMode ? 0. : info.layout.a).Count();
}

Messenger::Messenger()
{
  fDir = new G4UIdirectory("/xrcl/");
  fDir->SetGuidance("XrayCRLGeant4 control");
  fRefraction = new G4UIcmdWithABool("/xrcl/refraction/enable", this);
  fRefraction->SetGuidance("Enable/disable X-ray refraction at material boundaries");
  fEnergy = new G4UIcmdWithADoubleAndUnit("/xrcl/source/energy", this);
  fEnergy->SetGuidance("Photon energy");
  fEnergy->SetDefaultUnit("keV");
  fMode = new G4UIcmdWithAString("/xrcl/source/mode", this);
  fMode->SetCandidates("axis offaxis paraxial grid random fan_x fan_y");
  fSet = new G4UIcmdWithAString("/xrcl/set", this);
  fSet->SetGuidance("Configuration override dotted.key=json_value");
  fPrefix = new G4UIcmdWithAString("/xrcl/output/prefix", this);
  fBeamOnAll = new G4UIcmdWithoutParameter("/xrcl/run/beamOnAll", this);
  fBeamOnAll->SetGuidance("Run one event per ray of the configured source pattern");
}

Messenger::~Messenger()
{
  delete fBeamOnAll;
  delete fPrefix;
  delete fSet;
  delete fMode;
  delete fEnergy;
  delete fRefraction;
  delete fDir;
}

void Messenger::SetNewValue(G4UIcommand* cmd, G4String value)
{
  Config& cfg = Config::Instance();
  if (cmd == fRefraction) cfg.Set(std::string("refraction.enabled=") + (fRefraction->GetNewBoolValue(value) ? "true" : "false"));
  else if (cmd == fEnergy) cfg.Set("source.energy_keV=" + std::to_string(fEnergy->GetNewDoubleValue(value) / keV));
  else if (cmd == fMode) cfg.Set("source.mode=\"" + value + "\"");
  else if (cmd == fSet) cfg.Set(value);
  else if (cmd == fPrefix) cfg.Set("output.prefix=\"" + value + "\"");
  else if (cmd == fBeamOnAll) {
    G4RunManager::GetRunManager()->BeamOn(static_cast<G4int>(RaysForCurrentConfig()));
  }
}

}  // namespace xrcl
