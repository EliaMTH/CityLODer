#ifndef CITY
#define CITY

#include <cinolib/meshes/meshes.h>
#include <cinolib/merge_meshes_at_coincident_vertices.h>
// #include <cinolib/drawable_segment_soup.h>

#include <ground.h>
#include <building.h>
#include <street.h>
#include <auxiliary.h>

namespace cinolib
{

using std::vector;
using std::string;

class City
{
private:
    Polygonmesh<>    boundary;
    Trimesh<>        city_mesh;
    Trimesh<>        buildings_mesh;
    Ground           ground;
    vector<Building> buildings;
    vector<Street>   streets;
    bool             WITH_STREETS;
    vec3d            scene_center = vec3d(0,0,0);

    void mark_streets(const Trimesh<> &m);
    void print_poly_labels(const string &file_path) const;
    void print_edge_labels(const string &file_path) const;
    void translate_back();

public:
    City() : ground() {}
    ~City() {}

    void load_boundary_polygon  (const string &filepath);
    void load_buildings_data    (const string &dir_path);
    void load_streets_data      (const string &filepath);

    void compute_ground_mesh();
    void compute_buildings_mesh();
    void compute_city_mesh();

    void save(const string &dir_path);
    void write_streets_data(const string &filepath);
    // void visualize_streets(DrawableSegmentSoup &soup) const;

    uint n_buildings() const { return buildings.size(); }
    uint n_streets()   const { return streets.size();   }
    vec3d get_scene_center() const { return scene_center; }

    Trimesh<> get_city_mesh()       const { return city_mesh; }
    const Trimesh<> &get_city_mesh_ref() const { return city_mesh; }
    const Trimesh<> *get_city_mesh_ptr() const { return &city_mesh; }

    Trimesh<> get_buildings_mesh()       const { return buildings_mesh; }
    const Trimesh<> &get_buildings_mesh_ref() const { return buildings_mesh; }
    const Trimesh<> *get_buildings_mesh_ptr() const { return &buildings_mesh; }

    Trimesh<> get_ground_mesh()       const { return ground.get_ground(); }
    const Trimesh<> &get_ground_mesh_ref() const { return ground.get_ground_ref(); }
    const Trimesh<> *get_ground_mesh_ptr() const { return ground.get_ground_ptr(); }

    vector<Building> get_buildings() const { return buildings; }
    const vector<Building> &get_buildings_ref() const { return buildings; }

