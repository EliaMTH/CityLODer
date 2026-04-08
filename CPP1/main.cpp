#include <cinolib/meshes/meshes.h>
#include <cinolib/profiler.h>

#include <ground.h>
#include <buildings.h>
#include <city.h>
#include <auxiliary.h>

using namespace cinolib;

int main(int argc, char *argv[])
{
    if (argc != 4) {
        std::cout << "Welcome to TriCity! Please specify the followings:\n"
                     "- the path to the boundary .off file\n"
                     "- the path to the folder containing the triangulated buildings .off files\n"
                     "- the path to the output folder\n";
        exit(0);
    }
    std::string boundary_path  = argv[1];
    std::string buildings_path = argv[2];
    std::string output_path    = argv[3];

    Profiler prof;

    prof.push("Load data");
    Polygonmesh<> boundary(boundary_path.c_str());
    std::vector<std::string> buildings_dirs = load_buildings_dirs(buildings_path);
    std::string msg = " found " + std::to_string(buildings_dirs.size()) + " buildings.";
    prof.pop(true, msg);

    bool SAVE = !output_path.empty();
    if (SAVE) open_directory(output_path);

    prof.push("Create ground mesh");
    Trimesh<> ground = create_ground_mesh(boundary, buildings_dirs);
    prof.pop();
    if (SAVE) {
        ground.save((output_path + "/ground_mesh.obj").c_str());
    }

    prof.push("Create buildings mesh");
    Trimesh<> buildings = create_buildings_mesh(buildings_dirs);
    prof.pop();
    if (SAVE) {
        buildings.save((output_path + "/buildings_mesh.obj").c_str());
    }

    prof.push("Create city mesh");
    Trimesh<> city = create_city_mesh(ground, buildings);
    prof.pop();
    if (SAVE) {
        city.save((output_path + "/city_mesh.obj").c_str());
    }

    return 0;
}
