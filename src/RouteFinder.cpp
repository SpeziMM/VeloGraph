#include "RouteFinder.hpp"
#include "GeoUtils.hpp"
#include "WaypointGenerator.hpp"
#include "RoutePrecompute.hpp"
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <limits>
#include <iostream>
#include <cmath>

RouteFinder::RouteFinder(const Graph& graph, const RouteEvaluator& evaluator, unsigned int seed)
    : graph(graph), evaluator(evaluator), rng(seed ? seed : std::random_device{}()) {
}

RouteFinder::RouteResult RouteFinder::findOptimalCycle(long start_node,
                                                       double target_distance,
                                                       double distance_tolerance,
                                                       const RouteEvaluator::UserProfile& profile,
                                                       int num_iterations) {
    RouteResult best_result;
    best_result.total_distance = 0;
    best_result.fitness_score = -1.0;
    best_result.distance_error = std::numeric_limits<double>::max();

    std::cout << "[VeloGraph] Searching for optimal cycle..." << std::endl;
    std::cout << "  - Target distance: " << target_distance << "m" << std::endl;
    std::cout << "  - Iterations: " << num_iterations << std::endl;

    auto precompute = RoutePrecompute::precompute(graph, start_node, target_distance);

    double best_combined = -1.0;

    for (int i = 0; i < num_iterations; ++i) {
        std::cout << "\n--- Iteration " << i + 1 << "/" << num_iterations << " ---" << std::endl;

        std::vector<long> path = buildWaypointRoute(start_node, target_distance, profile, precompute);

        if (path.size() < 3 || path.front() != path.back()) {
            // Fallback to legacy walk
            path = buildCircularRoute(start_node, target_distance, profile);
        }

        if (path.size() < 3 || path.front() != path.back()) {
            std::cout << "  [Optimize] Discarding invalid or incomplete path." << std::endl;
            continue;
        }

        improveWith2Opt(path, profile, 500);

        correctDistance(path, start_node, target_distance, profile);

        auto score = evaluator.evaluateRoute(graph, path, profile);
        double actual_distance = score.total_distance;

        if (actual_distance < target_distance * 0.3) {
            std::cout << "  [Optimize] Discarding degenerate route (" << actual_distance << "m)" << std::endl;
            continue;
        }

        double distance_error = std::abs(actual_distance - target_distance);
        double distance_accuracy = 1.0 - std::min(1.0, distance_error / target_distance);
        double combined = score.total_fitness * 0.6 + distance_accuracy * 0.4;

        std::cout << "  [Optimize] Iteration " << i + 1 << " result: "
                  << "dist=" << actual_distance << "m, "
                  << "fitness=" << score.total_fitness << ", "
                  << "error=" << distance_error << "m, "
                  << "combined=" << combined << std::endl;

        if (combined > best_combined) {
            std::cout << "  [Optimize] *** New best route found! ***" << std::endl;
            best_combined = combined;
            best_result.path = path;
            best_result.total_distance = actual_distance;
            best_result.fitness_score = score.total_fitness;
            best_result.distance_error = distance_error;
            best_result.detailed_score = score;
        }
    }

    return best_result;
}

const Graph::Edge* RouteFinder::getEdge(long from, long to) const {
    const auto* edges = graph.getEdges(from);
    if (!edges) return nullptr;
    for (const auto& edge : *edges) {
        if (edge.to_node_id == to) return &edge;
    }
    return nullptr;
}

double RouteFinder::getPathDistance(const std::vector<long>& path) const {
    double total = 0.0;
    for (size_t i = 0; i < path.size() - 1; ++i) {
        const auto* edge = getEdge(path[i], path[i + 1]);
        if (edge) total += edge->weight;
    }
    return total;
}

bool RouteFinder::isValidCycle(const std::vector<long>& path, long start_node) const {
    if (path.size() < 3) return false;
    return path.front() == start_node && path.back() == start_node;
}

double RouteFinder::calculateTurnAngle(long prev_node, long current_node, long next_node) const {
    const auto* p1 = graph.getNode(prev_node);
    const auto* p2 = graph.getNode(current_node);
    const auto* p3 = graph.getNode(next_node);
    if (!p1 || !p2 || !p3) return 0.0;
    return GeoUtils::turnAngle(p1->lat, p1->lon, p2->lat, p2->lon, p3->lat, p3->lon);
}

