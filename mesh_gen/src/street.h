#ifndef STREET
#define STREET

#include <cinolib/meshes/meshes.h>

#include <auxiliary.h>
#include <json.hpp>

namespace cinolib
{

using std::vector;
using std::string;

enum Street_Classification {
    street      = 0,
    sidewalk    = 20,
    stair       = 21
};

class Street
{
private:
    vec3d v0;
    vec3d v1;
    int   ID;
    Street_Classification C;
    vec3d center;

public:
    ~Street() {}
    Street(const int _ID,
           const vec3d &_v0,
           const vec3d &_v1,
           const vec3d &_center,
           const Street_Classification &_C=Street_Classification::street);

    void set_v0(const vec3d &_v0) { v0 = _v0; }
    void set_v1(const vec3d &_v1) { v1 = _v1; }
    void set_ID(const int _ID)    { ID = _ID; }
    void set_center(const vec3d c){ center = c; }
    void set_classification(const Street_Classification &_C) { C = _C; }

    bool is_inside(const Polygonmesh<> &m) const;

    void add_to_mesh(Polygonmesh<> &m) const;

    void translate(const vec3d &v);
    void translate_back();

    void project(std::unordered_map<uint, double> &z_coords);

    void add_z_coordinate(const vector<vec2d> &ground_points_2D, std::map<uint, double> &z_coords);
    template<class T>
    void add_z_coordinate(const T &tree, std::map<uint, double> &z_coords);

    vec3d get_v0() const { return v0; }
    vec3d get_v1() const { return v1; }
    int   get_ID() const { return ID; }
    Street_Classification get_classification() const { return C; }

    std::tuple<int,vec3d,vec3d> as_tuple() const { return std::make_tuple(ID, v0, v1); }
};

// --------------------------------------------------------------------------------------------
// --------------------------------------------------------------------------------------------

Street::Street(const int _ID,
               const vec3d &_v0,
               const vec3d &_v1,
               const vec3d &_center,
               const Street_Classification &_C)
{
    v0 = _v0;
    v1 = _v1;
    ID = _ID;
    center = _center;
    C = _C;
}

// --------------------------------------------------------------------------------------------

void Street::translate(const vec3d &v)
{
    v0 += v;
    v1 += v;
}

// moves the street back to its original center (when it was created)
void Street::translate_back()
{
    vec3d c = center - 0.5 * (v0 + v1);
    v0 += c;
    v1 += c;
}

// --------------------------------------------------------------------------------------------

void Street::project(std::unordered_map<uint, double> &z_coords)
{
    uint offset = z_coords.size();
    uint id0 = offset;
    uint id1 = offset+1;
    project_point(id0, v0, z_coords);
    project_point(id1, v1, z_coords);
}

// --------------------------------------------------------------------------------------------

bool Street::is_inside(const Polygonmesh<> &m) const
{
    // check that the Street is inside the boundary
    if (!point_in_polygon(m, v0, 0) || !point_in_polygon(m, v1, 0)) {
        return false;
    }
    // check that the Street is not crossing a building
    for (uint pid=1; pid<m.num_polys(); ++pid) {
        if (point_in_polygon(m, v0, pid) || point_in_polygon(m, v1, pid)) {
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------------------------

void Street::add_to_mesh(Polygonmesh<> &m) const
{
    // add the Street points and edges to the mesh
    auto it0 = std::find(m.vector_verts().begin(), m.vector_verts().end(), v0);
    uint vid0 = it0 != m.vector_verts().end() ?
                    std::distance(m.vector_verts().begin(), it0) : m.vert_add(v0);

    auto it1 = std::find(m.vector_verts().begin(), m.vector_verts().end(), v1);
    uint vid1 = it1 != m.vector_verts().end() ?
                    std::distance(m.vector_verts().begin(), it1) : m.vert_add(v1);

    if (vid0 == vid1) {
        std::cout << "Street::add_Street_to_mesh - ERROR: the Street extremities coincide: "
                  << "vid0 = " << vid0 << ", vid1 = " << vid1 << "; "
                  << v0 << " and " << v1 << std::endl;
        // assert(false);
        return;
    }

    int eid = m.edge_id(vid0, vid1);
    if (eid == -1) {
        eid = m.edge_add(vid0, vid1);
    }
    m.edge_data(eid).label = ID;
}

// --------------------------------------------------------------------------------------------

void Street::add_z_coordinate(const vector<vec2d> &ground_points_2D, std::map<uint, double> &z_coords)
{
    // Extract Street data
    vec2d v0_2D = v0.rem_coord();
    vec2d v1_2D = v1.rem_coord();

    // Pick the closest points on the ground
    int vid0 = closest_point(ground_points_2D, v0_2D);
    int vid1 = closest_point(ground_points_2D, v1_2D);

    if (vid0 == -1 || vid1 == -1) {
        std::cerr<<"Street::add_Street_z_coordinate - ERROR: could not find closest ground point"<<std::endl;
        return;
    } else if (vid0 == vid1) {
        std::cout<<"Street::add_Street_z_coordinate - WARNING: the closest points to the extremities of Street " << ID << " coincide"<<std::endl;
    }

    // Add the relative z-coordinates
    v0 = vec3d(v0.x(), v0.y(), z_coords[vid0]);
    v1 = vec3d(v1.x(), v1.y(), z_coords[vid1]);
}

// --------------------------------------------------------------------------------------------

template<class T>
void Street::add_z_coordinate(const T &tree, std::map<uint, double> &z_coords)
{
    // Pick the closest points on the ground
    int vid0 = tree.nearest(v0.rem_coord());
    int vid1 = tree.nearest(v1.rem_coord());

    if (vid0 == -1 || vid1 == -1) {
        std::cerr<<"Street::add_Street_z_coordinate - ERROR: could not find closest ground point"<<std::endl;
        return;
    } else if (vid0 == vid1) {
        std::cout<<"Street::add_Street_z_coordinate - WARNING: the closest points to the extremities of Street " << ID << " coincide"<<std::endl;
    }

    // Add the relative z-coordinates
    v0.z() = z_coords[vid0];
    v1.z() = z_coords[vid1];
}

}

#endif // STREET
