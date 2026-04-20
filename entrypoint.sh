#!/bin/bash
set -e

# Required CLI inputs
INPUT2D_PATH="$1"
GRAPH_PATH="$2"
LAS_PATH="$3"
OUTPUT_FILE_PATH_AND_NAME="$4"

# Optional CLI inputs
CLASS_BUILDING="${5:-6}"
CLASS_GROUND="${6:-2}"
TEMP_FOLD="${7:-/working_folder}"
GROUND_POLYGON_NAME="${8:-/working_folder/ground_polygon}"

if [ "$#" -lt 4 ]; then
    echo "Correct usage: /entrypoint.sh <input2D_path> <graph_path> <las_path> <output_file_path_and_name> [class_building] [class_ground] [temp_fold] [ground_polygon_name]"
    exit 1
fi

OUTPUT_DIR="$(dirname "$OUTPUT_FILE_PATH_AND_NAME")"

mkdir -p "$TEMP_FOLD"
mkdir -p "$OUTPUT_DIR"

echo "---------------------------------------"
echo "Running core..."
echo "---------------------------------------"


python3 /core/cityloder_main.py \
    "$INPUT2D_PATH" \
    "$GRAPH_PATH" \
    "$LAS_PATH" \
    "$OUTPUT_FILE_PATH_AND_NAME" \
    "$CLASS_BUILDING" \
    "$CLASS_GROUND" \
    "$TEMP_FOLD" \
    "$GROUND_POLYGON_NAME"

echo "---------------------------------------"
echo "Running mesh_gen..."
echo "---------------------------------------"

MESH_GEN_START_MS=$(date +%s%3N)

set +e

/mesh_gen/build/city_iconic_mesh \
    "${GROUND_POLYGON_NAME}.off" \
    "$TEMP_FOLD" \
    "$GRAPH_PATH" \
    "$OUTPUT_FILE_PATH_AND_NAME"

PT2_EXIT_CODE=$?

set -e

MESH_GEN_END_MS=$(date +%s%3N)
MESH_GEN_DURATION_MS=$((MESH_GEN_END_MS - MESH_GEN_START_MS))

MESH_GEN_DURATION=$(awk "BEGIN { printf \"%.2f\", $MESH_GEN_DURATION_MS/1000 }")

echo "mesh_gen duration: ${MESH_GEN_DURATION}s"

if [ "$PT2_EXIT_CODE" -ne 0 ]; then
    echo "WARNING: city_iconic_mesh failed with exit code $PT2_EXIT_CODE"
    exit "$PT2_EXIT_CODE"
fi

echo "---------------------------------------"
echo "Pipeline completed successfully"
echo "---------------------------------------"