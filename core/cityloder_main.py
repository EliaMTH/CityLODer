import time
import os
import sys

from cityloder_preprocess_footprint import footprint_preprocess
from cityloder_preprocess_streetgraph import preprocess_graph
from cityloder_compute_buildings import main as pc2offs
from cityloder_processPC import read_las_file, generate_ground_polygon
from cityloder_cityJSONgen import main as cityJSONgen


def run_cityloder_pipeline(
    input2D_path: str,
    graph_path: str,
    las_path: str,
    outname: str,
    class_building: int = 6,
    class_ground: int = 2,
    temp_fold: str = "temp",
    ground_polygon_name: str = "ground_polygon",
) -> dict:
    """
    Run the full CityLoder pipeline.

    Args:
        - input2D_path: Path to the 2D footprint input file (.shp or similar).
        - graph_path: Path to the street graph file (.geojson).
        - las_path: Path to the LAS point cloud file.
        - outname: Output base name for generated CityJSON.
        - class_building: LAS classification code for buildings.
        - lass_ground: LAS classification code for ground.
        - temp_fold: Temporary working directory.
        - ground_polygon_name: Name/path used by generate_ground_polygon().

    Returns:
        Dictionary with execution metadata.
    """

    start = time.perf_counter()

    # Optional: ensure temp folder exists
    os.makedirs(temp_fold, exist_ok=True)

    # Read LAS / point cloud
    xyz_building, xyz_ground, coords = read_las_file(
        las_path,
        class_building,
        class_ground,
    )

    # Preprocess footprints
    M, final_id, final_check_adj, M_ori = footprint_preprocess(input2D_path)

    # Preprocess graph
    graph_updated_path = preprocess_graph(graph_path, xyz_ground, outname)

    # Compute buildings
    pc2offs(
        temp_fold,
        xyz_building,
        xyz_ground,
        M,
        final_id,
        final_check_adj,
    )

    # Generate ground polygon
    generate_ground_polygon(
        xyz_building,
        xyz_ground,
        ground_polygon_name,
    )

    # Generate CityJSON
    cityJSONgen(
        temp_fold,
        outname,
        graph_updated_path,
        M_ori,
        final_id,
    )

    end = time.perf_counter()
    elapsed = end - start

    result = {
        "success": True,
        "outname": outname,
        "temp_folder": temp_fold,
        "graph_updated_path": graph_updated_path,
        "elapsed_seconds": round(elapsed, 2),
    }

    print(f"Total execution time: {elapsed:.2f} seconds")
    return result

# -----------------------------------------------------------------------------

if __name__ == "__main__":
    try:
        if len(sys.argv) < 5:
            print(
                "Correct usage: python cityloder_main.py "
                "<input2D_path> <graph_path> <las_path> <outname> "
                "[class_building] [class_ground] [temp_fold] [ground_polygon_name]",
                file=sys.stderr,
            )
            sys.exit(1)

        input2D_path = sys.argv[1]
        graph_path = sys.argv[2]
        las_path = sys.argv[3]
        outname = sys.argv[4]

        class_building = int(sys.argv[5]) if len(sys.argv) > 5 else 6
        class_ground = int(sys.argv[6]) if len(sys.argv) > 6 else 2
        temp_fold = sys.argv[7] if len(sys.argv) > 7 else "temp"
        ground_polygon_name = sys.argv[8] if len(sys.argv) > 8 else "ground_polygon"

        result = run_cityloder_pipeline(
            input2D_path=input2D_path,
            graph_path=graph_path,
            las_path=las_path,
            outname=outname,
            class_building=class_building,
            class_ground=class_ground,
            temp_fold=temp_fold,
            ground_polygon_name=ground_polygon_name,
        )

        print(result)

    except ValueError:
        print(
            "Correct usage: class_building and class_ground must be integers",
            file=sys.stderr,
        )
        sys.exit(1)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)