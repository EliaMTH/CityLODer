#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace py = pybind11;

struct Edge
{
    double x1, y1, x2, y2;
    double minx, maxx, miny, maxy;
};

static inline double sqr(double v)
{
    return v * v;
}

static inline double point_segment_distance_sq(
    double px, double py,
    double x1, double y1,
    double x2, double y2)
{
    const double dx = x2 - x1;
    const double dy = y2 - y1;

    if (dx == 0.0 && dy == 0.0)
    {
        return sqr(px - x1) + sqr(py - y1);
    }

    const double t = ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy);
    const double tc = std::max(0.0, std::min(1.0, t));

    const double qx = x1 + tc * dx;
    const double qy = y1 + tc * dy;

    return sqr(px - qx) + sqr(py - qy);
}

class PreparedPolygon
{
public:
    PreparedPolygon(py::array_t<double, py::array::c_style | py::array::forcecast> poly_x,
                    py::array_t<double, py::array::c_style | py::array::forcecast> poly_y)
    {
        auto bx = poly_x.request();
        auto by = poly_y.request();

        if (bx.ndim != 1 || by.ndim != 1)
        {
            throw std::runtime_error("poly_x and poly_y must be 1D arrays");
        }
        if (bx.shape[0] != by.shape[0])
        {
            throw std::runtime_error("poly_x and poly_y must have the same length");
        }
        if (bx.shape[0] < 3)
        {
            throw std::runtime_error("polygon must have at least 3 vertices");
        }

        const auto n0 = static_cast<size_t>(bx.shape[0]);
        const double* xs = static_cast<const double*>(bx.ptr);
        const double* ys = static_cast<const double*>(by.ptr);

        vx_.assign(xs, xs + n0);
        vy_.assign(ys, ys + n0);

        if (n0 > 3 && vx_.front() == vx_.back() && vy_.front() == vy_.back())
        {
            vx_.pop_back();
            vy_.pop_back();
        }

        n_ = vx_.size();
        if (n_ < 3)
        {
            throw std::runtime_error("polygon must have at least 3 distinct vertices");
        }

        minx_ = maxx_ = vx_[0];
        miny_ = maxy_ = vy_[0];

        for (size_t i = 1; i < n_; ++i)
        {
            minx_ = std::min(minx_, vx_[i]);
            maxx_ = std::max(maxx_, vx_[i]);
            miny_ = std::min(miny_, vy_[i]);
            maxy_ = std::max(maxy_, vy_[i]);
        }

        edges_.reserve(n_);
        for (size_t i = 0; i < n_; ++i)
        {
            size_t j = (i + 1) % n_;
            Edge e;
            e.x1 = vx_[i];
            e.y1 = vy_[i];
            e.x2 = vx_[j];
            e.y2 = vy_[j];
            e.minx = std::min(e.x1, e.x2);
            e.maxx = std::max(e.x1, e.x2);
            e.miny = std::min(e.y1, e.y2);
            e.maxy = std::max(e.y1, e.y2);
            edges_.push_back(e);
        }
    }

    py::tuple query(py::array_t<double, py::array::c_style | py::array::forcecast> x,
                    py::array_t<double, py::array::c_style | py::array::forcecast> y,
                    double tol = 1e-9) const
    {
        auto bx = x.request();
        auto by = y.request();

        if (bx.ndim != 1 || by.ndim != 1)
        {
            throw std::runtime_error("x and y must be 1D arrays");
        }
        if (bx.shape[0] != by.shape[0])
        {
            throw std::runtime_error("x and y must have the same length");
        }

        const size_t m = static_cast<size_t>(bx.shape[0]);
        const double* px = static_cast<const double*>(bx.ptr);
        const double* pyv = static_cast<const double*>(by.ptr);

        py::array_t<bool> inside_arr(m);
        py::array_t<bool> on_arr(m);

        auto inside_req = inside_arr.request();
        auto on_req = on_arr.request();

        auto* inside = static_cast<bool*>(inside_req.ptr);
        auto* on = static_cast<bool*>(on_req.ptr);

        const double tol2 = tol * tol;

        for (size_t idx = 0; idx < m; ++idx)
        {
            const double x0 = px[idx];
            const double y0 = pyv[idx];

            bool on_boundary = false;
            bool inside_flag = false;

            if (x0 >= minx_ - tol && x0 <= maxx_ + tol &&
                y0 >= miny_ - tol && y0 <= maxy_ + tol)
            {
                for (const auto& e : edges_)
                {
                    if (x0 < e.minx - tol || x0 > e.maxx + tol ||
                        y0 < e.miny - tol || y0 > e.maxy + tol)
                    {
                        continue;
                    }

                    if (point_segment_distance_sq(x0, y0, e.x1, e.y1, e.x2, e.y2) <= tol2)
                    {
                        on_boundary = true;
                        break;
                    }
                }

                if (!on_boundary)
                {
                    bool c = false;
                    for (const auto& e : edges_)
                    {
                        const bool crosses = ((e.y1 > y0) != (e.y2 > y0));
                        if (crosses)
                        {
                            const double xinters =
                                e.x1 + (y0 - e.y1) * (e.x2 - e.x1) / (e.y2 - e.y1);
                            if (x0 < xinters)
                            {
                                c = !c;
                            }
                        }
                    }
                    inside_flag = c;
                }
            }

            on[idx] = on_boundary;
            inside[idx] = (on_boundary || inside_flag);
        }

        return py::make_tuple(inside_arr, on_arr);
    }

private:
    size_t n_;
    std::vector<double> vx_;
    std::vector<double> vy_;
    std::vector<Edge> edges_;
    double minx_, maxx_, miny_, maxy_;
};

static py::tuple inpolygon(
    py::array_t<double, py::array::c_style | py::array::forcecast> x,
    py::array_t<double, py::array::c_style | py::array::forcecast> y,
    py::array_t<double, py::array::c_style | py::array::forcecast> poly_x,
    py::array_t<double, py::array::c_style | py::array::forcecast> poly_y,
    double tol = 1e-9)
{
    PreparedPolygon prep(poly_x, poly_y);
    return prep.query(x, y, tol);
}

PYBIND11_MODULE(fast_inpolygon, m)
{
    m.doc() = "Fast batch point-in-polygon with MATLAB-like inside/on outputs";

    py::class_<PreparedPolygon>(m, "PreparedPolygon")
        .def(py::init<
             py::array_t<double, py::array::c_style | py::array::forcecast>,
             py::array_t<double, py::array::c_style | py::array::forcecast>>(),
             py::arg("poly_x"),
             py::arg("poly_y"))
        .def("query", &PreparedPolygon::query,
             py::arg("x"),
             py::arg("y"),
             py::arg("tol") = 1e-9);

    m.def("inpolygon", &inpolygon,
          py::arg("x"),
          py::arg("y"),
          py::arg("poly_x"),
          py::arg("poly_y"),
          py::arg("tol") = 1e-9);
}