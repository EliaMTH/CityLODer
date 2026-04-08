#ifndef GROUND
#define GROUND

#include <cinolib/meshes/meshes.h>
#include <cinolib/merge_meshes_at_coincident_vertices.h>

#include <auxiliary.h>
#include <triangulate_with_holes.h>

using namespace cinolib;

bool is_footprint_inside(const Polygonmesh<> &footprint,
                         const Polygonmesh<> &ground,
                         const uint pid)
{
    // check that the footprint is contained within the ground polygon
    for (vec3d &v : footprint.poly_verts(pid)) {
        if (!point_in_polygon(ground, v, pid)) {
            return false;
        }
    }
    return true;
}

void add_footprint_to_mesh(const Polygonmesh<> &footprint,
                           Polygonmesh<> &m,
                           std::vector<vec3d> &holes)
{
    // merge the footprint with the ground
    merge_meshes_at_coincident_vertices(m, footprint, m);

    // add a point inside the footprint to the holes list
    vec3d v = pick_point_in_polygon(footprint, 0);
    holes.push_back(v);
}

void add_footprint_to_mesh(const Polygonmesh<> &footprint,
                           Polygonmesh<> &m,
                           std::vector<uint> &holes)
{
    // merge the footprint with the ground
    merge_meshes_at_coincident_vertices(m, footprint, m);

    // add the footprint ID to the footprints list
    std::vector<uint> vlist;
    for (vec3d &v : footprint.poly_verts(0)) {
        uint vid = m.pick_vert(v);
        vlist.push_back(vid);
    }
    int pid = m.poly_id(vlist);
    assert(pid >= 0);
    holes.push_back(pid);
}

Trimesh<> create_ground_mesh(const Polygonmesh<> &boundary,
                             std::vector<std::string> &buildings_dirs)
{
    /*************** BOUNDARY ****************/

    // translate the ground to the origin
    Polygonmesh<> ground = boundary;
    vec3d center = ground.centroid();
    ground.translate(-center);

    std::unordered_map<uint, double> z_map;
    project(ground, z_map);

    /*************** FOOTPRINTS ****************/

    std::vector<vec3d> holes;
    for (int i=buildings_dirs.size()-1; i>=0; --i) {

        // load footprint mesh
        Polygonmesh<> footprint((buildings_dirs.at(i) + "/pavement_polygon.off").c_str());
        footprint.translate(-center);

        std::unordered_map<uint, double> footprint_z_map;
        project(footprint, footprint_z_map);

        // check that the footprint is contained within the ground polygon
        if (!is_footprint_inside(footprint, ground, 0)) {
            std::cout << "  create_ground_mesh - WARNING: footprint outside the ground polygon, discarded: "
                      << footprint.mesh_data().filename << std::endl;
            buildings_dirs.erase(buildings_dirs.begin() + i);
            continue;
        }

        // append map footprint_z_map to map z_map
        append_map(footprint.vector_verts(), footprint_z_map, ground.vector_verts(), z_map);

        // add the footprint to the ground mesh
        add_footprint_to_mesh(footprint, ground, holes);
    }

    /*************** GROUND ****************/

    // triangulate the ground with the buildings footprints
    Trimesh<> ground_tri = triangulate_with_holes(ground, holes);
    if (ground.num_verts() != ground_tri.num_verts()) {
        std::cout << "  create_ground_mesh - WARNING: " << ground_tri.num_verts() - ground.num_verts()
                  << " new vertices in triangulated ground mesh!" << std::endl;
    }

    // translate the mesh back to the original position
    project_back(ground_tri, z_map);
    ground_tri.translate(center);
    ground_tri.update_bbox();
    ground_tri.mesh_data().filename = ground.mesh_data().filename;

    return ground_tri;
}

#endif // GROUND
