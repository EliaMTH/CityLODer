#!/bin/bash

DATA_DIR="/home/tommaso/Scrivania/Lid2LOD/data/Matera"

sudo docker run --rm \
  -u $(id -u):$(id -g) \
  -e HOME=/tmp \
  -e MPLCONFIGDIR=/tmp/matplotlib \
  -v "${DATA_DIR}:/data" \
  cityloder \
  /data/footprints.shp \
  /data/street_graph_z.geojson \
  /data/point_cloud.las \
  /data/out \
  6 \
  2 \
  /data/temp \
  /data/temp/ground_polygon
  
  
