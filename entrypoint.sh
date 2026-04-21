#!/bin/bash
set -e

# Required CLI inputs
LAS_PATH="$1"
INPUT2D_PATH="$2"

# Optional CLI inputs
OUTPUT_FILE_PATH="$3"
OUTPUT_FILE_NAME="$4"
GRAPH_PATH="$5"
CLASS_BUILDING="$6"
CLASS_GROUND="$7"
TEMP_FOLD="$8"

if [ "$#" -lt 2 ]; then
    echo "At least footprints, and las are required."
    exit 1
fi

if [ "$GRAPH_PATH" = "." ]; then
    GRAPH_PATH=""
fi

GROUND_POLYGON_NAME="$TEMP_FOLD/goundpolygon"

mkdir -p "$TEMP_FOLD"
mkdir -p "$OUTPUT_FILE_PATH"

echo "---------------------------------------"
echo "Running core..."
echo "---------------------------------------"


python3 /core/cityloder_main.py \
    "$INPUT2D_PATH" \
    "$GRAPH_PATH" \
    "$LAS_PATH" \
    "$OUTPUT_FILE_PATH/$OUTPUT_FILE_NAME" \
    "$CLASS_BUILDING" \
    "$CLASS_GROUND" \
    "$TEMP_FOLD" \
    "$GROUND_POLYGON_NAME"

echo "---------------------------------------"
echo "Running mesh_gen..."
echo "---------------------------------------"

set +e

/mesh_gen/build/city_iconic_mesh \
    "${GROUND_POLYGON_NAME}.off" \
    "$TEMP_FOLD" \
    "$GRAPH_PATH" \
    "$OUTPUT_FILE_PATH/$OUTPUT_FILE_NAME"

PT2_EXIT_CODE=$?

set -e

if [ "$PT2_EXIT_CODE" -ne 0 ]; then
    echo "WARNING: city_iconic_mesh failed with exit code $PT2_EXIT_CODE"
    exit "$PT2_EXIT_CODE"
fi

echo "---------------------------------------"
echo "Pipeline completed successfully"
echo "---------------------------------------"
