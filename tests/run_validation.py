#!/usr/bin/env python3
"""Validation suite of the refraction model (stage 1, task 01).

Runs the Geant4 application for every test scenario, applies objective pass
criteria and writes validation_report.md / validation_results.json.
Exit code 0 only if every test passed.

usage: run_validation.py --app build/xraycrl --root . --out build/validation [--only T1,T5]
"""
import argparse
import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "analysis"))
from xrcl_common import RunError, fnum, read_csv, rel, run_app  # noqa: E402

DELTA25 = 4.32e-7
LENSES = ["objective1", "objective2", "objective2_N145", "objective2_N180", "condenser1", "condenser2"]
DESIGNS4 = ["objective1", "objective2", "condenser1", "condenser2"]


class Suite:
    def __init__(self, app, root, out):
        self.app, self.root, self.out = app, root, out
        self.results = []  # dicts: id, name, criterion, measured, status
        self.data = {}

    def run(self, base, name, overrides=None, macro=None):
        return run_app(self.app, self.root, base, name, self.out, overrides, macro)

    def record(self, tid, name, criterion, measured, ok, info=False):
        status = "INFO" if info else ("PASS" if ok else "FAIL")
        self.results.append(dict(id=tid, name=name, criterion=criterion, measured=measured, status=status))
        print(f"  [{status}] {tid} {name}: {measured}", flush=True)


def focus(summary, axis):
    return summary["focus"][axis]


def fa(summary, axis):  # method A
    return focus(summary, axis)["method_A_ray_pairs"]


def fc(summary, axis):  # method C
    return focus(summary, axis)["method_C_paraxial_matrix"]


def paraxial_base(lens):
    return f"config/runs/{lens}_full_paraxial.json"


# ---------------------------------------------------------------- tests
def t1_no_refraction(S):
    """delta = 0 and refraction switched off: directions must stay exactly (0,0,1)."""
    for label, ov in [("delta=0", {"refraction.delta_override": 0.0}), ("process off", {"refraction.enabled": False})]:
        name = "T1_" + label.replace(" ", "_").replace("=", "")
        s, d, _ = S.run(paraxial_base("objective1"), name, {**ov, "source.mode": "grid", "source.grid_n": 11})
        rays = read_csv(os.path.join(d, f"{name}_rays.csv"))
        dk = max(max(abs(r["kx"]), abs(r["ky"]), abs(r["kz"] - 1)) for r in rays)
        dx = max(max(abs(r["x_exit_um"] - r["x0_um"]), abs(r["y_exit_um"] - r["y0_um"])) for r in rays)
        valid = s["rays"]["valid"] == s["rays"]["generated"]
        nofocus = fc(s, "x")["z_focus_mm"] is None and fa(s, "x")["z_focus_mm"] is None
        S.record("T1", f"no refraction ({label}), objective 1 full set, 11x11 rays",
                 "|k - (0,0,1)| = 0, lateral shift < 1e-9 um, no finite focus",
                 f"max|dk| = {dk:.1e}, max lateral shift = {dx:.1e} um, valid {s['rays']['valid']}/{s['rays']['generated']}, focus: {'none' if nofocus else 'FOUND'}",
                 dk == 0 and dx < 1e-9 and valid and nofocus)


