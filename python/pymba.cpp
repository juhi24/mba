#include <iostream>
#include <string>
#include <sstream>
#include <memory>

#include <mba/mba.hpp>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

namespace py = pybind11;

void precondition(bool cond, std::string error_message) {
    if (!cond) throw std::runtime_error(error_message);
}

template <unsigned NDim>
struct python_mba {
    python_mba(
            py::array_t<double> &_lo,
            py::array_t<double> &_hi,
            py::array_t<int>    &_grid,
            py::array_t<double> &_coo,
            py::array_t<double> &_val,
            int max_levels = 8,
            double tol = 1e-8,
            double min_fill = 0.5
            )
    {
        typedef boost::array<size_t, NDim> index;
        typedef boost::array<double, NDim> point;

        py::buffer_info lo   = _lo.request();
        py::buffer_info hi   = _hi.request();
        py::buffer_info grid = _grid.request();
        py::buffer_info coo  = _coo.request();
        py::buffer_info val  = _val.request();

        precondition(lo.ndim == 1 && lo.shape[0] == NDim,
                "lo should be a vector of size " + std::to_string(NDim)
                );
        precondition(hi.ndim == 1 && hi.shape[0] == NDim,
                "hi should be a vector of size " + std::to_string(NDim)
                );
        precondition(grid.ndim == 1 && grid.shape[0] == NDim,
                "grid should be a vector of size " + std::to_string(NDim)
                );
        precondition(coo.ndim == 2 && coo.shape[1] == NDim,
                "coo should be a n x " + std::to_string(NDim) + " matrix"
                );
        precondition(val.ndim == 1 && val.shape[0] == coo.shape[0],
                "coo and val dimensions disagree"
                );

        const size_t n = coo.shape[0];

        const point *coo_begin = static_cast<const point*>(coo.ptr);
        const point *coo_end   = coo_begin + n;

        const double *val_begin = static_cast<const double*>(val.ptr);

        point cmin, cmax;
        index grid_size;

        std::copy_n(static_cast<const double*>(lo.ptr), n, boost::begin(cmin));
        std::copy_n(static_cast<const double*>(hi.ptr), n, boost::begin(cmax));
        std::copy_n(static_cast<const int*>(grid.ptr), n, boost::begin(grid_size));

        m = std::make_shared< mba::MBA<NDim> >(
                cmin, cmax, grid_size, coo_begin, coo_end, val_begin, max_levels, tol, min_fill);
    }

    py::array_t<double> apply(py::array_t<double> &_coo) const {
        typedef boost::array<double, NDim> point;

        py::buffer_info coo = _coo.request();

        const size_t ndim = coo.ndim;

        precondition(ndim >= 2 && coo.shape[ndim-1] == NDim,
                "coo should be a n x " + std::to_string(NDim) + " matrix"
                );

        std::vector<size_t> strides(ndim-1);
        std::vector<size_t> shape(ndim-1);

        shape.back() = coo.shape[ndim-2];
        strides.back() = sizeof(double);
        for(int i = ndim-2; i > 0; --i) {
            shape[i-1] = coo.shape[i-1];
            strides[i-1] = strides[i] * shape[i];
        }

        const size_t n = coo.count / NDim;

        const point *coo_begin = static_cast<const point*>(coo.ptr);
        const point *coo_end   = coo_begin + n;

        std::vector<double> val(n);

        std::transform(coo_begin, coo_end, val.begin(), std::ref(*m));

        return py::array(py::buffer_info(val.data(), sizeof(double),
                    py::format_descriptor<double>::value(),
                    ndim-1, shape, strides));
    }

    std::shared_ptr< mba::MBA<NDim> > m;
};


PYBIND11_PLUGIN(mba) {
    py::module m("mba", "Multilevel B-spline interpolation");

    py::class_< python_mba<2> >(m, "mba2", "Multilevel B-Spline in 2D")
        .def(py::init<
                    py::array_t<double>&,
                    py::array_t<double>&,
                    py::array_t<int>&,
                    py::array_t<double>&,
                    py::array_t<double>&,
                    int, double, double
                >(), "Constructor",
                py::arg("lo"),
                py::arg("hi"),
                py::arg("grid"),
                py::arg("coo"),
                py::arg("val"),
                py::arg("max_levels") = 8,
                py::arg("tol") = 1e-8,
                py::arg("min_fill") = 0.5
            )
        .def("__call__", &python_mba<2>::apply)
        .def("__repr__", [](const python_mba<2> &m){
                std::ostringstream s;
                s << *m.m;
                return s.str();
                })
        ;

    return m.ptr();
}
