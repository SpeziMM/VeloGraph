#ifndef OSMPARSER_HPP
#define OSMPARSER_HPP

#include <cstring>
#include <string>
#include <unordered_map>
#include <osmium/io/any_input.hpp>
#include <osmium/handler.hpp>
#include <osmium/visitor.hpp>
#include <osmium/osm/node.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/osm/tag.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include "Graph.hpp"

class OSMParser {
public:
    // Handler class for processing OSM data efficiently
    class GraphHandler : public osmium::handler::Handler {
    public:
        std::unordered_map<long, Graph::Node> nodes;
        std::vector<Graph::EdgeInput> edges;
        size_t nodes_processed = 0;
        size_t ways_processed = 0;

        // Helper to convert highway tag to enum
        static Graph::HighwayClass parseHighwayClass(const char* highway) {
            if (!highway) return Graph::HighwayClass::Unknown;
            if (!std::strcmp(highway, "motorway") || !std::strcmp(highway, "motorway_link")) return Graph::HighwayClass::Motorway;
            if (!std::strcmp(highway, "trunk") || !std::strcmp(highway, "trunk_link")) return Graph::HighwayClass::Trunk;
            if (!std::strcmp(highway, "primary") || !std::strcmp(highway, "primary_link")) return Graph::HighwayClass::Primary;
            if (!std::strcmp(highway, "secondary") || !std::strcmp(highway, "secondary_link")) return Graph::HighwayClass::Secondary;
            if (!std::strcmp(highway, "tertiary") || !std::strcmp(highway, "tertiary_link")) return Graph::HighwayClass::Tertiary;
            if (!std::strcmp(highway, "unclassified")) return Graph::HighwayClass::Unclassified;
            if (!std::strcmp(highway, "residential")) return Graph::HighwayClass::Residential;
            if (!std::strcmp(highway, "living_street")) return Graph::HighwayClass::LivingStreet;
            if (!std::strcmp(highway, "service")) return Graph::HighwayClass::Service;
            if (!std::strcmp(highway, "track")) return Graph::HighwayClass::Track;
            if (!std::strcmp(highway, "path") || !std::strcmp(highway, "footway") || !std::strcmp(highway, "bridleway")) return Graph::HighwayClass::Path;
            if (!std::strcmp(highway, "cycleway")) return Graph::HighwayClass::Cycleway;
            return Graph::HighwayClass::Unknown;
        }

        // Helper to convert surface tag to enum
        static Graph::SurfaceQuality parseSurfaceQuality(const char* surface, const char* smoothness) {
            // First check smoothness tag (more specific)
            if (smoothness) {
                if (!std::strcmp(smoothness, "excellent") || !std::strcmp(smoothness, "good")) return Graph::SurfaceQuality::Excellent;
                if (!std::strcmp(smoothness, "intermediate")) return Graph::SurfaceQuality::Intermediate;
                if (!std::strcmp(smoothness, "bad")) return Graph::SurfaceQuality::Bad;
                if (!std::strcmp(smoothness, "very_bad") || !std::strcmp(smoothness, "horrible") || !std::strcmp(smoothness, "impassable")) return Graph::SurfaceQuality::VeryBad;
            }
            // Fall back to surface tag
            if (surface) {
                if (!std::strcmp(surface, "asphalt") || !std::strcmp(surface, "paved")) return Graph::SurfaceQuality::Excellent;
                if (!std::strcmp(surface, "concrete") || !std::strcmp(surface, "paving_stones") || !std::strcmp(surface, "sett")) return Graph::SurfaceQuality::Good;
                if (!std::strcmp(surface, "compacted") || !std::strcmp(surface, "fine_gravel")) return Graph::SurfaceQuality::Intermediate;
                if (!std::strcmp(surface, "gravel") || !std::strcmp(surface, "pebblestone")) return Graph::SurfaceQuality::Bad;
                if (!std::strcmp(surface, "unpaved") || !std::strcmp(surface, "dirt") || !std::strcmp(surface, "grass") || !std::strcmp(surface, "sand") || !std::strcmp(surface, "mud")) return Graph::SurfaceQuality::VeryBad;
            }
            return Graph::SurfaceQuality::Unknown;
        }

        // Process ways (roads, paths, etc.)
        void way(const osmium::Way& way) {
            // Filter only roads/paths suitable for routing
            const char* highway = way.tags().get_value_by_key("highway");
            if (!highway) {
                return; // Not a road
            }

            // Bicycle access permission (used to allow otherwise-pedestrian ways)
            const char* bicycle = way.tags().get_value_by_key("bicycle");
            const bool bicycle_allowed = bicycle && (!std::strcmp(bicycle, "yes") || !std::strcmp(bicycle, "designated") || !std::strcmp(bicycle, "permissive"));

            // Skip non-routable ways
            if (!std::strcmp(highway, "proposed") || !std::strcmp(highway, "construction")) {
                return;
            }

            // Filter footways and steps unless explicitly bikeable
            if (!std::strcmp(highway, "footway") || !std::strcmp(highway, "steps") || !std::strcmp(highway, "corridor")) {
                if (!bicycle_allowed) {
                    return;
                }
            }

            // Extract OSM tags for fitness scoring
            Graph::HighwayClass hw_class = parseHighwayClass(highway);
            const char* surface = way.tags().get_value_by_key("surface");
            const char* smoothness = way.tags().get_value_by_key("smoothness");
            Graph::SurfaceQuality surf_quality = parseSurfaceQuality(surface, smoothness);
            
            const char* lit = way.tags().get_value_by_key("lit");
            bool is_lit = lit && (!std::strcmp(lit, "yes") || !std::strcmp(lit, "24/7"));
            
            const char* oneway = way.tags().get_value_by_key("oneway");
            bool is_oneway = oneway && !std::strcmp(oneway, "yes");

            // Extract edges from way nodes
            const auto& node_list = way.nodes();
            
            // Collect valid nodes for this way
            for (size_t i = 0; i < node_list.size(); ++i) {
                const auto& node_ref = node_list[i];
                const auto location = node_ref.location();
                if (location.valid()) {
                    const auto id = static_cast<long>(node_ref.ref());
                    const auto [_, inserted] = nodes.try_emplace(
                        id,
                        Graph::Node{static_cast<long>(id), location.lat(), location.lon(), 0}
                    );
                    if (inserted) {
                        nodes_processed++;
                    }
                }
            }
            
            for (size_t i = 0; i < node_list.size() - 1; ++i) {
                long from = static_cast<long>(node_list[i].ref());
                long to = static_cast<long>(node_list[i + 1].ref());
                
                edges.push_back({from, to, hw_class, surf_quality, is_lit, is_oneway});
                
                // Add reverse edge if bidirectional
                if (!is_oneway) {
                    edges.push_back({to, from, hw_class, surf_quality, is_lit, false});
                }
            }
            ways_processed++;
        }
    };

    // Parse OSM PBF file and populate graph
    static bool parse(const std::string& pbf_file, Graph& graph, bool simplify = true);
    
    // Get parsing statistics
    static void printStats(const GraphHandler& handler);
};

#endif
