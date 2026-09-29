#include "OSMParser.hpp"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdint>

namespace {
struct CacheHeader {
    char magic[4];
    uint32_t version;
    uint8_t simplify;
    uint64_t pbf_size;
    int64_t pbf_mtime;
};

int64_t toSeconds(std::filesystem::file_time_type t) {
    using namespace std::chrono;
    return duration_cast<seconds>(t.time_since_epoch()).count();
}

std::string sanitizeFilename(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        if (c == '/' || c == '\\' || c == ':' || c == ' ') {
            c = '_';
        }
    }
    return out;
}

std::string cachePathFor(const std::string& pbf_file, bool simplify) {
    const std::string base = sanitizeFilename(std::filesystem::path(pbf_file).filename().string());
    const std::string suffix = simplify ? "_simp" : "_raw";
    return (std::filesystem::path("output") / ("graph_cache_" + base + suffix + ".bin")).string();
}
}

bool OSMParser::parse(const std::string& pbf_file, Graph& graph, bool simplify) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::cout << "[OSMParser] Reading PBF file: " << pbf_file << std::endl;

        std::error_code fs_error;
        const auto pbf_size = std::filesystem::file_size(pbf_file, fs_error);
        const auto pbf_mtime = fs_error ? 0 : toSeconds(std::filesystem::last_write_time(pbf_file, fs_error));
        const auto cache_path = cachePathFor(pbf_file, simplify);
        if (!fs_error && std::filesystem::exists(cache_path)) {
            std::ifstream in(cache_path, std::ios::binary);
            CacheHeader header{};
            in.read(reinterpret_cast<char*>(&header), sizeof(header));
            if (in && header.magic[0] == 'V' && header.magic[1] == 'G' && header.magic[2] == 'P' && header.magic[3] == 'H' &&
                header.version == 3 && header.simplify == (simplify ? 1 : 0) &&
                header.pbf_size == pbf_size && header.pbf_mtime == pbf_mtime) {
                if (graph.deserialize(in)) {
                    std::cout << "[OSMParser] Loaded graph from cache: " << cache_path << std::endl;
                    return true;
                }
            }
        }
        
        // Create input file reader
        osmium::io::File input_file{pbf_file};
        osmium::io::Reader reader{input_file, osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};
        
        using index_type = osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location>;
        using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;
        
        index_type index;
        location_handler_type location_handler{index};
        location_handler.ignore_errors();
        
        // Create our routing extracting handler
        GraphHandler handler;
        
        // Apply handler to all objects in the file
        osmium::apply(reader, location_handler, handler);
        reader.close();
        
        auto end_parse = std::chrono::high_resolution_clock::now();
        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_parse - start_time);
        
        std::cout << "[OSMParser] Parsed in " << parse_duration.count() << "ms" << std::endl;
        std::cout << "[OSMParser] Routing Nodes: " << handler.nodes.size() << std::endl;
        std::cout << "[OSMParser] Edges: " << handler.edges.size() << std::endl;
        
        // Build graph from handler data
        std::cout << "[OSMParser] Building graph structure..." << std::endl;
        graph.buildFrom(handler.nodes, handler.edges);
        
        // Simplify graph
        if (simplify) {
            std::cout << "[OSMParser] Simplifying graph..." << std::endl;
            graph.simplifyGraph();
        } else {
            std::cout << "[OSMParser] Simplify skipped" << std::endl;
        }
        
        auto end_build = std::chrono::high_resolution_clock::now();
        auto build_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_build - end_parse);
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_build - start_time);
        
        std::cout << "[OSMParser] Graph built in " << build_duration.count() << "ms" << std::endl;
        std::cout << "[OSMParser] Total time: " << total_duration.count() << "ms" << std::endl;
        
        printStats(handler);

        std::filesystem::create_directories("output", fs_error);
        std::ofstream out(cache_path, std::ios::binary);
        if (out) {
            CacheHeader header{{'V', 'G', 'P', 'H'}, 3, static_cast<uint8_t>(simplify ? 1 : 0), pbf_size, pbf_mtime};
            out.write(reinterpret_cast<const char*>(&header), sizeof(header));
            if (!graph.serialize(out)) {
                std::cerr << "[OSMParser] Warning: failed to write graph cache." << std::endl;
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[OSMParser] Error: " << e.what() << std::endl;
        return false;
    }
}

void OSMParser::printStats(const GraphHandler& handler) {
    std::cout << "\n[Statistics]" << std::endl;
    std::cout << "  - Nodes processed (routing): " << handler.nodes_processed << std::endl;
    std::cout << "  - Ways processed: " << handler.ways_processed << std::endl;
    std::cout << "  - Unique routes in graph: " << handler.nodes.size() << std::endl;
    std::cout << "  - Total edges: " << handler.edges.size() << std::endl;
}
