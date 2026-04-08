#ifndef CITY_AUXILIARY
#define CITY_AUXILIARY

#include <cinolib/meshes/meshes.h>

#include <filesystem>
#include <iostream>
#include <streambuf>

namespace cinolib
{

namespace fs = std::filesystem;

/*************** GEOMETRIC OPERATIONS ****************/

template<class M, class V, class E, class P>
void map_z_vals(const AbstractPolygonMesh<M,V,E,P> &m, std::unordered_map<uint, double> &list)
{
    for (uint vid=0; vid<m.num_verts(); ++vid) {
        if (list.find(vid) == list.end()) {
            list[vid] = m.vert(vid).z();
        }
    }
}

template<class M, class V, class E, class P>
void project_mesh(AbstractPolygonMesh<M,V,E,P> &m, std::unordered_map<uint, double> &list)
{
    for (uint vid=0; vid<m.num_verts(); ++vid) {
        if (list.find(vid) == list.end()) {
            list[vid] = m.vert(vid).z();
        }
        m.vert(vid).z() = 0.;
    }
    m.update_bbox();
}

template<class M, class V, class E, class P>
void project_back_mesh(AbstractPolygonMesh<M,V,E,P> &m, const std::unordered_map<uint, double> &list)
{
    int counter = 0;
    std::vector<uint> new_vids;
    for (uint vid=0; vid<m.num_verts(); ++vid) {
        auto it = list.find(vid);
        if (it != list.end()) {
            m.vert(vid).z() = it->second;
        } else {
            new_vids.push_back(vid);
            ++counter;
            // std::cerr << "project_back - ERROR: the vertex does not exist" << std::endl;
            // assert(false);
        }
    }
    // set the z-coordinate of the new vertices to the average z-coordinate of their neighbors (excluding other new vertices)
    for (uint vid : new_vids) {
        double z = 0.;
        int n_neigh = 0;
        for (uint nbr : m.adj_v2v(vid)) {
            if (std::find(new_vids.begin(), new_vids.end(), nbr) == new_vids.end()) {
                z += m.vert(nbr).z();
                ++n_neigh;
            }
        }
        if (n_neigh > 0) {
            m.vert(vid).z() = z / n_neigh;
        }
    }
    m.update_bbox();
    if (counter > 0) {
        std::cerr << "  project_back - WARNING: " << counter
                  << " vertices were not found in the z_map, z-coordinate computed manually" << std::endl;
    }
}

void project_point(const uint id, vec3d &p, std::unordered_map<uint, double> &list)
{
    if (list.find(id) == list.end()) {
        list[id] = p.z();
    }
    p.z() = 0.;
}

void project_back_point(const uint id, vec3d &p, const std::unordered_map<uint, double> &list)
{
    auto it = list.find(id);
    if (it != list.end()) {
        p.z() = it->second;
    } else {
        std::cerr << "  project_back - ERROR: the vertex does not exist" << std::endl;
        // assert(false);
    }
}

void append_map(const std::vector<vec3d> &verts1,
                const std::unordered_map<uint, double> &map1,
                const std::vector<vec3d> &verts2,
                std::unordered_map<uint, double> &map2)
{
    assert(verts1.size() == map1.size());
    // assert(verts2.size() == map2.size());

    for (uint vid1=0; vid1<verts1.size(); ++vid1) {

        // if the vertex is already in m2, skip it
        vec3d v = verts1.at(vid1);
        if (std::find(verts2.begin(), verts2.end(), v) != verts2.end()) {
            continue;
        }

        // add the vertex to the map
        uint vid2 = map2.size();
        assert(map2.find(vid2) == map2.end());
        map2[vid2] = map1.at(vid1);
    }
}

bool mesh_has_duplicates(const Polygonmesh<> &m)
{
    std::vector<vec3d> verts = m.vector_verts();
    REMOVE_DUPLICATES_FROM_VEC(verts);
    return (verts.size() != m.num_verts());
}

template<class M, class V, class E, class P>
bool is_mesh_inside(const AbstractPolygonMesh<M,V,E,P> &m0, const AbstractPolygonMesh<M,V,E,P> &m1)
{
    // check if the mesh m1 is completely inside the mesh m0
    assert(m0.num_polys() == 1 && "WARNING - is_mesh_inside: m0 has more than one poly");
    for(uint pid=0; pid<m1.num_polys(); ++pid) {
        for (vec3d &v : m1.poly_verts(pid)) {
            if (!point_in_polygon(m0, v, 0)) {
                return false;
            }
        }
    }
    return true;
}

template<class M, class V, class E, class P>
bool polygons_intersect(const AbstractPolygonMesh<M,V,E,P> &m, const uint pid0, const uint pid1)
{
    assert(m.num_polys() > std::max(pid0, pid1));
    for (uint eid0 : m.adj_p2e(pid0)) {
        for (uint eid1 : m.adj_p2e(pid1)) {
            std::vector<vec3d> verts0 = m.edge_verts(eid0);
            std::vector<vec3d> verts1 = m.edge_verts(eid1);
            if (segment_segment_intersect_2d(verts0[0].rem_coord(), verts0[1].rem_coord(),
                                             verts1[0].rem_coord(), verts1[1].rem_coord())
                != DO_NOT_INTERSECT)
                return true;
        }
    }
    return false;
}

template<class M, class V, class E, class P>
bool point_in_polygon(const AbstractPolygonMesh<M,V,E,P> &m,
                      const vec3d p,
                      const uint pid)
{
    // WARNING: the check is performed projecting onto the plane z=0!
    bool inside = false;
    std::vector<std::vector<uint>> tris = polys_from_serialized_vids(m.poly_tessellation(pid), 3);
    for(std::vector<uint> t : tris) {
        vec3d t0 = m.vert(t[0]);
        vec3d t1 = m.vert(t[1]);
        vec3d t2 = m.vert(t[2]);

        vec2d p2d(p.x(), p.y());
        vec2d t0_2d(t0.x(), t0.y());
        vec2d t1_2d(t1.x(), t1.y());
        vec2d t2_2d(t2.x(), t2.y());

        if (point_in_triangle_2d(p2d, t0_2d, t1_2d, t2_2d)) {
            inside = true;
            break;
        }
    }
    return inside;
}

template<class M, class V, class E, class P>
vec3d pick_point_in_polygon(AbstractPolygonMesh<M,V,E,P> &m,
                            const uint pid)
{
    m.update_p_tessellation(pid);
    std::vector<uint> tess = m.poly_tessellation(pid);
    if (tess.empty()) {
        std::cerr << "pick_point_in_polygon - ERROR: could not triangulate polygon " << pid << std::endl;
        assert(false);
    }
    vec3d c;
    double eps   = 0.1;
    double shift = 0.1;
    for (uint i=0; i<tess.size()-2; i=i+3) {
        vec3d v0 = m.vert(tess.at(i));
        vec3d v1 = m.vert(tess.at(i+1));
        vec3d v2 = m.vert(tess.at(i+2));

        vec3d d1 = v0 - v1;
        vec3d d2 = v2 - v1;
        d1.normalize();
        d2.normalize();

        // avoid aligned edges
        if (d1.cross(d2).norm() > eps) {
            vec3d dir = d1 + d2;
            dir.normalize();
            c = v1 + dir * shift;
            return c;
        }
    }
    std::cerr << "pick_point_in_polygon - ERROR: could not find a point inside polygon " << pid << std::endl;
    return c;
}

int closest_point(const std::vector<vec2d> &points,
                   const vec2d &p)
{
    double MAX_DIST = DBL_MAX;
    int closest = -1;
    for (uint vid=0; vid<points.size(); ++vid) {
        double d = points.at(vid).dist_sqrd(p);
        if (d < MAX_DIST) {
            closest = vid;
            MAX_DIST = d;
        }
    }
    return closest;
}

/*************** IO OPERATIONS ****************/

/* create the directory if it does not exist, otherwise delete its content */
void open_directory(const std::string &path, bool erase = true) {
    if (!fs::exists(path)) {
        // if the folder does not exist, create it
        try {
            fs::create_directories(path);
        } catch (const std::exception &e) {
            std::cerr << "Error creating folder: " << e.what() << std::endl;
            assert(false);
        }
    } else if (erase) {
        // if the folder already exists, delete its content
        for (const auto &entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                fs::remove(entry.path());
            } else if (entry.is_directory()) {
                fs::remove_all(entry.path());
            }
        }
    }
}

