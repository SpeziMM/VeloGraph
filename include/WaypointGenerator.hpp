#ifndef WAYPOINTGENERATOR_HPP
#define WAYPOINTGENERATOR_HPP

#include "Graph.hpp"
#include "RoutePrecompute.hpp"
#include <vector>
#include <random>

class WaypointGenerator {
public:
    struct Waypoint {
        long node_id;
        double lat;
        double lon;
        double shortest_home;
    };

    struct WaypointTemplate {
        std::vector<Waypoint> waypoints;
    };

    static WaypointTemplate generate(
        const Graph& graph,
        long start_node,
        double target_distance,
        const PrecomputeResult& precompute,
        std::mt19937& rng);
};

#endif
