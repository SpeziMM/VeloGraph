#include "Graph.hpp"
#include "GeoUtils.hpp"
#include "RouteFinder.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

Graph ring(bool close) {
    Graph g;
    constexpr NodeId base = 5000000000LL;
    for (int i = 0; i < 12; ++i) {
        const double a = i * 2 * GeoUtils::pi / 12;
        g.addNode({base + i, 49 + .002 * std::cos(a), 8 + .003 * std::sin(a), 0});
    }
    for (int i = 0; i < (close ? 12 : 11); ++i)
        g.addEdge(base + i, base + (i + 1) % 12, Graph::HighwayClass::Cycleway,
                  Graph::SurfaceQuality::Excellent, true, true);
    g.buildSpatialIndex();
    return g;
}

int main() {
    try {
        constexpr NodeId start = 5000000000LL;
        auto g = ring(true);
        const auto& incoming = g.getIncomingEdges(start);
        check(incoming.size() == 1 && incoming[0].first == start + 11 &&
              incoming[0].second.to_node_id == start, "incoming view preserves directed edge");
        check(g.getIncomingEdges(-999).empty(), "missing incoming adjacency");
        std::stringstream cache;
        check(g.serialize(cache), "serialize");
        Graph copy;
        check(copy.deserialize(cache) && copy.getNode(start), "64-bit ID cache round trip");
        check(copy.edgeCount() == g.edgeCount(), "cache edges preserved");
        RouteEvaluator evaluator;
        const auto profile = RouteEvaluator::getProfileScenic();
        RouteFinder serial(g, evaluator, 777), parallel(g, evaluator, 777);
        serial.setThreads(1);
        parallel.setThreads(4);
        const auto a = serial.findOptimalCycle(start, 1400, .1, profile, 20);
        const auto b = parallel.findOptimalCycle(start, 1400, .1, profile, 20);
        check(!a.path.empty(), "directed ring must produce a loop");
        check(a.path == b.path && a.total_distance == b.total_distance, "thread determinism");
        double distance = 0;
        for (size_t i = 0; i + 1 < a.path.size(); ++i) {
            const auto* edges = g.getEdges(a.path[i]);
            bool found = false;
            if (edges) for (const auto& e : *edges) if (e.to_node_id == a.path[i + 1]) {
                found = true; distance += e.weight; break;
            }
            check(found, "every route step must be a forward edge");
        }
        check(std::abs(distance - a.total_distance) < 1e-8, "distance includes every edge");
        auto chain = ring(false);
        RouteFinder no_cycle(chain, evaluator, 777);
        no_cycle.setThreads(2);
        const auto failed = no_cycle.findOptimalCycle(start, 1400, .1, profile, 20);
        check(failed.path.empty(), "one-way chain must not invent a reverse return");
        check(failed.detailed_score.total_distance == 0, "failure score is initialized");
        for (double target : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
            bool rejected = false;
            try { serial.findOptimalCycle(start, target, .1, profile, 1); }
            catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "invalid target rejected");
        }
        bool invalid_score_rejected = false;
        try { evaluator.evaluateRoute(g, {start, start + 11}, profile); }
        catch (const std::invalid_argument&) { invalid_score_rejected = true; }
        check(invalid_score_rejected, "scorer must not silently skip missing edges");
        bool rejected = false;
        try { serial.findOptimalCycle(start, 1400, .1, profile, -1); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "negative iteration count rejected before allocation");
        std::cout << "All core tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
