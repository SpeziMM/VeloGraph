// Warm graph benchmark: validates every edge independently of the router's success flag.
#include "OSMParser.hpp"
#include "RouteFinder.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int argc, char** argv) try {
    if (argc < 4) {
        std::cerr << "Usage: route_benchmark map.pbf nodes.txt results.csv [iterations=100] [threads=4] [repeats=3]\n";
        return 1;
    }
    const int iterations = argc > 4 ? std::stoi(argv[4]) : 100;
    const unsigned threads = argc > 5 ? std::stoul(argv[5]) : 4;
    const int repeats = argc > 6 ? std::stoi(argv[6]) : 3;
    if (iterations <= 0 || repeats <= 0) throw std::invalid_argument("Counts must be positive");
    Graph graph;
    if (!OSMParser::parse(argv[1], graph, true)) return 1;
    graph.buildSpatialIndex();
    std::ifstream input(argv[2]);
    if (!input) throw std::runtime_error("Cannot read node list");
    std::vector<std::int64_t> nodes;
    for (std::string line; std::getline(input, line); ) {
        if (line.empty() || line.front() == '#') continue;
        nodes.push_back(std::stoll(line));
    }
    if (nodes.empty()) throw std::runtime_error("Empty node list");
    std::ofstream out(argv[3]);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << "engine,seed,repeat,start_node,success,valid,within_tolerance,distance_m,error_m,fitness,wall_ms,path\n";
    out << std::setprecision(12);
    RouteEvaluator evaluator;
    const auto profile = RouteEvaluator::getProfileScenic();
    constexpr double target = 5000;
    for (int repeat = -1; repeat < repeats; ++repeat) { // One unrecorded warmup pass.
        for (unsigned seed : {777u, 778u, 779u}) {
            for (auto node : nodes) {
#ifdef VELOGRAPH_BASELINE
                for (bool greedy : {false}) {
#else
                // Alternate order to reduce systematic drift between engines.
                for (bool greedy : {repeat % 2 == 0, repeat % 2 != 0}) {
#endif
                    RouteFinder finder(graph, evaluator, seed);
                    finder.setThreads(threads);
#ifndef VELOGRAPH_BASELINE
                    finder.setGreedyOnly(greedy);
#endif
                    const auto begin = std::chrono::steady_clock::now();
                    const auto r = finder.findOptimalCycle(node, target, .1, profile, iterations);
                    const auto end = std::chrono::steady_clock::now();
                    const bool success = !r.path.empty();
                    bool valid = r.path.size() >= 3 && r.path.front() == node && r.path.back() == node;
                    double distance = 0;
                    for (size_t i = 0; i + 1 < r.path.size(); ++i) {
                        const auto* edges = graph.getEdges(r.path[i]);
                        bool found = false;
                        if (edges) for (const auto& e : *edges) if (e.to_node_id == r.path[i + 1]) {
                            found = e.highway_class != Graph::HighwayClass::Motorway &&
                                    e.highway_class != Graph::HighwayClass::Trunk;
                            distance += e.weight; break;
                        }
                        valid = valid && found;
                    }
                    valid = valid && std::abs(distance - r.total_distance) < 1e-6;
                    if (repeat < 0) continue;
                    out << (greedy ? "greedy" : "hybrid") << ',' << seed << ',' << repeat << ',' << node
                        << ',' << success << ',' << valid << ',' << (valid && std::abs(distance - target) <= 500)
                        << ',' << (success ? r.total_distance : 0) << ',' << (success ? r.distance_error : 0)
                        << ',' << (success ? r.fitness_score : 0) << ','
                        << std::chrono::duration<double, std::milli>(end - begin).count() << ",\"";
                    for (auto id : r.path) out << id << ' ';
                    out << "\"\n";
                }
            }
        }
    }
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
