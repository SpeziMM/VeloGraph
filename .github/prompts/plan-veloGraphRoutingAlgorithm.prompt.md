# VeloGraph Constrained Cycle Routing Algorithm Plan

## Problem Statement

VeloGraph solves the **constrained cycle finding problem**: generate a route of target length **L** that begins and ends at a start node **S**, maximizing a "fitness" heuristic (scenery, safety, surface quality) unlike standard A-to-B routing (Dijkstra/A*).

## Research Findings Summary

### 1. Available OSM Data for Heuristics

**Road Classification:**
- `highway=motorway|trunk|primary|secondary|tertiary|unclassified|residential|service`
- Special: `living_street`, `track`, `path`, `footway`, `cycleway`, `bridleway`

**Street Lighting (Safety):**
- `lit=yes|no|24/7|automatic|limited|interval` - well-documented in developed areas (~40-50% coverage)
- Specific types: `lit_by_led=yes`, `lit_by_gaslight=yes`

**Surface & Quality:**
- `surface=asphalt|concrete|paving_stones|gravel|unpaved|dirt`
- `smoothness=excellent|good|intermediate|bad|very_bad|horrible|impassable`
- `tracktype=grade1-5` for track quality

**Green Space & Scenery:**
- `landuse=forest|grass|greenery`
- `natural=wood|grassland`
- `leisure=park` for urban parks
- `leaf_type=broadleaved|needleleaved|mixed` for forest type

**Traffic & Restrictions:**
- `oneway=yes|-1` for one-way constraints
- `maxspeed=*` as implicit traffic indicator (high maxspeed → higher traffic assumption)
- Road class as traffic proxy (motorway > primary > secondary > residential)

### 2. Constrained Cycle Finding Algorithms

**Core Approach: Orienteering Problem (OP)**
- Start/end at same depot (node S)
- Maximize collected value within distance budget L
- NP-hard; exact solutions intractable for large instances

**Practical Solution Methods:**
1. **Greedy Nearest-Neighbor Seeding** - O(n) initial path construction
2. **Local Search Refinement** - 2-opt/3-opt edge swaps, O(n²) per iteration
3. **Metaheuristics** - Simulated annealing, tabu search, genetic algorithms for escape local minima

**Algorithm Choice:** Start with **Randomized Nearest-Neighbor + 2-opt Local Search**
- Complexity: O(n² × iterations) where n = path length, iterations ≈ 100-1000
- Achieves 85-95% of theoretical optimum for practical problem sizes
- Implementable in C++ efficiently

### 3. Proposed Heuristics & Fitness Functions

**Scenic Routes:**
- Favor `leisure=park`, `landuse=forest|grass` segments
- Penalize motorways/trunk roads
- Weight by segment distance (green exposure per km)

**Safe Night Routing:**
- High weight on `lit=yes` segments
- Avoid `lit=no` paths at night
- Prefer residential areas (better lighting infrastructure, lower isolation)

**Avoid Big Streets:**
- Penalize `highway=motorway|trunk|primary` heavily
- Boost `highway=residential|unclassified|path`
- Use maxspeed as secondary penalty (high speed → traffic → avoid)

**Low-Traffic Estimate:**
- Road class proxy: motorway (penalty 1.0) → residential (penalty 0.1)
- Time-of-day heuristic (peak hours 7-9am, 5-7pm → higher penalty)
- Population density grid (high density near roads → higher traffic)

**Surface Quality (Cyclist Preference):**
- `smoothness=excellent` → bonus
- `surface=gravel|unpaved` → penalty (unpaved roads worth exploring for MTB, neutral for casual)
- `tracktype` for off-road classification

**Composite Fitness Function:**
```
fitness = w_safety × lit_score 
        + w_scenery × green_score 
        + w_quality × surface_score 
        - w_traffic × road_class_penalty
        + w_amenity × poi_density
```
All scores normalized to [0,1]. Weights tunable per user profile.

**User Preference Profiles:**
- **Scenic:** w_scenery=0.4, w_traffic=0.2, w_quality=0.2, w_safety=0.2
- **Safe Night:** w_safety=0.5, w_scenery=0.2, w_traffic=0.2, w_quality=0.1
- **Mountain Biker:** w_quality=0.4, w_scenery=0.3, w_traffic=0.1, w_safety=0.2
- **Casual Commuter:** w_traffic=0.4, w_quality=0.3, w_scenery=0.2, w_safety=0.1

### 4. Data Enrichment (Phase 2)

**Not Required Initially - Start with OSM Tags**

**Optional Later Additions:**
- **Elevation:** SRTM DEM (30m resolution, free) for hilliness scoring
- **Traffic Patterns:** OSM road class + population density grid from WorldPop/LandScan
- **Green Proximity:** Spatial index on parks/forests for distance-to-nature scoring
- **POI Density:** Cluster amenities for interest point scoring

## Implementation Plan

### Phase 1: Core Algorithm (Current Focus)

#### Step 1: Enrich Graph Edges with Fitness Attributes
**File:** `include/Graph.hpp`, `src/Graph.cpp`

