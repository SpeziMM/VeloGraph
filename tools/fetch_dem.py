#!/usr/bin/env python3
"""Download Copernicus GLO-30 DEM tiles covering an area.

Copernicus GLO-30 (~30 m) is hosted as open data on AWS S3 — no account or API key.
Tiles are 1deg x 1deg COG GeoTIFFs named by their SW corner. This fetches every tile
needed to cover a bounding box (or the extent of a node dump) into a directory that
tools/build_elevation.py then samples.

Usage:
    # from an explicit bbox (min_lon min_lat max_lon max_lat)
    python3 tools/fetch_dem.py --bbox 7.9 48.3 9.7 49.8 --out-dir data/dem

    # or derive the bbox from a node dump (inspect_graph --dump-nodes)
    python3 tools/fetch_dem.py --nodes nodes.txt --out-dir data/dem
"""
import argparse
import math
import os
import sys
import urllib.request

BASE = ("https://copernicus-dem-30m.s3.amazonaws.com/"
        "Copernicus_DSM_COG_10_{name}_DEM/Copernicus_DSM_COG_10_{name}_DEM.tif")


def tile_name(lat_sw, lon_sw):
    ns = "N" if lat_sw >= 0 else "S"
    ew = "E" if lon_sw >= 0 else "W"
    return f"{ns}{abs(lat_sw):02d}_00_{ew}{abs(lon_sw):03d}_00"


def bbox_from_nodes(path):
    mnlat = mxlat = mnlon = mxlon = None
    for line in open(path):
        p = line.split()
        if len(p) < 3:
            continue
        lat, lon = float(p[1]), float(p[2])
        mnlat = lat if mnlat is None else min(mnlat, lat)
        mxlat = lat if mxlat is None else max(mxlat, lat)
        mnlon = lon if mnlon is None else min(mnlon, lon)
        mxlon = lon if mxlon is None else max(mxlon, lon)
    if mnlat is None:
        sys.exit("No nodes read.")
    return mnlon, mnlat, mxlon, mxlat


def tiles_for_bbox(min_lon, min_lat, max_lon, max_lat):
    out = []
    for la in range(math.floor(min_lat), math.floor(max_lat) + 1):
        for lo in range(math.floor(min_lon), math.floor(max_lon) + 1):
            out.append((la, lo))
    return out


def main():
    ap = argparse.ArgumentParser(description="Fetch Copernicus GLO-30 DEM tiles for an area")
    ap.add_argument("--bbox", nargs=4, type=float, metavar=("MIN_LON", "MIN_LAT", "MAX_LON", "MAX_LAT"))
    ap.add_argument("--nodes", help="'id lat lon' dump to derive the bbox from")
    ap.add_argument("--out-dir", default="data/dem")
    args = ap.parse_args()

    if args.bbox:
        bbox = tuple(args.bbox)
    elif args.nodes:
        bbox = bbox_from_nodes(args.nodes)
    else:
        sys.exit("Provide --bbox or --nodes.")

    print(f"bbox: lon {bbox[0]:.3f}..{bbox[2]:.3f}  lat {bbox[1]:.3f}..{bbox[3]:.3f}")
    os.makedirs(args.out_dir, exist_ok=True)
    tiles = tiles_for_bbox(*bbox)
    print(f"{len(tiles)} tile(s) to ensure in {args.out_dir}")

    for la, lo in tiles:
        name = tile_name(la, lo)
        dest = os.path.join(args.out_dir, f"{name}.tif")
        if os.path.exists(dest) and os.path.getsize(dest) > 0:
            print(f"  have  {name}")
            continue
        url = BASE.format(name=name)
        try:
            print(f"  fetch {name} ...", end="", flush=True)
            urllib.request.urlretrieve(url, dest)
            print(f" {os.path.getsize(dest) // (1024*1024)} MB")
        except Exception as e:  # noqa: BLE001 — keep going on a missing/ocean tile
            print(f" SKIP ({e})")
            if os.path.exists(dest):
                os.remove(dest)

    print("done")


if __name__ == "__main__":
    main()
