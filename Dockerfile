# ------------------------------------------------------------
# 1. Base image with Python and C++ build tools
# ------------------------------------------------------------
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    python3 python3-pip python3-setuptools python3-dev \
    build-essential cmake git \
    && rm -rf /var/lib/apt/lists/*

# Create workspace for Python scripts
WORKDIR /workspace

# ------------------------------------------------------------
# 2. Install core dependencies
# ------------------------------------------------------------
COPY core/ /core/
RUN pip3 install --no-cache-dir -r /core/requirements.txt
RUN pip3 install --no-cache-dir pybind11

RUN c++ -O3 -Wall -shared -std=c++17 -fPIC \
    $(python3 -m pybind11 --includes) \
    /core/fast_inpolygon.cpp \
    -o /core/fast_inpolygon$(python3-config --extension-suffix)

# ------------------------------------------------------------
# 3. Copy mesh_gen repo (build + cinolib + src) and build
# ------------------------------------------------------------
COPY mesh_gen/ /mesh_gen/

RUN test -d /mesh_gen/cinolib && \
    test "$(ls -A /mesh_gen/cinolib)" || \
    (echo "ERROR: /mesh_gen/cinolib not found or folder is empty!" && exit 1)

RUN mkdir -p /mesh_gen/build \
    && cd /mesh_gen/build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release \
    && make -j4

# ------------------------------------------------------------
# 4. Entrypoint
# ------------------------------------------------------------
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
