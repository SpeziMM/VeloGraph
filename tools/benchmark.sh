#!/usr/bin/env bash
# VeloGraph hardware benchmark.
#
# Runs a FIXED workload (same algorithm, same params, same dataset) so the wall
# times below reflect the machine, not the problem. Good for answering "how much
# faster is box B than box A on this engine?".
#
# What it measures (median of several repeats, after warmup):
#   [A] Cold parse + build  - read PBF, parse, build graph, write cache, KD-tree
#   [B] Cached startup       - load graph cache + build spatial index
#   [C] Route-search rate    - steady-state ms per search iteration (startup amortized)
#
# Quick start (from repo root or anywhere):
#   tools/benchmark.sh                 # uses the default dataset, builds if needed
#   tools/benchmark.sh --download      # also fetch the dataset if it is missing
#   tools/benchmark.sh --repeats 5 --json results.jsonl
#
# Flags:
#   --pbf FILE          dataset to benchmark (default: Karlsruhe regbez)
#   --start LAT LON     start location, resolved to nearest node (portable across datasets)
#   --iterations N      search iterations for the throughput phase (default 2000)
#   --repeats N         measured repeats per phase, median is reported (default 3)
#   --warmup N          discarded warmup runs before timed phases (default 1)
#   --download          download the default dataset from Geofabrik if absent
#   --json FILE         append one machine-readable JSON line of results to FILE
#   -h, --help          this help

set -euo pipefail
cd "$(dirname "$0")/.."

# --- defaults ----------------------------------------------------------------
PBF="data/karlsruhe-regbez-251117.osm.pbf"
GEOFABRIK_URL="https://download.geofabrik.de/europe/germany/baden-wuerttemberg/karlsruhe-regbez-latest.osm.pbf"
START_LAT="49.0094"; START_LON="8.4044"   # central Karlsruhe
TARGET=5000; PROFILE="scenic"
ITERS=2000; REPEATS=3; WARMUP=1
DOWNLOAD=0; JSON=""
BIN="./build/VeloGraph"

# --- arg parsing -------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    --pbf) PBF="$2"; shift 2;;
    --start) START_LAT="$2"; START_LON="$3"; shift 3;;
    --iterations) ITERS="$2"; shift 2;;
    --repeats) REPEATS="$2"; shift 2;;
    --warmup) WARMUP="$2"; shift 2;;
    --download) DOWNLOAD=1; shift;;
    --json) JSON="$2"; shift 2;;
    -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
    *) echo "unknown flag: $1" >&2; exit 2;;
  esac
done

command -v python3 >/dev/null || { echo "error: python3 required for timing" >&2; exit 1; }

# --- build if needed ---------------------------------------------------------
if [[ ! -x "$BIN" ]]; then
  echo "[setup] $BIN not found, building (-O3 -march=native release)..."
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >/dev/null
fi

# --- dataset -----------------------------------------------------------------
if [[ ! -f "$PBF" ]]; then
  if [[ "$DOWNLOAD" == "1" && "$PBF" == "data/karlsruhe-regbez-251117.osm.pbf" ]]; then
    echo "[setup] downloading dataset from Geofabrik (~140 MB)..."
    mkdir -p data
    curl -fL --progress-bar "$GEOFABRIK_URL" -o "$PBF"
    echo "[setup] note: Geofabrik 'latest' may differ slightly from the recorded baseline."
  else
    echo "error: dataset not found: $PBF" >&2
    echo "       pass --download to fetch it, or --pbf <file> to use your own." >&2
    exit 1
  fi
fi

# A tiny dataset gives noise, not a hardware signal. Warn but proceed.
PBF_BYTES=$(wc -c < "$PBF" | tr -d ' ')
if [[ "$PBF_BYTES" -lt 5000000 ]]; then
  echo "[warn] '$PBF' is small (${PBF_BYTES}B); results will be noisy and not comparable to the baseline." >&2
fi

# Short dataset fingerprint so results are only compared against the same data.
if command -v md5 >/dev/null 2>&1; then PBF_HASH=$(md5 -q "$PBF" | cut -c1-12)
elif command -v md5sum >/dev/null 2>&1; then PBF_HASH=$(md5sum "$PBF" | cut -c1-12)
else PBF_HASH="?"; fi

# --- machine info ------------------------------------------------------------
OS="$(uname -srm)"
if [[ "$(uname -s)" == "Darwin" ]]; then
  CPU="$(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo '?')"
  CORES="$(sysctl -n hw.ncpu 2>/dev/null || echo '?')"
  MEM_GB="$(awk "BEGIN{printf \"%.0f\", $(sysctl -n hw.memsize)/1073741824}")"
  POWER="$(pmset -g batt 2>/dev/null | grep -qi 'AC Power' && echo 'AC' || echo 'BATTERY')"
