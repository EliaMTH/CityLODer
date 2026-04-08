import os
import sys
import json
import math
import numpy as np


## -------------------------------------------------------------------------------
## -------------------- Subfunctions (Helper/Utility Functions) ------------------
## -------------------------------------------------------------------------------

def off_to_dict(filepath):
    """
    Parses an OFF file into a dictionary with vertices and geometry.
    """
    with open(filepath, "r") as f:
        lines = [line.strip() for line in f if line.strip()]

    assert lines[0] == "OFF", "Not a valid OFF file"

    n_vertices, n_faces, _ = map(int, lines[1].split())
    vertices = []
    for i in range(2, 2 + n_vertices):
        x, y, z = map(float, lines[i].split())
        vertices.append([x, y, z])

    faces = []
    for i in range(2 + n_vertices, 2 + n_vertices + n_faces):
        parts = list(map(int, lines[i].split()))
        indices = parts[1:]
        faces.append(indices)

    if len(faces) == 1:
        geometry = [faces[0]]
    else:
        geometry = [faces]

    return {
        "geometry": geometry,
        "vertices": vertices
    }


def off_to_dict_walls(filepath):
    """
    Parses an OFF file representing walls into a dictionary with vertices and faces.
    """
    with open(filepath, "r") as f:
        lines = [line.strip() for line in f if line.strip()]

    assert lines[0] == "OFF", "Not a valid OFF file"

    n_vertices, n_faces, _ = map(int, lines[1].split())

    vertices = []
    for i in range(2, 2 + n_vertices):
        x, y, z = map(float, lines[i].split())
        vertices.append([x, y, z])

    faces = []
    for i in range(2 + n_vertices, 2 + n_vertices + n_faces):
        parts = list(map(int, lines[i].split()))
        indices = parts[1:]
        faces.append(indices)

    return {
        "geometry": faces,
        "vertices": vertices
    }


def merge_off_models(models, merged_name="merged_buildings"):
    """
    Merges multiple OFF parsed models into one, ensuring unique vertices
    and adjusted geometry indices.
    """
    all_vertices = []
    all_geometry = []
    vertex_offset = 0

    for model in models:
        for g in model["geometry"]:
            if all(isinstance(el, int) for el in g):
                geom_shifted = [i + vertex_offset for i in g]
                all_geometry.append(geom_shifted)
            else:
                geom_shifted = [[i + vertex_offset for i in ring] for ring in g]
                all_geometry.append(geom_shifted)

        all_vertices.extend(model["vertices"])
        vertex_offset += len(model["vertices"])

    unique_vertices = []
    vertex_map = {}
    for v in all_vertices:
        v_tuple = tuple(round(c, 8) for c in v)
        if v_tuple not in vertex_map:
            vertex_map[v_tuple] = len(unique_vertices)
            unique_vertices.append(list(v_tuple))

    new_geometry = []
    for g in all_geometry:
        if all(isinstance(el, int) for el in g):
            new_geometry.append([
                vertex_map[tuple(round(all_vertices[i][j], 8) for j in range(3))]
                for i in g
            ])
        else:
            new_geometry.append([
                [
                    vertex_map[tuple(round(all_vertices[i][j], 8) for j in range(3))]
                    for i in ring
                ]
                for ring in g
            ])

    return {
        "id": merged_name,
        "lod": "1.1",
        "geometry": new_geometry,
        "vertices": unique_vertices
    }


def parse_OFF_building(building_path):
    """
    Parses a building dataset from pavement, roof, and wall OFF files and merges them.
    """
    buildings_data_f = off_to_dict(f"{building_path}/pavement_polygon.off")
    buildings_data_r = off_to_dict(f"{building_path}/roof_polygon.off")
    buildings_data_w = off_to_dict_walls(f"{building_path}/facades.off")

    return merge_off_models(
        [buildings_data_f, buildings_data_r, buildings_data_w],
        os.path.basename(building_path)
    )


