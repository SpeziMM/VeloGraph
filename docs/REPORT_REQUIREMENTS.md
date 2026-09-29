# Interactive algorithm report — agreed scope

Status: implemented scope following the user's “implement the rest” instruction on
2026-09-29. Quality remains the priority over speed.

## Included

- A repository-owned, hierarchical interactive report: fast overview; drill-down
  through route stages, algorithms/data structures, efficiency and limitations;
  inputs/outputs, failure scenarios, historical benchmarks and hardware boundaries.
- Plain HTML/CSS/JavaScript with no framework, external assets, telemetry or tile
  requests. It works offline. Teaching diagrams are explicitly illustrative;
  checked-in engine outputs and benchmark data are labeled separately.
- A command builder for node, coordinate and center-plus-radius start modes; local
  route-JSON import and point inspection. The browser does not execute the engine.
- Engine start selection: nearest indexed graph node with default 250 m maximum
  snap, or nearest eligible starts inside a radius, ordered by distance then ID.
  Up to 8 candidates by default (1–64 configurable). Each candidate receives the
  requested iteration budget. Candidates run sequentially, iterations in parallel.
- The loop starts and ends at the selected node. Radius constrains the start only.
  Existing profile preferences, repeated roads and best-effort tolerance semantics
  stay unchanged. No new definition of route suitability is silently imposed.
- Focused tests for selection/CLI behavior; generated benchmark summaries; a source
  review fingerprint and CI freshness gate; agent instructions requiring updates.
- Keep the existing small engine/CLI/tools/tests/docs organization. Add a focused
  StartSelection module rather than a generic service or interface framework.

## Explicitly deferred by the user

1. [CPU scaling, stage costs and memory profiling — issue #4](https://github.com/SpeziMM/VeloGraph/issues/4).
   Includes GPU feasibility assessment; no CPU/GPU speedup claims or implementations
   are added in this phase.
2. [Reliable route infeasibility classification — issue #5](https://github.com/SpeziMM/VeloGraph/issues/5).
   No exact oracle, proof engine or new “impossible” status is implemented. An empty
   result still means only that this bounded search found no route.

## Defaults chosen for unspecified details

Developer-oriented English, with a short nontechnical overview. Use the existing
Karlsruhe data, current JSON output and offline SVG geometry. Retain a portable
CPU build. No public hosting, live backend, polygon selection or GPX export.
Historical benchmark results remain historical; the new area integration run is
an example, not a controlled multi-start performance benchmark.

## Completion checks

- Pipeline and contracts agree with the current source; links identify implementations.
- Report interactions work by keyboard and on narrow screens.
- Real route points and benchmark charts are derived from checked-in evidence.
- Point/area selection has deterministic, bounded semantics and regression coverage.
- CI detects added, edited or removed relevant source, and modified generated evidence.
- Known limitations and the two future issues remain prominent.
