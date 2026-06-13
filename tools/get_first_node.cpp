#include <iostream>
#include <osmium/io/any_input.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/osm/node.hpp>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <osm_file.pbf>\n";
        return 1;
    }

    osmium::io::File input_file{argv[1]};
    osmium::io::Reader reader{input_file, osmium::osm_entity_bits::node};

    while (osmium::memory::Buffer buffer = reader.read()) {
        for (const osmium::Node& node : buffer.select<osmium::Node>()) {
            std::cout << node.id() << "\n";
            reader.close();
            return 0;
        }
    }

    reader.close();
    std::cerr << "No nodes found.\n";
    return 1;
}
