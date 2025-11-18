#include "OSMParser.hpp"
#include <iostream>
#include <chrono>

bool OSMParser::parse(const std::string& pbf_file, Graph& graph) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "[OSMParser] Reading PBF file: " << pbf_file << std::endl;
        
        // Create input file reader
        osmium::io::File input_file{pbf_file};
        osmium::io::Reader reader{input_file};
        
        // Create handler
        GraphHandler handler;
        
        // Apply handler to all objects in the file
        osmium::apply(reader, handler);
        reader.close();
        
        auto end_parse = std::chrono::high_resolution_clock::now();
        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_parse - start_time);
        
        std::cout << "[OSMParser] Parsed in " << parse_duration.count() << "ms" << std::endl;
        std::cout << "[OSMParser] Nodes: " << handler.nodes.size() << std::endl;
        std::cout << "[OSMParser] Edges: " << handler.edges.size() << std::endl;
        
        // Build graph from handler data
        std::cout << "[OSMParser] Building graph structure..." << std::endl;
        
        // Add all nodes to graph
        for (const auto& [id, node] : handler.nodes) {
            graph.addNode(node);
        }
        
        // Add all edges to graph
        for (const auto& [from_id, to_id] : handler.edges) {
            // Check if both nodes exist
            if (handler.nodes.find(from_id) != handler.nodes.end() && 
                handler.nodes.find(to_id) != handler.nodes.end()) {
                graph.addEdge(from_id, to_id);
            }
        }
        
        auto end_build = std::chrono::high_resolution_clock::now();
        auto build_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_build - end_parse);
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_build - start_time);
        
        std::cout << "[OSMParser] Graph built in " << build_duration.count() << "ms" << std::endl;
        std::cout << "[OSMParser] Total time: " << total_duration.count() << "ms" << std::endl;
        
        printStats(handler);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[OSMParser] Error: " << e.what() << std::endl;
        return false;
    }
}

void OSMParser::printStats(const GraphHandler& handler) {
    std::cout << "\n[Statistics]" << std::endl;
    std::cout << "  - Nodes processed: " << handler.nodes_processed << std::endl;
    std::cout << "  - Ways processed: " << handler.ways_processed << std::endl;
    std::cout << "  - Unique nodes in graph: " << handler.nodes.size() << std::endl;
    std::cout << "  - Total edges: " << handler.edges.size() << std::endl;
}
