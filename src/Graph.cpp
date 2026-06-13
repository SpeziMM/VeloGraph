#include "Graph.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <unordered_set>
#include <thread>
#include <fstream>
#include <cstdint>

void Graph::addNode(const Node& n) {
    nodes[n.id] = n;
    index_built = false; // Invalidate index
}

void Graph::addEdge(long from_id, long to_id, HighwayClass hw_class, 
                    SurfaceQuality surface, bool is_lit, bool is_oneway) {
    // Calculate weight (distance) between nodes
    const Node* from = getNode(from_id);
    const Node* to = getNode(to_id);
    
    if (from && to) {
        Edge edge;
        edge.to_node_id = to_id;
        edge.weight = calculateDistance(*from, *to);
        edge.highway_class = hw_class;
        edge.surface = surface;
        edge.is_lit = is_lit;
        edge.is_oneway = is_oneway;
        adjacency_list[from_id].push_back(edge);
        
        // Store incoming edge for reverse traversal
        incoming_adjacency_list[to_id].emplace_back(from_id, edge);
    }
}

void Graph::buildFrom(const std::unordered_map<long, Node>& source_nodes,
                      const std::vector<EdgeInput>& edges) {
    nodes = source_nodes;
    adjacency_list.clear();
    incoming_adjacency_list.clear();
    index_built = false;

    adjacency_list.reserve(nodes.size());
    incoming_adjacency_list.reserve(nodes.size());

    const size_t edge_count = edges.size();
    if (edge_count == 0 || nodes.empty()) {
        return;
    }

    const unsigned int hw_threads = std::max(1u, std::thread::hardware_concurrency());
    const size_t target_per_thread = 100000;
    const size_t thread_count = std::max<size_t>(1, std::min<size_t>(hw_threads, (edge_count + target_per_thread - 1) / target_per_thread));

    using AdjMap = std::unordered_map<long, std::vector<Edge>>;
    using IncomingMap = std::unordered_map<long, std::vector<std::pair<long, Edge>>>;

    std::vector<AdjMap> local_adj(thread_count);
    std::vector<IncomingMap> local_incoming(thread_count);
    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    auto worker = [&](size_t tid, size_t begin, size_t end) {
        auto& adj = local_adj[tid];
        auto& inc = local_incoming[tid];

        for (size_t i = begin; i < end; ++i) {
            const auto& input = edges[i];
            auto from_it = nodes.find(input.from_id);
            if (from_it == nodes.end()) {
                continue;
            }
            auto to_it = nodes.find(input.to_id);
            if (to_it == nodes.end()) {
                continue;
            }

            Edge edge;
            edge.to_node_id = input.to_id;
            edge.weight = calculateDistance(from_it->second, to_it->second);
            edge.highway_class = input.highway_class;
            edge.surface = input.surface;
            edge.is_lit = input.is_lit;
            edge.is_oneway = input.is_oneway;

            adj[input.from_id].push_back(edge);
            inc[input.to_id].emplace_back(input.from_id, edge);
        }
    };

    const size_t chunk = (edge_count + thread_count - 1) / thread_count;
    for (size_t t = 0; t < thread_count; ++t) {
        const size_t begin = t * chunk;
        const size_t end = std::min(edge_count, begin + chunk);
        workers.emplace_back(worker, t, begin, end);
    }
    for (auto& w : workers) {
        w.join();
    }

    for (size_t t = 0; t < thread_count; ++t) {
        for (auto& [node_id, edges_vec] : local_adj[t]) {
            auto& target = adjacency_list[node_id];
            target.insert(target.end(), edges_vec.begin(), edges_vec.end());
        }
        for (auto& [node_id, edges_vec] : local_incoming[t]) {
            auto& target = incoming_adjacency_list[node_id];
            target.insert(target.end(), edges_vec.begin(), edges_vec.end());
        }
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

std::vector<long> Graph::findNodesInRadius(double lat, double lon, double radius_meters) const {
    std::vector<long> results;
    if (spatial_index.empty()) return results;
    findNodesInRange(0, spatial_index.size(), 0, lat, lon, radius_meters, results);
    return results;
}

void Graph::findNodesInRange(size_t start, size_t end, int depth,
                             double lat, double lon, double radius_meters,
                             std::vector<long>& results) const {
    if (start >= end) return;

    size_t mid = start + (end - start) / 2;
    long current_id = spatial_index[mid];
    const auto& current_node = nodes.at(current_id);

    double d = calculateDistance(lat, lon, current_node.lat, current_node.lon);
    if (d <= radius_meters) {
        results.push_back(current_id);
    }

    int axis = depth % 2;
    double diff = (axis == 0) ? (lat - current_node.lat) : (lon - current_node.lon);

    constexpr double R = 6371000.0;
    constexpr double DEG_TO_RAD = M_PI / 180.0;
    double plane_dist_meters;
    if (axis == 0) {
        plane_dist_meters = std::abs(diff) * DEG_TO_RAD * R;
    } else {
        plane_dist_meters = std::abs(diff) * DEG_TO_RAD * R * std::cos(lat * DEG_TO_RAD);
    }

    size_t near_start = start, near_end = mid;
    size_t far_start = mid + 1, far_end = end;
    if (diff > 0) {
        near_start = mid + 1; near_end = end;
        far_start = start; far_end = mid;
    }

    findNodesInRange(near_start, near_end, depth + 1, lat, lon, radius_meters, results);

    if (plane_dist_meters <= radius_meters) {
        findNodesInRange(far_start, far_end, depth + 1, lat, lon, radius_meters, results);
    }
}

std::vector<std::pair<long, Graph::Edge>> Graph::getIncomingEdges(long node_id) const {
    auto it = incoming_adjacency_list.find(node_id);
    if (it != incoming_adjacency_list.end()) {
        std::vector<std::pair<long, Edge>> result;
        result.reserve(it->second.size());
        
        for (const auto& pair : it->second) {
            long from_id = pair.first;
            Edge original_edge = pair.second;
            
            // Create reverse edge pointing back to from_id
            Edge reverse_edge = original_edge;
            reverse_edge.to_node_id = from_id;
            result.emplace_back(from_id, reverse_edge);
        }
        return result;
    }
    return {};
}

void Graph::rebuildIncomingFromAdjacency() {
    incoming_adjacency_list.clear();
    incoming_adjacency_list.reserve(nodes.size());
    for (const auto& [from_id, edges] : adjacency_list) {
        for (const auto& edge : edges) {
            incoming_adjacency_list[edge.to_node_id].emplace_back(from_id, edge);
        }
    }
}

bool Graph::serialize(std::ostream& out) const {
    const uint64_t node_count = static_cast<uint64_t>(nodes.size());
    const uint64_t adj_count = static_cast<uint64_t>(adjacency_list.size());
    out.write(reinterpret_cast<const char*>(&node_count), sizeof(node_count));
    out.write(reinterpret_cast<const char*>(&adj_count), sizeof(adj_count));

    for (const auto& [id, node] : nodes) {
        out.write(reinterpret_cast<const char*>(&node.id), sizeof(node.id));
        out.write(reinterpret_cast<const char*>(&node.lat), sizeof(node.lat));
        out.write(reinterpret_cast<const char*>(&node.lon), sizeof(node.lon));
        out.write(reinterpret_cast<const char*>(&node.flags), sizeof(node.flags));
    }

    for (const auto& [from_id, edges] : adjacency_list) {
        out.write(reinterpret_cast<const char*>(&from_id), sizeof(from_id));
        const uint64_t edge_count = static_cast<uint64_t>(edges.size());
        out.write(reinterpret_cast<const char*>(&edge_count), sizeof(edge_count));
        for (const auto& edge : edges) {
            out.write(reinterpret_cast<const char*>(&edge.to_node_id), sizeof(edge.to_node_id));
            out.write(reinterpret_cast<const char*>(&edge.weight), sizeof(edge.weight));
            const uint8_t hw = static_cast<uint8_t>(edge.highway_class);
            const uint8_t surf = static_cast<uint8_t>(edge.surface);
            const uint8_t lit = static_cast<uint8_t>(edge.is_lit ? 1 : 0);
            const uint8_t oneway = static_cast<uint8_t>(edge.is_oneway ? 1 : 0);
            out.write(reinterpret_cast<const char*>(&hw), sizeof(hw));
            out.write(reinterpret_cast<const char*>(&surf), sizeof(surf));
            out.write(reinterpret_cast<const char*>(&lit), sizeof(lit));
            out.write(reinterpret_cast<const char*>(&oneway), sizeof(oneway));
        }
    }

    return static_cast<bool>(out);
}

bool Graph::deserialize(std::istream& in) {
    uint64_t node_count = 0;
    uint64_t adj_count = 0;
    in.read(reinterpret_cast<char*>(&node_count), sizeof(node_count));
    in.read(reinterpret_cast<char*>(&adj_count), sizeof(adj_count));
    if (!in) {
        return false;
    }

    nodes.clear();
    adjacency_list.clear();
    incoming_adjacency_list.clear();
    nodes.reserve(static_cast<size_t>(node_count));
    adjacency_list.reserve(static_cast<size_t>(adj_count));

    for (uint64_t i = 0; i < node_count; ++i) {
        Node node{};
        in.read(reinterpret_cast<char*>(&node.id), sizeof(node.id));
        in.read(reinterpret_cast<char*>(&node.lat), sizeof(node.lat));
        in.read(reinterpret_cast<char*>(&node.lon), sizeof(node.lon));
        in.read(reinterpret_cast<char*>(&node.flags), sizeof(node.flags));
        if (!in) {
            return false;
        }
        nodes[node.id] = node;
    }

    for (uint64_t i = 0; i < adj_count; ++i) {
        long from_id = 0;
        uint64_t edge_count = 0;
        in.read(reinterpret_cast<char*>(&from_id), sizeof(from_id));
        in.read(reinterpret_cast<char*>(&edge_count), sizeof(edge_count));
        if (!in) {
            return false;
        }
        auto& edges = adjacency_list[from_id];
        edges.reserve(static_cast<size_t>(edge_count));
        for (uint64_t e = 0; e < edge_count; ++e) {
            Edge edge{};
            uint8_t hw = 0;
            uint8_t surf = 0;
            uint8_t lit = 0;
            uint8_t oneway = 0;
            in.read(reinterpret_cast<char*>(&edge.to_node_id), sizeof(edge.to_node_id));
            in.read(reinterpret_cast<char*>(&edge.weight), sizeof(edge.weight));
            in.read(reinterpret_cast<char*>(&hw), sizeof(hw));
            in.read(reinterpret_cast<char*>(&surf), sizeof(surf));
            in.read(reinterpret_cast<char*>(&lit), sizeof(lit));
            in.read(reinterpret_cast<char*>(&oneway), sizeof(oneway));
            if (!in) {
                return false;
            }
            edge.highway_class = static_cast<HighwayClass>(hw);
            edge.surface = static_cast<SurfaceQuality>(surf);
            edge.is_lit = lit != 0;
            edge.is_oneway = oneway != 0;
            edges.push_back(edge);
        }
    }

    rebuildIncomingFromAdjacency();
    index_built = false;
    return true;
}

void Graph::simplifyGraph() {
    struct SimplifyAction {
        long mid_id;
        long n1_id;
        long n2_id;
        size_t n1_edge_index;
        size_t n2_edge_index;
        double w_mid_to_n2;
        double w_mid_to_n1;
    };

    std::vector<long> node_ids;
    node_ids.reserve(nodes.size());
    for (const auto& pair : nodes) {
        node_ids.push_back(pair.first);
    }

    std::vector<SimplifyAction> actions;
    actions.reserve(node_ids.size() / 8);

    for (long node_id : node_ids) {
        auto out_edges_it = adjacency_list.find(node_id);
        auto in_edges_it = incoming_adjacency_list.find(node_id);

        if (out_edges_it == adjacency_list.end() || in_edges_it == incoming_adjacency_list.end()) {
            continue;
        }

        if (out_edges_it->second.size() != 2 || in_edges_it->second.size() != 2) {
            continue;
        }

        const auto& out_edges = out_edges_it->second;
        const auto& in_edges = in_edges_it->second;

        long neighbor1_id = in_edges[0].first;
        long neighbor2_id = in_edges[1].first;
        long out_neighbor1_id = out_edges[0].to_node_id;
        long out_neighbor2_id = out_edges[1].to_node_id;

        if (!((neighbor1_id == out_neighbor1_id && neighbor2_id == out_neighbor2_id) ||
              (neighbor1_id == out_neighbor2_id && neighbor2_id == out_neighbor1_id))) {
            continue;
        }

        size_t n1_edge_index = static_cast<size_t>(-1);
        size_t n2_edge_index = static_cast<size_t>(-1);
        auto n1_it = adjacency_list.find(neighbor1_id);
        auto n2_it = adjacency_list.find(neighbor2_id);
        if (n1_it == adjacency_list.end() || n2_it == adjacency_list.end()) {
            continue;
        }

        for (size_t i = 0; i < n1_it->second.size(); ++i) {
            if (n1_it->second[i].to_node_id == node_id) {
                n1_edge_index = i;
                break;
            }
        }
        for (size_t i = 0; i < n2_it->second.size(); ++i) {
            if (n2_it->second[i].to_node_id == node_id) {
                n2_edge_index = i;
                break;
            }
        }

        if (n1_edge_index == static_cast<size_t>(-1) || n2_edge_index == static_cast<size_t>(-1)) {
            continue;
        }

        const Edge* edge_mid_to_n1 = nullptr;
        const Edge* edge_mid_to_n2 = nullptr;
        if (out_edges[0].to_node_id == neighbor1_id) {
            edge_mid_to_n1 = &out_edges[0];
            edge_mid_to_n2 = &out_edges[1];
        } else {
            edge_mid_to_n1 = &out_edges[1];
            edge_mid_to_n2 = &out_edges[0];
        }

        const double max_merge_weight = 50.0;
        if (edge_mid_to_n1->weight > max_merge_weight || edge_mid_to_n2->weight > max_merge_weight) {
            continue;
        }

        if (edge_mid_to_n1->highway_class != edge_mid_to_n2->highway_class ||
            edge_mid_to_n1->surface != edge_mid_to_n2->surface ||
            edge_mid_to_n1->is_lit != edge_mid_to_n2->is_lit ||
            edge_mid_to_n1->is_oneway != edge_mid_to_n2->is_oneway) {
            continue;
        }

        actions.push_back({
            node_id,
            neighbor1_id,
            neighbor2_id,
            n1_edge_index,
            n2_edge_index,
            edge_mid_to_n2->weight,
            edge_mid_to_n1->weight
        });
    }

    if (actions.empty()) {
        return;
    }

    std::unordered_set<long> to_remove;
    to_remove.reserve(actions.size());
    int simplified_count = 0;

    for (const auto& action : actions) {
        if (to_remove.find(action.mid_id) != to_remove.end()) {
            continue;
        }

        auto mid_it = nodes.find(action.mid_id);
        auto n1_it = adjacency_list.find(action.n1_id);
        auto n2_it = adjacency_list.find(action.n2_id);
        if (mid_it == nodes.end() || n1_it == adjacency_list.end() || n2_it == adjacency_list.end()) {
            continue;
        }

        if (action.n1_edge_index >= n1_it->second.size() || action.n2_edge_index >= n2_it->second.size()) {
            continue;
        }

        auto& edge_n1_to_mid = n1_it->second[action.n1_edge_index];
        auto& edge_n2_to_mid = n2_it->second[action.n2_edge_index];
        if (edge_n1_to_mid.to_node_id != action.mid_id || edge_n2_to_mid.to_node_id != action.mid_id) {
            continue;
        }

        if (edge_n1_to_mid.highway_class != edge_n2_to_mid.highway_class ||
            edge_n1_to_mid.surface != edge_n2_to_mid.surface ||
            edge_n1_to_mid.is_lit != edge_n2_to_mid.is_lit ||
            edge_n1_to_mid.is_oneway != edge_n2_to_mid.is_oneway) {
            continue;
        }

        edge_n1_to_mid.to_node_id = action.n2_id;
        edge_n1_to_mid.weight += action.w_mid_to_n2;

        edge_n2_to_mid.to_node_id = action.n1_id;
        edge_n2_to_mid.weight += action.w_mid_to_n1;

        auto& incoming_n1 = incoming_adjacency_list[action.n1_id];
        auto& incoming_n2 = incoming_adjacency_list[action.n2_id];
        incoming_n1.erase(std::remove_if(incoming_n1.begin(), incoming_n1.end(),
            [&](const auto& edge_pair) {
                return edge_pair.first == action.mid_id;
            }), incoming_n1.end());
        incoming_n2.erase(std::remove_if(incoming_n2.begin(), incoming_n2.end(),
            [&](const auto& edge_pair) {
                return edge_pair.first == action.mid_id;
            }), incoming_n2.end());

        incoming_adjacency_list[action.n2_id].emplace_back(action.n1_id, edge_n1_to_mid);
        incoming_adjacency_list[action.n1_id].emplace_back(action.n2_id, edge_n2_to_mid);

        to_remove.insert(action.mid_id);
        simplified_count++;
    }

    for (long id : to_remove) {
        nodes.erase(id);
        adjacency_list.erase(id);
        incoming_adjacency_list.erase(id);
    }

    if (simplified_count > 0) {
        std::cout << "[Graph] Simplified " << simplified_count << " nodes." << std::endl;
    }
}