def t2_snell_plane(S):
    """Flat SU-8 slab: direction inside vs exact scalar Snell law; parallel faces restore the direction."""
    cases = [(1e-3, a) for a in (0, 10, 45, 80, 87, 88.5)] + [(DELTA25, a) for a in (10, 45, 80, 89, 89.97)]
    rows = []
    worst = 0.0
    all_ok = True
    for delta, alpha in cases:
        name = f"T2_slab_d{delta:g}_a{alpha:g}".replace(".", "p")
        ar = math.radians(alpha)
        off = -50.0 * math.sin(ar) if alpha > 89 else 0.0  # aim at the face centre for near-grazing incidence
        s, d, _ = S.run("config/runs/slab_snell.json", name,
                        {"refraction.delta_override": delta, "system.slab_tilt_deg": alpha,
                         "source.offset_um": [off, 0.0]})
        tr = read_csv(os.path.join(d, f"{name}_trace.csv")) if os.path.exists(os.path.join(d, f"{name}_trace.csv")) else []
        sin_t = math.sin(ar) / (1.0 - delta)
        expect_tir = sin_t > 1.0
        if alpha == 0:
            ok = len(tr) == 2 and all(abs(r["kx_after"]) == 0 and r["kz_after"] == 1 for r in tr)
            rows.append(dict(delta=delta, alpha=alpha, err=0.0, status="normal incidence"))
            all_ok &= ok
            continue
        first = tr[0]
        n = (first["nx"], first["ny"], first["nz"])
        k2 = (first["kx_after"], first["ky_after"], first["kz_after"])
        if expect_tir:
            k1 = (first["kx_before"], first["ky_before"], first["kz_before"])
            c = sum(a * b for a, b in zip(k1, n))
            mirror = [k1[i] - 2 * c * n[i] for i in range(3)]
            err = max(abs(mirror[i] - k2[i]) for i in range(3))
            ok = first["status"] == "total_reflection" and err < 1e-15 and len(tr) == 1
            rows.append(dict(delta=delta, alpha=alpha, err=err, status="total reflection"))
        else:
            cos2 = sum(a * b for a, b in zip(k2, n))
            sin2 = math.sqrt(sum((k2[(i + 1) % 3] * n[(i + 2) % 3] - k2[(i + 2) % 3] * n[(i + 1) % 3]) ** 2 for i in range(3)))
            theta2 = math.atan2(sin2, cos2)
            exact = math.asin(sin_t)
            err = abs(theta2 - exact)
            last = tr[-1]
            dout = max(abs(last["kx_after"] - tr[0]["kx_before"]), abs(last["kz_after"] - tr[0]["kz_before"]))
            ok = err < 1e-13 and len(tr) == 2 and dout < 1e-15 and first["status"] == "refracted"
            rows.append(dict(delta=delta, alpha=alpha, err=err, exit_dir_err=dout, status="refracted"))
            worst = max(worst, err)
        all_ok &= ok
    S.data["T2"] = rows
    n_tir = sum(r["status"] == "total reflection" for r in rows)
    S.record("T2", "vector Snell law at a plane SU-8 slab (delta = 1e-3 and 4.32e-7, 0..89.97 deg)",
             "|theta_refr - asin(sin(theta)/(1-delta))| < 1e-13 rad; exit direction = incident; TIR beyond the critical angle",
             f"{len(rows)} cases, worst angle error {worst:.1e} rad, {n_tir} total-reflection cases correct",
             all_ok)


def t3_single(S):
    """Single element of each design: focusing only in its own axis, f = R/(2 delta)."""
    rows = []
    ok_all = True
    for lens in DESIGNS4:
        name = f"T3_single_{lens}"
        s, d, _ = S.run(paraxial_base(lens), name, {"system.mode": "single"})
        R = s["layout"]["R_um"] * 1e-3
        f1 = R / (2 * DELTA25)
        eflA = fa(s, "x")["efl_mm"]
        eflC = fc(s, "x")["efl_mm"]
        y_slope = focus(s, "y")["max_slope_change_rad"]
        y_focus = fa(s, "y")["z_focus_mm"]
        r = dict(lens=lens, f_thin_mm=f1, efl_A_mm=eflA, efl_C_mm=eflC, rel_A_thin=rel(eflA, f1), rel_A_C=rel(eflA, eflC),
                 bfl_A_mm=fa(s, "x")["focus_from_lens_end_mm"], y_slope=y_slope)
        rows.append(r)
        ok = abs(r["rel_A_C"]) < 1e-6 and abs(r["rel_A_thin"]) < 1e-4 and y_slope == 0 and y_focus is None
        ok_all &= ok
    S.data["T3"] = rows
    S.record("T3", "single linear element (4 designs, 25 keV)",
             "EFL_trace vs matrix < 1e-6, vs R/(2 delta) < 1e-4; orthogonal axis: slope change exactly 0, no focus",
             "; ".join(f"{r['lens']}: f={r['efl_A_mm'] / 1000:.4f} m (A-C {r['rel_A_C']:.1e}, A-thin {r['rel_A_thin']:.1e})" for r in rows),
             ok_all)