double RouteFinder::distance(const Graph::Node* n1, const Graph::Node* n2) const {
    if (!n1 || !n2) return 0.0;
    return GeoUtils::distance(n1->lat, n1->lon, n2->lat, n2->lon);
}

double RouteFinder::distanceFromStart(long node, long start_node) const {
    const auto* n = graph.getNode(node);
    const auto* s = graph.getNode(start_node);
    return distance(n, s);
}

bool RouteFinder::isCyclingEdge(const Graph::Edge& edge, bool at_start_or_end, bool relaxed_mode) const {
    // At start or end of route, allow all paths to leave/return to building area
    if (at_start_or_end) return true;
    
    // Filter out very short edges (often artifacts or connectors)
    // In relaxed mode, allow shorter edges to help escape
    double min_weight = relaxed_mode ? 4.0 : 8.0;
    if (edge.weight < min_weight) return false;
    
    // Filter out motorways and service roads (driveways)
    switch (edge.highway_class) {
        case Graph::HighwayClass::Motorway:
        case Graph::HighwayClass::Trunk:
            return false; // Never cycle on motorways
            
        case Graph::HighwayClass::Service:
            // Only allow service roads in relaxed mode
            return relaxed_mode;
            
        default:
            return true;
    }
}

std::vector<long> RouteFinder::buildCircularRoute(
        long start_node, 
        double target_distance,
        const RouteEvaluator::UserProfile& profile) {
    
    std::vector<long> path;
    path.push_back(start_node);
    
    // Track visit counts to prevent loops
    std::unordered_map<long, int> visit_counts;
    visit_counts[start_node] = 1;
    
    double current_distance = 0.0;
    
    // Minimum air distance at halfway point: (target / PI) * 0.2
    // This ensures the route goes far enough out before returning, but accounts for road tortuosity
    double min_air_distance = (target_distance / M_PI) * 0.2;
    double half_distance = target_distance / 2.0;
    
    long current = start_node;
    double max_air_dist_reached = 0.0;
    
    std::cout << "  [Build] Target=" << target_distance << "m, min_air_distance=" << min_air_distance << "m at halfway" << std::endl;
    
    // Phase 1: Expand OUTWARD until we reach half distance
    // Check min_air_distance constraint at halfway point
    int steps = 0;
    bool halfway_reached = false;
    bool min_distance_ok = false;
    
    while (current_distance < half_distance || (!min_distance_ok && current_distance < target_distance * 0.8)) {
        if (steps++ > 10000) {
            std::cout << "    [Build] Safety break at " << steps << " steps" << std::endl;
            break;
        }
        
        // Also break if we've gone way past target (something went wrong)
        if (current_distance > target_distance) {
            std::cout << "    [Build] Distance limit break at " << current_distance << "m" << std::endl;
            break;
        }
        
        const auto* edges = graph.getEdges(current);
        if (!edges || edges->empty()) {
            std::cout << "    [Build] Dead end at step " << steps << std::endl;
            break;
        }
        
        double current_air_dist = distanceFromStart(current, start_node);
        if (current_air_dist > max_air_dist_reached) {
            max_air_dist_reached = current_air_dist;
        }
        
        // Prevent going too far away (max 60% of target distance)
        // If we are too far, we must turn back
        bool too_far = (current_air_dist > target_distance * 0.6);
        
        // Check halfway constraint
        if (current_distance >= half_distance * 0.9 && !halfway_reached) {
            halfway_reached = true;
            if (current_air_dist >= min_air_distance) {
                min_distance_ok = true;
                std::cout << "    [Build] Halfway check PASSED: air_dist=" << current_air_dist << "m >= " << min_air_distance << "m" << std::endl;
            } else {
                std::cout << "    [Build] Halfway check: air_dist=" << current_air_dist << "m < " << min_air_distance << "m (need to go further)" << std::endl;
            }
        }
        
        // Also pass if we've exceeded the minimum air distance
        if (current_air_dist >= min_air_distance) {
            min_distance_ok = true;
        }
        
        // Detect if we are stuck (high steps, low air distance)
        bool is_stuck = (!min_distance_ok && steps > 50 && max_air_dist_reached < min_air_distance * 0.3);
        
        // Collect valid candidates
        std::vector<std::pair<double, const Graph::Edge*>> candidates;
        
        // Allow all edge types at start (first 10 nodes) - no filtering
        bool at_start_area = (path.size() < 10);
        
        for (const auto& edge : *edges) {
            // Strict loop prevention:
            // 1. Never immediately go back to the previous node (U-turn) unless it's a dead end
            if (path.size() > 1 && edge.to_node_id == path[path.size()-2]) {
                // Only allow if this is a dead end (no other valid edges)
                // We'll handle this in the "no candidates" fallback
                continue;
            }
            
            // 2. Don't visit nodes we've already visited
            if (visit_counts.count(edge.to_node_id) && visit_counts[edge.to_node_id] > 0) continue;
            
            // Always use relaxed mode to allow service roads and short edges
            if (!isCyclingEdge(edge, at_start_area, true)) continue;
            
            double fitness = evaluator.evaluateEdge(edge, profile);
            
            // Calculate direction preference
            double next_air_dist = distanceFromStart(edge.to_node_id, start_node);
            
            // Before reaching min_air_distance: strongly prefer going outward
            double direction_bonus = 0.0;
            if (!min_distance_ok && !too_far) {
                // Prefer edges that increase air distance from start
                direction_bonus = (next_air_dist - current_air_dist) / 50.0;  // Normalize
                
                // If stuck, drastically increase bonus for going outward
                if (is_stuck) {
                     direction_bonus *= 3.0; 
                }
                
                direction_bonus = std::max(-0.2, direction_bonus);  // Small penalty for going inward
            } else if (too_far) {
                // We are too far, prefer going inward
                direction_bonus = (current_air_dist - next_air_dist) / 50.0;
                direction_bonus = std::max(-0.2, direction_bonus);
            }
            
            // Penalize sharp turns and reward straight paths
            if (path.size() >= 2) {
                double turn = calculateTurnAngle(path[path.size()-2], current, edge.to_node_id);
                if (turn < 15.0) {
                    fitness *= 1.2; // Bonus for going straight
                }
                else if (turn > 100) {
                    fitness *= 0.1;  // Severe penalty for >100 deg
                } else if (turn > 70) {
                    fitness *= 0.5;  // Heavy penalty for >70 deg
                } else if (turn > 40) {
                    fitness *= 0.9;  // Slight penalty for >40 deg
                }
            }
            
            // Penalize edges leading to a dead end, unless we are just starting
            const auto* next_edges = graph.getEdges(edge.to_node_id);
            if (!at_start_area && next_edges && next_edges->size() == 1) {
                fitness *= 0.2; // Heavy penalty for dead ends
            }

            // Penalize getting too close to the existing path to avoid small loops
            if (path.size() > 20) {
                const auto* next_node_ptr = graph.getNode(edge.to_node_id);
                if (next_node_ptr) {
                    // Check against non-recent parts of the path
                    for (size_t i = 0; i < path.size() - 15; ++i) {
                        if (distance(next_node_ptr, graph.getNode(path[i])) < 50.0) {
                            fitness *= 0.5; // Penalize if we get within 50m of an old path node
                            break; 
                        }
                    }
                }
            }

            double score = fitness * 0.6 + direction_bonus * 0.4;
            
            // Add randomness for diversity
            std::uniform_real_distribution<double> noise(0.85, 1.15);
            score *= noise(rng);
            
            candidates.emplace_back(score, &edge);
        }
        
        // If no unvisited candidates, allow revisiting with penalty (except start)
        if (candidates.empty()) {
            for (const auto& edge : *edges) {
                if (edge.to_node_id == start_node) continue;  // Don't close cycle prematurely
                
                // Allow U-turns here if necessary
                bool is_uturn = (path.size() > 1 && edge.to_node_id == path[path.size()-2]);
                
                // Don't visit if visited too many times (prevent infinite loops)
                if (visit_counts[edge.to_node_id] >= 2) continue;
                
                if (!isCyclingEdge(edge, at_start_area, true)) continue;
                
                double penalty = 0.1; // Heavy penalty for revisiting
                if (is_uturn) penalty = 0.05; // Even heavier for U-turn
                
                double fitness = evaluator.evaluateEdge(edge, profile) * penalty;
                candidates.emplace_back(fitness, &edge);
            }
        }
        
        if (candidates.empty()) {
            std::cout << "    [Build] No candidates at step " << steps << " (dist=" << current_distance << "m, air=" << current_air_dist << "m)" << std::endl;
            break;
        }
        
        // Sort and pick from top candidates
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // At the very beginning, be much more random to explore different directions
        size_t pick_range;
        if (path.size() < 20) {
             // Pick from top 10 or all candidates if fewer
             pick_range = std::min(size_t(10), candidates.size());
        } else {
             pick_range = std::min(size_t(3), candidates.size());
        }
        
        std::uniform_int_distribution<size_t> picker(0, pick_range - 1);
        const auto* chosen = candidates[picker(rng)].second;
        
        path.push_back(chosen->to_node_id);
        visit_counts[chosen->to_node_id]++;
        current_distance += chosen->weight;
        current = chosen->to_node_id;
    }
    
    std::cout << "  [Build] Outbound: " << path.size() << " nodes, " << current_distance << "m, max_air_dist=" << max_air_dist_reached << "m" << std::endl;
    
    // Phase 2: Find proper return path using A*
    std::cout << "  [Build] Finding return path from node " << current << " to " << start_node << std::endl;
    
    // Only avoid the most recent nodes (last 20% of path or min 10) to prevent immediate backtracking
    // This allows revisiting older parts of the path which are physically distant
    std::unordered_set<long> recent_nodes;
    size_t nodes_to_avoid = std::max(size_t(10), path.size() / 5);
    size_t start_idx = path.size() > nodes_to_avoid ? path.size() - nodes_to_avoid : 0;
    for (size_t i = start_idx; i < path.size(); ++i) {
        recent_nodes.insert(path[i]);
    }
    
    double remaining_budget = target_distance - current_distance;
    auto return_path = findReturnPath(current, start_node, remaining_budget * 2.0, profile, recent_nodes);
    
    if (return_path.empty()) {
        std::cout << "    [Build] A* return path failed, trying without any avoid..." << std::endl;
        // Fallback: allow ALL nodes except immediate predecessor
        std::unordered_set<long> minimal_avoid;
        if (path.size() >= 2) {
            minimal_avoid.insert(path[path.size() - 2]);  // Just avoid immediate predecessor
        }
        return_path = findReturnPath(current, start_node, remaining_budget * 3.0, profile, minimal_avoid);
    }
    
    if (return_path.empty()) {
        std::cout << "    [Build] Return path completely failed!" << std::endl;
        return path;  // Return incomplete path
    }
    
    // Append return path (skip first node as it's already current)
    for (size_t i = 1; i < return_path.size(); ++i) {
        path.push_back(return_path[i]);
    }
    
    double final_dist = getPathDistance(path);
    std::cout << "  [Build] Complete cycle: " << path.size() << " nodes, " << final_dist << "m" << std::endl;
    
    return path;
}

