#include "StartSelection.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() try {
    Graph graph;
    graph.addNode({9, 49, 8, 0});
    graph.addNode({3, 49, 8, 0}); // Exact distance tie: ID determines order.
    graph.addNode({5, 49.001, 8, 0});
    graph.addNode({7, 49.0005, 8, 0}); // Only a forbidden outgoing edge.
    graph.addNode({11, 50, 9, 0});
    graph.addNode({13, 51, 9, 0}); // Isolated nodes are not in the spatial index.
    graph.addEdge(9, 5, Graph::HighwayClass::Cycleway);
    graph.addEdge(3, 5, Graph::HighwayClass::Cycleway);
    graph.addEdge(5, 9, Graph::HighwayClass::Cycleway);
    graph.addEdge(7, 5, Graph::HighwayClass::Motorway);
    graph.addEdge(11, 5, Graph::HighwayClass::Cycleway);
    graph.buildSpatialIndex();
    const auto area = StartSelection::inArea(graph, 49, 8, 200, 2);
    check(area.eligible_count == 3 && area.candidates.size() == 2, "eligible and searched counts differ");
    check(area.candidates[0].id == 3 && area.candidates[1].id == 9, "stable tie ordering");
    check(StartSelection::inArea(graph, 0, 0, 10, 2).candidates.empty(), "empty area");
    const auto exact = StartSelection::nearest(graph, 50, 9, 1);
    check(exact.candidates.size() == 1 && exact.candidates.front().id == 11, "exact snap");
    check(StartSelection::nearest(graph, 51, 9, 1).candidates.empty(), "isolated node excluded from snapping");
    check(StartSelection::nearest(graph, 50.01, 9, 1).candidates.empty(), "snap distance limit");
    check(StartSelection::inArea(graph, 49, 8, 1, 8).eligible_count == 2, "radius excludes outer nodes");
    for (std::size_t limit : {0u, 65u}) {
        bool rejected = false;
        try { StartSelection::inArea(graph, 49, 8, 200, limit); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "invalid limit rejected");
    }
    std::cout << "Start selection tests passed\n";
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
}
