#!/usr/bin/env bash
# Full pipeline (run inside the container or any machine with Geant4 11.x, CMake, Python3+numpy+matplotlib):
#   configure + build, unit tests, validation suite, trace runs for figures, figures.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
cd "$ROOT"

echo "=== configure & build ($(geant4-config --version 2>/dev/null || echo 'Geant4 ?'))"
EXTRA=()
# conda toolchains ship only prefixed binutils: use gcc-ar when plain ar is missing
if ! command -v ar >/dev/null 2>&1 && command -v gcc-ar >/dev/null 2>&1; then
  EXTRA+=(-DCMAKE_AR="$(command -v gcc-ar)" -DCMAKE_RANLIB="$(command -v gcc-ranlib)")
fi
GEN=(); command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$ROOT" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=Release "${EXTRA[@]}"
cmake --build "$BUILD" -j "$(nproc 2>/dev/null || echo 4)"

echo "=== unit tests"
"$BUILD/unit_tests" | tee "$BUILD/unit_tests.log"

echo "=== validation suite"
set +e
python3 "$ROOT/tests/run_validation.py" --app "$BUILD/xraycrl" --root "$ROOT" --out "$ROOT/results/validation" ${ONLY:+--only "$ONLY"}
VAL=$?
set -e

echo "=== ray traces for figures"
"$BUILD/xraycrl" --config config/runs/objective1_fan_trace.json > results/objective1_fan_trace.log
"$BUILD/xraycrl" --config config/runs/objective1_fan_trace.json --set 'source.mode="fan_y"' \
  --set 'name="objective1_fan_trace_y"' --set 'output.directory="results/objective1_fan_trace_y"' \
  --set 'output.prefix="objective1_fan_trace_y"' > results/objective1_fan_trace_y.log
"$BUILD/xraycrl" --config config/runs/condenser2_full_grid.json --set 'source.mode="fan_x"' --set 'source.fan_n=11' \
  --set 'output.trace_events=11' --set 'name="condenser2_fan_trace"' \
  --set 'output.directory="results/condenser2_fan_trace"' --set 'output.prefix="condenser2_fan_trace"' > results/condenser2_fan_trace.log

echo "=== figures"
python3 "$ROOT/analysis/plot_results.py" --validation "$ROOT/results/validation" --traces "$ROOT/results" --out "$ROOT/results/figures"
exit $VAL
