#include <iomanip>

#include <cinolib/meshes/meshes.h>
#include <cinolib/profiler.h>

#include <city.h>
#include <auxiliary.h>

using namespace cinolib;

int main(int argc, char *argv[])
{
    if (argc != 5) {
        std::cout << "Welcome to CityIconicMesh! Please specify the followings:\n"
                     "- the path to the boundary .off file\n"
                     "- the path to the folder containing the triangulated buildings .off files\n"
                     "- the path to the street graph .geojson file (optional, otherwise use '')\n"
                     "- the path to the output folder\n";
        exit(0);
    }
    std::string boundary_path  = argv[1];
    std::string buildings_path = argv[2];
    std::string streets_path   = argv[3];
    std::string output_path    = argv[4];

    Profiler prof;
    City city;

    prof.push("Load data");
    city.load_boundary_polygon  (boundary_path);
    city.load_buildings_data    (buildings_path);
    city.load_streets_data      (streets_path);
    std::string msg = " found " +
                      std::to_string(city.n_buildings()) + " buildings and " +
                      std::to_string(city.n_streets())   + " streets.";
    double t_load = prof.pop(true, msg);

    prof.push("Create ground mesh");
    city.compute_ground_mesh();
    double t_ground = prof.pop();

    prof.push("Create buildings meshes");
    city.compute_buildings_mesh();
    double t_buildings = prof.pop();

    prof.push("Create city mesh");
    city.compute_city_mesh();
    double t_city = prof.pop();

    double t_total = t_load + t_ground + t_buildings + t_city;

    /*************** SAVE THE RESULT ****************/
    // WARNING: saving meshes translates them back to the original position
    if (!output_path.empty()) {
        prof.push("Save meshes");
        open_directory(output_path);
        city.save(output_path);
        prof.pop();
    }

    /*************** PRINT SUMMARY ****************/
    std::cout << std::string(16, '=') << " Summary " << std::string(16, '=') << std::endl;
    std::cout << std::left;
    std::cout << std::setw(30) << "Buildings in the mesh:"
              << std::right << std::setw(8) << city.n_buildings() << "\n";
    std::cout << std::left << std::setw(30) << "Streets in the mesh:"
              << std::right << std::setw(8) << city.n_streets() << "\n";

    std::cout << std::string(16, '=') << " Timings " << std::string(16, '=') << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << std::left << std::setw(30) << "Load data:"
              << std::right << std::setw(8) << t_load << " s\n";
    std::cout << std::left << std::setw(30) << "Ground mesh:"
              << std::right << std::setw(8) << t_ground << " s\n";
    std::cout << std::left << std::setw(30) << "Buildings mesh:"
              << std::right << std::setw(8) << t_buildings << " s\n";
    std::cout << std::left << std::setw(30) << "City mesh:"
              << std::right << std::setw(8) << t_city << " s\n";
    std::cout << std::left << std::setw(30) << "Total execution time:"
              << std::right << std::setw(8) << t_total << " s\n";
    std::cout << std::string(41, '=') << std::endl;

    return 0;
}
