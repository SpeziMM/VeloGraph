"""Invalid inputs must fail before loading OSM and without aborting."""
import subprocess
import sys

binary = sys.argv[1]
assert subprocess.run([binary, '--help'], capture_output=True).returncode == 0
for arguments in [
    ['--distance', 'nan'], ['--distance', 'inf'], ['--distance', '-1'],
    ['--iterations', '-1'], ['--iterations', '12oops'], ['--threads', '-1'],
    ['--threads', '999999999999999999999'], ['--seed', '-2'],
    ['--weight_gradient', '1.1'], ['--weight_turns', '-.2'],
    ['--distance'], ['--engine', 'unknown'], ['--profile', 'unknown'],
    ['--start', '91', '8'], ['--unknown'],
]:
    p = subprocess.run([binary, 'nonexistent.pbf', *arguments], capture_output=True, text=True, timeout=5)
    assert p.returncode == 1, (arguments, p.returncode, p.stderr)
    assert 'Error:' in p.stderr and 'Loading OSM' not in p.stdout, (arguments, p.stdout, p.stderr)
print('CLI rejection tests passed')
