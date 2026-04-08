#ifndef BUILDINGS
#define BUILDINGS

#include <filesystem>

#include <cinolib/merge_meshes_at_coincident_vertices.h>
#include <cinolib/meshes/meshes.h>
#include <cinolib/connected_components.h>

#include <triangulate_with_holes.h>
#include <auxiliary.h>

using namespace cinolib;
namespace fs = std::filesystem;

std::vector<std::string> load_buildings_dirs(const std::string &buildings_path)
{
    if (!fs::exists(buildings_path)) {
        std::cerr << "the buildings path does not exist" << std::endl;
        assert(false);
    }

    std::vector<std::string> buildings_dirs;
    for (const auto &entry : fs::directory_iterator(buildings_path)) {
        if (!entry.is_directory()) continue;

        std::string dir         = entry.path().string();
        std::string footprint   = dir + "/pavement_polygon.off";
        std::string roof        = dir + "/roof_polygon.off";
        std::string facades     = dir + "/facades.off";

        // check that the building and footprint files exist
        if (!fs::exists(footprint) || !fs::exists(roof) || !fs::exists(facades)) {
            std::cerr << "missing footprint, roof, or facades file" << std::endl;
            assert(false);
        }
        buildings_dirs.push_back(dir);
    }

    if (buildings_dirs.empty()) {
	std::cerr << "buildings_dirs is empty!" << std::endl;
        assert(false);
    }
    return buildings_dirs;
}

std::vector<uint> extract_holes_ccs(const Polygonmesh<> &m)
{
    // assume roofs do contain pitches
    std::vector<uint> holes;
    std::vector<std::unordered_set<uint>> ccs;

    if (connected_components(m, ccs) > 1) {
        // assumes the first ccs is the outer boundary
        ccs.erase(ccs.begin());

        for (const std::unordered_set<uint> &ccs : ccs) {
            std::vector<uint> vlist(ccs.begin(), ccs.end());
            int pid = m.poly_id(vlist);
            assert(pid >= 0);
            holes.push_back(pid);
        }
    }
    return holes;
}

std::vector<uint> extract_holes(const Polygonmesh<> &m)
{
    // assume roofs do not contain pitches
    std::vector<uint> holes;

    if (m.num_polys() > 1) {
        // assumes the first ccs is the outer boundary
        for (uint pid = 1; pid < m.num_polys(); ++pid) {
            holes.push_back(pid);
        }
    }
    return holes;
}

Trimesh<> tessellate(const Polygonmesh<> &m)
{
    std::vector<std::vector<uint>> tris;
    for (uint pid=0; pid<m.num_polys(); ++pid) {
        std::vector<uint> tess = m.poly_tessellation(pid);
        std::vector<std::vector<uint>> s_tess = polys_from_serialized_vids(tess, 3);
        tris.insert(tris.end(), s_tess.begin(), s_tess.end());
    }
    return Trimesh<>(m.vector_verts(), tris);
}

Trimesh<> create_building_mesh(const std::string &building_path)
{
    if (!fs::exists(building_path)) {
        std::cerr << "the building folder does not exist" << std::endl;
        assert(false);
    }

    /********************************* ROOF **********************************/

    // load the roof and translate it to the origin
    Polygonmesh<> roof ((building_path + "/roof_polygon.off").c_str());
    vec3d center = roof.centroid();
    roof.translate(-center);

    // store the original z-coordinates of the roof vertices
    std::unordered_map<uint, double> z_map;
    map_z_vals(roof, z_map);

    // triangulate the roof
    std::vector<uint> roof_holes = extract_holes(roof);
    Trimesh<> roof_tri = triangulate_with_holes(roof, roof_holes);
    assert(roof_tri.num_verts() == roof.num_verts());

    // project back to the original z-coordinates
    project_back(roof_tri, z_map);

    /********************************* FACADE **********************************/

    // load the facades and translate them to the origin (no projection)
    Polygonmesh<> facades ((building_path + "/facades.off").c_str());
    facades.translate(-center);

    // invert facades normals (if necessary)
    // for (uint pid=0; pid<facades.num_polys(); ++pid) {
    //     facades.poly_flip_winding_order(pid);
    // }
    // facades.update_bbox();

    // convert to trimesh
    Trimesh<> facades_tri = tessellate(facades);

    /********************************* BUILDING **********************************/

    // merge roof and facades
    Trimesh<> building;
    merge_meshes_at_coincident_vertices(roof_tri, facades_tri, building);
    building.mesh_data().filename = building_path;

    // translate back to the original position
    building.translate(center);
    building.update_bbox();

    return building;
}

Trimesh<> create_buildings_mesh(const std::vector<std::string> &buildings_dirs)
{
    Trimesh<> m;
    for (const std::string &building_path : buildings_dirs) {
        // generate building mesh
        Trimesh<> building = create_building_mesh(building_path);

        // add the building to the global mesh m
        uint n = m.num_polys();
        merge_meshes_at_coincident_vertices(m, building, m);

        // label the new polys with the building ID
        int building_ID = extract_directory_ID(building.mesh_data().filename);
        for (uint pid=n; pid<m.num_polys(); ++pid) {
            m.poly_data(pid).label = building_ID;
        }
    }
    return m;
}

#endif // BUILDINGS
