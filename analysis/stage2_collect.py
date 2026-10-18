#!/usr/bin/env python3
"""Stage 2 data production: interaction physics (Livermore) in the SU-8 lens elements.

Runs the Geant4 application and writes the self-described data set to --data:
  00_attenuation/          mu(E) of SU-8 and of the Sb-free control, 15-35 keV (G4EmCalculator)
  01_sanity_1000ev/        1000-photon runs of every lens array (adequacy check + ray pictures)
  02_lens_arrays_25keV/    1e5-photon runs of every lens array at 25 keV, Beer-Lambert check, focal plane
  03_sb_k_edge/            energy scan across the Sb K edge (30.49 keV), SU-8 vs SU-8 without Sb
  04_focal_spot_divergence/ illustrative finite-divergence beam: non-zero focal spot
  figures/                 all figures (PNG + PDF)
Raw application output goes to --raw (not needed afterwards; regenerated on every run).

usage: stage2_collect.py --app build/xraycrl --root . --raw results/stage2_raw --data data [--quick]
"""
import argparse
import csv
import gzip
import json
import math
import os
import shutil
import subprocess
import sys
import time

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xrcl_common import read_csv, run_app  # noqa: E402

LENSES = ["objective1", "objective2", "objective2_N145", "objective2_N180", "condenser1", "condenser2"]
LABEL = {"objective1": "Объектив 1 (N=154)", "objective2": "Объектив 2 (N=215)", "objective2_N145": "Объектив 2, N=145",
         "objective2_N180": "Объектив 2, N=180", "condenser1": "Конденсор 1 (N=21)", "condenser2": "Конденсор 2 (N=31)"}
MATERIALS = {"SU8": "../materials/su8.json", "SU8_noSb": "../materials/su8_noSb.json"}
SCAN_E = [25.0, 28.0, 29.0, 30.0, 30.3, 30.4, 30.45, 30.48, 30.50, 30.52, 30.55, 30.6, 31.0, 32.0, 33.0, 35.0]
SCAN_LENSES = ["objective1", "condenser2"]
SEED = 20261018
FATES = ["unscattered", "scattered_transmitted", "absorbed", "lost", "outside_aperture"]
RU_FATE = {"unscattered": "без взаимодействий", "scattered_transmitted": "рассеяны, дошли до выхода",
           "absorbed": "поглощены", "lost": "рассеяны из пучка", "outside_aperture": "вне апертуры"}

plt.rcParams.update({"font.size": 9, "axes.grid": True, "grid.alpha": 0.3, "savefig.bbox": "tight", "legend.fontsize": 8})


class Ctx:
    def __init__(self, a):
        self.app, self.root = os.path.abspath(a.app), os.path.abspath(a.root)
        self.raw, self.data = os.path.abspath(a.raw), os.path.abspath(a.data)
        self.quick = a.quick
        self.reuse = a.reuse_raw
        self.fig = os.path.join(self.data, "figures_src")
        os.makedirs(self.fig, exist_ok=True)
        self.log = []

    def run(self, lens, name, ov, sub):
        base = f"config/runs/{lens}_full_paraxial.json"
        if self.reuse:
            d = os.path.join(self.raw, sub, name)
            f = os.path.join(d, f"{name}_summary.json")
            if os.path.exists(f):  # re-use an existing raw run (figures/tables only)
                return json.load(open(f)), d
        t0 = time.time()
        s, d, _ = run_app(self.app, self.root, base, name, os.path.join(self.raw, sub), ov)
        self.log.append((name, round(time.time() - t0, 1)))
        print(f"    {name}: {time.time() - t0:.1f} s", flush=True)
        return s, d

    def n(self, full, quick):
        return quick if self.quick else full


def em_overrides(n, energy=25.0, material="SU8", trace=0, rays=True, inter=True, frac=1.0, seed=SEED):
    return {"physics.em": "livermore", "source.mode": "random", "source.n_random": n, "source.aperture_fraction": frac,
            "source.seed": seed, "source.energy_keV": energy, "material": MATERIALS[material],
            "output.trace_events": trace, "output.write_rays": rays, "output.write_interactions": inter,
            "output.write_secondaries": True}


def savefig(fig, ctx, name):
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(ctx.fig, f"{name}.{ext}"), dpi=180)
    plt.close(fig)
    print("    figure", name)


def write_csv(path, header, rows, gz=False):
    op = gzip.open if gz else open
    with op(path, "wt", newline="") as f:
        w = csv.writer(f)
        w.writerow(header)
        w.writerows(rows)


def fate_fracs(s):
    f = s["interactions"]["fate"]
    return {k: (f[k]["fraction"], f[k]["stat_error"]) for k in FATES}


# ------------------------------------------------------------------ 00 attenuation
def part0(ctx):
    print("== 00 attenuation coefficients", flush=True)
    out = os.path.join(ctx.data, "00_attenuation")
    os.makedirs(out, exist_ok=True)
    E = [round(15 + 0.05 * i, 4) for i in range(401)]
    E += [round(30.40 + 0.002 * i, 4) for i in range(101)]
    E = sorted(set(E))
    tables = {}
    for mat, path in MATERIALS.items():
        f = os.path.join(out, f"mu_{mat}_15-35keV.csv")
        ov = {"material": path, "physics.em": "livermore", "xs_table": {"file": f, "energies_keV": E}}
        # the xs-table mode writes only the table (no tracking, no summary)
        cmd = [ctx.app, "--config", os.path.join(ctx.root, "config/runs/objective1_pair_paraxial.json")]
        for k, v in ov.items():
            cmd += ["--set", f"{k}={json.dumps(v)}"]
        subprocess.run(cmd, cwd=ctx.root, check=True, capture_output=True, text=True)
        tables[mat] = read_csv(f)
    t, t0 = tables["SU8"], tables["SU8_noSb"]
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(12, 4.2))
    e = [r["energy_keV"] for r in t]
    a1.semilogy(e, [r["mu_total_per_cm"] for r in t], "k-", lw=1.6, label="SU-8: полное")
    a1.semilogy(e, [r["mu_phot_per_cm"] for r in t], "C3-", label="SU-8: фотоэффект")
    a1.semilogy(e, [r["mu_compt_per_cm"] for r in t], "C0-", label="SU-8: Комптон")
    a1.semilogy(e, [r["mu_rayl_per_cm"] for r in t], "C2-", label="SU-8: Рэлей")
    a1.semilogy(e, [r["mu_phot_Sb_per_cm"] for r in t], "C3:", label="вклад Sb в фотоэффект")
    a1.semilogy(e, [r["mu_total_per_cm"] for r in t0], "k--", lw=1.2, label="SU-8 без Sb: полное")
    a1.semilogy(e, [r["mu_phot_per_cm"] for r in t0], "C3--", lw=1, label="SU-8 без Sb: фотоэффект")
    a1.axvline(30.491, color="gray", lw=0.8, ls="-.")
    a1.text(30.6, 1.5, "K-край Sb\n30,49 кэВ", fontsize=8)
    a1.set_xlabel("энергия фотонов, кэВ")
    a1.set_ylabel("линейный коэффициент μ, 1/см")
    a1.set_title("Коэффициенты ослабления SU-8 (Geant4 Livermore, ρ = 1,24 г/см³)")
    a1.legend(ncol=2)
    share = [r["mu_phot_Sb_per_cm"] / r["mu_phot_per_cm"] for r in t]
    a2.plot(e, [100 * x for x in share], "C3-", label="доля Sb в фотопоглощении")
    a2.plot(e, [100 * r["mu_phot_Sb_per_cm"] / r["mu_total_per_cm"] for r in t], "k-", label="доля Sb в полном ослаблении")
    a2.axvline(30.491, color="gray", lw=0.8, ls="-.")
    a2.set_xlabel("энергия фотонов, кэВ")
    a2.set_ylabel("%")
    a2.set_title("Роль сурьмы (1,33 мас.%) в ослаблении SU-8")
    a2.legend()
    savefig(fig, ctx, "fig00_attenuation_SU8_Sb_edge")
    return tables


