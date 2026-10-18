#!/usr/bin/env python3
"""Collect all figures of stage 1 and stage 2 into ONE folder, numbered, with the figure
caption printed under every image (PNG) + vector originals (PDF) + FIGURES.md (list of captions
and the data files each figure is built from).

usage: figure_catalog.py --stage1 results/figures --stage2 data/figures_src --out data/figures [--keep-src]
"""
import argparse
import os
import shutil
import textwrap

import matplotlib

matplotlib.use("Agg")
import matplotlib.image as mpimg  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402

L = {"objective1": "объектив 1 (R = 9 мкм, A = 90 мкм, N = 154, L = 81 мм)",
     "objective2": "объектив 2 (R = 6 мкм, A = 60 мкм, N = 215, L = 81 мм)",
     "objective2_N145": "объектив 2, вариант N = 145 (L = 55 мм)",
     "objective2_N180": "объектив 2, вариант N = 180 (L = 68 мм)",
     "condenser1": "конденсор 1 (R = 89 мкм, A = 909 мкм, N = 21, L = 97,9 мм)",
     "condenser2": "конденсор 2 (R = 60 мкм, A = 609 мкм, N = 31, L = 97 мм)"}

S1 = "Этап 1 (только преломление, физика взаимодействий выключена). "
S2 = "Этап 2 (преломление + Geant4 Livermore: фотоэффект, комптоновское и рэлеевское рассеяние, флуоресценция). "

