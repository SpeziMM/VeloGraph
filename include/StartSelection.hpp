#ifndef START_SELECTION_HPP
#define START_SELECTION_HPP

#include "Graph.hpp"
#include <cstddef>
#include <vector>

namespace StartSelection {
struct Candidate {
    NodeId id;
    double offset_m;
};
struct Selection {
    std::vector<Candidate> candidates;
    std::size_t eligible_count = 0;
};

// Requires a built graph spatial index. Offsets are approximate ground meters.
Selection nearest(const Graph& graph, double lat, double lon, double max_snap_m);
// Eligible means an outgoing non-motorway/non-trunk edge, not proven route feasibility.
// Deterministic nearest-first order, with node ID as tie-breaker; at most limit starts.
Selection inArea(const Graph& graph, double lat, double lon, double radius_m, std::size_t limit);
}
#endif
