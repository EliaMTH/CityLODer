#!/bin/bash

DATA_DIR="$(pwd)/data/Matera"
mkdir -p "${DATA_DIR}/output"

echo ""
echo "Launching CityLODer on ${DATA_DIR}"
echo ""

sudo docker run --rm \
  -u $(id -u):$(id -g) \
  -e HOME=/tmp \
  -e MPLCONFIGDIR=/tmp/matplotlib \
  -v "${DATA_DIR}:/data" \
  cityloder \
  /data/footprints.shp \
  /data/street_graph_z.geojson \
  /data/point_cloud.las \
  /data/output/mesh \
  6 \
  2 \
  /data/output/tmp_buildings \
  /data/output/tmp_buildings/ground_polygon \
  |& tee "${DATA_DIR}/output/log.txt"
  
