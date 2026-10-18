# Validation report — XrayCRLGeant4, stage 1 (refraction model)

Total: 16 checks, **16 PASS**, **0 FAIL**, run time 0.1 min.

| ID | Test | Criterion | Result | Status |
|---|---|---|---|---|
| T1 | no refraction (delta=0), objective 1 full set, 11x11 rays | |k - (0,0,1)| = 0, lateral shift < 1e-9 um, no finite focus | max|dk| = 0.0e+00, max lateral shift = 0.0e+00 um, valid 121/121, focus: none | **PASS** |
| T1 | no refraction (process off), objective 1 full set, 11x11 rays | |k - (0,0,1)| = 0, lateral shift < 1e-9 um, no finite focus | max|dk| = 0.0e+00, max lateral shift = 0.0e+00 um, valid 121/121, focus: none | **PASS** |
| T2 | vector Snell law at a plane SU-8 slab (delta = 1e-3 and 4.32e-7, 0..89.97 deg) | |theta_refr - asin(sin(theta)/(1-delta))| < 1e-13 rad; exit direction = incident; TIR beyond the critical angle | 11 cases, worst angle error 2.7e-15 rad, 2 total-reflection cases correct | **PASS** |
| T3 | single linear element (4 designs, 25 keV) | EFL_trace vs matrix < 1e-6, vs R/(2 delta) < 1e-4; orthogonal axis: slope change exactly 0, no focus | objective1: f=10.4167 m (A-C -1.2e-11, A-thin 2.4e-07); objective2: f=6.9444 m (A-C -1.2e-11, A-thin 3.6e-07); condenser1: f=103.0093 m (A-C 1.1e-10, A-thin 2.4e-08); condenser2: f=69.4444 m (A-C 2.0e-10, A-thin 3.6e-08) | **PASS** |
| T4 | orthogonal pair X+Y (4 designs, 25 keV) | focus in both axes; trace vs matrix (EFL and focus position) < 1e-6; astigmatism z_x - z_y equal in both models | objective1: z_x - z_y = -0.2631 mm (matrix -0.2631); objective2: z_x - z_y = -0.1884 mm (matrix -0.1884); condenser1: z_x - z_y = -2.3310 mm (matrix -2.3310); condenser2: z_x - z_y = -1.5647 mm (matrix -1.5647) | **PASS** |
| T5 | full sets: 4 designs of Table 2 + objective 2 variants N=145, N=180 (25 keV) | all paraxial rays valid with 4N refractions; trace vs matrix: focus position / EFL < 1e-5 (relative to EFL); all 21x21 grid rays (90 % aperture) transmitted | objective1: BFL_x 38.08 mm (matrix 38.08), EFL_x 83.304 mm; objective2: BFL_x -0.749 mm (matrix -0.749), EFL_x 51.163 mm; objective2_N145: BFL_x 27.89 mm (matrix 27.89), EFL_x 58.462 mm; objective2_N180: BFL_x 12.61 mm (matrix 12.61), EFL_x 52.78 mm; condenser1: BFL_x 4871 mm (matrix 4871), EFL_x 4921.5 mm; condenser2: BFL_x 2207 mm (matrix 2207), EFL_x 2256.4 mm | **PASS** |
| T6 | dependence on N (objective 1 geometry, 25 keV, N = 1..250 per axis) | trace = matrix (< 1e-5); EFL = R/(2N delta) within 1 % while L/f < 0.02; EFL within 2 % of the continuous thick-lens model for all N | EFL/f_thin: N=1: 1.0000, N=2: 1.0000, N=5: 1.0002, N=10: 1.0008, N=20: 1.0034, N=50: 1.0214, N=100: 1.0894, N=154: 1.2316, N=250: 1.8151 | **PASS** |
| T7 | dependence on R (single element, p = 235 um, d = 10 um) | EFL/R = 1/(2 delta) within 1e-4; trace = matrix < 1e-6 | R=3: +7.2e-07, R=6: +3.6e-07, R=9: +2.4e-07, R=20: +1.1e-07, R=60: +3.6e-08, R=89: +2.4e-08, R=200: +1.1e-08 | **PASS** |
| T8 | dependence on delta (1e-8..1e-4) and on energy (15..35 keV, delta ~ E^-2) | EFL*delta = R/2 within 1e-3 (thickness effect grows with delta); trace = matrix < 1e-6; f/E^2 constant within 1e-4 | d=1e-08: +5.6e-09, d=1e-07: +5.6e-08, d=4.32e-07: +2.4e-07, d=1e-06: +5.6e-07, d=1e-05: +5.6e-06, d=0.0001: +5.6e-05; f/E^2 spread 5.4e-07 | **PASS** |
| T9 | surface representation: analytic solid vs G4ExtrudedSolid with n = 8..1024 chords | deflection error decreases monotonically with n with order >= 0.8 (chord slope error ~ 1/n); analytic solid used as reference | mean rel. deflection error n=8: 3.68e-01 -> n=1024: 1.40e-03, order 1.12 | **PASS** |
| T10 | comparison with the thesis Table 2 (20/25/30 keV; objective 2 with N=145/180 variants) | table FL reproduced by thin-lens R/(2N delta(E)) within 1.5 % (objective 1 @30 keV reported separately); traced EFL = matrix < 1e-5. Traced focus positions are NOT required to equal the table: the table is a thin-lens value | 12 table entries; worst table-vs-thin deviation 1.14%; objective 1 @30 keV: -6.57% | **PASS** |
| T11 | navigation robustness and geometry overlap checking | 3000 random rays over 99.9 % of the aperture: all transmitted with exactly 616 refractions, no G4Exception; G4 overlap check: 0 overlaps | valid 3000/3000, refractions 616..616, G4Exceptions 0; overlap checks: objective1 310 volumes checked, 0 overlaps, condenser1 44 volumes checked, 0 overlaps, objective2 432 volumes checked, 0 overlaps | **PASS** |
| T12 | Geant4 transport to screens vs analytic straight-line extrapolation from the exit plane | max deviation < 1e-6 um (validates extrapolation used for long-focus systems) | 484 screen hits, max deviation 2.1e-14 um | **PASS** |
| T13 | reproducibility (same seed, two runs) | identical ray files | identical | **PASS** |
| T14 | macro control: /xrcl/refraction/enable false and /xrcl/source/energy 20 keV | no deflection with refraction off; delta rescaled to (25/20)^2 * 4.32e-7 | refraction off: no deflection; 20 keV delta = 6.7500e-07 | **PASS** |
| T15 | method B: closed-form RMS minimum vs sampled RMS(z) scan (objective 1, 21x21 grid) | |z_closed - z_scan| <= scan step | x: closed form 119.0770 mm, scan 119.1292 mm (step 0.125); y: closed form 119.3400 mm, scan 119.3784 mm (step 0.125) | **PASS** |

