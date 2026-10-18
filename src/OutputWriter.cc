#include "OutputWriter.hh"

#include "Config.hh"
#include "DetectorConstruction.hh"
#include "FocusAnalysis.hh"
#include "RayStore.hh"
#include "RefractiveIndex.hh"

#include "G4SystemOfUnits.hh"
#include "G4Version.hh"
#include "G4EmCalculator.hh"
#include "G4Gamma.hh"
#include "G4Material.hh"
#include "G4Element.hh"
#include "G4ios.hh"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace xrcl {

namespace {

json Num(double v) { return std::isfinite(v) ? json(v) : json(nullptr); }

std::ofstream OpenCsv(const fs::path& p)
{
  std::ofstream out(p);
  if (!out) throw std::runtime_error("Cannot write " + p.string());
  out << std::setprecision(17);
  return out;
}

json AxisJson(const char* name, const AxisFocus& f, const ParaxialAxis& P, const LensLayout& L, double delta)
{
  const double zEnd = L.zEnd, zMid = 0.5 * (L.zStart + L.zEnd), Lsys = L.zEnd - L.zStart;
  json j;
  j["axis"] = name;
  j["n_focusing_elements"] = P.nFocusing;

  // ---------------- analytic references ----------------
  json an;
  double f0 = kNaN;
  if (P.nFocusing > 0 && delta > 0) {
    f0 = L.R / (2. * P.nFocusing * delta);
    an["f_thin_mm"] = f0 / mm;
    an["f_thin_formula"] = "R / (2 N delta), N = elements focusing in this axis";
    const double x = std::sqrt(Lsys / f0);
    an["continuous_thick_lens"] = {
      {"omega_L", x},
      {"bfl_from_lens_end_mm", Num(f0 * x / std::tan(x) / mm)},
      {"efl_mm", Num(f0 * x / std::sin(x) / mm)},
      {"focus_from_lens_centre_mm", Num((f0 * x / std::tan(x) + 0.5 * Lsys) / mm)},
      {"note", "u'' = -u/(f_thin L): focusing power spread uniformly over the system length L"}};
    an["thesis_formula_1_7_3"] = {{"focus_from_lens_centre_mm", (f0 + Lsys / 6.) / mm},
                                  {"formula", "F_L = R/(delta N_total) + L/6 = f_thin + L/6 (from the centre)"}};
  }
  j["analytic"] = an;

  // ---------------- method C: paraxial matrices ----------------
  json c;
  c["valid"] = P.valid;
  c["matrix_first_to_last_vertex"] = {P.A, P.B, P.C, P.D};
  c["z_focus_mm"] = Num(P.zFocus / mm);
  c["focus_from_lens_end_mm"] = Num((P.zFocus - zEnd) / mm);
  c["focus_from_last_vertex_mm"] = Num(P.bflFromLastVertex / mm);
  c["focus_from_lens_centre_mm"] = Num((P.zFocus - zMid) / mm);
  c["efl_mm"] = Num(P.efl / mm);
  c["z_rear_principal_plane_mm"] = Num(P.zPrincipal / mm);
  j["method_C_paraxial_matrix"] = c;

  // ---------------- method A: paraxial ray pairs ----------------
  json a;
  json pairs = json::array();
  for (const auto& p : f.pairs) {
    pairs.push_back({{"h_um", p.h / um}, {"valid", p.valid}, {"z_cross_mm", Num(p.zCross / mm)},
                     {"lateral_um", Num(p.lateral / um)}, {"efl_mm", Num(p.efl / mm)}});
  }
  a["pairs"] = pairs;
  a["z_focus_mm"] = Num(f.zParaxial / mm);
  a["focus_from_lens_end_mm"] = Num((f.zParaxial - zEnd) / mm);
  a["focus_from_lens_centre_mm"] = Num((f.zParaxial - zMid) / mm);
  a["efl_mm"] = Num(f.eflParaxial / mm);
  a["extrapolation"] = "Richardson h->0 from the two smallest heights";
  j["method_A_ray_pairs"] = a;

  // ---------------- method B: RMS width ----------------
  json b;
  b["n_rays"] = f.nRms;
  b["z_rms_min_mm"] = Num(f.zRmsMin / mm);
  b["rms_min_from_lens_end_mm"] = Num((f.zRmsMin - zEnd) / mm);
  b["rms_min_um"] = Num(f.rmsMin / um);
  b["rms_at_exit_plane_um"] = Num(f.rmsAtExit / um);
  j["method_B_rms"] = b;

  j["max_slope_change_rad"] = f.maxSlopeChange;

  // ---------------- comparisons ----------------
  json cmp;
  if (std::isfinite(f.zParaxial) && P.valid) {
    cmp["A_minus_C_focus_mm"] = (f.zParaxial - P.zFocus) / mm;
    cmp["A_minus_C_focus_rel_to_efl"] = (f.zParaxial - P.zFocus) / P.efl;
    cmp["A_vs_C_efl_rel"] = (f.eflParaxial - P.efl) / P.efl;
  }
  if (std::isfinite(f.zParaxial) && std::isfinite(f0)) {
    cmp["A_efl_vs_thin_rel"] = (f.eflParaxial - f0) / f0;
    cmp["A_efl_minus_thin_mm"] = (f.eflParaxial - f0) / mm;
  }
  if (P.valid && std::isfinite(f0)) cmp["C_efl_vs_thin_rel"] = (P.efl - f0) / f0;
  j["comparison"] = cmp;
  return j;
}

}  // namespace

