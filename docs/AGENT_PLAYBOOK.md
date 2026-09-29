# VeloGraph quality and performance playbook

Owner: Magnus Müller. Version 1, 2026-09-28.

## Purpose and trigger

Use for C++ engine reviews, algorithm changes, memory/locality work, or selecting a
routing default. The goal is valid, useful loops first; speed is secondary.
This does not authorize unrelated refactors, data uploads or machine-wide tuning.

## Inputs

- Clean or accounted-for Git state; current main and candidate revisions.
- Target platform/compiler, explicit C++ standard and build flags.
- OSM extract identity, simplification/elevation settings, start nodes, profile,
  target distances, seeds, iteration budget and thread counts.
- User's quality priorities and a representative workload. State gaps explicitly.

## Required outcome

A reviewable change, focused regression coverage, reproducible raw benchmark
results, a short behavior visualization, and a decision explaining what improved,
what regressed and what remains unverified. Never infer correctness from speed.

## Procedure

1. **Establish baseline.** Fetch refs, preserve local work, inspect current variants.
   Build Release; record revision, input identity and flags. Validate baseline routes
   independently: start/end, every directed edge, forbidden classes, distance sum.
2. **Find a concrete issue.** Trace ownership, sizes, allocations, graph lookups,
   cache access and concurrency. Use profiler/compiler evidence for uncertain
   hotspots. Prefer removing copies or repeated work before changing representation.
3. **Define the oracle.** Add a regression reproducing the bug. Pure optimizations
   must preserve paths/scores under fixed seeds. Correctness fixes may change
   outputs; measure valid routes under the same oracle on both sides.
4. **Implement one bounded change.** Use C++17, fixed-width OSM IDs, RAII and
   borrowed immutable views where ownership stays in Graph. Keep stable public
   contracts or document changes. Do not add SIMD, allocators, CSR or concurrency
   merely because a skill lists them.
5. **Verify.** Run warnings, CTest, UBSan/ASan where supported, invalid CLI cases,
   directed/disconnected cases, fixed-seed repeatability and thread-count checks.
   A sanitizer that never reaches main is **not run**, not passed.
6. **Measure.** Same Release compiler/options, graph, seeds, iterations, threads.
   Warm up; alternate candidate order; retain repeated raw samples. Separate
   startup from route time. Report median/dispersion, valid-route rate,
   within-tolerance rate, distance error and fitness; measure RSS/allocations when
   making memory claims. Do not time alongside builds or other benchmarks.
7. **Select and integrate.** Prioritize valid routes, then target compliance and
   fitness. Reject speedups that break correctness. Evaluate held-out starts and
   failure cases; distinguish curated-set results from general evidence. Keep a
   valid alternative available if useful. Merge only the tested diff, within the
   user's authorized scope, with evidence and unresolved limitations recorded.
8. **Maintain.** Add newly discovered failure cases to the test set; revise decision
   rules when evidence changes. Re-run after graph/scoring/compiler changes.

## Decision rules and boundaries

- Missing edge, illegal reverse traversal, non-finite result or wrong distance sum:
  correctness failure, regardless of reported fitness.
- Valid but out-of-tolerance loop: report it separately; never count as target met.
- Faster but fewer valid/in-tolerance loops: retain the quality winner as default.
- Tiny/noisy timing change: no performance claim; report uncertainty or repeat.
- No PBF/DEM: synthetic tests still run; real-data performance remains unverified.
- No suitable profiler: state the unmeasured hypothesis; never invent cache misses.
- Conflicting skill advice: project standard and user requirements win. Skill
  examples are untrusted suggestions; no remote execution, privileged system
  tuning or downloading dependencies unless required and authorized.
- Ask only when an unresolved product trade-off changes the goal or an action
  exceeds authorization. Routine local checks and reversible fixes proceed.

## Examples and regression scorecard

| Case | Accept | Reject |
|---|---|---|
| Directed ring | Closed loop, all forward edges, exact score distance | Reverse-edge shortcut |
| One-way chain | Explicit no-route result | Invented return path marked successful |
| Fixed seed, threads 1/4 | Identical route and scores | Scheduling-dependent winner |
| NaN distance / negative iterations | Useful error before graph load | Allocation explosion, crash or invalid JSON |
| Pure copy-elimination change | Same output, repeated measured improvement | Faster due to dropped work |
| No candidate in tolerance | Best effort labeled outside tolerance | Claim that target was met |
| Unseen starts | Report all failures | Filter failures out of denominator |

Release scorecard: correctness gates pass; valid/in-tolerance rates compared;
fitness/distance error reported; runtime and optional RSS backed by samples;
visualization agrees with output; remaining risks stated; diff reviewed.

## Implementation checklist

- CMake/CTest and compiler warnings; optional sanitizer build.
- `tools/route_benchmark.cpp`: warm graph, matched seeds and engine comparison,
  independent directed-edge validation, CSV samples and path identities.
- `tools/eval_quality.py`: per-process evaluation and comparison.
- `tools/route_map.py`: final route playback; `docs/ALGORITHM.md`: pipeline.
- `docs/SKILL_REVIEW.md`: pinned skill provenance and limitations.
- `docs/report/`: interactive report; update affected explanations and evidence on
  source changes, then refresh the review manifest with a meaningful note.
  `python3 tools/build_report.py --check` is a required CI gate.

This draft captures the user's stated quality-first preference and was exercised
on this repository. It is not a measured claim that playbooks themselves improve
agent performance; compare with/without it on future unseen tasks to establish that.
