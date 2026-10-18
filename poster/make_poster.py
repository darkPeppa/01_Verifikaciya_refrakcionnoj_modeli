#!/usr/bin/env python3
"""A1 poster (594 x 841 mm) built directly from the computed results of the project.

usage: make_poster.py --results <stage-1 results dir> --data <stage-2 data dir> --out <dir>
Writes poster_A1.pdf (vector, for printing) and poster_A1.png (preview).
"""
import argparse
import csv
import gzip
import json
import math
import os

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from matplotlib.lines import Line2D  # noqa: E402
from matplotlib.patches import FancyBboxPatch  # noqa: E402

NAVY, ACC, ORANGE, GREEN, GREY, VIOLET = "#14325c", "#c0392b", "#e67e22", "#2e8b57", "#8c8c8c", "#8e44ad"
LIGHT = "#eef3f9"
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 15, "axes.titlesize": 17, "axes.labelsize": 15,
                     "xtick.labelsize": 13, "ytick.labelsize": 13, "legend.fontsize": 12, "axes.grid": True,
                     "grid.alpha": 0.3, "axes.edgecolor": "#555555"})
W, H = 594 / 25.4, 841 / 25.4
NAME = {"objective1": "Объектив 1", "objective2": "Объектив 2", "objective2_N145": "Объектив 2, N=145",
        "objective2_N180": "Объектив 2, N=180", "condenser1": "Конденсор 1", "condenser2": "Конденсор 2"}
SHORT = {"objective1": "Об.1", "objective2": "Об.2", "objective2_N145": "Об.2\nN145", "objective2_N180": "Об.2\nN180",
         "condenser1": "Конд.1", "condenser2": "Конд.2"}


def rc(x, nd):
    """number with a decimal comma"""
    return f"{x:.{nd}f}".replace(".", ",")


def rcsv(path):
    op = gzip.open if path.endswith(".gz") else open
    with op(path, "rt") as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith("#")))
    out = []
    for r in rows:
        o = {}
        for k, v in r.items():
            try:
                o[k] = float(v)
            except (TypeError, ValueError):
                o[k] = v
        out.append(o)
    return out


def box(fig, x, y, w, h, title, color=NAVY):
    fig.patches.append(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.002,rounding_size=0.006", transform=fig.transFigure,
                                      facecolor="white", edgecolor=color, linewidth=2.2, zorder=-5))
    fig.patches.append(FancyBboxPatch((x, y + h - 0.021), w, 0.021, boxstyle="round,pad=0.002,rounding_size=0.006",
                                      transform=fig.transFigure, facecolor=color, edgecolor=color, zorder=-4))
    fig.text(x + 0.01, y + h - 0.0105, title, color="white", fontsize=24, fontweight="bold", va="center")


def text(fig, x, y, s, size=16, **kw):
    fig.text(x, y, s, fontsize=size, va="top", ha="left", linespacing=1.38, **kw)


def table(fig, rect, rows, fs=14, scale=2.2, col_w=None, hl_rows=()):
    ax = fig.add_axes(rect)
    ax.axis("off")
    tb = ax.table(cellText=rows, loc="center", cellLoc="center", colWidths=col_w)
    tb.auto_set_font_size(False)
    tb.set_fontsize(fs)
    tb.scale(1, scale)
    for (i, j), cell in tb.get_celld().items():
        cell.set_edgecolor("#c8c8c8")
        if i == 0:
            cell.set_facecolor(NAVY)
            cell.set_text_props(color="white", fontweight="bold")
        elif i in hl_rows:
            cell.set_facecolor("#f3f6fa")
    return ax


