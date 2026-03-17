#include "NumpyConversions.h"

#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/BoundingRegionBuilder.h>
#include <CesiumGeospatial/BoundingRegionWithLooseFittingHeights.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/EarthGravitationalModel1996Grid.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/EllipsoidTangentPlane.h>
#include <CesiumGeospatial/GeographicProjection.h>
#include <CesiumGeospatial/GlobeAnchor.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/GlobeTransforms.h>
#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumGeospatial/S2CellBoundingVolume.h>
#include <CesiumGeospatial/S2CellID.h>
#include <CesiumGeospatial/SimplePlanarEllipsoidCurve.h>
#include <CesiumGeospatial/WebMercatorProjection.h>
#include <CesiumGeospatial/calcQuadtreeMaxGeometricError.h>
#include <CesiumUtility/Math.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <span>
#include <string>

namespace py = pybind11;
using CesiumPython::isNumpyPointsArray;
using CesiumPython::requirePointsArray;
using CesiumPython::toDmat;
using CesiumPython::toDvec;
using CesiumPython::toNumpy;
using CesiumPython::toNumpyView;

namespace {

std::vector<glm::dvec2> toDvec2Vector(const py::object& vertices) {
  std::vector<glm::dvec2> converted;
  if (isNumpyPointsArray(vertices, 2)) {
    auto arr = requirePointsArray(vertices, 2);
    auto in = arr.unchecked<2>();
    converted.reserve(static_cast<size_t>(in.shape(0)));
    for (py::ssize_t i = 0; i < in.shape(0); ++i) {
      converted.emplace_back(in(i, 0), in(i, 1));
    }
    return converted;
  }

  py::iterable iterable = py::reinterpret_borrow<py::iterable>(vertices);
  converted.reserve(static_cast<size_t>(py::len(iterable)));
  for (py::handle h : iterable) {
    converted.push_back(toDvec<2>(h));
  }
  return converted;
}

py::list fromPlaneSpan(std::span<const CesiumGeometry::Plane> values) {
  py::list out;
  for (const auto& value : values) {
    out.append(py::cast(value));
  }
  return out;
}

py::array_t<double> batchGeodeticSurfaceNormal(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&ellipsoid](const double* in, double* out) {
        glm::dvec3 v =
            ellipsoid.geodeticSurfaceNormal(glm::dvec3(in[0], in[1], in[2]));
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
      });
}

py::array_t<double>
cartographicToNumpy(const CesiumGeospatial::Cartographic& value) {
  py::array_t<double> result({py::ssize_t(3)});
  auto out = result.mutable_unchecked<1>();
  out(0) = value.longitude;
  out(1) = value.latitude;
  out(2) = value.height;
  return result;
}

py::array_t<double> batchCartographicFromDegrees(const py::handle& degrees) {
  constexpr double kDeg2Rad = CesiumUtility::Math::OnePi / 180.0;
  return CesiumPython::batchTransform(
      degrees,
      3,
      3,
      [](const double* in, double* out) {
        constexpr double kD2R = CesiumUtility::Math::OnePi / 180.0;
        out[0] = in[0] * kD2R;
        out[1] = in[1] * kD2R;
        out[2] = in[2];
      });
}

py::array_t<double> batchEllipsoidScaleToGeodeticSurface(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& cartesians) {
  return CesiumPython::batchTransform(
      cartesians,
      3,
      3,
      [&ellipsoid](const double* in, double* out) {
        auto r =
            ellipsoid.scaleToGeodeticSurface(glm::dvec3(in[0], in[1], in[2]));
        if (r) {
          out[0] = r->x;
          out[1] = r->y;
          out[2] = r->z;
        } else {
          constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
          out[0] = kNaN;
          out[1] = kNaN;
          out[2] = kNaN;
        }
      });
}

py::array_t<double> batchEllipsoidCartographicToCartesian(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& cartographics) {
  return CesiumPython::batchTransform(
      cartographics,
      3,
      3,
      [&ellipsoid](const double* in, double* out) {
        CesiumGeospatial::Cartographic c(in[0], in[1], in[2]);
        glm::dvec3 v = ellipsoid.cartographicToCartesian(c);
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
      });
}

py::array_t<double> ellipsoidCartographicToCartesian(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& cartographic) {
  glm::dvec3 c = toDvec<3>(cartographic);
  CesiumGeospatial::Cartographic value(c.x, c.y, c.z);
  return toNumpy(ellipsoid.cartographicToCartesian(value));
}

py::array_t<double> batchEllipsoidCartesianToCartographic(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& cartesians) {
  return CesiumPython::batchTransform(
      cartesians,
      3,
      3,
      [&ellipsoid](const double* in, double* out) {
        auto c =
            ellipsoid.cartesianToCartographic(glm::dvec3(in[0], in[1], in[2]));
        if (c) {
          out[0] = c->longitude;
          out[1] = c->latitude;
          out[2] = c->height;
        } else {
          constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
          out[0] = kNaN;
          out[1] = kNaN;
          out[2] = kNaN;
        }
      });
}

py::array_t<double> ellipsoidCartesianToCartographic(
    const CesiumGeospatial::Ellipsoid& ellipsoid,
    const py::handle& cartesian) {
  glm::dvec3 v = toDvec<3>(cartesian);
  py::array_t<double> result({py::ssize_t(3)});
  auto out = result.mutable_unchecked<1>();
  auto c = ellipsoid.cartesianToCartographic(v);
  if (c) {
    out(0) = c->longitude;
    out(1) = c->latitude;
    out(2) = c->height;
  } else {
    out(0) = std::numeric_limits<double>::quiet_NaN();
    out(1) = std::numeric_limits<double>::quiet_NaN();
    out(2) = std::numeric_limits<double>::quiet_NaN();
  }
  return result;
}

py::array_t<double> batchGeographicProject(
    const CesiumGeospatial::GeographicProjection& projection,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&projection](const double* in, double* out) {
        CesiumGeospatial::Cartographic cartographic(in[0], in[1], in[2]);
        glm::dvec3 projected = projection.project(cartographic);
        out[0] = projected.x;
        out[1] = projected.y;
        out[2] = projected.z;
      });
}

py::array_t<double> batchGeographicUnproject(
    const CesiumGeospatial::GeographicProjection& projection,
    const py::handle& positions) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      positions);
  if (arr.ndim() != 2 || (arr.shape(1) != 2 && arr.shape(1) != 3)) {
    throw py::value_error("Expected numpy shape (N,2) or (N,3).");
  }
  const py::ssize_t rows = arr.shape(0);
  const py::ssize_t cols = arr.shape(1);
  const double* inData = arr.data();
  py::array_t<double> result({rows, py::ssize_t(3)});
  double* outData = static_cast<double*>(result.mutable_data());

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < rows; ++i) {
      const double* row = inData + i * cols;
      CesiumGeospatial::Cartographic cartographic =
          cols == 2 ? projection.unproject(glm::dvec2(row[0], row[1]))
                    : projection.unproject(glm::dvec3(row[0], row[1], row[2]));
      double* out = outData + i * 3;
      out[0] = cartographic.longitude;
      out[1] = cartographic.latitude;
      out[2] = cartographic.height;
    }
  }

  return result;
}

