
#include <iostream>
#include <vector>
#include "../include/Graph.hpp"

int main() {
    std::cout << "[VeloGraph] Initializing Route Engine..." << std::endl;

    // TODO: Load OSM Data
    // Parsing complexity target: O(N) where N is lines in XML
    
    // TODO: Build Spatial Index (Quadtree)
    // Construction: O(N log N)
    
    std::cout << "[VeloGraph] Engine Ready." << std::endl;
    return 0;
}
