# CityLODer

![License](https://img.shields.io/badge/license-GPL%20v3-blue.svg)

**CityLODer** is an automated pipeline for generating **Level of Detail 1 (LOD1)** urban models from airborne LiDAR point clouds and building footprints. It converts raw geospatial data into high-quality 3D city models suitable for visualization, analysis, and further processing.

The tool provides **OFF meshes** and **CityJSON** file city models. It can also integrate street network data from GeoJSON files when available.

CityLODer is designed for reproducible and cross-platform use through Docker, with wrapper scripts for Windows PowerShell and Linux/macOS Bash. Advanced users can also run the Python and C++ components locally for customization and development.

## Requirements

CityLODer can be run using Docker, which is the recommended method. Docker 20.1 or later is required. Usage is simplified through wrapper scripts for both Linux/macOS and Windows.

Advanced users can also run CityLODer in a local Python/C++ development environment (see (see [Local Development](#local-development))).


## Installation

### 1. Clone the Repository

```bash
git clone --recurse-submodules https://github.com/EliaMTH/CityLODer.git
cd CityLODer
```

### 2. Build the Docker Image

```bash
docker build -t cityloder .
```


## Input Data

CityLODer expects all input datasets to use the *same coordinate reference system*.

This is critical: the point cloud, building footprints, and optional street network must be spatially aligned.

### Point Cloud

Supported formats:

* `.las`
* `.laz`

**Ground** and **building** classes must be known (automatic classification included in the pipeline coming soon!).

Common ASPRS LAS classes include:

| Code | Description       |
| ---: | ----------------- |
|    0 | Never classified  |
|    1 | Unclassified      |
|    2 | **Ground**            |
|    3 | Low vegetation    |
|    4 | Medium vegetation |
|    5 | High vegetation   |
|    6 | **Building**          |
|    7 | Low point / noise |
|    8 | Model key-point   |
|    9 | Water             |

### Building Footprints

Supported format:

* ESRI Shapefile

Required files:

```text
footprints.shp
footprints.shx
footprints.dbf
```

Optional but recommended:

```text
footprints.prj
```


### Street Network

Supported format:

* GeoJSON

Requirements:

* LineString geometries

The street network is optional. To skip street processing, omit the graph parameter or use `"."`.

### External Ground Boundary

Supported format:

* OFF polygon, generated from a 2D ESRI Shapefile with the `tools/boundary_to_off.py` tool (see [Processing with an External Ground Boundary](#processing-with-an-external-ground-boundary))

By default, the ground extent is the convex hull of the point cloud, enlarged by 50 m. An external boundary lets you constrain the ground to a custom area (e.g., a municipality boundary).

The external boundary is optional. To use the default ground extent, omit the boundary parameter or use `"."`.

## Usage

CityLODer expects the input data to be placed inside a single folder. This folder should contain the airborne LiDAR point cloud, the building footprints, and, optionally, the street network file.

For example:

```text
data/
├── point_cloud.las
├── footprints.shp
├── footprints.shx
├── footprints.dbf
├── footprints.prj
├── (street_network.geojson)
└── (ground_polygon.off)
````

### Windows PowerShell

#### Minimal Example

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\point_cloud.las" `
  -INPUT2D_PATH "data\footprints.shp"
```

#### Full Example

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\point_cloud.las" `
  -INPUT2D_PATH "data\footprints.shp" `
  -OUTPUT_FILE_PATH "output" `
  -OUTPUT_FILE_NAME "mycity" `
  -GRAPH_PATH "data\street_network.geojson" `
  -CLASS_BUILDING 6 `
  -CLASS_GROUND 2 `
  -EXTERNAL_BOUNDARY_GROUND "data\ground_polygon.off"
```

> **Note:** `-EXTERNAL_BOUNDARY_GROUND` on Windows is **TO BE TESTED**.

### Linux/macOS Bash Wrapper

Make the wrapper executable if needed:

```bash
chmod +x wrapper_linux/cityloder
```

#### Minimal Example

```bash
./wrapper_linux/cityloder \
  -LAS_PATH "data/point_cloud.las" \
  -INPUT2D_PATH "data/footprints.shp"
```

#### Full Example

```bash
./wrapper_linux/cityloder \
  -LAS_PATH "data/point_cloud.las" \
  -INPUT2D_PATH "data/footprints.shp" \
  -OUTPUT_FILE_PATH "output" \
  -OUTPUT_FILE_NAME "mycity" \
  -GRAPH_PATH "data/street_network.geojson" \
  -CLASS_BUILDING 6 \
  -CLASS_GROUND 2 \
  -EXTERNAL_BOUNDARY_GROUND "data/ground_polygon.off"
```


### Custom, Debug, or Batch Processing

For custom configurations, debugging, or batch processing, CityLODer can be run directly through Docker.

The repository includes a `launcher.sh` script (in the `misc` folder) that shows an example direct Docker invocation. You can edit this script to match your dataset paths, output folders, classification codes, and optional street network input.

A direct Docker command has the following structure:

```bash
docker run --rm \
  -u $(id -u):$(id -g) \
  -v "$(pwd)/data:/data" \
  cityloder \
  /data/point_cloud.las \
  /data/footprints.shp \
  /data/output \
  mycity \
  /data/street_network.geojson \
  6 \
  2 \
  /data/output/tmp
```

On Windows PowerShell, the equivalent command is:

```powershell
docker run --rm `
  -v "$($PWD.Path)\data:/data" `
  cityloder `
  /data/point_cloud.las `
  /data/footprints.shp `
  /data/output `
  mycity `
  /data/street_network.geojson `
  6 `
  2 `
  /data/output/tmp
```

To use an external ground boundary, append the path of the OFF polygon as a ninth argument (e.g., `/data/ground_polygon.off`). Keep it outside the output folder if your script cleans that folder before each run.


## Parameters

### Required Parameters

| Parameter      | Description                              | Example                |
| -------------- | ---------------------------------------- | ---------------------- |
| `LAS_PATH`     | Path to the LAS/LAZ point cloud          | `point_cloud.las` |
| `INPUT2D_PATH` | Path to the building footprint shapefile | `footprints.shp`  |

### Optional Parameters

| Parameter          |  Default | Description                                      |
| ------------------ | -------: | ------------------------------------------------ |
| `OUTPUT_FILE_PATH` | `output` | Output directory                                 |
| `OUTPUT_FILE_NAME` | `mycity` | Base name for output model folder/files          |
| `GRAPH_PATH`       |      `.` | Optional street network GeoJSON. Use `.` to skip |
| `CLASS_BUILDING`   |      `6` | LAS classification code for building points      |
| `CLASS_GROUND`     |      `2` | LAS classification code for ground points        |
| `TEMP_FOLD`        |      `.` | Temporary working directory                      |
| `EXTERNAL_BOUNDARY_GROUND` | `.` | Optional ground boundary OFF polygon. Use `.` to skip |

## Output Files

After successful execution, the output directory contains the generated city model files.

Typical structure:

```text
output/
├── city_JSON.city.json
├── (street_graph_updated.geojson)
└── mycity/
    ├── buildings_mesh.off
    ├── ground_mesh.off
    └── city_mesh.off
```

Depending on whether a street network is provided, `street_graph_updated.geojson` may or may not be generated.

### Output Description

| File                           | Description                                              |
| ------------------------------ | -------------------------------------------------------- |
| `city_JSON.city.json`          | CityJSON model containing reconstructed city geometry    |
| `buildings_mesh.off`           | OFF mesh containing reconstructed buildings              |
| `ground_mesh.off`              | OFF mesh containing the reconstructed ground surface     |
| `city_mesh.off`                | Combined OFF mesh containing buildings and ground        |
| `(street_graph_updated.geojson)` | Optional street graph updated with elevation information |

## Example Workflows

### Simple Processing

```bash
sudo ./cityloder_app \
  -LAS_PATH matera.las \
  -INPUT2D_PATH matera.shp
```

This writes outputs using the default output folder and model name.

### Processing with Custom Output Name

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\city.las" `
  -INPUT2D_PATH "data\buildings.shp" `
  -OUTPUT_FILE_PATH "results" `
  -OUTPUT_FILE_NAME "matera_model"
```

### Processing with Street Network

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\city.las" `
  -INPUT2D_PATH "data\buildings.shp" `
  -OUTPUT_FILE_PATH "results\processed_city" `
  -OUTPUT_FILE_NAME "final_model" `
  -GRAPH_PATH "data\streets.geojson"
```

### Custom Classification Codes

Use this when the input LAS file uses non-standard class codes.

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\custom_classification.las" `
  -INPUT2D_PATH "data\footprints.shp" `
  -OUTPUT_FILE_PATH "output" `
  -CLASS_BUILDING 7 `
  -CLASS_GROUND 3
```

### Processing with an External Ground Boundary

Use this to constrain the ground to a custom area instead of the default convex hull of the point cloud.

**1. Convert the boundary to OFF.** The `tools/boundary_to_off.py` tool reads a 2D boundary polygon from a shapefile and assigns to each vertex the elevation of the nearest point (in XY) of the point cloud. The tool is not part of the Docker image, so the `tools` folder is mounted explicitly:

```bash
docker run --rm \
  -u $(id -u):$(id -g) \
  -v "$(pwd)/data:/data" \
  -v "$(pwd)/tools:/tools" \
  --entrypoint python3 \
  cityloder \
  /tools/boundary_to_off.py \
  /data/boundary.shp \
  /data/point_cloud.las \
  /data/ground_polygon.off \
  --ground-class 2
```

On Windows PowerShell (**TO BE TESTED**), the equivalent command is:

```powershell
docker run --rm `
  -v "$($PWD.Path)\data:/data" `
  -v "$($PWD.Path)\tools:/tools" `
  --entrypoint python3 `
  cityloder `
  /tools/boundary_to_off.py `
  /data/boundary.shp `
  /data/point_cloud.las `
  /data/ground_polygon.off `
  --ground-class 2
```

| Argument         | Default                 | Description                                                         |
| ---------------- | ----------------------- | ------------------------------------------------------------------- |
| `boundary`       | —                       | Boundary polygon shapefile. If it holds several polygons, the largest one is used. Holes are ignored |
| `las`            | —                       | Point cloud used for the elevation                                  |
| `output`         | `./ground_polygon.off`  | Output OFF polygon                                                  |
| `--ground-class` | `2`                     | LAS class used for the elevation. Use `-1` to use all points (e.g., for a DTM) |

The tool prints the maximum and mean XY distance between the boundary vertices and their nearest point: a large value means that part of the boundary lies outside the point cloud.

**2. Run CityLODer with the boundary:**

```bash
./wrapper_linux/cityloder \
  -LAS_PATH "data/point_cloud.las" \
  -INPUT2D_PATH "data/footprints.shp" \
  -EXTERNAL_BOUNDARY_GROUND "data/ground_polygon.off"
```

On Windows PowerShell (**TO BE TESTED**):

```powershell
.\wrapper_windows\cityloder_app.ps1 `
  -LAS_PATH "data\point_cloud.las" `
  -INPUT2D_PATH "data\footprints.shp" `
  -EXTERNAL_BOUNDARY_GROUND "data\ground_polygon.off"
```

The log reports `Using external ground boundary: ...` before running mesh_gen.

## Troubleshooting

### Docker Image Not Found

Make sure the Docker image has been built and that its name matches the one expected by the wrappers. By default, the wrappers expect the image to be named `cityloder`:

```bash
docker build -t cityloder .
````

### Permission Denied on Linux/macOS

Make the wrapper executable:

```bash
chmod +x wrapper_linux/cityloder
```

If Docker permission errors occur, either run with `sudo` or add your user to the Docker group:

```bash
sudo usermod -aG docker $USER
```

Then log out and log back in.

### PowerShell Script Execution Is Disabled

Run:

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Empty or Misaligned Output

Check that all input files use the same coordinate reference system:

* LAS/LAZ point cloud
* Building footprint shapefile
* Optional street network GeoJSON

Coordinate mismatch is one of the most common causes of empty or incorrect results.

### No Building Geometry Generated

Check that:

* the LAS file contains building-classified points,
* the building class code is correct,
* the footprints overlap the point cloud,
* the shapefile geometries are valid.

For example, if buildings are classified as class `7`, use:

```powershell
-CLASS_BUILDING 7
```

### No Ground Mesh Generated

Check that the LAS file contains ground-classified points and that the ground classification code is correct.

The default ground class is `2`.

### External Ground Boundary Not Found

The boundary path is resolved inside the Docker container. With the wrappers, it must be relative to the current folder; with a direct Docker command, it must point inside the mounted `/data` folder.

### Wrong Ground Elevation Along the Boundary

Check that the boundary shapefile and the point cloud use the same coordinate reference system, and that the boundary lies inside the point cloud. Boundary vertices far from the point cloud take the elevation of the nearest point, which may be far away.

## Local Development and Dependencies

CityLODer can be run without Docker using a local Python/C++ environment.

### System Requirements

* Python 3.9+
* C++17-compatible compiler, such as:
  * `g++`
  * `clang`
  * MSVC on Windows
* CMake 3.10+
* Python development headers

### Python Dependencies

* [NumPy](https://numpy.org/) — numerical computing
* [SciPy](https://scipy.org/) — scientific computing
* [Shapely](https://shapely.readthedocs.io/) — geometric operations
* [Laspy](https://github.com/laspy/laspy) — LAS/LAZ point cloud handling
* [pyshp](https://github.com/karoly/pyshp) — shapefile handling
* [pybind11](https://github.com/pybind/pybind11) — Python/C++ bindings
* [tqdm](https://github.com/tqdm/tqdm) — progress bars

### C++ Dependencies

CityLODer also relies on the following open-source C++ libraries:

* [cinolib](https://github.com/maxicino/cinolib) — mesh processing and geometry algorithms
* [nlohmann/json](https://github.com/nlohmann/json) — JSON serialization
* [Eigen](http://eigen.tuxfamily.org/) — linear algebra


## Publication
You can find the CityLODer paper [here](https://www.sciencedirect.com/science/article/pii/S1524070326000184).

### DOI:

```text
(https://doi.org/10.1016/j.gmod.2026.101337)
```

### BibTeX

```bibtex
@article{SORGENTE2026101337,
title = {CityLODer: City models from airborne point clouds},
journal = {Graphical Models},
volume = {147},
pages = {101337},
year = {2026},
issn = {1524-0703},
doi = {https://doi.org/10.1016/j.gmod.2026.101337},
url = {https://www.sciencedirect.com/science/article/pii/S1524070326000184},
author = {Tommaso Sorgente and Elia {Moscoso Thompson} and Chiara Romanengo}
}
```

If you use CityLODer in academic work, please cite the paper above.

## Datasets

A shareable subset of the comparison dataset used in the CityLODer tests (Genoa area) is available on [Zenodo](https://zenodo.org/records/21623262).

## Contributors

CityLODer is developed by:

**CNR — Istituto di Matematica Applicata e Tecnologie Informatiche**

* **Tommaso Sorgente**
  [tommaso.sorgente@cnr.it](mailto:tommaso.sorgente@cnr.it)

* **Elia Moscoso Thompson**
  [elia.moscosothompson@cnr.it](mailto:elia.moscosothompson@cnr.it)

* **Chiara Romanengo**
  [chiara.romanengo@cnr.it](mailto:chiara.romanengo@cnr.it)

## Contributing

Contributions are welcome.

You can contribute by:

* reporting bugs,
* suggesting features,
* improving documentation,
* sharing test data or usage examples.

Please use GitHub issues for discussions.

## Coming soon

Planned or ongoing work:

* [ ] Preprocessing step to conform street data and building footprint data
* [ ] Automatic classification support
