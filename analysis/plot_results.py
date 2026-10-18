#!/usr/bin/env python3
"""Scientific figures from the validation suite and the ray-trace runs (PNG + PDF).

usage: plot_results.py --validation build/validation --traces results --out results/figures
"""
import argparse
import json
import math
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xrcl_common import read_csv  # noqa: E402

plt.rcParams.update({"font.size": 9, "axes.grid": True, "grid.alpha": 0.3, "figure.dpi": 110,
                     "savefig.bbox": "tight", "legend.fontsize": 8})
LABEL = {"objective1": "Объектив 1 (N=154)", "objective2": "Объектив 2 (N=215)", "objective2_N145": "Объектив 2, N=145",
         "objective2_N180": "Объектив 2, N=180", "condenser1": "Конденсор 1 (N=21)", "condenser2": "Конденсор 2 (N=31)"}


def save(fig, out, name):
    for ext in ("png", "pdf"):
        fig.savefig(os.path.join(out, f"{name}.{ext}"), dpi=200)
    plt.close(fig)
    print("  wrote", name)


def lay_key(lay):
    for k, v in (("Objective 1", "objective1"), ("Condenser 2", "condenser2")):
        if lay.get("lens_name", "") == k:
            return v
    return ""


def trajectories(run_dir, prefix, axis, out, name):
    """Ray paths in the x-z (or y-z) plane: source -> every refracting surface -> extrapolation past the focus."""
    summ = json.load(open(os.path.join(run_dir, f"{prefix}_summary.json")))
    rays = read_csv(os.path.join(run_dir, f"{prefix}_rays.csv"))
    trace = read_csv(os.path.join(run_dir, f"{prefix}_trace.csv"))
    lay = summ["layout"]
    c = "x" if axis == "x" else "y"
    zf = summ["focus"][axis]["method_A_ray_pairs"]["z_focus_mm"] or summ["focus"][axis]["method_C_paraxial_matrix"]["z_focus_mm"]
    z_end = lay["z_end_mm"]
    z_stop = max(zf, z_end) + 0.35 * abs(zf - z_end) + 1.0
    paths = {}
    for r in rays:
        paths[int(r["id"])] = [(lay["z_source_mm"], r[f"{c}0_um"])]
    for t in trace:
        paths[int(t["event"])].append((t["z_mm"], t[f"{c}_um"]))
    for r in rays:
        p = paths[int(r["id"])]
        p.append((r["z_exit_mm"], r[f"{c}_exit_um"]))
        s = (z_stop - r["z_exit_mm"]) / r["kz"]
        p.append((z_stop, r[f"{c}_exit_um"] + s * r[f"k{c}"] * 1000.0))
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4), gridspec_kw={"width_ratios": [2, 1]})
    cmap = plt.get_cmap("viridis")
    n = len(paths)
    for i, (k, p) in enumerate(sorted(paths.items())):
        z, u = zip(*p)
        for ax in (a1, a2):
            ax.plot(z, u, lw=0.8, color=cmap(i / max(n - 1, 1)))
    half_a = lay["parabolic_aperture_um"] / 2
    for ax in (a1, a2):
        ax.axvline(zf, color="crimson", ls="--", lw=0.9, label=f"параксиальный фокус z = {zf:.3f} мм")
        ax.set_xlabel("z, мм")
        ax.set_ylabel(f"{c}, мкм")
    a1.axvspan(lay["z_start_mm"], z_end, color="tab:blue", alpha=0.12, label=f"линза ({lay['n_elements']} элементов)")
    a1.axhline(half_a, color="gray", lw=0.6, ls=":")
    a1.axhline(-half_a, color="gray", lw=0.6, ls=":", label="параболическая апертура")
    a1.set_title(f"{LABEL.get(lay_key(lay), lay['lens_name'])}: лучи в плоскости {c}–z, E = {summ['energy_keV']:g} кэВ")
    a1.legend(loc="lower left")
    w = max(0.06 * abs(zf - z_end), 0.5)
    a2.set_xlim(zf - w, zf + w)
    a2.set_ylim(-0.08 * half_a, 0.08 * half_a)
    a2.set_title("увеличено: область фокуса")
    save(fig, out, name)