def parse_OFF_building_dataset(main_folder):
    """
    Parses all subfolders in a main folder recursively, running parse_OFF_building
    on folders containing the required OFF files.
    """
    all_buildings_data = {}

    for root, dirs, files in os.walk(main_folder):
        pavement_file = "pavement_polygon.off"
        roof_file = "roof_polygon.off"
        facades_file = "facades.off"

        if pavement_file in files and roof_file in files and facades_file in files:
            try:
                data = parse_OFF_building(root)
                all_buildings_data[os.path.basename(root)] = data
            except Exception as e:
                print(f"Failed to parse {root}: {e}")

    return all_buildings_data


## -------------------------------------------------------------------------------
## ----------------------------- STREETGRAPH UTILITIES ---------------------------
## -------------------------------------------------------------------------------

def _ensure_xyz(coord, default_z=0.0):
    """
    Converts [x, y] or [x, y, z] into [x, y, z].
    """
    if len(coord) == 2:
        return [float(coord[0]), float(coord[1]), float(default_z)]
    elif len(coord) >= 3:
        return [float(coord[0]), float(coord[1]), float(coord[2])]
    else:
        raise ValueError(f"Invalid coordinate: {coord}")


def _make_json_safe(value):
    """
    Recursively converts values to JSON-safe types.
    """
    if value is None:
        return None
    if isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, list):
        return [_make_json_safe(v) for v in value]
    if isinstance(value, dict):
        return {str(k): _make_json_safe(v) for k, v in value.items()}
    return str(value)


def _get_road_object_id(props):
    """
    Use roadID, else fid, else unknown.
    """
    if "roadID" in props and props["roadID"] not in [None, ""]:
        return str(props["roadID"])
    if "fid" in props and props["fid"] not in [None, ""]:
        return str(props["fid"])
    return "unknown"


def _get_selected_road_attributes(props):
    """
    Keep only name, length, lanes.
    Use -1 if missing.
    """
    return {
        "name": _make_json_safe(props.get("name", -1)) if props.get("name", None) not in [None, ""] else -1,
        "length": _make_json_safe(props.get("length", -1)) if props.get("length", None) not in [None, ""] else -1,
        "lanes": _make_json_safe(props.get("lanes", -1)) if props.get("lanes", None) not in [None, ""] else -1,
        "NO": _make_json_safe(props.get("NO", -1)) if props.get("NO", None) not in [None, ""] else -1,
    }


def _normalize_2d(vx, vy):
    norm = math.hypot(vx, vy)
    if norm == 0:
        return 0.0, 0.0
    return vx / norm, vy / norm


def _line_to_road_polygon(coords_xyz, width=2.0):
    """
    Converts a centerline polyline into a 2D buffered road polygon of total width `width`.
    Output is one ring as a list of [x, y, z].

    This uses a simple offset-polyline construction:
    - compute segment normals
    - average normals at interior vertices
    - offset left/right by width/2
    """
    if len(coords_xyz) < 2:
        return None

    half_w = width / 2.0
    n = len(coords_xyz)

    pts = [(p[0], p[1], p[2]) for p in coords_xyz]
    seg_dirs = []
    seg_normals = []

    for i in range(n - 1):
        x1, y1, _ = pts[i]
        x2, y2, _ = pts[i + 1]
        dx, dy = x2 - x1, y2 - y1
        ux, uy = _normalize_2d(dx, dy)

        if ux == 0.0 and uy == 0.0:
            seg_dirs.append((0.0, 0.0))
            seg_normals.append((0.0, 0.0))
        else:
            seg_dirs.append((ux, uy))
            seg_normals.append((-uy, ux))  # left normal

    left_side = []
    right_side = []

    for i in range(n):
        x, y, z = pts[i]

        if i == 0:
            nx, ny = seg_normals[0]
        elif i == n - 1:
            nx, ny = seg_normals[-1]
        else:
            n1x, n1y = seg_normals[i - 1]
            n2x, n2y = seg_normals[i]
            nx, ny = n1x + n2x, n1y + n2y

            norm = math.hypot(nx, ny)
            if norm == 0:
                nx, ny = seg_normals[i]
            else:
                nx, ny = nx / norm, ny / norm

        lx = x + nx * half_w
        ly = y + ny * half_w
        rx = x - nx * half_w
        ry = y - ny * half_w

        left_side.append([lx, ly, z])
        right_side.append([rx, ry, z])

    ring = left_side + right_side[::-1]

    # Remove duplicate consecutive vertices
    cleaned = [ring[0]]
    for p in ring[1:]:
        if not (
            abs(p[0] - cleaned[-1][0]) < 1e-9 and
            abs(p[1] - cleaned[-1][1]) < 1e-9 and
            abs(p[2] - cleaned[-1][2]) < 1e-9
        ):
            cleaned.append(p)

    if len(cleaned) < 3:
        return None

    return cleaned


