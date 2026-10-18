#include "Config.hh"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace xrcl {

namespace {

template <typename T>
T Get(const json& j, const char* key, T def)
{
  if (j.is_object() && j.contains(key) && !j.at(key).is_null()) return j.at(key).get<T>();
  return def;
}

std::string ResolvePath(const std::string& base, const std::string& p)
{
  if (p.empty()) return p;
  fs::path path(p);
  if (path.is_absolute()) return path.string();
  return (fs::path(base) / path).lexically_normal().string();
}

}  // namespace

Config& Config::Instance()
{
  static Config instance;
  return instance;
}

json Config::LoadJson(const std::string& path)
{
  std::ifstream in(path);
  if (!in) throw std::runtime_error("Cannot open configuration file: " + path);
  try {
    return json::parse(in, nullptr, true, /*ignore_comments=*/true);
  }
  catch (const json::exception& e) {
    throw std::runtime_error("JSON error in " + path + ": " + e.what());
  }
}

void Config::Load(const std::string& path, const std::vector<std::string>& overrides)
{
  fRoot = LoadJson(path);
  fBaseDir = fs::path(path).parent_path().string();

  // Material and lens may be given inline or as a path relative to the run config.
  for (const char* key : {"material", "lens"}) {
    if (fRoot.contains(key) && fRoot[key].is_string()) {
      const std::string file = ResolvePath(fBaseDir, fRoot[key].get<std::string>());
      json sub = LoadJson(file);
      sub["file"] = file;
      sub["_dir"] = fs::path(file).parent_path().string();
      fRoot[key] = sub;
    }
    else if (fRoot.contains(key)) {
      fRoot[key]["_dir"] = fBaseDir;
    }
  }
  for (const auto& o : overrides) Set(o);
  Parse();
}

void Config::Set(const std::string& assignment)
{
  const auto eq = assignment.find('=');
  if (eq == std::string::npos) throw std::runtime_error("Override must be key=value: " + assignment);
  const std::string key = assignment.substr(0, eq);
  const std::string raw = assignment.substr(eq + 1);
  json value;
  try {
    value = json::parse(raw);
  }
  catch (const json::exception&) {
    value = raw;  // plain string
  }
  if ((key == "material" || key == "lens") && value.is_string()) {
    // a path: load the referenced file (relative to the run configuration)
    const std::string file = ResolvePath(fBaseDir, value.get<std::string>());
    json sub = LoadJson(file);
    sub["file"] = file;
    sub["_dir"] = fs::path(file).parent_path().string();
    fRoot[key] = sub;
    Parse();
    return;
  }
  json* node = &fRoot;
  std::stringstream ss(key);
  std::string part;
  std::vector<std::string> parts;
  while (std::getline(ss, part, '.')) parts.push_back(part);
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    if (!(*node).contains(parts[i]) || !(*node)[parts[i]].is_object()) (*node)[parts[i]] = json::object();
    node = &(*node)[parts[i]];
  }
  (*node)[parts.back()] = value;
  if (!fRoot.is_null()) Parse();
}

