#include <iostream>
#include <fstream>
#include <string>
#include "Graph.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <graph_cache.bin>\n";
        return 1;
    }

    const std::string cache_path = argv[1];
    std::ifstream in(cache_path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open cache: " << cache_path << "\n";
        return 1;
    }

    struct CacheHeader {
        char magic[4];
        uint32_t version;
        uint8_t simplify;
        uint64_t pbf_size;
        int64_t pbf_mtime;
    };

    CacheHeader header{};
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in) {
        std::cerr << "Failed to read cache header\n";
        return 1;
    }

    Graph graph;
    if (!graph.deserialize(in)) {
        std::cerr << "Failed to deserialize graph\n";
        return 1;
    }

    long best_node = -1;
    size_t best_degree = 0;
    long best_weight_node = -1;
    double best_weight_sum = 0.0;
    long best_long_edge_node = -1;
    double best_long_edge_sum = 0.0;

    for (const auto& [node_id, edges] : graph.getAdjacencyList()) {
        if (edges.size() > best_degree) {
            best_degree = edges.size();
            best_node = node_id;
        }

        double sum_weight = 0.0;
        for (const auto& edge : edges) {
            sum_weight += edge.weight;
        }
        if (sum_weight > best_weight_sum) {
            best_weight_sum = sum_weight;
            best_weight_node = node_id;
        }

        if (edges.size() >= 4 && sum_weight > best_long_edge_sum) {
            best_long_edge_sum = sum_weight;
            best_long_edge_node = node_id;
        }
    }

    std::cout << "max_degree_node " << best_node << " " << best_degree << "\n";
    std::cout << "max_weight_node " << best_weight_node << " " << best_weight_sum << "\n";
    std::cout << "max_weight_degree4_node " << best_long_edge_node << " " << best_long_edge_sum << "\n";

    const long start_node = 163816;
    const auto* edges = graph.getEdges(start_node);
    std::cout << "start_node_degree " << start_node << " " << (edges ? edges->size() : 0) << "\n";

    return 0;
}
