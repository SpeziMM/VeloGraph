#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <vector>

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

    void addNode(const Node& n);
    // ... 
};

#endif
