# Roadmap Progress

Living status for the VeloGraph improvement roadmap. Plan:
`~/.claude/plans/analyse-the-code-zany-sedgewick.md`. Branch: `roadmap/quality`.

Each Epic is implemented → documented here → verified → committed, before the next starts.
Greedy engine stays a permanent fallback (AOP added alongside, never deletes greedy).

## Status overview

| Epic | Scope | Status |
|------|-------|--------|
| **E1** | `--seed` determinism + this doc | ✅ done & verified |
| **E2** | Centralize hyperparameters into `RouteParams` + `--params` loader | ⬜ TODO |
| **E3** | Rich JSON output + `tools/eval_quality.py` harness | ✅ done & verified |
| **E4** | `tools/sweep.py` + `route_map.py` viz upgrades | ⬜ TODO |
| **EP** | Parallelize the iteration search across threads (speed) | ✅ done & verified |
| **E5** | Parse new OSM tags + extend `Edge` + cache version bump | ⬜ TODO |
| **E6** | New scoring components + `running` profile | ⬜ TODO |
| **EE** | Elevation/gradient: DEM sidecar + hill-aware scoring | ✅ done & verified |
| **E7** | AOP penalty-method engine (`--engine aop`); greedy stays default | ⬜ TODO |
| **E8** | VNS local search (opt-in refinement) | ⬜ TODO |
| **E9** | Promote best engine to default iff it wins + stays under runtime cap | ⬜ TODO |

---

## E1 — Deterministic seed ✅

**What changed**
- `include/RouteFinder.hpp` — constructor gains `unsigned int seed = 0` (0 = nondeterministic via
  `random_device`, nonzero = reproducible).
- `src/RouteFinder.cpp` — constructor seeds the single `mt19937 rng` member from `seed` when nonzero.
  That member threads through all randomness (candidate noise, shuffles, and
  `WaypointGenerator::generate`), so one seed fixes the whole search.
- `src/main.cpp` — new `--seed <n>` CLI flag (default 0), passed to the `RouteFinder` constructor;
  added to usage text.

**Why** Reproducibility is the prerequisite for every later A/B comparison ("compare to see impact").
Without a fixed seed, before/after numbers are noise.

**How verified** (start node 281756094, 5000 m, scenic, 100 iters):
- `--seed 777` run twice → **byte-identical** route JSON (`diff -q` clean).
- `--seed 778` → **different** route (4998.9 m vs 4971.5 m), confirming the seed actually drives search.
- Build clean with `cmake .. && make` (`-O3 -march=native`), no warnings.

**Notes / gotchas**
- The engine's per-iteration success rate is low at some start nodes; raise `--iterations` for reliable
  output. This route-reliability weakness is pre-existing (not seed-related) and is exactly what the E3
  harness will quantify (fail-rate metric) and the E7 AOP engine aims to fix.
- Seed `0` is reserved for "random"; use any nonzero value for reproducible runs.

**Remaining for E1:** none.

---

## E3 — Measurement harness ✅

**What changed**
- `src/main.cpp` — `exportPathToJSON` now writes a `"run"` block: `start_node`,
  `target_distance_m`, `iterations`, `seed`, `success`, `wall_time_ms`, `distance_error_m`,
  `node_count`. The route find is wrapped in `std::chrono::steady_clock`. JSON is now emitted
  **even on failure** (`success:false`, empty `nodes`) so the harness can record fail-rate.
- `tools/eval_quality.py` — runs the engine over a set of start nodes at a fixed `--seed`,
  parses the JSON, and aggregates: fail-rate, distance-error (mean/median/p90), mean fitness,
  mean wall-time. Writes a per-node CSV; `--baseline X --compare Y` prints a delta table
  (with speedup ratio on wall-time).
- `tools/inspect_graph.cpp` — added `--sample N [seed]` to emit N reproducibly-sampled,
  well-connected (degree≥3) start-node IDs, one per line, to feed the harness.

**Why** This is the ruler. Phases after this (parallelization, scoring) only count if the
numbers move — and E1's seed determinism makes the before/after comparable.