# ------------------------------------------------------------------ panels
def lens_3d(fig, rect):
    ax = fig.add_axes(rect, projection="3d")
    R, d, p, a = 9.0, 10.0, 235.0, 45.0
    gap, Hh = 120.0, 70.0
    u = np.linspace(-a, a, 40)
    U, V = np.meshgrid(u, np.linspace(-Hh, Hh, 2))
    S = d / 2 + U ** 2 / (2 * R)
    for i in range(4):
        zc = i * (p + gap)
        col = "#3d7ab8" if i % 2 == 0 else "#5fa35f"
        for sg in (-1, 1):
            if i % 2 == 0:
                ax.plot_surface(zc + sg * S, U, V, color=col, alpha=0.45, linewidth=0)
            else:
                ax.plot_surface(zc + sg * S, V, U, color=col, alpha=0.45, linewidth=0)
    zs = np.linspace(-250, 4 * (p + gap) + 600, 50)
    zf = 4 * (p + gap) + 900
    for x0, y0 in [(30, 0), (-30, 0), (0, 30), (0, -30), (20, 20), (-20, -20)]:
        k = np.clip(zs / zf, 0, 1)
        ax.plot(zs, x0 * (1 - k), y0 * (1 - k), color=ACC, lw=1.6)
    ax.set_xlabel("z (ось пучка)", labelpad=6)
    ax.set_ylabel("x", labelpad=0)
    ax.set_zlabel("y", labelpad=0)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_zticks([])
    ax.view_init(elev=22, azim=-58)
    ax.set_box_aspect((3.2, 1, 1))
    ax.set_title("")


def beam_panels(fig, rect_top, rect_bot, ddir):
    base = os.path.join(ddir, "06_beam_visualisation")
    name = "beam_objective1_25keV"
    s = json.load(open(os.path.join(base, f"{name}_summary.json")))
    rays = {int(r["id"]): r for r in rcsv(os.path.join(base, f"{name}_rays.csv.gz"))}
    tr = rcsv(os.path.join(base, f"{name}_trace.csv.gz"))
    lay = s["layout"]
    zf = s["focus"]["x"]["method_C_paraxial_matrix"]["z_focus_mm"]
    z_end = lay["z_end_mm"]
    hw = 3 * max(lay["height_um"] / 2, lay["parabolic_aperture_um"] / 2) / 1000 + 1.0
    paths, last_dir, marks = {}, {}, []
    for t in tr:
        ev = int(t["event"])
        paths.setdefault(ev, [(lay["z_source_mm"], rays[ev]["x0_um"] / 1000)]).append((t["z_mm"], t["x_um"] / 1000))
        last_dir[ev] = (t["kx_after"], t["kz_after"])
        if t["status"] in ("phot", "compt", "Rayl"):
            marks.append((t["z_mm"], t["x_um"] / 1000, t["status"]))
    zst = zf * 1.3
    a1 = fig.add_axes(rect_top)
    a2 = fig.add_axes(rect_bot)
    col = {"unscattered": NAVY, "scattered_transmitted": ORANGE, "absorbed": ACC, "lost": GREY, "outside_aperture": "m"}
    for ev, p in paths.items():
        r = rays[ev]
        p = list(p)
        if r["reached_exit"]:
            p.append((r["z_exit_mm"], r["x_exit_um"] / 1000))
            p.append((zst, r["x_exit_um"] / 1000 + (zst - r["z_exit_mm"]) / r["kz"] * r["kx"]))
        elif r["fate"] == "lost" and ev in last_dir:
            kx, kz = last_dir[ev]
            z0, x0 = p[-1]
            Lm = 1e9
            if abs(kx) > 1e-12:
                Lm = min(Lm, (math.copysign(hw, kx) - x0) / kx)
            if abs(kz) > 1e-12:
                Lm = min(Lm, ((zst if kz > 0 else lay["z_source_mm"]) - z0) / kz)
            p.append((z0 + Lm * kz, x0 + Lm * kx))
        z, x = zip(*p)
        lost = r["fate"] == "lost"
        a1.plot(z, [v * 1000 for v in x], color=col[r["fate"]], lw=0.6 if lost else 0.9, alpha=0.22 if lost else 0.85)
        a2.plot(z, x, color=col[r["fate"]], lw=1.0, alpha=0.8)
    for proc, m, c in (("phot", "x", ACC), ("compt", "o", ORANGE), ("Rayl", "s", GREEN)):
        pts = [(z, x) for z, x, pp in marks if pp == proc]
        if pts:
            zz, xx = zip(*pts)
            a1.plot(zz, [v * 1000 for v in xx], m, color=c, ms=6, mew=1.6, mfc="none" if m != "x" else c, ls="none")
            a2.plot(zz, xx, m, color=c, ms=5, mew=1.3, mfc="none" if m != "x" else c, ls="none")
    for ax in (a1, a2):
        ax.axvspan(0, z_end, color="#3d7ab8", alpha=0.10)
        ax.axvline(zf, color="crimson", ls="--", lw=1.6)
        ax.set_xlim(lay["z_source_mm"], zst)
    a1.set_ylim(-68, 68)
    a1.set_ylabel("x, мкм")
    a1.text(z_end / 2, 58, f"объектив 1: {lay['n_elements']} элементов SU-8, 81 мм", ha="center", fontsize=14)
    a1.text(zf + 1, -60, f"фокус: {zf - z_end:.1f} мм\nза торцом", color="crimson", fontsize=14)
    a1.set_title(f"300 фотонов 25 кэВ, масштаб апертуры: прошедшие без взаимодействий сходятся в расчётном фокусе", fontsize=16)
    a2.set_ylim(-hw, hw)
    a2.set_ylabel("x, мм")
    a2.set_xlabel("z, мм")
    a2.set_title("тот же пучок, масштаб миллиметров: рассеянные фотоны разлетаются из линзы", fontsize=16)
    h = [Line2D([], [], color=col[k], lw=3, label=l) for k, l in (("unscattered", "без взаимодействий"),
         ("scattered_transmitted", "рассеян, дошёл до выхода"), ("absorbed", "поглощён"), ("lost", "рассеян из пучка"))]
    h += [Line2D([], [], marker=m, color=c, ls="none", ms=9, mew=2, mfc="none" if m != "x" else c, label=l)
          for m, c, l in (("x", ACC, "фотоэффект"), ("o", ORANGE, "комптоновское"), ("s", GREEN, "рэлеевское"))]
    a2.legend(handles=h, loc="lower right", ncol=4, fontsize=12.5, framealpha=0.95)