std::vector<long> RouteFinder::findReturnPath(
        long from_node, long to_node,
        double max_distance,
        const RouteEvaluator::UserProfile& profile,
        const std::unordered_set<long>& avoid_nodes) {
    
    // A* search allowing ~10% of visited nodes to be revisited
    // Also consider reverse edges (one-way streets backward)
    struct SearchNode {
        long node_id;
        double g_cost;      // Distance traveled
        double f_cost;      // g + heuristic
        long parent;
        
        bool operator>(const SearchNode& other) const {
            return f_cost > other.f_cost;
        }
    };
    
    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> open_set;
    std::unordered_map<long, double> g_costs;
    std::unordered_map<long, long> parents;
    
    // Heuristic: straight-line distance to target
    auto heuristic = [this, to_node](long node) -> double {
        const auto* n = graph.getNode(node);
        const auto* t = graph.getNode(to_node);
        if (!n || !t) return 0.0;
        double dx = (n->lon - t->lon) * std::cos((n->lat + t->lat) * M_PI / 360.0);
        double dy = n->lat - t->lat;
        return std::sqrt(dx*dx + dy*dy) * 111319.5;
    };
    
    // Create a set of nodes we CAN revisit (~10% of avoid_nodes)
    std::unordered_set<long> can_revisit;
    if (!avoid_nodes.empty()) {
        std::vector<long> avoid_vec(avoid_nodes.begin(), avoid_nodes.end());
        size_t revisit_count = std::max(size_t(1), avoid_vec.size() / 3);  // 33% - allow more revisits
        std::shuffle(avoid_vec.begin(), avoid_vec.end(), rng);
        for (size_t i = 0; i < revisit_count && i < avoid_vec.size(); ++i) {
            can_revisit.insert(avoid_vec[i]);
        }
    }
    
    open_set.push({from_node, 0.0, heuristic(from_node), -1});
    g_costs[from_node] = 0.0;
    parents[from_node] = -1;
    
    int nodes_explored = 0;
    const int MAX_SEARCH_NODES = 5000;  // Limit search - if can't find in 5k nodes, probably won't find
    
    while (!open_set.empty() && nodes_explored < MAX_SEARCH_NODES) {
        auto current = open_set.top();
        open_set.pop();
        nodes_explored++;
        
        if (current.node_id == to_node) {
            // Reconstruct path
            std::vector<long> path;
            long node = to_node;
            while (node != -1) {
                path.push_back(node);
                node = parents[node];
            }
            std::reverse(path.begin(), path.end());
            std::cout << "    [Return] Found path: " << path.size() << " nodes, " << current.g_cost << "m (explored " << nodes_explored << " nodes)" << std::endl;
            return path;
        }
        
        // Skip if we've found a better path to this node
        if (current.g_cost > g_costs[current.node_id] + 0.01) continue;
        
        // Skip if too far
        if (current.g_cost > max_distance) continue;
        
        // Collect all edges: forward edges + reverse edges (for one-way streets)
        std::vector<Graph::Edge> all_edges;
        
        const auto* forward_edges = graph.getEdges(current.node_id);
        if (forward_edges) {
            for (const auto& e : *forward_edges) {
                all_edges.push_back(e);
            }
        }
        
        // Add reverse edges (allows traversing one-way streets backward)
        auto reverse_edges = graph.getIncomingEdges(current.node_id);
        for (const auto& [from_id, rev_edge] : reverse_edges) {
            // Create edge pointing to from_id
            Graph::Edge backward_edge = rev_edge;
            backward_edge.to_node_id = from_id;
            all_edges.push_back(backward_edge);
        }
        
        for (const auto& edge : all_edges) {
            // Check if we can visit this node
            bool is_target = (edge.to_node_id == to_node);
            bool in_avoid = avoid_nodes.find(edge.to_node_id) != avoid_nodes.end();
            bool in_can_revisit = can_revisit.find(edge.to_node_id) != can_revisit.end();
            
            // Allow if: target node, not in avoid list, or in can_revisit set
            if (!is_target && in_avoid && !in_can_revisit) {
                continue;
            }
            
            // Calculate edge cost (lower fitness = higher cost)
            double fitness = evaluator.evaluateEdge(edge, profile);
            double edge_cost = edge.weight * (1.0 + (1.0 - fitness));  // Penalize low-fitness edges
            
            double new_g = current.g_cost + edge_cost;
            
            // Check if this exceeds max distance
            if (current.g_cost > max_distance) {
                continue;
            }
            
            if (g_costs.find(edge.to_node_id) == g_costs.end() || new_g < g_costs[edge.to_node_id]) {
                g_costs[edge.to_node_id] = new_g;
                parents[edge.to_node_id] = current.node_id;
                double f = new_g + heuristic(edge.to_node_id);
                open_set.push({edge.to_node_id, new_g, f, current.node_id});
            }
        }
    }
    
    std::cout << "    [Return] No path found (explored " << nodes_explored << " nodes, queue=" << open_set.size() << ")" << std::endl;
    return {};  // No path found
}

