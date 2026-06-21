#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include "Graph.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph_cache.bin> [--sample N [seed]] [--dump-nodes]\n";
        std::cerr << "  --sample N [seed]   Print N reproducibly-sampled start-node IDs "
                     "(degree>=3), one per line\n";
        std::cerr << "  --dump-nodes        Print 'id lat lon' for every node "
                     "(input for tools/build_elevation.py)\n";
        return 1;
    }

    const std::string cache_path = argv[1];
    int sample_n = 0;
    unsigned int sample_seed = 1;
    bool dump_nodes = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--sample" && i + 1 < argc) {
            sample_n = std::stoi(argv[++i]);
            if (i + 1 < argc && argv[i + 1][0] != '-') sample_seed = std::stoul(argv[++i]);
        } else if (arg == "--dump-nodes") {
            dump_nodes = true;
        }
    }
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

    // Dump mode: emit 'id lat lon' for every node referenced by the graph.
    if (dump_nodes) {
        std::vector<long> ids;
        for (const auto& [node_id, edges] : graph.getAdjacencyList()) {
            ids.push_back(node_id);
            for (const auto& e : edges) ids.push_back(e.to_node_id);
        }
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        std::cout.precision(8);
        for (long id : ids) {
            const auto* n = graph.getNode(id);
            if (n) std::cout << n->id << " " << n->lat << " " << n->lon << "\n";
        }
        return 0;
    }

    // Sample mode: emit N well-connected node IDs for the eval harness, reproducibly.
    if (sample_n > 0) {
        std::vector<long> candidates;
        for (const auto& [node_id, edges] : graph.getAdjacencyList()) {
            if (edges.size() >= 3) candidates.push_back(node_id);
        }
        std::sort(candidates.begin(), candidates.end());  // stable order before shuffle
        std::mt19937 rng(sample_seed);
        std::shuffle(candidates.begin(), candidates.end(), rng);
        int n = std::min<int>(sample_n, candidates.size());
        for (int i = 0; i < n; ++i) std::cout << candidates[i] << "\n";
        return 0;
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