def t4_pair(S):
    rows = []
    ok_all = True
    for lens in DESIGNS4:
        name = f"T4_pair_{lens}"
        s, d, _ = S.run(paraxial_base(lens), name, {"system.mode": "pair"})
        r = dict(lens=lens)
        ok = True
        for ax in "xy":
            r[f"z_{ax}_A"] = fa(s, ax)["z_focus_mm"]
            r[f"z_{ax}_C"] = fc(s, ax)["z_focus_mm"]
            r[f"efl_{ax}_A"] = fa(s, ax)["efl_mm"]
            r[f"efl_{ax}_C"] = fc(s, ax)["efl_mm"]
            ok &= r[f"z_{ax}_A"] is not None and abs(rel(r[f"efl_{ax}_A"], r[f"efl_{ax}_C"])) < 1e-6
            ok &= abs(focus(s, ax)["comparison"]["A_minus_C_focus_rel_to_efl"]) < 1e-6
        r["astig_A"] = s["focus"]["astigmatism_A_zx_minus_zy_mm"]
        r["astig_C"] = s["focus"]["astigmatism_C_zx_minus_zy_mm"]
        ok &= abs(r["astig_A"] - r["astig_C"]) < 1e-6 * abs(r["efl_x_C"])
        rows.append(r)
        ok_all &= ok
    S.data["T4"] = rows
    S.record("T4", "orthogonal pair X+Y (4 designs, 25 keV)",
             "focus in both axes; trace vs matrix (EFL and focus position) < 1e-6; astigmatism z_x - z_y equal in both models",
             "; ".join(f"{r['lens']}: z_x - z_y = {r['astig_A']:.4f} mm (matrix {r['astig_C']:.4f})" for r in rows), ok_all)


def t5_full(S):
    rows = []
    ok_all = True
    for lens in LENSES:
        name = f"T5_full_{lens}"
        s, d, _ = S.run(paraxial_base(lens), name)
        g, gd, _ = S.run(f"config/runs/{lens}_full_grid.json", f"T5_grid_{lens}", {"output.write_rays": True})
        r = dict(lens=lens, n_elements=s["layout"]["n_elements"], built_length_mm=s["layout"]["built_length_mm"],
                 gap_um=s["layout"]["gap_um"], warnings=s["layout"]["warnings"])
        ok = s["rays"]["valid"] == s["rays"]["generated"] == 8
        ok &= s["rays"]["min_refractions_valid"] == s["rays"]["max_refractions_valid"] == 2 * r["n_elements"]
        for ax in "xy":
            A, C = fa(s, ax), fc(s, ax)
            cmp = focus(s, ax)["comparison"]
            r[f"bfl_A_{ax}"] = A["focus_from_lens_end_mm"]
            r[f"bfl_C_{ax}"] = C["focus_from_lens_end_mm"]
            r[f"efl_A_{ax}"] = A["efl_mm"]
            r[f"efl_C_{ax}"] = C["efl_mm"]
            r[f"f_thin_{ax}"] = focus(s, ax)["analytic"]["f_thin_mm"]
            r[f"bfl_cont_{ax}"] = focus(s, ax)["analytic"]["continuous_thick_lens"]["bfl_from_lens_end_mm"]
            r[f"dz_AC_rel_{ax}"] = cmp["A_minus_C_focus_rel_to_efl"]
            r[f"defl_AC_rel_{ax}"] = cmp["A_vs_C_efl_rel"]
            B = focus(g, ax)["method_B_rms"]
            r[f"rms_focus_{ax}"] = B["rms_min_from_lens_end_mm"]
            r[f"rms_min_um_{ax}"] = B["rms_min_um"]
            ok &= abs(cmp["A_minus_C_focus_rel_to_efl"]) < 1e-5 and abs(cmp["A_vs_C_efl_rel"]) < 1e-5
        r["grid_valid"] = g["rays"]["valid"]
        r["grid_generated"] = g["rays"]["generated"]
        ok &= g["rays"]["valid"] == g["rays"]["generated"]
        r["elapsed_s"] = s["_elapsed_s"] + g["_elapsed_s"]
        rows.append(r)
        ok_all &= ok
    S.data["T5"] = rows
    S.record("T5", "full sets: 4 designs of Table 2 + objective 2 variants N=145, N=180 (25 keV)",
             "all paraxial rays valid with 4N refractions; trace vs matrix: focus position / EFL < 1e-5 (relative to EFL); all 21x21 grid rays (90 % aperture) transmitted",
             "; ".join(f"{r['lens']}: BFL_x {r['bfl_A_x']:.4g} mm (matrix {r['bfl_C_x']:.4g}), EFL_x {r['efl_A_x']:.5g} mm" for r in rows),
             ok_all)


