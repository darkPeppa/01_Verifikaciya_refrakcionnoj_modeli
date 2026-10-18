// Modular physics list. Stage 1 (refraction verification): transportation +
// XrayRefractionProcess only, so geometric optics is tested without any
// stochastic interaction. Later stages switch on a Geant4 EM constructor with
// physics.em = livermore | penelope | option4; the refraction process is then
// simply added on top of it.
#ifndef XRCL_PHYSICS_LIST_HH
#define XRCL_PHYSICS_LIST_HH

#include "G4VModularPhysicsList.hh"
#include "G4VPhysicsConstructor.hh"

#include "Config.hh"

#include <string>

namespace xrcl {

class XrayRefractionPhysics : public G4VPhysicsConstructor {
 public:
  XrayRefractionPhysics() : G4VPhysicsConstructor("XrayRefraction") {}
  void ConstructParticle() override;
  void ConstructProcess() override;
};

class PhysicsList : public G4VModularPhysicsList {
 public:
  explicit PhysicsList(const PhysicsSpec& spec);
  void SetCuts() override;
};

}  // namespace xrcl

#endif