# ------------------------------------------------------------------ helpers for ray pictures
def trajectories_with_interactions(ctx, s, d, name, title, fname, zf=None):
    rays = {int(r["id"]): r for r in read_csv(os.path.join(d, f"{name}_rays.csv"))}
    trace = read_csv(os.path.join(d, f"{name}_trace.csv"))
    lay = s["layout"]
    zf = zf if zf is not None else s["focus"]["x"]["method_C_paraxial_matrix"]["z_focus_mm"]
    z_end = lay["z_end_mm"]
    z_stop = max(zf, z_end) + 0.25 * abs(max(zf, z_end) - lay["z_start_mm"])
    paths, marks = {}, []
    for t in trace:
        ev = int(t["event"])
        p = paths.setdefault(ev, [(lay["z_source_mm"], rays[ev]["x0_um"])])
        p.append((t["z_mm"], t["x_um"]))
        if t["status"] in ("phot", "compt", "Rayl"):
            marks.append((t["z_mm"], t["x_um"], t["status"]))
    fig, ax = plt.subplots(figsize=(11, 4.6))
    col = {"unscattered": "C0", "scattered_transmitted": "C1", "absorbed": "C3", "lost": "C7", "outside_aperture": "C4"}
    seen = set()
    for ev, p in paths.items():
        r = rays[ev]
        fate = r["fate"]
        if r["reached_exit"]:
            p.append((r["z_exit_mm"], r["x_exit_um"]))
            sc = (z_stop - r["z_exit_mm"]) / r["kz"]
            p.append((z_stop, r["x_exit_um"] + sc * r["kx"] * 1000))
        z, x = zip(*p)
        lab = {"unscattered": "без взаимодействий", "scattered_transmitted": "рассеян, дошёл до выхода",
               "absorbed": "поглощён (фотоэффект)", "lost": "рассеян из пучка", "outside_aperture": "вне апертуры"}[fate]
        ax.plot(z, x, color=col[fate], lw=0.8, alpha=0.85, label=lab if fate not in seen else None)
        seen.add(fate)
    mk = {"phot": ("x", "C3", "фотоэффект"), "compt": ("o", "C1", "комптоновское рассеяние"), "Rayl": ("s", "C2", "рэлеевское рассеяние")}
    for proc, (m, c, lab) in mk.items():
        pts = [(z, x) for z, x, p in marks if p == proc]
        if pts:
            ax.plot(*zip(*pts), m, color=c, ms=6, mfc="none" if m != "x" else c, mew=1.4, label=lab, ls="none")
    ax.axvspan(lay["z_start_mm"], z_end, color="tab:blue", alpha=0.08, label="линза")
    ax.axvline(zf, color="crimson", ls="--", lw=1, label=f"расчётный фокус z = {zf:.2f} мм")
    half = lay["parabolic_aperture_um"] / 2
    ax.set_ylim(-1.6 * half, 1.6 * half)
    ax.set_xlim(lay["z_source_mm"] - 0.02 * z_stop, z_stop)
    ax.set_xlabel("z, мм")
    ax.set_ylabel("x, мкм")
    ax.set_title(title)
    ax.legend(loc="lower left", ncol=3, fontsize=7)
    savefig(fig, ctx, fname)


# ------------------------------------------------------------------ 01 sanity
def part1(ctx):
    print("== 01 sanity runs (1000 photons)", flush=True)
    out = os.path.join(ctx.data, "01_sanity_1000ev")
    os.makedirs(out, exist_ok=True)
    rows = []
    for lens in LENSES:
        name = f"sanity_{lens}_25keV"
        s, d = ctx.run(lens, name, em_overrides(1000, trace=40), "01")
        shutil.copy(os.path.join(d, f"{name}_summary.json"), out)
        for kind in ("rays", "trace", "interactions"):
            src = os.path.join(d, f"{name}_{kind}.csv")
            if os.path.exists(src):
                with open(src, "rb") as fi, gzip.open(os.path.join(out, f"{name}_{kind}.csv.gz"), "wb") as fo:
                    shutil.copyfileobj(fi, fo)
        ff = fate_fracs(s)
        foc = s["focus"]["x"]["method_C_paraxial_matrix"]
        rows.append([lens, 1000] + [f"{ff[k][0]:.4f}" for k in FATES] +
                    [f"{s['interactions']['attenuation_coefficients']['total']['mu_per_cm']:.4f}",
                     f"{foc['focus_from_lens_end_mm']:.4f}", f"{foc['z_focus_mm']:.4f}"])
        trajectories_with_interactions(ctx, s, d, name, f"{LABEL[lens]}, 25 кэВ, физика Livermore: 40 случайных фотонов (проверка адекватности)",
                                       f"fig01_sanity_rays_{lens}")
    write_csv(os.path.join(out, "sanity_table.csv"),
              ["lens", "n_photons"] + [f"frac_{k}" for k in FATES] + ["mu_total_per_cm", "focus_from_lens_end_mm", "z_focus_mm"], rows)


# ------------------------------------------------------------------ 02 lens arrays at 25 keV
def thesis_transmission(mu, N, R_um, d_um, a_um, frac):
    """Thesis formula (5.2.6) without roughness, per axis, crossed system T = Tx * Ty
    (straight rays, uniform illumination of |x| <= frac*a)."""
    k = N * mu * 1e-4 / R_um   # 1/um^2   (mu in 1/cm -> 1/um: *1e-4)
    X = frac * a_um
    tx = math.exp(-N * mu * 1e-4 * d_um) * math.sqrt(math.pi / (4 * k)) * math.erf(X * math.sqrt(k)) / X
    return tx * tx