def waist_bars(fig, rect, ddir):
    rows = rcsv(os.path.join(ddir, "05_waist_plane_components", "table_waist_plane_components.csv"))
    rows = [r for r in rows if r["spot_radius_um"] == 1.0]
    lenses = ["objective1", "objective2_N180", "condenser2"]
    energies = [25.0, 30.4, 30.6, 35.0]
    ax = fig.add_axes(rect)
    xs, labels = [], []
    pos = 0
    data = {(r["lens"], round(r["energy_keV"], 2)): r for r in rows}
    comp = [("pct_in_focal_spot", "в фокусное пятно r ≤ 1 мкм", NAVY), ("pct_noise_at_plane", "шум в плоскости перетяжки", ORANGE),
            ("pct_absorbed", "поглощены", ACC), ("pct_scattered_out_of_beam", "рассеяны из пучка", GREY)]
    for li, l in enumerate(lenses):
        for E in energies:
            r = data.get((l, E))
            b = 0
            for key, lab, c in comp:
                v = r[key] if r else 0
                ax.bar(pos, v, bottom=b, color=c, width=0.8, label=lab if (li == 0 and E == 25.0) else None)
                if v > 9:
                    ax.text(pos, b + v / 2, f"{v:.0f}", ha="center", va="center", color="white", fontsize=12, fontweight="bold")
                b += v
            xs.append(pos)
            labels.append(f"{E:g}")
            pos += 1
        ax.text(pos - 2.5, 103, NAME[l], ha="center", fontsize=14, fontweight="bold")
        pos += 0.7
    ax.set_xticks(xs)
    ax.set_xticklabels(labels, fontsize=12)
    ax.set_xlabel("энергия, кэВ", fontsize=14)
    ax.set_ylabel("% фотонов, упавших на массив")
    ax.set_ylim(0, 110)
    ax.grid(axis="x", visible=False)
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.17), ncol=2, fontsize=13)


def noise_composition(fig, rect, ddir):
    rows = rcsv(os.path.join(ddir, "05_waist_plane_components", "table_waist_plane_components.csv"))
    rows = [r for r in rows if r["spot_radius_um"] == 1.0 and r["lens"] == "objective1"]
    rows.sort(key=lambda r: r["energy_keV"])
    ax = fig.add_axes(rect)
    x = np.arange(len(rows))
    ray = np.array([100 * (r["n_spot_rayleigh"] + r["n_halo_rayleigh"]) / r["n_incident"] for r in rows])
    com = np.array([100 * (r["n_spot_compton"] + r["n_halo_compton"]) / r["n_incident"] for r in rows])
    flu = np.array([100 * r["n_fluorescence_at_plane"] / r["n_incident"] for r in rows])
    ax.bar(x, ray, color=GREEN, width=0.7, label="рэлеевское")
    ax.bar(x, com, bottom=ray, color=ORANGE, width=0.7, label="комптоновское")
    ax.bar(x, flu, bottom=ray + com, color=VIOLET, width=0.7, label="флуоресценция Sb")
    ax.set_xticks(x)
    ax.set_xticklabels([f"{r['energy_keV']:g}" for r in rows])
    ax.set_xlabel("энергия, кэВ", fontsize=13)
    ax.set_ylabel("% от упавших", fontsize=13)
    ax.set_title("состав шума, объектив 1", fontsize=15)
    ax.legend(fontsize=11, loc="upper right")