def parse_street_geojson(geojson_path, default_z=0.0, road_width=2.0):
    """
    Parses a GeoJSON street graph where each feature is expected to be a LineString.
    Each input street feature becomes one Road object represented as a MultiSurface.

    Returns a list of dicts:
        {
            "id": "road id",
            "type": "Road",
            "lod": "1",
            "vertices": [[x, y, z], ...],
            "surfaces": [[[0, 1, 2, ...]]],
            "attributes": {"name": ..., "length": ..., "lanes": ...}
        }
    """
    with open(geojson_path, "r", encoding="utf-8") as f:
        gj = json.load(f)

    if gj.get("type") == "FeatureCollection":
        features = gj.get("features", [])
    elif gj.get("type") == "Feature":
        features = [gj]
    elif gj.get("type") in ("LineString", "MultiLineString"):
        features = [{"type": "Feature", "geometry": gj, "properties": {}}]
    else:
        raise ValueError(
            "Unsupported GeoJSON root type. Expected FeatureCollection, Feature, LineString, or MultiLineString."
        )

    streets = []

    for feat_idx, feat in enumerate(features):
        geometry = feat.get("geometry")
        props = feat.get("properties", {}) or {}

        if not geometry:
            continue

        object_id = _get_road_object_id(props)
        attrs = _get_selected_road_attributes(props)

        gtype = geometry.get("type")

        if gtype == "LineString":
            coords = geometry.get("coordinates", [])
            if len(coords) < 2:
                continue

            line_xyz = [_ensure_xyz(c, default_z=default_z) for c in coords]
            polygon_ring = _line_to_road_polygon(line_xyz, width=road_width)
            if polygon_ring is None:
                continue

            streets.append({
                "id": object_id,
                "type": "Road",
                "lod": "1",
                "vertices": polygon_ring,
                "surfaces": [[list(range(len(polygon_ring)))]],
                "attributes": attrs
            })

        elif gtype == "MultiLineString":
            # Defensive support. Each sub-line becomes one polygon surface inside one MultiSurface object.
            multi_coords = geometry.get("coordinates", [])
            if not multi_coords:
                continue

            vertices = []
            surfaces = []
            offset = 0

            for line in multi_coords:
                if len(line) < 2:
                    continue

                line_xyz = [_ensure_xyz(c, default_z=default_z) for c in line]
                polygon_ring = _line_to_road_polygon(line_xyz, width=road_width)
                if polygon_ring is None:
                    continue

                vertices.extend(polygon_ring)
                ring_idx = list(range(offset, offset + len(polygon_ring)))
                surfaces.append([ring_idx])
                offset += len(polygon_ring)

            if not surfaces:
                continue

            streets.append({
                "id": object_id,
                "type": "Road",
                "lod": "1",
                "vertices": vertices,
                "surfaces": surfaces,
                "attributes": attrs
            })

        else:
            print(f"Skipping non-line feature of type '{gtype}'")

    return streets


## -------------------------------------------------------------------------------
## ---------------------------- CITYJSON GENERATION ------------------------------
## -------------------------------------------------------------------------------