def part2(ctx):
    print("== 02 lens arrays at 25 keV", flush=True)
    out = os.path.join(ctx.data, "02_lens_arrays_25keV")
    os.makedirs(out, exist_ok=True)
    N = ctx.n(100000, 5000)
    fate_rows, proc_rows, bl_rows, focal_rows, prof_all = [], [], [], [], {}
    for lens in LENSES:
        name = f"{lens}_25keV"
        s, d = ctx.run(lens, name, em_overrides(N, trace=0), "02")
        g, gd = ctx.run(lens, f"{lens}_25keV_geometry", {**em_overrides(N), "physics.em": "none", "output.write_interactions": False}, "02")
        rays = read_csv(os.path.join(d, f"{name}_rays.csv"))
        geo = {int(r["id"]): r for r in read_csv(os.path.join(gd, f"{lens}_25keV_geometry_rays.csv"))}
        I = s["interactions"]
        mu = I["attenuation_coefficients"]
        mut, muph = mu["total"]["mu_per_cm"], mu["phot"]["mu_per_cm"]
        # ---- compact per-photon file
        crow = []
        exp_unsc, exp_phot_first = 0.0, 0.0
        for r in rays:
            L = geo[int(r["id"])]["path_in_lens_mm"]
            p = math.exp(-mut * L / 10)
            exp_unsc += p
            exp_phot_first += muph / mut * (1 - p)
            crow.append([int(r["id"]), f"{r['x0_um']:.6f}", f"{r['y0_um']:.6f}", r["fate"], int(r["n_phot"]), int(r["n_compt"]),
                         int(r["n_rayl"]), r["first_process"], int(r["first_element"]), f"{r['first_z_mm']:.6f}", int(r["absorbed_element"]),
                         f"{r['absorbed_z_mm']:.6f}", f"{L:.6f}", f"{p:.6e}",
                         f"{r['x_exit_um']:.9g}" if r["reached_exit"] else "", f"{r['y_exit_um']:.9g}" if r["reached_exit"] else "",
                         f"{r['kx']:.17g}" if r["reached_exit"] else "", f"{r['ky']:.17g}" if r["reached_exit"] else "",
                         f"{r['kz']:.17g}" if r["reached_exit"] else "", f"{r['exit_energy_keV']:.6g}", f"{r['edep_lens_keV']:.6g}"])
        write_csv(os.path.join(out, f"{lens}_25keV_photons.csv.gz"),
                  ["id", "x0_um", "y0_um", "fate", "n_phot", "n_compt", "n_rayl", "first_process", "first_element", "first_z_mm",
                   "absorbed_element", "absorbed_z_mm", "path_in_lens_mm_geometric", "p_unscattered_beer_lambert",
                   "x_exit_um", "y_exit_um", "kx", "ky", "kz", "exit_energy_keV", "edep_lens_keV"], crow, gz=True)
        # interactions (gz), element profile, secondaries
        src = os.path.join(d, f"{name}_interactions.csv")
        with open(src, "rb") as fi, gzip.open(os.path.join(out, f"{lens}_25keV_interactions.csv.gz"), "wb") as fo:
            shutil.copyfileobj(fi, fo)
        shutil.copy(os.path.join(d, f"{name}_element_profile.csv"), os.path.join(out, f"{lens}_25keV_element_profile.csv"))
        sec = os.path.join(d, f"{name}_secondaries_exit.csv")
        if os.path.exists(sec):
            shutil.copy(sec, os.path.join(out, f"{lens}_25keV_secondaries_exit.csv"))
        shutil.copy(os.path.join(d, f"{name}_summary.json"), os.path.join(out, f"{lens}_25keV_summary.json"))
        prof_all[lens] = read_csv(os.path.join(d, f"{name}_element_profile.csv"))
        # ---- tables
        n = len(rays)
        ff = fate_fracs(s)
        fate_rows.append([lens, n] + sum(([f"{ff[k][0]:.5f}", f"{ff[k][1]:.5f}"] for k in FATES), []) +
                         [f"{I['energy_deposit_in_lens']['fraction_of_beam_energy']:.5f}", f"{I['energy_deposit_in_lens']['per_photon_keV']:.4f}"])
        P = I["processes"]
        proc_rows.append([lens, n] + sum(([P[p]["interactions"], f"{P[p]['fraction_of_photons']:.5f}", f"{P[p]['first_interaction_fraction']:.5f}"]
                                          for p in ("phot", "compt", "Rayl")), []) +
                         [f"{I['mean_interactions_per_photon']:.5f}", I["secondaries"]["electrons_created_by_primaries"],
                          I["secondaries"]["gammas_created_by_primaries"], I["secondaries"]["secondary_gammas_at_exit_plane"]])
        lay = s["layout"]
        tth = thesis_transmission(mut, lay["n_focusing_x"], lay["R_um"], lay["d_um"], lay["parabolic_aperture_um"] / 2, 1.0)
        mc = ff["unscattered"]
        bl_rows.append([lens, n, f"{mut:.6f}", f"{mc[0]:.5f}", f"{mc[1]:.5f}", f"{exp_unsc / n:.5f}", f"{(mc[0] - exp_unsc / n) / mc[1]:+.2f}",
                        f"{tth:.5f}", f"{(tth - mc[0]) / mc[0]:+.4f}",
                        f"{P['phot']['first_interaction_fraction']:.5f}", f"{exp_phot_first / n:.5f}"])
        # ---- focal plane (for foci downstream of the lens)
        foc = s["focus"]["x"]["method_C_paraxial_matrix"]
        zF = foc["z_focus_mm"]
        efl = foc["efl_mm"]
        lam_um = 12.39842 / 25.0 * 1e-4
        beta = mut * 1e2 * lam_um * 1e-6 / (4 * math.pi)
        delta = s["material"]["delta"]
        spot_thesis_um = 0.66 * math.sqrt(lam_um * efl * 1e3 * beta / delta)
        a_eff_um = lay["parabolic_aperture_um"] * math.sqrt(max(mc[0], 1e-12))  # side of an equally transmitting clear square
        r_un, r_sc = [], []
        if foc["focus_from_lens_end_mm"] > 0:
            for r in rays:
                if r["fate"] not in ("unscattered", "scattered_transmitted"):
                    continue
                t = (zF - r["z_exit_mm"]) / r["kz"]
                x = r["x_exit_um"] + t * r["kx"] * 1000
                y = r["y_exit_um"] + t * r["ky"] * 1000
                (r_un if r["fate"] == "unscattered" else r_sc).append((x, y))
            un = np.array(r_un) if r_un else np.zeros((0, 2))
            sc = np.array(r_sc) if r_sc else np.zeros((0, 2))
            rr_un = np.hypot(un[:, 0], un[:, 1]) if len(un) else np.array([])
            rr_sc = np.hypot(sc[:, 0], sc[:, 1]) if len(sc) else np.array([])
            in1 = int((rr_sc < 1.0).sum())
            focal_rows.append([lens, f"{zF:.4f}", f"{foc['focus_from_lens_end_mm']:.4f}", f"{efl:.4f}", len(rr_un),
                               f"{np.sqrt(np.mean(un[:, 0] ** 2)):.3e}" if len(un) else "", f"{np.max(rr_un) if len(rr_un) else 0:.3e}",
                               len(rr_sc), in1, f"{len(rr_sc) and np.median(rr_sc):.1f}", f"{spot_thesis_um:.4f}", f"{a_eff_um:.2f}"])
            np.savetxt(os.path.join(out, f"{lens}_25keV_focal_plane_xy.csv.gz"),
                       np.array([[x, y, 0] for x, y in r_un] + [[x, y, 1] for x, y in r_sc]),
                       delimiter=",", header="x_um,y_um,scattered(0=no,1=yes)", comments="", fmt="%.6e")
        else:
            focal_rows.append([lens, f"{zF:.4f}", f"{foc['focus_from_lens_end_mm']:.4f}", f"{efl:.4f}", "", "", "", "", "", "",
                               f"{spot_thesis_um:.4f}", f"{a_eff_um:.2f}"])
    write_csv(os.path.join(out, "table_fate_25keV.csv"),
              ["lens", "n_photons"] + sum(([f"frac_{k}", f"err_{k}"] for k in FATES), []) + ["edep_fraction_of_beam", "edep_per_photon_keV"], fate_rows)
    write_csv(os.path.join(out, "table_processes_25keV.csv"),
              ["lens", "n_photons"] + sum(([f"n_{p}", f"frac_photons_with_{p}", f"frac_first_{p}"] for p in ("phot", "compt", "rayl")), []) +
              ["mean_interactions_per_photon", "n_secondary_electrons", "n_secondary_gammas", "n_secondary_gammas_at_exit"], proc_rows)
    write_csv(os.path.join(out, "table_beer_lambert_check_25keV.csv"),
              ["lens", "n_photons", "mu_total_per_cm", "mc_unscattered", "mc_err", "beer_lambert_expected", "pull_sigma",
               "thesis_formula_5_2_6", "thesis_rel_dev", "mc_first_phot", "expected_first_phot"], bl_rows)
    write_csv(os.path.join(out, "table_focal_plane_25keV.csv"),
              ["lens", "z_focus_mm", "focus_from_lens_end_mm", "efl_mm", "n_unscattered_at_focus", "rms_x_unscattered_um",
               "max_r_unscattered_um", "n_scattered_at_focus_plane", "n_scattered_within_1um", "median_r_scattered_um",
               "thesis_spot_5_2_5_um", "effective_aperture_um"], focal_rows)
    # ---- figures
    fig, ax = plt.subplots(figsize=(9, 4))
    x = np.arange(len(LENSES))
    bottom = np.zeros(len(LENSES))
    ru = {r[0]: r for r in fate_rows}
    cols = {"unscattered": "C0", "scattered_transmitted": "C1", "absorbed": "C3", "lost": "C7"}
    names = {"unscattered": "прошли без взаимодействий", "scattered_transmitted": "рассеяны, дошли до выхода",
             "absorbed": "поглощены (фотоэффект)", "lost": "рассеяны из пучка"}
    for k in cols:
        v = np.array([float(ru[l][2 + 2 * FATES.index(k)]) for l in LENSES]) * 100
        ax.bar(x, v, bottom=bottom, color=cols[k], label=names[k])
        for i, vv in enumerate(v):
            if vv > 4:
                ax.text(i, bottom[i] + vv / 2, f"{vv:.1f}", ha="center", va="center", fontsize=7, color="w")
        bottom += v
    ax.set_xticks(x)
    ax.set_xticklabels([LABEL[l] for l in LENSES], rotation=15)
    ax.set_ylabel("доля фотонов, %")
    ax.set_title(f"Судьба фотонов 25 кэВ в массивах линз SU-8 (равномерное освещение апертуры, {N} фотонов на массив)")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.22), ncol=4)
    savefig(fig, ctx, "fig02_photon_fate_25keV")

    fig, axs = plt.subplots(2, 3, figsize=(13, 6.5))
    for ax, lens in zip(axs.flat, LENSES):
        p = prof_all[lens]
        z = [r["z_center_mm"] for r in p]
        for col, lab, c in (("n_phot", "фотоэффект", "C3"), ("n_compt", "Комптон", "C1"), ("n_rayl", "Рэлей", "C2")):
            ax.plot(z, [r[col] for r in p], ".", ms=2.5 if len(p) > 100 else 6, color=c, label=lab)
        ax.set_title(LABEL[lens])
        ax.set_xlabel("z элемента, мм")
        ax.set_ylabel("число взаимодействий в элементе")
        ax.legend()
    fig.suptitle("Где взаимодействуют фотоны 25 кэВ: распределение по элементам линзы")
    fig.tight_layout()
    savefig(fig, ctx, "fig03_interactions_along_lens_25keV")

    # transmission vs (x0^2 + y0^2)/a^2 : MC vs Beer-Lambert
    fig, axs = plt.subplots(2, 3, figsize=(13, 6.5))
    for ax, lens in zip(axs.flat, LENSES):
        with gzip.open(os.path.join(out, f"{lens}_25keV_photons.csv.gz"), "rt") as f:
            rows = list(csv.DictReader(f))
        a = json.load(open(os.path.join(out, f"{lens}_25keV_summary.json")))["layout"]["parabolic_aperture_um"] / 2
        rho = np.array([(float(r["x0_um"]) ** 2 + float(r["y0_um"]) ** 2) / a ** 2 for r in rows])
        un = np.array([r["fate"] == "unscattered" for r in rows])
        pbl = np.array([float(r["p_unscattered_beer_lambert"]) for r in rows])
        bins = np.linspace(0, 2, 21)
        idx = np.digitize(rho, bins) - 1
        c = 0.5 * (bins[1:] + bins[:-1])
        mc = [un[idx == i].mean() if (idx == i).any() else np.nan for i in range(20)]
        er = [np.sqrt(max(un[idx == i].mean() * (1 - un[idx == i].mean()), 1e-9) / max((idx == i).sum(), 1)) for i in range(20)]
        bl = [pbl[idx == i].mean() if (idx == i).any() else np.nan for i in range(20)]
        ax.errorbar(c, mc, yerr=er, fmt="o", ms=3, label="Монте-Карло Geant4")
        ax.plot(c, bl, "k-", label="Бугер–Ламберт–Бер, μ·L по трассе")
        ax.set_title(LABEL[lens])
        ax.set_xlabel("(x₀² + y₀²) / a²")
        ax.set_ylabel("доля без взаимодействий")
        ax.legend()
    fig.suptitle("Пропускание (без взаимодействий) в зависимости от удаления луча от оси, 25 кэВ")
    fig.tight_layout()
    savefig(fig, ctx, "fig04_transmission_profile_25keV")

    # focal plane image for objective 1 and condenser 2
    fig, axs = plt.subplots(1, 2, figsize=(12, 5))
    for ax, lens in zip(axs, ["objective1", "condenser2"]):
        f = os.path.join(out, f"{lens}_25keV_focal_plane_xy.csv.gz")
        if not os.path.exists(f):
            continue
        dat = np.loadtxt(f, delimiter=",", skiprows=1)
        un, sc = dat[dat[:, 2] == 0], dat[dat[:, 2] == 1]
        lim = 60 if lens.startswith("obj") else 600
        ax.plot(sc[:, 0], sc[:, 1], ".", ms=2, color="C1", alpha=0.6, label=f"рассеянные ({len(sc)})")
        ax.plot(un[:, 0], un[:, 1], "o", ms=5, color="C0", label=f"без взаимодействий ({len(un)}): все в точке r < 1 нм")
        ax.set_xlim(-lim, lim)
        ax.set_ylim(-lim, lim)
        ax.set_aspect("equal")
        ax.set_xlabel("x, мкм")
        ax.set_ylabel("y, мкм")
        ax.set_title(f"{LABEL[lens]}: фокальная плоскость (геометрический фокус)")
        ax.legend(loc="upper right")
    savefig(fig, ctx, "fig05_focal_plane_25keV")
    return fate_rows