def t6_N(S):
    rows = []
    ok_all = True
    for N in (1, 2, 5, 10, 20, 50, 100, 154, 250):
        name = f"T6_obj1_N{N}"
        s, d, _ = S.run(paraxial_base("objective1"), name, {"system.N_per_axis": N})
        f0 = focus(s, "x")["analytic"]["f_thin_mm"]
        cont = focus(s, "x")["analytic"]["continuous_thick_lens"]
        r = dict(N=N, L_mm=s["layout"]["built_length_mm"], f_thin=f0, efl_A=fa(s, "x")["efl_mm"], efl_C=fc(s, "x")["efl_mm"],
                 bfl_A=fa(s, "x")["focus_from_lens_end_mm"], bfl_C=fc(s, "x")["focus_from_lens_end_mm"],
                 efl_cont=cont["efl_mm"], bfl_cont=cont["bfl_from_lens_end_mm"])
        r["ratio_efl_thin"] = r["efl_A"] / f0
        r["L_over_f"] = r["L_mm"] / f0
        ok = abs(rel(r["efl_A"], r["efl_C"])) < 1e-5
        if r["L_over_f"] < 0.02:
            ok &= abs(r["ratio_efl_thin"] - 1) < 0.01
        ok &= abs(rel(r["efl_A"], r["efl_cont"])) < 0.02  # thick-lens continuum model
        rows.append(r)
        ok_all &= ok
    S.data["T6"] = rows
    S.record("T6", "dependence on N (objective 1 geometry, 25 keV, N = 1..250 per axis)",
             "trace = matrix (< 1e-5); EFL = R/(2N delta) within 1 % while L/f < 0.02; EFL within 2 % of the continuous thick-lens model for all N",
             "EFL/f_thin: " + ", ".join(f"N={r['N']}: {r['ratio_efl_thin']:.4f}" for r in rows), ok_all)


def t7_R(S):
    rows = []
    ok_all = True
    for R in (3, 6, 9, 20, 60, 89, 200):
        name = f"T7_single_R{R}"
        s, d, _ = S.run(paraxial_base("objective1"), name, {"system.mode": "single", "lens.R_um": R})
        efl = fa(s, "x")["efl_mm"]
        r = dict(R_um=R, efl_mm=efl, efl_over_R=efl / (R * 1e-3), expected=1 / (2 * DELTA25), rel_C=rel(efl, fc(s, "x")["efl_mm"]))
        r["dev"] = r["efl_over_R"] / r["expected"] - 1
        rows.append(r)
        ok_all &= abs(r["dev"]) < 1e-4 and abs(r["rel_C"]) < 1e-6
    S.data["T7"] = rows
    S.record("T7", "dependence on R (single element, p = 235 um, d = 10 um)",
             "EFL/R = 1/(2 delta) within 1e-4; trace = matrix < 1e-6",
             ", ".join(f"R={r['R_um']}: {r['dev']:+.1e}" for r in rows), ok_all)


