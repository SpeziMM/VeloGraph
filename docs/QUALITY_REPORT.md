# Quality-first integration — 2026-09-29

## Decision

Keep the hybrid waypoint engine as default, with greedy available through
`--engine greedy`. Both now obey directed connectivity and share scoring,
refinement and distance-tolerance selection. Hybrid finds more valid and
within-tolerance loops on the measured and held-out starts. Greedy is faster but
loses quality, so it does not replace the default.

The integration starts from `roadmap/quality` (`d9716f3`) and incorporates
`origin/main` (`bcb833d`). Main previously contained the parser but not the current
routing implementation. Earlier `simple_improvements` introduced the hybrid;
roadmap added deterministic parallel iterations and elevation. The quality
comparison below isolates the old roadmap behavior and the two corrected engines;
it is not an exhaustive search of every historical parameter configuration.

## Changes

- Require portable C++17, honor Debug/Release flags, target-scoped dependencies,
  warnings, optional native tuning and sanitizer instrumentation.
- Use `std::int64_t` OSM node IDs throughout. Borrow incoming adjacency by const
  reference instead of allocating/copying it on every reverse-precompute visit.
- Remove invented reverse edges from route searches. Independently validate
  complete closed paths; reject missing edges in scoring. Keep actual meters
  separate from weighted return-search cost and validate the return budget.
- Initialize failure results, validate CLI/API inputs, propagate worker exceptions,
  make the silent logging sink thread-local, and avoid launching an extra thread
  for serial searches. Fixed seeds remain independent of thread scheduling.
- Reject impossible 2-opt swaps before allocating/copying a candidate path.
  Remove redundant search state; no speculative custom allocator or graph rewrite.
- Rank target-compliant candidates first; export the actual seed, engine and
  explicit tolerance result. Invalid input exits 1; no route exits 2.
- Add directed-graph/cache/determinism regressions, CLI checks, Linux CI,
  reproducible benchmarking, route playback and an operational agent playbook.

## Method

Local macOS 26.6.2, arm64, Apple Clang 17.0.0 (clang-1700.6.3.2), C++17 Release,
`-O3 -march=native`, four route workers. No DEM overlay. Input is the simplified
Karlsruhe regional graph, scenic profile, 5,000 m target, 10% tolerance, 100
iterations, seeds 777/778/779. Parsing/cache loading and spatial-index construction
are excluded from route timings; precompute and refinement are included.

There is one unrecorded warmup pass followed by three measured passes. Engines
alternate order each pass. Each node/seed combination counts once for quality;
all repeats count for timing. All repeated paths/scores were identical. A separate
validator checks closure, every forward edge, forbidden motorway/trunk classes and
summed distance rather than trusting the router's success flag.

The existing 21-start set was curated from earlier reported successes and gives
63 cases; its rates are selection-biased. Twenty additional starts sampled with
`inspect_graph --sample 20 2026` give 60 held-out cases; failures were not filtered
out after sampling. Both sets come from one region and one target/profile, which
limits generalization. Graph-valid does not mean all OSM access restrictions are
implemented; see [remaining limitations](ALGORITHM.md#remaining-quality-limitations).

PBF SHA-256: `4ffb0649df8f1fd9c7e4cb275b82ddf5d3bbf87e08741668d8541825ca227f22`.
Start lists: [existing](../tools/eval_nodes_karlsruhe.txt),
[held-out](../tools/eval_nodes_heldout.txt).

## Results

| Engine / set | Valid loops | Within tolerance | Invalid reported successes | Mean fitness¹ | Mean error¹ | Median / p90 ms |
|---|---:|---:|---:|---:|---:|---:|
| Old roadmap hybrid / 63 | 21 | 17 | 40 | .3734 | 557.0 m | 9.527 / 25.930 |
| Corrected hybrid / 63 | 48 | 35 | 0 | .3844 | 652.2 m | 3.061 / 10.633 |
| Corrected greedy / 63 | 37 | 17 | 0 | .3172 | 1566.1 m | 1.825 / 3.255 |
| Corrected hybrid / held-out 60 | 31 | 23 | 0 | .3842 | 520.8 m | 2.455 / 7.030 |
| Corrected greedy / held-out 60 | 27 | 15 | 0 | .3564 | 1246.5 m | 1.745 / 4.982 |

¹ Means include only valid returned loops, whose membership differs between rows.
They are not paired estimates of per-route improvement. In particular the old
router's attractive reported success rate included invalid reverse traversals.
The old/new timing comparison changes behavior, so it is not a pure optimization
speedup. Unsuccessful searches are retained in timing samples and denominators.

## Output-preserving optimization check

An ablation removed only the early connectivity check before copying a 2-opt path.
All 378 compared engine/node/seed/repeat outputs had exactly the same paths and
scores. Hybrid pooled median fell from 4.204 to 3.061 ms (27% lower); p90 from
15.532 to 10.633 ms. Greedy medians were 1.967 and 1.825 ms.

Hybrid whole-pass totals before/after were 402/318, 455/377, and 414/333 ms.
The ablation binaries ran sequentially, not in randomized interleaved order; this
is local evidence of reduced work, not a statistically controlled universal
speedup. No hardware counters, peak RSS or allocation profiler were measured, so
there is no claim about measured cache misses or peak-memory reduction.

## Reproduction and evidence

```sh
cmake -S . -B build/quality -DCMAKE_BUILD_TYPE=Release -DVELOGRAPH_NATIVE=ON
cmake --build build/quality --parallel
ctest --test-dir build/quality --output-on-failure
mkdir -p runs
./build/quality/route_benchmark data/karlsruhe-regbez-251117.osm.pbf \
  tools/eval_nodes_karlsruhe.txt runs/current.csv 100 4 3
./build/quality/route_benchmark data/karlsruhe-regbez-251117.osm.pbf \
  tools/eval_nodes_heldout.txt runs/heldout.csv 100 4 3
python3 tools/summarize_benchmark.py runs/current.csv runs/heldout.csv
```

[Archived measurements](benchmarks/2026-09-29/) retain every timed sample and
SHA-256 route identities; the local original CSVs retain full paths. Run the same
summarizer on archived CSVs. `baseline_verified.csv` is d9716f3 linked to the new
benchmark with `VELOGRAPH_BASELINE`; `no_precheck.csv` removes the early block in
`improveWith2Opt`; `precheck.csv` is the final engine. Other final changes after
measurement affect CLI validation, visualization and reporting, not routing.

Local Release CTest and Debug UBSan CTest passed (core and CLI). Apple ASan could
not complete runtime initialization before main; a process sample showed an
ASan initialization lock, so local ASan is **not passed**. Linux CI passed both Release and Debug with ASan+UBSan for engine commit
`9c1fab7` ([run](https://github.com/SpeziMM/VeloGraph/actions/runs/36566194892)).
The integration PR retains checks for subsequent documentation-only revisions.

Playback was checked for slider input, keyboard activation and completion at 100%.
The map renders a final route (4.97 km in the demo), not internal search expansions.

## Follow-up priorities

Improve OSM bicycle access/one-way interpretation and preserve simplified edge
geometry; then broaden quality evaluation to more regions, distances, profiles and
DEM scenarios. A multi-label distance-constrained search may recover feasible
loops missed by the current bounded single-label heuristic. Evaluate it with this
same validity oracle before changing the default.
