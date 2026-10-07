#ifndef GROUND
#define GROUND

#include <cinolib/meshes/meshes.h>

#include <auxiliary.h>
#include <triangulate_with_holes.h>
#include <street.h>
#include <building.h>

namespace cinolib
{

using std::vector;
using std::string;

class Ground
{
private:
    Trimesh<>        ground_mesh;
    vector<vec3d>    holes;
    Polygonmesh<>    boundary;
    vector<Building> buildings;
    vector<Street>   streets;
    bool             WITH_STREETS;
    vec3d            scene_center;

    void translate_back();
    void mark_streets(const Polygonmesh<> &m);

public:
    Ground()  {}
    ~Ground() {}

    void setup(const Polygonmesh<>    &boundary_mesh,
               const vector<Building> &buildings_data,
               const vector<Street>   &street_data,
               const vec3d            &center);

    void compute();

    void verify_mesh();

    void save(const string &filename);

    Trimesh<> get_ground()            const { return ground_mesh; }
    const Trimesh<> &get_ground_ref() const { return ground_mesh; }
    const Trimesh<> *get_ground_ptr() const { return &ground_mesh; }
};

// --------------------------------------------------------------------------------------------

void Ground::setup(const Polygonmesh<>    &boundary_mesh,
                   const vector<Building> &buildings_data,
                   const vector<Street>   &street_data,
                   const vec3d            &center)
{
    boundary     = boundary_mesh;
    buildings    = buildings_data;
    streets      = street_data;
    WITH_STREETS = !streets.empty();
    scene_center = center;
}

// --------------------------------------------------------------------------------------------

void Ground::compute()
{
    // start from the boundary and translate it the origin
    Polygonmesh<> m = boundary;
    std::unordered_map<uint, double> z_map;
    project_mesh(m, z_map);

    // add footprints to the mesh
    vector<vec3d> holes;
    int count = 0;
    for (Building B : buildings) {
        std::cout << "\r\033[K"; // deletes the line
        std::cout << "Ground::compute_ground_mesh - building " << count << " / "
                  << buildings.size() << std::flush;
        // project the footprint to the plane z=0
        std::unordered_map<uint, double> footprint_z_map;
        B.project_footprint(footprint_z_map);
        // append footprint_z_map to z_map
        append_map(B.get_footprint().vector_verts(), footprint_z_map, m.vector_verts(), z_map);
        // add the footprint to the mesh
        B.add_footprint_to_mesh(m);
        // add a point inside the footprint to the holes list
        // holes.push_back(pick_point_in_polygon(B.get_footprint(), 0));
        holes.push_back(B.get_point_in_footprint());
        count++;
    }
    std::cout << std::endl;

    // add streets to the mesh
    if (WITH_STREETS) {
        count = 0;
        for (Street s : streets) {
            std::cout << "\r\033[K"; // deletes the line
            std::cout << "Ground::compute - street " << count << " / "
                      << streets.size() << std::flush;
            // project the street to the plane z=0
            std::unordered_map<uint, double> street_z_map;
            s.project(street_z_map);
            // append street_z_map to z_map
            append_map({s.get_v0(), s.get_v1()}, street_z_map, m.vector_verts(), z_map);
            // add the street to the mesh and assign a label to the corresponding edges
            s.add_to_mesh(m);
            count++;
        }
        std::cout << std::endl;
    }
    if (z_map.size() != m.num_verts()) {
        std::cout << "  Ground::compute - ERROR: projection added "
                  << z_map.size() - m.num_verts()
                  << " new vertices in the mesh!" << std::endl;
        exit(0);
    }

    // triangulate the mesh with holes
    ground_mesh = triangulate_with_holes(m, holes);

    // copy edge labels
    if (WITH_STREETS) {
        mark_streets(m);
    }
    if (m.num_verts() != ground_mesh.num_verts()) {
        std::cout << "  Ground::compute - WARNING: triangulation added "
                  << ground_mesh.num_verts() - m.num_verts()
                  << " new vertices in the ground mesh!" << std::endl;
    }
    // translate back to the original position
    project_back_mesh(ground_mesh, z_map);
    ground_mesh.update_bbox();
    ground_mesh.mesh_data().filename = m.mesh_data().filename;
}

// --------------------------------------------------------------------------------------------

void Ground::verify_mesh()
{
    double TOLL_1D = 1e-6;
    double TOLL_2D = 1e-12;

    uint non_manifold_verts = 0;
    for (uint vid=0; vid<ground_mesh.num_verts(); ++vid) {
        if (!ground_mesh.vert_is_manifold(vid)) {
            non_manifold_verts++;
            std::cout << "  non-manifold vertex ID: " << vid << " at position " << ground_mesh.vert(vid) << std::endl;
        }
    }

    uint non_manifold_edges = 0;
    for (uint eid=0; eid<ground_mesh.num_edges(); ++eid) {
        if (!ground_mesh.edge_is_manifold(eid)) {
            non_manifold_edges++;
        }
    }

    uint small_edges = 0;
    for (uint eid=0; eid<ground_mesh.num_edges(); ++eid) {
        if (ground_mesh.edge_length(eid) < TOLL_1D) {
            small_edges++;
        }
    }

    uint small_polys = 0;
    for (uint pid=0; pid<ground_mesh.num_polys(); ++pid) {
        if (ground_mesh.poly_area(pid) < TOLL_2D) {
            small_polys++;
        }
    }

    std::vector<vec3d> verts = ground_mesh.vector_verts();
    REMOVE_DUPLICATES_FROM_VEC(verts);
    uint duplicate_verts = ground_mesh.num_verts() - verts.size();

    std::vector<std::vector<uint>> polys = ground_mesh.vector_polys();
    REMOVE_DUPLICATES_FROM_VEC(polys);
    uint duplicate_polys = ground_mesh.num_polys() - polys.size();

    std::string message = "Ground mesh verification: [";
    if (non_manifold_verts > 0)
        message += "\n  non-manifold vertices: " + std::to_string(non_manifold_verts);
    if (non_manifold_edges > 0)
        message += "\n  non-manifold edges: " + std::to_string(non_manifold_edges);
    if (small_edges > 0)
        message += "\n  small edges (< " + std::to_string(TOLL_1D) + "): " + std::to_string(small_edges);
    if (small_polys > 0)
        message += "\n  small polys (< " + std::to_string(TOLL_2D) + "): " + std::to_string(small_polys);
    if (duplicate_verts > 0)
        message += "\n  duplicate vertices: " + std::to_string(duplicate_verts);
    if (duplicate_polys > 0)
        message += "\n  duplicate polys: " + std::to_string(duplicate_polys);
    message += "\n]\n";
    std::cout << message << std::flush;
}

// --------------------------------------------------------------------------------------------

void Ground::translate_back()
{
    vec3d c = scene_center /*- ground_mesh.centroid()*/;
    ground_mesh.translate(c);
}

// --------------------------------------------------------------------------------------------

void Ground::mark_streets(const Polygonmesh<> &m)
{
    if (!WITH_STREETS) return;

    // mark the edges corresponding to streets
    int count = 0;
    for (uint eid=0; eid<m.num_edges(); ++eid) {
        auto vids = m.edge_vert_ids(eid);
        int eid_tri = ground_mesh.edge_id(vids);
        if (eid_tri == -1) {
            count++;
            continue;
        }
        ground_mesh.edge_data(eid_tri).label = m.edge_data(eid).label;

        // if the edge corresponds to a street, set the z-coordinate of the vertices
        // perchè la z delle strade è sbagliata?
        if (ground_mesh.edge_data(eid_tri).label != -1) {
            for (uint vid : ground_mesh.edge_vert_ids(eid_tri)) {
                double avg_z = 0.;
                for (uint nbr : ground_mesh.adj_v2v(vid)) {
                    avg_z += ground_mesh.vert(nbr).z();
                }
                avg_z /= ground_mesh.adj_v2v(vid).size();
                ground_mesh.vert(vid).z() = avg_z;
            }
        }
    }
    if (count > 0) {
        std::cout << "  Ground::mark_streets - WARNING: triangulation modified " << count
                  << " edges in the ground mesh!" << std::endl;
    }
}

// --------------------------------------------------------------------------------------------

void Ground::save(const string &filename)
{
    translate_back();
    ground_mesh.save(filename.c_str());
}

}

#endif // GROUND