void RouteFinder::improveWith2Opt(
        std::vector<long>& path,
        const RouteEvaluator::UserProfile& profile,
        int max_iterations) {
    
    if (path.size() < 4) return;  // Need at least 4 nodes for 2-opt
    
    auto current_score = evaluator.evaluateRoute(graph, path, profile);
    double best_fitness = current_score.total_fitness;
    
    for (int iter = 0; iter < max_iterations; ++iter) {
        bool improved = false;
        
        // Try all 2-opt swaps
        for (size_t i = 1; i < path.size() - 2; ++i) {
            for (size_t j = i + 1; j < path.size() - 1; ++j) {
                // Create new path with segment [i,j] reversed
                std::vector<long> new_path = path;
                std::reverse(new_path.begin() + i, new_path.begin() + j + 1);
                
                // Check if edges exist in reversed segment
                bool valid = true;
                for (size_t k = 0; k < new_path.size() - 1 && valid; ++k) {
                    if (!getEdge(new_path[k], new_path[k+1])) {
                        valid = false;
                    }
                }
                
                if (!valid) continue;
                
                // Check turn angles in new path
                bool acceptable_turns = true;
                for (size_t k = 1; k < new_path.size() - 1 && acceptable_turns; ++k) {
                    double turn = calculateTurnAngle(new_path[k-1], new_path[k], new_path[k+1]);
                    if (turn > MAX_TURN_ANGLE + 20) {  // Allow slight flexibility in 2-opt
                        acceptable_turns = false;
                    }
                }
                
                if (!acceptable_turns) continue;
                
                // Evaluate new path
                auto new_score = evaluator.evaluateRoute(graph, new_path, profile);
                
                if (new_score.total_fitness > best_fitness) {
                    path = new_path;
                    best_fitness = new_score.total_fitness;
                    improved = true;
                    goto next_iteration; // Restart with the improved path
                }
            }
             if (improved) break; // Move to the next i if we improved
        }

    next_iteration:
        if (!improved) {
            break; // No improvement in a full pass, so we're done
        }
    }

    std::cout << "  [2-Opt] Final fitness: " << best_fitness << std::endl;
}