**How verified** (Karlsruhe regbez, 5000 m, scenic, seed 777):
- Re-running the harness produced **byte-identical route-quality columns** (distance, fitness,
  all sub-scores, node_count); only `wall_time_ms` varies, as expected.
- Compare/delta mode works end-to-end on two CSVs.
- Surfaced a real finding: high fail-rate at random start nodes (the pre-existing
  route-reliability weakness E1 flagged) — exactly what the harness exists to quantify.

**Notes / gotchas**
- Determinism guarantee is on *route output*, not wall-time. A CSV `diff` will differ on the
  timing column by design.
- A curated routable start-node set lives in `tools/eval_nodes_karlsruhe.txt` (successes
  filtered from a 50-node sample) so speed/quality baselines aren't dominated by failures.

**Remaining for E3:** none.

---

## EP — Parallel iteration search ✅

**What changed**
- `include/RouteFinder.hpp` / `src/RouteFinder.cpp` — the `findOptimalCycle` loop now runs the
  independent iterations across a work-stealing thread pool (`std::thread` + `std::atomic`
  index). The shared `mt19937` member is gone; each iteration gets a private RNG seeded from a
  `std::seed_seq{base_seed_, i}`. The per-iteration body is factored into `runIteration(...)`,
  and the RNG is threaded as a parameter through `buildWaypointRoute` / `buildCircularRoute` /
  `findSegmentPath` / `findReturnPath` / `correctDistance` (all now `const`). Results are
  collected per-index and reduced with a strict `>` so the lowest index wins ties → the winner
  is independent of thread scheduling.
- Per-iteration debug logging is gated behind a `verbose_` flag (`vlog()`); it would otherwise
  garble under threads. Top-level banners stay on `std::cout`.
- `src/main.cpp` — new `--threads <n>` flag (0 = auto = `hardware_concurrency`), forwarded via
  `RouteFinder::setThreads`. Used to measure scaling and prove thread-count independence.

**Why** The iterations are embarrassingly parallel and `graph`/`evaluator`/`precompute` are all
read-only during the search — the single shared RNG was the only thing forcing them serial.

**How verified** (Karlsruhe regbez, node 281756094, 5000 m, scenic, seed 777):
- **Determinism upgrade:** route JSON is **byte-identical across `--threads 1/2/4/15`** (and
  across repeated runs) — stronger than E1, which only fixed the sequential order.
- **Speedup** (4000 iterations): 1→956 ms, 2→653 ms, 4→312 ms (3.1×), 15→119 ms (**8.0×**).
  Sub-linear at low thread counts because iteration cost is highly variable (fast failures vs
  full 2-opt); the atomic work-stealing balances it out as threads increase.
- Aggregate route quality (fail-rate / mean fitness / distance-error distribution) over the
  curated 21-node set is statistically unchanged vs the sequential baseline — see
  `runs/baseline_seq.csv` vs `runs/parallel.csv`.

**Notes / gotchas**
- The per-iteration seeding scheme changed (independent streams per `i` instead of one advancing
  stream), so individual per-node routes differ from the old sequential engine; only the
  *aggregate distribution* is comparable, which is what the E3 harness reports.
- `improveWith2Opt` is deterministic (no RNG) and was left unthreaded internally.

**Remaining for EP:** none. (Future: the O(n³) 2-opt re-eval is the next speed lever — see plan.)

---

## EE — Elevation / gradient scoring ✅

**What changed** (elevation is a *runtime overlay*, deliberately NOT in the graph cache — so
no cache-version bump and no re-parse; serialization is field-by-field so the new fields are
simply not written):
- `include/Graph.hpp` — `Node` gains `float elevation`, `Edge` gains `float grade`; new
  `Graph::loadElevation(path)` reads a sidecar, sets node elevations, and computes each edge's
  signed grade (forward + incoming copies).
- `include/RouteEvaluator.hpp` / `src/RouteEvaluator.cpp` — `UserProfile.weight_gradient`
  (default **0** = ignore elevation, mirrors how `weight_turns` sits outside `normalize()`).
  `evaluateEdge` subtracts `weight_gradient * |grade|/0.10` (symmetric steepness penalty);
  `evaluateRoute` accumulates `total_ascent_m` and a distance-weighted `gradient_penalty`,
  both added to `RouteScore`.
