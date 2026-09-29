"""End-to-end coordinate/area routing on a self-contained directed OSM ring."""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    points = [(49 + .002 * math.cos(i * math.tau / 12), 8 + .003 * math.sin(i * math.tau / 12)) for i in range(12)]
    nodes = ''.join(f'<node id="{5000000000+i}" lat="{lat}" lon="{lon}"/>' for i, (lat, lon) in enumerate(points))
    refs = ''.join(f'<nd ref="{5000000000+i%12}"/>' for i in range(13))
    (root / 'ring.osm').write_text(f'<osm version="0.6">{nodes}<way id="1">{refs}<tag k="highway" v="cycleway"/><tag k="oneway" v="yes"/></way></osm>')
    def run(args, expected=0):
        command = [binary, str(root/'ring.osm'), '--distance', '1400', '--iterations', '20', '--seed', '777', '--threads', '2', '--output_path', str(root/'route.json'), *args]
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=20)
        assert result.returncode == expected, (command, result.stdout, result.stderr)
        return json.loads((root/'route.json').read_text()) if expected == 0 else None
    point = run(['--start', '49.002', '8', '--max_snap', '2'])
    assert point['run']['start_mode'] == 'point' and point['run']['start_offset_m'] < .01
    original = run(['--start_node', '5000000000'])
    assert original['nodes'] == point['nodes'], 'point selection changes identical node search'
    area_args = ['--start', '49', '8', '--start_radius', '400', '--start_candidates', '3']
    area = run(area_args)
    assert area['run']['start_mode'] == 'area' and area['run']['eligible_starts'] == 12
    assert area['run']['searched_starts'] == 3 and area['run']['start_offset_m'] <= 400
    assert area['nodes'][0] == area['nodes'][-1], 'area route must close at chosen start'
    ids = [n['id'] for n in area['nodes']]
    assert all(b == 5000000000 + ((a - 5000000000 + 1) % 12) for a, b in zip(ids, ids[1:])), 'forward edges only'
    assert area['nodes'] == run(area_args)['nodes'], 'area selection must be deterministic'
    run(['--start', '49', '8', '--max_snap', '1'], 1)
    run(['--start', '0', '0', '--start_radius', '1'], 1)
print('Coordinate and area CLI tests passed')