else
  CPU="$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs || echo '?')"
  CORES="$(nproc 2>/dev/null || echo '?')"
  MEM_GB="$(awk '/MemTotal/{printf "%.0f", $2/1048576}' /proc/meminfo 2>/dev/null || echo '?')"
  POWER="n/a"
fi
GIT="$(git rev-parse --short HEAD 2>/dev/null || echo '?')"
COMPILER="$(grep -m1 CMAKE_CXX_COMPILER:FILEPATH build/CMakeCache.txt 2>/dev/null | cut -d= -f2 || echo '?')"

echo "=============================================================="
echo " VeloGraph hardware benchmark"
echo "=============================================================="
echo " cpu      : $CPU ($CORES logical cores)"
echo " memory   : ${MEM_GB} GB    power: $POWER"
echo " os       : $OS"
echo " compiler : ${COMPILER##*/}    build: -O3 -march=native"
echo " git      : $GIT"
echo " dataset  : $PBF  (${PBF_BYTES}B, md5:$PBF_HASH)"
echo " workload : start=$START_LAT,$START_LON  target=${TARGET}m  profile=$PROFILE"
echo " sampling : $WARMUP warmup + $REPEATS repeats per phase, $ITERS iters for throughput"
echo "--------------------------------------------------------------"
[[ "$POWER" == "BATTERY" ]] && echo " [warn] on battery: CPU may be throttled, numbers not comparable."

COMMON=("$PBF" --start "$START_LAT" "$START_LON" --target_distance "$TARGET" --profile "$PROFILE")

# --- helpers -----------------------------------------------------------------
# Run a command silently, print only the child process wall time in seconds.
timer() {
  python3 - "$@" <<'PY'
import subprocess, sys, time
t = time.perf_counter()
subprocess.run(sys.argv[1:], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
print(f"{time.perf_counter()-t:.4f}")
PY
}
# Print "median min" of the numbers passed as args.
stats() { printf '%s\n' "$@" | sort -n | awk '
  {a[NR]=$1}
  END{ n=NR; m=(n%2)?a[(n+1)/2]:(a[n/2]+a[n/2+1])/2;
       printf "%.4f %.4f\n", m, a[1] }'; }

# Warmup then REPEATS timed runs at a given iteration count; echoes "median min".
run_repeats() {
  local iters="$1"; local i; local samples=()
  for ((i=0; i<WARMUP; i++)); do timer "$BIN" "${COMMON[@]}" --iterations "$iters" >/dev/null; done
  for ((i=0; i<REPEATS; i++)); do samples+=("$(timer "$BIN" "${COMMON[@]}" --iterations "$iters")"); done
  stats "${samples[@]}"
}

# --- [A] cold parse + build --------------------------------------------------
echo "[A] cold parse + build (cache cleared each run)..."
cold_samples=()
for ((i=0; i<REPEATS; i++)); do
  rm -f output/graph_cache_*.bin 2>/dev/null || true
  cold_samples+=("$(timer "$BIN" "${COMMON[@]}" --iterations 1)")
done
read -r COLD_MED COLD_MIN < <(stats "${cold_samples[@]}")
printf '    median %ss   min %ss\n' "$COLD_MED" "$COLD_MIN"

# --- [B] cached startup ------------------------------------------------------
echo "[B] cached startup (graph cache + spatial index)..."
read -r WARM_MED WARM_MIN < <(run_repeats 1)
printf '    median %ss   min %ss\n' "$WARM_MED" "$WARM_MIN"

# --- [C] route-search throughput ---------------------------------------------
echo "[C] route-search throughput ($ITERS iterations)..."
read -r RUN_MED RUN_MIN < <(run_repeats "$ITERS")
PER_ITER_MS=$(awk "BEGIN{printf \"%.4f\", ($RUN_MED-$WARM_MED)/($ITERS-1)*1000}")
printf '    median %ss total  =>  %s ms/iteration\n' "$RUN_MED" "$PER_ITER_MS"

# --- summary -----------------------------------------------------------------
echo "--------------------------------------------------------------"
printf ' RESULT  cold=%ss  cached=%ss  per_iter=%sms\n' "$COLD_MED" "$WARM_MED" "$PER_ITER_MS"
echo " baseline (Apple M5 Pro): cold~6.5s  cached~2.2s  per_iter~0.28ms"
echo "=============================================================="

# --- optional machine-readable line ------------------------------------------
if [[ -n "$JSON" ]]; then
  printf '{"ts":"%s","cpu":"%s","cores":%s,"mem_gb":%s,"os":"%s","git":"%s","pbf_md5":"%s","cold_s":%s,"cached_s":%s,"per_iter_ms":%s}\n' \
    "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$CPU" "$CORES" "$MEM_GB" "$OS" "$GIT" "$PBF_HASH" \
    "$COLD_MED" "$WARM_MED" "$PER_ITER_MS" >> "$JSON"
  echo "appended result to $JSON"
fi
