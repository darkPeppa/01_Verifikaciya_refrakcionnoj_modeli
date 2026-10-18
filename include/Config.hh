// Central run configuration: every physical and numerical parameter of a run is read
// from JSON files (run config -> material file + lens file) and can be overridden from
// the command line with --set dotted.path=value. No physical constants live in C++ code
// other than those of CLHEP/Geant4.
#ifndef XRCL_CONFIG_HH
#define XRCL_CONFIG_HH

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace xrcl {

using json = nlohmann::json;

struct DeltaSpec {
  // constant | power_law | table | free_electron
  std::string model = "power_law";
  double value = 0.;          // constant model
  double deltaRef = 0.;       // power_law: delta at energyRefKeV
  double energyRefKeV = 25.;  // power_law reference energy
  double exponent = -2.;      // power_law exponent (delta ~ E^exponent)
  std::string tableFile;      // table: CSV energy_keV,delta (log-log interpolation)
  std::string source;         // provenance string, copied to the summary
};

struct MaterialSpec {
  std::string name = "SU8";
  double densityGcm3 = 1.24;
  double densityUncertaintyGcm3 = 0.;
  std::vector<std::pair<std::string, double>> massPercentSource;  // as published
  bool normalize = true;
  DeltaSpec delta;
  double betaRef = 0.;  // stored for later stages (absorption); not used in stage 1
  double betaRefEnergyKeV = 25.;
  std::string file;
};

struct LensSpec {
  std::string name;
  std::string source;
  double radiusUm = 0.;      // R, parabola vertex radius
  double apertureUm = 0.;    // A, design aperture (table value)
  int nPerAxis = 0;          // N, elements focusing in ONE transverse direction
  double lengthUm = 0.;      // p, longitudinal size of one element
  double webUm = 10.;        // d, central web thickness
  double frameUm = 0.;       // optional solid frame beyond the parabolic aperture
  double heightUm = 0.;      // extent in the non-focusing direction (0 -> automatic)
  double nominalLengthMm = 0.;  // L, nominal total length of the full set
  std::string gapMode = "from_length";  // from_length | fixed
  double gapUm = 0.;         // used when gapMode == fixed
  json reference;            // reference focal lengths (cm) from the source table, by keV
  std::string file;
};

struct SystemSpec {
  // single | pair | full | slab
  std::string mode = "full";
  int nPerAxisOverride = 0;           // > 0 overrides lens N for the full set
  std::string firstOrientation = "X";  // focusing axis of the first element
  double slabThicknessUm = 100.;
  double slabTiltDeg = 0.;            // slab normal tilted in the x-z plane
};

struct GeometrySpec {
  std::string surface = "analytic";  // analytic | tessellated
  int segments = 64;                 // tessellated: polygon segments per parabola
  bool checkOverlaps = true;
  double planeOffsetUm = 5.;         // distance of registration planes from lens faces
};

struct RefractionSpec {
  bool enabled = true;
  bool hasDeltaOverride = false;  // replace the lens material delta by a constant
  double deltaOverride = 0.;
};

struct SourceSpec {
  double energyKeV = 25.;
  // axis | offaxis | paraxial | grid | random | fan_x | fan_y
  std::string mode = "grid";
  int gridN = 21;
  int fanN = 11;
  double apertureFraction = 0.9;    // grid/random/fan extent relative to the half aperture
  double paraxialFraction = 0.01;   // paraxial heights h and 2h, relative to half aperture
  double offsetUm[2] = {0., 0.};
  double tiltUrad[2] = {0., 0.};
  double divergenceUrad[2] = {0., 0.};  // Gaussian sigma of ray angles (finite source size / divergence)
  int nRandom = 1000;
  long seed = 12345;
  double distanceMm = 1.;  // source plane distance upstream of the lens entrance
};

struct OutputSpec {
  std::string directory = "results";
  std::string prefix = "run";
  int traceEvents = 0;   // number of events whose every refraction is written
  int rmsScanPoints = 401;
  bool writeRays = true;
  bool writeInteractions = true;  // every interaction of the primary photons
  bool writeSecondaries = true;   // secondary photons crossing the exit plane
};

struct PhysicsSpec {
  std::string em = "none";  // none | livermore | penelope | option4  (stage 1: none)
  bool fluo = true;         // atomic de-excitation: fluorescence (Sb K lines etc.)
  bool auger = false;       // Auger electron cascade
};

class Config {
 public:
  static Config& Instance();

  void Load(const std::string& path, const std::vector<std::string>& overrides);
  // Re-apply a single "a.b.c=value" override (also used by UI commands).
  void Set(const std::string& assignment);

  const json& Resolved() const { return fRoot; }
  std::string name;
  MaterialSpec material;
  LensSpec lens;
  SystemSpec system;
  GeometrySpec geometry;
  RefractionSpec refraction;
  SourceSpec source;
  OutputSpec output;
  PhysicsSpec physics;
  std::vector<double> screensMm;  // registration screens downstream of the exit plane
  // optional: table of attenuation coefficients mu(E) of the lens material (then no tracking)
  std::string xsTableFile;
  std::vector<double> xsEnergiesKeV;

 private:
  Config() = default;
  void Parse();
  static json LoadJson(const std::string& path);
  json fRoot;
  std::string fBaseDir;
};

}  // namespace xrcl

#endif
