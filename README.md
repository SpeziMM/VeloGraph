VeloGraph: High-Performance Loop Generator

VeloGraph is a C++ based routing engine designed to generate circular running and cycling routes (loops) based on user constraints.

Unlike standard A-to-B routing (Dijkstra/A*), VeloGraph solves the constrained cycle finding problem, aiming to generate a route of length $L$ that begins and ends at a start node $S$, maximizing "fitness" heuristic (scenery, low traffic).

🚀 Engineering Goals & Complexity Interest

This project is a playground for low-level systems programming and algorithmic optimization.

Memory Efficiency: * Parsing OpenStreetMap (OSM) data (often gigabytes in size) using custom stream processing to minimize RAM overhead.

Implementing Compressed Sparse Row (CSR) or optimized Adjacency Lists to represent the graph, targeting minimal cache misses.

Spatial Indexing: * Implementation of a Quadtree (or R-Tree) to perform spatial range queries (e.g., "Find all nodes within 5km") in $O(\log N)$ time.

Heuristic Search: * Implementing a modified Depth-First Search (DFS) with pruning and randomized heuristics (Monte Carlo steps) to find cycles, as finding a perfect cycle of exact length is theoretically NP-Hard.

🛠 Tech Stack

Language: C++17 (Focus on RAII, smart pointers, and template metaprogramming)

Build System: CMake

Data Source: OpenStreetMap (XML/PBF)

External Libs: (Planned) libosmium for PBF parsing, otherwise STL only.

🏗 Architecture

Parser Module: Streams OSM XML nodes/ways and filters irrelevant data (buildings, power lines) to construct a routing graph.

Graph Core: Stores nodes (intersections) and edges (roads) with weighted costs (distance + elevation penalty).

Spatial Index: Partitions the 2D map space to allow fast "Nearest Neighbor" lookups.

Solver:

Phase 1: Generate waypoints roughly forming a polygon (Triangle/Square) fitting the distance.

Phase 2: Run A* pathfinding between waypoints.

Phase 3: Optimize loop closure.

⚡ Performance Benchmarks (Goals)

Operation

Target Time

Graph Build (City Scale)

< 500ms

Route Generation (10km)

< 50ms

Spatial Query (Radius)

< 1ms

🏃‍♂️ Getting Started

## Prerequisites

**C++ Dependencies:**
- C++ Compiler (GCC/Clang with C++17 support)
- CMake (3.10+)
- libosmium2-dev
- libprotozero-dev
- libbz2-dev
- libexpat1-dev
- zlib1g-dev

**Python Dependencies (for visualization):**
- python3
- matplotlib
- numpy

## Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y libosmium2-dev libprotozero-dev libbz2-dev \
    libexpat1-dev zlib1g-dev python3-matplotlib python3-numpy

# Build the project
mkdir build
cd build
cmake ..
make
```

## Usage

```bash
# Parse OSM data (PBF or XML format)
./build/VeloGraph data/map.osm.pbf

# Visualize the generated path
python3 visualize_path.py sample_path.json output.png
```

## Features Implemented

✅ **Efficient OSM PBF Parser**
- Stream-based processing with libosmium
- Filters highway tags for routing networks
- Bidirectional and one-way road support
- Sub-millisecond parsing for small datasets

✅ **Graph Construction**
- Adjacency list representation
- Haversine distance calculation for edge weights
- O(1) node lookups with unordered_map

✅ **Path Visualization**
- Python script with matplotlib
- Annotated paths with start/end markers
- Export to PNG format

See [OSM_PARSER_README.md](OSM_PARSER_README.md) for detailed documentation.

🗺 Data

To test the engine, download an .osm.pbf extract from [Geofabrik](https://download.geofabrik.de/). Place it in the `/data` folder.