# ------------------------------------------------------------------ 03 Sb K edge
def part3(ctx):
    print("== 03 Sb K-edge scan", flush=True)
    out = os.path.join(ctx.data, "03_sb_k_edge")
    os.makedirs(out, exist_ok=True)
    N = ctx.n(50000, 2000)
    energies = SCAN_E if not ctx.quick else [25.0, 30.4, 30.6, 35.0]
    rows = []
    for lens in SCAN_LENSES:
        for mat in MATERIALS:
            for E in energies:
                name = f"scan_{lens}_{mat}_{E:.2f}keV"
                s, d = ctx.run(lens, name, em_overrides(N, energy=E, material=mat, rays=False, inter=False), "03")
                ff = fate_fracs(s)
                I = s["interactions"]
                mu = I["attenuation_coefficients"]
                foc = s["focus"]["x"]["method_C_paraxial_matrix"]
                rows.append([lens, mat, E, N] + sum(([f"{ff[k][0]:.5f}", f"{ff[k][1]:.5f}"] for k in FATES[:4]), []) +
                            [f"{I['processes'][p]['fraction_of_photons']:.5f}" for p in ("phot", "compt", "Rayl")] +
                            [f"{I['energy_deposit_in_lens']['fraction_of_beam_energy']:.5f}", I["secondaries"]["gammas_created_by_primaries"],
                             I["secondaries"]["secondary_gammas_at_exit_plane"],
                             f"{mu['total']['mu_per_cm']:.6f}", f"{mu['phot']['mu_per_cm']:.6f}", f"{mu['photoelectric_by_element'].get('Sb', {}).get('phot_mu_per_cm', 0):.6f}",
                             f"{s['material']['delta']:.6e}", f"{foc['focus_from_lens_end_mm']:.5f}", f"{foc['efl_mm']:.5f}"])
    hdr = ["lens", "material", "energy_keV", "n_photons"] + sum(([f"frac_{k}", f"err_{k}"] for k in FATES[:4]), []) + \
          ["frac_photons_with_phot", "frac_photons_with_compt", "frac_photons_with_rayl", "edep_fraction_of_beam",
           "n_secondary_gammas_created", "n_secondary_gammas_at_exit", "mu_total_per_cm", "mu_phot_per_cm", "mu_phot_Sb_per_cm",
           "delta", "focus_from_lens_end_mm", "efl_mm"]
    write_csv(os.path.join(out, "table_sb_edge_scan.csv"), hdr, rows)

    # detailed runs just below / above the edge for all lens arrays (with Sb)
    Nd = ctx.n(100000, 3000)
    det = []
    for lens in LENSES:
        for E in (30.40, 30.60):
            name = f"{lens}_SU8_{E:.2f}keV"
            s, d = ctx.run(lens, name, em_overrides(Nd, energy=E, material="SU8", rays=False, inter=False), "03d")
            ff = fate_fracs(s)
            det.append([lens, E, Nd] + [f"{ff[k][0]:.5f}" for k in FATES[:4]] +
                       [f"{s['interactions']['energy_deposit_in_lens']['fraction_of_beam_energy']:.5f}",
                        s["interactions"]["secondaries"]["secondary_gammas_at_exit_plane"]])
            sec = os.path.join(d, f"{name}_secondaries_exit.csv")
            if os.path.exists(sec):
                shutil.copy(sec, os.path.join(out, f"{lens}_SU8_{E:.2f}keV_secondaries_exit.csv"))
            shutil.copy(os.path.join(d, f"{name}_summary.json"), out)
    write_csv(os.path.join(out, "table_all_lenses_below_above_edge.csv"),
              ["lens", "energy_keV", "n_photons"] + [f"frac_{k}" for k in FATES[:4]] + ["edep_fraction_of_beam", "n_secondary_gammas_at_exit"], det)

    # figures
    data = {}
    for r in rows:
        data.setdefault((r[0], r[1]), []).append(r)
    fig, axs = plt.subplots(1, 2, figsize=(13, 4.6))
    for ax, lens in zip(axs, SCAN_LENSES):
        for mat, ls in (("SU8", "-"), ("SU8_noSb", "--")):
            rr = data[(lens, mat)]
            E = [r[2] for r in rr]
            for k, c in (("unscattered", "C0"), ("absorbed", "C3"), ("lost", "C7")):
                i = 4 + 2 * FATES.index(k)
                ax.errorbar(E, [100 * float(r[i]) for r in rr], yerr=[100 * float(r[i + 1]) for r in rr], fmt="o" + ls, ms=3, color=c,
                            lw=1, label=f"{RU_FATE[k]}, {'SU-8' if mat == 'SU8' else 'SU-8 без Sb'}")
        ax.axvline(30.491, color="gray", ls="-.", lw=0.8)
        ax.set_xlabel("энергия, кэВ")
        ax.set_ylabel("доля фотонов, %")
        ax.set_title(f"{LABEL[lens]}: K-край сурьмы")
        ax.legend(fontsize=6.5, ncol=2)
    savefig(fig, ctx, "fig06_sb_edge_scan")

    # fluorescence spectrum at the exit plane above the edge
    fig, ax = plt.subplots(figsize=(7, 4))
    for lens, c in (("objective1", "C0"), ("condenser2", "C1"), ("condenser1", "C2")):
        for E, ls in ((30.40, ":"), (30.60, "-")):
            f = os.path.join(out, f"{lens}_SU8_{E:.2f}keV_secondaries_exit.csv")
            if not os.path.exists(f):
                continue
            en = [r["energy_keV"] for r in read_csv(f)]
            if en:
                ax.hist(en, bins=np.arange(0, 31, 0.25), histtype="step", ls=ls, color=c, label=f"{LABEL[lens]}, {E} кэВ ({len(en)} фот.)")
    for line, lab in ((26.36, "Sb Kα"), (29.73, "Sb Kβ"), (3.6, "Sb L")):
        ax.axvline(line, color="k", lw=0.6, ls="--")
        ax.text(line, ax.get_ylim()[1] * 0.9 if ax.get_ylim()[1] > 1 else 1, lab, fontsize=7, rotation=90)
    ax.set_xlabel("энергия вторичного фотона на выходе линзы, кэВ")
    ax.set_ylabel("число фотонов")
    ax.set_title(f"Вторичное излучение на выходе ({Nd} первичных фотонов)")
    ax.legend(fontsize=7)
    savefig(fig, ctx, "fig07_secondary_spectrum_exit")
    return rows, det