// Find the last numeric sequence in the directory name
int extract_directory_ID(const std::string& directory)
{
    if (directory.empty()) return -1;
    std::size_t pos = directory.find_last_not_of("0123456789");
    if (pos == std::string::npos || pos == directory.length() - 1) return -1;
    std::string numberStr = directory.substr(pos + 1);
    return std::stoi(numberStr);
}

template<class M, class V, class E, class P>
DrawablePolygonmesh<> convert_to_drawable(const AbstractPolygonMesh<M,V,E,P> &m)
{
    DrawablePolygonmesh<> dm(m.vector_verts(), m.vector_polys());

    for (uint pid=0; pid<dm.num_polys(); ++pid) {
        dm.poly_data(pid).label = m.poly_data(pid).label;
    }

    for (uint eid=0; eid<dm.num_edges(); ++eid) {
        dm.edge_data(eid).label = m.edge_data(eid).label;
        dm.edge_data(eid).flags[MARKED] = (dm.edge_data(eid).label != -1);
    }

    for (uint vid=0; vid<dm.num_verts(); ++vid) {
        dm.vert_data(vid).label = m.vert_data(vid).label;
        dm.vert_data(vid).flags[MARKED] = (dm.vert_data(vid).label != -1);
        if (dm.vert_data(vid).label != -1) {
            dm.vert_data(vid).color = Color::BLUE();
            // gui.push_marker(dm.vert(vid), "", Color::BLUE(), 2);
        }
    }

    return dm;
}

struct NullBuffer : std::streambuf
{
    int overflow(int c) override { return c; } // discard prints
};

std::streambuf* suppress_stdout()
{
    std::cout<<std::flush;
    static NullBuffer null_buf;
    std::streambuf* old_buf = std::cout.rdbuf(&null_buf); // hides cout prints
    return old_buf;
}

void restore_stdout(std::streambuf* old_buf)
{
    if (old_buf) {
        std::cout.rdbuf(old_buf); // restore cout
        old_buf = nullptr;
    }
    std::cout<<std::flush;
}

}

#endif // CITY_AUXILIARY
