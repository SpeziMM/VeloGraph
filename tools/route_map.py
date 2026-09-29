#!/usr/bin/env python3
"""
VeloGraph route map renderer.

Reads a route JSON (as produced by VeloGraph --output_path) and writes a
Leaflet HTML map (CDN and tiles require network access) on real OpenStreetMap tiles. The polyline is
colored by traversal order (start -> end) so out-and-back spurs and segment
structure are visible. Start node is marked; revisited nodes are highlighted.

Usage:
    python3 tools/route_map.py output/demo_route.json output/route_map.html
"""

import argparse
import json
import os
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from collections import Counter

HTML = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<meta name="referrer" content="origin"/>
<title>VeloGraph route</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.css"/>
<script src="https://cdn.jsdelivr.net/npm/leaflet@1.9.4/dist/leaflet.js"></script>
<style>
  html, body {{ margin:0; height:100%; font-family: system-ui, sans-serif; }}
  #map {{ height:100%; }}
  .panel {{ position:absolute; top:12px; right:12px; z-index:1000; background:#fff;
           border:1px solid #ccc; border-radius:8px; padding:12px 14px; width:230px;
           box-shadow:0 1px 4px rgba(0,0,0,.2); font-size:13px; line-height:1.5; }}
  @media (max-width: 600px) {{ .panel {{ width:190px; font-size:12px; max-height:48vh; overflow:auto; }} }}
  button, input {{ font:inherit; }}
  .panel h3 {{ margin:0 0 8px; font-size:15px; font-weight:600; }}
  .panel .row {{ display:flex; justify-content:space-between; }}
  .panel .row span:last-child {{ font-weight:600; }}
  .legend {{ position:absolute; bottom:40px; left:12px; z-index:1000; background:#fff;
            border:1px solid #ccc; border-radius:8px; padding:10px 12px; font-size:12px; }}
  .bar {{ height:10px; width:160px; border-radius:5px;
         background:linear-gradient(90deg,#1d9e75,#ef9f27,#e24b4a); margin:4px 0; }}
  .legend .ends {{ display:flex; justify-content:space-between; width:160px; color:#555; }}
</style>
</head>
<body>
<div id="map"></div>
<div class="panel">
  <h3>VeloGraph · final loop</h3>
  <p>Nearby graph → ellipse waypoints → segment search → refinement → best valid loop.</p>
  <p><strong>{target_status}</strong></p>
  <p id="tile-notice" hidden>Street tiles are disabled for file views. Open this map through the route-map Python tool’s local HTTP viewer.</p>
  <div class="row"><span>Distance</span><span>{dist_km} km</span></div>
  <div class="row"><span>Fitness</span><span>{fitness}</span></div>
  <div class="row"><span>Scenery</span><span>{scenery}</span></div>
  <div class="row"><span>Quality</span><span>{quality}</span></div>
  <div class="row"><span>Traffic pen.</span><span>{traffic}</span></div>
  <div class="row"><span>Turn pen.</span><span>{turns}</span></div>
  <div class="row"><span>Nodes</span><span>{n_nodes}</span></div>
  <div class="row"><span>Revisited</span><span>{n_rev}</span></div>
  <p>Playback follows the final route, not internal search steps.</p>
  <button id="play" type="button">Play route</button>
  <label for="position">Route progress</label>
  <input id="position" type="range" min="0" max="{last_node}" value="0" style="width:100%"/>
  <output id="progress" for="position">0%</output>
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
  map.attributionControl.addAttribution('&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap contributors</a>');
  if (location.protocol === 'http:' || location.protocol === 'https:') {{
    L.tileLayer('https://tile.openstreetmap.org/{{z}}/{{x}}/{{y}}.png', {{
      maxZoom: 19, referrerPolicy: 'origin', updateWhenIdle: true, keepBuffer: 0
    }}).addTo(map);
  }} else {{
    document.getElementById('tile-notice').hidden = false;
  }}

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
  const marker = L.circleMarker(coords[0], {{radius:8, color:'#111', fillColor:'#fff', fillOpacity:1}}).addTo(map);
  const slider = document.getElementById('position');
  const play = document.getElementById('play');
  let timer = null;
  function stopPlayback() {{ clearInterval(timer); timer = null; play.textContent = 'Play route'; }}
  function showProgress() {{
    marker.setLatLng(coords[Number(slider.value)]);
    document.getElementById('progress').textContent = Math.round(100 * slider.value / Math.max(1, coords.length - 1)) + '%';
  }}
  slider.addEventListener('input', () => {{ stopPlayback(); showProgress(); }});
  play.addEventListener('click', () => {{
    if (timer) {{ stopPlayback(); return; }}
    if (Number(slider.value) === coords.length - 1) slider.value = 0;
    play.textContent = 'Pause';
    timer = setInterval(() => {{
      slider.value = Math.min(coords.length - 1, Number(slider.value) + Math.max(1, Math.ceil(coords.length / 60)));
      showProgress();
      if (Number(slider.value) === coords.length - 1) stopPlayback();
    }}, 100);
  }});
</script>
</body>
</html>
"""


def serve_map(path, port=0, open_browser=True):
    """Serve only this generated map on loopback, never the surrounding output files."""
    content = Path(path).read_bytes()

    class MapHandler(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path not in ('/', '/route.html'):
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(content)))
            self.send_header('Referrer-Policy', 'origin')
            self.end_headers()
            self.wfile.write(content)

    with ThreadingHTTPServer(('127.0.0.1', port), MapHandler) as server:
        url = f'http://127.0.0.1:{server.server_port}/route.html'
        print(f'Map viewer: {url} (Ctrl+C to stop)', flush=True)
        if open_browser:
            webbrowser.open(url)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', nargs='?', default='output/demo_route.json')
    parser.add_argument('output', nargs='?', default='output/route_map.html')
    parser.add_argument('--no-open', action='store_true', help='Generate only; with --serve, do not launch a browser')
    parser.add_argument('--serve', action='store_true', help='Serve the map even with --no-open')
    parser.add_argument('--port', type=int, default=0, help='Local HTTP port (default: choose an available port)')
    args = parser.parse_args()
    in_file, out_file = args.input, args.output
    with open(in_file) as source:
        data = json.load(source)
    nodes = data['nodes']
    if not nodes:
        parser.error('The route is empty: generate a successful route before rendering a map.')
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
        last_node=len(nodes) - 1,
        target_status=('Within requested distance tolerance' if data.get('run', {}).get('within_tolerance')
                       else 'Best effort · target tolerance not confirmed'),
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

    os.makedirs(os.path.dirname(out_file) or ".", exist_ok=True)
    with open(out_file, "w") as f:
        f.write(html)
    abs_path = os.path.abspath(out_file)
    print(f"Map written to {abs_path}")
    if args.serve or not args.no_open:
        serve_map(abs_path, args.port, open_browser=not args.no_open)


if __name__ == "__main__":
    main()
