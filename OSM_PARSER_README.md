# OSM PBF Parser and Path Visualization

## Overview

This implementation provides an efficient OpenStreetMap (OSM) PBF parser using libosmium and a Python-based path visualization tool.

## Features

- **Efficient PBF Parsing**: Uses libosmium for fast, streaming OSM data processing
- **Graph Construction**: Builds an optimized adjacency list representation with Haversine distance calculation
- **Path Visualization**: Python script to visualize paths on parsed graphs using matplotlib
- **Performance**: Targets sub-second parsing for city-scale maps
- **Graph Cache**: Stores the built (and optionally simplified) graph for fast reuse

## Dependencies

### C++ (Parser)
- libosmium2-dev
- libprotozero-dev
- libbz2-dev
- libexpat1-dev
- zlib1g-dev
- C++17 compiler

### Python (Visualization)
- python3
- matplotlib
- numpy
- contextily (optional, for map backgrounds)
- folium (optional, for interactive maps)

## Installation

### Install Dependencies (macOS)
```bash
brew update
brew install osmium-tool protozero libbz2 expat zlib

# Python dependencies (basic)
pip3 install matplotlib numpy folium

# Optional: For map backgrounds with contextily (requires GDAL)
brew install gdal
pip3 install contextily
```

### Build the Project
```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### 1. Parse OSM Data

Download an OSM extract from [Geofabrik](https://download.geofabrik.de/) or use your own OSM file:

```bash
# With PBF file
./build/VeloGraph data/city.osm.pbf

# With XML file
./build/VeloGraph data/city.osm
```

The parser will:
- Parse the OSM file and extract nodes and ways
- Build a graph with nodes and edges
- Calculate distances using Haversine formula
- Export a sample path to `sample_path.json`

Caching:
- The graph is cached under `output/graph_cache_<pbf_name>_{simp|raw}.bin`.
- The cache is invalidated when the PBF size or modification time changes.

### 2. Visualize the Path

Visualize a route with `tools/route_map.py` (Leaflet + OpenStreetMap tiles):

```bash
# Render route JSON to an interactive HTML map and open it
python3 tools/route_map.py output/sample_path.json output/route_map.html
```

The map shows the loop polyline, highlights revisited nodes, and renders a stats panel
(distance, fitness, scenery/quality/traffic/turn breakdown) from the route JSON.

## Example Output

```
[VeloGraph] Initializing Route Engine...

[VeloGraph] Loading OSM data...
[OSMParser] Reading PBF file: data/test_map.osm
[OSMParser] Parsed in 1ms
[OSMParser] Nodes: 10
[OSMParser] Edges: 19
[OSMParser] Building graph structure...
[OSMParser] Graph built in 0ms
[OSMParser] Total time: 1ms

[Statistics]
  - Nodes processed: 10
  - Ways processed: 4
  - Unique nodes in graph: 10
  - Total edges: 19

[VeloGraph] Graph Statistics:
  - Nodes: 10
  - Edges: 19

[VeloGraph] Exporting sample path with 11 nodes...
[VeloGraph] Sample path exported to sample_path.json
[VeloGraph] Use 'python3 tools/route_map.py sample_path.json' to visualize

[VeloGraph] Engine Ready.
```

## Architecture

### OSMParser
- **Handler-based processing**: Implements osmium::handler::Handler for efficient streaming
- **Filtering**: Extracts only highway tags (roads, paths) for routing
- **Bidirectional edges**: Automatically creates reverse edges for two-way roads
- **One-way support**: Respects `oneway=yes` tags

### Graph
- **Adjacency list representation**: O(1) average case traversal
- **Node storage**: Uses unordered_map for O(1) node lookups
- **Edge weights**: Calculated using Haversine formula for accurate distances
- **Memory efficient**: Stores only essential routing data

### Visualizer
- **JSON input**: Simple format for path data exchange
- **Matplotlib rendering**: Clear, annotated path visualization
- **Start/End markers**: Green for start, red for end
- **Statistics overlay**: Shows node counts and coordinate ranges

## Data Format

### Path JSON Format
```json
{
  "nodes": [
    {"id": 1, "lat": 48.1351, "lon": 11.5701},
    {"id": 2, "lat": 48.1352, "lon": 11.5703},
    ...
  ]
}
```

## Performance

The parser is optimized for:
- **Streaming processing**: Low memory overhead
- **Fast parsing**: Sub-second for city-scale maps
- **Efficient graph construction**: Linear time complexity O(N)
- **Cache-friendly**: Adjacency list structure
- **Cache reuse**: Subsequent runs can skip parsing/building when the cache is valid

## Testing

A test OSM file is provided in `data/test_map.osm` with a simple street network for testing.

## Future Enhancements

- [ ] Spatial indexing (Quadtree/R-Tree) for range queries
- [ ] A* pathfinding implementation
- [ ] Loop generation algorithm
- [ ] Interactive visualization
- [ ] Support for elevation data
- [ ] Route optimization based on fitness heuristics
