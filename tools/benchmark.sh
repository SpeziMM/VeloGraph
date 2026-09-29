#!/usr/bin/env bash
# VeloGraph performance benchmark.
#
# Measures cold parse+build, cached startup, and route-search throughput so a
# run on a new machine can be compared against the recorded baseline.
#
# Usage:
#   tools/benchmark.sh [PBF_FILE] [START_NODE]
#
# Defaults match the reference baseline (Karlsruhe regbez, node 281756094).

set -euo pipefail
cd "$(dirname "$0")/.."

PBF="${1:-data/karlsruhe-regbez-251117.osm.pbf}"
START="${2:-281756094}"
BIN="./build/VeloGraph"
COMMON=(--start_node "$START" --target_distance 5000 --profile scenic)

if [[ ! -x "$BIN" ]]; then
  echo "error: $BIN not found. Build first: cd build && cmake .. && make -j" >&2
  exit 1
fi
if [[ ! -f "$PBF" ]]; then
  echo "error: PBF file not found: $PBF" >&2
  exit 1
fi

echo "=== VeloGraph benchmark ==="
echo "host:  $(uname -mns)"
if command -v sysctl >/dev/null 2>&1; then
  echo "cpu:   $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo '?')  cores: $(sysctl -n hw.ncpu 2>/dev/null || echo '?')"
elif [[ -r /proc/cpuinfo ]]; then
  echo "cpu:   $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs)  cores: $(nproc)"
fi
echo "pbf:   $PBF"
echo

elapsed() { local s=$1 e=$2; awk "BEGIN{printf \"%.2f\", $e-$s}"; }

# Cold parse: remove cache to force a full reparse + build.
rm -f output/graph_cache_*.bin 2>/dev/null || true
echo "[1/3] Cold parse + build (cache cleared)..."
"$BIN" "$PBF" "${COMMON[@]}" --iterations 1 2>/dev/null | \
  grep -E "Parsed in|Graph built|Total time" | sed 's/^/      /'

# Cached startup + small route run.
echo "[2/3] Cached startup + 1 route iteration..."
s=$(date +%s.%N); "$BIN" "$PBF" "${COMMON[@]}" --iterations 1 >/dev/null 2>&1; e=$(date +%s.%N)
T1=$(elapsed "$s" "$e")
echo "      wall: ${T1}s"

# Route-search throughput: amortize startup over many iterations.
echo "[3/3] Route search throughput (2000 iterations)..."
s=$(date +%s.%N); "$BIN" "$PBF" "${COMMON[@]}" --iterations 2000 >/dev/null 2>&1; e=$(date +%s.%N)
T2k=$(elapsed "$s" "$e")
PER=$(awk "BEGIN{printf \"%.3f\", ($T2k-$T1)/1999*1000}")
echo "      wall: ${T2k}s  =>  ${PER} ms/iteration"

echo
echo "Compare against the M5 Pro baseline: parse ~2.2s, build ~4.3s,"
echo "cached startup ~2.2s, ~0.28 ms/iteration."
