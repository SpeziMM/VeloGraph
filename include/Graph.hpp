#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <vector>
#include <unordered_map>
#include <cmath>
#include <iosfwd>

// Using Adjacency List for O(1) traversal average case
// and O(V + E) space complexity.
class Graph {
public:
    // Highway classification for traffic estimation
    enum class HighwayClass : unsigned char {
        Motorway = 0,    // Highest traffic
        Trunk = 1,
        Primary = 2,
        Secondary = 3,
        Tertiary = 4,
        Unclassified = 5,
        Residential = 6,
        LivingStreet = 7,
        Service = 8,
        Track = 9,
        Path = 10,
        Cycleway = 11,   // Lowest traffic (dedicated)
        Unknown = 12
    };

    // Surface quality for comfort scoring
    enum class SurfaceQuality : unsigned char {
        Excellent = 0,   // Smooth asphalt
        Good = 1,        // Concrete, paving stones
        Intermediate = 2,// Older asphalt
        Bad = 3,         // Gravel, compacted
        VeryBad = 4,     // Unpaved, dirt
        Unknown = 5
    };

    struct Node {
        long id;
        double lat;
        double lon;
        // Optimization: Use bit-packing for flags (traffic, surface type)
        unsigned char flags; 
    };

    struct Edge {
        long to_node_id;
        double weight;              // Distance in meters
        HighwayClass highway_class; // Road type for traffic estimation
        SurfaceQuality surface;     // Surface quality
        bool is_lit;                // Street lighting
        bool is_oneway;             // One-way restriction
    };

    struct EdgeInput {
        long from_id;
        long to_id;
        HighwayClass highway_class;
        SurfaceQuality surface;
        bool is_lit;
        bool is_oneway;
    };

private:
    std::unordered_map<long, Node> nodes;
    std::unordered_map<long, std::vector<Edge>> adjacency_list;
    std::unordered_map<long, std::vector<std::pair<long, Edge>>> incoming_adjacency_list; // to_id -> list of (from_id, edge)
    
    // Spatial Index (K-D Tree)
    std::vector<long> spatial_index; // Stores node IDs
    bool index_built = false;

    void buildKDTree(size_t start, size_t end, int depth);
    void findNearestNeighbor(size_t start, size_t end, int depth, double lat, double lon, long& best_node, double& min_dist) const;
    void findNodesInRange(size_t start, size_t end, int depth, double lat, double lon, double radius_meters, std::vector<long>& results) const;

    // Calculate Haversine distance between two nodes (in meters)
    double calculateDistance(const Node& from, const Node& to) const;
    double calculateDistance(double lat1, double lon1, double lat2, double lon2) const;

public:
    void addNode(const Node& n);
    void addEdge(long from_id, long to_id, HighwayClass hw_class = HighwayClass::Unknown, 
                 SurfaceQuality surface = SurfaceQuality::Unknown, 
                 bool is_lit = false, bool is_oneway = false);
    void buildFrom(const std::unordered_map<long, Node>& source_nodes,
                   const std::vector<EdgeInput>& edges);
    
    // Spatial queries
    void buildSpatialIndex();
    const Node* findClosestNode(double lat, double lon) const;
    std::vector<long> findNodesInRadius(double lat, double lon, double radius_meters) const;
    
    // Getters
    const std::unordered_map<long, std::vector<Edge>>& getAdjacencyList() const { return adjacency_list; }
    const Node* getNode(long id) const;
    const std::vector<Edge>* getEdges(long node_id) const;
    
    // Get all nodes that have edges TO this node (for reverse traversal)
    std::vector<std::pair<long, Edge>> getIncomingEdges(long node_id) const;
    
    // Simplify graph by merging degree-2 nodes with compatible edges
    void simplifyGraph();

    bool serialize(std::ostream& out) const;
    bool deserialize(std::istream& in);
    void rebuildIncomingFromAdjacency();

    size_t nodeCount() const { return nodes.size(); }
    size_t edgeCount() const;
};

#endif
