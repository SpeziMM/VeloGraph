
#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <charconv>
#include <filesystem>
#include <stdexcept>
#include <string_view>
#include "../include/Graph.hpp"
#include "../include/StartSelection.hpp"
#include "../include/OSMParser.hpp"
#include "../include/RouteEvaluator.hpp"
#include "../include/RouteFinder.hpp"

// Per-run metadata for the eval harness (tools/eval_quality.py reads these).
struct RunMeta {
    NodeId start_node = -1;
    double target_distance = 0.0;
    int iterations = 0;
    unsigned int seed = 0;
    bool success = false;
    std::string start_mode = "node";
    double start_offset_m = 0;
    double start_radius_m = 0;
    double tolerance = .1;
    std::size_t eligible_starts = 0;
    std::size_t searched_starts = 0;
    double wall_time_ms = 0.0;
    std::string engine = "hybrid";
    bool within_tolerance = false;
    double distance_error = 0.0;  // |actual - target|, only meaningful when success
};

void exportPathToJSON(const Graph& graph, const std::vector<NodeId>& path_ids,
                      const std::string& filename,
                      const RunMeta& meta,
                      const RouteEvaluator::RouteScore* score = nullptr) {
    const auto parent = std::filesystem::path(filename).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream out(filename);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << std::fixed << std::setprecision(6);
    out << "{\n";

    out << "  \"run\": {\n";
    out << "    \"engine\": \"" << meta.engine << "\",\n";
    out << "    \"start_mode\": \"" << meta.start_mode << "\",\n";
    out << "    \"start_offset_m\": " << meta.start_offset_m << ",\n";
    out << "    \"start_radius_m\": " << meta.start_radius_m << ",\n";
    out << "    \"distance_tolerance\": " << meta.tolerance << ",\n";
    out << "    \"eligible_starts\": " << meta.eligible_starts << ",\n";
    out << "    \"searched_starts\": " << meta.searched_starts << ",\n";
    out << "    \"within_tolerance\": " << (meta.within_tolerance ? "true" : "false") << ",\n";
    out << "    \"start_node\": " << meta.start_node << ",\n";
    out << "    \"target_distance_m\": " << meta.target_distance << ",\n";
    out << "    \"iterations\": " << meta.iterations << ",\n";
    out << "    \"seed\": " << meta.seed << ",\n";
    out << "    \"success\": " << (meta.success ? "true" : "false") << ",\n";
    out << "    \"wall_time_ms\": " << meta.wall_time_ms << ",\n";
    out << "    \"distance_error_m\": " << meta.distance_error << ",\n";
    out << "    \"node_count\": " << path_ids.size() << "\n";
    out << "  },\n";

    if (score) {
        out << "  \"stats\": {\n";
        out << "    \"total_distance_m\": " << score->total_distance << ",\n";
        out << "    \"fitness_score\": " << score->total_fitness << ",\n";
        out << "    \"safety_score\": " << score->safety_score << ",\n";
        out << "    \"scenery_score\": " << score->scenery_score << ",\n";
        out << "    \"quality_score\": " << score->quality_score << ",\n";
        out << "    \"traffic_penalty\": " << score->traffic_penalty << ",\n";
        out << "    \"turn_penalty\": " << score->turn_penalty << ",\n";
        out << "    \"gradient_penalty\": " << score->gradient_penalty << ",\n";
        out << "    \"total_ascent_m\": " << score->total_ascent_m << "\n";
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
    std::cerr << "Usage: " << prog_name << " <osm_file.pbf> [options]\n"
        "  --start_node <id>         OSM start node (or use --start)\n"
        "  --start <lat> <lon>       Snap coordinates to nearest node\n"
        "  --max_snap <meters>      Maximum coordinate snapping distance (250)\n"
        "  --start_radius <meters>  Search starts in this radius around --start\n"
        "  --start_candidates <n>   Nearest eligible starts to try, 1..64 (8)\n"
        "  --distance <meters>      Target distance; --target_distance is an alias (5000)\n"
        "  --profile <name>         scenic, safe-night, mountain-bike, casual\n"
        "  --iterations <n>         Positive iteration count (10)\n"
        "  --tolerance <fraction>   Prefer routes within this distance tolerance (0.1)\n"
        "  --engine <name>          hybrid or greedy (hybrid)\n"
        "  --seed <n>               0 = random; nonzero = reproducible\n"
        "  --threads <n>            0 = hardware concurrency\n"
        "  --elevation <file>       Elevation sidecar\n"
        "  --weight_turns <w>       Turn penalty [0,1]\n"
        "  --weight_gradient <w>    Slope penalty [0,1]\n"
        "  --no_simplify            Skip degree-2 graph simplification\n"
        "  --output_path <file>     Output JSON (output/sample_path.json)\n";
}

template <typename T>
T parseInteger(std::string_view text) {
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::invalid_argument("Invalid integer: " + std::string(text));
    return value;
}

double parseNumber(const std::string& text) {
    size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(value))
        throw std::invalid_argument("Invalid finite number: " + text);
    return value;
}