def rms_curves(vdir, out):
    lenses = [l for l in LABEL if os.path.exists(os.path.join(vdir, f"T5_grid_{l}", f"T5_grid_{l}_rms.csv"))]
    if not lenses:
        return
    fig, axs = plt.subplots(2, 3, figsize=(12, 6.5))
    for ax, lens in zip(axs.flat, lenses):
        d = os.path.join(vdir, f"T5_grid_{lens}")
        scan = read_csv(os.path.join(d, f"T5_grid_{lens}_rms.csv"))
        s = json.load(open(os.path.join(d, f"T5_grid_{lens}_summary.json")))
        z = [r["z_mm"] for r in scan]
        ax.plot(z, [r["rms_x_um"] for r in scan], label="RMS(x)")
        ax.plot(z, [r["rms_y_um"] for r in scan], ls="--", label="RMS(y)")
        for axis, col in (("x", "C0"), ("y", "C1")):
            zc = s["focus"][axis]["method_C_paraxial_matrix"]["z_focus_mm"]
            if zc is not None:
                ax.axvline(zc, color=col, lw=0.7, ls=":")
        ax.axvline(s["layout"]["z_end_mm"], color="k", lw=0.8, label="выходной торец линзы")
        ax.set_title(LABEL[lens])
        ax.set_xlabel("z, мм")
        ax.set_ylabel("RMS ширина пучка, мкм")
        ax.legend()
    fig.suptitle("RMS ширина пучка за линзой (21×21 луч по 90 % апертуры, 25 кэВ); пунктир — параксиальные фокусы x и y (матричная модель)")
    fig.tight_layout()
    save(fig, out, "fig3_rms_vs_z")


def focal_comparison(data, out):
    rows = data.get("T5")
    if not rows:
        return
    fig, axs = plt.subplots(1, 2, figsize=(12, 4.2))
    groups = [("objective", "мм", 1.0), ("condenser", "м", 1e-3)]
    for ax, (kind, unit, sc) in zip(axs, groups):
        sel = [r for r in rows if r["lens"].startswith(kind)]
        keys = [("f_thin_x", "тонкая линза R/(2Nδ)"), ("efl_A_x", "EFL, трассировка"), ("efl_C_x", "EFL, матрицы"),
                ("bfl_cont_x", "фокус от торца, непрерывная модель"), ("bfl_A_x", "фокус от торца, трассировка"),
                ("bfl_C_x", "фокус от торца, матрицы")]
        w = 0.8 / len(keys)
        for i, (k, lab) in enumerate(keys):
            ax.bar([j + (i - len(keys) / 2) * w + w / 2 for j in range(len(sel))], [r[k] * sc for r in sel], w, label=lab)
        ax.set_xticks(range(len(sel)))
        ax.set_xticklabels([LABEL[r["lens"]] for r in sel], rotation=15)
        ax.set_ylabel(f"расстояние, {unit}")
        ax.axhline(0, color="k", lw=0.6)
        ax.set_title(("объективы" if kind == "objective" else "конденсоры") + ", ось x, 25 кэВ")
    axs[0].legend(ncol=2)
    save(fig, out, "fig4_focal_lengths_analytic_vs_numeric")

    t10 = data.get("T10")
    if t10:
        fig, ax = plt.subplots(figsize=(7.5, 4))
        labels = [f"{LABEL[r['lens']]}\n{r['E_keV']} кэВ" for r in t10]
        x = range(len(t10))
        ax.plot(x, [100 * r["table_vs_thin"] for r in t10], "o", label="табл. 2 / R/(2Nδ(E)) − 1")
        ax.plot(x, [100 * (r["table_cm"] / r["efl_cm"] - 1) for r in t10], "s", label="табл. 2 / EFL (трассировка) − 1")
        ax.plot(x, [100 * (r["table_cm"] / r["bfl_end_cm"] - 1) if abs(r["bfl_end_cm"]) > 1e-9 else math.nan for r in t10],
                "^", label="табл. 2 / (фокус от торца) − 1")
        ax.set_xticks(list(x))
        ax.set_xticklabels(labels, rotation=90, fontsize=7)
        ax.set_ylabel("относительное различие, %")
        ax.set_yscale("symlog", linthresh=2)
        ax.legend()
        ax.set_title("Фокусные расстояния табл. 2 диссертации vs тонколинзовая формула и трассировка")
        save(fig, out, "fig4b_table2_comparison")