def t8_delta(S):
    rows = []
    ok_all = True
    for dl in (1e-8, 1e-7, 4.32e-7, 1e-6, 1e-5, 1e-4):
        name = f"T8_single_delta{dl:g}"
        s, d, _ = S.run(paraxial_base("objective1"), name, {"system.mode": "single", "refraction.delta_override": dl})
        efl = fa(s, "x")["efl_mm"]
        r = dict(delta=dl, efl_mm=efl, efl_delta=efl * dl, rel_C=rel(efl, fc(s, "x")["efl_mm"]))
        r["dev"] = r["efl_delta"] / (0.009 / 2) - 1
        rows.append(r)
        ok_all &= abs(r["dev"]) < 1e-3 and abs(r["rel_C"]) < 1e-6
    # energy scan with the power-law delta(E) ~ E^-2  ->  f ~ E^2
    for E in (15, 20, 25, 30, 35):
        name = f"T8_single_E{E}"
        s, d, _ = S.run(paraxial_base("objective1"), name, {"system.mode": "single", "source.energy_keV": E})
        efl = fa(s, "x")["efl_mm"]
        rows.append(dict(energy_keV=E, efl_mm=efl, delta=s["material"]["delta"], f_over_E2=efl / E ** 2))
    eng = [r for r in rows if "energy_keV" in r]
    spread = max(r["f_over_E2"] for r in eng) / min(r["f_over_E2"] for r in eng) - 1
    ok_all &= spread < 1e-4
    S.data["T8"] = rows
    S.record("T8", "dependence on delta (1e-8..1e-4) and on energy (15..35 keV, delta ~ E^-2)",
             "EFL*delta = R/2 within 1e-3 (thickness effect grows with delta); trace = matrix < 1e-6; f/E^2 constant within 1e-4",
             ", ".join(f"d={r['delta']:g}: {r['dev']:+.1e}" for r in rows if "dev" in r) + f"; f/E^2 spread {spread:.1e}",
             ok_all)


def t9_geometry(S):
    """Exact analytic surfaces vs polygonal (tessellated) surfaces with n chords per parabola."""
    base = paraxial_base("objective1")
    common = {"system.mode": "single", "source.mode": "fan_x", "source.fan_n": 40, "source.aperture_fraction": 0.95}
    s0, d0, _ = S.run(base, "T9_analytic", common)
    ref = {int(r["id"]): r for r in read_csv(os.path.join(d0, "T9_analytic_rays.csv"))}
    rms0 = focus(s0, "x")["method_B_rms"]["z_rms_min_mm"]
    rows = []
    for n in (8, 16, 32, 64, 128, 256, 512, 1024):
        name = f"T9_tess_{n}"
        s, d, _ = S.run(base, name, {**common, "geometry.surface": "tessellated", "geometry.segments": n})
        rays = read_csv(os.path.join(d, f"{name}_rays.csv"))
        errs = []
        for r in rays:
            a = ref[int(r["id"])]
            if a["valid"] and r["valid"] and abs(a["kx"]) > 0:
                errs.append(abs(r["kx"] - a["kx"]) / abs(a["kx"]))
        rms = focus(s, "x")["method_B_rms"]["z_rms_min_mm"]
        rows.append(dict(segments=n, mean_rel_deflection_error=sum(errs) / len(errs), max_rel_deflection_error=max(errs),
                         rms_focus_rel_error=abs(rms - rms0) / abs(rms0), valid=s["rays"]["valid"]))
    # observed convergence order of the mean deflection error
    order = math.log(rows[2]["mean_rel_deflection_error"] / rows[-1]["mean_rel_deflection_error"]) / math.log(rows[-1]["segments"] / rows[2]["segments"])
    mono = all(rows[i + 1]["mean_rel_deflection_error"] < rows[i]["mean_rel_deflection_error"] for i in range(len(rows) - 1))
    S.data["T9"] = dict(rows=rows, order=order)
    ok = mono and order > 0.8 and all(r["valid"] == 40 for r in rows)
    S.record("T9", "surface representation: analytic solid vs G4ExtrudedSolid with n = 8..1024 chords",
             "deflection error decreases monotonically with n with order >= 0.8 (chord slope error ~ 1/n); analytic solid used as reference",
             f"mean rel. deflection error n=8: {rows[0]['mean_rel_deflection_error']:.2e} -> n=1024: {rows[-1]['mean_rel_deflection_error']:.2e}, order {order:.2f}",
             ok)


