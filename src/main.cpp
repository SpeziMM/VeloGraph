
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include "../include/Graph.hpp"
#include "../include/OSMParser.hpp"
#include "../include/RouteEvaluator.hpp"
#include "../include/RouteFinder.hpp"

void exportPathToJSON(const Graph& graph, const std::vector<long>& path_ids, 
                      const std::string& filename,
                      const RouteEvaluator::RouteScore* score = nullptr) {
    std::ofstream out(filename);
    out << std::fixed << std::setprecision(6);
    out << "{\n";
    
    if (score) {
        out << "  \"stats\": {\n";
        out << "    \"total_distance_m\": " << score->total_distance << ",\n";
        out << "    \"fitness_score\": " << score->total_fitness << ",\n";
        out << "    \"safety_score\": " << score->safety_score << ",\n";
        out << "    \"scenery_score\": " << score->scenery_score << ",\n";
        out << "    \"quality_score\": " << score->quality_score << ",\n";
        out << "    \"traffic_penalty\": " << score->traffic_penalty << ",\n";
        out << "    \"turn_penalty\": " << score->turn_penalty << "\n";
        out << "  },\n";
    }
    
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

void printUsage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <osm_file.pbf> [options]" << std::endl;
    std::cerr << "\nOptions:" << std::endl;
    std::cerr << "  --start <lat> <lon>     Start coordinates" << std::endl;
    std::cerr << "  --distance <meters>     Target route distance (default: 5000)" << std::endl;
    std::cerr << "  --profile <name>        User profile: scenic, safe-night, mountain-bike, casual" << std::endl;
    std::cerr << "  --iterations <n>        Search iterations (default: 100)" << std::endl;
    std::cerr << "  --tolerance <fraction>  Distance tolerance fraction (default: 0.1)" << std::endl;
    std::cerr << "  --seed <n>              RNG seed for reproducible routes (default: 0 = random)" << std::endl;
    std::cerr << "\nCustom weights (override profile, values 0.0-1.0):" << std::endl;
    std::cerr << "  --safety <weight>       Weight for lit streets (default: from profile)" << std::endl;
    std::cerr << "  --scenery <weight>      Weight for scenic/low-traffic (default: from profile)" << std::endl;
    std::cerr << "  --quality <weight>      Weight for surface quality (default: from profile)" << std::endl;
    std::cerr << "  --traffic <weight>      Penalty for high-traffic roads (default: from profile)" << std::endl;
    std::cerr << "  --night                 Enable night mode (boost safety weight)" << std::endl;
    std::cerr << "  --no_simplify           Skip graph simplification (faster startup)" << std::endl;
    std::cerr << "\nExamples:" << std::endl;
    std::cerr << "  " << prog_name << " data/map.osm.pbf --start 49.0069 8.4037 --distance 10000 --profile scenic" << std::endl;
    std::cerr << "  " << prog_name << " data/map.osm.pbf --start 49.0069 8.4037 --safety 0.5 --quality 0.3 --night" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "[VeloGraph] Initializing Route Engine..." << std::endl;

    // Check for PBF file argument
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string pbf_file = argv[1];
    long start_node_id = -1;
    double start_lat = 0.0, start_lon = 0.0;
    bool use_lat_lon = false;
    double target_distance = 5000.0;
    std::string profile_name = "bike_commute";
    std::string output_path = "output/sample_path.json";
    int iterations = 10;
    bool simplify = true;
    double custom_turn_weight = -1.0;
    unsigned int seed = 0;  // 0 = nondeterministic; nonzero = reproducible

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--start_node" && i + 1 < argc) {
            start_node_id = std::stol(argv[++i]);
        } else if (arg == "--start" && i + 2 < argc) {
            start_lat = std::stod(argv[++i]);
            start_lon = std::stod(argv[++i]);
            use_lat_lon = true;
        } else if (arg == "--target_distance" && i + 1 < argc) {
            target_distance = std::stod(argv[++i]);
        } else if (arg == "--distance" && i + 1 < argc) {
            target_distance = std::stod(argv[++i]);
        } else if (arg == "--profile" && i + 1 < argc) {
            profile_name = argv[++i];
        } else if (arg == "--output_path" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--iterations" && i + 1 < argc) {
            iterations = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--weight_turns" && i + 1 < argc) {
            custom_turn_weight = std::stod(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<unsigned int>(std::stoul(argv[++i]));
        } else if (arg == "--no_simplify") {
            simplify = false;
        }
    }

    if (start_node_id == -1 && !use_lat_lon) {
        std::cerr << "Error: --start_node <id> or --start <lat> <lon> is required." << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // Create graph
    Graph graph;
    
    // Parse OSM data
    std::cout << "\n[VeloGraph] Loading OSM data from " << pbf_file << "..." << std::endl;
    if (!OSMParser::parse(pbf_file, graph, simplify)) {
        std::cerr << "[VeloGraph] Failed to parse OSM file!" << std::endl;
        return 1;
    }
    
    std::cout << "\n[VeloGraph] Graph Statistics:" << std::endl;
    std::cout << "  - Nodes: " << graph.nodeCount() << std::endl;
    std::cout << "  - Edges: " << graph.edgeCount() << std::endl;
    
    // Build spatial index
    std::cout << "\n[VeloGraph] Building spatial index..." << std::endl;
    graph.buildSpatialIndex();
    
    // Resolve lat/lon to node if needed
    if (use_lat_lon) {
        const auto* closest = graph.findClosestNode(start_lat, start_lon);
        if (closest) {
            start_node_id = closest->id;
            std::cout << "[VeloGraph] Resolved (" << start_lat << ", " << start_lon
                      << ") to node " << start_node_id << std::endl;
        } else {
            std::cerr << "[VeloGraph] No node found near given coordinates!" << std::endl;
            return 1;
        }
    }

    // Get user profile
    RouteEvaluator evaluator;
    auto profile = RouteEvaluator::getProfileByName(profile_name);
    if (custom_turn_weight >= 0.0) {
        profile.weight_turns = custom_turn_weight;
    }

    std::cout << "\n[VeloGraph] Profile: " << profile.name << std::endl;

    const Graph::Node* start_node = graph.getNode(start_node_id);
    
    if (start_node) {
        std::cout << "[VeloGraph] Found start node: " << start_node->id 
                  << " at " << start_node->lat << ", " << start_node->lon << std::endl;
        
        // Find optimal cycle route
        RouteFinder finder(graph, evaluator, seed);
        auto result = finder.findOptimalCycle(start_node->id, target_distance, 0.1, profile, iterations);
        
        if (!result.path.empty()) {
            std::cout << "\n[VeloGraph] Route found!" << std::endl;
            std::cout << "  - Distance: " << result.total_distance << "m" << std::endl;
            std::cout << "  - Fitness: " << result.fitness_score << std::endl;
            
            exportPathToJSON(graph, result.path, output_path, &result.detailed_score);
            std::cout << "\n[VeloGraph] Route exported to " << output_path << std::endl;
            std::cout << "[VeloGraph] Use 'python3 visualize_path.py " << output_path << "' to visualize" << std::endl;
        } else {
            std::cerr << "[VeloGraph] Could not find a valid cycle route!" << std::endl;
        }
    } else {
        std::cerr << "[VeloGraph] Could not find start node with ID: " << start_node_id << std::endl;
    }
    
    std::cout << "\n[VeloGraph] Engine Finished." << std::endl;
    return 0;
}
