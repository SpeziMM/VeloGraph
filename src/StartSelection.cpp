#include "StartSelection.hpp"
#include "GeoUtils.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
void validate(double lat, double lon, double radius) {
    if (!std::isfinite(lat) || !std::isfinite(lon) || std::abs(lat) > 90 ||
        std::abs(lon) > 180 || !std::isfinite(radius) || radius <= 0)
        throw std::invalid_argument("Invalid start coordinates or radius");
}
}

StartSelection::Selection StartSelection::nearest(const Graph& graph, double lat,
                                                  double lon, double max_snap_m) {
    validate(lat, lon, max_snap_m);
    const auto* node = graph.findClosestNode(lat, lon);
    if (!node) return {};
    const double offset = GeoUtils::distance(lat, lon, node->lat, node->lon);
    if (offset > max_snap_m) return {};
    return {{{node->id, offset}}, 1};
}

StartSelection::Selection StartSelection::inArea(const Graph& graph, double lat,
                                                 double lon, double radius_m, std::size_t limit) {
    validate(lat, lon, radius_m);
    if (limit == 0 || limit > 64) throw std::invalid_argument("Start candidate limit must be in [1,64]");
    Selection result;
    for (NodeId id : graph.findNodesInRadius(lat, lon, radius_m)) {
        const auto* node = graph.getNode(id);
        const auto* edges = graph.getEdges(id);
        if (!node || !edges) continue;
        const bool eligible = std::any_of(edges->begin(), edges->end(), [](const Graph::Edge& edge) {
            return edge.highway_class != Graph::HighwayClass::Motorway &&
                   edge.highway_class != Graph::HighwayClass::Trunk;
        });
        const double offset = GeoUtils::distance(lat, lon, node->lat, node->lon);
        if (eligible && offset <= radius_m) result.candidates.push_back({id, offset});
    }
    std::sort(result.candidates.begin(), result.candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.offset_m != b.offset_m ? a.offset_m < b.offset_m : a.id < b.id;
    });
    result.eligible_count = result.candidates.size();
    if (result.candidates.size() > limit) result.candidates.resize(limit);
    return result;
}
