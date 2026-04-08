#ifndef TRIANGULATE_WITH_HOLES_H
#define TRIANGULATE_WITH_HOLES_H

#include <cinolib/meshes/meshes.h>
#include <cinolib/triangle_wrap.h>

using namespace cinolib;

Trimesh<> triangulate_with_holes(const Polygonmesh<> &m, const std::vector<vec3d> &holes)
{
    std::vector<vec2d> verts_in;
    for(uint vid=0; vid<m.num_verts(); ++vid) {
        verts_in.push_back(vec2d(m.vert(vid).x(), m.vert(vid).y()));
    }

    std::vector<uint>  edges_in;
    for(uint eid=0; eid<m.num_edges(); ++eid) {
        edges_in.push_back(m.edge_vert_id(eid,0));
        edges_in.push_back(m.edge_vert_id(eid,1));
    }

    std::vector<vec2d> holes_centers;
    for(const vec3d &v : holes) {
        holes_centers.push_back(vec2d(v.x(), v.y()));
    }

    Trimesh<> m_tri;
    std::string t_flags = "QYY";
    // double angle_min = 30.;
    // double area_max = pow(m.edge_avg_length(), 2) * 10.;
    // t_flags += "q" + std::to_string(angle_min); // angle constraint
    // t_flags += "a" + std::to_string(area_max);  // area constraint
    double z_coord = 0.;
    triangle_wrap(verts_in, edges_in, holes_centers, z_coord, t_flags.c_str(), m_tri);

    return m_tri;
}

Trimesh<> triangulate_with_holes(const Polygonmesh<> &m, const std::vector<uint> &holes)
{
    std::vector<vec2d> verts_in;
    for(uint vid=0; vid<m.num_verts(); ++vid) {
        verts_in.push_back(vec2d(m.vert(vid).x(), m.vert(vid).y()));
    }

    std::vector<uint>  edges_in;
    for(uint eid=0; eid<m.num_edges(); ++eid) {
        edges_in.push_back(m.edge_vert_id(eid,0));
        edges_in.push_back(m.edge_vert_id(eid,1));
    }

    std::vector<vec2d> holes_centers;

    for(uint pid : holes) {
        std::vector<uint> tess = m.poly_tessellation(pid);
        vec3d c;
        for (uint i=0; i<tess.size()-3; i=i+3) {
            vec3d v0 = m.vert(tess.at(i));
            vec3d v1 = m.vert(tess.at(i+1));
            vec3d v2 = m.vert(tess.at(i+2));
            // avoid aligned edges
            if (triangle_area(v0, v1, v2) > 1e-6) {
                // c = (v0 + v1 + v2) / 3.;
                vec3d dir = (v0 + v1 + v2) / 3.;
                c = v0 + (dir-v0) * 1e-4;
                break;
            }
        }
        assert(!c.is_null() && "find_holes: could not find a point inside the polygon");
        holes_centers.push_back(vec2d(c.x(), c.y()));
    }

    Trimesh<> m_tri;
    std::string t_flags = "QYY";
    // double angle_min = 30.;
    // double area_max = pow(m.edge_avg_length(), 2) * 10.;
    // t_flags += "q" + std::to_string(angle_min); // angle constraint
    // t_flags += "a" + std::to_string(area_max);  // area constraint
    double z_coord = 0.;
    triangle_wrap(verts_in, edges_in, holes_centers, z_coord, t_flags.c_str(), m_tri);

    return m_tri;
}

Trimesh<> triangulate_with_holes_and_edges(const Polygonmesh<> &m, const std::vector<uint> &holes, const std::vector<std::pair<uint,uint>> &edges)
{
    std::vector<vec2d> verts_in;
    for(uint vid=0; vid<m.num_verts(); ++vid) {
        verts_in.push_back(vec2d(m.vert(vid).x(), m.vert(vid).y()));
    }

    std::vector<uint>  edges_in;
    for(uint eid=0; eid<m.num_edges(); ++eid) {
        edges_in.push_back(m.edge_vert_id(eid,0));
        edges_in.push_back(m.edge_vert_id(eid,1));
    }
    for(const std::pair<uint,uint> &e : edges) {
        edges_in.push_back(e.first);
        edges_in.push_back(e.second);
    }

    std::vector<vec2d> holes_centers;
    for(uint pid : holes) {
        std::vector<uint> tess = m.poly_tessellation(pid);
        vec3d c;
        for (uint i=0; i<tess.size()-3; i=i+3) {
            vec3d v0 = m.vert(tess.at(i));
            vec3d v1 = m.vert(tess.at(i+1));
            vec3d v2 = m.vert(tess.at(i+2));
            // avoid aligned edges
            if (triangle_area(v0, v1, v2) > 1e-6) {
                c = (v0 + v1 + v2) / 3.;
                break;
            }
        }
        assert(!c.is_null() && "find_holes: could not find a point inside the polygon");

        holes_centers.push_back(vec2d(c.x(), c.y()));
    }

    Trimesh<> m_tri;
    std::string t_flags = "QYY";
    // double angle_min = 30.;
    // double area_max = pow(m.edge_avg_length(), 2) * 10.;
    // t_flags += "q" + std::to_string(angle_min); // angle constraint
    // t_flags += "a" + std::to_string(area_max);  // area constraint
    double z_coord = 0.;
    triangle_wrap(verts_in, edges_in, holes_centers, z_coord, t_flags.c_str(), m_tri);

    return m_tri;
}

#endif // TRIANGULATE_WITH_HOLES_H