# ------------------------------------------------------------------ 04 divergence / finite spot
def part4(ctx):
    print("== 04 finite-divergence illustration", flush=True)
    out = os.path.join(ctx.data, "04_focal_spot_divergence")
    os.makedirs(out, exist_ok=True)
    rows = []
    fig, axs = plt.subplots(1, 3, figsize=(13, 4.2))
    for ax, sig in zip(axs, (0.0, 1.0, 3.0)):
        name = f"objective1_25keV_div{sig:g}urad"
        ov = em_overrides(ctx.n(20000, 2000), trace=0, inter=False)
        ov["source.divergence_urad"] = [sig, sig]
        s, d = ctx.run("objective1", name, ov, "04")
        foc = s["focus"]["x"]["method_C_paraxial_matrix"]
        rays = [r for r in read_csv(os.path.join(d, f"{name}_rays.csv")) if r["fate"] == "unscattered"]
        zF = foc["z_focus_mm"]
        xy = np.array([[r["x_exit_um"] + (zF - r["z_exit_mm"]) / r["kz"] * r["kx"] * 1000,
                        r["y_exit_um"] + (zF - r["z_exit_mm"]) / r["kz"] * r["ky"] * 1000] for r in rays])
        rx, ry = xy[:, 0].std(), xy[:, 1].std()
        pred = foc["efl_mm"] * 1e3 * sig * 1e-6
        rows.append([sig, len(xy), f"{rx:.5f}", f"{ry:.5f}", f"{2.3548 * rx:.5f}", f"{pred:.5f}"])
        h = ax.hist2d(xy[:, 0], xy[:, 1], bins=80, range=[[-1.2, 1.2], [-1.2, 1.2]], cmap="magma")
        ax.set_aspect("equal")
        ax.set_title(f"σθ = {sig:g} мкрад: σ = {rx * 1000:.0f} нм (ожид. EFL·σθ = {pred * 1000:.0f} нм)")
        ax.set_xlabel("x, мкм")
        ax.set_ylabel("y, мкм")
        np.savetxt(os.path.join(out, f"{name}_focal_plane_xy.csv.gz"), xy, delimiter=",", header="x_um,y_um", comments="", fmt="%.6e")
    fig.suptitle("Объектив 1, 25 кэВ: фокальное пятно при угловой расходимости пучка (иллюстрация)")
    savefig(fig, ctx, "fig08_focal_spot_vs_divergence")
    write_csv(os.path.join(out, "table_focal_spot_vs_divergence.csv"),
              ["divergence_sigma_urad", "n_unscattered", "sigma_x_um", "sigma_y_um", "fwhm_x_um", "expected_sigma_efl_times_theta_um"], rows)


