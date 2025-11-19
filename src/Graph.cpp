#include "Graph.hpp"
#include <cmath>

void Graph::addNode(const Node& n) {
    nodes[n.id] = n;
}

void Graph::addEdge(long from_id, long to_id) {
    // Calculate weight (distance) between nodes
    const Node* from = getNode(from_id);
    const Node* to = getNode(to_id);
    
    if (from && to) {
        Edge edge;
        edge.to_node_id = to_id;
        edge.weight = calculateDistance(*from, *to);
        adjacency_list[from_id].push_back(edge);
    }
}

const Graph::Node* Graph::getNode(long id) const {
    auto it = nodes.find(id);
    return (it != nodes.end()) ? &it->second : nullptr;
}

const std::vector<Graph::Edge>* Graph::getEdges(long node_id) const {
    auto it = adjacency_list.find(node_id);
    return (it != adjacency_list.end()) ? &it->second : nullptr;
}

size_t Graph::edgeCount() const {
    size_t count = 0;
    for (const auto& [node_id, edges] : adjacency_list) {
        count += edges.size();
    }
    return count;
}

// Haversine formula to calculate distance between two lat/lon points
double Graph::calculateDistance(const Node& from, const Node& to) const {
    const double R = 6371000.0; // Earth radius in meters
    
    double lat1_rad = from.lat * M_PI / 180.0;
    double lat2_rad = to.lat * M_PI / 180.0;
    double delta_lat = (to.lat - from.lat) * M_PI / 180.0;
    double delta_lon = (to.lon - from.lon) * M_PI / 180.0;
    
    double a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);
    
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    
    return R * c; // Distance in meters
}
