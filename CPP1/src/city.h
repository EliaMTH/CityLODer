#ifndef CITY
#define CITY

#include <cinolib/meshes/meshes.h>
#include <cinolib/merge_meshes_at_coincident_vertices.h>

#include <buildings.h>
#include <auxiliary.h>

using namespace cinolib;

void add_buildings_to_mesh(Trimesh<> &city,
                           const std::vector<std::string> &buildings_dirs)
{
    // translate the city to the origin
    vec3d center = city.centroid();
    city.translate(-center);

    // initialize poly labels
    for (uint pid=0; pid<city.num_polys(); ++pid) {
        city.poly_data(pid).label = -1;
    }

    for (const std::string &building_path : buildings_dirs) {

        // generate building mesh
        Trimesh<> building = create_building_mesh(building_path);

        // translate the building to the origin
        building.translate(-center);

        // add the building to the city
        uint n = city.num_polys();
        merge_meshes_at_coincident_vertices(city, building, city);

        // label the new polys with the building ID
        int building_ID = extract_directory_ID(building.mesh_data().filename);
        for (uint pid=n; pid<city.num_polys(); ++pid) {
            city.poly_data(pid).label = building_ID;
        }
    }

    // translate the city back to the original position
    city.translate(center);
    city.update_bbox();
}

Trimesh<> create_city_mesh(const Trimesh<> &ground,
                           const std::vector<Trimesh<>> &buildings)
{
    // initialize the city with the ground mesh
    Trimesh<> city = ground;

    // copy edge labels 
    for (uint eid=0; eid<city.num_edges(); ++eid) {
        city.edge_data(eid).label = ground.edge_data(eid).label;
    }

    // translate the city to the origin
    vec3d center = city.centroid();
    city.translate(-center);

    // initialize poly labels
    for (uint pid=0; pid<city.num_polys(); ++pid) {
        city.poly_data(pid).label = -1;
    }

    for (int i=1; i<buildings.size(); ++i) {
        Trimesh<> building = buildings.at(i);

        // translate the building to the origin
        building.translate(-center);
\
        // add the building to the city
        uint n = city.num_polys();
        merge_meshes_at_coincident_vertices(city, building, city);

        // label the new polys with the building ID
        int building_ID = extract_directory_ID(building.mesh_data().filename);
        for (uint pid=n; pid<city.num_polys(); ++pid) {
            city.poly_data(pid).label = building_ID;
        }
    }

    // translate the city back to the original position
    city.translate(center);
    city.update_bbox();

    return city;
}

Trimesh<> create_city_mesh(const Trimesh<> &ground,
                           const Trimesh<> &buildings)
{
    Trimesh<> city;
    merge_meshes_at_coincident_vertices(buildings, ground, city);

    uint n = buildings.num_polys();
    for (uint pid=0; pid<city.num_polys(); ++pid) {
        city.poly_data(pid).label = (pid < n) ? buildings.poly_data(pid).label : -1;
    }
    return city;
}

#endif // CITY