# ------------------------------------------------------------------ 05 waist plane: where do the photons end up
WAIST_E = [25.0, 30.40, 30.60, 35.0]
SPOT_R_UM = [0.1, 1.0, 10.0]   # radii used to count "in the focal spot"


def classify_at_plane(rays, secs, zw, r_spot):
    """Every incident photon -> one category at the waist plane z = zw."""
    cat = {k: 0 for k in ("spot_unscattered", "halo_unscattered", "spot_rayleigh", "halo_rayleigh", "spot_compton", "halo_compton",
                          "absorbed", "lost", "other")}
    pts = {"unscattered": [], "rayleigh": [], "compton": [], "fluorescence": []}
    for r in rays:
        f = r["fate"]
        if f == "absorbed":
            cat["absorbed"] += 1
            continue
        if f == "lost" or not r["reached_exit"]:
            cat["lost"] += 1
            continue
        t = (zw - r["z_exit_mm"]) / r["kz"]
        x = r["x_exit_um"] + t * r["kx"] * 1000
        y = r["y_exit_um"] + t * r["ky"] * 1000
        rr = math.hypot(x, y)
        if f == "unscattered":
            kind = "unscattered"
        elif r["n_compt"] > 0:
            kind = "compton"
        elif r["n_rayl"] > 0:
            kind = "rayleigh"
        else:
            cat["other"] += 1
            continue
        pts[kind].append((x, y, r.get("exit_energy_keV", 0)))
        cat[("spot_" if rr <= r_spot else "halo_") + kind] += 1
    nfl_spot = nfl = 0
    for sct in secs:
        t = (zw - sct["z_mm"]) / sct["kz"] if sct["kz"] > 0 else None
        if t is None:
            continue
        x = sct["x_um"] + t * sct["kx"] * 1000
        y = sct["y_um"] + t * sct["ky"] * 1000
        pts["fluorescence"].append((x, y, sct["energy_keV"]))
        nfl += 1
        nfl_spot += math.hypot(x, y) <= r_spot
    return cat, pts, nfl, nfl_spot


