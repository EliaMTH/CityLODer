"""
Ad-hoc script: run cityloder_cityJSONgen on pre-existing OFF meshes in data/buildings.
Skips LAS processing, footprint preprocessing, and mesh generation entirely.

Differences from the normal pipeline:
  - Building files are named pavement.off / roof.off (not pavement_polygon.off / roof_polygon.off)
  - No shapefile -> building attributes left empty
  - No street GeoJSON -> roads omitted
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "core"))

import cityloder_cityJSONgen as cjg

BUILDINGS_FOLDER = os.path.join(os.path.dirname(__file__), "data", "buildings")
OUTPUT_FILE = os.path.join(os.path.dirname(__file__), "output.city.json")


def _parse_building(folder):
    pavement = cjg.read_off(os.path.join(folder, "pavement.off"), wrap_faces=True)
    roof = cjg.read_off(os.path.join(folder, "roof.off"))
    facades = cjg.read_off(os.path.join(folder, "facades.off"))
    merged = cjg.merge_models([pavement, roof, facades], os.path.basename(folder))
    merged["n_floor"] = len(pavement["geometry"])
    merged["n_roof"] = len(roof["geometry"])
    return merged


def _parse_building_dataset(main_folder):
    required = {"pavement.off", "roof.off", "facades.off"}
    valid_roots = []
    for root, _, files in os.walk(main_folder):
        if required.issubset(files):
            valid_roots.append(root)

    def natural_key(s):
        return [int(p) if p.isdigit() else p.lower() for p in re.split(r"(\d+)", s)]

    buildings = {}
    for root in sorted(valid_roots, key=lambda p: natural_key(os.path.basename(p))):
        try:
            buildings[os.path.basename(root)] = _parse_building(root)
        except Exception as e:
            print(f"  skipped {root}: {e}")
    return buildings


# No shapefile -> return empty attributes instead of crashing
cjg._build_metadata = lambda building_num, index_original, footprint_original: {}

buildings = _parse_building_dataset(BUILDINGS_FOLDER)
print(f"Parsed {len(buildings)} buildings")

cjg.create_cityjson(
    buildings=list(buildings.values()),
    streets=[],
    output_file=OUTPUT_FILE,
)
