#!/usr/bin/env python3
"""
VeloGraph Path Visualizer
Visualizes paths on parsed OSM graphs using matplotlib
"""

import json
import sys
import matplotlib.pyplot as plt
import numpy as np

def load_path(filename):
    """Load path data from JSON file"""
    with open(filename, 'r') as f:
        data = json.load(f)
    return data['nodes']

def visualize_path(nodes, output_file=None):
    """Visualize a path on a map using matplotlib"""
    if not nodes:
        print("Error: No nodes to visualize")
        return
    
    # Extract coordinates
    lats = [node['lat'] for node in nodes]
    lons = [node['lon'] for node in nodes]
    
    # Create figure
    fig, ax = plt.subplots(figsize=(12, 10))
    
    # Plot the path
    ax.plot(lons, lats, 'b-', linewidth=2, label='Path', alpha=0.7)
    
    # Mark start and end points
    ax.plot(lons[0], lats[0], 'go', markersize=12, label='Start', zorder=5)
    ax.plot(lons[-1], lats[-1], 'ro', markersize=12, label='End', zorder=5)
    
    # Plot intermediate nodes
    ax.plot(lons[1:-1], lats[1:-1], 'b.', markersize=4, alpha=0.5)
    
    # Add labels and formatting
    ax.set_xlabel('Longitude', fontsize=12)
    ax.set_ylabel('Latitude', fontsize=12)
    ax.set_title(f'VeloGraph Path Visualization ({len(nodes)} nodes)', fontsize=14, fontweight='bold')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    
    # Add statistics
    stats_text = f"Nodes: {len(nodes)}\n"
    stats_text += f"Lat range: [{min(lats):.4f}, {max(lats):.4f}]\n"
    stats_text += f"Lon range: [{min(lons):.4f}, {max(lons):.4f}]"
    ax.text(0.02, 0.98, stats_text, transform=ax.transAxes,
            verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
            fontfamily='monospace', fontsize=9)
    
    # Adjust layout
    plt.tight_layout()
    
    # Save or show
    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"Path visualization saved to {output_file}")
    else:
        plt.show()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 visualize_path.py <path.json> [output.png]")
        print("Example: python3 visualize_path.py sample_path.json")
        print("         python3 visualize_path.py sample_path.json output.png")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else None
    
    print(f"Loading path from {input_file}...")
    nodes = load_path(input_file)
    print(f"Loaded {len(nodes)} nodes")
    
    print("Generating visualization...")
    visualize_path(nodes, output_file)
    
    if not output_file:
        print("Displaying visualization (close window to exit)")

if __name__ == '__main__':
    main()
