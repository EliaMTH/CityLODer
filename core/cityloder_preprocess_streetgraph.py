#!/usr/bin/env python3

import json
from pathlib import Path
import numpy as np
from scipy.spatial import cKDTree


def preprocess_graph(geojson_path, xyz_ground, out_folder):
    """
    Add Z values to all coordinates in a GeoJSON street graph using the nearest
    LAS point in 2D, considering only points of a chosen LAS class.

    Parameters
    ----------
    geojson_path : str or Path
        Path to the input GeoJSON file. Supported geometries are:
        - LineString
        - MultiLineString

    xyz_ground : float array
        las coordinates of the ground points

    Returns
    -------
    Path to the output GeoJSON file, saved next to the input file with suffix "_updated".

    Notes
    -----
    Existing Z values, if present, are overwritten.
    Feature metadata is normalized so that:
    - "NO" is kept/set from existing "NO", else "osmid", else 0
    - properties with null values are removed
    """
    geojson_path = Path(geojson_path)


    # Load the GeoJSON structure in memory.
    with geojson_path.open("r", encoding="utf-8") as f:
        geojson = json.load(f)

    # Initialize stuff
    nodes = []
    xy = []

    # Collect references to the actual coordinate lists in the GeoJSON.
    for feature in geojson.get("features", []):
        geom = feature.get("geometry")
        if not geom:
            continue

        # Make sure NO field exists and remove NULL properties
        props = feature.setdefault("properties", {})
        props["NO"] = props.get("NO") or props.get("osmid") or 0 # To be updated
        feature["properties"] = {k: v for k, v in props.items() if v is not None}

        # Normalize both geometry types into an iterable of lines.
        if geom["type"] == "LineString":
            parts = [geom["coordinates"]]
        elif geom["type"] == "MultiLineString":
            parts = geom["coordinates"]
        else:
            raise ValueError(f"Unsupported geometry type: {geom['type']}")

        # Store each coordinate list itself, plus its XY values for the KDTree query.
        for line in parts:
            for coord in line:
                if len(coord) >= 2:
                    nodes.append(coord)
                    xy.append(coord[:2])

    # Only run the nearest-neighbor step if at least one valid coordinate exists.
    if xy:
        xy = np.asarray(xy, dtype=float)
        _, idx = cKDTree(xyz_ground[:, :2]).query(xy, k=1)
        for coord, z in zip(nodes, xyz_ground[idx, 2]):
            coord[:] = [coord[0], coord[1], float(z)]

    # Save beside the input file, appending "_updated" to the filename.
    out_path = Path(out_folder).parent / f"{geojson_path.stem}_updated{geojson_path.suffix}"
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(geojson, f, ensure_ascii=False, indent=2)


    return out_path
