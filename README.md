# VeloGraph

C++17 cycling-loop generator using OpenStreetMap. Given a start and target length,
it searches for a closed route scored for scenery, lighting, surface, traffic,
turns and optional elevation. It is an experimental heuristic router.

The default **hybrid** engine creates ellipse waypoints, searches between them,
falls back to a greedy walk when needed, and refines the result. Directed-edge
validation prevents invented reverse connections. Quality takes priority over
speed: candidates within distance tolerance rank ahead of best-effort loops.

## Build and test

Requires CMake 3.18+, a C++17 compiler, libosmium, protozero, zlib, bzip2 and expat.
Python 3 is optional for CLI tests and visualization; these tools use its standard
library. Leaflet and map tiles require internet access in the browser.

```sh
# Debian / Ubuntu
sudo apt-get install cmake g++ libosmium2-dev libprotozero-dev libbz2-dev libexpat1-dev zlib1g-dev python3
# macOS dependencies: brew install cmake libosmium protozero
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`-DVELOGRAPH_NATIVE=ON` enables tuning for the build machine; it is off by default.
For debugging, use `-DCMAKE_BUILD_TYPE=Debug -DVELOGRAPH_SANITIZERS=ON`.

## Generate and view a route

Download a regional `.osm.pbf` extract from [Geofabrik](https://download.geofabrik.de/)
and put it in `data/`. Run from the repository root:

```sh
./build/VeloGraph data/map.osm.pbf --start 49.0 8.4 \
  --target_distance 5000 --profile scenic --iterations 100 \
  --seed 777 --threads 4 --engine hybrid --tolerance 0.1 \
  --output_path output/route.json
python3 tools/route_map.py output/route.json output/route.html
```

Use `--start_node ID` instead of coordinates for reproducible evaluations.
`--engine greedy` retains the comparison algorithm. Other profiles are `safe-night`,
`mountain-bike`, and `casual`. `--help` lists the complete interface.

JSON includes the actual seed, engine, measured search time, score breakdown and
`within_tolerance`. A valid loop outside tolerance is a labeled best effort;
no route exits with code 2, input/runtime errors with code 1. Playback follows the
final route, while the panel briefly explains the search pipeline.

The map tool serves the generated page on loopback HTTP until Ctrl+C so browsers
can send the referrer required by the [OSM tile policy](https://operations.osmfoundation.org/policies/tiles/).
Direct `file://` views show route geometry but disable street tiles. `--no-open`
generates HTML only; add `--serve` to serve without launching a browser. Tiles
use the canonical HTTPS endpoint, linked copyright attribution and normal browser
caching; there is no bulk download or offline tile feature. Browsers or embedded
viewers that strip referrers must not be used to load the OSM tile layer.

## Interactive algorithm report

Open [the engine notebook](docs/report/index.html) for the pipeline, algorithms/data
structures, input command builder, recorded route points, failure scenarios and
benchmark comparisons. It works offline. [Scope](docs/REPORT_REQUIREMENTS.md) and
[maintenance instructions](docs/report/README.md) live alongside it. CI requires a
reviewed report refresh when relevant code or evidence changes.

Coordinate starts snap within 250 m by default (`--max_snap` changes the limit).
To search nearby starts instead, use `--start LAT LON --start_radius 500
--start_candidates 8`. The nearest eligible starts are tried in deterministic order;
the limit is 1–64 and iterations apply **per start**. The chosen node is both start
and finish. `run` reports `start_mode`, `start_offset_m`, `start_radius_m`,
`distance_tolerance`, `eligible_starts` and `searched_starts`. An unsuccessful subset
search does not rule out other starts in the area.

## Evidence and next changes

- [Algorithm diagram and limitations](docs/ALGORITHM.md)
- [Measured quality and performance report](docs/QUALITY_REPORT.md)
- [Quality-first agent playbook](docs/AGENT_PLAYBOOK.md)
- [Installed C++ skills and security review](docs/SKILL_REVIEW.md)
- [Developer details and elevation tooling](CLAUDE.md)

On the recorded 63-case set, corrected hybrid returned 48 valid loops, compared
with 37 for corrected greedy. On 60 held-out cases, it met the target tolerance in
23 cases versus 15. This supports the current default for that workload, not a
claim of universal optimality. Full OSM bicycle restrictions, richer geometry and
broader datasets remain future work.
