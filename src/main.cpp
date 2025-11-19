
#include <iostream>
#include <vector>
#include <fstream>
#include "../include/Graph.hpp"
#include "../include/OSMParser.hpp"

void exportPathToJSON(const Graph& graph, const std::vector<long>& path_ids, const std::string& filename) {
    std::ofstream out(filename);
    out << "{\n";
    out << "  \"nodes\": [\n";
    
    for (size_t i = 0; i < path_ids.size(); ++i) {
        const Graph::Node* node = graph.getNode(path_ids[i]);
        if (node) {
            out << "    {\"id\": " << node->id 
                << ", \"lat\": " << node->lat 
                << ", \"lon\": " << node->lon << "}";
            if (i < path_ids.size() - 1) out << ",";
            out << "\n";
        }
    }
    
    out << "  ]\n";
    out << "}\n";
    out.close();
}

int main(int argc, char* argv[]) {
    std::cout << "[VeloGraph] Initializing Route Engine..." << std::endl;

    // Check for PBF file argument
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <osm_file.pbf> [start_lat start_lon]" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/map.osm.pbf" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/map.osm.pbf 49.0069 8.4037" << std::endl;
        return 1;
    }

    std::string pbf_file = argv[1];
    double start_lat = 0.0;
    double start_lon = 0.0;
    bool use_custom_start = false;

    if (argc >= 4) {
        try {
            start_lat = std::stod(argv[2]);
            start_lon = std::stod(argv[3]);
            use_custom_start = true;
        } catch (const std::exception& e) {
            std::cerr << "[VeloGraph] Invalid coordinates provided." << std::endl;
            return 1;
        }
    }
    
    // Create graph
    Graph graph;
    
    // Parse OSM data
    std::cout << "\n[VeloGraph] Loading OSM data..." << std::endl;
    if (!OSMParser::parse(pbf_file, graph)) {
        std::cerr << "[VeloGraph] Failed to parse OSM file!" << std::endl;
        return 1;
    }
    
    std::cout << "\n[VeloGraph] Graph Statistics:" << std::endl;
    std::cout << "  - Nodes: " << graph.nodeCount() << std::endl;
    std::cout << "  - Edges: " << graph.edgeCount() << std::endl;
    
    // Create a sample path for visualization (first few connected nodes)
    std::vector<long> sample_path;
    
    if (use_custom_start) {
        std::cout << "\n[VeloGraph] Building spatial index..." << std::endl;
        graph.buildSpatialIndex();
        
        std::cout << "[VeloGraph] Finding closest node to " << start_lat << ", " << start_lon << "..." << std::endl;
        const Graph::Node* start_node = graph.findClosestNode(start_lat, start_lon);
        
        if (start_node) {
            std::cout << "[VeloGraph] Found start node: " << start_node->id 
                      << " at " << start_node->lat << ", " << start_node->lon << std::endl;
            
            long current = start_node->id;
            sample_path.push_back(current);
            
            // Follow the path for a few steps
            for (int i = 0; i < 50; ++i) {
                const auto* current_edges = graph.getEdges(current);
                if (current_edges && !current_edges->empty()) {
                    long next = (*current_edges)[0].to_node_id;
                    sample_path.push_back(next);
                    current = next;
                } else {
                    break;
                }
            }
        } else {
            std::cerr << "[VeloGraph] Could not find a start node!" << std::endl;
        }
    } else {
        const auto& nodes = graph.getNodes();
        if (!nodes.empty()) {
            // Find a node with edges
            for (const auto& [node_id, node] : nodes) {
                const auto* edges = graph.getEdges(node_id);
                if (edges && edges->size() >= 2) {
                    sample_path.push_back(node_id);
                    
                    // Follow the path for a few steps
                    long current = node_id;
                    for (int i = 0; i < 50; ++i) {
                        const auto* current_edges = graph.getEdges(current);
                        if (current_edges && !current_edges->empty()) {
                            long next = (*current_edges)[0].to_node_id;
                            sample_path.push_back(next);
                            current = next;
                        } else {
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
        
    if (!sample_path.empty()) {
            std::cout << "\n[VeloGraph] Exporting sample path with " << sample_path.size() << " nodes..." << std::endl;
            exportPathToJSON(graph, sample_path, "sample_path.json");
            std::cout << "[VeloGraph] Sample path exported to sample_path.json" << std::endl;
            std::cout << "[VeloGraph] Use 'python3 visualize_path.py sample_path.json' to visualize" << std::endl;
        }
    
    std::cout << "\n[VeloGraph] Engine Ready." << std::endl;
    return 0;
}