py::array_t<double> batchWebMercatorProject(
    const CesiumGeospatial::WebMercatorProjection& projection,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&projection](const double* in, double* out) {
        CesiumGeospatial::Cartographic cartographic(in[0], in[1], in[2]);
        glm::dvec3 projected = projection.project(cartographic);
        out[0] = projected.x;
        out[1] = projected.y;
        out[2] = projected.z;
      });
}

py::array_t<double> batchWebMercatorUnproject(
    const CesiumGeospatial::WebMercatorProjection& projection,
    const py::handle& positions) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      positions);
  if (arr.ndim() != 2 || (arr.shape(1) != 2 && arr.shape(1) != 3)) {
    throw py::value_error("Expected numpy shape (N,2) or (N,3).");
  }
  const py::ssize_t rows = arr.shape(0);
  const py::ssize_t cols = arr.shape(1);
  const double* inData = arr.data();
  py::array_t<double> result({rows, py::ssize_t(3)});
  double* outData = static_cast<double*>(result.mutable_data());

  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < rows; ++i) {
      const double* row = inData + i * cols;
      CesiumGeospatial::Cartographic cartographic =
          cols == 2 ? projection.unproject(glm::dvec2(row[0], row[1]))
                    : projection.unproject(glm::dvec3(row[0], row[1], row[2]));
      double* out = outData + i * 3;
      out[0] = cartographic.longitude;
      out[1] = cartographic.latitude;
      out[2] = cartographic.height;
    }
  }

  return result;
}

py::array_t<double> batchProjectPosition(
    const CesiumGeospatial::Projection& projection,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&projection](const double* in, double* out) {
        CesiumGeospatial::Cartographic cartographic(in[0], in[1], in[2]);
        glm::dvec3 projected =
            CesiumGeospatial::projectPosition(projection, cartographic);
        out[0] = projected.x;
        out[1] = projected.y;
        out[2] = projected.z;
      });
}

py::array_t<double> batchUnprojectPosition(
    const CesiumGeospatial::Projection& projection,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&projection](const double* in, double* out) {
        CesiumGeospatial::Cartographic cartographic =
            CesiumGeospatial::unprojectPosition(
                projection,
                glm::dvec3(in[0], in[1], in[2]));
        out[0] = cartographic.longitude;
        out[1] = cartographic.latitude;
        out[2] = cartographic.height;
      });
}

py::array_t<double> batchTransformPoints(
    const py::handle& positions,
    const std::function<glm::dvec3(const glm::dvec3&)>& transform) {
  return CesiumPython::batchTransform(
      positions,
      3,
      3,
      [&transform](const double* in, double* out) {
        glm::dvec3 mapped = transform(glm::dvec3(in[0], in[1], in[2]));
        out[0] = mapped.x;
        out[1] = mapped.y;
        out[2] = mapped.z;
      });
}

py::array_t<double> batchTangentPlaneProject(
    const CesiumGeospatial::EllipsoidTangentPlane& plane,
    const py::handle& positions) {
  return CesiumPython::batchTransform(
      positions,
      3,
      2,
      [&plane](const double* in, double* out) {
        glm::dvec2 mapped =
            plane.projectPointToNearestOnPlane(glm::dvec3(in[0], in[1], in[2]));
        out[0] = mapped.x;
        out[1] = mapped.y;
      });
}

py::array_t<double> batchS2DistanceSquared(
    const CesiumGeospatial::S2CellBoundingVolume& volume,
    const py::handle& positions) {
  return CesiumPython::batchScalar3d(
      positions,
      3,
      [&volume](const double* row) {
        return volume.computeDistanceSquaredToPosition(
            glm::dvec3(row[0], row[1], row[2]));
      });
}

} // namespace