def t10_table(S):
    rows = []
    ok_all = True
    for lens in LENSES:
        cfg = json.load(open(os.path.join(S.root, "config", "lenses", f"{lens}.json")))
        for E, f_table_cm in cfg["reference_focal_cm"].items():
            name = f"T10_{lens}_{E}keV"
            s, d, _ = S.run(paraxial_base(lens), name, {"source.energy_keV": float(E)})
            ref = s["reference_table"]
            ax = focus(s, "x")
            r = dict(lens=lens, E_keV=int(E), N=s["layout"]["N_per_axis_design"], L_mm=s["layout"]["built_length_mm"],
                     delta=s["material"]["delta"], table_cm=f_table_cm, thin_cm=ref["thin_lens_cm"], table_vs_thin=ref["table_vs_thin_rel"],
                     efl_cm=ax["method_A_ray_pairs"]["efl_mm"] / 10, bfl_end_cm=ax["method_A_ray_pairs"]["focus_from_lens_end_mm"] / 10,
                     focus_centre_cm=ax["method_A_ray_pairs"]["focus_from_lens_centre_mm"] / 10,
                     thesis_173_cm=ax["analytic"]["thesis_formula_1_7_3"]["focus_from_lens_centre_mm"] / 10,
                     rel_AC=ax["comparison"]["A_vs_C_efl_rel"])
            rows.append(r)
            known = (lens == "objective1" and int(E) == 30)
            r["known_discrepancy"] = known
            if not known:
                ok_all &= abs(r["table_vs_thin"]) < 0.015
            ok_all &= abs(r["rel_AC"]) < 1e-5
    S.data["T10"] = rows
    worst = max(abs(r["table_vs_thin"]) for r in rows if not r["known_discrepancy"])
    S.record("T10", "comparison with the thesis Table 2 (20/25/30 keV; objective 2 with N=145/180 variants)",
             "table FL reproduced by thin-lens R/(2N delta(E)) within 1.5 % (objective 1 @30 keV reported separately); traced EFL = matrix < 1e-5. "
             "Traced focus positions are NOT required to equal the table: the table is a thin-lens value",
             f"{len(rows)} table entries; worst table-vs-thin deviation {worst:.2%}; objective 1 @30 keV: "
             + ", ".join(f"{r['table_vs_thin']:+.2%}" for r in rows if r["known_discrepancy"]),
             ok_all)


def t11_navigation(S):
    """Random rays through the full objective 1, and Geant4 overlap checks."""
    s, d, log = S.run(paraxial_base("objective1"), "T11_random_obj1",
                      {"source.mode": "random", "source.n_random": 3000, "source.aperture_fraction": 0.999})
    rays = s["rays"]
    ok = rays["valid"] == rays["generated"] and rays["min_refractions_valid"] == rays["max_refractions_valid"] == 616
    nexc = log.count("G4Exception")
    # overlap checks (cond1 has touching elements: gap clamped to 0)
    ov = []
    for lens, mode in (("objective1", "full"), ("condenser1", "full"), ("objective2", "full")):
        name = f"T11_overlaps_{lens}"
        s2, d2, log2 = S.run(paraxial_base(lens), name, {"geometry.check_overlaps": True, "system.mode": mode})
        ov.append((lens, log2.count("Overlap is detected"), log2.count("G4Exception"), log2.count("Checking overlaps")))
    ok &= nexc == 0 and all(o[1] == 0 and o[2] == 0 and o[3] > 0 for o in ov)
    S.record("T11", "navigation robustness and geometry overlap checking",
             "3000 random rays over 99.9 % of the aperture: all transmitted with exactly 616 refractions, no G4Exception; G4 overlap check: 0 overlaps",
             f"valid {rays['valid']}/{rays['generated']}, refractions {rays['min_refractions_valid']}..{rays['max_refractions_valid']}, "
             f"G4Exceptions {nexc}; overlap checks: " + ", ".join(f"{o[0]} {o[3]} volumes checked, {o[1]} overlaps" for o in ov),
             ok)