def create_cityjson(buildings=None, streets=None, output_file="default.city.json", scale=None):
    """
    Creates a CityJSON v2.0 file from parsed building data and optional street data.
    """
    buildings = buildings or []
    streets = streets or []
    scale = scale or [0.001, 0.001, 0.001]

    global_vertices_world = []
    vertex_map = {}

    def normalize_vertex(v):
        return tuple(round(float(c), 8) for c in v)

    def get_vertex_index(vertex, local_vertices):
        if isinstance(vertex, int):
            vertex = local_vertices[vertex]

        vkey = normalize_vertex(vertex)
        if vkey not in vertex_map:
            vertex_map[vkey] = len(global_vertices_world)
            global_vertices_world.append(list(vkey))
        return vertex_map[vkey]

    cityjson_dict = {
        "type": "CityJSON",
        "version": "2.0",
        "CityObjects": {},
        "vertices": []
    }

    # Buildings
    for b in buildings:
        geom_faces = []
        semantic_surfaces = []
        semantics_values = []

        for idx, face in enumerate(b["geometry"]):
            if all(isinstance(f, list) for f in face):
                face_indices = [[get_vertex_index(v, b["vertices"]) for v in ring] for ring in face]
            else:
                face_indices = [[get_vertex_index(v, b["vertices"]) for v in face]]

            geom_faces.append(face_indices)

            if idx == 0:
                semantic_type = "FloorSurface"
            elif idx == 1:
                semantic_type = "RoofSurface"
            else:
                semantic_type = "WallSurface"

            semantic_surfaces.append({"type": semantic_type})
            semantics_values.append(idx)

        cityjson_dict["CityObjects"][b["id"]] = {
            "type": "Building",
            "geometry": [{
                "type": "Solid",
                "lod": b.get("lod", "1.1"),
                "boundaries": [geom_faces],
                "semantics": {
                    "surfaces": semantic_surfaces,
                    "values": [semantics_values]
                }
            }]
        }

    # Streets as polygonal MultiSurface roads
    used_ids = set(cityjson_dict["CityObjects"].keys())

    for i, s in enumerate(streets):
        street_id = s["id"]
        if street_id in used_ids:
            street_id = f"{street_id}_{i}"
        used_ids.add(street_id)

        ms_boundaries = []
        for surf in s["surfaces"]:
            surf_indices = [
                [get_vertex_index(v_idx, s["vertices"]) for v_idx in ring]
                for ring in surf
            ]
            ms_boundaries.append(surf_indices)

        if not ms_boundaries:
            continue

        cityjson_dict["CityObjects"][street_id] = {
            "type": s.get("type", "Road"),
            "attributes": s.get("attributes", {}),
            "geometry": [{
                "type": "MultiSurface",
                "lod": s.get("lod", "1"),
                "boundaries": ms_boundaries
            }]
        }

    if not global_vertices_world:
        raise ValueError("No vertices found. Nothing to write.")

    vertices_array = np.array(global_vertices_world, dtype=float)
    min_coords = vertices_array.min(axis=0)
    scale_arr = np.array(scale, dtype=float)

    int_vertices = np.rint((vertices_array - min_coords) / scale_arr).astype(int)

    cityjson_dict["vertices"] = int_vertices.tolist()
    cityjson_dict["transform"] = {
        "scale": scale,
        "translate": min_coords.tolist()
    }

    if output_file.endswith(".city.json"):
        pass  # already correct
    elif output_file.endswith(".json"):
        output_file = output_file[:-5] + ".city.json"
    else:
        output_file = f"{output_file}.city.json"

    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(cityjson_dict, f, indent=2)

    print(f"CityJSON written to {output_file}")


## --------------------------------------------------------------------
## ---------------------------------- MAIN ----------------------------
## --------------------------------------------------------------------

def main(working_folder, outname, street_geojson=None):
    """
    Main function that processes the input folder and generates a CityJSON output file.
    """
    bb = parse_OFF_building_dataset(working_folder)

    streets = []
    if street_geojson is not None:
        streets = parse_street_geojson(
            street_geojson,
            default_z=0.0,
            road_width=2.0
        )

    create_cityjson(
        buildings=list(bb.values()),
        streets=streets,
        output_file=outname
    )


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 Lid2LODpt2.py <working_folder> <outname> [street_geojson]")
        sys.exit(1)

    working_folder = sys.argv[1]
    outname = sys.argv[2]
    street_geojson = sys.argv[3] if len(sys.argv) > 3 else None

    main(working_folder, outname, street_geojson)