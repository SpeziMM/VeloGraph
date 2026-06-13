#!/usr/bin/env python3
"""
VeloGraph route map renderer.

Reads a route JSON (as produced by VeloGraph --output_path) and writes a
self-contained Leaflet HTML map on real OpenStreetMap tiles. The polyline is
colored by traversal order (start -> end) so out-and-back spurs and segment
structure are visible. Start node is marked; revisited nodes are highlighted.

Usage:
    python3 tools/route_map.py output/demo_route.json output/route_map.html
"""

import json
import sys
import os
import webbrowser
from collections import Counter

HTML = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<title>VeloGraph route</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.js"></script>
<style>
  html, body {{ margin:0; height:100%; font-family: system-ui, sans-serif; }}
  #map {{ height:100%; }}
  .panel {{ position:absolute; top:12px; right:12px; z-index:1000; background:#fff;
           border:1px solid #ccc; border-radius:8px; padding:12px 14px; width:230px;
           box-shadow:0 1px 4px rgba(0,0,0,.2); font-size:13px; line-height:1.5; }}
  .panel h3 {{ margin:0 0 8px; font-size:15px; font-weight:600; }}
  .panel .row {{ display:flex; justify-content:space-between; }}
  .panel .row span:last-child {{ font-weight:600; }}
  .legend {{ position:absolute; bottom:18px; left:12px; z-index:1000; background:#fff;
            border:1px solid #ccc; border-radius:8px; padding:10px 12px; font-size:12px; }}
  .bar {{ height:10px; width:160px; border-radius:5px;
         background:linear-gradient(90deg,#1d9e75,#ef9f27,#e24b4a); margin:4px 0; }}
  .legend .ends {{ display:flex; justify-content:space-between; width:160px; color:#555; }}
</style>
</head>
<body>
<div id="map"></div>
<div class="panel">
  <h3>Route stats</h3>
  <div class="row"><span>Distance</span><span>{dist_km} km</span></div>
  <div class="row"><span>Fitness</span><span>{fitness}</span></div>
  <div class="row"><span>Scenery</span><span>{scenery}</span></div>
  <div class="row"><span>Quality</span><span>{quality}</span></div>
  <div class="row"><span>Traffic pen.</span><span>{traffic}</span></div>
  <div class="row"><span>Turn pen.</span><span>{turns}</span></div>
  <div class="row"><span>Nodes</span><span>{n_nodes}</span></div>
  <div class="row"><span>Revisited</span><span>{n_rev}</span></div>
</div>
<div class="legend">
  Traversal order
  <div class="bar"></div>
  <div class="ends"><span>start</span><span>end</span></div>
</div>
<script>
  var coords = {coords};
  var revisited = {revisited};
  var map = L.map('map');
  L.tileLayer('https://{{s}}.tile.openstreetmap.org/{{z}}/{{x}}/{{y}}.png', {{
    maxZoom: 19, attribution: '&copy; OpenStreetMap contributors'
  }}).addTo(map);

  function lerp(a,b,t){{ return a+(b-a)*t; }}
  function colorAt(t){{
    // green -> amber -> red
    if (t < 0.5) {{ var u=t/0.5;
      return 'rgb('+Math.round(lerp(29,239,u))+','+Math.round(lerp(158,159,u))+','+Math.round(lerp(117,39,u))+')';
    }} else {{ var u=(t-0.5)/0.5;
      return 'rgb('+Math.round(lerp(239,226,u))+','+Math.round(lerp(159,75,u))+','+Math.round(lerp(39,74,u))+')';
    }}
  }}

  for (var i=0; i<coords.length-1; i++) {{
    var t = i/(coords.length-1);
    L.polyline([coords[i], coords[i+1]], {{color: colorAt(t), weight:6, opacity:0.85}}).addTo(map);
  }}

  // revisited node markers (small, to show backtracking)
  revisited.forEach(function(c){{
    L.circleMarker(c, {{radius:4, color:'#a32d2d', weight:1, fillColor:'#e24b4a', fillOpacity:0.7}})
      .bindTooltip('revisited').addTo(map);
  }});

  // start marker
  L.marker(coords[0]).addTo(map).bindPopup('Start / finish');

  map.fitBounds(L.latLngBounds(coords).pad(0.15));
</script>
</body>
</html>
"""


def main():
    in_file = sys.argv[1] if len(sys.argv) > 1 else "output/demo_route.json"
    out_file = sys.argv[2] if len(sys.argv) > 2 else "output/route_map.html"

    data = json.load(open(in_file))
    nodes = data["nodes"]
    stats = data.get("stats", {})

    coords = [[round(n["lat"], 6), round(n["lon"], 6)] for n in nodes]

    ids = [n["id"] for n in nodes]
    counts = Counter(ids)
    rev_ids = {k for k, v in counts.items() if v > 1}
    seen = set()
    revisited = []
    for n in nodes:
        if n["id"] in rev_ids and n["id"] in seen:
            revisited.append([round(n["lat"], 6), round(n["lon"], 6)])
        seen.add(n["id"])

    html = HTML.format(
        coords=json.dumps(coords),
        revisited=json.dumps(revisited),
        dist_km=f"{stats.get('total_distance_m', 0)/1000:.2f}",
        fitness=f"{stats.get('fitness_score', 0):.3f}",
        scenery=f"{stats.get('scenery_score', 0):.2f}",
        quality=f"{stats.get('quality_score', 0):.2f}",
        traffic=f"{stats.get('traffic_penalty', 0):.2f}",
        turns=f"{stats.get('turn_penalty', 0):.2f}",
        n_nodes=len(nodes),
        n_rev=len(rev_ids),
    )

    with open(out_file, "w") as f:
        f.write(html)
    abs_path = os.path.abspath(out_file)
    print(f"Map written to {abs_path}")
    webbrowser.open(f"file://{abs_path}")


if __name__ == "__main__":
    main()
