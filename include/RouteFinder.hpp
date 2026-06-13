#ifndef ROUTEFINDER_HPP
#define ROUTEFINDER_HPP

#include "Graph.hpp"
#include "RouteEvaluator.hpp"
#include "RoutePrecompute.hpp"
#include <vector>
#include <random>
#include <cmath>
#include <unordered_set>

class RouteFinder {
public:
    struct RouteResult {
        std::vector<long> path;
        double total_distance;
        double fitness_score;
        double distance_error;  // |actual - target|
        RouteEvaluator::RouteScore detailed_score;
    };

    RouteFinder(const Graph& graph, const RouteEvaluator& evaluator);

    // Main entry point: find best cycle within tolerance of target distance
    RouteResult findOptimalCycle(long start_node,
                                double target_distance,
                                double distance_tolerance,
                                const RouteEvaluator::UserProfile& profile,
                                int num_iterations = 100);

private:
    const Graph& graph;
    const RouteEvaluator& evaluator;
    std::mt19937 rng;
    
    // Configuration
    static constexpr double MAX_TURN_ANGLE = 120.0;  // Degrees - penalize sharper turns
    // Min air distance at halfway: (target_distance / PI) * 0.8

    // Build a circular route that respects minimum radius constraint (legacy fallback)
    std::vector<long> buildCircularRoute(long start_node,
                                         double target_distance,
                                         const RouteEvaluator::UserProfile& profile);

    // Waypoint-driven route builder (primary)
    std::vector<long> buildWaypointRoute(long start_node,
                                         double target_distance,
                                         const RouteEvaluator::UserProfile& profile,
                                         const PrecomputeResult& precompute);

    // Budget-aware segment A*
    std::vector<long> findSegmentPath(long from_node, long to_node,
                                      double target_segment_distance,
                                      double max_segment_distance,
                                      const RouteEvaluator::UserProfile& profile,
                                      const PrecomputeResult& precompute,
                                      const std::unordered_set<long>& avoid_nodes);

    // Find path back to start using A* with fitness weighting
    std::vector<long> findReturnPath(long from_node, long to_node,
                                     double max_distance,
                                     const RouteEvaluator::UserProfile& profile,
                                     const std::unordered_set<long>& avoid_nodes);

    // Distance correction after 2-opt
    void correctDistance(std::vector<long>& path,
                        long start_node,
                        double target_distance,
                        const RouteEvaluator::UserProfile& profile);

    // 2-opt local search improvement
    void improveWith2Opt(std::vector<long>& path,
                        const RouteEvaluator::UserProfile& profile,
                        int max_iterations);

    // Helper: compute total path distance
    double getPathDistance(const std::vector<long>& path) const;

    // Helper: get edge between two nodes
    const Graph::Edge* getEdge(long from, long to) const;

    // Helper: check if path forms valid cycle
    bool isValidCycle(const std::vector<long>& path, long start_node) const;
    
    // Helper: calculate turn angle at node (degrees)
    double calculateTurnAngle(long prev_node, long current_node, long next_node) const;
    
    // Helper: calculate distance between two nodes
    double distance(const Graph::Node* n1, const Graph::Node* n2) const;
    
    // Helper: calculate distance from start (in degrees, approximation)
    double distanceFromStart(long node, long start_node) const;
    
    // Helper: check if edge type is suitable for cycling
    // at_start_or_end=true means we're at start/end of route, allow all paths
    // relaxed_mode=true means we allow service roads and shorter edges to escape dead ends
    bool isCyclingEdge(const Graph::Edge& edge, bool at_start_or_end, bool relaxed_mode = false) const;
};

#endif
