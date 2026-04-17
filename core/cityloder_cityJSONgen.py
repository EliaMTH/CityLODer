import os
import re
import json
import math
import numpy as np

# ---------------------------------------------------------------------
# OFF PARSING
# ---------------------------------------------------------------------

def read_off(filepath, wrap_faces=False):
    """Read an OFF file and return {'vertices': ..., 'geometry': ...}."""
    with open(filepath, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]

    if lines[0] != "OFF":
        raise ValueError(f"Not a valid OFF file: {filepath}")

    n_vertices, n_faces, _ = map(int, lines[1].split())

    vertices = [list(map(float, lines[i].split())) for i in range(2, 2 + n_vertices)]
    faces = [list(map(int, lines[i].split()))[1:] for i in range(2 + n_vertices, 2 + n_vertices + n_faces)]

    return {
        "vertices": vertices,
        "geometry": [faces[0]] if wrap_faces and len(faces) == 1 else ([faces] if wrap_faces else faces),
    }


def merge_models(models, model_id="merged_buildings"):
    """Merge parsed models, deduplicating vertices."""
    all_vertices = []
    all_geometry = []
    offset = 0

    for model in models:
        for geom in model["geometry"]:
            if all(isinstance(x, int) for x in geom):
                all_geometry.append([i + offset for i in geom])
            else:
                all_geometry.append([[i + offset for i in ring] for ring in geom])
        all_vertices.extend(model["vertices"])
        offset += len(model["vertices"])

    unique_vertices = []
    vertex_map = {}

    def key(v):
        return tuple(round(c, 8) for c in v)

    for v in all_vertices:
        k = key(v)
        if k not in vertex_map:
            vertex_map[k] = len(unique_vertices)
            unique_vertices.append(list(k))

    def remap(index):
        return vertex_map[key(all_vertices[index])]

    new_geometry = []
    for geom in all_geometry:
        if all(isinstance(x, int) for x in geom):
            new_geometry.append([remap(i) for i in geom])
        else:
            new_geometry.append([[remap(i) for i in ring] for ring in geom])

    return {
        "id": model_id,
        "lod": "1",
        "geometry": new_geometry,
        "vertices": unique_vertices,
    }


def parse_building(folder):
    """Parse one building from pavement, roof, and facade OFF files."""
    return merge_models(
        [
            read_off(os.path.join(folder, "pavement_polygon.off"), wrap_faces=True),
            read_off(os.path.join(folder, "roof_polygon.off"), wrap_faces=True),
            read_off(os.path.join(folder, "facades.off")),
        ],
        os.path.basename(folder),
    )


def parse_building_dataset(main_folder):
    """Parse all valid building folders recursively."""
    required = {"pavement_polygon.off", "roof_polygon.off", "facades.off"}
    buildings = {}

    def natural_key(s):
        return [int(part) if part.isdigit() else part.lower()
                for part in re.split(r'(\d+)', s)]

    valid_roots = []

    for root, _, files in os.walk(main_folder):
        if required.issubset(files):
            valid_roots.append(root)

    for root in sorted(valid_roots, key=lambda p: natural_key(os.path.basename(p))):
        try:
            buildings[os.path.basename(root)] = parse_building(root)
        except Exception as e:
            print(f"Failed to parse {root}: {e}")

    return buildings

# ---------------------------------------------------------------------
# STREET UTILITIES
# ---------------------------------------------------------------------

def ensure_xyz(coord, default_z=0.0):
    if len(coord) == 2:
        return [float(coord[0]), float(coord[1]), float(default_z)]
    if len(coord) >= 3:
        return [float(coord[0]), float(coord[1]), float(coord[2])]
    raise ValueError(f"Invalid coordinate: {coord}")


def json_safe(value):
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, list):
        return [json_safe(v) for v in value]
    if isinstance(value, dict):
        return {str(k): json_safe(v) for k, v in value.items()}
    return str(value)


def road_id(props):
    return str(props.get("roadID") or props.get("fid") or "unknown")


