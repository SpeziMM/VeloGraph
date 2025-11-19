#ifndef OSMPARSER_HPP
#define OSMPARSER_HPP

#include <string>
#include <unordered_map>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/osm/tag.hpp>
#include "Graph.hpp"

class OSMParser {
public:
    // Handler class for processing OSM data efficiently
    class GraphHandler : public osmium::handler::Handler {
    public:
        std::unordered_map<osmium::object_id_type, Graph::Node> nodes;
        std::vector<std::pair<osmium::object_id_type, osmium::object_id_type>> edges;
        size_t nodes_processed = 0;
        size_t ways_processed = 0;

        // Process nodes (points with coordinates)
        void node(const osmium::Node& node) {
            Graph::Node n;
            n.id = node.id();
            n.lat = node.location().lat();
            n.lon = node.location().lon();
            n.flags = 0;
            
            nodes[node.id()] = n;
            nodes_processed++;
        }

        // Process ways (roads, paths, etc.)
        void way(const osmium::Way& way) {
            // Filter only roads/paths suitable for routing
            const char* highway = way.tags().get_value_by_key("highway");
            if (!highway) {
                return; // Not a road
            }

            // Skip non-routable ways
            std::string hw_type(highway);
            if (hw_type == "proposed" || hw_type == "construction") {
                return;
            }

            // Extract edges from way nodes
            const auto& node_list = way.nodes();
            for (size_t i = 0; i < node_list.size() - 1; ++i) {
                osmium::object_id_type from = node_list[i].ref();
                osmium::object_id_type to = node_list[i + 1].ref();
                
                edges.emplace_back(from, to);
                
                // Check if bidirectional (most roads are)
                const char* oneway = way.tags().get_value_by_key("oneway");
                if (!oneway || std::string(oneway) != "yes") {
                    edges.emplace_back(to, from); // Add reverse edge
                }
            }
            ways_processed++;
        }
    };

    // Parse OSM PBF file and populate graph
    static bool parse(const std::string& pbf_file, Graph& graph);
    
    // Get parsing statistics
    static void printStats(const GraphHandler& handler);
};

#endif