def part5(ctx):
    print("== 05 waist plane: contributions of the effects", flush=True)
    out = os.path.join(ctx.data, "05_waist_plane_components")
    os.makedirs(out, exist_ok=True)
    N = ctx.n(100000, 3000)
    energies = WAIST_E if not ctx.quick else [25.0, 30.6]
    rows, spot_rows, store = [], [], {}
    for E in energies:
        for lens in LENSES:
            name = f"waist_{lens}_{E:.2f}keV"
            s, d = ctx.run(lens, name, em_overrides(N, energy=E, inter=False), "05")
            rays = read_csv(os.path.join(d, f"{name}_rays.csv"))
            secf = os.path.join(d, f"{name}_secondaries_exit.csv")
            secs = read_csv(secf) if os.path.exists(secf) else []
            foc = s["focus"]["x"]["method_C_paraxial_matrix"]
            focy = s["focus"]["y"]["method_C_paraxial_matrix"]
            z_end = s["layout"]["z_end_mm"]
            # waist = plane of minimum RMS width of the unscattered photons (closed form over x and y together)
            un = [r for r in rays if r["fate"] == "unscattered"]
            X = np.array([[r["x_exit_um"], r["y_exit_um"], r["kx"] / r["kz"] * 1000, r["ky"] / r["kz"] * 1000] for r in un])
            if len(X) > 3:
                xc, yc = X[:, 0] - X[:, 0].mean(), X[:, 1] - X[:, 1].mean()
                tx, ty = X[:, 2] - X[:, 2].mean(), X[:, 3] - X[:, 3].mean()
                sw = -(np.dot(xc, tx) + np.dot(yc, ty)) / (np.dot(tx, tx) + np.dot(ty, ty))   # mm
                zw = un[0]["z_exit_mm"] + sw
                rms_w = math.sqrt((np.sum((xc + sw * tx) ** 2) + np.sum((yc + sw * ty) ** 2)) / (2 * len(X)))
            else:
                zw, rms_w = foc["z_focus_mm"], float("nan")
            inside = zw < z_end
            lam_um = 12.39842 / E * 1e-4
            mut = s["interactions"]["attenuation_coefficients"]["total"]["mu_per_cm"]
            beta = mut * 1e2 * lam_um * 1e-6 / (4 * math.pi)
            spot_diff = 0.66 * math.sqrt(lam_um * foc["efl_mm"] * 1e3 * beta / s["material"]["delta"])
            spot_rows.append([lens, E, f"{zw:.5f}", f"{zw - z_end:.5f}", f"{foc['z_focus_mm']:.5f}", f"{focy['z_focus_mm']:.5f}",
                              f"{foc['z_focus_mm'] - focy['z_focus_mm']:.5f}", f"{rms_w:.3e}", f"{spot_diff:.4f}", f"{foc['efl_mm']:.4f}",
                              "inside_lens" if inside else "after_lens"])
            n = len(rays)
            for R in SPOT_R_UM:
                if inside:
                    continue
                cat, pts, nfl, nfl_spot = classify_at_plane(rays, secs, zw, R)
                rows.append([lens, E, R, n] + [cat[k] for k in cat] + [nfl, nfl_spot] +
                            [f"{100 * (cat['spot_unscattered']) / n:.3f}",
                             f"{100 * (cat['halo_unscattered'] + cat['spot_rayleigh'] + cat['halo_rayleigh'] + cat['spot_compton'] + cat['halo_compton']) / n:.3f}",
                             f"{100 * cat['absorbed'] / n:.3f}", f"{100 * cat['lost'] / n:.3f}"])
                if R == 1.0:
                    store[(lens, E)] = (cat, pts, n, zw)
            shutil.copy(os.path.join(d, f"{name}_summary.json"), out)
            if lens in ("objective1", "condenser2"):
                keep = ["id", "x0_um", "y0_um", "fate", "n_phot", "n_compt", "n_rayl", "first_process", "first_element", "first_z_mm",
                        "absorbed_element", "absorbed_z_mm", "x_exit_um", "y_exit_um", "z_exit_mm", "kx", "ky", "kz", "exit_energy_keV",
                        "edep_lens_keV", "n_sec_gamma", "n_sec_electron"]
                write_csv(os.path.join(out, f"{lens}_{E:.2f}keV_photons_ntuple.csv.gz"), keep,
                          [[r[k] for k in keep] for r in rays], gz=True)
                if secs:
                    shutil.copy(secf, os.path.join(out, f"{lens}_{E:.2f}keV_secondaries_exit.csv"))
    hdr = ["lens", "energy_keV", "spot_radius_um", "n_incident", "n_spot_unscattered", "n_halo_unscattered", "n_spot_rayleigh",
           "n_halo_rayleigh", "n_spot_compton", "n_halo_compton", "n_absorbed", "n_lost", "n_other", "n_fluorescence_at_plane",
           "n_fluorescence_in_spot", "pct_in_focal_spot", "pct_noise_at_plane", "pct_absorbed", "pct_scattered_out_of_beam"]
    write_csv(os.path.join(out, "table_waist_plane_components.csv"), hdr, rows)
    write_csv(os.path.join(out, "table_waist_and_spot.csv"),
              ["lens", "energy_keV", "z_waist_mm", "waist_from_lens_end_mm", "z_paraxial_focus_x_mm", "z_paraxial_focus_y_mm",
               "astigmatism_zx_minus_zy_mm", "rms_radius_at_waist_unscattered_um", "spot_estimate_thesis_5_2_5_um", "efl_mm", "waist_position"],
              spot_rows)

    # ---- figure: percentages per lens and energy (top) + composition of the noise (bottom)
    lens_ok = list(LENSES)
    fig, axs = plt.subplots(2, len(energies), figsize=(4.4 * len(energies), 9.5), sharey="row",
                            gridspec_kw={"height_ratios": [2.2, 1]})
    axs = np.atleast_2d(axs)
    comp = [("spot_unscattered", "в фокусном пятне r ≤ 1 мкм (без взаимодействий)", "#14325c"),
            ("noise", "шум в плоскости перетяжки (Рэлей + Комптон)", "#e67e22"),
            ("absorbed", "поглощены (фотоэффект)", "#c0392b"),
            ("lost", "рассеяны из пучка (не дошли до плоскости)", "#999999")]
    for j, E in enumerate(energies):
        ax = axs[0, j]
        x = np.arange(len(lens_ok))
        b = np.zeros(len(lens_ok))
        for key, lab, c in comp:
            v = []
            for l in lens_ok:
                if (l, E) not in store:
                    v.append(0)
                    continue
                cat, _, n, _ = store[(l, E)]
                if key == "noise":
                    v.append(100 * sum(cat[k] for k in ("halo_unscattered", "spot_rayleigh", "halo_rayleigh", "spot_compton", "halo_compton")) / n)
                else:
                    v.append(100 * cat[key] / n)
            v = np.array(v)
            ax.bar(x, v, bottom=b, color=c, label=lab if j == len(energies) - 1 else None, width=0.75)
            for i, vv in enumerate(v):
                if vv > 7:
                    ax.text(i, b[i] + vv / 2, f"{vv:.0f}", ha="center", va="center", color="w", fontsize=9, fontweight="bold")
            b += v
        for i, l in enumerate(lens_ok):
            if (l, E) not in store:
                ax.text(i, 50, "перетяжка\nвнутри линзы", rotation=90, ha="center", va="center", fontsize=8, color="#555555")
        ax.set_xticks(x)
        ax.set_xticklabels([])
        ax.set_ylim(0, 100)
        ax.set_title(f"{E:g} кэВ" + (" (ниже K-края Sb)" if E == 30.4 else " (выше K-края Sb)" if E == 30.6 else ""))
        # noise composition, % of incident photons
        ax2 = axs[1, j]
        b2 = np.zeros(len(lens_ok))
        for key, lab, c in (("rayleigh", "шум: только рэлеевское рассеяние", "#2e8b57"), ("compton", "шум: комптоновское рассеяние", "#e67e22"),
                            ("fluorescence", "вторичные фотоны (флуоресценция Sb)", "#8e44ad")):
            v = []
            for l in lens_ok:
                if (l, E) not in store:
                    v.append(0)
                    continue
                cat, pts, n, _ = store[(l, E)]
                v.append(100 * len(pts[key]) / n)
            v = np.array(v)
            ax2.bar(x, v, bottom=b2, color=c, width=0.75, label=lab if j == len(energies) - 1 else None)
            b2 += v
        ax2.set_xticks(x)
        ax2.set_xticklabels([LABEL[l].replace(" (", "\n(").replace(", ", ",\n") for l in lens_ok], rotation=90, fontsize=8)
    axs[0, 0].set_ylabel("% фотонов, упавших на массив")
    axs[1, 0].set_ylabel("шум в плоскости перетяжки, %\nот упавших фотонов")
    axs[0, -1].legend(loc="upper left", bbox_to_anchor=(1.02, 1), fontsize=9)
    axs[1, -1].legend(loc="upper left", bbox_to_anchor=(1.02, 1), fontsize=9)
    fig.suptitle(f"Плоскость перетяжки: судьба фотонов, равномерно упавших на апертуру массива ({N} фотонов на точку). "
                 "Ни один рассеянный фотон не попал в пятно r ≤ 1 мкм", fontsize=11)
    fig.tight_layout()
    savefig(fig, ctx, "fig09_waist_plane_components")

    # ---- figure: radial distribution of the components at the waist plane, objective 1
    for lens in ("objective1", "condenser2"):
        if (lens, energies[0]) not in store:
            continue
        fig, axs = plt.subplots(1, len(energies), figsize=(4.2 * len(energies), 4), sharey=True)
        axs = np.atleast_1d(axs)
        lim = 3000 if lens.startswith("obj") else 30000
        bins = np.logspace(-6, math.log10(lim * 3), 50)
        for ax, E in zip(axs, energies):
            cat, pts, n, zw = store[(lens, E)]
            for kind, c, lab in (("unscattered", "#14325c", "без взаимодействий"), ("rayleigh", "#2e8b57", "Рэлей"),
                                 ("compton", "#e67e22", "Комптон"), ("fluorescence", "#8e44ad", "вторичные (флуоресценция)")):
                rr = [math.hypot(x, y) for x, y, _ in pts[kind]]
                if rr:
                    ax.hist(np.clip(rr, 1e-6, None), bins=bins, histtype="step", lw=1.6, color=c, label=f"{lab}: {len(rr)}")
            ax.set_xscale("log")
            ax.set_yscale("log")
            ax.axvline(1.0, color="k", ls=":", lw=0.8)
            ax.set_xlabel("расстояние от оси в плоскости перетяжки, мкм")
            ax.set_title(f"{E:g} кэВ")
            ax.legend(fontsize=7)
        axs[0].set_ylabel("число фотонов")
        fig.suptitle(f"{LABEL[lens]}: радиальное распределение фотонов в плоскости перетяжки ({N} падающих)")
        savefig(fig, ctx, f"fig10_waist_radial_{lens}")
    return rows, spot_rows