    vector<Street>   get_streets()   const { return streets;   }
    const vector<Street>   &get_streets_ref()   const { return streets;   }
    void set_streets(const vector<Street> &S) { streets = S; }
};

// --------------------------------------------------------------------------------------------

void City::translate_back()
{
    vec3d c = scene_center /*- city_mesh.centroid()*/;
    city_mesh.translate(c);
    buildings_mesh.translate(c);
    boundary.translate(c);
}

// --------------------------------------------------------------------------------------------

void City::compute_ground_mesh()
{
    ground.setup(boundary, buildings, streets, scene_center);
    ground.compute();
}

// --------------------------------------------------------------------------------------------

void City::compute_buildings_mesh()
{
    buildings_mesh.clear();
    int count = 0;
    for (Building &B : buildings) {
        std::cout << "\r\033[K"; // deletes the line
        std::cout << "City::compute_buildings_mesh - building " << count << " / "
                  << buildings.size() << std::flush;
        if (!B.COMPUTED) {
            auto buf = suppress_stdout();
            B.create_building_mesh();
            restore_stdout(buf);
        }
        // add the building to the global mesh m
        uint n = buildings_mesh.num_polys();
        merge_meshes_at_coincident_vertices(buildings_mesh, B.get_building(), buildings_mesh);
        // label the new polys with the building ID
        int building_ID = B.get_building_ID();
        for (uint pid=n; pid<buildings_mesh.num_polys(); ++pid) {
            buildings_mesh.poly_data(pid).label = building_ID;
        }
        ++count;
    }
    std::cout << std::endl;
}

// --------------------------------------------------------------------------------------------

void City::compute_city_mesh()
{
    // merge meshes
    const Trimesh<> *ground_mesh = ground.get_ground_ptr();
    if (buildings_mesh.num_polys()==0) {
        compute_buildings_mesh();
    }
    merge_meshes_at_coincident_vertices(*ground_mesh, buildings_mesh, city_mesh);
    // copy mesh labels
    uint n_ground    = ground_mesh->num_polys();
    uint n_buildings = buildings_mesh.num_polys();
    uint n_city      = city_mesh.num_polys();
    uint n           = n_ground + n_buildings;
    for (uint pid=0; pid<n_city; ++pid) {
        if (pid%100 == 0) {
            std::cout << "\r\033[K"; // deletes the line
            std::cout << "City::compute_city_mesh - poly " << pid << " / "
                      << n_city << std::flush;
        }
        if (pid < n_ground) {
            city_mesh.poly_data(pid).label = ground_mesh->poly_data(pid).label;
        } else if (pid < n) {
            city_mesh.poly_data(pid).label = buildings_mesh.poly_data(pid - n_ground).label;
        } else {
            city_mesh.poly_data(pid).label = -1;
        }
    }
    std::cout << std::endl;
    if (WITH_STREETS) {
        mark_streets(*ground_mesh);
    }
}

// --------------------------------------------------------------------------------------------

void City::mark_streets(const Trimesh<> &m)
{
    if (!WITH_STREETS) return;

    // mark the edges corresponding to streets
    int count = 0;
    for (uint eid=0; eid<m.num_edges(); ++eid) {
        auto vids = m.edge_vert_ids(eid);
        int eid_tri = city_mesh.edge_id(vids);
        if (eid_tri == -1) {
            count++;
            continue;
        }
        city_mesh.edge_data(eid_tri).label = m.edge_data(eid).label;

        // if the edge corresponds to a street, set the z-coordinate of the vertices
        // perchè la z delle strade è sbagliata?
        if (city_mesh.edge_data(eid_tri).label != -1) {
            for (uint vid : city_mesh.edge_vert_ids(eid_tri)) {
                double avg_z = 0.;
                for (uint nbr : city_mesh.adj_v2v(vid)) {
                    avg_z += city_mesh.vert(nbr).z();
                }
                avg_z /= city_mesh.adj_v2v(vid).size();
                city_mesh.vert(vid).z() = avg_z;
            }
        }
    }
    if (count > 0) {
        std::cout << "  City::mark_streets - WARNING: " << count
                  << " edges not found in triangulated ground!" << std::endl;
    }
}

// --------------------------------------------------------------------------------------------

void City::save(const string &dir_path)
{
    string city_filename        = dir_path + "/city_mesh.obj";
    string buildings_filename   = dir_path + "/buildings_mesh.obj";
    string ground_filename      = dir_path + "/ground_mesh.obj";
    string poly_labels_filename = dir_path + "/polys_buildings_IDs.csv";
    string edge_labels_filename = dir_path + "/edges_streets_IDs.csv";

    translate_back();

    buildings_mesh.poly_color_wrt_label();
    city_mesh.poly_color_wrt_label();

    city_mesh.save(city_filename.c_str());
    buildings_mesh.save(buildings_filename.c_str());
    ground.save(ground_filename.c_str());
    // print_poly_labels(poly_labels_filename);
    // print_edge_labels(edge_labels_filename);
}

// --------------------------------------------------------------------------------------------

// print a csv file with the ID of each poly and its label
void City::print_poly_labels(const string &file_path) const
{
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file.\n";
        return;
    }
    file << "poly_ID building_ID\n";
    for (uint pid=0; pid<city_mesh.num_polys(); ++pid) {
        file << pid << " " << city_mesh.poly_data(pid).label << "\n";
    }
    file.close();
}

// --------------------------------------------------------------------------------------------