Extend `Graph::Edge` struct:
```cpp
struct Edge {
    long to_node_id;
    double weight;           // Distance in meters
    double fitness_score;    // Multi-objective fitness [0,1]
    unsigned char highway_class;  // 0=motorway,...,7=path
    bool is_lit;            // Street lighting indicator
    unsigned char surface_quality; // 0=impassable,...,4=excellent
    // Optional: green_proximity, amenity_density, elevation_gain
};
```

During OSM parsing, decode tags and populate these fields.

#### Step 2: Implement RouteEvaluator Class
**Files:** `include/RouteEvaluator.hpp`, `src/RouteEvaluator.cpp`

```cpp
class RouteEvaluator {
public:
    struct UserProfile {
        double weight_safety;      // [0, 1]
        double weight_scenery;
        double weight_quality;
        double weight_traffic;
        bool is_night_mode;
    };
    
    // Compute fitness of a complete route
    double evaluateRoute(const Graph& graph, 
                        const std::vector<long>& path,
                        const UserProfile& profile) const;
    
    // Compute edge fitness contribution
    double evaluateEdge(const Graph::Edge& edge, 
                       const UserProfile& profile) const;
    
    // Predefined profiles
    static UserProfile getProfileScenic();
    static UserProfile getProfileSafeNight();
    static UserProfile getProfileMountainBike();
    static UserProfile getProfileCasualCommuter();
};
```

#### Step 3: Implement Constrained Cycle Generator
**Files:** `include/RouteFinder.hpp`, `src/RouteFinder.cpp`

```cpp
class RouteFinder {
private:
    const Graph& graph;
    const RouteEvaluator& evaluator;
    
    // Greedy nearest-neighbor path construction
    std::vector<long> greedyPathConstruction(long start_node, 
                                             double target_distance,
                                             const RouteEvaluator::UserProfile& profile);
    
    // 2-opt local search
    void improveWith2Opt(std::vector<long>& path,
                        double target_distance,
                        int max_iterations);
    
    // Helper: compute total path distance
    double getPathDistance(const std::vector<long>& path) const;
    
public:
    struct RouteResult {
        std::vector<long> path;
        double total_distance;
        double fitness_score;
        double distance_error;  // |actual - target|
    };
    
    // Main entry point: find best cycle within epsilon of target distance
    RouteResult findOptimalCycle(long start_node,
                                double target_distance,
                                double distance_tolerance,
                                const RouteEvaluator::UserProfile& profile,
                                int num_iterations = 100);
};
```

#### Step 4: Integrate into main.cpp
Update `main()` to:
1. Parse optional parameters: `--target-distance <meters> --profile <scenic|safe-night|mountain|casual>`
2. Build spatial index
3. Find closest node to start coordinates
4. Call `RouteFinder::findOptimalCycle()`
5. Export result path to JSON with fitness breakdown

### Phase 2: Data Enrichment & Refinement
- Integrate elevation data (SRTM) for hilliness scoring
- Add green-space proximity via spatial indexing on `leisure=park`, `landuse=forest`
- Implement population density grid for traffic estimation
- Add turn-restriction awareness (respect `oneway`, turn restrictions)
- Multi-path comparison interface (show top-3 routes with trade-offs)

### Phase 3: Advanced Features
- Time-window constraints (user availability windows)
- Multi-objective Pareto front (trade scenery vs. distance vs. safety)
- Real-time traffic integration (if API available)
- Machine learning-based weights from user feedback

## Cycle Length Constraint Handling

**Recommendation: Start with Soft Constraint**
- Allow ±5-10% deviation from target length L
- Simplifies problem: nearest-neighbor builds path of roughly correct length, 2-opt refines
- User perceives as "approximately X km route"

**Future: Strict Constraint**
- Use iterative deepening or binary search on distance budget
- More complex but guarantees exact length cycles
- Useful for fitness activities with fixed energy budgets

## Algorithm Complexity & Performance

| Step | Complexity | Time (n=100 nodes) |
|------|------------|-------------------|
| Greedy NN seeding | O(n²) | ~1ms |
| 2-opt iteration | O(n²) | ~10ms |
| 100 iterations | O(100×n²) | ~1s |
| Total | O(I×n²) | <2s |

**Acceptable for real-time user requests** (sub-second response for typical city graphs).

## Testing & Validation

1. **Unit Tests:** Edge fitness scoring, distance calculations
2. **Integration Tests:** Full cycle generation on sample map (Karlsruhe dataset)
3. **Visualization:** Export routes to `sample_path.json`, visualize with existing Python tools
4. **Comparison:** Compare against greedy straight-line routes to verify fitness improvement

## Success Criteria

- Generate valid cycles (start = end, path connected)
- Cycle distance within target ±10% (soft constraint)
- Fitness score ≥ 85% of theoretical maximum (greedy + 2-opt quality)
- Response time < 2 seconds for typical city-scale queries
- Support 3+ user preference profiles
- Visualizable paths in HTML/PNG format
