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
| **E3** | Rich JSON output + `tools/eval_quality.py` harness | ⬜ TODO |
| **E4** | `tools/sweep.py` + `route_map.py` viz upgrades | ⬜ TODO |
| **E5** | Parse new OSM tags + extend `Edge` + cache version bump | ⬜ TODO |
| **E6** | New scoring components + `running` profile | ⬜ TODO |
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

## Next: E2

Centralize the ~60 magic constants (turn penalties, waypoint eccentricity/jitter, distance-correction
thresholds, etc.) into `include/RouteParams.hpp` with defaults, loadable via `--params <file.json>`.
Verify gate: **defaults must reproduce current routes bit-for-bit** (regression-safe refactor — use the
E1 seed determinism to diff before/after).
