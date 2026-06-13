#include "RouteEvaluator.hpp"
#include <algorithm>

// Predefined profiles
RouteEvaluator::UserProfile RouteEvaluator::getProfileScenic() {
    UserProfile p;
    p.name = "scenic";
    p.weight_safety = 0.2;
    p.weight_scenery = 0.4;
    p.weight_quality = 0.2;
    p.weight_traffic = 0.2;
    p.weight_turns = 0.15;
    p.is_night_mode = false;
    return p;
}

RouteEvaluator::UserProfile RouteEvaluator::getProfileSafeNight() {
    UserProfile p;
    p.name = "safe-night";
    p.weight_safety = 0.5;
    p.weight_scenery = 0.1;
    p.weight_quality = 0.2;
    p.weight_traffic = 0.2;
    p.weight_turns = 0.10;
    p.is_night_mode = true;
    return p;
}

RouteEvaluator::UserProfile RouteEvaluator::getProfileMountainBike() {
    UserProfile p;
    p.name = "mountain-bike";
    p.weight_safety = 0.1;
    p.weight_scenery = 0.3;
    p.weight_quality = 0.1;
    p.weight_traffic = 0.5;
    p.weight_turns = 0.05;
    p.is_night_mode = false;
    return p;
}

RouteEvaluator::UserProfile RouteEvaluator::getProfileCasualCommuter() {
    UserProfile p;
    p.name = "casual";
    p.weight_safety = 0.25;
    p.weight_scenery = 0.15;
    p.weight_quality = 0.35;
    p.weight_traffic = 0.25;
    p.weight_turns = 0.20;
    p.is_night_mode = false;
    return p;
}

RouteEvaluator::UserProfile RouteEvaluator::getProfileByName(const std::string& name) {
    if (name == "scenic") return getProfileScenic();
    if (name == "safe-night" || name == "night") return getProfileSafeNight();
    if (name == "mountain-bike" || name == "mtb") return getProfileMountainBike();
    if (name == "casual" || name == "commuter") return getProfileCasualCommuter();
    return getProfileScenic(); // Default
}

double RouteEvaluator::getTrafficPenalty(Graph::HighwayClass hw_class) {
    switch (hw_class) {
        case Graph::HighwayClass::Motorway:     return 1.0;
        case Graph::HighwayClass::Trunk:        return 0.9;
        case Graph::HighwayClass::Primary:      return 0.8;
        case Graph::HighwayClass::Secondary:    return 0.6;
        case Graph::HighwayClass::Tertiary:     return 0.4;
        case Graph::HighwayClass::Unclassified: return 0.3;
        case Graph::HighwayClass::Residential:  return 0.2;
        case Graph::HighwayClass::LivingStreet: return 0.1;
        case Graph::HighwayClass::Service:      return 0.15;
        case Graph::HighwayClass::Track:        return 0.1;
        case Graph::HighwayClass::Path:         return 0.05;
        case Graph::HighwayClass::Cycleway:     return 0.0;  // Best for cyclists
        default:                                return 0.5;
    }
}

double RouteEvaluator::getSurfaceScore(Graph::SurfaceQuality surface) {
    switch (surface) {
        case Graph::SurfaceQuality::Excellent:    return 1.0;
        case Graph::SurfaceQuality::Good:         return 0.8;
        case Graph::SurfaceQuality::Intermediate: return 0.6;
        case Graph::SurfaceQuality::Bad:          return 0.3;
        case Graph::SurfaceQuality::VeryBad:      return 0.1;
        default:                                  return 0.5;  // Unknown
    }
}

double RouteEvaluator::getSceneryScore(Graph::HighwayClass hw_class) {
    // Scenic routes are typically low-traffic paths, tracks, residential
    switch (hw_class) {
        case Graph::HighwayClass::Cycleway:     return 1.0;
        case Graph::HighwayClass::Path:         return 0.95;
        case Graph::HighwayClass::Track:        return 0.9;
        case Graph::HighwayClass::LivingStreet: return 0.8;
        case Graph::HighwayClass::Residential:  return 0.7;
        case Graph::HighwayClass::Service:      return 0.5;
        case Graph::HighwayClass::Unclassified: return 0.6;
        case Graph::HighwayClass::Tertiary:     return 0.4;
        case Graph::HighwayClass::Secondary:    return 0.25;
        case Graph::HighwayClass::Primary:      return 0.1;
        case Graph::HighwayClass::Trunk:        return 0.05;
        case Graph::HighwayClass::Motorway:     return 0.0;
        default:                                return 0.5;
    }
}