int main(int argc, char* argv[]) try {
    std::cout << "[VeloGraph] Initializing Route Engine..." << std::endl;

    // Check for PBF file argument
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    if (std::string_view(argv[1]) == "--help") { printUsage(argv[0]); return 0; }
    std::string pbf_file = argv[1];
    NodeId start_node_id = -1;
    double start_lat = 0.0, start_lon = 0.0;
    bool use_lat_lon = false;
    bool node_requested = false;
    double start_radius = 0.0, max_snap = 250.0;
    unsigned start_candidates = 8;
    bool area_requested = false, snap_requested = false, candidates_requested = false;
    double target_distance = 5000.0;
    std::string profile_name = "scenic";
    std::string output_path = "output/sample_path.json";
    int iterations = 10;
    double tolerance = .1;
    std::string engine = "hybrid";
    bool simplify = true;
    double custom_turn_weight = -1.0;
    double custom_gradient_weight = -1.0;
    std::string elevation_file;
    unsigned int seed = 0;  // 0 = nondeterministic; nonzero = reproducible
    unsigned int threads = 0;  // 0 = auto (hardware_concurrency)

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + arg);
            return argv[++i];
        };
        if (arg == "--start_node") { start_node_id = parseInteger<NodeId>(next()); node_requested = true; }
        else if (arg == "--max_snap") { max_snap = parseNumber(next()); snap_requested = true; }
        else if (arg == "--start_radius") { start_radius = parseNumber(next()); area_requested = true; }
        else if (arg == "--start_candidates") { start_candidates = parseInteger<unsigned>(next()); candidates_requested = true; }
        else if (arg == "--start") {
            start_lat = parseNumber(next()); start_lon = parseNumber(next()); use_lat_lon = true;
        } else if (arg == "--distance" || arg == "--target_distance") target_distance = parseNumber(next());
        else if (arg == "--profile") profile_name = next();
        else if (arg == "--output_path") output_path = next();
        else if (arg == "--iterations") iterations = parseInteger<int>(next());
        else if (arg == "--seed") seed = parseInteger<unsigned>(next());
        else if (arg == "--threads") threads = parseInteger<unsigned>(next());
        else if (arg == "--tolerance") tolerance = parseNumber(next());
        else if (arg == "--engine") engine = next();
        else if (arg == "--elevation") elevation_file = next();
        else if (arg == "--weight_turns" || arg == "--weight_gradient") {
            const auto value = parseNumber(next());
            if (value < 0 || value > 1) throw std::invalid_argument(arg + " must be in [0,1]");
            if (arg == "--weight_turns") custom_turn_weight = value;
            else custom_gradient_weight = value;
        } else if (arg == "--no_simplify") simplify = false;
        else throw std::invalid_argument("Unknown option: " + arg);
    }
    if ((area_requested && (!use_lat_lon || start_radius <= 0)) ||
        max_snap <= 0 || (snap_requested && (!use_lat_lon || area_requested)) ||
        (candidates_requested && !area_requested) || start_candidates == 0 || start_candidates > 64 ||
        (use_lat_lon && node_requested))
        throw std::invalid_argument("Use one start mode; positive radius/snap and 1..64 candidates required");
    if (target_distance <= 0 || iterations <= 0 || tolerance < 0 || tolerance > 1)
        throw std::invalid_argument("Distance and iterations must be positive; tolerance must be in [0,1]");
    if (std::abs(start_lat) > 90 || std::abs(start_lon) > 180)
        throw std::invalid_argument("Coordinates outside latitude/longitude range");
    if (engine != "hybrid" && engine != "greedy") throw std::invalid_argument("Unknown engine: " + engine);
    if (profile_name != "scenic" && profile_name != "safe-night" &&
        profile_name != "mountain-bike" && profile_name != "casual" &&
        profile_name != "night" && profile_name != "mtb" && profile_name != "commuter")
        throw std::invalid_argument("Unknown profile: " + profile_name);

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

    // Optional elevation overlay (sidecar from tools/build_elevation.py)
    if (!elevation_file.empty()) {
        std::cout << "\n[VeloGraph] Loading elevation from " << elevation_file << "..." << std::endl;
        const auto matched = graph.loadElevation(elevation_file);
        if (matched < 0) {
            throw std::runtime_error("Failed to read requested elevation sidecar");
        } else {
            std::cout << "  - Elevation applied to " << matched << " nodes" << std::endl;
        }
    }

    StartSelection::Selection starts;
    if (use_lat_lon) {
        starts = area_requested
            ? StartSelection::inArea(graph, start_lat, start_lon, start_radius, start_candidates)
            : StartSelection::nearest(graph, start_lat, start_lon, max_snap);
        if (starts.candidates.empty())
            throw std::invalid_argument("No eligible start within the requested area or snapping distance");
    } else {
        if (!graph.getNode(start_node_id)) throw std::invalid_argument("Start node does not exist");
        starts = {{{start_node_id, 0.0}}, 1};
    }

    // Get user profile
    RouteEvaluator evaluator;
    auto profile = RouteEvaluator::getProfileByName(profile_name);
    if (custom_turn_weight >= 0.0) {
        profile.weight_turns = custom_turn_weight;
    }
    if (custom_gradient_weight >= 0.0) {
        profile.weight_gradient = custom_gradient_weight;
    }

    std::cout << "\n[VeloGraph] Profile: " << profile.name << std::endl;

    RouteFinder finder(graph, evaluator, seed);
    finder.setThreads(threads);
    finder.setGreedyOnly(engine == "greedy");
    RouteFinder::RouteResult result{};
    auto selected_start = starts.candidates.front();
    const auto t0 = std::chrono::steady_clock::now();
    auto combined = [target_distance](const RouteFinder::RouteResult& route) {
        return .6 * route.fitness_score + .4 * (1.0 - std::min(1.0, route.distance_error / target_distance));
    };
    for (const auto& candidate : starts.candidates) {
        auto route = finder.findOptimalCycle(candidate.id, target_distance, tolerance, profile, iterations);
        if (route.path.empty()) continue;
        const bool in_band = route.distance_error <= target_distance * tolerance;
        const bool best_in_band = !result.path.empty() && result.distance_error <= target_distance * tolerance;
        if (result.path.empty() || (in_band && !best_in_band) ||
            (in_band == best_in_band && combined(route) > combined(result))) {
            result = std::move(route);
            selected_start = candidate;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    RunMeta meta;
    meta.start_node = selected_start.id;
    meta.start_mode = area_requested ? "area" : (use_lat_lon ? "point" : "node");
    meta.start_offset_m = selected_start.offset_m;
    meta.start_radius_m = start_radius;
    meta.tolerance = tolerance;
    meta.eligible_starts = starts.eligible_count;
    meta.searched_starts = starts.candidates.size();
    meta.target_distance = target_distance;
    meta.iterations = iterations;
    meta.seed = finder.seed();
    meta.engine = engine;
    meta.within_tolerance = !result.path.empty() && result.distance_error <= target_distance * tolerance;
    meta.success = !result.path.empty();
    meta.wall_time_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    // distance_error stays DBL_MAX when nothing was found; report 0 on failure.
    meta.distance_error = meta.success ? result.distance_error : 0.0;

    if (meta.success) {
        std::cout << "\n[VeloGraph] Route found!" << std::endl;
        std::cout << "  - Distance: " << result.total_distance << "m" << std::endl;
        std::cout << "  - Fitness: " << result.fitness_score << std::endl;
        std::cout << "  - Total ascent: " << result.detailed_score.total_ascent_m << "m" << std::endl;
        std::cout << "  - Wall time: " << meta.wall_time_ms << "ms" << std::endl;

        exportPathToJSON(graph, result.path, output_path, meta, &result.detailed_score);
        std::cout << "\n[VeloGraph] Route exported to " << output_path << std::endl;
        std::cout << "[VeloGraph] Use 'python3 tools/route_map.py " << output_path << "' to visualize" << std::endl;
    } else {
        std::cerr << "[VeloGraph] Search found no valid cycle; this does not prove none exists." << std::endl;
        // Still emit JSON (empty path, success=false) so the eval harness can record the failure.
        exportPathToJSON(graph, result.path, output_path, meta, nullptr);
        return 2;
    }

    std::cout << "\n[VeloGraph] Engine Finished." << std::endl;
    return 0;
}
 catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
}