# ------------------------------------------------------------------ 06 beam picture: everything that flies
def part6(ctx):
    print("== 06 beam visualisation", flush=True)
    out = os.path.join(ctx.data, "06_beam_visualisation")
    os.makedirs(out, exist_ok=True)
    for lens, ntr in (("objective1", 300), ("condenser2", 300)):
        name = f"beam_{lens}_25keV"
        s, d = ctx.run(lens, name, em_overrides(ntr, trace=ntr, inter=True), "06")
        rays = {int(r["id"]): r for r in read_csv(os.path.join(d, f"{name}_rays.csv"))}
        tr = read_csv(os.path.join(d, f"{name}_trace.csv"))
        for kind in ("rays", "trace", "interactions"):
            with open(os.path.join(d, f"{name}_{kind}.csv"), "rb") as fi, gzip.open(os.path.join(out, f"{name}_{kind}.csv.gz"), "wb") as fo:
                shutil.copyfileobj(fi, fo)
        shutil.copy(os.path.join(d, f"{name}_summary.json"), out)
        lay = s["layout"]
        zf = s["focus"]["x"]["method_C_paraxial_matrix"]["z_focus_mm"]
        z_end = lay["z_end_mm"]
        half_world = 3 * max(lay["height_um"] / 2, lay["parabolic_aperture_um"] / 2) / 1000 + 1.0
        paths, last_dir, marks = {}, {}, []
        for t in tr:
            ev = int(t["event"])
            p = paths.setdefault(ev, [(lay["z_source_mm"], rays[ev]["x0_um"] / 1000)])
            p.append((t["z_mm"], t["x_um"] / 1000))
            last_dir[ev] = (t["kx_after"], t["kz_after"])
            if t["status"] in ("phot", "compt", "Rayl"):
                marks.append((t["z_mm"], t["x_um"] / 1000, t["status"]))
        zst = max(zf, z_end) + 0.35 * max(zf, z_end)
        fig, (a1, a2) = plt.subplots(2, 1, figsize=(13, 9.5), gridspec_kw={"height_ratios": [1, 1.25]})
        col = {"unscattered": "#14325c", "scattered_transmitted": "#e67e22", "absorbed": "#c0392b", "lost": "#888888", "outside_aperture": "m"}
        for ev, p in paths.items():
            r = rays[ev]
            p = list(p)
            if r["reached_exit"]:
                p.append((r["z_exit_mm"], r["x_exit_um"] / 1000))
                t = (zst - r["z_exit_mm"]) / r["kz"] if r["kz"] > 0 else 0
                p.append((zst, r["x_exit_um"] / 1000 + t * r["kx"]))
            elif r["fate"] == "lost" and ev in last_dir:
                kx, kz = last_dir[ev]
                z0, x0 = p[-1]
                # continue the scattered photon in a straight line to the edge of the world
                L = 1e9
                if abs(kx) > 1e-12:
                    L = min(L, (math.copysign(half_world, kx) - x0) / kx)
                if abs(kz) > 1e-12:
                    L = min(L, ((zst if kz > 0 else lay["z_source_mm"]) - z0) / kz)
                p.append((z0 + L * kz, x0 + L * kx))
            z, x = zip(*p)
            lost = r["fate"] == "lost"
            a1.plot(z, x, color=col[r["fate"]], lw=0.6 if lost else 0.8, alpha=0.25 if lost else 0.8)
            a2.plot(z, x, color=col[r["fate"]], lw=0.9, alpha=0.75)
        for proc, m, c in (("phot", "x", "#c0392b"), ("compt", "o", "#e67e22"), ("Rayl", "s", "#2e8b57")):
            pts = [(z, x) for z, x, pp in marks if pp == proc]
            if pts:
                for ax in (a1, a2):
                    ax.plot(*zip(*pts), m, color=c, ms=4, mfc="none" if m != "x" else c, ls="none")
        for ax in (a1, a2):
            ax.axvspan(lay["z_start_mm"], z_end, color="#3d7ab8", alpha=0.10)
            ax.axvline(zf, color="crimson", ls="--", lw=1)
            ax.set_xlim(lay["z_source_mm"], zst)
            ax.set_xlabel("z, мм")
        a = lay["parabolic_aperture_um"] / 2000
        a1.set_ylim(-1.5 * a, 1.5 * a)
        a1.set_ylabel("x, мм")
        a1.set_title(f"{LABEL[lens]}, 25 кэВ, {ntr} фотонов: масштаб апертуры — фокусировка прошедших фотонов")
        a2.set_ylim(-half_world, half_world)
        a2.set_ylabel("x, мм")
        a2.set_title("тот же пучок в масштабе мм: рассеянные фотоны (серые) разлетаются из линзы")
        from matplotlib.lines import Line2D
        h = [Line2D([], [], color=col[k], lw=2, label=l) for k, l in (("unscattered", "без взаимодействий"),
             ("scattered_transmitted", "рассеян, дошёл до выхода"), ("absorbed", "поглощён"), ("lost", "рассеян из пучка"))]
        h += [Line2D([], [], marker=m, color=c, ls="none", mfc="none" if m != "x" else c, label=l)
              for m, c, l in (("x", "#c0392b", "фотоэффект"), ("o", "#e67e22", "Комптон"), ("s", "#2e8b57", "Рэлей"))]
        h.append(Line2D([], [], color="crimson", ls="--", label=f"расчётный фокус z = {zf:.1f} мм"))
        a2.legend(handles=h, loc="lower left", ncol=4, fontsize=8)
        fig.tight_layout()
        savefig(fig, ctx, f"fig11_beam_through_array_{lens}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app", required=True)
    ap.add_argument("--root", required=True)
    ap.add_argument("--raw", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--quick", action="store_true", help="few photons, for testing the pipeline")
    ap.add_argument("--parts", default="0,1,2,3,4,5,6")
    ap.add_argument("--reuse-raw", action="store_true", help="re-use existing raw runs in --raw (rebuild tables/figures only)")
    a = ap.parse_args()
    ctx = Ctx(a)
    t0 = time.time()
    parts = {int(p) for p in a.parts.split(",")}
    if 0 in parts:
        part0(ctx)
    if 1 in parts:
        part1(ctx)
    if 2 in parts:
        part2(ctx)
    if 3 in parts:
        part3(ctx)
    if 4 in parts:
        part4(ctx)
    if 5 in parts:
        part5(ctx)
    if 6 in parts:
        part6(ctx)
    with open(os.path.join(ctx.data, "run_log.csv"), "a") as f:
        for n, t in ctx.log:
            f.write(f"{n},{t}\n")
    print(f"done in {(time.time() - t0) / 60:.1f} min")


if __name__ == "__main__":
    main()
