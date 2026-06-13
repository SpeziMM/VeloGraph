# VeloGraph - Cycling Loop Route Generator

C++17 routing engine that generates circular cycling routes (loops) from OpenStreetMap data.
Unlike A-to-B routing, VeloGraph solves the **constrained cycle finding problem**: given a start node and target distance, find a loop that maximizes a fitness heuristic (scenery, safety, surface quality).

## Build & Run

```bash
# Build (requires libosmium, zlib, bzip2, expat)
cd build && cmake .. && make -j$(sysctl -n hw.ncpu)

# Run — requires a .osm.pbf file and a start node ID
./build/VeloGraph data/karlsruhe_small.osm.pbf \
  --start_node <NODE_ID> \
  --target_distance 5000 \
  --profile scenic \
  --iterations 10

# Visualize output
python3 visualize_path.py output/sample_path.json output.png
```

### CLI flags
| Flag | Description | Default |
|------|-------------|---------|
| `--start_node <id>` | OSM node ID (required) | — |
| `--target_distance <m>` | Target loop distance in meters | 5000 |
| `--profile <name>` | `scenic`, `safe-night`, `mountain-bike`, `casual` | scenic |
| `--iterations <n>` | Number of search iterations | 10 |
| `--no_simplify` | Skip graph simplification | off |
| `--output_path <file>` | JSON output path | `output/sample_path.json` |

### Getting a start node ID
Use the `inspect_graph` or `get_first_node` tools in `tools/` to find valid node IDs from a parsed graph, or use `findClosestNode()` in the Graph API with lat/lon coordinates.

## Architecture

```
OSMParser (libosmium)  →  Graph (adjacency list + KD-tree)  →  RouteFinder  →  JSON output
     ↓                         ↓                                    ↓
  PBF/XML input          spatial index                     RouteEvaluator (fitness scoring)
```

### Key modules

| File | Purpose |
|------|---------|
| `include/OSMParser.hpp`, `src/OSMParser.cpp` | Parses .osm.pbf via libosmium, extracts highway/surface/lit tags, builds edges. Caches parsed graphs to `output/graph_cache_*.bin`. |
| `include/Graph.hpp`, `src/Graph.cpp` | Adjacency list graph with KD-tree spatial index, Haversine distance, graph simplification (merge degree-2 nodes), binary serialization. |
| `include/RouteFinder.hpp`, `src/RouteFinder.cpp` | The cycle-finding algorithm: Phase 1 outbound walk + Phase 2 A* return + 2-opt optimization. |
| `include/RouteEvaluator.hpp`, `src/RouteEvaluator.cpp` | Weighted fitness scoring: safety (lit), scenery (low-traffic), surface quality, traffic penalty. User profiles. |
| `src/main.cpp` | CLI entry point, argument parsing, JSON export. |
| `visualize_path.py` | Python matplotlib path visualizer. |
| `tools/` | Standalone utilities: `parse_only.cpp`, `inspect_graph.cpp`, `get_first_node.cpp`. |

## Algorithm overview (RouteFinder)

1. **Outbound walk** (`buildCircularRoute`): Greedy stochastic walk expanding outward from start. Uses fitness scoring + direction bias + turn-angle penalties + randomness. Walks until ~50% of target distance, enforcing minimum air-distance from start.
2. **Return path** (`findReturnPath`): A* search back to start, using fitness-weighted edge costs and haversine heuristic. Avoids recently-visited nodes but allows partial revisits.
3. **2-opt refinement** (`improveWith2Opt`): Local search swapping path segments to improve fitness while maintaining edge connectivity and turn-angle constraints.
4. **Multi-iteration search** (`findOptimalCycle`): Runs N iterations, keeps best by fitness score.

## Data

- OSM PBF files go in `data/` (gitignored). Download from [Geofabrik](https://download.geofabrik.de/).
- Graph caches are written to `output/graph_cache_*.bin` and auto-invalidated when the PBF changes.
- Route output is JSON with node coordinates + fitness breakdown.

## Dependencies

- **C++17** compiler (GCC/Clang)
- **CMake** 3.10+
- **libosmium** (osmium headers + protozero)
- **zlib**, **bzip2**, **expat** (for PBF decompression)
- **Python 3** + matplotlib + numpy (visualization only)

On macOS: `brew install libosmium protozero`
On Ubuntu: `apt install libosmium2-dev libprotozero-dev libbz2-dev libexpat1-dev zlib1g-dev`

## Code conventions

- C++17, STL-only (no Boost). RAII, raw pointers for non-owning references.
- Build flags: `-O3 -march=native` for release.
- No test framework currently. Validation is done by running the engine on real PBF data and visualizing output.