def n_dependence(data, out):
    rows = data.get("T6")
    if not rows:
        return
    N = [r["N"] for r in rows]
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(11, 4))
    a1.loglog(N, [r["f_thin"] for r in rows], "k-", label="тонкая линза R/(2Nδ)")
    a1.loglog(N, [r["efl_cont"] for r in rows], "C2--", label="EFL, модель непрерывной толстой линзы")
    a1.loglog(N, [r["efl_A"] for r in rows], "o", label="EFL, трассировка Geant4 (метод A)")
    a1.loglog(N, [r["efl_C"] for r in rows], "x", ms=8, label="EFL, матричная модель (метод C)")
    a1.set_xlabel("N (элементов на одно поперечное направление)")
    a1.set_ylabel("фокусное расстояние, мм")
    a1.legend()
    a1.set_title("Геометрия объектива 1, 25 кэВ")
    a2.semilogx(N, [r["ratio_efl_thin"] for r in rows], "o-", label="EFL / f_thin")
    a2.semilogx(N, [r["bfl_A"] / r["f_thin"] for r in rows], "s-", label="(фокус − торец) / f_thin")
    a2.axhline(1, color="k", lw=0.6)
    a2.set_xlabel("N")
    a2.set_ylabel("отношение")
    a2.legend()
    a2.set_title("отход от закона тонкой линзы f ∝ 1/N")
    save(fig, out, "fig5_focal_length_vs_N")


def geometry_convergence(data, out):
    d = data.get("T9")
    if not d:
        return
    rows = d["rows"]
    n = [r["segments"] for r in rows]
    fig, ax = plt.subplots(figsize=(6, 4))
    ax.loglog(n, [r["mean_rel_deflection_error"] for r in rows], "o-", label="среднее |Δθ|/θ по 40 лучам")
    ax.loglog(n, [r["max_rel_deflection_error"] for r in rows], "s-", label="максимум |Δθ|/θ")
    ax.loglog(n, [r["rms_focus_rel_error"] for r in rows], "^-", label="ошибка положения RMS-фокуса")
    ref = rows[2]["mean_rel_deflection_error"] * n[2]
    ax.loglog(n, [ref / k for k in n], "k:", label="∝ 1/n")
    ax.set_xlabel("число хорд на параболу (G4ExtrudedSolid)")
    ax.set_ylabel("относительная ошибка vs точное тело")
    ax.set_title(f"Ошибка многогранной поверхности, один элемент (порядок {d['order']:.2f})")
    ax.legend()
    save(fig, out, "fig6_surface_representation_error")


def snell(data, out):
    rows = [r for r in data.get("T2", []) if r["status"] == "refracted"]
    if not rows:
        return
    fig, ax = plt.subplots(figsize=(6, 3.6))
    for dl, m in ((1e-3, "o"), (4.32e-7, "s")):
        sel = [r for r in rows if r["delta"] == dl]
        ax.semilogy([r["alpha"] for r in sel], [max(r["err"], 1e-18) for r in sel], m, label=f"δ = {dl:g}")
    ax.set_xlabel("угол падения, град")
    ax.set_ylabel("|θ_trace − θ_Snell|, rad")
    ax.set_title("Плоская пластина SU-8: угол преломления Geant4 vs точный закон Снеллиуса")
    ax.legend()
    save(fig, out, "fig7_snell_plane_slab")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--validation", required=True)
    ap.add_argument("--traces", default="results")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    vres = os.path.join(a.validation, "validation_results.json")
    data = json.load(open(vres))["data"] if os.path.exists(vres) else {}
    for prefix, axis, name in (("objective1_fan_trace", "x", "fig1_rays_xz_objective1"),
                               ("objective1_fan_trace_y", "y", "fig2_rays_yz_objective1"),
                               ("condenser2_fan_trace", "x", "fig1b_rays_xz_condenser2")):
        d = os.path.join(a.traces, prefix)
        if os.path.exists(os.path.join(d, f"{prefix}_trace.csv")):
            trajectories(d, prefix, axis, a.out, name)
    rms_curves(a.validation, a.out)
    focal_comparison(data, a.out)
    n_dependence(data, a.out)
    geometry_convergence(data, a.out)
    snell(data, a.out)


if __name__ == "__main__":
    main()