void WriteRunOutput()
{
  const Config& cfg = Config::Instance();
  const GeometryInfo& info = GeometryInfo::Instance();
  const RayStore& store = RayStore::Instance();
  const auto& rays = store.Rays();
  const LensLayout& L = info.layout;

  const fs::path dir(cfg.output.directory);
  fs::create_directories(dir);
  const std::string pre = cfg.output.prefix;

  const double energy = store.energy;
  const double delta = RefractiveIndexRegistry::Instance().Delta(info.lensMaterial, energy);
  const double ox = cfg.source.offsetUm[0] * um, oy = cfg.source.offsetUm[1] * um;

  // ---------------- rays.csv ----------------
  if (cfg.output.writeRays) {
    auto out = OpenCsv(dir / (pre + "_rays.csv"));
    out << "id,x0_um,y0_um,kx0,ky0,kz0,valid,reached_exit,outside_aperture,interacted,n_refractions,n_total_reflections,"
           "n_elements,x_exit_um,y_exit_um,z_exit_mm,kx,ky,kz,path_in_lens_mm,delta_path_nm,track_length_mm,"
           "fate,n_phot,n_compt,n_rayl,first_process,first_element,first_z_mm,absorbed_by,absorbed_element,"
           "absorbed_z_mm,exit_energy_keV,edep_lens_keV,n_sec_gamma,n_sec_electron\n";
    for (const auto& r : rays) {
      out << r.id << ',' << r.x0 / um << ',' << r.y0 / um << ',' << r.dir0.x() << ',' << r.dir0.y() << ','
          << r.dir0.z() << ',' << r.Valid() << ',' << r.reachedExit << ',' << r.outsideAperture << ','
          << r.interacted << ',' << r.nRefractions << ',' << r.nTotalReflections << ',' << r.nElementsEntered << ','
          << r.exitPos.x() / um << ',' << r.exitPos.y() / um << ',' << r.exitPos.z() / mm << ',' << r.exitDir.x()
          << ',' << r.exitDir.y() << ',' << r.exitDir.z() << ',' << r.pathInLens / mm << ','
          << r.deltaPath / nm << ',' << r.trackLength / mm << ',' << r.Fate() << ',' << r.nPhot << ','
          << r.nCompt << ',' << r.nRayl << ',' << r.firstProcess << ',' << r.firstElement << ','
          << (r.firstProcess.empty() ? 0. : r.firstPos.z() / mm) << ',' << r.absorbedBy << ',' << r.absorbedElement
          << ',' << (r.absorbed ? r.absorbedPos.z() / mm : 0.) << ',' << r.exitEnergy / keV << ','
          << r.edepLens / keV << ',' << r.nSecGamma << ',' << r.nSecElectron << '\n';
    }
  }
  // ---------------- interactions.csv ----------------
  auto& inter = RayStore::Instance().Interactions();
  if (!inter.empty()) {
    auto out = OpenCsv(dir / (pre + "_interactions.csv"));
    out << "event,index,process,element,x_um,y_um,z_mm,energy_before_keV,energy_after_keV,cos_theta,"
           "n_sec_gamma,sec_gamma_energy_keV,n_sec_electron,sec_electron_energy_keV\n";
    for (const auto& i : inter) {
      out << i.event << ',' << i.index << ',' << i.process << ',' << i.element << ',' << i.position.x() / um << ','
          << i.position.y() / um << ',' << i.position.z() / mm << ',' << i.energyBefore / keV << ','
          << i.energyAfter / keV << ',' << i.cosTheta << ',' << i.nSecGamma << ',' << i.secGammaEnergy / keV << ','
          << i.nSecElectron << ',' << i.secElectronEnergy / keV << '\n';
    }
  }
  // ---------------- secondaries_exit.csv ----------------
  auto& secs = RayStore::Instance().Secondaries();
  if (!secs.empty()) {
    auto out = OpenCsv(dir / (pre + "_secondaries_exit.csv"));
    out << "event,particle,creator_process,parent_id,energy_keV,x_um,y_um,z_mm,kx,ky,kz\n";
    for (const auto& t : secs) {
      out << t.event << ',' << t.particle << ',' << t.creator << ',' << t.parentId << ',' << t.energy / keV << ','
          << t.position.x() / um << ',' << t.position.y() / um << ',' << t.position.z() / mm << ','
          << t.direction.x() << ',' << t.direction.y() << ',' << t.direction.z() << '\n';
    }
  }
  // ---------------- element_profile.csv: where the primary photons interact ----------------
  if (cfg.physics.em != "none" && !L.elements.empty()) {
    std::vector<long> nPh(L.elements.size()), nCo(L.elements.size()), nRa(L.elements.size()), nAbs(L.elements.size());
    for (const auto& i : inter) {
      if (i.element < 0) continue;
      const auto k = static_cast<std::size_t>(i.element);
      if (i.process == "phot") ++nPh[k];
      else if (i.process == "compt") ++nCo[k];
      else if (i.process == "Rayl") ++nRa[k];
    }
    for (const auto& r : rays)
      if (r.absorbed && r.absorbedElement >= 0) ++nAbs[static_cast<std::size_t>(r.absorbedElement)];
    auto out = OpenCsv(dir / (pre + "_element_profile.csv"));
    out << "element,focus_axis,z_center_mm,n_phot,n_compt,n_rayl,n_primaries_absorbed\n";
    for (const auto& e : L.elements) {
      const auto k = static_cast<std::size_t>(e.index);
      out << e.index << ',' << (e.axis == FocusAxis::X ? 'X' : 'Y') << ',' << e.zCenter / mm << ',' << nPh[k] << ','
          << nCo[k] << ',' << nRa[k] << ',' << nAbs[k] << '\n';
    }
  }

  // ---------------- trace.csv ----------------
  if (!store.Trace().empty()) {
    auto out = OpenCsv(dir / (pre + "_trace.csv"));
    out << "event,seq,element,surface,x_um,y_um,z_mm,kx_before,ky_before,kz_before,kx_after,ky_after,kz_after,"
           "nx,ny,nz,energy_keV,material_before,material_after,track_length_mm,delta_path_before_step_nm,status\n";
    for (const auto& t : store.Trace()) {
      out << t.event << ',' << t.seq << ',' << t.element << ',' << t.surface << ',' << t.position.x() / um << ','
          << t.position.y() / um << ',' << t.position.z() / mm << ',' << t.dirBefore.x() << ',' << t.dirBefore.y()
          << ',' << t.dirBefore.z() << ',' << t.dirAfter.x() << ',' << t.dirAfter.y() << ',' << t.dirAfter.z() << ','
          << t.normal.x() << ',' << t.normal.y() << ',' << t.normal.z() << ',' << t.energy / keV << ','
          << t.matBefore << ',' << t.matAfter << ',' << t.trackLength / mm << ',' << t.deltaPath / nm << ','
          << t.status << '\n';
    }
  }
  // ---------------- geometry.csv ----------------
  if (!L.elements.empty()) {
    auto out = OpenCsv(dir / (pre + "_geometry.csv"));
    out << "index,focus_axis,z_center_mm,z_entrance_vertex_mm,z_exit_vertex_mm,z_min_mm,z_max_mm\n";
    for (const auto& e : L.elements) {
      out << e.index << ',' << (e.axis == FocusAxis::X ? 'X' : 'Y') << ',' << e.zCenter / mm << ','
          << (e.zCenter - 0.5 * L.d) / mm << ',' << (e.zCenter + 0.5 * L.d) / mm << ',' << (e.zCenter - 0.5 * L.p) / mm
          << ',' << (e.zCenter + 0.5 * L.p) / mm << '\n';
    }
  }

  // ---------------- focus analysis ----------------
  const FocusResult fr = AnalyseFocus(rays, info.zExitPlane, ox, oy);
  // The matrix model describes the same optics as the traced run: no refraction -> delta = 0.
  const double deltaOptics = cfg.refraction.enabled ? delta : 0.;
  const ParaxialAxis px = ComputeParaxial(L, FocusAxis::X, deltaOptics);
  const ParaxialAxis py = ComputeParaxial(L, FocusAxis::Y, deltaOptics);

  // RMS(z) scan for plotting
  {
    double zA = info.zExitPlane, zB = info.zExitPlane;
    for (double z : {fr.x.zRmsMin, fr.y.zRmsMin, px.zFocus, py.zFocus}) {
      if (std::isfinite(z)) {
        zA = std::min(zA, z);
        zB = std::max(zB, z);
      }
    }
    // Straight-line extrapolation is physical only downstream of the exit plane;
    // a virtual focus inside the lens (BFL < 0) is reported but not scanned.
    const double span = std::max(zB - zA, 1. * mm);
    zA = info.zExitPlane;
    zB += 0.3 * span;
    auto out = OpenCsv(dir / (pre + "_rms.csv"));
    out << "z_mm,rms_x_um,rms_y_um\n";
    const int n = std::max(cfg.output.rmsScanPoints, 2);
    for (int i = 0; i < n; ++i) {
      const double z = zA + (zB - zA) * i / (n - 1);
      double rx, ry;
      RmsAt(rays, z, rx, ry);
      out << z / mm << ',' << rx / um << ',' << ry / um << '\n';
    }
  }

  // ---------------- screens: Geant4 transport vs straight-line extrapolation ----------------
  double maxScreenDev = 0;
  int nScreenHits = 0;
  if (!info.screens.empty()) {
    auto out = OpenCsv(dir / (pre + "_screens.csv"));
    out << "id,screen,z_mm,x_um,y_um,x_extrapolated_um,y_extrapolated_um\n";
    for (const auto& r : rays) {
      if (!r.Valid()) continue;
      for (const auto& h : r.screens) {
        const double s = (h.position.z() - r.exitPos.z()) / r.exitDir.z();
        const double xe = r.exitPos.x() + s * r.exitDir.x();
        const double ye = r.exitPos.y() + s * r.exitDir.y();
        maxScreenDev = std::max({maxScreenDev, std::abs(xe - h.position.x()), std::abs(ye - h.position.y())});
        ++nScreenHits;
        out << r.id << ',' << h.screen << ',' << h.position.z() / mm << ',' << h.position.x() / um << ','
            << h.position.y() / um << ',' << xe / um << ',' << ye / um << '\n';
      }
    }
  }

  // ---------------- summary.json ----------------
  json s;
  s["name"] = cfg.name;
  s["geant4_version"] = G4Version;
  s["energy_keV"] = energy / keV;
  s["refraction_enabled"] = cfg.refraction.enabled;
  s["config"] = cfg.Resolved();

  json mat;
  mat["name"] = cfg.material.name;
  mat["density_g_cm3"] = cfg.material.densityGcm3;
  json src = json::array(), used = json::array();
  for (const auto& [el, pc] : info.materialReport.sourcePercent) src.push_back({el, pc});
  for (const auto& [el, w] : info.materialReport.usedFraction) used.push_back({el, w});
  mat["mass_percent_source"] = src;
  mat["mass_percent_source_sum"] = info.materialReport.sourceSum;
  mat["mass_fraction_used"] = used;
  mat["electron_density_per_cm3"] = info.materialReport.electronDensityPerCm3;
  mat["delta"] = delta;
  const DeltaModel* model = RefractiveIndexRegistry::Instance().Model(info.lensMaterial);
  mat["delta_model"] = model ? model->Describe() : "none";
  mat["delta_source"] = cfg.refraction.hasDeltaOverride ? "refraction.delta_override" : cfg.material.delta.source;
  mat["delta_free_electron_estimate"] = FreeElectronDelta(info.lensMaterial).Delta(energy);
  s["material"] = mat;

  json lay;
  lay["mode"] = cfg.system.mode;
  if (info.slabMode) {
    lay["slab_thickness_um"] = info.slabThickness / um;
    lay["slab_tilt_deg"] = info.slabTilt / deg;
  }
  else {
    lay["lens_name"] = cfg.lens.name;
    lay["lens_source"] = cfg.lens.source;
    lay["surface_representation"] = cfg.geometry.surface;
    if (cfg.geometry.surface == "tessellated") lay["segments_per_parabola"] = cfg.geometry.segments;
    lay["R_um"] = L.R / um;
    lay["d_um"] = L.d / um;
    lay["p_um"] = L.p / um;
    lay["parabolic_aperture_um"] = 2 * L.a / um;
    lay["table_aperture_um"] = L.tableAperture / um;
    lay["frame_um"] = L.frame / um;
    lay["height_um"] = 2 * L.halfHeight / um;
    lay["N_per_axis_design"] = L.nPerAxisDesign;
    lay["n_elements"] = L.elements.size();
    lay["n_focusing_x"] = L.nX;
    lay["n_focusing_y"] = L.nY;
    lay["gap_um"] = L.gap / um;
    lay["gap_from_nominal_length_um"] = L.gapFromNominal / um;
    lay["nominal_length_mm"] = L.nominalLength / mm;
    lay["built_length_mm"] = L.builtLength / mm;
    lay["z_start_mm"] = L.zStart / mm;
    lay["z_end_mm"] = L.zEnd / mm;
    lay["notes"] = L.notes;
    lay["warnings"] = L.warnings;
  }
  lay["z_source_mm"] = info.zSource / mm;
  lay["z_entrance_plane_mm"] = info.zEntrancePlane / mm;
  lay["z_exit_plane_mm"] = info.zExitPlane / mm;
  s["layout"] = lay;

  int nExit = 0, nValid = 0, nOut = 0, nTir = 0, nInter = 0, minRefr = 1 << 30, maxRefr = 0;
  for (const auto& r : rays) {
    nExit += r.reachedExit;
    nValid += r.Valid();
    nOut += r.outsideAperture;
    nTir += (r.nTotalReflections > 0);
    nInter += r.interacted;
    if (r.Valid()) {
      minRefr = std::min(minRefr, r.nRefractions);
      maxRefr = std::max(maxRefr, r.nRefractions);
    }
  }
  s["rays"] = {{"generated", rays.size()},
               {"reached_exit_plane", nExit},
               {"valid", nValid},
               {"outside_aperture", nOut},
               {"with_total_reflection", nTir},
               {"interacted", nInter},
               {"min_refractions_valid", nValid ? minRefr : 0},
               {"max_refractions_valid", maxRefr},
               {"expected_refractions_per_ray", 2 * L.elements.size()},
               {"source_mode", cfg.source.mode}};

  if (!info.slabMode) {
    json focus;
    focus["x"] = AxisJson("x", fr.x, px, L, deltaOptics);
    focus["y"] = AxisJson("y", fr.y, py, L, deltaOptics);
    if (std::isfinite(fr.x.zParaxial) && std::isfinite(fr.y.zParaxial)) {
      focus["astigmatism_A_zx_minus_zy_mm"] = (fr.x.zParaxial - fr.y.zParaxial) / mm;
    }
    if (px.valid && py.valid) focus["astigmatism_C_zx_minus_zy_mm"] = (px.zFocus - py.zFocus) / mm;
    s["focus"] = focus;

    // reference value of the source table for this energy, if any
    std::ostringstream key;
    key << std::lround(energy / keV);
    if (std::abs(energy / keV - std::lround(energy / keV)) < 1e-9 && cfg.lens.reference.contains(key.str())) {
      const double fTable = cfg.lens.reference[key.str()].get<double>() * cm;
      json ref;
      ref["table_focal_cm"] = fTable / cm;
      ref["table_N_per_axis"] = L.nPerAxisDesign;
      if (px.nFocusing > 0 && delta > 0) {
        const double f0 = L.R / (2. * px.nFocusing * delta);
        ref["thin_lens_cm"] = f0 / cm;
        ref["table_vs_thin_rel"] = (fTable - f0) / f0;
      }
      if (std::isfinite(fr.x.zParaxial)) {
        ref["traced_focus_from_lens_end_cm"] = (fr.x.zParaxial - L.zEnd) / cm;
        ref["traced_efl_cm"] = fr.x.eflParaxial / cm;
      }
      s["reference_table"] = ref;
    }
  }
  if (cfg.physics.em != "none") {
    json in;
    const double n = static_cast<double>(rays.size());
    std::map<std::string, long> fate, first;
    long nInt[3] = {0, 0, 0}, nWith[3] = {0, 0, 0}, nSecG = 0, nSecE = 0;
    double edep = 0;
    for (const auto& r : rays) {
      ++fate[r.Fate()];
      if (!r.firstProcess.empty()) ++first[r.firstProcess];
      nInt[0] += r.nPhot;
      nInt[1] += r.nCompt;
      nInt[2] += r.nRayl;
      nWith[0] += r.nPhot > 0;
      nWith[1] += r.nCompt > 0;
      nWith[2] += r.nRayl > 0;
      nSecG += r.nSecGamma;
      nSecE += r.nSecElectron;
      edep += r.edepLens;
    }
    json jf;
    for (const char* f : {"unscattered", "scattered_transmitted", "absorbed", "lost", "outside_aperture"}) {
      const double k = static_cast<double>(fate[f]);
      jf[f] = {{"count", fate[f]}, {"fraction", k / n}, {"stat_error", std::sqrt(std::max(k, 1.) * (1 - k / n)) / n}};
    }
    in["fate"] = jf;
    in["fate_definitions"] = {
      {"unscattered", "reached the exit plane without any interaction (contributes to the image/focus)"},
      {"scattered_transmitted", "reached the exit plane after >= 1 Compton/Rayleigh scattering (background)"},
      {"absorbed", "primary photon terminated by an interaction (photoelectric absorption)"},
      {"lost", "left the world without crossing the exit plane (scattered sideways/backwards)"},
      {"outside_aperture", "no interaction but passed outside the parabolic aperture"}};
    const char* pn[3] = {"phot", "compt", "Rayl"};
    json jp;
    for (int i = 0; i < 3; ++i) {
      jp[pn[i]] = {{"interactions", nInt[i]}, {"photons_with_at_least_one", nWith[i]},
                   {"fraction_of_photons", nWith[i] / n}, {"first_interaction_count", first[pn[i]]},
                   {"first_interaction_fraction", first[pn[i]] / n}};
    }
    in["processes"] = jp;
    in["mean_interactions_per_photon"] = (nInt[0] + nInt[1] + nInt[2]) / n;
    in["energy_deposit_in_lens"] = {{"total_keV", edep / keV}, {"per_photon_keV", edep / keV / n},
                                    {"fraction_of_beam_energy", edep / (n * energy)}};
    long nExitSec = static_cast<long>(secs.size());
    double eSec = 0;
    for (const auto& t : secs) eSec += t.energy;
    in["secondaries"] = {{"gammas_created_by_primaries", nSecG}, {"electrons_created_by_primaries", nSecE},
                         {"secondary_gammas_at_exit_plane", nExitSec},
                         {"mean_energy_secondary_gammas_at_exit_keV", nExitSec ? eSec / nExitSec / keV : 0.}};
    // Attenuation coefficients of the lens material from the Geant4 models used in this run
    G4EmCalculator calc;
    const G4Material* m = info.lensMaterial;
    const double rho = m->GetDensity() / (g / cm3);
    json mu;
    double muTot = 0;
    for (const char* p : pn) {
      const double v = calc.ComputeCrossSectionPerVolume(energy, G4Gamma::Definition(), p, m) * cm;
      muTot += v;
      mu[p] = {{"mu_per_cm", v}, {"mu_over_rho_cm2_g", v / rho}};
    }
    mu["total"] = {{"mu_per_cm", muTot}, {"mu_over_rho_cm2_g", muTot / rho}};
    json byEl;
    const G4ElementVector* els = m->GetElementVector();
    const G4double* nAt = m->GetVecNbOfAtomsPerVolume();
    for (std::size_t i = 0; i < m->GetNumberOfElements(); ++i) {
      const G4Element* el = (*els)[i];
      const double sp = calc.ComputeCrossSectionPerAtom(energy, G4Gamma::Definition(), "phot", el->GetZ(),
                                                        el->GetA());
      byEl[el->GetSymbol()] = {{"phot_mu_per_cm", sp * nAt[i] * cm}};
    }
    mu["photoelectric_by_element"] = byEl;
    mu["source"] = "G4EmCalculator, models of physics list '" + cfg.physics.em + "'";
    in["attenuation_coefficients"] = mu;
    in["physics"] = {{"em", cfg.physics.em}, {"fluorescence", cfg.physics.fluo}, {"auger", cfg.physics.auger}};
    s["interactions"] = in;
    G4cout << "  fate: unscattered " << fate["unscattered"] / n << ", scattered " << fate["scattered_transmitted"] / n
           << ", absorbed " << fate["absorbed"] / n << ", lost " << fate["lost"] / n << "; mu_tot = " << muTot
           << " 1/cm\n";
  }
  if (!info.screens.empty()) {
    s["screens_check"] = {{"hits", nScreenHits}, {"max_deviation_um", maxScreenDev / um},
                          {"meaning", "Geant4-transported position minus straight-line extrapolation from exit plane"}};
  }
  {
    std::ofstream out(dir / (pre + "_summary.json"));
    out << std::setw(2) << s << '\n';
  }

  // ---------------- console summary ----------------
  G4cout << "\n=== XrayCRLGeant4 run '" << cfg.name << "' : E = " << energy / keV << " keV, delta = " << delta
         << " ===\n  rays: " << rays.size() << " generated, " << nValid << " valid, " << nOut << " outside aperture, "
         << nTir << " with total reflection\n";
  if (!info.slabMode) {
    for (const auto* ax : {"x", "y"}) {
      const json& f = s["focus"][ax];
      G4cout << "  axis " << ax << ": N_focusing = " << f["n_focusing_elements"]
             << " | from lens end: A = " << f["method_A_ray_pairs"]["focus_from_lens_end_mm"]
             << " mm, C = " << f["method_C_paraxial_matrix"]["focus_from_lens_end_mm"]
             << " mm | EFL: A = " << f["method_A_ray_pairs"]["efl_mm"]
             << " mm, C = " << f["method_C_paraxial_matrix"]["efl_mm"] << " mm\n";
    }
  }
  for (const auto& w : L.warnings) G4cout << "  WARNING: " << w << '\n';
  G4cout << "  output: " << (dir / (pre + "_summary.json")).string() << G4endl;
}

}  // namespace xrcl
