#include <iostream>
#include <chrono>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include "OSMParser.hpp"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <osm_file.pbf>\n";
        return 1;
    }

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
    const auto end = std::chrono::high_resolution_clock::now();
    const auto total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "[Benchmark] Parse-only wall time: " << total_ms << "ms\n";
    std::cout << "[Benchmark] Routing nodes: " << handler.nodes.size() << "\n";
    std::cout << "[Benchmark] Edges: " << handler.edges.size() << "\n";
    return 0;
}
