#include "RoutePrecompute.hpp"
#include "GeoUtils.hpp"
#include <queue>
#include <iostream>
#include <chrono>

bool RoutePrecompute::isCyclingEdge(const Graph::Edge& edge, bool relaxed) {
    double min_weight = relaxed ? 4.0 : 8.0;
    if (edge.weight < min_weight) return false;

    switch (edge.highway_class) {
        case Graph::HighwayClass::Motorway:
        case Graph::HighwayClass::Trunk:
            return false;
        case Graph::HighwayClass::Service:
            return relaxed;
        default:
            return true;
    }
}

std::unordered_map<long, double> RoutePrecompute::dijkstraFromStart(
        const Graph& graph, long start_node, double max_graph_distance) {

    struct DNode {
        long id;
        double dist;
        bool operator>(const DNode& o) const { return dist > o.dist; }
    };

    std::unordered_map<long, double> dist;
    std::priority_queue<DNode, std::vector<DNode>, std::greater<DNode>> pq;

    dist[start_node] = 0.0;
    pq.push({start_node, 0.0});

    while (!pq.empty()) {
        auto [node_id, d] = pq.top();
        pq.pop();

        if (d > dist[node_id]) continue;
        if (d > max_graph_distance) continue;

        // Traverse reverse graph: incoming edges give us nodes that CAN reach node_id
        auto incoming = graph.getIncomingEdges(node_id);
        for (const auto& [from_id, edge] : incoming) {
            if (!isCyclingEdge(edge, true)) continue;

            double new_dist = d + edge.weight;
            if (new_dist > max_graph_distance) continue;

            auto it = dist.find(from_id);
            if (it == dist.end() || new_dist < it->second) {
                dist[from_id] = new_dist;
                pq.push({from_id, new_dist});
            }
        }
    }

    return dist;
}

std::unordered_set<long> RoutePrecompute::extractSubgraph(
        const Graph& graph, long start_node, double air_radius) {

    const auto* start = graph.getNode(start_node);
    if (!start) return {};

    auto node_ids = graph.findNodesInRadius(start->lat, start->lon, air_radius);

    std::unordered_set<long> result(node_ids.begin(), node_ids.end());
    result.insert(start_node);

    if (result.size() < 100) {
        node_ids = graph.findNodesInRadius(start->lat, start->lon, air_radius * 2.0);
        result = std::unordered_set<long>(node_ids.begin(), node_ids.end());
        result.insert(start_node);
    }

    return result;
}

void RoutePrecompute::markDeadEnds(
        const Graph& graph,
        const std::unordered_set<long>& subgraph_nodes,
        std::unordered_set<long>& dead_end_nodes) {

    for (long node_id : subgraph_nodes) {
        const auto* edges = graph.getEdges(node_id);
        if (!edges) {
            dead_end_nodes.insert(node_id);
            continue;
        }

        int cycling_degree = 0;
        for (const auto& edge : *edges) {
            if (subgraph_nodes.count(edge.to_node_id) && isCyclingEdge(edge, true)) {
                cycling_degree++;
            }
        }

        if (cycling_degree <= 1) {
            dead_end_nodes.insert(node_id);
        }
    }
}

PrecomputeResult RoutePrecompute::precompute(
        const Graph& graph, long start_node, double target_distance) {

    auto t0 = std::chrono::high_resolution_clock::now();

    PrecomputeResult result;

    double air_radius = target_distance * 0.6;
    result.subgraph_nodes = extractSubgraph(graph, start_node, air_radius);

    double max_graph_dist = target_distance * 1.0;
    result.shortest_home = dijkstraFromStart(graph, start_node, max_graph_dist);

    markDeadEnds(graph, result.subgraph_nodes, result.dead_end_nodes);

    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "[Precompute] " << ms << "ms: "
              << result.subgraph_nodes.size() << " subgraph, "
              << result.shortest_home.size() << " reachable, "
              << result.dead_end_nodes.size() << " dead-ends" << std::endl;

    return result;
}
