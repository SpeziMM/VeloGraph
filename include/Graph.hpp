#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <cmath>

// Using Adjacency List for O(1) traversal average case
// and O(V + E) space complexity.
class Graph {
public:
    struct Node {
        long id;
        double lat;
        double lon;
        // Optimization: Use bit-packing for flags (traffic, surface type)
        unsigned char flags; 
    };

    struct Edge {
        long to_node_id;
        double weight; // Distance or cost
    };

private:
    std::unordered_map<long, Node> nodes;
    std::unordered_map<long, std::vector<Edge>> adjacency_list;
    
    // Calculate Haversine distance between two nodes (in meters)
    double calculateDistance(const Node& from, const Node& to) const;

public:
    void addNode(const Node& n);
    void addEdge(long from_id, long to_id);
    
    // Getters
    const std::unordered_map<long, Node>& getNodes() const { return nodes; }
    const std::unordered_map<long, std::vector<Edge>>& getAdjacencyList() const { return adjacency_list; }
    const Node* getNode(long id) const;
    const std::vector<Edge>* getEdges(long node_id) const;
    
    size_t nodeCount() const { return nodes.size(); }
    size_t edgeCount() const;
};

#endif
