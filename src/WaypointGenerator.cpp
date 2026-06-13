#include "WaypointGenerator.hpp"
#include "GeoUtils.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <unordered_set>

WaypointGenerator::WaypointTemplate WaypointGenerator::generate(
        const Graph& graph,
        long start_node,
        double target_distance,
        const PrecomputeResult& precompute,
        std::mt19937& rng) {

    const auto* start = graph.getNode(start_node);
    if (!start) return {};

    constexpr double ROAD_FACTOR = 1.3;
    double R = target_distance / (2.0 * M_PI * ROAD_FACTOR);

    std::uniform_int_distribution<int> n_dist(4, 6);
    int N = n_dist(rng);

    std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * M_PI);
    double base_angle = angle_dist(rng);

    std::uniform_real_distribution<double> ecc_dist(0.7, 1.0);
    double eccentricity = ecc_dist(rng);

    std::uniform_real_distribution<double> jitter(0.8, 1.2);

    double budget_limit = target_distance * 0.6;

    WaypointTemplate templ;

    for (int i = 0; i < N; ++i) {
        double angle = base_angle + (2.0 * M_PI * i) / N;

        double r = R * jitter(rng);
        double rx = r;
        double ry = r * eccentricity;
        double cos_a = std::cos(angle);
        double sin_a = std::sin(angle);
        double eff_r = (rx * ry) / std::sqrt(
            ry * ry * cos_a * cos_a + rx * rx * sin_a * sin_a);

        double dlat, dlon;
        GeoUtils::offsetToLatLon(start->lat, eff_r, angle, dlat, dlon);

        double wp_lat = start->lat + dlat;
        double wp_lon = start->lon + dlon;

        const auto* snapped = graph.findClosestNode(wp_lat, wp_lon);
        if (!snapped) continue;

        auto it = precompute.shortest_home.find(snapped->id);
        if (it == precompute.shortest_home.end()) continue;
        if (it->second > budget_limit) continue;
        if (precompute.dead_end_nodes.count(snapped->id)) continue;

        Waypoint wp;
        wp.node_id = snapped->id;
        wp.lat = snapped->lat;
        wp.lon = snapped->lon;
        wp.shortest_home = it->second;
        templ.waypoints.push_back(wp);
    }

    // Snapping can reorder waypoints relative to their ideal angular positions,
    // and distinct ideal positions can collapse onto the same graph node.
    // Deduplicate, then sort by bearing from the start so the route traverses
    // them in a consistent rotational order (no self-crossing segments).
    std::unordered_set<long> seen;
    std::vector<Waypoint> unique_wps;
    for (const auto& wp : templ.waypoints) {
        if (seen.insert(wp.node_id).second) {
            unique_wps.push_back(wp);
        }
    }

    // Bearing of each waypoint relative to start (atan2 over lat/lon deltas).
    std::sort(unique_wps.begin(), unique_wps.end(),
        [start](const Waypoint& a, const Waypoint& b) {
            double angle_a = std::atan2(a.lat - start->lat, a.lon - start->lon);
            double angle_b = std::atan2(b.lat - start->lat, b.lon - start->lon);
            return angle_a < angle_b;
        });

    // Randomize traversal direction: clockwise vs counter-clockwise.
    std::uniform_int_distribution<int> dir_dist(0, 1);
    if (dir_dist(rng) == 1) {
        std::reverse(unique_wps.begin(), unique_wps.end());
    }

    // Random rotation of the start index so the loop doesn't always begin
    // toward the same waypoint.
    if (!unique_wps.empty()) {
        std::uniform_int_distribution<size_t> start_dist(0, unique_wps.size() - 1);
        size_t offset = start_dist(rng);
        std::rotate(unique_wps.begin(), unique_wps.begin() + offset, unique_wps.end());
    }

    templ.waypoints = std::move(unique_wps);
    return templ;
}
