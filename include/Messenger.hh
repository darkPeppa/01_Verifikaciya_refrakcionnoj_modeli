// UI commands (usable in .mac files):
//   /xrcl/refraction/enable <bool>     switch the refraction process on/off
//   /xrcl/source/energy <value> <unit> photon energy (geometry unchanged)
//   /xrcl/source/mode <mode>           axis|offaxis|paraxial|grid|random|fan_x|fan_y
//   /xrcl/set <dotted.key=json>        any configuration override (applies to the next run;
//                                      geometry keys need /run/reinitializeGeometry)
//   /xrcl/output/prefix <name>
//   /xrcl/run/beamOnAll                run one event per generated ray
#ifndef XRCL_MESSENGER_HH
#define XRCL_MESSENGER_HH

#include "G4UImessenger.hh"

class G4UIcmdWithABool;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAString;
class G4UIcmdWithoutParameter;
class G4UIdirectory;

namespace xrcl {

class Messenger : public G4UImessenger {
 public:
  Messenger();
  ~Messenger() override;
  void SetNewValue(G4UIcommand* cmd, G4String value) override;

 private:
  G4UIdirectory* fDir;
  G4UIcmdWithABool* fRefraction;
  G4UIcmdWithADoubleAndUnit* fEnergy;
  G4UIcmdWithAString* fMode;
  G4UIcmdWithAString* fSet;
  G4UIcmdWithAString* fPrefix;
  G4UIcmdWithoutParameter* fBeamOnAll;
};

std::size_t RaysForCurrentConfig();

}  // namespace xrcl

#endif