def mu_edge(fig, rect, ddir):
    a = rcsv(os.path.join(ddir, "00_attenuation", "mu_SU8_15-35keV.csv"))
    b = rcsv(os.path.join(ddir, "00_attenuation", "mu_SU8_noSb_15-35keV.csv"))
    ax = fig.add_axes(rect)
    e = [r["energy_keV"] for r in a]
    ax.plot(e, [r["mu_total_per_cm"] for r in a], color=NAVY, lw=2.6, label="SU-8: полное")
    ax.plot(e, [r["mu_phot_per_cm"] for r in a], color=ACC, lw=2, label="SU-8: фотоэффект")
    ax.plot(e, [r["mu_phot_Sb_per_cm"] for r in a], color=ACC, lw=1.8, ls=":", label="  из них сурьма")
    ax.plot(e, [r["mu_compt_per_cm"] for r in a], color=ORANGE, lw=1.6, label="Комптон")
    ax.plot(e, [r["mu_rayl_per_cm"] for r in a], color=GREEN, lw=1.6, label="Рэлей")
    ax.plot(e, [r["mu_total_per_cm"] for r in b], color=NAVY, lw=2, ls="--", label="SU-8 без Sb: полное")
    ax.axvline(30.491, color="gray", ls="-.")
    ax.text(30.7, 1.15, "K-край Sb\n30,49 кэВ", fontsize=13)
    ax.set_xlabel("энергия, кэВ")
    ax.set_ylabel("μ, 1/см")
    ax.set_xlim(15, 35)
    ax.set_ylim(0, 1.6)
    ax.set_title("Ослабление в SU-8 (Geant4 Livermore)")
    ax.legend(fontsize=11.5, loc="upper left", ncol=2)