// ======================== NEW: Waypoint-driven route builder ========================

std::vector<long> RouteFinder::buildWaypointRoute(
        long start_node,
        double target_distance,
        const RouteEvaluator::UserProfile& profile,
        const PrecomputeResult& precompute) {

    auto templ = WaypointGenerator::generate(graph, start_node, target_distance, precompute, rng);

    if (templ.waypoints.size() < 2) {
        std::cout << "  [WP] Too few waypoints (" << templ.waypoints.size() << "), skipping" << std::endl;
        return {};
    }

    std::cout << "  [WP] Generated " << templ.waypoints.size() << " waypoints" << std::endl;

    std::vector<long> waypoint_ids;
    waypoint_ids.push_back(start_node);
    for (const auto& wp : templ.waypoints) {
        waypoint_ids.push_back(wp.node_id);
    }
    waypoint_ids.push_back(start_node);

    int N_segments = static_cast<int>(waypoint_ids.size()) - 1;

    std::vector<long> full_path;
    full_path.push_back(start_node);

    double distance_spent = 0.0;
    std::unordered_set<long> used_nodes;
    used_nodes.insert(start_node);
    int failed_segments = 0;

    for (int seg = 0; seg < N_segments; ++seg) {
        long from = waypoint_ids[seg];
        long to = waypoint_ids[seg + 1];

        double remaining_total = target_distance - distance_spent;
        double remaining_segments = N_segments - seg;

        double this_segment_target = remaining_total / remaining_segments;

        if (seg < N_segments - 1) {
            auto it = precompute.shortest_home.find(to);
            if (it != precompute.shortest_home.end()) {
                double min_home_from_to = it->second;
                double max_this = remaining_total - min_home_from_to;
                this_segment_target = std::min(this_segment_target, max_this);
            }
        }
        this_segment_target = std::max(this_segment_target, 100.0);

        auto segment_path = findSegmentPath(
            from, to, this_segment_target, this_segment_target * 2.0,
            profile, precompute, used_nodes);

        if (segment_path.empty()) {
            segment_path = findReturnPath(from, to, this_segment_target * 3.0, profile, used_nodes);
        }

        if (segment_path.empty()) {
            std::unordered_set<long> empty_avoid;
            segment_path = findReturnPath(from, to, this_segment_target * 5.0, profile, empty_avoid);
        }

        if (segment_path.empty()) {
            failed_segments++;
            if (failed_segments > N_segments / 2) {
                std::cout << "  [WP] Too many failed segments, aborting" << std::endl;
                return {};
            }
            continue;
        }

        for (size_t i = 1; i < segment_path.size(); ++i) {
            full_path.push_back(segment_path[i]);
            used_nodes.insert(segment_path[i]);
        }

        double seg_dist = 0.0;
        for (size_t i = 0; i < segment_path.size() - 1; ++i) {
            const auto* e = getEdge(segment_path[i], segment_path[i + 1]);
            if (e) seg_dist += e->weight;
        }
        distance_spent += seg_dist;
    }

    if (full_path.size() < 3 || full_path.front() != full_path.back()) {
        return {};
    }

    std::cout << "  [WP] Complete cycle: " << full_path.size() << " nodes, "
              << distance_spent << "m" << std::endl;
    return full_path;
}

