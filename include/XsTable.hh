// Table of photon attenuation coefficients mu(E) of the lens material, per process
// (phot, compt, Rayl) and photoelectric contribution of every element, computed with
// G4EmCalculator from the EM models of the selected physics list.
#ifndef XRCL_XS_TABLE_HH
#define XRCL_XS_TABLE_HH

#include <string>
#include <vector>

namespace xrcl {

void WriteXsTable(const std::string& file, const std::vector<double>& energiesKeV);

}  // namespace xrcl

#endif