void Config::Parse()
{
  name = Get<std::string>(fRoot, "name", "run");

  // ---- material ----
  const json& m = fRoot.value("material", json::object());
  material = MaterialSpec{};
  material.name = Get<std::string>(m, "name", "SU8");
  material.file = Get<std::string>(m, "file", "");
  material.densityGcm3 = Get<double>(m, "density_g_cm3", 1.24);
  material.densityUncertaintyGcm3 = Get<double>(m, "density_uncertainty_g_cm3", 0.);
  material.normalize = Get<bool>(m, "normalize_mass_fractions", true);
  material.massPercentSource.clear();
  if (m.contains("mass_percent")) {
    // keep the published order: array of [symbol, percent]
    for (const auto& item : m["mass_percent"]) {
      material.massPercentSource.emplace_back(item.at(0).get<std::string>(), item.at(1).get<double>());
    }
  }
  material.betaRef = Get<double>(m, "beta_ref", 0.);
  material.betaRefEnergyKeV = Get<double>(m, "beta_ref_energy_keV", 25.);
  const json& d = m.value("delta", json::object());
  material.delta.model = Get<std::string>(d, "model", "power_law");
  material.delta.value = Get<double>(d, "value", 0.);
  material.delta.deltaRef = Get<double>(d, "delta_ref", 0.);
  material.delta.energyRefKeV = Get<double>(d, "energy_ref_keV", 25.);
  material.delta.exponent = Get<double>(d, "exponent", -2.);
  material.delta.tableFile = ResolvePath(Get<std::string>(m, "_dir", fBaseDir), Get<std::string>(d, "table_file", ""));
  material.delta.source = Get<std::string>(d, "source", "");

  // ---- lens ----
  const json& l = fRoot.value("lens", json::object());
  lens = LensSpec{};
  lens.name = Get<std::string>(l, "name", "lens");
  lens.source = Get<std::string>(l, "source", "");
  lens.file = Get<std::string>(l, "file", "");
  lens.radiusUm = Get<double>(l, "R_um", 0.);
  lens.apertureUm = Get<double>(l, "A_um", 0.);
  lens.nPerAxis = Get<int>(l, "N_per_axis", 0);
  lens.lengthUm = Get<double>(l, "p_um", 0.);
  lens.webUm = Get<double>(l, "d_um", 10.);
  lens.frameUm = Get<double>(l, "frame_um", 0.);
  lens.heightUm = Get<double>(l, "height_um", 0.);
  lens.nominalLengthMm = Get<double>(l, "L_nominal_mm", 0.);
  lens.gapMode = Get<std::string>(l, "gap_mode", "from_length");
  lens.gapUm = Get<double>(l, "gap_um", 0.);
  lens.reference = l.value("reference_focal_cm", json::object());

  // ---- system ----
  const json& s = fRoot.value("system", json::object());
  system = SystemSpec{};
  system.mode = Get<std::string>(s, "mode", "full");
  system.nPerAxisOverride = Get<int>(s, "N_per_axis", 0);
  system.firstOrientation = Get<std::string>(s, "first_orientation", "X");
  system.slabThicknessUm = Get<double>(s, "slab_thickness_um", 100.);
  system.slabTiltDeg = Get<double>(s, "slab_tilt_deg", 0.);

  // ---- geometry ----
  const json& g = fRoot.value("geometry", json::object());
  geometry = GeometrySpec{};
  geometry.surface = Get<std::string>(g, "surface", "analytic");
  geometry.segments = Get<int>(g, "segments", 64);
  geometry.checkOverlaps = Get<bool>(g, "check_overlaps", true);
  geometry.planeOffsetUm = Get<double>(g, "plane_offset_um", 5.);

  // ---- refraction ----
  const json& r = fRoot.value("refraction", json::object());
  refraction = RefractionSpec{};
  refraction.enabled = Get<bool>(r, "enabled", true);
  if (r.contains("delta_override") && !r["delta_override"].is_null()) {
    refraction.hasDeltaOverride = true;
    refraction.deltaOverride = r["delta_override"].get<double>();
  }

  // ---- source ----
  const json& src = fRoot.value("source", json::object());
  source = SourceSpec{};
  source.energyKeV = Get<double>(src, "energy_keV", 25.);
  source.mode = Get<std::string>(src, "mode", "grid");
  source.gridN = Get<int>(src, "grid_n", 21);
  source.fanN = Get<int>(src, "fan_n", 11);
  source.apertureFraction = Get<double>(src, "aperture_fraction", 0.9);
  source.paraxialFraction = Get<double>(src, "paraxial_fraction", 0.01);
  if (src.contains("offset_um")) {
    source.offsetUm[0] = src["offset_um"].at(0).get<double>();
    source.offsetUm[1] = src["offset_um"].at(1).get<double>();
  }
  if (src.contains("tilt_urad")) {
    source.tiltUrad[0] = src["tilt_urad"].at(0).get<double>();
    source.tiltUrad[1] = src["tilt_urad"].at(1).get<double>();
  }
  if (src.contains("divergence_urad")) {
    source.divergenceUrad[0] = src["divergence_urad"].at(0).get<double>();
    source.divergenceUrad[1] = src["divergence_urad"].at(1).get<double>();
  }
  source.nRandom = Get<int>(src, "n_random", 1000);
  source.seed = Get<long>(src, "seed", 12345);
  source.distanceMm = Get<double>(src, "distance_mm", 1.);

  // ---- output ----
  const json& o = fRoot.value("output", json::object());
  output = OutputSpec{};
  output.directory = Get<std::string>(o, "directory", "results/" + name);
  output.prefix = Get<std::string>(o, "prefix", name);
  output.traceEvents = Get<int>(o, "trace_events", 0);
  output.rmsScanPoints = Get<int>(o, "rms_scan_points", 401);
  output.writeRays = Get<bool>(o, "write_rays", true);
  output.writeInteractions = Get<bool>(o, "write_interactions", true);
  output.writeSecondaries = Get<bool>(o, "write_secondaries", true);

  // ---- physics ----
  const json& ph = fRoot.value("physics", json::object());
  physics = PhysicsSpec{};
  physics.em = Get<std::string>(ph, "em", "none");
  physics.fluo = Get<bool>(ph, "fluo", true);
  physics.auger = Get<bool>(ph, "auger", false);

  xsTableFile.clear();
  xsEnergiesKeV.clear();
  if (fRoot.contains("xs_table") && fRoot["xs_table"].is_object()) {
    xsTableFile = Get<std::string>(fRoot["xs_table"], "file", "");
    for (const auto& v : fRoot["xs_table"].value("energies_keV", json::array())) xsEnergiesKeV.push_back(v.get<double>());
  }
  screensMm.clear();
  if (fRoot.contains("screens_mm")) {
    for (const auto& v : fRoot["screens_mm"]) screensMm.push_back(v.get<double>());
  }
}

}  // namespace xrcl
