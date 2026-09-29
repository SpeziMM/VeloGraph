#!/usr/bin/env python3
"""Summarize route_benchmark CSVs; verify repeated outputs before counting cases."""
import argparse
import csv
import statistics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('csv', nargs='+')
    args = parser.parse_args()
    for path in args.csv:
        with open(path) as source:
            rows = list(csv.DictReader(source))
        for engine in sorted({r['engine'] for r in rows}):
            samples = [r for r in rows if r['engine'] == engine]
            cases = {}
            for row in samples:
                key = (row['start_node'], row['seed'])
                stable = {k: v for k, v in row.items() if k not in ('wall_ms', 'repeat')}
                if key in cases and cases[key] != stable:
                    raise ValueError(f'Non-deterministic output: {path}, {key}')
                cases[key] = stable
            values = list(cases.values())
            valid = [r for r in values if r['valid'] == '1']
            times = sorted(float(r['wall_ms']) for r in samples)
            in_band = sum(r['within_tolerance'] == '1' for r in values)
            invalid = sum(r['success'] == '1' and r['valid'] != '1' for r in values)
            print(f'{path}: {engine}, cases={len(values)}, valid={len(valid)}, '
                  f'in_tolerance={in_band}, invalid_reported_success={invalid}')
            if valid:
                print(f'  valid-only mean fitness={statistics.mean(float(r["fitness"]) for r in valid):.4f}, '
                      f'mean error={statistics.mean(float(r["error_m"]) for r in valid):.1f} m')
            print(f'  median={statistics.median(times):.3f} ms, '
                  f'p90={times[int(.9 * (len(times) - 1))]:.3f} ms, samples={len(times)}')


if __name__ == '__main__':
    main()