def t12_screens(S):
    s, d, _ = S.run(paraxial_base("objective1"), "T12_screens",
                    {"source.mode": "grid", "source.grid_n": 11, "screens_mm": [10.0, 38.0, 60.0, 150.0]})
    sc = s["screens_check"]
    ok = sc["hits"] > 0 and sc["max_deviation_um"] < 1e-6
    S.record("T12", "Geant4 transport to screens vs analytic straight-line extrapolation from the exit plane",
             "max deviation < 1e-6 um (validates extrapolation used for long-focus systems)",
             f"{sc['hits']} screen hits, max deviation {sc['max_deviation_um']:.1e} um", ok)


def t13_reproducibility(S):
    a, da, _ = S.run(paraxial_base("objective1"), "T13_repro_a", {"source.mode": "random", "source.n_random": 300})
    b, db, _ = S.run(paraxial_base("objective1"), "T13_repro_b", {"source.mode": "random", "source.n_random": 300})
    ra = open(os.path.join(da, "T13_repro_a_rays.csv")).read()
    rb = open(os.path.join(db, "T13_repro_b_rays.csv")).read()
    S.record("T13", "reproducibility (same seed, two runs)", "identical ray files", "identical" if ra == rb else "DIFFERENT", ra == rb)


def t14_macro(S):
    mac = os.path.join(S.out, "t14.mac")
    with open(mac, "w") as f:
        f.write("/xrcl/refraction/enable false\n/xrcl/run/beamOnAll\n")
    s, d, _ = S.run(paraxial_base("objective1"), "T14_macro_off", macro=mac)
    off = focus(s, "x")["max_slope_change_rad"] == 0 and not s["refraction_enabled"]
    with open(mac, "w") as f:
        f.write("/xrcl/source/energy 20 keV\n/xrcl/run/beamOnAll\n")
    s2, d2, _ = S.run(paraxial_base("objective1"), "T14_macro_energy", macro=mac)
    e_ok = abs(s2["energy_keV"] - 20) < 1e-12 and abs(s2["material"]["delta"] / (DELTA25 * (25 / 20) ** 2) - 1) < 1e-12
    S.record("T14", "macro control: /xrcl/refraction/enable false and /xrcl/source/energy 20 keV",
             "no deflection with refraction off; delta rescaled to (25/20)^2 * 4.32e-7",
             f"refraction off: {'no deflection' if off else 'DEFLECTION'}; 20 keV delta = {s2['material']['delta']:.4e}", off and e_ok)


def t15_rms_scan(S):
    lens = "objective1"
    d = os.path.join(S.out, f"T5_grid_{lens}")
    s = json.load(open(os.path.join(d, f"T5_grid_{lens}_summary.json")))
    scan = read_csv(os.path.join(d, f"T5_grid_{lens}_rms.csv"))
    ok = True
    txt = []
    for ax in "xy":
        zc = focus(s, ax)["method_B_rms"]["z_rms_min_mm"]
        col = f"rms_{ax}_um"
        imin = min(range(len(scan)), key=lambda i: scan[i][col])
        step = scan[1]["z_mm"] - scan[0]["z_mm"]
        ok &= abs(scan[imin]["z_mm"] - zc) <= step
        txt.append(f"{ax}: closed form {zc:.4f} mm, scan {scan[imin]['z_mm']:.4f} mm (step {step:.3g})")
    S.record("T15", "method B: closed-form RMS minimum vs sampled RMS(z) scan (objective 1, 21x21 grid)",
             "|z_closed - z_scan| <= scan step", "; ".join(txt), ok)


