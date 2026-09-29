#ifndef ROUTEPRECOMPUTE_HPP
#define ROUTEPRECOMPUTE_HPP

#include "Graph.hpp"
#include <unordered_map>
#include <unordered_set>

struct PrecomputeResult {
    std::unordered_map<NodeId, double> shortest_home;
    std::unordered_set<NodeId> subgraph_nodes;
    std::unordered_set<NodeId> dead_end_nodes;
};

class RoutePrecompute {
public:
    static PrecomputeResult precompute(const Graph& graph,
                                       NodeId start_node,
                                       double target_distance);

    static bool isCyclingEdge(const Graph::Edge& edge, bool relaxed = false);

private:
    static std::unordered_map<NodeId, double> dijkstraFromStart(
        const Graph& graph, NodeId start_node, double max_graph_distance);

    static std::unordered_set<NodeId> extractSubgraph(
        const Graph& graph, NodeId start_node, double air_radius);

    static void markDeadEnds(
        const Graph& graph,
        const std::unordered_set<NodeId>& subgraph_nodes,
        std::unordered_set<NodeId>& dead_end_nodes);
};

#endif
