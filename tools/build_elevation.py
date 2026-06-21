#!/usr/bin/env python3
"""Build a VeloGraph elevation sidecar from a DEM raster (or a synthetic surface).

The C++ engine treats elevation as a runtime overlay: it does NOT live in the graph
cache. This tool samples an elevation per graph node and writes a compact binary the
engine loads via `--elevation <file>`.

Pipeline:
    # 1) dump node coordinates from a graph cache
    ./build/inspect_graph output/graph_cache_<pbf>_simp.bin --dump-nodes > nodes.txt

    # 2a) sample a real DEM (GeoTIFF / .hgt) -- needs `pip install rasterio`
    python3 tools/build_elevation.py --nodes nodes.txt --dem srtm_karlsruhe.tif \\
        --out output/elevation.bin

    # 2b) or synthesize a terrain for testing the pipeline end-to-end (no DEM needed)
    python3 tools/build_elevation.py --nodes nodes.txt --synthetic \\
        --out output/elevation.bin

    # 3) route with hill avoidance
    ./build/VeloGraph data/foo.osm.pbf --start_node <id> --elevation output/elevation.bin \\
        --weight_gradient 0.5

Sidecar format (little-endian): magic 'VGEL', uint32 version=1, uint64 count,
then count * (int64 node_id, float32 elevation_m).
"""
import argparse
import math
import os
import struct
import sys


def read_nodes(path):
    """Yield (id, lat, lon) from an 'id lat lon' dump (file or '-' for stdin)."""
    f = sys.stdin if path == "-" else open(path)
    try:
        for line in f:
            parts = line.split()
            if len(parts) >= 3:
                yield int(parts[0]), float(parts[1]), float(parts[2])
    finally:
        if f is not sys.stdin:
            f.close()


def synthetic_elevation(lat, lon):
    """Deterministic rolling terrain for pipeline testing (meters).

    Smooth km-scale hills plus a broad west->east rise (loosely echoing the Rhine
    valley climbing into the Black Forest). No external data required.
    """
    base = 110.0
    macro = 220.0 * (lon - 8.2)                      # gentle regional tilt
    hills = (45.0 * math.sin(lat * 220.0)
             + 40.0 * math.cos(lon * 240.0)
             + 25.0 * math.sin((lat + lon) * 380.0)) # finer rolling features
    return max(0.0, base + macro + hills)


def sample_dem(nodes, dem_paths):
    """Sample node elevations from one or more DEM tiles (assumed EPSG:4326 lon/lat).

    Nodes are grouped by 1deg tile so each raster is sampled in a single batch; nodes
    with no covering tile get 0.0.
    """
    try:
        import rasterio
    except ImportError:
        sys.exit("rasterio not installed. `pip install rasterio`, or use --synthetic.")
    from collections import defaultdict

    ds_by_key = {}
    for p in dem_paths:
        ds = rasterio.open(p)
        b = ds.bounds
        # Copernicus COGs carry a half-pixel border (bottom ~= 48.9999), so round to the
        # integer SW corner rather than floor.
        key = (round(b.bottom), round(b.left))
        ds_by_key[key] = ds

    groups = defaultdict(list)
    for (nid, lat, lon) in nodes:
        groups[(math.floor(lat), math.floor(lon))].append((nid, lat, lon))

    for key, grp in groups.items():
        ds = ds_by_key.get(key)
        if ds is None:
            for (nid, _lat, _lon) in grp:
                yield nid, 0.0
            continue
        nodata = ds.nodata
        coords = [(lon, lat) for (_nid, lat, lon) in grp]
        for (nid, _lat, _lon), val in zip(grp, ds.sample(coords, indexes=1)):
            e = float(val[0])
            if nodata is not None and e == nodata:
                e = 0.0
            yield nid, e


def write_sidecar(pairs, out_path):
    pairs = list(pairs)
    with open(out_path, "wb") as f:
        f.write(b"VGEL")
        f.write(struct.pack("<IQ", 1, len(pairs)))
        for nid, elev in pairs:
            f.write(struct.pack("<qf", nid, elev))
    return len(pairs)


def main():
    ap = argparse.ArgumentParser(description="Build a VeloGraph elevation sidecar")
    ap.add_argument("--nodes", required=True, help="'id lat lon' dump, or '-' for stdin")
    ap.add_argument("--dem", nargs="+", help="DEM raster tile(s) (GeoTIFF/.hgt) sampled via rasterio")
    ap.add_argument("--dem-dir", help="directory of DEM tiles (*.tif) to sample, e.g. data/dem")
    ap.add_argument("--synthetic", action="store_true",
                    help="generate a deterministic test terrain instead of sampling a DEM")
    ap.add_argument("--out", required=True, help="output sidecar path")
    args = ap.parse_args()

    dem_paths = list(args.dem) if args.dem else []
    if args.dem_dir:
        import glob
        dem_paths += sorted(glob.glob(os.path.join(args.dem_dir, "*.tif")))
    if not dem_paths and not args.synthetic:
        sys.exit("Provide --dem <tiles>, --dem-dir <dir>, or --synthetic.")

    nodes = list(read_nodes(args.nodes))
    if not nodes:
        sys.exit("No nodes read.")

    if args.synthetic:
        pairs = ((nid, synthetic_elevation(lat, lon)) for (nid, lat, lon) in nodes)
    else:
        print(f"Sampling {len(nodes)} nodes from {len(dem_paths)} DEM tile(s)...")
        pairs = sample_dem(nodes, dem_paths)

    n = write_sidecar(pairs, args.out)
    src = "synthetic" if args.synthetic else f"{len(dem_paths)} DEM tile(s)"
    print(f"Wrote {n} node elevations to {args.out} ({src})")


if __name__ == "__main__":
    main()
