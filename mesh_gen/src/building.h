#ifndef BUILDING
#define BUILDING

#include <filesystem>

#include <cinolib/meshes/meshes.h>
#include <cinolib/merge_meshes_at_coincident_vertices.h>
#include <cinolib/connected_components.h>

#include <triangulate_with_holes.h>
#include <auxiliary.h>

namespace cinolib
{

using std::vector;
using std::string;
namespace fs = std::filesystem;

enum Building_Classification {
    building    = 0,
    pavement    = 10,
    roof        = 11,
    facade      = 12,
    pitch       = 20,
    window      = 30,
    door        = 31,
    balcony     = 32
};

class Building
{
private:
    Polygonmesh<> footprint;
    Polygonmesh<> roof;
    Polygonmesh<> facades;
    Polygonmesh<> building;
    uint          building_ID;
    vec3d         center;
    Building_Classification C;

    std::vector<uint> extract_holes_ccs(const Polygonmesh<> &m) const;
    std::vector<uint> extract_holes(const Polygonmesh<> &m) const;
    Trimesh<>         tessellate(const Polygonmesh<> &m) const;

public:
    ~Building() {}
    Building(const Polygonmesh<> &_footprint,
             const Polygonmesh<> &_roof,
             const Polygonmesh<> &_facades,
             const uint          &ID,
             const vec3d         &_center,
             const Building_Classification &_C=Building_Classification::building);

    bool COMPUTED;
    bool is_inside(const Polygonmesh<> &m) const;
    void translate(const vec3d &v);
    void translate_back();

    // footprint operations
    void project_footprint(std::unordered_map<uint, double> &map);
    template<class M, class V, class E, class P>
    void add_footprint_to_mesh(AbstractPolygonMesh<M,V,E,P> &m) const;

    // building operations
    void create_building_mesh();
    void project_building(std::unordered_map<uint, double> &map);
    template<class M, class V, class E, class P>
    void add_building_to_mesh(AbstractPolygonMesh<M,V,E,P> &m) const;

    vec3d get_point_in_footprint() { return pick_point_in_polygon(footprint, 0); }

    // getters
    Polygonmesh<> get_footprint()   const { return footprint; }
    Polygonmesh<> get_roof()        const { return roof; }
    Polygonmesh<> get_facades()     const { return facades; }
    Polygonmesh<> get_building()    const { return building; }
    uint          get_building_ID() const { return building_ID; }
};

// --------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------

Building::Building(const Polygonmesh<> &_footprint,
                   const Polygonmesh<> &_roof,
                   const Polygonmesh<> &_facades,
                   const uint          &ID,
                   const vec3d         &_center,
                   const Building_Classification &_C)
{
    footprint       = _footprint;
    roof            = _roof;
    facades         = _facades;
    building_ID     = ID;
    center          = _center;
    COMPUTED        = false;
    C               = _C;
}

// --------------------------------------------------------------------------------------------

std::vector<uint> Building::extract_holes_ccs(const Polygonmesh<> &m) const
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

// --------------------------------------------------------------------------------------------

std::vector<uint> Building::extract_holes(const Polygonmesh<> &m) const
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

// --------------------------------------------------------------------------------------------

Trimesh<> Building::tessellate(const Polygonmesh<> &m) const
{
    std::vector<std::vector<uint>> tris;
    for (uint pid=0; pid<m.num_polys(); ++pid) {
        std::vector<uint> tess = m.poly_tessellation(pid);
        std::vector<std::vector<uint>> s_tess = polys_from_serialized_vids(tess, 3);
        tris.insert(tris.end(), s_tess.begin(), s_tess.end());
    }
    return Trimesh<>(m.vector_verts(), tris);
}

// --------------------------------------------------------------------------------------------

bool Building::is_inside(const Polygonmesh<> &m) const
{
    for (vec3d &v : footprint.poly_verts(0)) {
        if (!point_in_polygon(m, v, 0)) return false;
    }
    return true;
}

// --------------------------------------------------------------------------------------------

void Building::translate(const vec3d &v)
{
    footprint.translate(v);
    roof.translate(v);
    facades.translate(v);
}

// --------------------------------------------------------------------------------------------

// moves the building back to its original center (when it was created)
void Building::translate_back()
{
    vec3d c = center - footprint.centroid();
    footprint.translate(c);
    roof.translate(c);
    facades.translate(c);
}

// --------------------------------------------------------------------------------------------

void Building::project_footprint(std::unordered_map<uint, double> &map)
{
    project_mesh(footprint, map);
}

// --------------------------------------------------------------------------------------------

template<class M, class V, class E, class P>
void Building::add_footprint_to_mesh(AbstractPolygonMesh<M,V,E,P> &m) const
{
    merge_meshes_at_coincident_vertices(m, footprint, m);
}

// --------------------------------------------------------------------------------------------

void Building::project_building(std::unordered_map<uint, double> &map)
{
    project_mesh(building, map);
}

// --------------------------------------------------------------------------------------------

template<class M, class V, class E, class P>
void Building::add_building_to_mesh(AbstractPolygonMesh<M,V,E,P> &m) const
{
    uint n = m.num_polys();
    merge_meshes_at_coincident_vertices(m, building, m);
    for (uint pid=n; pid<m.num_polys(); ++pid) {
        m.poly_data(pid).label = building_ID;
    }
}

// --------------------------------------------------------------------------------------------

void Building::create_building_mesh()
{
    // store the original z-coordinates of the roof vertices
    std::unordered_map<uint, double> z_map;
    map_z_vals(roof, z_map);

    // triangulate the roof
    std::vector<uint> roof_holes = extract_holes(roof);
    Trimesh<> roof_tri = triangulate_with_holes(roof, roof_holes);
    if (roof_tri.num_verts() != roof.num_verts()) {
        std::cerr << "Building::create_building_mesh - ERROR: triangulation failed for Building "
                  << building_ID << ", roof has " << roof.num_verts() << " verts but the triangulated roof has "
                  << roof_tri.num_verts() << " verts. Check the roof mesh and its holes." << std::endl;
    }
    assert(roof_tri.num_verts() == roof.num_verts());

    // project back to the original z-coordinates
    project_back_mesh(roof_tri, z_map);

    // invert facades normals (if necessary)
    // for (uint pid=0; pid<facades.num_polys(); ++pid) {
    //     facades.poly_flip_winding_order(pid);
    // }
    // facades.update_bbox();

    // convert to trimesh
    Trimesh<> facades_tri = tessellate(facades);

    // merge roof and facades
    merge_meshes_at_coincident_vertices(roof_tri, facades_tri, building);
    building.mesh_data().filename = "building_" + std::to_string(building_ID);
    building.update_bbox();
    COMPUTED = true;
}

}

#endif // BUILDING