void initGeospatialBindings(py::module& m) {
  py::enum_<CesiumGeospatial::LocalDirection>(m, "LocalDirection")
      .value("East", CesiumGeospatial::LocalDirection::East)
      .value("North", CesiumGeospatial::LocalDirection::North)
      .value("West", CesiumGeospatial::LocalDirection::West)
      .value("South", CesiumGeospatial::LocalDirection::South)
      .value("Up", CesiumGeospatial::LocalDirection::Up)
      .value("Down", CesiumGeospatial::LocalDirection::Down)
      .export_values();

  py::class_<CesiumGeospatial::Cartographic>(m, "Cartographic")
      .def(
          py::init<double, double, double>(),
          py::arg("longitude"),
          py::arg("latitude"),
          py::arg("height") = 0.0)
      .def_static(
          "from_degrees",
          [](const py::object& lonOrArray,
             std::optional<double> lat,
             double height) -> py::object {
            if (py::isinstance<py::array>(lonOrArray)) {
              py::array arr = py::reinterpret_borrow<py::array>(lonOrArray);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchCartographicFromDegrees(lonOrArray);
              }
            }
            double lon = lonOrArray.cast<double>();
            double latVal = lat.value_or(0.0);
            return py::cast(
                CesiumGeospatial::Cartographic::fromDegrees(
                    lon,
                    latVal,
                    height));
          },
          py::arg("longitude_degrees"),
          py::arg("latitude_degrees") = py::none(),
          py::arg("height") = 0.0,
          "Convert degrees to radians. Accepts (lon, lat, height) scalars or "
          "an (N,3) numpy array of [lon, lat, height] in degrees.")
      .def_readwrite("longitude", &CesiumGeospatial::Cartographic::longitude)
      .def_readwrite("latitude", &CesiumGeospatial::Cartographic::latitude)
      .def_readwrite("height", &CesiumGeospatial::Cartographic::height)
      .def("__eq__", &CesiumGeospatial::Cartographic::operator==)
      .def(
          "__hash__",
          [](const CesiumGeospatial::Cartographic& self) {
            std::size_t h = std::hash<double>{}(self.longitude);
            h ^= std::hash<double>{}(self.latitude) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            h ^= std::hash<double>{}(self.height) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            return h;
          })
      .def("__repr__", [](const CesiumGeospatial::Cartographic& self) {
        return "Cartographic(longitude=" + std::to_string(self.longitude) +
               ", latitude=" + std::to_string(self.latitude) +
               ", height=" + std::to_string(self.height) + ")";
      });

  py::class_<CesiumGeospatial::Ellipsoid>(m, "Ellipsoid")
      .def(
          py::init<double, double, double>(),
          py::arg("x"),
          py::arg("y"),
          py::arg("z"))
      .def_property_readonly_static(
          "WGS84",
          [](py::object) { return CesiumGeospatial::Ellipsoid::WGS84; })
      .def_property_readonly_static(
          "UNIT_SPHERE",
          [](py::object) { return CesiumGeospatial::Ellipsoid::UNIT_SPHERE; })
      .def_property_readonly(
          "radii",
          [](const CesiumGeospatial::Ellipsoid& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getRadii(), base);
          })
      .def(
          "geodetic_surface_normal",
          [](const CesiumGeospatial::Ellipsoid& self,
             const py::object& position) -> py::object {
            if (py::isinstance<py::array>(position)) {
              py::array arr = py::reinterpret_borrow<py::array>(position);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchGeodeticSurfaceNormal(self, position);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                return toNumpy(self.geodeticSurfaceNormal(toDvec<3>(position)));
              }
              throw py::value_error("Expected numpy shape (3,) or (N,3).");
            }
            auto c = py::cast<CesiumGeospatial::Cartographic>(position);
            return toNumpy(self.geodeticSurfaceNormal(c));
          },
          py::arg("position"))
      .def(
          "geodetic_surface_normal",
          [](const CesiumGeospatial::Ellipsoid& self,
             const CesiumGeospatial::Cartographic& position) {
            return toNumpy(self.geodeticSurfaceNormal(position));
          },
          py::arg("position"))
      .def(
          "cartographic_to_cartesian",
          [](const CesiumGeospatial::Ellipsoid& self,
             const CesiumGeospatial::Cartographic& cartographic) {
            glm::dvec3 cartesian = self.cartographicToCartesian(cartographic);
            return CesiumGeospatial::Cartographic(
                cartesian.x,
                cartesian.y,
                cartesian.z);
          },
          py::arg("cartographic"))
      .def(
          "cartographic_to_cartesian",
          [](const CesiumGeospatial::Ellipsoid& self,
             const py::object& cartographic) -> py::object {
            if (py::isinstance<py::array>(cartographic)) {
              py::array arr = py::reinterpret_borrow<py::array>(cartographic);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchEllipsoidCartographicToCartesian(
                    self,
                    cartographic);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                return ellipsoidCartographicToCartesian(self, cartographic);
              }
              throw py::value_error("Expected numpy shape (3,) or (N,3).");
            }
            auto c = py::cast<CesiumGeospatial::Cartographic>(cartographic);
            glm::dvec3 cartesian = self.cartographicToCartesian(c);
            return toNumpy(cartesian);
          },
          py::arg("cartographic"))
      .def(
          "cartesian_to_cartographic",
          [](const CesiumGeospatial::Ellipsoid& self,
             const py::object& cartesian) -> py::object {
            if (py::isinstance<py::array>(cartesian)) {
              py::array arr = py::reinterpret_borrow<py::array>(cartesian);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchEllipsoidCartesianToCartographic(self, cartesian);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                return ellipsoidCartesianToCartographic(self, cartesian);
              }
              throw py::value_error("Expected numpy shape (3,) or (N,3).");
            }
            auto c = py::cast<CesiumGeospatial::Cartographic>(cartesian);
            auto result = self.cartesianToCartographic(
                glm::dvec3(c.longitude, c.latitude, c.height));
            if (!result) {
              return py::none();
            }
            return py::cast(*result);
          },
          py::arg("cartesian"))
      .def(
          "scale_to_geodetic_surface",
          [](const CesiumGeospatial::Ellipsoid& self,
             const py::object& cartesian) -> py::object {
            if (py::isinstance<py::array>(cartesian)) {
              py::array arr = py::reinterpret_borrow<py::array>(cartesian);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchEllipsoidScaleToGeodeticSurface(self, cartesian);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                auto result = self.scaleToGeodeticSurface(toDvec<3>(cartesian));
                if (!result)
                  return py::none();
                return toNumpy(*result);
              }
              throw py::value_error("Expected numpy shape (3,) or (N,3).");
            }
            auto result = self.scaleToGeodeticSurface(toDvec<3>(cartesian));
            if (!result)
              return py::none();
            return toNumpy(*result);
          },
          py::arg("cartesian"))
      .def(
          "scale_to_geocentric_surface",
          [](const CesiumGeospatial::Ellipsoid& self,
             const py::object& cartesian) -> py::object {
            auto result = self.scaleToGeocentricSurface(toDvec<3>(cartesian));
            if (!result) {
              return py::none();
            }
            return toNumpy(*result);
          },
          py::arg("cartesian"))
      .def_property_readonly(
          "maximum_radius",
          &CesiumGeospatial::Ellipsoid::getMaximumRadius)
      .def_property_readonly(
          "minimum_radius",
          &CesiumGeospatial::Ellipsoid::getMinimumRadius)
      .def("__eq__", &CesiumGeospatial::Ellipsoid::operator==)
      .def("__ne__", &CesiumGeospatial::Ellipsoid::operator!=)
      .def("__repr__", [](const CesiumGeospatial::Ellipsoid& self) {
        const auto& r = self.getRadii();
        return "Ellipsoid(x=" + std::to_string(r.x) +
               ", y=" + std::to_string(r.y) + ", z=" + std::to_string(r.z) +
               ")";
      });

  py::class_<CesiumGeospatial::GlobeRectangle>(m, "GlobeRectangle")
      .def(
          py::init<double, double, double, double>(),
          py::arg("west"),
          py::arg("south"),
          py::arg("east"),
          py::arg("north"))
      .def_static(
          "from_degrees",
          &CesiumGeospatial::GlobeRectangle::fromDegrees,
          py::arg("west_degrees"),
          py::arg("south_degrees"),
          py::arg("east_degrees"),
          py::arg("north_degrees"))
      .def_static(
          "from_rectangle_radians",
          &CesiumGeospatial::GlobeRectangle::fromRectangleRadians,
          py::arg("rectangle"))
      .def(
          "to_simple_rectangle",
          &CesiumGeospatial::GlobeRectangle::toSimpleRectangle)
      .def_property_readonly_static(
          "EMPTY",
          [](py::object) { return CesiumGeospatial::GlobeRectangle::EMPTY; })
      .def_property_readonly_static(
          "MAXIMUM",
          [](py::object) { return CesiumGeospatial::GlobeRectangle::MAXIMUM; })
      .def_property(
          "west",
          &CesiumGeospatial::GlobeRectangle::getWest,
          &CesiumGeospatial::GlobeRectangle::setWest)
      .def_property(
          "south",
          &CesiumGeospatial::GlobeRectangle::getSouth,
          &CesiumGeospatial::GlobeRectangle::setSouth)
      .def_property(
          "east",
          &CesiumGeospatial::GlobeRectangle::getEast,
          &CesiumGeospatial::GlobeRectangle::setEast)
      .def_property(
          "north",
          &CesiumGeospatial::GlobeRectangle::getNorth,
          &CesiumGeospatial::GlobeRectangle::setNorth)
      .def_property_readonly(
          "southwest",
          &CesiumGeospatial::GlobeRectangle::getSouthwest)
      .def_property_readonly(
          "southeast",
          &CesiumGeospatial::GlobeRectangle::getSoutheast)
      .def_property_readonly(
          "northwest",
          &CesiumGeospatial::GlobeRectangle::getNorthwest)
      .def_property_readonly(
          "northeast",
          &CesiumGeospatial::GlobeRectangle::getNortheast)
      .def("compute_width", &CesiumGeospatial::GlobeRectangle::computeWidth)
      .def("compute_height", &CesiumGeospatial::GlobeRectangle::computeHeight)
      .def("compute_center", &CesiumGeospatial::GlobeRectangle::computeCenter)
      .def(
          "contains",
          &CesiumGeospatial::GlobeRectangle::contains,
          py::arg("cartographic"))
      .def("is_empty", &CesiumGeospatial::GlobeRectangle::isEmpty)
      .def(
          "compute_intersection",
          &CesiumGeospatial::GlobeRectangle::computeIntersection,
          py::arg("other"))
      .def(
          "compute_union",
          &CesiumGeospatial::GlobeRectangle::computeUnion,
          py::arg("other"))
      .def(
          "compute_normalized_coordinates",
          [](const CesiumGeospatial::GlobeRectangle& self,
             const CesiumGeospatial::Cartographic& cartographic) {
            return toNumpy(self.computeNormalizedCoordinates(cartographic));
          },
          py::arg("cartographic"))
      .def(
          "split_at_anti_meridian",
          &CesiumGeospatial::GlobeRectangle::splitAtAntiMeridian)
      .def("__contains__", &CesiumGeospatial::GlobeRectangle::contains)
      .def_static(
          "equals",
          &CesiumGeospatial::GlobeRectangle::equals,
          py::arg("left"),
          py::arg("right"))
      .def_static(
          "equals_epsilon",
          &CesiumGeospatial::GlobeRectangle::equalsEpsilon,
          py::arg("left"),
          py::arg("right"),
          py::arg("relative_epsilon"))
      .def("__repr__", [](const CesiumGeospatial::GlobeRectangle& self) {
        return "GlobeRectangle(west=" + std::to_string(self.getWest()) +
               ", south=" + std::to_string(self.getSouth()) +
               ", east=" + std::to_string(self.getEast()) +
               ", north=" + std::to_string(self.getNorth()) + ")";
      });

  py::class_<CesiumGeospatial::CartographicPolygon>(m, "CartographicPolygon")
      .def(
          py::init([](const py::object& polygon) {
            return CesiumGeospatial::CartographicPolygon(
                toDvec2Vector(polygon));
          }),
          py::arg("polygon"))
      .def_property_readonly(
          "vertices",
          [](const CesiumGeospatial::CartographicPolygon& self) {
            return toNumpy(self.getVertices());
          })
      .def_property_readonly(
          "indices",
          [](const CesiumGeospatial::CartographicPolygon& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getIndices(), base);
          })
      .def(
          "bounding_rectangle",
          [](const CesiumGeospatial::CartographicPolygon& self) -> py::object {
            const auto& rectangle = self.getBoundingRectangle();
            if (!rectangle) {
              return py::none();
            }
            return py::cast(*rectangle);
          })
      .def_static(
          "rectangle_is_within_polygons",
          &CesiumGeospatial::CartographicPolygon::rectangleIsWithinPolygons,
          py::arg("rectangle"),
          py::arg("cartographic_polygons"))
      .def_static(
          "rectangle_is_outside_polygons",
          &CesiumGeospatial::CartographicPolygon::rectangleIsOutsidePolygons,
          py::arg("rectangle"),
          py::arg("cartographic_polygons"));

  py::class_<CesiumGeospatial::EllipsoidTangentPlane>(
      m,
      "EllipsoidTangentPlane")
      .def(
          py::init([](const py::object& origin,
                      const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumGeospatial::EllipsoidTangentPlane(
                toDvec<3>(origin),
                ellipsoid);
          }),
          py::arg("origin"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          py::init([](const py::object& east_north_up_to_fixed_frame,
                      const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumGeospatial::EllipsoidTangentPlane(
                toDmat<4>(east_north_up_to_fixed_frame),
                ellipsoid);
          }),
          py::arg("east_north_up_to_fixed_frame"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_property_readonly(
          "ellipsoid",
          &CesiumGeospatial::EllipsoidTangentPlane::getEllipsoid,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "origin",
          [](const CesiumGeospatial::EllipsoidTangentPlane& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getOrigin(), base);
          })
      .def_property_readonly(
          "x_axis",
          [](const CesiumGeospatial::EllipsoidTangentPlane& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getXAxis(), base);
          })
      .def_property_readonly(
          "y_axis",
          [](const CesiumGeospatial::EllipsoidTangentPlane& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getYAxis(), base);
          })
      .def_property_readonly(
          "z_axis",
          [](const CesiumGeospatial::EllipsoidTangentPlane& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getZAxis(), base);
          })
      .def_property_readonly(
          "plane",
          &CesiumGeospatial::EllipsoidTangentPlane::getPlane,
          py::return_value_policy::reference_internal)
      .def(
          "project_point_to_nearest_on_plane",
          [](const CesiumGeospatial::EllipsoidTangentPlane& self,
             const py::object& cartesian) -> py::object {
            if (isNumpyPointsArray(cartesian, 3)) {
              return batchTangentPlaneProject(self, cartesian);
            }
            return toNumpy(
                self.projectPointToNearestOnPlane(toDvec<3>(cartesian)));
          },
          py::arg("cartesian"));

  py::class_<CesiumGeospatial::SimplePlanarEllipsoidCurve>(
      m,
      "SimplePlanarEllipsoidCurve")
      .def_static(
          "from_earth_centered_earth_fixed_coordinates",
          [](const CesiumGeospatial::Ellipsoid& ellipsoid,
             const py::object& source_ecef,
             const py::object& destination_ecef) -> py::object {
            auto result = CesiumGeospatial::SimplePlanarEllipsoidCurve::
                fromEarthCenteredEarthFixedCoordinates(
                    ellipsoid,
                    toDvec<3>(source_ecef),
                    toDvec<3>(destination_ecef));
            if (!result) {
              return py::none();
            }
            return py::cast(*result);
          },
          py::arg("ellipsoid"),
          py::arg("source_ecef"),
          py::arg("destination_ecef"))
      .def_static(
          "from_longitude_latitude_height",
          [](const CesiumGeospatial::Ellipsoid& ellipsoid,
             const CesiumGeospatial::Cartographic& source,
             const CesiumGeospatial::Cartographic& destination) -> py::object {
            auto result = CesiumGeospatial::SimplePlanarEllipsoidCurve::
                fromLongitudeLatitudeHeight(ellipsoid, source, destination);
            if (!result) {
              return py::none();
            }
            return py::cast(*result);
          },
          py::arg("ellipsoid"),
          py::arg("source"),
          py::arg("destination"))
      .def(
          "position",
          [](const CesiumGeospatial::SimplePlanarEllipsoidCurve& self,
             double percentage,
             double additionalHeight) {
            return toNumpy(self.getPosition(percentage, additionalHeight));
          },
          py::arg("percentage"),
          py::arg("additional_height") = 0.0);

  py::class_<CesiumGeospatial::S2CellBoundingVolume>(m, "S2CellBoundingVolume")
      .def(
          py::init<
              const CesiumGeospatial::S2CellID&,
              double,
              double,
              const CesiumGeospatial::Ellipsoid&>(),
          py::arg("cell_id"),
          py::arg("minimum_height"),
          py::arg("maximum_height"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_property_readonly(
          "cell_id",
          &CesiumGeospatial::S2CellBoundingVolume::getCellID,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "minimum_height",
          &CesiumGeospatial::S2CellBoundingVolume::getMinimumHeight)
      .def_property_readonly(
          "maximum_height",
          &CesiumGeospatial::S2CellBoundingVolume::getMaximumHeight)
      .def_property_readonly(
          "center",
          [](const CesiumGeospatial::S2CellBoundingVolume& self) {
            return toNumpy(self.getCenter());
          })
      .def_property_readonly(
          "vertices",
          [](const CesiumGeospatial::S2CellBoundingVolume& self) {
            return toNumpy(self.getVertices());
          })
      .def(
          "intersect_plane",
          &CesiumGeospatial::S2CellBoundingVolume::intersectPlane,
          py::arg("plane"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeospatial::S2CellBoundingVolume& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchS2DistanceSquared(self, position);
            }
            return py::cast(
                self.computeDistanceSquaredToPosition(toDvec<3>(position)));
          },
          py::arg("position"))
      .def_property_readonly(
          "bounding_planes",
          [](const CesiumGeospatial::S2CellBoundingVolume& self) {
            return fromPlaneSpan(self.getBoundingPlanes());
          })
      .def(
          "compute_bounding_region",
          &CesiumGeospatial::S2CellBoundingVolume::computeBoundingRegion,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  py::class_<CesiumGeospatial::BoundingRegionBuilder>(
      m,
      "BoundingRegionBuilder")
      .def(py::init<>())
      .def(
          "to_region",
          &CesiumGeospatial::BoundingRegionBuilder::toRegion,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "to_globe_rectangle",
          &CesiumGeospatial::BoundingRegionBuilder::toGlobeRectangle)
      .def_property(
          "pole_tolerance",
          &CesiumGeospatial::BoundingRegionBuilder::getPoleTolerance,
          &CesiumGeospatial::BoundingRegionBuilder::setPoleTolerance)
      .def(
          "expand_to_include_position",
          &CesiumGeospatial::BoundingRegionBuilder::expandToIncludePosition,
          py::arg("position"))
      .def(
          "expand_to_include_globe_rectangle",
          &CesiumGeospatial::BoundingRegionBuilder::
              expandToIncludeGlobeRectangle,
          py::arg("rectangle"));

  py::class_<CesiumGeospatial::EarthGravitationalModel1996Grid>(
      m,
      "EarthGravitationalModel1996Grid")
      .def_static(
          "from_buffer",
          [](const py::bytes& buffer) -> py::object {
            std::vector<std::byte> bytes = CesiumPython::bytesToVector(buffer);
            auto result =
                CesiumGeospatial::EarthGravitationalModel1996Grid::fromBuffer(
                    std::span<const std::byte>(bytes.data(), bytes.size()));
            if (!result) {
              return py::none();
            }
            return py::cast(*result);
          },
          py::arg("buffer"))
      .def(
          "sample_height",
          &CesiumGeospatial::EarthGravitationalModel1996Grid::sampleHeight,
          py::arg("position"))
      .def(
          "sample_height",
          [](const CesiumGeospatial::EarthGravitationalModel1996Grid& self,
             const py::handle& positions) {
            return CesiumPython::batchScalar3d(
                positions,
                3,
                [&self](const double* row) {
                  CesiumGeospatial::Cartographic c(row[0], row[1], row[2]);
                  return self.sampleHeight(c);
                });
          },
          py::arg("positions"));

  py::class_<CesiumGeospatial::GlobeAnchor>(m, "GlobeAnchor")
      .def_static(
          "from_anchor_to_local_transform",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem&
                 local_coordinate_system,
             const py::object& anchor_to_local) {
            return CesiumGeospatial::GlobeAnchor::fromAnchorToLocalTransform(
                local_coordinate_system,
                toDmat<4>(anchor_to_local));
          },
          py::arg("local_coordinate_system"),
          py::arg("anchor_to_local"))
      .def_static(
          "from_anchor_to_fixed_transform",
          [](const py::object& anchor_to_fixed) {
            return CesiumGeospatial::GlobeAnchor::fromAnchorToFixedTransform(
                toDmat<4>(anchor_to_fixed));
          },
          py::arg("anchor_to_fixed"))
      .def(
          py::init([](const py::object& anchor_to_fixed) {
            return CesiumGeospatial::GlobeAnchor(toDmat<4>(anchor_to_fixed));
          }),
          py::arg("anchor_to_fixed"))
      .def(
          "anchor_to_fixed_transform",
          [](const CesiumGeospatial::GlobeAnchor& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getAnchorToFixedTransform(), base);
          })
      .def(
          "set_anchor_to_fixed_transform",
          [](CesiumGeospatial::GlobeAnchor& self,
             const py::object& new_anchor_to_fixed,
             bool adjustOrientation,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            self.setAnchorToFixedTransform(
                toDmat<4>(new_anchor_to_fixed),
                adjustOrientation,
                ellipsoid);
          },
          py::arg("new_anchor_to_fixed"),
          py::arg("adjust_orientation"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "anchor_to_local_transform",
          [](const CesiumGeospatial::GlobeAnchor& self,
             const CesiumGeospatial::LocalHorizontalCoordinateSystem&
                 local_coordinate_system) {
            return toNumpy(
                self.getAnchorToLocalTransform(local_coordinate_system));
          },
          py::arg("local_coordinate_system"))
      .def(
          "set_anchor_to_local_transform",
          [](CesiumGeospatial::GlobeAnchor& self,
             const CesiumGeospatial::LocalHorizontalCoordinateSystem&
                 local_coordinate_system,
             const py::object& new_anchor_to_local,
             bool adjustOrientation,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            self.setAnchorToLocalTransform(
                local_coordinate_system,
                toDmat<4>(new_anchor_to_local),
                adjustOrientation,
                ellipsoid);
          },
          py::arg("local_coordinate_system"),
          py::arg("new_anchor_to_local"),
          py::arg("adjust_orientation"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  py::class_<CesiumGeospatial::BoundingRegion>(m, "BoundingRegion")
      .def(
          py::init<
              const CesiumGeospatial::GlobeRectangle&,
              double,
              double,
              const CesiumGeospatial::Ellipsoid&>(),
          py::arg("rectangle"),
          py::arg("minimum_height"),
          py::arg("maximum_height"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_property_readonly(
          "rectangle",
          &CesiumGeospatial::BoundingRegion::getRectangle,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "minimum_height",
          &CesiumGeospatial::BoundingRegion::getMinimumHeight)
      .def_property_readonly(
          "maximum_height",
          &CesiumGeospatial::BoundingRegion::getMaximumHeight)
      .def_property_readonly(
          "bounding_box",
          &CesiumGeospatial::BoundingRegion::getBoundingBox,
          py::return_value_policy::reference_internal)
      .def(
          "intersect_plane",
          &CesiumGeospatial::BoundingRegion::intersectPlane,
          py::arg("plane"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeospatial::BoundingRegion& self,
             const py::object& position,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return self.computeDistanceSquaredToPosition(
                toDvec<3>(position),
                ellipsoid);
          },
          py::arg("position"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "compute_distance_squared_to_cartographic",
          [](const CesiumGeospatial::BoundingRegion& self,
             const CesiumGeospatial::Cartographic& position,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return self.computeDistanceSquaredToPosition(position, ellipsoid);
          },
          py::arg("position"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "compute_distance_squared_to_known_position",
          [](const CesiumGeospatial::BoundingRegion& self,
             const CesiumGeospatial::Cartographic& cartographic_position,
             const py::object& cartesian_position) {
            return self.computeDistanceSquaredToPosition(
                cartographic_position,
                toDvec<3>(cartesian_position));
          },
          py::arg("cartographic_position"),
          py::arg("cartesian_position"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeospatial::BoundingRegion& self,
             py::array_t<double, py::array::c_style> positions,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            auto buf = positions.unchecked<2>();
            if (buf.shape(1) != 3) {
              throw py::value_error("Expected (N,3) array of positions.");
            }
            const py::ssize_t n = buf.shape(0);
            py::array_t<double> result(n);
            double* out = static_cast<double*>(result.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                out[i] = self.computeDistanceSquaredToPosition(
                    glm::dvec3(buf(i, 0), buf(i, 1), buf(i, 2)),
                    ellipsoid);
              }
            }
            return result;
          },
          py::arg("positions"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "compute_union",
          &CesiumGeospatial::BoundingRegion::computeUnion,
          py::arg("other"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_static(
          "equals",
          &CesiumGeospatial::BoundingRegion::equals,
          py::arg("left"),
          py::arg("right"))
      .def_static(
          "equals_epsilon",
          &CesiumGeospatial::BoundingRegion::equalsEpsilon,
          py::arg("left"),
          py::arg("right"),
          py::arg("relative_epsilon"))
      .def("__repr__", [](const CesiumGeospatial::BoundingRegion& self) {
        return "BoundingRegion(min_height=" +
               std::to_string(self.getMinimumHeight()) +
               ", max_height=" + std::to_string(self.getMaximumHeight()) + ")";
      });

  py::class_<CesiumGeospatial::GlobeTransforms>(m, "GlobeTransforms")
      .def_static(
          "east_north_up_to_fixed_frame",
          [](const py::object& origin,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return toNumpy(
                CesiumGeospatial::GlobeTransforms::eastNorthUpToFixedFrame(
                    toDvec<3>(origin),
                    ellipsoid));
          },
          py::arg("origin"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  py::class_<CesiumGeospatial::LocalHorizontalCoordinateSystem>(
      m,
      "LocalHorizontalCoordinateSystem")
      .def(
          py::init<
              const CesiumGeospatial::Cartographic&,
              CesiumGeospatial::LocalDirection,
              CesiumGeospatial::LocalDirection,
              CesiumGeospatial::LocalDirection,
              double,
              const CesiumGeospatial::Ellipsoid&>(),
          py::arg("origin"),
          py::arg("x_axis_direction") = CesiumGeospatial::LocalDirection::East,
          py::arg("y_axis_direction") = CesiumGeospatial::LocalDirection::North,
          py::arg("z_axis_direction") = CesiumGeospatial::LocalDirection::Up,
          py::arg("scale_to_meters") = 1.0,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          py::init([](const py::object& origin_ecef,
                      CesiumGeospatial::LocalDirection x_axis_direction,
                      CesiumGeospatial::LocalDirection y_axis_direction,
                      CesiumGeospatial::LocalDirection z_axis_direction,
                      double scaleToMeters,
                      const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumGeospatial::LocalHorizontalCoordinateSystem(
                toDvec<3>(origin_ecef),
                x_axis_direction,
                y_axis_direction,
                z_axis_direction,
                scaleToMeters,
                ellipsoid);
          }),
          py::arg("origin_ecef"),
          py::arg("x_axis_direction") = CesiumGeospatial::LocalDirection::East,
          py::arg("y_axis_direction") = CesiumGeospatial::LocalDirection::North,
          py::arg("z_axis_direction") = CesiumGeospatial::LocalDirection::Up,
          py::arg("scale_to_meters") = 1.0,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          py::init([](const py::object& local_to_ecef) {
            return CesiumGeospatial::LocalHorizontalCoordinateSystem(
                toDmat<4>(local_to_ecef));
          }),
          py::arg("local_to_ecef"))
      .def(
          py::init([](const py::object& local_to_ecef,
                      const py::object& ecef_to_local) {
            return CesiumGeospatial::LocalHorizontalCoordinateSystem(
                toDmat<4>(local_to_ecef),
                toDmat<4>(ecef_to_local));
          }),
          py::arg("local_to_ecef"),
          py::arg("ecef_to_local"))
      .def_property_readonly(
          "east",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(1.0, 0.0, 0.0)));
          })
      .def_property_readonly(
          "north",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(0.0, 1.0, 0.0)));
          })
      .def_property_readonly(
          "up",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(0.0, 0.0, 1.0)));
          })
      .def_property_readonly(
          "west",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(-1.0, 0.0, 0.0)));
          })
      .def_property_readonly(
          "south",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(0.0, -1.0, 0.0)));
          })
      .def_property_readonly(
          "down",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            return toNumpy(
                self.localDirectionToEcef(glm::dvec3(0.0, 0.0, -1.0)));
          })
      .def(
          "local_to_ecef_transformation",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getLocalToEcefTransformation(), base);
          })
      .def(
          "ecef_to_local_transformation",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getEcefToLocalTransformation(), base);
          })
      .def(
          "local_position_to_ecef",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self,
             const py::object& local_position) -> py::object {
            if (isNumpyPointsArray(local_position, 3)) {
              return batchTransformPoints(
                  local_position,
                  [&self](const glm::dvec3& v) {
                    return self.localPositionToEcef(v);
                  });
            }
            return toNumpy(self.localPositionToEcef(toDvec<3>(local_position)));
          },
          py::arg("local_position"))
      .def(
          "ecef_position_to_local",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self,
             const py::object& ecef_position) -> py::object {
            if (isNumpyPointsArray(ecef_position, 3)) {
              return batchTransformPoints(
                  ecef_position,
                  [&self](const glm::dvec3& v) {
                    return self.ecefPositionToLocal(v);
                  });
            }
            return toNumpy(self.ecefPositionToLocal(toDvec<3>(ecef_position)));
          },
          py::arg("ecef_position"))
      .def(
          "local_direction_to_ecef",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self,
             const py::object& local_direction) -> py::object {
            if (isNumpyPointsArray(local_direction, 3)) {
              return batchTransformPoints(
                  local_direction,
                  [&self](const glm::dvec3& v) {
                    return self.localDirectionToEcef(v);
                  });
            }
            return toNumpy(
                self.localDirectionToEcef(toDvec<3>(local_direction)));
          },
          py::arg("local_direction"))
      .def(
          "ecef_direction_to_local",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self,
             const py::object& ecef_direction) -> py::object {
            if (isNumpyPointsArray(ecef_direction, 3)) {
              return batchTransformPoints(
                  ecef_direction,
                  [&self](const glm::dvec3& v) {
                    return self.ecefDirectionToLocal(v);
                  });
            }
            return toNumpy(
                self.ecefDirectionToLocal(toDvec<3>(ecef_direction)));
          },
          py::arg("ecef_direction"))
      .def(
          "compute_transformation_to_another_local",
          [](const CesiumGeospatial::LocalHorizontalCoordinateSystem& self,
             const CesiumGeospatial::LocalHorizontalCoordinateSystem& target) {
            return toNumpy(self.computeTransformationToAnotherLocal(target));
          },
          py::arg("target"));

  py::class_<CesiumGeospatial::BoundingRegionWithLooseFittingHeights>(
      m,
      "BoundingRegionWithLooseFittingHeights")
      .def(
          py::init<const CesiumGeospatial::BoundingRegion&>(),
          py::arg("bounding_region"))
      .def(
          "bounding_region",
          &CesiumGeospatial::BoundingRegionWithLooseFittingHeights::
              getBoundingRegion,
          py::return_value_policy::reference_internal)
      .def(
          "compute_conservative_distance_squared_to_position",
          [](const CesiumGeospatial::BoundingRegionWithLooseFittingHeights&
                 self,
             const py::object& position,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return self.computeConservativeDistanceSquaredToPosition(
                toDvec<3>(position),
                ellipsoid);
          },
          py::arg("position"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "compute_conservative_distance_squared_to_cartographic",
          [](const CesiumGeospatial::BoundingRegionWithLooseFittingHeights&
                 self,
             const CesiumGeospatial::Cartographic& position,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return self.computeConservativeDistanceSquaredToPosition(
                position,
                ellipsoid);
          },
          py::arg("position"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "compute_conservative_distance_squared_to_known_position",
          [](const CesiumGeospatial::BoundingRegionWithLooseFittingHeights&
                 self,
             const CesiumGeospatial::Cartographic& cartographic_position,
             const py::object& cartesian_position) {
            return self.computeConservativeDistanceSquaredToPosition(
                cartographic_position,
                toDvec<3>(cartesian_position));
          },
          py::arg("cartographic_position"),
          py::arg("cartesian_position"));

  py::class_<CesiumGeospatial::GeographicProjection>(m, "GeographicProjection")
      .def(
          py::init<const CesiumGeospatial::Ellipsoid&>(),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_property_readonly(
          "ellipsoid",
          &CesiumGeospatial::GeographicProjection::getEllipsoid,
          py::return_value_policy::reference_internal)
      .def(
          "project",
          [](const CesiumGeospatial::GeographicProjection& self,
             const py::object& cartographic) -> py::object {
            if (py::isinstance<py::array>(cartographic)) {
              auto arr = py::reinterpret_borrow<py::array_t<
                  double,
                  py::array::c_style | py::array::forcecast>>(cartographic);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchGeographicProject(self, cartographic);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                CesiumGeospatial::Cartographic c =
                    py::cast<CesiumGeospatial::Cartographic>(cartographic);
                return toNumpy(self.project(c));
              }
              throw py::value_error(
                  "Expected cartographic numpy shape (3,) or (N,3).");
            }
            CesiumGeospatial::Cartographic c =
                py::cast<CesiumGeospatial::Cartographic>(cartographic);
            glm::dvec3 projected = self.project(c);
            return cartographicToNumpy(
                CesiumGeospatial::Cartographic(
                    projected.x,
                    projected.y,
                    projected.z));
          },
          py::arg("cartographic"))
      .def(
          "unproject",
          [](const CesiumGeospatial::GeographicProjection& self,
             const py::object& projected) -> py::object {
            if (py::isinstance<py::array>(projected)) {
              auto arr = py::reinterpret_borrow<py::array_t<
                  double,
                  py::array::c_style | py::array::forcecast>>(projected);
              if (arr.ndim() == 2 && (arr.shape(1) == 2 || arr.shape(1) == 3)) {
                return batchGeographicUnproject(self, projected);
              }
              if (arr.ndim() != 1) {
                throw py::value_error(
                    "Expected projected sequence length 2 or 3.");
              }
              if (arr.shape(0) == 2) {
                return cartographicToNumpy(
                    self.unproject(toDvec<2>(projected)));
              }
              if (arr.shape(0) == 3) {
                return cartographicToNumpy(
                    self.unproject(toDvec<3>(projected)));
              }
              throw py::value_error(
                  "Expected projected sequence length 2 or 3.");
            }
            if (py::isinstance<CesiumGeospatial::Cartographic>(projected)) {
              const CesiumGeospatial::Cartographic& c =
                  py::cast<const CesiumGeospatial::Cartographic&>(projected);
              return cartographicToNumpy(self.unproject(
                  glm::dvec3(c.longitude, c.latitude, c.height)));
            }
            py::sequence seq = py::reinterpret_borrow<py::sequence>(projected);
            if (py::len(seq) == 2) {
              return cartographicToNumpy(self.unproject(toDvec<2>(seq)));
            }
            if (py::len(seq) == 3) {
              return cartographicToNumpy(self.unproject(toDvec<3>(seq)));
            }
            throw py::value_error("Expected projected sequence length 2 or 3.");
          },
          py::arg("projected"))
      .def("__eq__", &CesiumGeospatial::GeographicProjection::operator==)
      .def("__ne__", &CesiumGeospatial::GeographicProjection::operator!=);

  py::class_<CesiumGeospatial::WebMercatorProjection>(
      m,
      "WebMercatorProjection")
      .def(
          py::init<const CesiumGeospatial::Ellipsoid&>(),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_property_readonly_static(
          "MAXIMUM_LATITUDE",
          [](py::object) {
            return CesiumGeospatial::WebMercatorProjection::MAXIMUM_LATITUDE;
          })
      .def_property_readonly(
          "ellipsoid",
          &CesiumGeospatial::WebMercatorProjection::getEllipsoid,
          py::return_value_policy::reference_internal)
      .def(
          "project",
          [](const CesiumGeospatial::WebMercatorProjection& self,
             const py::object& cartographic) -> py::object {
            if (py::isinstance<py::array>(cartographic)) {
              auto arr = py::reinterpret_borrow<py::array_t<
                  double,
                  py::array::c_style | py::array::forcecast>>(cartographic);
              if (arr.ndim() == 2 && arr.shape(1) == 3) {
                return batchWebMercatorProject(self, cartographic);
              }
              if (arr.ndim() == 1 && arr.shape(0) == 3) {
                CesiumGeospatial::Cartographic c =
                    py::cast<CesiumGeospatial::Cartographic>(cartographic);
                return toNumpy(self.project(c));
              }
              throw py::value_error(
                  "Expected cartographic numpy shape (3,) or (N,3).");
            }
            CesiumGeospatial::Cartographic c =
                py::cast<CesiumGeospatial::Cartographic>(cartographic);
            glm::dvec3 projected = self.project(c);
            return cartographicToNumpy(
                CesiumGeospatial::Cartographic(
                    projected.x,
                    projected.y,
                    projected.z));
          },
          py::arg("cartographic"))
      .def(
          "unproject",
          [](const CesiumGeospatial::WebMercatorProjection& self,
             const py::object& projected) -> py::object {
            if (py::isinstance<py::array>(projected)) {
              auto arr = py::reinterpret_borrow<py::array_t<
                  double,
                  py::array::c_style | py::array::forcecast>>(projected);
              if (arr.ndim() == 2 && (arr.shape(1) == 2 || arr.shape(1) == 3)) {
                return batchWebMercatorUnproject(self, projected);
              }
              if (arr.ndim() != 1) {
                throw py::value_error(
                    "Expected projected sequence length 2 or 3.");
              }
              if (arr.shape(0) == 2) {
                return cartographicToNumpy(
                    self.unproject(toDvec<2>(projected)));
              }
              if (arr.shape(0) == 3) {
                return cartographicToNumpy(
                    self.unproject(toDvec<3>(projected)));
              }
              throw py::value_error(
                  "Expected projected sequence length 2 or 3.");
            }
            if (py::isinstance<CesiumGeospatial::Cartographic>(projected)) {
              const CesiumGeospatial::Cartographic& c =
                  py::cast<const CesiumGeospatial::Cartographic&>(projected);
              return cartographicToNumpy(self.unproject(
                  glm::dvec3(c.longitude, c.latitude, c.height)));
            }
            py::sequence seq = py::reinterpret_borrow<py::sequence>(projected);
            if (py::len(seq) == 2) {
              return cartographicToNumpy(self.unproject(toDvec<2>(seq)));
            }
            if (py::len(seq) == 3) {
              return cartographicToNumpy(self.unproject(toDvec<3>(seq)));
            }
            throw py::value_error("Expected projected sequence length 2 or 3.");
          },
          py::arg("projected"))
      .def_static(
          "mercator_angle_to_geodetic_latitude",
          &CesiumGeospatial::WebMercatorProjection::
              mercatorAngleToGeodeticLatitude,
          py::arg("mercator_angle"))
      .def_static(
          "geodetic_latitude_to_mercator_angle",
          &CesiumGeospatial::WebMercatorProjection::
              geodeticLatitudeToMercatorAngle,
          py::arg("latitude"))
      .def("__eq__", &CesiumGeospatial::WebMercatorProjection::operator==)
      .def("__ne__", &CesiumGeospatial::WebMercatorProjection::operator!=);

  py::class_<CesiumGeospatial::Projection>(m, "Projection")
      .def(
          py::init<const CesiumGeospatial::GeographicProjection&>(),
          py::arg("projection"))
      .def(
          py::init<const CesiumGeospatial::WebMercatorProjection&>(),
          py::arg("projection"))
      .def("kind_name", [](const CesiumGeospatial::Projection& self) {
        if (std::holds_alternative<CesiumGeospatial::GeographicProjection>(
                self)) {
          return std::string("geographic");
        }
        if (std::holds_alternative<CesiumGeospatial::WebMercatorProjection>(
                self)) {
          return std::string("web_mercator");
        }
        return std::string("unrecognized");
      });

  py::class_<CesiumGeospatial::S2CellID>(m, "S2CellID")
      .def(py::init<uint64_t>(), py::arg("id"))
      .def_static(
          "from_token",
          &CesiumGeospatial::S2CellID::fromToken,
          py::arg("token"))
      .def_static(
          "from_face_level_position",
          &CesiumGeospatial::S2CellID::fromFaceLevelPosition,
          py::arg("face"),
          py::arg("level"),
          py::arg("position"))
      .def_static(
          "from_quadtree_tile_id",
          &CesiumGeospatial::S2CellID::fromQuadtreeTileID,
          py::arg("face"),
          py::arg("quadtree_tile_id"))
      .def("is_valid", &CesiumGeospatial::S2CellID::isValid)
      .def_property_readonly("id", &CesiumGeospatial::S2CellID::getID)
      .def("to_token", &CesiumGeospatial::S2CellID::toToken)
      .def_property_readonly("level", &CesiumGeospatial::S2CellID::getLevel)
      .def_property_readonly("face", &CesiumGeospatial::S2CellID::getFace)
      .def_property_readonly(
          "center",
          [](const CesiumGeospatial::S2CellID& self) {
            return cartographicToNumpy(self.getCenter());
          })
      .def_property_readonly(
          "vertices",
          [](const CesiumGeospatial::S2CellID& self) {
            auto verts = self.getVertices();
            py::array_t<double> out({py::ssize_t(4), py::ssize_t(3)});
            auto buf = out.mutable_unchecked<2>();
            for (py::ssize_t i = 0; i < 4; ++i) {
              buf(i, 0) = verts[i].longitude;
              buf(i, 1) = verts[i].latitude;
              buf(i, 2) = verts[i].height;
            }
            return out;
          })
      .def_property_readonly("parent", &CesiumGeospatial::S2CellID::getParent)
      .def("child", &CesiumGeospatial::S2CellID::getChild, py::arg("index"))
      .def(
          "compute_bounding_rectangle",
          &CesiumGeospatial::S2CellID::computeBoundingRectangle)
      .def(
          "__eq__",
          [](const CesiumGeospatial::S2CellID& self,
             const CesiumGeospatial::S2CellID& other) {
            return self.getID() == other.getID();
          },
          py::is_operator())
      .def(
          "__hash__",
          [](const CesiumGeospatial::S2CellID& self) {
            return std::hash<uint64_t>{}(self.getID());
          })
      .def("__repr__", [](const CesiumGeospatial::S2CellID& self) {
        return "S2CellID(token='" + self.toToken() +
               "', level=" + std::to_string(self.getLevel()) +
               ", face=" + std::to_string(self.getFace()) + ")";
      });

  m.def(
      "project_position",
      [](const CesiumGeospatial::Projection& projection,
         const py::object& position) -> py::object {
        if (isNumpyPointsArray(position, 3)) {
          return batchProjectPosition(projection, position);
        }
        auto projected = CesiumGeospatial::projectPosition(
            projection,
            py::cast<CesiumGeospatial::Cartographic>(position));
        return toNumpy(projected);
      },
      py::arg("projection"),
      py::arg("position"));

  m.def(
      "unproject_position",
      [](const CesiumGeospatial::Projection& projection,
         const py::object& position) -> py::object {
        if (isNumpyPointsArray(position, 3)) {
          return batchUnprojectPosition(projection, position);
        }
        return cartographicToNumpy(
            CesiumGeospatial::unprojectPosition(
                projection,
                toDvec<3>(position)));
      },
      py::arg("projection"),
      py::arg("position"));

  m.def(
      "projection_ellipsoid",
      [](const CesiumGeospatial::Projection& projection) {
        return CesiumGeospatial::getProjectionEllipsoid(projection);
      },
      py::arg("projection"));

  m.def(
      "project_rectangle_simple",
      &CesiumGeospatial::projectRectangleSimple,
      py::arg("projection"),
      py::arg("rectangle"));

  m.def(
      "unproject_rectangle_simple",
      &CesiumGeospatial::unprojectRectangleSimple,
      py::arg("projection"),
      py::arg("rectangle"));

  m.def(
      "project_region_simple",
      &CesiumGeospatial::projectRegionSimple,
      py::arg("projection"),
      py::arg("region"));

  m.def(
      "unproject_region_simple",
      &CesiumGeospatial::unprojectRegionSimple,
      py::arg("projection"),
      py::arg("box"),
      py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  m.def(
      "compute_projected_rectangle_size",
      [](const CesiumGeospatial::Projection& projection,
         const CesiumGeometry::Rectangle& rectangle,
         double maxHeight,
         const CesiumGeospatial::Ellipsoid& ellipsoid) {
        return toNumpy(
            CesiumGeospatial::computeProjectedRectangleSize(
                projection,
                rectangle,
                maxHeight,
                ellipsoid));
      },
      py::arg("projection"),
      py::arg("rectangle"),
      py::arg("max_height"),
      py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  m.def(
      "calc_quadtree_max_geometric_error",
      &CesiumGeospatial::calcQuadtreeMaxGeometricError,
      py::arg("ellipsoid"));
}
