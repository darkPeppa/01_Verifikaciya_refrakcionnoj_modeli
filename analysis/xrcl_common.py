"""Shared helpers: run the Geant4 application with configuration overrides and read its output."""
import csv
import json
import math
import os
import subprocess
import time


class RunError(RuntimeError):
    pass


def run_app(app, root, base_config, name, outdir, overrides=None, macro=None, timeout=3600):
    """Run xraycrl with --config <base_config> and --set overrides; return (summary, run_dir, log_text)."""
    run_dir = os.path.join(outdir, name)
    os.makedirs(run_dir, exist_ok=True)
    cmd = [app, "--config", os.path.join(root, base_config),
           "--set", f"name=\"{name}\"",
           "--set", f"output.directory=\"{run_dir}\"",
           "--set", f"output.prefix=\"{name}\""]
    for k, v in (overrides or {}).items():
        cmd += ["--set", f"{k}={json.dumps(v)}"]
    if macro:
        cmd += ["--macro", macro]
    t0 = time.time()
    proc = subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=timeout)
    log = proc.stdout + proc.stderr
    with open(os.path.join(run_dir, "log.txt"), "w") as f:
        f.write(" ".join(cmd) + "\n\n" + log)
    if proc.returncode != 0:
        raise RunError(f"{name}: exit code {proc.returncode}\n{log[-3000:]}")
    with open(os.path.join(run_dir, f"{name}_summary.json")) as f:
        summary = json.load(f)
    summary["_elapsed_s"] = time.time() - t0
    return summary, run_dir, log


def read_csv(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(line for line in f if not line.startswith("#")):
            row = {}
            for k, v in r.items():
                try:
                    row[k] = float(v)
                except (TypeError, ValueError):
                    row[k] = v
            rows.append(row)
    return rows


def rel(a, b):
    if a is None or b is None or b == 0:
        return math.nan
    return (a - b) / b


def fnum(v, nd=6):
    if v is None:
        return "—"
    if isinstance(v, float) and not math.isfinite(v):
        return "nan"
    if isinstance(v, (int,)) and not isinstance(v, bool):
        return str(v)
    if isinstance(v, float):
        if v != 0 and (abs(v) < 1e-3 or abs(v) >= 1e6):
            return f"{v:.{nd - 1}e}"
        return f"{v:.{nd}g}"
    return str(v)
