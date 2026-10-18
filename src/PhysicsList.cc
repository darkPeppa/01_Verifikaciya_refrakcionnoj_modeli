#include "PhysicsList.hh"

#include "XrayRefractionProcess.hh"

#include "G4Electron.hh"
#include "G4EmParameters.hh"
#include "G4EmLivermorePhysics.hh"
#include "G4EmPenelopePhysics.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4Gamma.hh"
#include "G4Geantino.hh"
#include "G4Positron.hh"
#include "G4ProcessManager.hh"
#include "G4SystemOfUnits.hh"

#include <stdexcept>

namespace xrcl {

void XrayRefractionPhysics::ConstructParticle()
{
  G4Gamma::Definition();
  G4Electron::Definition();
  G4Positron::Definition();
  G4Geantino::Definition();
}

void XrayRefractionPhysics::ConstructProcess()
{
  // Registered directly with the process manager: a discrete process whose
  // PostStepDoIt runs after G4Transportation (ordering index > transportation).
  G4ProcessManager* pm = G4Gamma::Definition()->GetProcessManager();
  pm->AddDiscreteProcess(new XrayRefractionProcess());
}

PhysicsList::PhysicsList(const PhysicsSpec& spec)
{
  SetVerboseLevel(0);
  const std::string& em = spec.em;
  if (em == "livermore") RegisterPhysics(new G4EmLivermorePhysics());
  else if (em == "penelope") RegisterPhysics(new G4EmPenelopePhysics());
  else if (em == "option4") RegisterPhysics(new G4EmStandardPhysics_option4());
  else if (em != "none") throw std::runtime_error("Unknown physics.em option: " + em);
  if (em != "none") {
    // Set after the EM constructor (it resets the parameters to its own defaults).
    G4EmParameters* p = G4EmParameters::Instance();
    p->SetGeneralProcessActive(false);  // keep phot / compt / Rayl as separate, named processes
    p->SetFluo(spec.fluo);
    p->SetAuger(spec.auger);
    p->SetPixe(false);
    p->SetVerbose(0);
  }
  RegisterPhysics(new XrayRefractionPhysics());
}

void PhysicsList::SetCuts()
{
  // Micro-structures of tens of micrometres: production cuts must be checked
  // explicitly once EM physics is enabled (stage 2); 1 um is used as default.
  SetDefaultCutValue(1. * um);
  G4VUserPhysicsList::SetCuts();
}

}  // namespace xrcl
