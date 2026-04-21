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
  /data/point_cloud.las \
  /data/footprints.shp \
  /data/output \
  mesh \
  /data/street_graph_z.geojson \
  6 \
  2 \
  /data/output/tmp_buildings \
  |& tee "${DATA_DIR}/output/log.txt"
  