// print a csv file with the ID of each poly and its label
void City::print_edge_labels(const string &file_path) const
{
    std::ofstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file.\n";
        return;
    }
    file << "edge_v0_ID edge_v1_ID edge_ID\n";
    for (uint eid=0; eid<city_mesh.num_edges(); ++eid) {
        auto verts = city_mesh.edge_vert_ids(eid);
        file << verts.front() << " " << verts.back() << " " << city_mesh.edge_data(eid).label << "\n";
    }
    file.close();
}

// --------------------------------------------------------------------------------------------

void City::load_boundary_polygon(const string &filepath)
{
    boundary.load(filepath.c_str());
    if (boundary.num_polys() != 1 && !boundary.mesh_is_manifold()) {
        throw std::runtime_error("Error: the boundary mesh should contain a single (manifold) polygon");
    }
    scene_center = boundary.centroid();
    boundary.translate(-scene_center);
}

// --------------------------------------------------------------------------------------------

void City::load_buildings_data(const string &dir_path)
{
    if (!fs::exists(dir_path)) {
        std::cerr << "load_buildings - ERROR: the buildings path does not exist" << std::endl;
        exit(0);
    }
    for (const auto &entry : fs::directory_iterator(dir_path)) {
        if (!entry.is_directory()) continue;
        string dir            = entry.path().string();
        int    ID             = extract_directory_ID(dir);
        string footprint_file = dir + "/pavement_polygon.off";
        string roof_file      = dir + "/roof_polygon.off";
        string facades_file   = dir + "/facades.off";
        if (!fs::exists(footprint_file) || !fs::exists(roof_file) || !fs::exists(facades_file)) {
            std::cerr << "load_buildings - WARNING: missing footprint, roof, or facades file, "
                      << "discarded Building " << ID << std::endl;
            continue;
        }

        // load data
        // if (ID == 68 ) {
        //     std::cout << "load_buildings - WARNING: could not load footprint mesh, "
        //               << "discarded Building " << ID << std::endl;
        //     continue;
        // }
        auto buf = suppress_stdout();
        Polygonmesh<> m_footprint(footprint_file.c_str());
        Polygonmesh<> m_roof(roof_file.c_str());
        Polygonmesh<> m_facades(facades_file.c_str());
        restore_stdout(buf);
        vec3d building_center = m_footprint.centroid();
        Building_Classification C = Building_Classification::building;

        // check data
        if (!m_footprint.mesh_is_manifold()) {
            std::cout << "load_buildings - WARNING: footprint mesh is not manifold, "
                      << "discarded Building " << ID << std::endl;
            continue;
        }
        if (mesh_has_duplicates(m_footprint)) {
            std::cout << "load_buildings - WARNING: footprint has duplicate vertices, "
                      << "discarded Building " << ID << std::endl;
            continue;
        }

        // check if the roof polygons intersect (I am using the roof here because the footprint may be non planar)
        bool intersect = false;
        for (uint pid0=0; pid0<m_roof.num_polys(); ++pid0) {
            for (uint pid1=1; pid1<m_roof.num_polys(); ++pid1) {
                if (pid0 == pid1) continue;
                if (polygons_intersect(m_roof, pid0, pid1)) {
                    intersect = true;
                    break;
                }
            }
        }
        if (intersect) {
            std::cout << "load_buildings - WARNING: roof contains intersecting polygons, "
                      << "discarded Building " << ID << std::endl;
            continue;
        }

        Building B(m_footprint, m_roof, m_facades, ID, building_center, C);
        B.translate(-scene_center);
        if (!B.is_inside(boundary)) {
            std::cout << "load_buildings - WARNING: footprint is outside the boundary polygon, "
                      << "discarded Building " << ID << std::endl;
            continue;
        }
        buildings.push_back(B);
    }
    if (buildings.empty()) {
        std::cerr << "load_buildings - ERROR: no buildings found!" << std::endl;
        exit(0);
    }
}

// --------------------------------------------------------------------------------------------