# (stage, source name, caption, data files)
CATALOG = [
    (1, "fig1_rays_xz_objective1",
     S1 + "Ход 11 лучей веера через " + L["objective1"] + " при 25 кэВ в плоскости x–z: точки излома — 616 преломляющих "
     "поверхностей, за линзой лучи продолжены прямыми. Справа — увеличенная область фокуса: лучи сходятся в расчётной точке "
     "(параксиальный фокус z = 119,077 мм, т.е. 38,08 мм за выходным торцом).",
     "results/objective1_fan_trace/*_trace.csv, *_rays.csv"),
    (1, "fig2_rays_yz_objective1",
     S1 + "То же для плоскости y–z (фокусировка элементами, ориентированными по y). Фокус по y смещён на 0,263 мм "
     "относительно фокуса по x — астигматизм, вызванный чередованием X/Y-элементов.",
     "results/objective1_fan_trace_y/"),
    (1, "fig1b_rays_xz_condenser2",
     S1 + "Ход лучей через " + L["condenser2"] + " при 25 кэВ: фокус на расстоянии 2,21 м за линзой.",
     "results/condenser2_fan_trace/"),
    (1, "fig3_rms_vs_z",
     S1 + "Среднеквадратичная ширина пучка RMS(x), RMS(y) за линзой для шести массивов (21×21 луч по 90 % апертуры, 25 кэВ). "
     "Минимум совпадает с параксиальными фокусами матричной модели (пунктир). У объектива 2 (N = 215) фокус лежит внутри "
     "линзы, поэтому за торцом пучок только расходится.",
     "results/validation/T5_grid_*/*_rms.csv"),
    (1, "fig4_focal_lengths_analytic_vs_numeric",
     S1 + "Фокусные расстояния при 25 кэВ: тонколинзовая оценка R/(2Nδ), эффективное фокусное расстояние (EFL) и расстояние "
     "от выходного торца до фокуса по трассировке Geant4, по независимой матричной модели и по модели непрерывной толстой "
     "линзы. Трассировка и матрицы совпадают с точностью 10⁻¹⁰.",
     "results/validation/validation_results.json (T5)"),
    (1, "fig4b_table2_comparison",
     S1 + "Сравнение фокусных расстояний таблицы 2 диссертации с формулой тонкой линзы и с расчётом. Таблица совпадает с "
     "R/(2Nδ(E)) в пределах 1,2 % (кроме объектива 1 при 30 кэВ: −6,6 %), но реальные фокусы длинных объективов заметно "
     "ближе к торцу.",
     "results/validation/validation_results.json (T10)"),
    (1, "fig5_focal_length_vs_N",
     S1 + "Зависимость фокусного расстояния от числа элементов N (геометрия объектива 1, 25 кэВ). При N ≲ 20 выполняется "
     "закон тонкой линзы f ∝ 1/N; для длинных линз EFL растёт, а фокус приближается к торцу и при N > ~200 уходит внутрь линзы.",
     "results/validation/validation_results.json (T6)"),
    (1, "fig6_surface_representation_error",
     S1 + "Ошибка отклонения лучей при замене точных параболических поверхностей многогранником из n хорд (G4ExtrudedSolid) "
     "относительно точного тела ParabolicLensSolid: сходимость только ∝ 1/n — поэтому в модели используется точное тело.",
     "results/validation/validation_results.json (T9)"),
    (1, "fig7_snell_plane_slab",
     S1 + "Проверка закона Снеллиуса на плоской пластине SU-8: отличие угла преломления, полученного в Geant4, от точного "
     "значения asin(sin θ/(1−δ)) — на уровне машинной точности (≤ 3·10⁻¹⁵ рад) при δ = 10⁻³ и 4,32·10⁻⁷.",
     "results/validation/T2_*/"),
    (2, "fig11_beam_through_array_objective1",
     S2 + "Пучок 25 кэВ (300 фотонов, равномерно по апертуре) проходит через " + L["objective1"] + ". Вверху — масштаб "
     "апертуры: прошедшие без взаимодействий фотоны (синие) сходятся в расчётном фокусе; ×, ○, □ — точки фотоэффекта, "
     "комптоновского и рэлеевского рассеяния. Внизу — тот же пучок в масштабе миллиметров: рассеянные фотоны (серые) "
     "разлетаются из линзы.",
     "data/06_beam_visualisation/beam_objective1_25keV_*"),
    (2, "fig11_beam_through_array_condenser2",
     S2 + "То же для " + L["condenser2"] + " (фокус за пределами рисунка, 2,2 м).",
     "data/06_beam_visualisation/beam_condenser2_25keV_*"),
] + [
    (2, f"fig01_sanity_rays_{k}",
     S2 + f"Проверка адекватности на малом числе событий: 40 из 1000 фотонов 25 кэВ, {L[k]}. Фотоны без взаимодействий "
     "сходятся в расчётном фокусе (красный пунктир); показаны точки и типы взаимодействий.",
     f"data/01_sanity_1000ev/sanity_{k}_25keV_*")
    for k in L
] + [
    (2, "fig00_attenuation_SU8_Sb_edge",
     "Коэффициенты ослабления SU-8 (ρ = 1,24 г/см³) из моделей Geant4 Livermore по процессам, вклад сурьмы и контрольный "
     "состав без Sb; справа — доля Sb в фотопоглощении и в полном ослаблении. На K-крае Sb (30,49 кэВ) полное ослабление "
     "скачком возрастает в 2,2 раза.",
     "data/00_attenuation/mu_*.csv"),
    (2, "fig02_photon_fate_25keV",
     S2 + "Судьба фотонов 25 кэВ, равномерно освещающих апертуру каждого из шести массивов (100 000 фотонов на массив): "
     "прошли без взаимодействий / рассеяны и дошли до выхода / поглощены / рассеяны из пучка.",
     "data/02_lens_arrays_25keV/table_fate_25keV.csv"),
    (2, "fig03_interactions_along_lens_25keV",
     S2 + "Число взаимодействий каждого типа в каждом элементе вдоль линзы (25 кэВ). В объективах лучи сходятся к оси внутри "
     "линзы, проходят меньше материала, и к концу линзы взаимодействий меньше.",
     "data/02_lens_arrays_25keV/*_element_profile.csv"),
    (2, "fig04_transmission_profile_25keV",
     S2 + "Доля фотонов без взаимодействий в зависимости от удаления точки входа от оси, (x₀² + y₀²)/a². Точки — Монте-Карло "
     "Geant4, линия — закон Бугера–Ламберта–Бера exp(−μL) с длиной пути по реальной (преломлённой) траектории: совпадение "
     "в пределах статистики для всех массивов.",
     "data/02_lens_arrays_25keV/*_photons.csv.gz, table_beer_lambert_check_25keV.csv"),
    (2, "fig05_focal_plane_25keV",
     S2 + "Фокальная плоскость при 25 кэВ: фотоны без взаимодействий собраны в точку (геометрическое пятно ≪ 1 мкм), "
     "рассеянные фотоны, дошедшие до плоскости, распределены на сотни микрометров — миллиметры.",
     "data/02_lens_arrays_25keV/*_focal_plane_xy.csv.gz"),
    (2, "fig09_waist_plane_components",
     S2 + "Плоскость перетяжки (минимум RMS фотонов без взаимодействий) при 25; 30,4; 30,6 и 35 кэВ. Вверху — доля упавших "
     "на массив фотонов, пришедших в фокусное пятно r ≤ 1 мкм, в шум, поглощённых и рассеянных из пучка; внизу — состав "
     "шума (рэлеевское, комптоновское рассеяние, флуоресценция Sb). Рассеянные фотоны в пятно не попадают.",
     "data/05_waist_plane_components/table_waist_plane_components.csv"),
    (2, "fig10_waist_radial_objective1",
     S2 + "Радиальное распределение фотонов в плоскости перетяжки, " + L["objective1"] + ", по компонентам: без "
     "взаимодействий (в пятне), рэлеевские, комптоновские, вторичные (флуоресценция Sb выше K-края).",
     "data/05_waist_plane_components/objective1_*_photons_ntuple.csv.gz"),
    (2, "fig10_waist_radial_condenser2",
     S2 + "То же для " + L["condenser2"] + ".",
     "data/05_waist_plane_components/condenser2_*_photons_ntuple.csv.gz"),
    (2, "fig06_sb_edge_scan",
     S2 + "Сканирование по энергии через K-край сурьмы для объектива 1 и конденсора 2: SU-8 (сплошные) и SU-8 без Sb "
     "(штриховые), 50 000 фотонов на точку. На краю доля прошедших без взаимодействий падает вдвое, без Sb скачка нет.",
     "data/03_sb_k_edge/table_sb_edge_scan.csv"),
    (2, "fig07_secondary_spectrum_exit",
     S2 + "Спектр вторичных фотонов на выходной плоскости под (30,40 кэВ) и над (30,60 кэВ) K-краем сурьмы: над краем "
     "появляется флуоресценция Sb Kα/Kβ.",
     "data/03_sb_k_edge/*_secondaries_exit.csv"),
    (2, "fig08_focal_spot_vs_divergence",
     S2 + "Фокальное пятно объектива 1 при 25 кэВ для пучка с угловой расходимостью σθ = 0, 1, 3 мкрад (модель конечного "
     "источника, иллюстрация): размер пятна σ ≈ EFL·σθ.",
     "data/04_focal_spot_divergence/"),
]


