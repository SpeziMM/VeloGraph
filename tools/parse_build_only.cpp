#include <chrono>
#include <iostream>
#include <string>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include "OSMParser.hpp"
#include "Graph.hpp"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <osm_file.pbf> [sequential|parallel] [nosimplify]\n";
        return 1;
    }

    const std::string mode = (argc >= 3) ? argv[2] : "parallel";
    if (mode != "sequential" && mode != "parallel") {
        std::cerr << "Invalid mode: " << mode << " (use sequential or parallel)\n";
        return 1;
    }

    const bool skip_simplify = (argc == 4 && std::string(argv[3]) == "nosimplify");

    Graph graph;
    const auto start = std::chrono::high_resolution_clock::now();

    osmium::io::File input_file{argv[1]};
    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

    using index_type = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location>;
    using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

    index_type index;
    location_handler_type location_handler{index};
    location_handler.ignore_errors();

    OSMParser::GraphHandler handler;
    osmium::apply(reader, location_handler, handler);
    reader.close();

    if (mode == "sequential") {
        for (const auto& [_, node] : handler.nodes) {
            graph.addNode(node);
        }

        for (const auto& edge : handler.edges) {
            if (handler.nodes.find(edge.from_id) != handler.nodes.end() &&
                handler.nodes.find(edge.to_id) != handler.nodes.end()) {
                graph.addEdge(edge.from_id, edge.to_id, edge.highway_class,
                             edge.surface, edge.is_lit, edge.is_oneway);
            }
        }
    } else {
        graph.buildFrom(handler.nodes, handler.edges);
    }

    if (!skip_simplify) {
        graph.simplifyGraph();
    }

    const auto end = std::chrono::high_resolution_clock::now();

    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[Benchmark] Mode: " << mode << "\n";
    std::cout << "[Benchmark] Parse+build wall time: " << total_ms << "ms\n";
    std::cout << "[Benchmark] Simplify: " << (skip_simplify ? "skipped" : "enabled") << "\n";
    std::cout << "[Benchmark] Nodes: " << graph.nodeCount() << "\n";
    std::cout << "[Benchmark] Edges: " << graph.edgeCount() << "\n";
    return 0;
}