## Full lens sets at 25 keV (delta = 4.32e-7)

| Lens | elements | L built, mm | gap, um | f_thin, mm | EFL trace, mm | EFL matrix, mm | focus from lens end, trace, mm | matrix, mm | continuous model, mm | RMS-min focus (21x21 grid), mm | RMS min, um |
|---|---|---|---|---|---|---|---|---|---|---|---|
| objective1 | 308 | 81 | 28.0782 | 67.6407 | 83.303838 | 83.303838 | 38.0767649 | 38.0767649 | 38.2059 | 38.077 | 1.81484e-05 |
| objective2 | 430 | 81 | 28.4382 | 32.2997 | 51.1625904 | 51.1625904 | -0.748973665 | -0.748973664 | -0.65447 | -0.748871 | 1.07524e-05 |
| objective2_N145 | 290 | 55 | 29.7578 | 47.8927 | 58.4621699 | 58.4621699 | 27.8902608 | 27.8902608 | 27.9823 | 27.8904 | 1.30141e-05 |
| objective2_N180 | 360 | 68 | 28.9694 | 38.5802 | 52.7795842 | 52.7795842 | 12.6142804 | 12.6142804 | 12.7072 | 12.6144 | 1.11678e-05 |
| condenser1 | 42 | 97.902 | 0 | 4905.2 | 4921.52095 | 4921.52095 | 4871.32306 | 4871.32306 | 4872.53 | 4871.35 | 6.20956e-04 |
| condenser2 | 62 | 97 | 9.67213 | 2240.14 | 2256.3769 | 2256.3769 | 2206.91895 | 2206.91895 | 2207.72 | 2206.93 | 3.94541e-04 |

## Comparison with Table 2 of the master thesis

| Lens | E, keV | N | delta | table FL, cm | R/(2N delta), cm | table vs thin | traced EFL, cm | traced focus from lens end, cm | from lens centre, cm | thesis (1.7.3) from centre, cm |
|---|---|---|---|---|---|---|---|---|---|---|
| objective1 | 20 | 154 | 6.7500e-07 | 4.3 | 4.3290 | -0.67% | 6.0463 | 1.2053 | 5.2553 | 5.6790 |
| objective1 | 25 | 154 | 4.3200e-07 | 6.8 | 6.7641 | +0.53% | 8.3304 | 3.8077 | 7.8577 | 8.1141 |
| objective1 | 30 | 154 | 3.0000e-07 | 9.1 | 9.7403 | -6.57% (!) | 11.2343 | 6.8648 | 10.9148 | 11.0903 |
| objective2 | 30 | 215 | 3.0000e-07 | 4.7 | 4.6512 | +1.05% | 6.3374 | 1.5655 | 5.6155 | 6.0012 |
| objective2_N145 | 20 | 145 | 6.7500e-07 | 3.1 | 3.0651 | +1.14% | 4.2189 | 0.9575 | 3.7075 | 3.9818 |
| objective2_N180 | 25 | 180 | 4.3200e-07 | 3.9 | 3.8580 | +1.09% | 5.2780 | 1.2614 | 4.6614 | 4.9914 |
| condenser1 | 20 | 21 | 6.7500e-07 | 314 | 313.9330 | +0.02% | 315.5669 | 310.5426 | 315.4377 | 315.5647 |
| condenser1 | 25 | 21 | 4.3200e-07 | 490 | 490.5203 | -0.11% | 492.1521 | 487.1323 | 492.0274 | 492.1520 |
| condenser1 | 30 | 21 | 3.0000e-07 | 706 | 706.3492 | -0.05% | 707.9799 | 702.9626 | 707.8577 | 707.9809 |
| condenser2 | 20 | 31 | 6.7500e-07 | 143 | 143.3692 | -0.26% | 144.9972 | 140.0414 | 144.8914 | 144.9858 |
| condenser2 | 25 | 31 | 4.3200e-07 | 224 | 224.0143 | -0.01% | 225.6377 | 220.6919 | 225.5419 | 225.6310 |
| condenser2 | 30 | 31 | 3.0000e-07 | 323 | 322.5806 | +0.13% | 324.2015 | 319.2611 | 324.1111 | 324.1973 |
