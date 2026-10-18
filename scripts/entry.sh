#!/usr/bin/env bash
# Command dispatcher inside the container. Everything is written to $OUT (/work/out);
# the host scripts (run_windows.bat, run.sh) copy $OUT to the host folder "output".
#
#   all        stage-1 validation + figures, stage-2 data, poster        (~30-40 min)
#   quick      short check of the whole chain with few photons           (~3-5 min)
#   validate   stage 1: refraction-model validation (16 checks) + figures (~5 min)
#   stage2     stage 2: interaction physics data -> data/                 (~25-30 min)
#   poster     poster from the computed results (needs validate + stage2 output)
#   calc ARGS  one run of the program, e.g. calc --config config/runs/objective1_full_paraxial.json
#   shell      interactive shell in the container
set -uo pipefail
cd /work
OUT="${OUT:-/work/out}"
APP=/work/build/xraycrl
mkdir -p "$OUT/results" "$OUT/data" "$OUT/poster"
# relative output paths of the configurations (results/...) end up in $OUT
ln -sfn "$OUT/results" /work/results

log() { echo; echo "=================== $* ==================="; }

validate() {
  log "STAGE 1: validation of the refraction model"
  python3 tests/run_validation.py --app "$APP" --root /work --out "$OUT/results/validation" ${ONLY:+--only "$ONLY"}
  local rc=$?
  log "STAGE 1: ray traces for figures"
  "$APP" --config config/runs/objective1_fan_trace.json > "$OUT/results/objective1_fan_trace.log" 2>&1
  "$APP" --config config/runs/objective1_fan_trace.json --set 'source.mode="fan_y"' --set 'name="objective1_fan_trace_y"' \
    --set 'output.directory="results/objective1_fan_trace_y"' --set 'output.prefix="objective1_fan_trace_y"' \
    > "$OUT/results/objective1_fan_trace_y.log" 2>&1
  "$APP" --config config/runs/condenser2_full_grid.json --set 'source.mode="fan_x"' --set 'source.fan_n=11' \
    --set 'output.trace_events=11' --set 'name="condenser2_fan_trace"' \
    --set 'output.directory="results/condenser2_fan_trace"' --set 'output.prefix="condenser2_fan_trace"' \
    > "$OUT/results/condenser2_fan_trace.log" 2>&1
  python3 analysis/plot_results.py --validation "$OUT/results/validation" --traces "$OUT/results" --out "$OUT/results/figures"
  return $rc
}

stage2() {
  log "STAGE 2: interaction physics in the lenses (Livermore)"
  python3 analysis/stage2_collect.py --app "$APP" --root /work --raw "$OUT/raw" --data "$OUT/data" "$@"
  local rc=$?
  cp docs/DATA_README.md "$OUT/data/README.md"
  [ "${KEEP_RAW:-0}" = "1" ] || rm -rf "$OUT/raw"
  return $rc
}

catalog() {
  log "FIGURES: numbered, captioned set in data/figures"
  python3 analysis/figure_catalog.py --stage1 "$OUT/results/figures" --stage2 "$OUT/data/figures_src" --out "$OUT/data/figures"
}

poster() {
  log "POSTER"
  python3 poster/make_poster.py --results "$OUT/results" --data "$OUT/data" --out "$OUT/poster" --assets "$OUT/poster/assets"
  if python3 -c "import pptx" 2>/dev/null; then
    python3 poster/make_poster_pptx.py --results "$OUT/results" --data "$OUT/data" --assets "$OUT/poster/assets" --out "$OUT/poster/poster_A1.pptx"
  else
    echo "python-pptx not installed: editable poster_A1.pptx skipped (pip install python-pptx)"
  fi
}

cmd="${1:-all}"
[ $# -gt 0 ] && shift
case "$cmd" in
  all)      validate; v=$?; stage2; s=$?; catalog; poster; p=$?
            log "DONE: validation exit $v, stage 2 exit $s, poster exit $p"; exit $(( v || s || p )) ;;
  quick)    ONLY=T1,T2,T3,T5,T6,T9,T10,T13 validate; v=$?; stage2 --quick; s=$?; catalog; poster; p=$?
            log "QUICK CHECK DONE: validation exit $v, stage 2 exit $s, poster exit $p"; exit $(( v || s || p )) ;;
  validate) validate ;;
  stage2)   stage2 "$@"; s=$?; catalog; exit $s ;;
  figures)  catalog ;;
  poster)   poster ;;
  calc)     "$APP" "$@" ;;
  unit)     /work/build/unit_tests ;;
  shell)    exec bash ;;
  *)        echo "unknown command: $cmd (all | quick | validate | stage2 | poster | calc ... | unit | shell)"; exit 2 ;;
esac
