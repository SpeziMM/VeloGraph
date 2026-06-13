#ifndef ROUTEEVALUATOR_HPP
#define ROUTEEVALUATOR_HPP

#include "Graph.hpp"
#include "GeoUtils.hpp"
#include <vector>
#include <string>

class RouteEvaluator {
public:
    // User preference profile for route scoring
    struct UserProfile {
        double weight_safety;      // Prefer lit streets [0, 1]
        double weight_scenery;     // Prefer low-traffic scenic routes [0, 1]
        double weight_quality;     // Prefer good surface quality [0, 1]
        double weight_traffic;     // Penalize high-traffic roads [0, 1]
        double weight_turns;       // Penalize sharp turns [0, 1]
        bool is_night_mode;        // Boost safety weight at night
        std::string name;

        // Normalize weights to sum to 1.0
        void normalize() {
            double sum = weight_safety + weight_scenery + weight_quality + weight_traffic + weight_turns;
            if (sum > 0) {
                weight_safety /= sum;
                weight_scenery /= sum;
                weight_quality /= sum;
                weight_traffic /= sum;
                weight_turns /= sum;
            }
        }
    };

    // Route evaluation result with breakdown
    struct RouteScore {
        double total_fitness;       // Combined weighted score [0, 1]
        double safety_score;        // Lit coverage percentage
        double scenery_score;       // Low-traffic percentage
        double quality_score;       // Surface quality score
        double traffic_penalty;     // High-traffic penalty
        double turn_penalty;        // Sharp turns per km, normalized [0, 1]
        double total_distance;      // Total route distance in meters
    };

    // Predefined user profiles
    static UserProfile getProfileScenic();
    static UserProfile getProfileSafeNight();
    static UserProfile getProfileMountainBike();
    static UserProfile getProfileCasualCommuter();
    static UserProfile getProfileByName(const std::string& name);

    // Evaluate a complete route
    RouteScore evaluateRoute(const Graph& graph, 
                            const std::vector<long>& path,
                            const UserProfile& profile) const;

    // Evaluate a single edge (for incremental scoring)
    double evaluateEdge(const Graph::Edge& edge, 
                       const UserProfile& profile) const;

private:
    // Convert highway class to traffic penalty [0, 1]
    static double getTrafficPenalty(Graph::HighwayClass hw_class);
    
    // Convert surface quality to score [0, 1]
    static double getSurfaceScore(Graph::SurfaceQuality surface);
    
    // Scenic score based on road type (low traffic = scenic)
    static double getSceneryScore(Graph::HighwayClass hw_class);
};

#endif
