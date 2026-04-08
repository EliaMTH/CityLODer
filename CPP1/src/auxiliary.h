#ifndef CITY_AUXILIARY
#define CITY_AUXILIARY

#include <cinolib/meshes/meshes.h>

#include <filesystem>

using namespace cinolib;
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
void project(AbstractPolygonMesh<M,V,E,P> &m, std::unordered_map<uint, double> &list)
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
void project_back(AbstractPolygonMesh<M,V,E,P> &m, const std::unordered_map<uint, double> &list)
{
    int counter = 0;
    for (uint vid=0; vid<m.num_verts(); ++vid) {
        auto it = list.find(vid);
        if (it != list.end()) {
            m.vert(vid).z() = it->second;
        } else {
            ++counter;
            // std::cerr << "project_back - ERROR: the vertex does not exist" << std::endl;
            // assert(false);
        }
    }
    m.update_bbox();
    if (counter > 0) {
        std::cerr << "  project_back - WARNING: " << counter
                  << " vertices were not found in the z_map" << std::endl;
    }
}

void project(const uint id, vec3d &p, std::unordered_map<uint, double> &list)
{
    if (list.find(id) == list.end()) {
        list[id] = p.z();
    }
    p.z() = 0.;
}

void project_back(const uint id, vec3d &p, const std::unordered_map<uint, double> &list)
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
vec3d pick_point_in_polygon(const AbstractPolygonMesh<M,V,E,P> &m,
                            const uint pid)
{
    std::vector<uint> tess = m.poly_tessellation(pid);
    vec3d c;
    double eps = .1;
    double shift = .1;
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
    std::cerr << "pick_point_in_polygon - ERROR: could not find a point inside the polygon " << pid << std::endl;
    return c;
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

// print a csv file with the ID of each poly and its label
template<class M, class V, class E, class P>
void print_poly_labels(const AbstractPolygonMesh<M,V,E,P> &m,
                       const std::string &file_path)
{
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file.\n";
        return;
    }

    file << "poly_ID building_ID\n";
    for (uint pid=0; pid<m.num_polys(); ++pid) {
        file << pid << " " << m.poly_data(pid).label << "\n";
    }
    file.close();
}

// print a csv file with the ID of each poly and its label
template<class M, class V, class E, class P>
void print_edge_labels(const AbstractPolygonMesh<M,V,E,P> &m,
                       const std::string &file_path)
{
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file.\n";
        return;
    }

    file << "edge_v0_ID edge_v1_ID edge_ID\n";
    for (uint eid=0; eid<m.num_edges(); ++eid) {
        auto verts = m.edge_vert_ids(eid);
        file << verts.front() << " " << verts.back() << " " << m.edge_data(eid).label << "\n";
    }
    file.close();
}

#endif // CITY_AUXILIARY
