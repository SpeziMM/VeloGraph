#ifndef ROUTEFINDER_HPP
#define ROUTEFINDER_HPP

#include "Graph.hpp"
#include "RouteEvaluator.hpp"
#include "RoutePrecompute.hpp"
#include <vector>
#include <random>
#include <cmath>
#include <ostream>
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

    // seed: 0 = nondeterministic (random_device); nonzero = reproducible runs
    RouteFinder(const Graph& graph, const RouteEvaluator& evaluator, unsigned int seed = 0);

    // Per-iteration progress logging (off by default; would garble under threading)
    void setVerbose(bool v) { verbose_ = v; }

    // Cap worker threads (0 = auto = hardware_concurrency). Output is identical regardless.
    void setThreads(unsigned n) { max_threads_ = n; }

    // Main entry point: find best cycle within tolerance of target distance
    RouteResult findOptimalCycle(long start_node,
                                double target_distance,
                                double distance_tolerance,
                                const RouteEvaluator::UserProfile& profile,
                                int num_iterations = 100);

private:
    const Graph& graph;
    const RouteEvaluator& evaluator;
    // Base seed for the run. Per-iteration RNGs are derived from (base_seed_, i) so the
    // search is reproducible regardless of how iterations are scheduled across threads.
    unsigned int base_seed_;
    bool verbose_ = false;
    unsigned int max_threads_ = 0;  // 0 = auto

    // Result of one independent search iteration (collected, then reduced deterministically).
    struct IterationResult {
        std::vector<long> path;
        double combined = -1.0;       // 0.6*fitness + 0.4*distance_accuracy
        double total_distance = 0.0;
        double fitness = 0.0;
        double distance_error = 0.0;
        RouteEvaluator::RouteScore score;
        bool valid = false;
    };

    // Run one independent iteration with its own deterministically-seeded RNG.
    IterationResult runIteration(int i, long start_node, double target_distance,
                                 const RouteEvaluator::UserProfile& profile,
                                 const PrecomputeResult& precompute) const;

    // Verbose-gated log stream (real cout when verbose_, else a discarding sink).
    std::ostream& vlog() const;

    // Configuration
    static constexpr double MAX_TURN_ANGLE = 120.0;  // Degrees - penalize sharper turns
    // Min air distance at halfway: (target_distance / PI) * 0.8

    // Build a circular route that respects minimum radius constraint (legacy fallback)
    std::vector<long> buildCircularRoute(long start_node,
                                         double target_distance,
                                         const RouteEvaluator::UserProfile& profile,
                                         std::mt19937& rng) const;

    // Waypoint-driven route builder (primary)
    std::vector<long> buildWaypointRoute(long start_node,
                                         double target_distance,
                                         const RouteEvaluator::UserProfile& profile,
                                         const PrecomputeResult& precompute,
                                         std::mt19937& rng) const;

    // Budget-aware segment A*
    std::vector<long> findSegmentPath(long from_node, long to_node,
                                      double target_segment_distance,
                                      double max_segment_distance,
                                      const RouteEvaluator::UserProfile& profile,
                                      const PrecomputeResult& precompute,
                                      const std::unordered_set<long>& avoid_nodes,
                                      std::mt19937& rng) const;

    // Find path back to start using A* with fitness weighting
    std::vector<long> findReturnPath(long from_node, long to_node,
                                     double max_distance,
                                     const RouteEvaluator::UserProfile& profile,
                                     const std::unordered_set<long>& avoid_nodes,
                                     std::mt19937& rng) const;

    // Distance correction after 2-opt
    void correctDistance(std::vector<long>& path,
                        long start_node,
                        double target_distance,
                        const RouteEvaluator::UserProfile& profile,
                        std::mt19937& rng) const;

    // 2-opt local search improvement (deterministic; no RNG)
    void improveWith2Opt(std::vector<long>& path,
                        const RouteEvaluator::UserProfile& profile,
                        int max_iterations) const;

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