def edge_scan(fig, rect, ddir):
    rows = rcsv(os.path.join(ddir, "03_sb_k_edge", "table_sb_edge_scan.csv"))
    ax = fig.add_axes(rect)
    for lens, c in (("objective1", NAVY), ("condenser2", GREEN)):
        for mat, ls, mk in (("SU8", "-", "o"), ("SU8_noSb", "--", "s")):
            rr = sorted([r for r in rows if r["lens"] == lens and r["material"] == mat], key=lambda r: r["energy_keV"])
            ax.plot([r["energy_keV"] for r in rr], [100 * r["frac_unscattered"] for r in rr], mk + ls, ms=6, color=c, lw=1.8,
                    label=f"{NAME[lens]}, {'SU-8' if mat == 'SU8' else 'SU-8 без Sb'}")
    ax.axvline(30.491, color="gray", ls="-.")
    ax.set_xlabel("энергия, кэВ")
    ax.set_ylabel("прошли без взаимодействий, %")
    ax.set_ylim(0, 60)
    ax.set_title("Пропускание массивов через K-край Sb")
    ax.legend(fontsize=11.5, loc="lower left", ncol=2)


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", required=True)
    ap.add_argument("--data", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--assets", default="", help="also write separate panel images (for the PPTX poster) to this dir")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    if a.assets:
        os.makedirs(a.assets, exist_ok=True)
        f = plt.figure(figsize=(15.5, 7.0))
        beam_panels(f, [0.07, 0.56, 0.91, 0.38], [0.07, 0.08, 0.91, 0.40], a.data)
        f.savefig(os.path.join(a.assets, "beam_objective1.png"), dpi=220, facecolor="white")
        plt.close(f)
        f = plt.figure(figsize=(7, 2.6))
        lens_3d(f, [0, 0, 1, 1])
        f.savefig(os.path.join(a.assets, "lens_elements_3d.png"), dpi=220, facecolor="white")
        plt.close(f)
    vres = json.load(open(os.path.join(a.results, "validation", "validation_results.json")))
    data = vres["data"]
    npass = sum(r["status"] == "PASS" for r in vres["results"])
    ntot = len(vres["results"])
    t3 = {r["lens"]: r for r in data["T3"]}
    t5 = {r["lens"]: r for r in data["T5"]}
    t10 = data["T10"]
    waist = {(r["lens"], round(r["energy_keV"], 2)): r for r in rcsv(os.path.join(a.data, "05_waist_plane_components", "table_waist_and_spot.csv"))}
    comp = {(r["lens"], round(r["energy_keV"], 2)): r for r in rcsv(os.path.join(a.data, "05_waist_plane_components", "table_waist_plane_components.csv"))
            if r["spot_radius_um"] == 1.0}
    bl = {r["lens"]: r for r in rcsv(os.path.join(a.data, "02_lens_arrays_25keV", "table_beer_lambert_check_25keV.csv"))}
    scan = rcsv(os.path.join(a.data, "03_sb_k_edge", "table_sb_edge_scan.csv"))
    sc = {(r["lens"], r["material"], round(r["energy_keV"], 2)): r for r in scan}
    div = rcsv(os.path.join(a.data, "04_focal_spot_divergence", "table_focal_spot_vs_divergence.csv"))
    mu = {round(r["energy_keV"], 3): r for r in rcsv(os.path.join(a.data, "00_attenuation", "mu_SU8_15-35keV.csv"))}

    fig = plt.figure(figsize=(W, H))
    fig.patch.set_facecolor(LIGHT)
    fig.patches.append(FancyBboxPatch((0, 0.912), 1, 0.088, transform=fig.transFigure, facecolor=NAVY, edgecolor="none",
                                      boxstyle="square,pad=0", zorder=-5))
    fig.text(0.5, 0.979, "Моделирование прохождения жёсткого рентгеновского излучения", ha="center", va="center",
             color="white", fontsize=40, fontweight="bold")
    fig.text(0.5, 0.958, "через преломляющие линзы SU-8 микроскопа МИР 15–35 кэВ в Geant4", ha="center", va="center",
             color="white", fontsize=36, fontweight="bold")
    fig.text(0.5, 0.936, "А. А. Неустроева¹, Е. Ф. Резникова¹, В. П. Назьмов¹ ² ³", ha="center", va="center", color="white",
             fontsize=25, fontweight="bold")
    fig.text(0.5, 0.920, "¹ ИЯФ им. Г. И. Будкера СО РАН   ² ЦКП «СКИФ», ИК СО РАН   ³ ИХТТМ СО РАН,  Новосибирск",
             ha="center", va="center", color="#dfe8f3", fontsize=18)

    c1, cw1 = 0.015, 0.30
    c2, cw2 = 0.330, 0.655

    # ===== row 1: task + model | beam through the array
    box(fig, c1, 0.640, cw1, 0.262, "Задача и модель")
    text(fig, c1 + 0.01, 0.873,
         "Объективы и конденсоры МИР — длинные составные\n"
         "преломляющие линзы из сотен линейных параболических\n"
         "элементов SU-8, скрещённых по x и y. Цель — модель в\n"
         "Geant4, где преломление на каждой поверхности считается\n"
         "точно, а в том же материале работают фотоэффект,\n"
         "комптоновское и рэлеевское рассеяние.", size=15.5)
    lens_3d(fig, [c1 + 0.0, 0.733, cw1, 0.080])
    text(fig, c1 + 0.01, 0.731,
         "• элементы X, Y, X, Y … (схема, масштаб условный)\n"
         "• профиль z = ±(d/2 + x²/2R), апертура 2√(R(p − d))\n"
         "• своё тело G4VSolid: точные пересечения луч–парабола\n"
         "• процесс преломления γ: векторный закон Снеллиуса\n"
         "   в устойчивой форме (δ ~ 10⁻⁷ без потери точности)\n"
         "• SU-8: 1,24 г/см³, C/O/H/Sb/F/S (Sb 1,33 %),\n"
         "   δ(25 кэВ) = 4,32·10⁻⁷; физика Livermore + флуоресценция", size=14.5)

    box(fig, c2, 0.640, cw2, 0.262, "Пучок через массив: моделирование Geant4")
    beam_panels(fig, [c2 + 0.045, 0.790, cw2 - 0.06, 0.080], [c2 + 0.045, 0.665, cw2 - 0.06, 0.095], a.data)

    # ===== row 2: stage 1 verification | waist plane
    box(fig, c1, 0.332, cw1, 0.300, "Этап 1: расчёт = моделирование")
    rows = [["Один элемент", "R/(2δ), м", "Geant4, м", "отклон."]]
    for l in ("objective1", "objective2", "condenser1", "condenser2"):
        r = t3[l]
        rows.append([NAME[l], f"{r['f_thin_mm'] / 1000:.4f}", f"{r['efl_A_mm'] / 1000:.4f}", f"{r['rel_A_thin']:.0e}"])
    text(fig, c1 + 0.01, 0.603, "Фокусное расстояние одного элемента, 25 кэВ:", size=15, fontweight="bold")
    table(fig, [c1 + 0.006, 0.515, cw1 - 0.012, 0.075], rows, fs=14, scale=2.2, col_w=[0.34, 0.22, 0.22, 0.2])
    rows = [["Массив", "EFL G4, мм", "EFL матр.", "фокус, мм", "расх."]]
    for l in ("objective1", "objective2_N180", "objective2", "condenser1", "condenser2"):
        r = t5[l]
        rows.append([NAME[l], f"{r['efl_A_x']:.3f}", f"{r['efl_C_x']:.3f}", f"{r['bfl_A_x']:.2f}", f"{abs(r['defl_AC_rel_x']):.0e}"])
    text(fig, c1 + 0.01, 0.497, "Массивы, 25 кэВ: трассировка Geant4 vs матрицы\n(фокус — от выходного торца):", size=15, fontweight="bold")
    table(fig, [c1 + 0.006, 0.383, cw1 - 0.012, 0.085], rows, fs=13.5, scale=2.1, col_w=[0.3, 0.19, 0.19, 0.18, 0.14])
    o1t = next(r for r in t10 if r["lens"] == "objective1" and r["E_keV"] == 25)
    text(fig, c1 + 0.01, 0.378,
         f"• {npass}/{ntot} автоматических проверок PASS; Снеллиус: ≤ 3·10⁻¹⁵ рад\n"
         f"• табл. 2 диссертации = тонкая линза R/(2Nδ) (±1,2 %);\n"
         f"   реальный фокус об. 1: {rc(o1t['bfl_end_cm'], 2)} см от торца вместо {rc(o1t['table_cm'], 1)} см", size=14.2)

    box(fig, c2, 0.332, cw2, 0.300, "Плоскость перетяжки: куда попадают фотоны, упавшие на массив")
    waist_bars(fig, [c2 + 0.045, 0.420, 0.40, 0.165], a.data)
    noise_composition(fig, [c2 + 0.505, 0.470, 0.135, 0.115], a.data)
    w1 = waist[("objective1", 25.0)]
    rows = [["25 кэВ", "перетяжка, мм", "zx − zy, мм", "пятно, нм"]]
    for l in ("objective1", "objective2_N180", "condenser2"):
        w = waist[(l, 25.0)]
        rows.append([NAME[l].replace("Объектив", "Об.").replace("Конденсор", "Конд."), f"{w['waist_from_lens_end_mm']:.1f}",
                     f"{w['astigmatism_zx_minus_zy_mm']:.2f}", f"{1000 * w['spot_estimate_thesis_5_2_5_um']:.0f}"])
    table(fig, [c2 + 0.462, 0.350, 0.185, 0.072], rows, fs=11, scale=1.8, col_w=[0.24, 0.29, 0.25, 0.22])
    text(fig, c2 + 0.465, 0.432, "перетяжка — от выходного торца;\nпятно — оценка по формуле (5.2.5)", size=12)
    d1 = next(r for r in div if r["divergence_sigma_urad"] == 1.0)
    text(fig, c2 + 0.012, 0.372,
         f"В пятно (r ≤ 1 мкм) попадают только фотоны без взаимодействий;\n"
         f"рассеянные (0,1–0,8 % упавших) приходят в сотнях мкм – мм от оси.\n"
         f"Геометрическое пятно идеальной линзы < 0,05 мкм (астигматизм X/Y);\n"
         f"реальное: дифракция с поглощением ~{1000 * w1['spot_estimate_thesis_5_2_5_um']:.0f} нм (5.2.5), источник 1 мкрад → σ = {1000 * d1['sigma_x_um']:.0f} нм.",
         size=14)

    # ===== row 3: Sb
    box(fig, c1, 0.135, 0.97, 0.190, "Роль сурьмы: K-край 30,49 кэВ", color="#7a1f1f")
    mu_edge(fig, [c1 + 0.045, 0.165, 0.27, 0.125], a.data)
    edge_scan(fig, [c1 + 0.37, 0.165, 0.27, 0.125], a.data)
    m25, m30a, m30b = mu[25.0], mu[30.48], mu[30.5]
    o_lo, o_hi = sc[("objective1", "SU8", 30.48)], sc[("objective1", "SU8", 30.5)]
    o_ns = sc[("objective1", "SU8_noSb", 25.0)]
    o_25 = sc[("objective1", "SU8", 25.0)]
    o_35 = sc[("objective1", "SU8", 35.0)]
    text(fig, c1 + 0.668, 0.300,
         f"• 1,33 мас.% Sb дают {100 * m25['mu_phot_Sb_per_cm'] / m25['mu_phot_per_cm']:.0f}% фотопоглощения SU-8\n"
         f"   уже при 25 кэВ\n"
         f"• на краю μ: {rc(m30a['mu_total_per_cm'], 3)} → {rc(m30b['mu_total_per_cm'], 3)} 1/см (×{rc(m30b['mu_total_per_cm'] / m30a['mu_total_per_cm'], 1)})\n"
         f"• объектив 1, без взаимодействий:\n"
         f"   {100 * o_lo['frac_unscattered']:.0f}% (30,48 кэВ) → {100 * o_hi['frac_unscattered']:.0f}% (30,50 кэВ);\n"
         f"   35 кэВ: {100 * o_35['frac_unscattered']:.0f}%\n"
         f"• без Sb при 25 кэВ: {100 * o_ns['frac_unscattered']:.0f}% вместо {100 * o_25['frac_unscattered']:.0f}%\n"
         f"• выше края — флуоресценция Sb Kα/Kβ", size=16.5)

    # ===== conclusions
    box(fig, c1, 0.012, 0.97, 0.116, "Выводы")
    text(fig, c1 + 0.012, 0.102,
         "1. Модель Geant4 с точным преломлением воспроизводит аналитику: один элемент — 10⁻⁷, массивы из 300–430 элементов — 10⁻¹⁰ (матричная модель).\n"
         "2. Табл. 2 — тонколинзовые оценки: фокус длинных объективов в 1,8–3 раза ближе к торцу; объектив 2 (N = 215) при 25 кэВ фокусирует внутри себя.\n"
         f"3. В фокус приходит только непровзаимодействовавшая часть пучка: {comp[('condenser1', 25.0)]['pct_in_focal_spot']:.0f}–{comp[('objective2_N145', 25.0)]['pct_in_focal_spot']:.0f}% при 25 кэВ; "
         "остальное\n    поглощается (фотоэффект) или рассеивается из пучка; шум в плоскости перетяжки < 1 % и весь вне пятна.\n"
         "4. Над K-краем Sb пропускание падает вдвое: рабочую энергию МИР выгодно держать ≤ 30,4 кэВ, а снижение доли Sb в резисте увеличит\n"
         "    пропускание (без Sb при 25 кэВ — в 1,3 раза больше фотонов в фокусе).\n"
         "5. Монте-Карло совпадает с законом Бугера–Ламберта–Бера по преломлённым траекториям; формула (5.2.6) с прямыми лучами занижает\n"
         "    пропускание длинных объективов на 18–37 %, для конденсоров верна до 1 %.",
         size=16)
    fig.text(0.985, 0.004, f"Geant4 11.4.2 · Монте-Карло 10⁵ фотонов на точку · закон Бугера–Ламберта–Бера подтверждён (|pull| < 1) · "
             f"все графики построены из файлов данных проекта", ha="right", va="bottom", fontsize=11, color="#555555")
    pdf = os.path.join(a.out, "poster_A1.pdf")
    fig.savefig(pdf)
    fig.savefig(os.path.join(a.out, "poster_A1.png"), dpi=60)
    print("poster written:", pdf)


if __name__ == "__main__":
    main()
