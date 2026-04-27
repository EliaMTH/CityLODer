#!/bin/bash

# Elenco delle directory (percorsi assoluti o relativi)
DATA_DIRS=(
  #"$(pwd)/data/Comparison"
  "$(pwd)/data/Matera"
  #"$(pwd)/data/Catania"
  #"$(pwd)/data/Genova_HSM"
  #"$(pwd)/data/Genova_CS"
)

# Parametri di default
P1=6
P2=2

# Override per Catania
if [ "$(basename "$DATA_DIRS")" = "Catania" ]; then
  P1=1
  P2=2
fi
  
for DATA_DIR in "${DATA_DIRS[@]}"; do
  echo ""
  echo "Launching CityLODer on ${DATA_DIR}"
  echo ""

  rm -rf "${DATA_DIR}/output"
  mkdir -p "${DATA_DIR}/output"

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
    "$P1" \
    "$P2" \
    /data/output/tmp_buildings \
    |& tee "${DATA_DIR}/output/log.txt"

done


