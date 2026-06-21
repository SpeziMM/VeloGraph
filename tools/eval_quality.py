#!/usr/bin/env python3
"""VeloGraph route-quality eval harness.

Runs the engine over a set of start nodes at a fixed seed and aggregates quality
+ speed metrics, so before/after changes (parallelization, scoring) are measured
rather than guessed.

Usage:
    # 1) sample start nodes once from a graph cache
    ./build/inspect_graph output/graph_cache_<pbf>_simp.bin --sample 30 > nodes.txt

    # 2) evaluate
    python3 tools/eval_quality.py --pbf data/foo.osm.pbf --nodes-file nodes.txt \\
        --seed 777 --distance 5000 --iterations 100 --profile scenic --out runs/a.csv

    # 3) compare two runs
    python3 tools/eval_quality.py --baseline runs/a.csv --compare runs/b.csv
"""
import argparse
import csv
import json
import os
import statistics
import subprocess
import sys
import tempfile

FIELDS = [
    "start_node", "success", "wall_time_ms", "total_distance_m",
    "distance_error_m", "fitness_score", "safety_score", "scenery_score",
    "quality_score", "traffic_penalty", "turn_penalty",
    "gradient_penalty", "total_ascent_m", "node_count",
]


def load_nodes(args):
    if args.nodes:
        return [int(x) for x in args.nodes.split(",") if x.strip()]
    if args.nodes_file:
        with open(args.nodes_file) as f:
            return [int(line) for line in f if line.strip() and not line.startswith("#")]
    sys.exit("Provide --nodes or --nodes-file (sample via inspect_graph --sample N).")


def run_one(binary, pbf, node, args, out_json):
    cmd = [
        binary, pbf,
        "--start_node", str(node),
        "--target_distance", str(args.distance),
        "--profile", args.profile,
        "--iterations", str(args.iterations),
        "--seed", str(args.seed),
        "--output_path", out_json,
    ]
    if args.elevation:
        cmd += ["--elevation", args.elevation]
    if args.weight_gradient is not None:
        cmd += ["--weight_gradient", str(args.weight_gradient)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 and not os.path.exists(out_json):
        return {"start_node": node, "success": False}
    try:
        with open(out_json) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError):
        return {"start_node": node, "success": False}
    run = data.get("run", {})
    stats = data.get("stats", {})
    return {
        "start_node": node,
        "success": run.get("success", False),
        "wall_time_ms": run.get("wall_time_ms", 0.0),
        "total_distance_m": stats.get("total_distance_m", 0.0),
        "distance_error_m": run.get("distance_error_m", 0.0),
        "fitness_score": stats.get("fitness_score", 0.0),
        "safety_score": stats.get("safety_score", 0.0),
        "scenery_score": stats.get("scenery_score", 0.0),
        "quality_score": stats.get("quality_score", 0.0),
        "traffic_penalty": stats.get("traffic_penalty", 0.0),
        "turn_penalty": stats.get("turn_penalty", 0.0),
        "gradient_penalty": stats.get("gradient_penalty", 0.0),
        "total_ascent_m": stats.get("total_ascent_m", 0.0),
        "node_count": run.get("node_count", 0),
    }


def pct(values, p):
    if not values:
        return 0.0
    s = sorted(values)
    k = max(0, min(len(s) - 1, int(round((p / 100.0) * (len(s) - 1)))))
    return s[k]


def summarize(rows):
    n = len(rows)
    ok = [r for r in rows if r.get("success")]
    fail_rate = 100.0 * (n - len(ok)) / n if n else 0.0
    wt = [r["wall_time_ms"] for r in rows if "wall_time_ms" in r]
    derr = [r["distance_error_m"] for r in ok]
    fit = [r["fitness_score"] for r in ok]
    asc = [r.get("total_ascent_m", 0.0) for r in ok]
    return {
        "n": n,
        "fail_rate_pct": fail_rate,
        "mean_wall_ms": statistics.mean(wt) if wt else 0.0,
        "mean_dist_err_m": statistics.mean(derr) if derr else 0.0,
        "median_dist_err_m": statistics.median(derr) if derr else 0.0,
        "p90_dist_err_m": pct(derr, 90),
        "mean_fitness": statistics.mean(fit) if fit else 0.0,
        "mean_ascent_m": statistics.mean(asc) if asc else 0.0,
    }