def road_attributes(props):
    attrs = {}

    for key in ("name", "length", "lanes"):
        value = props.get(key)
        if value is not None:
            attrs[key] = json_safe(value)

    no_value = props.get("NO")
    if no_value is None:
        no_value = props.get("osmid", 0)

    attrs["NO"] = json_safe(no_value)

    return attrs


def normalize_2d(x, y):
    norm = math.hypot(x, y)
    return (0.0, 0.0) if norm == 0 else (x / norm, y / norm)


def line_to_road_polygon(coords_xyz, width=2.0):
    """Convert a centerline into a simple buffered polygon ring."""
    if len(coords_xyz) < 2:
        return None

    half_w = width / 2.0
    pts = [(p[0], p[1], p[2]) for p in coords_xyz]

    seg_normals = []
    for (x1, y1, _), (x2, y2, _) in zip(pts, pts[1:]):
        ux, uy = normalize_2d(x2 - x1, y2 - y1)
        seg_normals.append((0.0, 0.0) if (ux, uy) == (0.0, 0.0) else (-uy, ux))

    left, right = [], []
    for i, (x, y, z) in enumerate(pts):
        if i == 0:
            nx, ny = seg_normals[0]
        elif i == len(pts) - 1:
            nx, ny = seg_normals[-1]
        else:
            nx, ny = seg_normals[i - 1][0] + seg_normals[i][0], seg_normals[i - 1][1] + seg_normals[i][1]
            norm = math.hypot(nx, ny)
            nx, ny = seg_normals[i] if norm == 0 else (nx / norm, ny / norm)

        left.append([x + nx * half_w, y + ny * half_w, z])
        right.append([x - nx * half_w, y - ny * half_w, z])

    ring = left + right[::-1]

    cleaned = [ring[0]]
    for p in ring[1:]:
        if any(abs(a - b) >= 1e-9 for a, b in zip(p, cleaned[-1])):
            cleaned.append(p)

    return cleaned if len(cleaned) >= 3 else None


def parse_street_geojson(path, default_z=0.0, road_width=2.0):
    """Parse GeoJSON roads into CityJSON-style road objects."""
    with open(path, "r", encoding="utf-8") as f:
        gj = json.load(f)

    if gj.get("type") == "FeatureCollection":
        features = gj.get("features", [])
    elif gj.get("type") == "Feature":
        features = [gj]
    elif gj.get("type") in ("LineString", "MultiLineString"):
        features = [{"type": "Feature", "geometry": gj, "properties": {}}]
    else:
        raise ValueError("Expected FeatureCollection, Feature, LineString, or MultiLineString")

    streets = []

    for feat in features:
        geometry = feat.get("geometry")
        props = feat.get("properties", {}) or {}
        if not geometry:
            continue

        gtype = geometry.get("type")
        obj = {
            "id": road_id(props),
            "type": "Road",
            "lod": "1",
            "attributes": road_attributes(props),
        }

        if gtype == "LineString":
            coords = geometry.get("coordinates", [])
            ring = line_to_road_polygon([ensure_xyz(c, default_z) for c in coords], road_width) if len(coords) >= 2 else None
            if ring:
                obj["vertices"] = ring
                obj["surfaces"] = [[list(range(len(ring)))]]
                streets.append(obj)

        elif gtype == "MultiLineString":
            vertices, surfaces, offset = [], [], 0
            for line in geometry.get("coordinates", []):
                if len(line) < 2:
                    continue
                ring = line_to_road_polygon([ensure_xyz(c, default_z) for c in line], road_width)
                if not ring:
                    continue
                vertices.extend(ring)
                surfaces.append([list(range(offset, offset + len(ring)))])
                offset += len(ring)

            if surfaces:
                obj["vertices"] = vertices
                obj["surfaces"] = surfaces
                streets.append(obj)
        else:
            print(f"Skipping non-line feature of type '{gtype}'")

    return streets


# ---------------------------------------------------------------------
# CITYJSON
# ---------------------------------------------------------------------

