#include "Graph.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

void Graph::addNode(const Node& n) {
    nodes[n.id] = n;
    index_built = false; // Invalidate index
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
    return calculateDistance(from.lat, from.lon, to.lat, to.lon);
}

double Graph::calculateDistance(double lat1, double lon1, double lat2, double lon2) const {
    constexpr double R = 6371000.0; // Earth radius in meters
    constexpr double DEG_TO_RAD = M_PI / 180.0;

    // Optimization: Use Equirectangular approximation for short distances (< ~11km)
    if (std::abs(lat1 - lat2) < 0.1 && std::abs(lon1 - lon2) < 0.1) {
        const double avg_lat_rad = (lat1 + lat2) * 0.5 * DEG_TO_RAD;
        const double delta_lat = (lat2 - lat1) * DEG_TO_RAD;
        const double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
        
        const double x = delta_lon * std::cos(avg_lat_rad);
        const double y = delta_lat;
        
        return R * std::sqrt(x * x + y * y);
    }

    // Fallback to Haversine
    double lat1_rad = lat1 * DEG_TO_RAD;
    double lat2_rad = lat2 * DEG_TO_RAD;
    double delta_lat = (lat2 - lat1) * DEG_TO_RAD;
    double delta_lon = (lon2 - lon1) * DEG_TO_RAD;
    
    double a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
               std::cos(lat1_rad) * std::cos(lat2_rad) *
               std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);
    
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    
    return R * c;
}

void Graph::buildSpatialIndex() {
    if (index_built) return;
    
    spatial_index.clear();
    spatial_index.reserve(nodes.size());
    for (const auto& pair : nodes) {
        // Only include nodes that have outgoing edges
        auto it = adjacency_list.find(pair.first);
        if (it != adjacency_list.end() && !it->second.empty()) {
            spatial_index.push_back(pair.first);
        }
    }
    
    buildKDTree(0, spatial_index.size(), 0);
    index_built = true;
}

void Graph::buildKDTree(size_t start, size_t end, int depth) {
    if (start >= end) return;

    size_t mid = start + (end - start) / 2;
    int axis = depth % 2; // 0 for lat, 1 for lon

    auto comparator = [this, axis](long id_a, long id_b) {
        const auto& node_a = nodes.at(id_a);
        const auto& node_b = nodes.at(id_b);
        return (axis == 0) ? (node_a.lat < node_b.lat) : (node_a.lon < node_b.lon);
    };

    std::nth_element(spatial_index.begin() + start, 
                     spatial_index.begin() + mid, 
                     spatial_index.begin() + end, 
                     comparator);

    buildKDTree(start, mid, depth + 1);
    buildKDTree(mid + 1, end, depth + 1);
}

const Graph::Node* Graph::findClosestNode(double lat, double lon) const {
    if (spatial_index.empty()) return nullptr;
    
    long best_node = -1;
    double min_dist = std::numeric_limits<double>::max();
    
    findNearestNeighbor(0, spatial_index.size(), 0, lat, lon, best_node, min_dist);
    
    return (best_node != -1) ? getNode(best_node) : nullptr;
}

void Graph::findNearestNeighbor(size_t start, size_t end, int depth, double lat, double lon, long& best_node, double& min_dist) const {
    if (start >= end) return;

    size_t mid = start + (end - start) / 2;
    long current_id = spatial_index[mid];
    const auto& current_node = nodes.at(current_id);

    double d = calculateDistance(lat, lon, current_node.lat, current_node.lon);
    if (d < min_dist) {
        min_dist = d;
        best_node = current_id;
    }

    int axis = depth % 2;
    double diff = (axis == 0) ? (lat - current_node.lat) : (lon - current_node.lon);
    
    // Determine which side to search first
    size_t near_start = start, near_end = mid;
    size_t far_start = mid + 1, far_end = end;
    
    if (diff > 0) {
        // Query is greater than current node, search right side first
        near_start = mid + 1;
        near_end = end;
        far_start = start;
        far_end = mid;
    }

    findNearestNeighbor(near_start, near_end, depth + 1, lat, lon, best_node, min_dist);

    // Check if we need to search the other side
    // Calculate distance to the splitting plane
    double plane_dist_meters;
    constexpr double R = 6371000.0;
    constexpr double DEG_TO_RAD = M_PI / 180.0;
    
    if (axis == 0) { // Latitude split
        plane_dist_meters = std::abs(diff) * DEG_TO_RAD * R;
    } else { // Longitude split
        // Use the query latitude for a conservative estimate of longitude degree length
        plane_dist_meters = std::abs(diff) * DEG_TO_RAD * R * std::cos(lat * DEG_TO_RAD);
    }

    if (plane_dist_meters < min_dist) {
        findNearestNeighbor(far_start, far_end, depth + 1, lat, lon, best_node, min_dist);
    }
}
