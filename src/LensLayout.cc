#include "LensLayout.hh"

#include "G4SystemOfUnits.hh"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace xrcl {

LensLayout LensLayout::Build(const LensSpec& lens, const SystemSpec& sys)
{
  LensLayout L;
  L.mode = sys.mode;
  if (sys.mode == "slab") return L;

  L.R = lens.radiusUm * um;
  L.d = lens.webUm * um;
  L.p = lens.lengthUm * um;
  L.frame = lens.frameUm * um;
  L.tableAperture = lens.apertureUm * um;
  if (L.R <= 0 || L.d <= 0 || L.p <= L.d) {
    std::ostringstream os;
    os << "Lens '" << lens.name << "': need R > 0, d > 0 and p > d (R=" << lens.radiusUm << " um, d=" << lens.webUm
       << " um, p=" << lens.lengthUm << " um)";
    throw std::runtime_error(os.str());
  }
  L.a = std::sqrt(L.R * (L.p - L.d));
  {
    std::ostringstream os;
    os << "parabolic aperture 2*sqrt(R(p-d)) = " << 2 * L.a / um << " um (d = " << L.d / um
       << " um); source-table aperture A = " << lens.apertureUm << " um";
    L.notes.push_back(os.str());
    if (L.tableAperture > 0 && std::abs(2 * L.a - L.tableAperture) > 1e-3 * L.tableAperture) {
      std::ostringstream w;
      w << "aperture implied by (R, p, d) differs from table A by " << (2 * L.a - L.tableAperture) / um
        << " um (relative " << (2 * L.a - L.tableAperture) / L.tableAperture << ")";
      L.warnings.push_back(w.str());
    }
  }
  if (lens.heightUm > 0) {
    L.halfHeight = 0.5 * lens.heightUm * um;
  }
  else {
    L.halfHeight = 2. * (L.a + L.frame);
    std::ostringstream os;
    os << "element height (non-focusing extent) not given: using 4*(a+frame) = " << 2 * L.halfHeight / um << " um";
    L.notes.push_back(os.str());
  }
  if (L.halfHeight < L.a + L.frame) {
    L.warnings.push_back("element height smaller than the aperture: orthogonal elements do not cover the aperture");
  }

  L.nPerAxisDesign = lens.nPerAxis;
  const int nDesign = 2 * lens.nPerAxis;
  L.nominalLength = lens.nominalLengthMm * mm;
  if (nDesign > 1 && L.nominalLength > 0) {
    L.gapFromNominal = (L.nominalLength - nDesign * L.p) / (nDesign - 1);
  }
  if (lens.gapMode == "fixed") {
    L.gap = lens.gapUm * um;
    std::ostringstream os;
    os << "gap fixed by configuration: " << L.gap / um << " um";
    L.notes.push_back(os.str());
  }
  else if (lens.gapMode == "from_length") {
    if (nDesign < 2 || L.nominalLength <= 0) {
      L.gap = 0;
      L.notes.push_back("no nominal length: elements placed without gaps");
    }
    else if (L.gapFromNominal < 0) {
      L.gap = 0;
      std::ostringstream w;
      w << "nominal length L = " << L.nominalLength / mm << " mm is shorter than 2*N*p = " << nDesign * L.p / mm
        << " mm (gap would be " << L.gapFromNominal / um
        << " um, i.e. overlapping elements); gap set to 0 and the length reconstructed as 2*N*p";
      L.warnings.push_back(w.str());
    }
    else {
      L.gap = L.gapFromNominal;
      std::ostringstream os;
      os << "gap derived from nominal length: (L - 2Np)/(2N - 1) = " << L.gap / um << " um";
      L.notes.push_back(os.str());
    }
  }
  else {
    throw std::runtime_error("Unknown gap_mode " + lens.gapMode);
  }

  const FocusAxis first = (sys.firstOrientation == "Y" || sys.firstOrientation == "y") ? FocusAxis::Y : FocusAxis::X;
  const FocusAxis second = (first == FocusAxis::X) ? FocusAxis::Y : FocusAxis::X;
  int nElements = 0;
  if (sys.mode == "single") nElements = 1;
  else if (sys.mode == "pair") nElements = 2;
  else if (sys.mode == "full") {
    const int n = sys.nPerAxisOverride > 0 ? sys.nPerAxisOverride : lens.nPerAxis;
    if (sys.nPerAxisOverride > 0 && sys.nPerAxisOverride != lens.nPerAxis) {
      std::ostringstream os;
      os << "N per axis overridden: " << n << " instead of design " << lens.nPerAxis << " (gap kept)";
      L.notes.push_back(os.str());
    }
    nElements = 2 * n;
  }
  else throw std::runtime_error("Unknown system mode " + sys.mode);
  if (nElements < 1) throw std::runtime_error("Lens '" + lens.name + "': no elements to build");

  for (int i = 0; i < nElements; ++i) {
    ElementPlacement e;
    e.index = i;
    e.axis = (i % 2 == 0) ? first : second;
    e.zCenter = 0.5 * L.p + i * (L.p + L.gap);
    L.elements.push_back(e);
    if (e.axis == FocusAxis::X) ++L.nX;
    else ++L.nY;
  }
  L.zStart = 0.;
  L.builtLength = nElements * L.p + (nElements - 1) * L.gap;
  L.zEnd = L.builtLength;
  if (sys.mode == "full" && L.nominalLength > 0) {
    std::ostringstream os;
    os << "built length " << L.builtLength / mm << " mm (nominal " << L.nominalLength / mm << " mm, 2N p = "
       << nElements * L.p / mm << " mm)";
    L.notes.push_back(os.str());
  }
  return L;
}

}  // namespace xrcl