def compose(png, caption, number, out_png):
    img = mpimg.imread(png)
    h, w = img.shape[:2]
    dpi = 150
    fw = w / dpi
    lines = textwrap.wrap(f"Рис. {number}. {caption}", width=max(60, int(fw * 9.3)))
    cap_h = (0.35 + 0.24 * len(lines))
    fig = plt.figure(figsize=(fw, h / dpi + cap_h), dpi=dpi)
    ax = fig.add_axes([0, cap_h / (h / dpi + cap_h), 1, (h / dpi) / (h / dpi + cap_h)])
    ax.imshow(img)
    ax.axis("off")
    fig.text(0.02, (cap_h - 0.15) / (h / dpi + cap_h), "\n".join(lines), ha="left", va="top", fontsize=12, linespacing=1.35)
    fig.savefig(out_png, dpi=dpi, facecolor="white")
    plt.close(fig)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage1", required=True)
    ap.add_argument("--stage2", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--keep-src", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    md = ["# Рисунки (этап 1 и этап 2)", "",
          "Все рисунки построены из расчётных файлов проекта. `NN_имя.png` — с подписью, `NN_имя.pdf` — векторный оригинал "
          "без подписи (для статьи/постера).", "",
          "| № | файл | подпись | исходные данные |", "|---|---|---|---|"]
    n = 0
    for stage, name, cap, src in CATALOG:
        d = a.stage1 if stage == 1 else a.stage2
        png = os.path.join(d, name + ".png")
        if not os.path.exists(png):
            print("  (missing, skipped)", name)
            continue
        n += 1
        base = f"{n:02d}_{name.split('_', 1)[1] if name.startswith('fig') else name}"
        compose(png, cap, n, os.path.join(a.out, base + ".png"))
        pdf = os.path.join(d, name + ".pdf")
        if os.path.exists(pdf):
            shutil.copy(pdf, os.path.join(a.out, base + ".pdf"))
        md.append(f"| {n} | `{base}.png` | {cap} | `{src}` |")
        print("  figure", base)
    with open(os.path.join(a.out, "FIGURES.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    if not a.keep_src and os.path.abspath(a.stage2) != os.path.abspath(a.out):
        shutil.rmtree(a.stage2, ignore_errors=True)
    print(f"{n} figures in {a.out}")


if __name__ == "__main__":
    main()