- `src/main.cpp` — `--elevation <file>` and `--weight_gradient <w>` flags; reports total ascent
  and adds `gradient_penalty` + `total_ascent_m` to the JSON `stats`.
- `tools/inspect_graph.cpp` — `--dump-nodes` emits `id lat lon` for every node.
- `tools/build_elevation.py` — samples a DEM (rasterio; GeoTIFF/.hgt) **or** a deterministic
  `--synthetic` terrain, writing the `VGEL` sidecar (`int64 id, float32 elev`).
- `tools/eval_quality.py` — `--elevation` / `--weight_gradient` passthrough; CSV + summary now
  carry `total_ascent_m` and `gradient_penalty`.

**Why** Hilliness is the biggest missing realism axis for cycling. Keeping elevation as a
sidecar overlay avoids touching the parser/cache and keeps the C++ build STL-only (DEM
sampling lives in Python).

**How verified** (node 281756094, 5000 m, scenic, seed 777, synthetic terrain):
- **Regression-safe:** elevation loaded + `weight_gradient 0` ⇒ route is **node-for-node
  identical** to the no-elevation run (distance + fitness match); only the new `total_ascent_m`
  measurement appears.
- **Effective:** `weight_gradient 0.6` cut single-route ascent **237 m → 138 m**. Aggregate over
  the curated 21-node set: see `runs/elev_off.csv` vs `runs/elev_on.csv`.

**Real DEM workflow (Copernicus GLO-30, no account/API key):**
```bash
./build/inspect_graph output/graph_cache_<pbf>_simp.bin --dump-nodes > nodes.txt
python3 tools/fetch_dem.py    --nodes nodes.txt --out-dir data/dem          # 1deg tiles from AWS S3
python3 tools/build_elevation.py --nodes nodes.txt --dem-dir data/dem --out output/elevation_real.bin
./build/VeloGraph <pbf> --start_node <id> --elevation output/elevation_real.bin --weight_gradient 0.4
```
- `tools/fetch_dem.py` downloads every 1°×1° tile covering a bbox (or a node dump) from the
  public `copernicus-dem-30m` S3 bucket. `build_elevation.py --dem-dir` samples across tiles,
  grouping nodes by 1° cell. DEM tiles are gitignored (`data/dem/`, ~250 MB for the regbez).

**How verified (real data):** Karlsruhe regbez, all 1.05M nodes sampled at 100% coverage,
elevations 83 m (Rhine valley) → 1159 m (N. Black Forest), matching geography; a direct rasterio
sample of a node equalled its sidecar value. Routing from a **hilly** Black Forest node
(281756094 @ 705 m, 5 km, seed 777): `weight_gradient 0.4` cut ascent **65 m → 24 m (−64%)** at
similar distance. Over the curated 21-node set (mostly flat Rhine-plain start points) the effect
is ~neutral (mean ascent 68 → 73 m) — there's nothing to avoid on flat ground. So the knob helps
exactly where it should and is roughly a no-op elsewhere; total ascent isn't the penalty's direct
objective (it penalizes per-edge |grade|), but the two correlate strongly in real hilly terrain.
CSVs: `runs/real_off.csv` vs `runs/real_on.csv`.

**Notes / gotchas**
- Copernicus COGs carry a half-pixel border (bounds.bottom ≈ 48.9999), so tile lookup rounds to
  the integer SW corner — flooring drops ~all matches. (Fixed in `build_elevation.py`.)
- Penalty is symmetric (|grade|), i.e. "avoid hilly terrain". A "seek hills" training mode would
  need a signed/negative-weight variant — future work.

**Remaining for EE:** none.

---

## Next: E2

Centralize the ~60 magic constants (turn penalties, waypoint eccentricity/jitter, distance-correction
thresholds, etc.) into `include/RouteParams.hpp` with defaults, loadable via `--params <file.json>`.
Verify gate: **defaults must reproduce current routes bit-for-bit** (regression-safe refactor — use the
E1 seed determinism to diff before/after).
