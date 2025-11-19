#!/usr/bin/env python3
"""
VeloGraph Path Visualizer
Visualizes paths on parsed OSM graphs using matplotlib with map backgrounds
"""

import json
import sys
import webbrowser
import os

try:
    import folium
    HAS_FOLIUM = True
except ImportError:
    HAS_FOLIUM = False

def load_path(filename):
    """Load path data from JSON file"""
    with open(filename, 'r') as f:
        data = json.load(f)
    return data['nodes']

def create_interactive_map(nodes, output_file='path_map.html'):
    """Create an interactive HTML map using folium"""
    if not HAS_FOLIUM:
        print("Warning: folium not installed.")
        print("Install with: pip3 install folium")
        return
    
    if not nodes:
        print("Error: No nodes to visualize")
        return
    
    # Extract coordinates
    lats = [node['lat'] for node in nodes]
    lons = [node['lon'] for node in nodes]
    
    # Calculate center point
    center_lat = (min(lats) + max(lats)) / 2
    center_lon = (min(lons) + max(lons)) / 2
    
    # Create map
    m = folium.Map(location=[center_lat, center_lon], zoom_start=14, 
                   tiles='OpenStreetMap', control_scale=True)
    
    # Add path as a polyline
    path_coords = [[node['lat'], node['lon']] for node in nodes]
    folium.PolyLine(path_coords, color='blue', weight=4, opacity=0.8, 
                    popup=f'Path with {len(nodes)} nodes').add_to(m)
    
    # Add start marker
    folium.Marker(
        [lats[0], lons[0]],
        popup=f'Start (Node {nodes[0]["id"]})',
        icon=folium.Icon(color='green', icon='play', prefix='fa'),
        tooltip='Start Point'
    ).add_to(m)
    
    # Add end marker
    folium.Marker(
        [lats[-1], lons[-1]],
        popup=f'End (Node {nodes[-1]["id"]})',
        icon=folium.Icon(color='red', icon='stop', prefix='fa'),
        tooltip='End Point'
    ).add_to(m)
    
    # Add intermediate nodes as circle markers
    for i, node in enumerate(nodes[1:-1], 1):
        folium.CircleMarker(
            [node['lat'], node['lon']],
            radius=3,
            color='blue',
            fill=True,
            fillColor='blue',
            fillOpacity=0.6,
            popup=f'Node {node["id"]} (#{i})',
            tooltip=f'Node #{i}'
        ).add_to(m)
    
    # Add statistics
    stats_html = f"""
    <div style="position: fixed; 
                top: 10px; right: 10px; width: 200px; height: auto; 
                background-color: white; border:2px solid grey; z-index:9999; 
                font-size:14px; padding: 10px">
    <h4 style="margin-top:0">Path Statistics</h4>
    <b>Nodes:</b> {len(nodes)}<br>
    <b>Lat range:</b> {min(lats):.4f} - {max(lats):.4f}<br>
    <b>Lon range:</b> {min(lons):.4f} - {max(lons):.4f}
    </div>
    """
    m.get_root().html.add_child(folium.Element(stats_html))
    
    # Save map
    m.save(output_file)
    print(f"Interactive map saved to {output_file}")
    
    # Open in browser automatically
    abs_path = os.path.abspath(output_file)
    print(f"Opening map in browser...")
    webbrowser.open(f'file://{abs_path}')

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
        print("Usage: python3 visualize_path.py <path.json> [output.html]")
        print("\nExamples:")
        print("  python3 visualize_path.py sample_path.json")
        print("  python3 visualize_path.py sample_path.json my_route.html")
        print("\nInstall required library:")
        print("  pip3 install folium")
        sys.exit(1)
    
    if not HAS_FOLIUM:
        print("Error: folium is not installed.")
        print("Install with: pip3 install folium")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2] if len(sys.argv) > 2 else 'path_map.html'
    
    # Ensure output file has .html extension
    if not output_file.endswith('.html'):
        output_file += '.html'
    
    print(f"Loading path from {input_file}...")
    nodes = load_path(input_file)
    print(f"Loaded {len(nodes)} nodes\n")
    
    print("Generating interactive map...")
    create_interactive_map(nodes, output_file)

if __name__ == '__main__':
    main()
