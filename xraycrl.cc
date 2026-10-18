// XrayCRLGeant4 - ray tracing of X-rays through compound refractive lenses of
// orthogonally crossed linear parabolic elements (stage 1: refraction verification).
//
// usage: xraycrl --config run.json [--set key=value]... [--macro file.mac] [--vis vis.mac]
//   Without --macro the application runs one event per ray of the configured source.
#include "Config.hh"
#include "XsTable.hh"
#include "DetectorConstruction.hh"
#include "Messenger.hh"
#include "PhysicsList.hh"
#include "UserActions.hh"

#include "G4RunManagerFactory.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "G4ios.hh"
#include "Randomize.hh"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
void Usage()
{
  std::cerr << "usage: xraycrl --config run.json [--set key=value]... [--macro file.mac] [--vis vis.mac]\n";
}
}  // namespace

int main(int argc, char** argv)
{
  std::string configFile, macro, visMacro;
  std::vector<std::string> overrides;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        Usage();
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--config") configFile = next();
    else if (a == "--set") overrides.push_back(next());
    else if (a == "--macro") macro = next();
    else if (a == "--vis") visMacro = next();
    else {
      Usage();
      return 2;
    }
  }
  if (configFile.empty()) {
    Usage();
    return 2;
  }

  try {
    xrcl::Config& cfg = xrcl::Config::Instance();
    cfg.Load(configFile, overrides);
    CLHEP::HepRandom::setTheSeed(cfg.source.seed);  // reproducible (no stochastic physics in stage 1)

    std::unique_ptr<G4RunManager> rm(G4RunManagerFactory::CreateRunManager(G4RunManagerType::Serial));
    rm->SetUserInitialization(new xrcl::DetectorConstruction());
    rm->SetUserInitialization(new xrcl::PhysicsList(cfg.physics));
    rm->SetUserInitialization(new xrcl::ActionInitialization());
    xrcl::Messenger messenger;

    G4UImanager* ui = G4UImanager::GetUIpointer();
    ui->ApplyCommand("/control/verbose 0");
    ui->ApplyCommand("/run/verbose 0");
    ui->ApplyCommand("/event/verbose 0");
    ui->ApplyCommand("/tracking/verbose 0");
    rm->Initialize();

    std::unique_ptr<G4VisManager> vis;
    if (!visMacro.empty()) {
      vis = std::make_unique<G4VisExecutive>("quiet");
      vis->Initialize();
      ui->ApplyCommand("/control/execute " + visMacro);
    }
    if (!cfg.xsTableFile.empty()) {
      rm->BeamOn(0);  // builds the physics tables
      xrcl::WriteXsTable(cfg.xsTableFile, cfg.xsEnergiesKeV);
      return 0;
    }
    if (!macro.empty()) {
      ui->ApplyCommand("/control/execute " + macro);
    }
    else {
      rm->BeamOn(static_cast<G4int>(xrcl::RaysForCurrentConfig()));
    }
  }
  catch (const std::exception& e) {
    G4cerr << "ERROR: " << e.what() << G4endl;
    return 1;
  }
  return 0;
}