def print_summary(s, label=""):
    print(f"\n=== Summary {label} ===")
    print(f"  nodes evaluated : {s['n']}")
    print(f"  fail-rate       : {s['fail_rate_pct']:.1f}%")
    print(f"  mean wall time  : {s['mean_wall_ms']:.1f} ms")
    print(f"  dist error      : mean {s['mean_dist_err_m']:.0f}m  "
          f"median {s['median_dist_err_m']:.0f}m  p90 {s['p90_dist_err_m']:.0f}m")
    print(f"  mean fitness    : {s['mean_fitness']:.4f}")
    print(f"  mean ascent     : {s['mean_ascent_m']:.0f} m")


def write_csv(rows, path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in FIELDS})


def read_csv(path):
    with open(path) as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for k in FIELDS:
            if k in ("start_node", "node_count"):
                r[k] = int(float(r[k])) if r.get(k) not in ("", None) else 0
            elif k == "success":
                r[k] = str(r[k]).lower() in ("true", "1")
            else:
                r[k] = float(r[k]) if r.get(k) not in ("", None) else 0.0
    return rows


def compare(baseline_csv, compare_csv):
    a = summarize(read_csv(baseline_csv))
    b = summarize(read_csv(compare_csv))
    print_summary(a, f"baseline ({baseline_csv})")
    print_summary(b, f"compare  ({compare_csv})")

    def delta(key, lower_better=True, unit=""):
        d = b[key] - a[key]
        arrow = "improved" if (d < 0) == lower_better else "worse"
        if abs(d) < 1e-9:
            arrow = "unchanged"
        speedup = ""
        if key == "mean_wall_ms" and b[key] > 0:
            speedup = f"  ({a[key] / b[key]:.2f}x)"
        print(f"  {key:18s}: {a[key]:.2f} -> {b[key]:.2f}{unit}  ({d:+.2f}) {arrow}{speedup}")

    print("\n=== Delta (compare vs baseline) ===")
    delta("fail_rate_pct")
    delta("mean_wall_ms")
    delta("mean_dist_err_m")
    delta("p90_dist_err_m")
    delta("mean_fitness", lower_better=False)
    delta("mean_ascent_m")


def main():
    ap = argparse.ArgumentParser(description="VeloGraph route-quality eval harness")
    ap.add_argument("--pbf", help="OSM PBF file")
    ap.add_argument("--binary", default="./build/VeloGraph")
    ap.add_argument("--nodes", help="comma-separated start node IDs")
    ap.add_argument("--nodes-file", help="file with one start node ID per line")
    ap.add_argument("--seed", type=int, default=777)
    ap.add_argument("--distance", type=float, default=5000)
    ap.add_argument("--iterations", type=int, default=100)
    ap.add_argument("--profile", default="scenic")
    ap.add_argument("--elevation", help="elevation sidecar (tools/build_elevation.py)")
    ap.add_argument("--weight_gradient", type=float, default=None,
                    help="hill-avoidance weight [0-1] passed to the engine")
    ap.add_argument("--out", help="write per-node results to this CSV")
    ap.add_argument("--baseline", help="baseline CSV (with --compare, diff two runs)")
    ap.add_argument("--compare", help="compare CSV (with --baseline)")
    args = ap.parse_args()

    if args.baseline and args.compare:
        compare(args.baseline, args.compare)
        return
    if not args.pbf:
        sys.exit("--pbf required (or use --baseline/--compare to diff existing CSVs).")

    nodes = load_nodes(args)
    print(f"Evaluating {len(nodes)} start nodes "
          f"(seed={args.seed}, dist={args.distance}, iters={args.iterations}, "
          f"profile={args.profile})...")

    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        for i, node in enumerate(nodes, 1):
            out_json = os.path.join(tmp, f"route_{node}.json")
            row = run_one(args.binary, args.pbf, node, args, out_json)
            rows.append(row)
            ok = "ok " if row.get("success") else "FAIL"
            print(f"  [{i}/{len(nodes)}] node {node}: {ok} "
                  f"{row.get('wall_time_ms', 0):.0f}ms "
                  f"err={row.get('distance_error_m', 0):.0f}m "
                  f"fit={row.get('fitness_score', 0):.3f}")

    s = summarize(rows)
    print_summary(s)
    if args.out:
        write_csv(rows, args.out)
        print(f"\nPer-node CSV written to {args.out}")


if __name__ == "__main__":
    main()