ALL = [t1_no_refraction, t2_snell_plane, t3_single, t4_pair, t5_full, t6_N, t7_R, t8_delta, t9_geometry, t10_table,
       t11_navigation, t12_screens, t13_reproducibility, t14_macro, t15_rms_scan]


def write_report(S, elapsed):
    with open(os.path.join(S.out, "validation_results.json"), "w") as f:
        json.dump(dict(results=S.results, data=S.data, elapsed_s=elapsed), f, indent=2, default=str)
    lines = ["# Validation report — XrayCRLGeant4, stage 1 (refraction model)", ""]
    npass = sum(r["status"] == "PASS" for r in S.results)
    nfail = sum(r["status"] == "FAIL" for r in S.results)
    lines += [f"Total: {len(S.results)} checks, **{npass} PASS**, **{nfail} FAIL**, run time {elapsed / 60:.1f} min.", ""]
    lines += ["| ID | Test | Criterion | Result | Status |", "|---|---|---|---|---|"]
    for r in S.results:
        lines.append(f"| {r['id']} | {r['name']} | {r['criterion']} | {r['measured']} | **{r['status']}** |")
    if "T5" in S.data:
        lines += ["", "## Full lens sets at 25 keV (delta = 4.32e-7)", "",
                  "| Lens | elements | L built, mm | gap, um | f_thin, mm | EFL trace, mm | EFL matrix, mm | focus from lens end, trace, mm | matrix, mm | continuous model, mm | RMS-min focus (21x21 grid), mm | RMS min, um |",
                  "|---|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in S.data["T5"]:
            lines.append(f"| {r['lens']} | {r['n_elements']} | {fnum(r['built_length_mm'])} | {fnum(r['gap_um'])} | {fnum(r['f_thin_x'])} | "
                         f"{fnum(r['efl_A_x'], 9)} | {fnum(r['efl_C_x'], 9)} | {fnum(r['bfl_A_x'], 9)} | {fnum(r['bfl_C_x'], 9)} | {fnum(r['bfl_cont_x'])} | "
                         f"{fnum(r['rms_focus_x'])} | {fnum(r['rms_min_um_x'])} |")
    if "T10" in S.data:
        lines += ["", "## Comparison with Table 2 of the master thesis", "",
                  "| Lens | E, keV | N | delta | table FL, cm | R/(2N delta), cm | table vs thin | traced EFL, cm | traced focus from lens end, cm | from lens centre, cm | thesis (1.7.3) from centre, cm |",
                  "|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in S.data["T10"]:
            lines.append(f"| {r['lens']} | {r['E_keV']} | {r['N']} | {r['delta']:.4e} | {r['table_cm']} | {r['thin_cm']:.4f} | {r['table_vs_thin']:+.2%}{' (!)' if r['known_discrepancy'] else ''} | "
                         f"{r['efl_cm']:.4f} | {r['bfl_end_cm']:.4f} | {r['focus_centre_cm']:.4f} | {r['thesis_173_cm']:.4f} |")
    with open(os.path.join(S.out, "validation_report.md"), "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--only", default="")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    S = Suite(os.path.abspath(args.app), root, out)
    only = {x.strip().upper() for x in args.only.split(",") if x.strip()}
    t0 = time.time()
    for t in ALL:
        tid = t.__name__.split("_")[0].upper()
        if only and tid not in only:
            continue
        print(f"--- {tid}: {t.__doc__.strip().splitlines()[0] if t.__doc__ else t.__name__}", flush=True)
        try:
            t(S)
        except (RunError, KeyError, ValueError, TypeError, ZeroDivisionError, IndexError, FileNotFoundError) as e:
            S.record(tid, t.__name__, "test executed without error", f"ERROR: {type(e).__name__}: {e}"[:600], False)
    write_report(S, time.time() - t0)
    nfail = sum(r["status"] == "FAIL" for r in S.results)
    print(f"\n{len(S.results) - nfail}/{len(S.results)} checks without failure; report: {os.path.join(out, 'validation_report.md')}")
    return 1 if nfail else 0


if __name__ == "__main__":
    sys.exit(main())