double RouteEvaluator::evaluateEdge(const Graph::Edge& edge, 
                                    const UserProfile& profile) const {
    double safety_score = edge.is_lit ? 1.0 : 0.0;
    double scenery_score = getSceneryScore(edge.highway_class);
    double quality_score = getSurfaceScore(edge.surface);
    double traffic_penalty = getTrafficPenalty(edge.highway_class);

    // Night mode: heavily penalize unlit streets
    double effective_safety_weight = profile.weight_safety;
    if (profile.is_night_mode && !edge.is_lit) {
        effective_safety_weight *= 2.0;  // Double penalty for unlit at night
    }

    // Compute weighted fitness (higher is better)
    double fitness = effective_safety_weight * safety_score
                   + profile.weight_scenery * scenery_score
                   + profile.weight_quality * quality_score
                   - profile.weight_traffic * traffic_penalty;

    // Clamp to [0, 1]
    return std::max(0.0, std::min(1.0, fitness));
}

RouteEvaluator::RouteScore RouteEvaluator::evaluateRoute(
        const Graph& graph,
        const std::vector<long>& path,
        const UserProfile& profile) const {

    RouteScore result = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    if (path.size() < 2) return result;

    double total_lit_distance = 0.0;
    double total_scenery_weighted = 0.0;
    double total_quality_weighted = 0.0;
    double total_traffic_weighted = 0.0;

    for (size_t i = 0; i < path.size() - 1; ++i) {
        const auto* edges = graph.getEdges(path[i]);
        if (!edges) continue;

        for (const auto& edge : *edges) {
            if (edge.to_node_id == path[i + 1]) {
                double dist = edge.weight;
                result.total_distance += dist;

                if (edge.is_lit) total_lit_distance += dist;
                total_scenery_weighted += getSceneryScore(edge.highway_class) * dist;
                total_quality_weighted += getSurfaceScore(edge.surface) * dist;
                total_traffic_weighted += getTrafficPenalty(edge.highway_class) * dist;
                break;
            }
        }
    }

    // Count sharp turns (> 45 degrees)
    int sharp_turns = 0;
    for (size_t i = 1; i + 1 < path.size(); ++i) {
        const auto* p1 = graph.getNode(path[i - 1]);
        const auto* p2 = graph.getNode(path[i]);
        const auto* p3 = graph.getNode(path[i + 1]);
        if (!p1 || !p2 || !p3) continue;
        double angle = GeoUtils::turnAngle(p1->lat, p1->lon, p2->lat, p2->lon, p3->lat, p3->lon);
        if (angle > 45.0) sharp_turns++;
    }

    if (result.total_distance > 0) {
        result.safety_score = total_lit_distance / result.total_distance;
        result.scenery_score = total_scenery_weighted / result.total_distance;
        result.quality_score = total_quality_weighted / result.total_distance;
        result.traffic_penalty = total_traffic_weighted / result.total_distance;

        double turns_per_km = sharp_turns / (result.total_distance / 1000.0);
        result.turn_penalty = std::min(1.0, turns_per_km / 10.0);

        double effective_safety_weight = profile.weight_safety;
        if (profile.is_night_mode) {
            effective_safety_weight *= (1.0 + (1.0 - result.safety_score));
        }

        result.total_fitness = effective_safety_weight * result.safety_score
                             + profile.weight_scenery * result.scenery_score
                             + profile.weight_quality * result.quality_score
                             - profile.weight_traffic * result.traffic_penalty
                             - profile.weight_turns * result.turn_penalty;

        result.total_fitness = std::max(0.0, std::min(1.0, result.total_fitness));
    }

    return result;
}
