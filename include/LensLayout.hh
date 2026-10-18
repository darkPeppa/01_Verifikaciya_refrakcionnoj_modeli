// Longitudinal layout of a compound refractive lens: element positions, orientations,
// gaps and the consistency checks of the input parameters. Pure geometry, no Geant4
// run-time dependency (lengths in Geant4 units, i.e. mm).
#ifndef XRCL_LENS_LAYOUT_HH
#define XRCL_LENS_LAYOUT_HH

#include "Config.hh"

#include <string>
#include <vector>

namespace xrcl {

enum class FocusAxis { X, Y };

struct ElementPlacement {
  int index = 0;
  FocusAxis axis = FocusAxis::X;
  double zCenter = 0.;
};

struct LensLayout {
  // element geometry
  double R = 0, d = 0, p = 0, frame = 0, halfHeight = 0;
  double a = 0;          // parabolic half-aperture sqrt(R (p - d))
  double tableAperture = 0;  // A from the source table
  // system
  std::string mode;
  int nPerAxisDesign = 0;    // N of the design (per transverse direction)
  int nX = 0, nY = 0;        // elements focusing in x / in y actually built
  double gap = 0;            // free space between neighbouring elements
  double gapFromNominal = 0; // gap implied by the nominal length (may be negative)
  double nominalLength = 0;  // L of the full design
  double builtLength = 0;    // first entrance face to last exit face
  double zStart = 0, zEnd = 0;
  std::vector<ElementPlacement> elements;
  std::vector<std::string> notes;     // decisions taken while resolving the layout
  std::vector<std::string> warnings;  // inconsistencies of the input data

  static LensLayout Build(const LensSpec& lens, const SystemSpec& sys);
  int NumFocusing(FocusAxis ax) const { return ax == FocusAxis::X ? nX : nY; }
};

}  // namespace xrcl

#endif
