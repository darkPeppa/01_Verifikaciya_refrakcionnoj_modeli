// Stepping / event / run actions and the action initialisation.
#ifndef XRCL_USER_ACTIONS_HH
#define XRCL_USER_ACTIONS_HH

#include "G4UserRunAction.hh"
#include "G4UserSteppingAction.hh"
#include "G4VUserActionInitialization.hh"

namespace xrcl {

class SteppingAction : public G4UserSteppingAction {
 public:
  void UserSteppingAction(const G4Step* step) override;
};

class RunAction : public G4UserRunAction {
 public:
  void BeginOfRunAction(const G4Run* run) override;
  void EndOfRunAction(const G4Run* run) override;
};

class ActionInitialization : public G4VUserActionInitialization {
 public:
  void Build() const override;
};

}  // namespace xrcl

#endif