void City::load_streets_data(const string &filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cout << "load_streets_GeoJSON - Failed to open Streets file: " << filepath << "\n";
        WITH_STREETS = false;
        return;
    }
    nlohmann::json geojson;
    file >> geojson;
    if (!geojson.contains("features")) {
        std::cerr << "Invalid GeoJSON: no features\n";
    }

    uint count = 0;
    for (const auto &feature : geojson["features"]) {
        if (!feature.contains("geometry") || !feature.contains("properties")) continue;
        const auto &geom = feature["geometry"];
        if (geom["type"] != "LineString") continue;
        const auto &props = feature["properties"];
        // ---- Street ID
        int eid = 0;
        if (props.contains("NO")) {
            eid = props["NO"].get<int>();
        } else if (props.contains("osmid")) {
            eid = props["osmid"].get<int>();
        } else continue;
        // ---- Street coordinates
        const auto &coords = geom["coordinates"];
        if (coords.size() < 2) continue;
        // ---- create segments
        for (size_t i=0; i+1<coords.size(); ++i) {
            const auto &p0 = coords[i];
            const auto &p1 = coords[i + 1];
            double x0 = p0[0];
            double y0 = p0[1];
            double z0 = (p0.size() > 2) ? p0[2].get<double>() : 0.0;
            double x1 = p1[0];
            double y1 = p1[1];
            double z1 = (p1.size() > 2) ? p1[2].get<double>() : 0.0;
            vec3d v0(x0, y0, z0);
            vec3d v1(x1, y1, z1);
            vec3d street_center = 0.5 * (v0 + v1);
            Street_Classification C = Street_Classification::street;

            Street s(eid, v0, v1, street_center, C);
            s.translate(-scene_center);
            if (!s.is_inside(boundary)) {
                // std::cout << "load_streets_GeoJSON - WARNING: street segment outside the boundary, discarded: "
                // << eid << std::endl;
                ++count;
                continue;
            }
            streets.push_back(s);
        }
    }
    if (count > 0) {
        std::cout << "load_streets_GeoJSON - WARNING: " << count
                  << " street segments discarded because outside the boundary." << std::endl;
    }
    WITH_STREETS = !streets.empty();
}

// --------------------------------------------------------------------------------------------

void City::write_streets_data(const string &filepath)
{
    vec3d c = scene_center - city_mesh.centroid();
    for (Street &s : streets) {
        s.translate(c);
    }

    nlohmann::json geojson;
    geojson["type"] = "FeatureCollection";
    geojson["name"] = "Street_segments";
    // ---- CRS (EPSG:25833)
    geojson["crs"] = {
        {"type", "name"},
        {"properties", {{"name", "urn:ogc:def:crs:EPSG::25833"}}}
    };
    geojson["features"] = nlohmann::json::array();
    for (auto &s : streets) {
        int sid  = s.get_ID();
        vec3d v0 = s.get_v0();
        vec3d v1 = s.get_v1();
        nlohmann::json feature;
        feature["type"] = "Feature";
        // ---- properties
        feature["properties"] = {
            {"NO", sid}
        };
        // ---- geometry
        nlohmann::json coords = nlohmann::json::array();
        coords.push_back({v0.x(), v0.y(), v0.z()});
        coords.push_back({v1.x(), v1.y(), v1.z()});
        feature["geometry"] = {
            {"type", "LineString"},
            {"coordinates", coords}
        };
        geojson["features"].push_back(feature);
    }
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open output GeoJSON file");
    }
    file << geojson.dump(2); // pretty print
}

// --------------------------------------------------------------------------------------------

// void City::visualize_streets(DrawableSegmentSoup &soup) const
// {
//     for (const Street &s : streets) {
//         soup.push_seg(s.get_v0(), s.get_v1(), Color::BLUE());
//     }
//     soup.thickness = 1.0;
//     soup.no_depth_test = true;
//     soup.use_gl_lines = true;
// }

}

#endif // CITY