def create_cityjson(buildings=None, streets=None, output_file="default.city.json", scale=None, footprint_original = None, index_original=None):
    buildings = buildings or []
    streets = streets or []
    scale = scale or [0.001, 0.001, 0.001]

    global_vertices = []
    vertex_map = {}

    def vkey(v):
        return tuple(round(float(c), 8) for c in v)

    def get_vertex_index(vertex, local_vertices):
        if isinstance(vertex, int):
            vertex = local_vertices[vertex]
        k = vkey(vertex)
        if k not in vertex_map:
            vertex_map[k] = len(global_vertices)
            global_vertices.append(list(k))
        return vertex_map[k]
    
    def build_metadata(j,index_original,footprint_original):
        # Get the i-th entry
        idx_entry = index_original[j-1] - 1

        # Normalize to a list
        if isinstance(idx_entry, (int, str)):
            indices = [idx_entry]
        else:
            indices = list(idx_entry)

        # Fields to exclude from metadata
        exclude_fields = {"Geometry", "X", "Y"}

        # Initialize output dictionary using fields from the first referenced building
        metadata = {}
        first_record = footprint_original[indices[0]]

        for key in first_record.keys():
            if key not in exclude_fields:
                metadata[key] = []

        # Collect values from all referenced buildings
        for idx in indices:
            record = footprint_original[idx]

            for key in metadata.keys():
                value = record.get(key, "")

                # Convert numpy scalars/arrays safely to string
                if value is None:
                    value_str = ""
                else:
                    value_str = str(value)

                metadata[key].append(value_str)

        # Concatenate with semicolon
        for key in metadata:
            metadata[key] = ";".join(metadata[key])

        return metadata
        

    cityjson = {
        "type": "CityJSON",
        "version": "2.0",
        "CityObjects": {},
        "vertices": [],
    }

    for b in buildings:
        faces = []
        semantics = []

        building_num = int(re.search(r"\d+", b["id"]).group())

        for i, face in enumerate(b["geometry"]):
            rings = face if all(isinstance(x, list) for x in face) else [face]
            faces.append([[get_vertex_index(v, b["vertices"]) for v in ring] for ring in rings])
            semantics.append({"type": ["FloorSurface", "RoofSurface", "WallSurface"][min(i, 2)]})

        cityjson["CityObjects"][b["id"]] = {
            "type": "Building",
            "geometry": [{
                "type": "Solid",
                "lod": b.get("lod", "1"),
                "boundaries": [faces],
                "semantics": {
                    "surfaces": semantics,
                    "values": [list(range(len(faces)))]
                },
            }],
            "attributes": build_metadata(building_num, index_original, footprint_original)
        }

    used_ids = set(cityjson["CityObjects"])
    for i, s in enumerate(streets):
        sid = s["id"] if s["id"] not in used_ids else f"{s['id']}_{i}"
        used_ids.add(sid)

        boundaries = [
            [[get_vertex_index(v, s["vertices"]) for v in ring] for ring in surf]
            for surf in s["surfaces"]
        ]

        if not boundaries:
            continue

        cityjson["CityObjects"][sid] = {
            "type": s.get("type", "Road"),
            "attributes": s.get("attributes", {}),
            "geometry": [{
                "type": "MultiSurface",
                "lod": s.get("lod", "1"),
                "boundaries": boundaries,
            }],
        }

    if not global_vertices:
        raise ValueError("No vertices found. Nothing to write.")

    vertices = np.array(global_vertices, dtype=float)
    translate = vertices.min(axis=0)
    scale_arr = np.array(scale, dtype=float)

    cityjson["vertices"] = np.rint((vertices - translate) / scale_arr).astype(int).tolist()
    cityjson["transform"] = {"scale": scale, "translate": translate.tolist()}

    if output_file.endswith(".json") and not output_file.endswith(".city.json"):
        output_file = output_file[:-5] + ".city.json"
    elif not output_file.endswith(".city.json"):
        output_file += ".city.json"

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(cityjson, f, indent=2)

    print(f"CityJSON written to {output_file}")


# ---------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------

def main(working_folder, outname, street_geojson=None, footprint_original=None, index_original=None):
    buildings = parse_building_dataset(working_folder)
    streets = parse_street_geojson(street_geojson, default_z=0.0, road_width=2.0) if street_geojson else []
    create_cityjson(list(buildings.values()), streets, outname, None, footprint_original, index_original)