// ======================== NEW: Budget-aware segment A* ========================

std::vector<long> RouteFinder::findSegmentPath(
        long from_node, long to_node,
        double target_segment_distance,
        double max_segment_distance,
        const RouteEvaluator::UserProfile& profile,
        const PrecomputeResult& precompute,
        const std::unordered_set<long>& avoid_nodes) {

    struct SearchNode {
        long node_id;
        double g_cost;
        double g_dist;
        double f_cost;
        long parent;
        bool operator>(const SearchNode& other) const { return f_cost > other.f_cost; }
    };

    auto air_dist_to_goal = [this, to_node](long node) -> double {
        return distanceFromStart(node, to_node);
    };

    auto heuristic = [&](long node, double g_dist) -> double {
        double h_air = air_dist_to_goal(node);
        double remaining_budget = target_segment_distance - g_dist;
        if (remaining_budget > h_air) {
            return remaining_budget * 0.8;
        }
        return h_air;
    };

    std::priority_queue<SearchNode, std::vector<SearchNode>, std::greater<SearchNode>> open_set;
    std::unordered_map<long, double> g_costs;
    std::unordered_map<long, double> g_dists;
    std::unordered_map<long, long> parents;

    std::unordered_set<long> can_revisit;
    if (!avoid_nodes.empty()) {
        std::vector<long> avoid_vec(avoid_nodes.begin(), avoid_nodes.end());
        size_t revisit_count = std::max(size_t(1), avoid_vec.size() / 3);
        std::shuffle(avoid_vec.begin(), avoid_vec.end(), rng);
        for (size_t i = 0; i < revisit_count && i < avoid_vec.size(); ++i) {
            can_revisit.insert(avoid_vec[i]);
        }
    }

    open_set.push({from_node, 0.0, 0.0, heuristic(from_node, 0.0), -1});
    g_costs[from_node] = 0.0;
    g_dists[from_node] = 0.0;
    parents[from_node] = -1;

    int nodes_explored = 0;
    const int MAX_SEARCH_NODES = 10000;

    while (!open_set.empty() && nodes_explored < MAX_SEARCH_NODES) {
        auto current = open_set.top();
        open_set.pop();
        nodes_explored++;

        if (current.node_id == to_node) {
            std::vector<long> path;
            long node = to_node;
            while (node != -1) {
                path.push_back(node);
                node = parents[node];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        if (current.g_cost > g_costs[current.node_id] + 0.01) continue;
        if (current.g_dist > max_segment_distance) continue;

        auto process_edge = [&](const Graph::Edge& edge, long target_id) {
            if (!RoutePrecompute::isCyclingEdge(edge, true)) return;

            bool is_target = (target_id == to_node);
            if (!is_target) {
                if (precompute.dead_end_nodes.count(target_id)) return;
                bool in_avoid = avoid_nodes.count(target_id);
                bool in_revisit = can_revisit.count(target_id);
                if (in_avoid && !in_revisit) return;
            }

            double new_dist = current.g_dist + edge.weight;
            if (new_dist > max_segment_distance) return;

            double fitness = evaluator.evaluateEdge(edge, profile);
            double edge_cost = edge.weight * (1.0 + (1.0 - fitness));
            double new_g = current.g_cost + edge_cost;

            if (g_costs.find(target_id) == g_costs.end() || new_g < g_costs[target_id]) {
                g_costs[target_id] = new_g;
                g_dists[target_id] = new_dist;
                parents[target_id] = current.node_id;
                double f = new_g + heuristic(target_id, new_dist);
                open_set.push({target_id, new_g, new_dist, f, current.node_id});
            }
        };

        const auto* edges = graph.getEdges(current.node_id);
        if (edges) {
            for (const auto& edge : *edges) {
                process_edge(edge, edge.to_node_id);
            }
        }

        auto incoming = graph.getIncomingEdges(current.node_id);
        for (const auto& [from_id, rev_edge] : incoming) {
            Graph::Edge backward_edge = rev_edge;
            backward_edge.to_node_id = from_id;
            process_edge(backward_edge, from_id);
        }
    }

    return {};
}

// ======================== NEW: Distance correction ========================

void RouteFinder::correctDistance(
        std::vector<long>& path,
        long start_node,
        double target_distance,
        const RouteEvaluator::UserProfile& profile) {

    double actual = getPathDistance(path);
    double error_ratio = (actual - target_distance) / target_distance;

    if (std::abs(error_ratio) <= 0.10) return;

    if (error_ratio > 0.10 && path.size() > 6) {
        // Too long: try to shortcut a random segment
        double best_new_dist = actual;
        std::vector<long> best_path = path;

        for (int attempt = 0; attempt < 10; ++attempt) {
            std::uniform_int_distribution<size_t> idx_dist(1, path.size() - 3);
            size_t i = idx_dist(rng);
            size_t max_j = std::min(i + 20, path.size() - 2);
            if (max_j <= i + 1) continue;
            std::uniform_int_distribution<size_t> j_dist(i + 2, max_j);
            size_t j = j_dist(rng);

            std::unordered_set<long> empty_avoid;
            auto shortcut = findReturnPath(path[i], path[j], actual * 0.5, profile, empty_avoid);
            if (shortcut.empty()) continue;

            std::vector<long> new_path;
            new_path.insert(new_path.end(), path.begin(), path.begin() + i);
            new_path.insert(new_path.end(), shortcut.begin(), shortcut.end());
            new_path.insert(new_path.end(), path.begin() + j + 1, path.end());

            double new_dist = getPathDistance(new_path);
            double new_error = std::abs(new_dist - target_distance);
            double cur_error = std::abs(best_new_dist - target_distance);

            if (new_error < cur_error && new_path.front() == new_path.back()) {
                best_new_dist = new_dist;
                best_path = new_path;
            }
        }

        if (std::abs(best_new_dist - target_distance) < std::abs(actual - target_distance)) {
            auto old_score = evaluator.evaluateRoute(graph, path, profile);
            auto new_score = evaluator.evaluateRoute(graph, best_path, profile);
            if (new_score.total_fitness >= old_score.total_fitness * 0.95) {
                path = best_path;
            }
        }
    } else if (error_ratio < -0.10 && path.size() > 4) {
        // Too short: insert a detour at the farthest point from start
        size_t max_air_idx = 1;
        double max_air = 0;
        for (size_t i = 1; i + 1 < path.size(); ++i) {
            double d = distanceFromStart(path[i], start_node);
            if (d > max_air) { max_air = d; max_air_idx = i; }
        }

        double deficit = target_distance - actual;

        // Find a nearby node not on the path to route through
        const auto* far_node = graph.getNode(path[max_air_idx]);
        if (!far_node) return;

        const auto* edges = graph.getEdges(path[max_air_idx]);
        if (!edges) return;

        for (const auto& edge : *edges) {
            if (!RoutePrecompute::isCyclingEdge(edge, true)) continue;

            std::unordered_set<long> empty_avoid;
            auto detour_out = findReturnPath(path[max_air_idx], edge.to_node_id,
                                             deficit * 0.6, profile, empty_avoid);
            if (detour_out.empty()) continue;

            auto detour_back = findReturnPath(edge.to_node_id, path[max_air_idx],
                                              deficit * 0.6, profile, empty_avoid);
            if (detour_back.empty()) continue;

            double detour_dist = 0;
            for (size_t k = 0; k + 1 < detour_out.size(); ++k) {
                const auto* e = getEdge(detour_out[k], detour_out[k + 1]);
                if (e) detour_dist += e->weight;
            }
            for (size_t k = 0; k + 1 < detour_back.size(); ++k) {
                const auto* e = getEdge(detour_back[k], detour_back[k + 1]);
                if (e) detour_dist += e->weight;
            }

            if (detour_dist < deficit * 0.3 || detour_dist > deficit * 1.5) continue;

            std::vector<long> new_path;
            new_path.insert(new_path.end(), path.begin(), path.begin() + max_air_idx);
            new_path.insert(new_path.end(), detour_out.begin(), detour_out.end());
            for (size_t k = 1; k < detour_back.size(); ++k) {
                new_path.push_back(detour_back[k]);
            }
            new_path.insert(new_path.end(), path.begin() + max_air_idx + 1, path.end());

            if (new_path.front() == new_path.back()) {
                auto old_score = evaluator.evaluateRoute(graph, path, profile);
                auto new_score = evaluator.evaluateRoute(graph, new_path, profile);
                if (new_score.total_fitness >= old_score.total_fitness * 0.95) {
                    path = new_path;
                }
            }
            break;
        }
    }
}
