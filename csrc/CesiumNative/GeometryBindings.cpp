#include "NumpyConversions.h"

#include <CesiumGeometry/Availability.h>
#include <CesiumGeometry/Axis.h>
#include <CesiumGeometry/AxisAlignedBox.h>
#include <CesiumGeometry/BoundingCylinderRegion.h>
#include <CesiumGeometry/BoundingSphere.h>
#include <CesiumGeometry/CullingResult.h>
#include <CesiumGeometry/CullingVolume.h>
#include <CesiumGeometry/IntersectionTests.h>
#include <CesiumGeometry/OctreeAvailability.h>
#include <CesiumGeometry/OctreeTileID.h>
#include <CesiumGeometry/OctreeTilingScheme.h>
#include <CesiumGeometry/OrientedBoundingBox.h>
#include <CesiumGeometry/Plane.h>
#include <CesiumGeometry/QuadtreeAvailability.h>
#include <CesiumGeometry/QuadtreeRectangleAvailability.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeometry/QuadtreeTileRectangularRange.h>
#include <CesiumGeometry/QuadtreeTilingScheme.h>
#include <CesiumGeometry/Ray.h>
#include <CesiumGeometry/Rectangle.h>
#include <CesiumGeometry/TileAvailabilityFlags.h>
#include <CesiumGeometry/Transforms.h>
#include <CesiumGeometry/clipTriangleAtAxisAlignedThreshold.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <string>
#include <vector>

namespace py = pybind11;
using CesiumPython::isNumpyPointsArray;
using CesiumPython::requirePointsArray;
using CesiumPython::toDmat;
using CesiumPython::toDquat;
using CesiumPython::toDvec;
using CesiumPython::toNumpy;
using CesiumPython::toNumpyView;

namespace {

py::array_t<bool> batchRectangleContains(
    const CesiumGeometry::Rectangle& rectangle,
    const py::handle& points) {
  return CesiumPython::batchPredicate3d(
      points,
      2,
      [&rectangle](const double* row) {
        return rectangle.contains(glm::dvec2(row[0], row[1]));
      });
}

py::array_t<double> batchRectangleSignedDistance(
    const CesiumGeometry::Rectangle& rectangle,
    const py::handle& points) {
  return CesiumPython::batchScalar3d(
      points,
      2,
      [&rectangle](const double* row) {
        return rectangle.computeSignedDistance(glm::dvec2(row[0], row[1]));
      });
}

py::array_t<bool> batchBoxContains(
    const CesiumGeometry::AxisAlignedBox& box,
    const py::handle& points) {
  return CesiumPython::batchPredicate3d(points, 3, [&box](const double* row) {
    return box.contains(glm::dvec3(row[0], row[1], row[2]));
  });
}

py::array_t<bool> batchSphereContains(
    const CesiumGeometry::BoundingSphere& sphere,
    const py::handle& points) {
  return CesiumPython::batchPredicate3d(
      points,
      3,
      [&sphere](const double* row) {
        return sphere.contains(glm::dvec3(row[0], row[1], row[2]));
      });
}

py::array_t<double> batchSphereDistanceSquared(
    const CesiumGeometry::BoundingSphere& sphere,
    const py::handle& points) {
  return CesiumPython::batchScalar3d(points, 3, [&sphere](const double* row) {
    return sphere.computeDistanceSquaredToPosition(
        glm::dvec3(row[0], row[1], row[2]));
  });
}

py::list batchQuadtreePositionToTile(
    const CesiumGeometry::QuadtreeTilingScheme& scheme,
    const py::handle& points,
    uint32_t level) {
  auto arr = requirePointsArray(points, 2);
  auto in = arr.unchecked<2>();
  py::list out;
  for (py::ssize_t i = 0; i < in.shape(0); ++i) {
    out.append(
        py::cast(scheme.positionToTile(glm::dvec2(in(i, 0), in(i, 1)), level)));
  }
  return out;
}

py::list batchOctreePositionToTile(
    const CesiumGeometry::OctreeTilingScheme& scheme,
    const py::handle& points,
    uint32_t level) {
  auto arr = requirePointsArray(points, 3);
  auto in = arr.unchecked<2>();
  py::list out;
  for (py::ssize_t i = 0; i < in.shape(0); ++i) {
    out.append(
        py::cast(scheme.positionToTile(
            glm::dvec3(in(i, 0), in(i, 1), in(i, 2)),
            level)));
  }
  return out;
}

py::array_t<bool> batchOrientedBoxContains(
    const CesiumGeometry::OrientedBoundingBox& box,
    const py::handle& points) {
  return CesiumPython::batchPredicate3d(points, 3, [&box](const double* row) {
    return box.contains(glm::dvec3(row[0], row[1], row[2]));
  });
}

py::array_t<double> batchOrientedBoxDistanceSquared(
    const CesiumGeometry::OrientedBoundingBox& box,
    const py::handle& points) {
  return CesiumPython::batchScalar3d(points, 3, [&box](const double* row) {
    return box.computeDistanceSquaredToPosition(
        glm::dvec3(row[0], row[1], row[2]));
  });
}

// ---------------------------------------------------------------------------
// Vectorized ray-OBB intersection (N rays vs 1 OBB)
// ---------------------------------------------------------------------------

/// (N,3) origins, (N,3) directions, OBB -> (N,) float64 parametric t (NaN =
/// miss)
py::array_t<double> batchRayObbParametric(
    const py::handle& origins_obj,
    const py::handle& directions_obj,
    const CesiumGeometry::OrientedBoundingBox& obb) {
  return CesiumPython::batchPairedScalar(
      origins_obj,
      directions_obj,
      3,
      [&obb](const double* o, const double* d) {
        CesiumGeometry::Ray ray(
            glm::dvec3(o[0], o[1], o[2]),
            glm::dvec3(d[0], d[1], d[2]));
        auto t = CesiumGeometry::IntersectionTests::rayOBBParametric(ray, obb);
        return t.has_value() ? *t : std::numeric_limits<double>::quiet_NaN();
      });
}

/// (N,3) origins, (N,3) directions, OBB -> (N,3) ECEF hit points (NaN = miss)
py::array_t<double> batchRayObb(
    const py::handle& origins_obj,
    const py::handle& directions_obj,
    const CesiumGeometry::OrientedBoundingBox& obb) {
  return CesiumPython::batchPairedTransform(
      origins_obj,
      directions_obj,
      3,
      3,
      [&obb](const double* o, const double* d, double* out) {
        CesiumGeometry::Ray ray(
            glm::dvec3(o[0], o[1], o[2]),
            glm::dvec3(d[0], d[1], d[2]));
        auto hit = CesiumGeometry::IntersectionTests::rayOBB(ray, obb);
        if (hit) {
          out[0] = hit->x;
          out[1] = hit->y;
          out[2] = hit->z;
        } else {
          constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
          out[0] = kNaN;
          out[1] = kNaN;
          out[2] = kNaN;
        }
      });
}

py::array_t<bool> batchCylinderRegionContains(
    const CesiumGeometry::BoundingCylinderRegion& region,
    const py::handle& points) {
  return CesiumPython::batchPredicate3d(
      points,
      3,
      [&region](const double* row) {
        return region.contains(glm::dvec3(row[0], row[1], row[2]));
      });
}

py::array_t<double> batchCylinderRegionDistanceSquared(
    const CesiumGeometry::BoundingCylinderRegion& region,
    const py::handle& points) {
  return CesiumPython::batchScalar3d(points, 3, [&region](const double* row) {
    return region.computeDistanceSquaredToPosition(
        glm::dvec3(row[0], row[1], row[2]));
  });
}

// ---------------------------------------------------------------------------
// Batch helpers for Plane
// ---------------------------------------------------------------------------

py::array_t<double> batchPlanePointDistance(
    const CesiumGeometry::Plane& plane,
    const py::handle& points) {
  return CesiumPython::batchScalar3d(points, 3, [&plane](const double* row) {
    return plane.getPointDistance(glm::dvec3(row[0], row[1], row[2]));
  });
}

py::array_t<double> batchPlaneProjectPoint(
    const CesiumGeometry::Plane& plane,
    const py::handle& points) {
  return CesiumPython::batchTransform(
      points,
      3,
      3,
      [&plane](const double* in, double* out) {
        auto p = plane.projectPointOntoPlane(
            glm::dvec3(in[0], in[1], in[2]));
        out[0] = p.x;
        out[1] = p.y;
        out[2] = p.z;
      });
}

// ---------------------------------------------------------------------------
// Batch helpers for point-in-triangle
// ---------------------------------------------------------------------------

py::array_t<bool> batchPointInTriangle2d(
    const py::handle& points,
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::dvec2& c) {
  return CesiumPython::batchPredicate3d(
      points,
      2,
      [&a, &b, &c](const double* row) {
        return CesiumGeometry::IntersectionTests::pointInTriangle(
            glm::dvec2(row[0], row[1]),
            a,
            b,
            c);
      });
}

py::array_t<bool> batchPointInTriangle3d(
    const py::handle& points,
    const glm::dvec3& a,
    const glm::dvec3& b,
    const glm::dvec3& c) {
  return CesiumPython::batchPredicate3d(
      points,
      3,
      [&a, &b, &c](const double* row) {
        return CesiumGeometry::IntersectionTests::pointInTriangle(
            glm::dvec3(row[0], row[1], row[2]),
            a,
            b,
            c);
      });
}

// ---------------------------------------------------------------------------
// Batch helper for QuadtreeRectangleAvailability
// ---------------------------------------------------------------------------

py::array_t<int32_t> batchQuadtreeMaxLevelAtPosition(
    const CesiumGeometry::QuadtreeRectangleAvailability& avail,
    const py::handle& points) {
  auto arr = requirePointsArray(points, 2);
  auto in = arr.unchecked<2>();
  const py::ssize_t n = in.shape(0);
  py::array_t<int32_t> result(n);
  int32_t* out = static_cast<int32_t*>(result.mutable_data());
  for (py::ssize_t i = 0; i < n; ++i) {
    out[i] = static_cast<int32_t>(
        avail.computeMaximumLevelAtPosition(glm::dvec2(in(i, 0), in(i, 1))));
  }
  return result;
}

py::list
subtreeBuffersToPython(const CesiumGeometry::AvailabilitySubtree& subtree) {
  py::list out;
  for (const auto& buffer : subtree.buffers) {
    out.append(
        CesiumPython::spanToBytes(
            std::span<const std::byte>(buffer.data(), buffer.size())));
  }
  return out;
}

void setSubtreeBuffersFromPython(
    CesiumGeometry::AvailabilitySubtree& subtree,
    const py::iterable& buffers) {
  subtree.buffers.clear();
  for (py::handle item : buffers) {
    subtree.buffers.emplace_back(
        CesiumPython::bytesToVector(py::cast<py::bytes>(item)));
  }
}

uint32_t countOnesInBufferBytes(const py::bytes& bytes) {
  std::vector<std::byte> buffer = CesiumPython::bytesToVector(bytes);
  return CesiumGeometry::AvailabilityUtilities::countOnesInBuffer(
      std::span<const std::byte>(buffer.data(), buffer.size()));
}

std::vector<CesiumGeometry::TriangleClipVertex>
clipTriangleAtAxisAlignedThreshold(
    double threshold,
    bool keepAbove,
    int i0,
    int i1,
    int i2,
    double u0,
    double u1,
    double u2) {
  std::vector<CesiumGeometry::TriangleClipVertex> result;
  CesiumGeometry::clipTriangleAtAxisAlignedThreshold(
      threshold,
      keepAbove,
      i0,
      i1,
      i2,
      u0,
      u1,
      u2,
      result);
  return result;
}

} // namespace

void initGeometryBindings(py::module& m) {
  py::class_<CesiumGeometry::ConstantAvailability>(m, "ConstantAvailability")
      .def(py::init<>())
      .def(py::init<bool>(), py::arg("constant"))
      .def_readwrite(
          "constant",
          &CesiumGeometry::ConstantAvailability::constant);

  py::class_<CesiumGeometry::SubtreeBufferView>(m, "SubtreeBufferView")
      .def(py::init<>())
      .def(
          py::init<uint32_t, uint32_t, uint8_t>(),
          py::arg("byte_offset"),
          py::arg("byte_length"),
          py::arg("buffer"))
      .def_readwrite(
          "byte_offset",
          &CesiumGeometry::SubtreeBufferView::byteOffset)
      .def_readwrite(
          "byte_length",
          &CesiumGeometry::SubtreeBufferView::byteLength)
      .def_readwrite("buffer", &CesiumGeometry::SubtreeBufferView::buffer);

  py::class_<CesiumGeometry::AvailabilitySubtree>(m, "AvailabilitySubtree")
      .def(py::init<>())
      .def_readwrite(
          "tile_availability",
          &CesiumGeometry::AvailabilitySubtree::tileAvailability)
      .def_readwrite(
          "content_availability",
          &CesiumGeometry::AvailabilitySubtree::contentAvailability)
      .def_readwrite(
          "subtree_availability",
          &CesiumGeometry::AvailabilitySubtree::subtreeAvailability)
      .def("get_buffers", &subtreeBuffersToPython)
      .def(
          "set_buffers",
          [](CesiumGeometry::AvailabilitySubtree& self,
             const py::iterable& buffers) {
            setSubtreeBuffersFromPython(self, buffers);
          },
          py::arg("buffers"));

  py::class_<CesiumGeometry::AvailabilityNode>(m, "AvailabilityNode");

  py::class_<CesiumGeometry::AvailabilityTree>(m, "AvailabilityTree");

  py::class_<CesiumGeometry::AvailabilityAccessor>(m, "AvailabilityAccessor")
      .def(
          py::init<
              const CesiumGeometry::AvailabilityView&,
              const CesiumGeometry::AvailabilitySubtree&>(),
          py::arg("view"),
          py::arg("subtree"))
      .def(
          "is_buffer_view",
          &CesiumGeometry::AvailabilityAccessor::isBufferView)
      .def("is_constant", &CesiumGeometry::AvailabilityAccessor::isConstant)
      .def_property_readonly(
          "constant",
          &CesiumGeometry::AvailabilityAccessor::getConstant)
      .def("size", &CesiumGeometry::AvailabilityAccessor::size)
      .def(
          "get_buffer_bytes",
          [](const CesiumGeometry::AvailabilityAccessor& self) {
            return CesiumPython::spanToBytes(self.getBufferAccessor());
          });

  m.def(
      "count_ones_in_byte",
      &CesiumGeometry::AvailabilityUtilities::countOnesInByte,
      py::arg("value"));

  m.def("count_ones_in_buffer", &countOnesInBufferBytes, py::arg("buffer"));

  py::class_<CesiumGeometry::InterpolatedVertex>(m, "InterpolatedVertex")
      .def(py::init<>())
      .def(
          py::init<int, int, double>(),
          py::arg("first"),
          py::arg("second"),
          py::arg("t"))
      .def_readwrite("first", &CesiumGeometry::InterpolatedVertex::first)
      .def_readwrite("second", &CesiumGeometry::InterpolatedVertex::second)
      .def_readwrite("t", &CesiumGeometry::InterpolatedVertex::t)
      .def(
          "__eq__",
          &CesiumGeometry::InterpolatedVertex::operator==,
          py::is_operator())
      .def(
          "__ne__",
          &CesiumGeometry::InterpolatedVertex::operator!=,
          py::is_operator());

  m.def(
      "clipTriangleAtAxisAlignedThreshold",
      &clipTriangleAtAxisAlignedThreshold,
      py::arg("threshold"),
      py::arg("keep_above"),
      py::arg("i0"),
      py::arg("i1"),
      py::arg("i2"),
      py::arg("u0"),
      py::arg("u1"),
      py::arg("u2"));

  py::enum_<CesiumGeometry::Axis>(m, "Axis")
      .value("X", CesiumGeometry::Axis::X)
      .value("Y", CesiumGeometry::Axis::Y)
      .value("Z", CesiumGeometry::Axis::Z)
      .export_values();

  py::enum_<CesiumGeometry::CullingResult>(m, "CullingResult")
      .value("Outside", CesiumGeometry::CullingResult::Outside)
      .value("Intersecting", CesiumGeometry::CullingResult::Intersecting)
      .value("Inside", CesiumGeometry::CullingResult::Inside)
      .export_values();

  py::enum_<CesiumGeometry::TileAvailabilityFlags>(m, "TileAvailabilityFlags")
      .value(
          "TILE_AVAILABLE",
          CesiumGeometry::TileAvailabilityFlags::TILE_AVAILABLE)
      .value(
          "CONTENT_AVAILABLE",
          CesiumGeometry::TileAvailabilityFlags::CONTENT_AVAILABLE)
      .value(
          "SUBTREE_AVAILABLE",
          CesiumGeometry::TileAvailabilityFlags::SUBTREE_AVAILABLE)
      .value(
          "SUBTREE_LOADED",
          CesiumGeometry::TileAvailabilityFlags::SUBTREE_LOADED)
      .value("REACHABLE", CesiumGeometry::TileAvailabilityFlags::REACHABLE)
      .export_values();

  py::class_<CesiumGeometry::Rectangle>(m, "Rectangle")
      .def(py::init<>())
      .def(
          py::init<double, double, double, double>(),
          py::arg("minimum_x"),
          py::arg("minimum_y"),
          py::arg("maximum_x"),
          py::arg("maximum_y"))
      .def_readwrite("minimum_x", &CesiumGeometry::Rectangle::minimumX)
      .def_readwrite("minimum_y", &CesiumGeometry::Rectangle::minimumY)
      .def_readwrite("maximum_x", &CesiumGeometry::Rectangle::maximumX)
      .def_readwrite("maximum_y", &CesiumGeometry::Rectangle::maximumY)
      .def(
          "contains",
          [](const CesiumGeometry::Rectangle& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 2)) {
              return batchRectangleContains(self, position);
            }
            return py::cast(self.contains(toDvec<2>(position)));
          },
          py::arg("position"))
      .def("overlaps", &CesiumGeometry::Rectangle::overlaps, py::arg("other"))
      .def(
          "fully_contains",
          &CesiumGeometry::Rectangle::fullyContains,
          py::arg("other"))
      .def(
          "compute_signed_distance",
          [](const CesiumGeometry::Rectangle& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 2)) {
              return batchRectangleSignedDistance(self, position);
            }
            return py::cast(self.computeSignedDistance(toDvec<2>(position)));
          },
          py::arg("position"))
      .def_property_readonly(
          "lower_left",
          [](const CesiumGeometry::Rectangle& self) {
            return toNumpy(self.getLowerLeft());
          })
      .def_property_readonly(
          "lower_right",
          [](const CesiumGeometry::Rectangle& self) {
            return toNumpy(self.getLowerRight());
          })
      .def_property_readonly(
          "upper_left",
          [](const CesiumGeometry::Rectangle& self) {
            return toNumpy(self.getUpperLeft());
          })
      .def_property_readonly(
          "upper_right",
          [](const CesiumGeometry::Rectangle& self) {
            return toNumpy(self.getUpperRight());
          })
      .def_property_readonly(
          "center",
          [](const CesiumGeometry::Rectangle& self) {
            return toNumpy(self.getCenter());
          })
      .def("compute_width", &CesiumGeometry::Rectangle::computeWidth)
      .def("compute_height", &CesiumGeometry::Rectangle::computeHeight)
      .def(
          "compute_intersection",
          &CesiumGeometry::Rectangle::computeIntersection,
          py::arg("other"))
      .def(
          "compute_union",
          &CesiumGeometry::Rectangle::computeUnion,
          py::arg("other"))
      .def(
          "__contains__",
          [](const CesiumGeometry::Rectangle& self,
             const py::object& position) {
            return self.contains(toDvec<2>(position));
          })
      .def("__repr__", [](const CesiumGeometry::Rectangle& self) {
        return "Rectangle(x=[" + std::to_string(self.minimumX) + ", " +
               std::to_string(self.maximumX) + "], y=[" +
               std::to_string(self.minimumY) + ", " +
               std::to_string(self.maximumY) + "])";
      });

  py::class_<CesiumGeometry::AxisAlignedBox>(m, "AxisAlignedBox")
      .def(py::init<>())
      .def(
          py::init<double, double, double, double, double, double>(),
          py::arg("minimum_x"),
          py::arg("minimum_y"),
          py::arg("minimum_z"),
          py::arg("maximum_x"),
          py::arg("maximum_y"),
          py::arg("maximum_z"))
      .def_readwrite("minimum_x", &CesiumGeometry::AxisAlignedBox::minimumX)
      .def_readwrite("minimum_y", &CesiumGeometry::AxisAlignedBox::minimumY)
      .def_readwrite("minimum_z", &CesiumGeometry::AxisAlignedBox::minimumZ)
      .def_readwrite("maximum_x", &CesiumGeometry::AxisAlignedBox::maximumX)
      .def_readwrite("maximum_y", &CesiumGeometry::AxisAlignedBox::maximumY)
      .def_readwrite("maximum_z", &CesiumGeometry::AxisAlignedBox::maximumZ)
      .def_readwrite("length_x", &CesiumGeometry::AxisAlignedBox::lengthX)
      .def_readwrite("length_y", &CesiumGeometry::AxisAlignedBox::lengthY)
      .def_readwrite("length_z", &CesiumGeometry::AxisAlignedBox::lengthZ)
      .def_property(
          "center",
          [](const CesiumGeometry::AxisAlignedBox& self) {
            return toNumpy(self.center);
          },
          [](CesiumGeometry::AxisAlignedBox& self, const py::object& center) {
            self.center = toDvec<3>(center);
          })
      .def(
          "contains",
          [](const CesiumGeometry::AxisAlignedBox& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchBoxContains(self, position);
            }
            return py::cast(self.contains(toDvec<3>(position)));
          },
          py::arg("position"))
      .def_static(
          "from_positions",
          [](const py::object& positions) {
            if (py::isinstance<py::array>(positions)) {
              auto arr = requirePointsArray(positions, 3);
              auto points = arr.unchecked<2>();
              std::vector<glm::dvec3> converted;
              converted.reserve(static_cast<size_t>(points.shape(0)));
              for (py::ssize_t i = 0; i < points.shape(0); ++i) {
                converted.emplace_back(
                    points(i, 0),
                    points(i, 1),
                    points(i, 2));
              }
              return CesiumGeometry::AxisAlignedBox::fromPositions(converted);
            }

            return CesiumGeometry::AxisAlignedBox::fromPositions(
                CesiumPython::iterableToVec<3>(
                    py::reinterpret_borrow<py::iterable>(positions)));
          },
          py::arg("positions"))
      .def(
          "__contains__",
          [](const CesiumGeometry::AxisAlignedBox& self,
             const py::object& position) {
            return self.contains(toDvec<3>(position));
          })
      .def("__repr__", [](const CesiumGeometry::AxisAlignedBox& self) {
        return "AxisAlignedBox(min=[" + std::to_string(self.minimumX) + ", " +
               std::to_string(self.minimumY) + ", " +
               std::to_string(self.minimumZ) + "], max=[" +
               std::to_string(self.maximumX) + ", " +
               std::to_string(self.maximumY) + ", " +
               std::to_string(self.maximumZ) + "])";
      });

  py::class_<CesiumGeometry::Plane>(m, "Plane")
      .def(py::init<>())
      .def(
          py::init([](const py::object& normal, double distance) {
            return CesiumGeometry::Plane(toDvec<3>(normal), distance);
          }),
          py::arg("normal"),
          py::arg("distance"))
      .def(
          py::init([](const py::object& point, const py::object& normal) {
            return CesiumGeometry::Plane(toDvec<3>(point), toDvec<3>(normal));
          }),
          py::arg("point"),
          py::arg("normal"))
      .def_property_readonly_static(
          "ORIGIN_XY_PLANE",
          [](py::object) { return CesiumGeometry::Plane::ORIGIN_XY_PLANE; })
      .def_property_readonly_static(
          "ORIGIN_YZ_PLANE",
          [](py::object) { return CesiumGeometry::Plane::ORIGIN_YZ_PLANE; })
      .def_property_readonly_static(
          "ORIGIN_ZX_PLANE",
          [](py::object) { return CesiumGeometry::Plane::ORIGIN_ZX_PLANE; })
      .def_property_readonly(
          "normal",
          [](const CesiumGeometry::Plane& self) {
            return toNumpy(self.getNormal());
          })
      .def_property_readonly("distance", &CesiumGeometry::Plane::getDistance)
      .def(
          "get_point_distance",
          [](const CesiumGeometry::Plane& self,
             const py::object& point) -> py::object {
            if (isNumpyPointsArray(point, 3)) {
              return batchPlanePointDistance(self, point);
            }
            return py::cast(self.getPointDistance(toDvec<3>(point)));
          },
          py::arg("point"))
      .def(
          "project_point_onto_plane",
          [](const CesiumGeometry::Plane& self,
             const py::object& point) -> py::object {
            if (isNumpyPointsArray(point, 3)) {
              return batchPlaneProjectPoint(self, point);
            }
            return toNumpy(self.projectPointOntoPlane(toDvec<3>(point)));
          },
          py::arg("point"))
      .def("__repr__", [](const CesiumGeometry::Plane& self) {
        const auto& n = self.getNormal();
        return "Plane(normal=[" + std::to_string(n.x) + ", " +
               std::to_string(n.y) + ", " + std::to_string(n.z) +
               "], distance=" + std::to_string(self.getDistance()) + ")";
      });

  py::class_<CesiumGeometry::Ray>(m, "Ray")
      .def(
          py::init([](const py::object& origin, const py::object& direction) {
            return CesiumGeometry::Ray(toDvec<3>(origin), toDvec<3>(direction));
          }),
          py::arg("origin"),
          py::arg("direction"))
      .def_property_readonly(
          "origin",
          [](const CesiumGeometry::Ray& self) {
            return toNumpy(self.getOrigin());
          })
      .def_property_readonly(
          "direction",
          [](const CesiumGeometry::Ray& self) {
            return toNumpy(self.getDirection());
          })
      .def(
          "point_from_distance",
          [](const CesiumGeometry::Ray& self, double distance) {
            return toNumpy(self.pointFromDistance(distance));
          },
          py::arg("distance"))
      .def("__repr__", [](const CesiumGeometry::Ray& self) {
        const auto& o = self.getOrigin();
        const auto& d = self.getDirection();
        return "Ray(origin=[" + std::to_string(o.x) + ", " +
               std::to_string(o.y) + ", " + std::to_string(o.z) +
               "], direction=[" + std::to_string(d.x) + ", " +
               std::to_string(d.y) + ", " + std::to_string(d.z) + "])";
      })
      .def(
          "transform",
          [](const CesiumGeometry::Ray& self, const py::object& mat) {
            return self.transform(toDmat<4>(mat));
          },
          py::arg("transformation"))
      .def("__neg__", [](const CesiumGeometry::Ray& self) { return -self; });

  py::class_<CesiumGeometry::BoundingSphere>(m, "BoundingSphere")
      .def(
          py::init([](const py::object& center, double radius) {
            return CesiumGeometry::BoundingSphere(toDvec<3>(center), radius);
          }),
          py::arg("center"),
          py::arg("radius"))
      .def_property_readonly(
          "center",
          [](const CesiumGeometry::BoundingSphere& self) {
            return toNumpy(self.getCenter());
          })
      .def_property_readonly(
          "radius",
          &CesiumGeometry::BoundingSphere::getRadius)
      .def(
          "intersect_plane",
          &CesiumGeometry::BoundingSphere::intersectPlane,
          py::arg("plane"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeometry::BoundingSphere& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchSphereDistanceSquared(self, position);
            }
            return py::cast(
                self.computeDistanceSquaredToPosition(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "contains",
          [](const CesiumGeometry::BoundingSphere& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchSphereContains(self, position);
            }
            return py::cast(self.contains(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "__contains__",
          [](const CesiumGeometry::BoundingSphere& self,
             const py::object& position) {
            return self.contains(toDvec<3>(position));
          })
      .def("__repr__", [](const CesiumGeometry::BoundingSphere& self) {
        const auto& c = self.getCenter();
        return "BoundingSphere(center=[" + std::to_string(c.x) + ", " +
               std::to_string(c.y) + ", " + std::to_string(c.z) +
               "], radius=" + std::to_string(self.getRadius()) + ")";
      })
      .def(
          "transform",
          [](const CesiumGeometry::BoundingSphere& self,
             const py::object& mat) {
            return self.transform(toDmat<4>(mat));
          },
          py::arg("transformation"));

  py::class_<CesiumGeometry::OrientedBoundingBox>(m, "OrientedBoundingBox")
      .def(
          py::init([](const py::object& center, const py::object& half_axes) {
            return CesiumGeometry::OrientedBoundingBox(
                toDvec<3>(center),
                toDmat<3>(half_axes));
          }),
          py::arg("center"),
          py::arg("half_axes"))
      .def_property_readonly(
          "center",
          [](const CesiumGeometry::OrientedBoundingBox& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getCenter(), base);
          })
      .def_property_readonly(
          "half_axes",
          [](const CesiumGeometry::OrientedBoundingBox& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getHalfAxes(), base);
          })
      .def_property_readonly(
          "inverse_half_axes",
          [](const CesiumGeometry::OrientedBoundingBox& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getInverseHalfAxes(), base);
          })
      .def_property_readonly(
          "lengths",
          [](const CesiumGeometry::OrientedBoundingBox& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.getLengths(), base);
          })
      .def(
          "intersect_plane",
          &CesiumGeometry::OrientedBoundingBox::intersectPlane,
          py::arg("plane"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeometry::OrientedBoundingBox& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchOrientedBoxDistanceSquared(self, position);
            }
            return py::cast(
                self.computeDistanceSquaredToPosition(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "contains",
          [](const CesiumGeometry::OrientedBoundingBox& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchOrientedBoxContains(self, position);
            }
            return py::cast(self.contains(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "transform",
          [](const CesiumGeometry::OrientedBoundingBox& self,
             const py::object& transformation) {
            return self.transform(toDmat<4>(transformation));
          },
          py::arg("transformation"))
      .def(
          "to_axis_aligned",
          &CesiumGeometry::OrientedBoundingBox::toAxisAligned)
      .def("to_sphere", &CesiumGeometry::OrientedBoundingBox::toSphere)
      .def_static(
          "from_axis_aligned",
          &CesiumGeometry::OrientedBoundingBox::fromAxisAligned,
          py::arg("axis_aligned"))
      .def_static(
          "from_sphere",
          &CesiumGeometry::OrientedBoundingBox::fromSphere,
          py::arg("sphere"))
      .def_property_readonly(
          "corners",
          [](const CesiumGeometry::OrientedBoundingBox& self)
              -> py::array_t<double> {
            const glm::dvec3& c = self.getCenter();
            const glm::dmat3& ha = self.getHalfAxes();
            py::array_t<double> result({py::ssize_t{8}, py::ssize_t{3}});
            auto out = result.mutable_unchecked<2>();
            int idx = 0;
            for (int sx : {-1, 1})
              for (int sy : {-1, 1})
                for (int sz : {-1, 1}) {
                  glm::dvec3 v = c + double(sx) * ha[0] + double(sy) * ha[1] +
                                 double(sz) * ha[2];
                  out(idx, 0) = v.x;
                  out(idx, 1) = v.y;
                  out(idx, 2) = v.z;
                  ++idx;
                }
            return result;
          },
          "The 8 corners of the box as an (8, 3) float64 array in ECEF "
          "coordinates.")
      .def(
          "__contains__",
          [](const CesiumGeometry::OrientedBoundingBox& self,
             const py::object& position) {
            return self.contains(toDvec<3>(position));
          })
      .def("__repr__", [](const CesiumGeometry::OrientedBoundingBox& self) {
        const auto& c = self.getCenter();
        const auto& l = self.getLengths();
        return "OrientedBoundingBox(center=[" + std::to_string(c.x) + ", " +
               std::to_string(c.y) + ", " + std::to_string(c.z) +
               "], lengths=[" + std::to_string(l.x) + ", " +
               std::to_string(l.y) + ", " + std::to_string(l.z) + "])";
      });

  py::class_<CesiumGeometry::BoundingCylinderRegion>(
      m,
      "BoundingCylinderRegion")
      .def(
          py::init([](const py::object& translation,
                      const py::object& rotation,
                      double height,
                      const py::object& radial_bounds) {
            return CesiumGeometry::BoundingCylinderRegion(
                toDvec<3>(translation),
                toDquat(rotation),
                height,
                toDvec<2>(radial_bounds));
          }),
          py::arg("translation"),
          py::arg("rotation"),
          py::arg("height"),
          py::arg("radial_bounds"))
      .def(
          py::init([](const py::object& translation,
                      const py::object& rotation,
                      double height,
                      const py::object& radial_bounds,
                      const py::object& angular_bounds) {
            return CesiumGeometry::BoundingCylinderRegion(
                toDvec<3>(translation),
                toDquat(rotation),
                height,
                toDvec<2>(radial_bounds),
                toDvec<2>(angular_bounds));
          }),
          py::arg("translation"),
          py::arg("rotation"),
          py::arg("height"),
          py::arg("radial_bounds"),
          py::arg("angular_bounds"))
      .def_property_readonly(
          "center",
          [](const CesiumGeometry::BoundingCylinderRegion& self) {
            return toNumpy(self.getCenter());
          })
      .def(
          "translation",
          [](const CesiumGeometry::BoundingCylinderRegion& self) {
            return toNumpy(self.getTranslation());
          })
      .def_property_readonly(
          "rotation",
          [](const CesiumGeometry::BoundingCylinderRegion& self) {
            return toNumpy(self.getRotation());
          })
      .def_property_readonly(
          "height",
          &CesiumGeometry::BoundingCylinderRegion::getHeight)
      .def(
          "radial_bounds",
          [](const CesiumGeometry::BoundingCylinderRegion& self) {
            return toNumpy(self.getRadialBounds());
          })
      .def(
          "angular_bounds",
          [](const CesiumGeometry::BoundingCylinderRegion& self) {
            return toNumpy(self.getAngularBounds());
          })
      .def(
          "intersect_plane",
          &CesiumGeometry::BoundingCylinderRegion::intersectPlane,
          py::arg("plane"))
      .def(
          "compute_distance_squared_to_position",
          [](const CesiumGeometry::BoundingCylinderRegion& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchCylinderRegionDistanceSquared(self, position);
            }
            return py::cast(
                self.computeDistanceSquaredToPosition(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "contains",
          [](const CesiumGeometry::BoundingCylinderRegion& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchCylinderRegionContains(self, position);
            }
            return py::cast(self.contains(toDvec<3>(position)));
          },
          py::arg("position"))
      .def(
          "transform",
          [](const CesiumGeometry::BoundingCylinderRegion& self,
             const py::object& transformation) {
            return self.transform(toDmat<4>(transformation));
          },
          py::arg("transformation"))
      .def(
          "to_oriented_bounding_box",
          &CesiumGeometry::BoundingCylinderRegion::toOrientedBoundingBox)
      .def(
          "__contains__",
          [](const CesiumGeometry::BoundingCylinderRegion& self,
             const py::object& position) {
            return self.contains(toDvec<3>(position));
          })
      .def("__repr__", [](const CesiumGeometry::BoundingCylinderRegion& self) {
        return "BoundingCylinderRegion(height=" +
               std::to_string(self.getHeight()) + ")";
      });

  py::class_<CesiumGeometry::CullingVolume>(m, "CullingVolume")
      .def(py::init<>())
      .def_readwrite("left_plane", &CesiumGeometry::CullingVolume::leftPlane)
      .def_readwrite("right_plane", &CesiumGeometry::CullingVolume::rightPlane)
      .def_readwrite("top_plane", &CesiumGeometry::CullingVolume::topPlane)
      .def_readwrite(
          "bottom_plane",
          &CesiumGeometry::CullingVolume::bottomPlane);

  m.def(
      "create_culling_volume",
      [](const py::object& position,
         const py::object& direction,
         const py::object& up,
         double fovx,
         double fovy) {
        return CesiumGeometry::createCullingVolume(
            toDvec<3>(position),
            toDvec<3>(direction),
            toDvec<3>(up),
            fovx,
            fovy);
      },
      py::arg("position"),
      py::arg("direction"),
      py::arg("up"),
      py::arg("fovx"),
      py::arg("fovy"));

  m.def(
      "create_culling_volume_from_matrix",
      [](const py::object& clip_matrix) {
        return CesiumGeometry::createCullingVolume(toDmat<4>(clip_matrix));
      },
      py::arg("clip_matrix"));

  m.def(
      "create_culling_volume_from_extents",
      [](const py::object& position,
         const py::object& direction,
         const py::object& up,
         double l,
         double r,
         double b,
         double t,
         double n) {
        return CesiumGeometry::createCullingVolume(
            toDvec<3>(position),
            toDvec<3>(direction),
            toDvec<3>(up),
            l,
            r,
            b,
            t,
            n);
      },
      py::arg("position"),
      py::arg("direction"),
      py::arg("up"),
      py::arg("left"),
      py::arg("right"),
      py::arg("bottom"),
      py::arg("top"),
      py::arg("near"));

  m.def(
      "create_orthographic_culling_volume",
      [](const py::object& position,
         const py::object& direction,
         const py::object& up,
         double l,
         double r,
         double b,
         double t,
         double n) {
        return CesiumGeometry::createOrthographicCullingVolume(
            toDvec<3>(position),
            toDvec<3>(direction),
            toDvec<3>(up),
            l,
            r,
            b,
            t,
            n);
      },
      py::arg("position"),
      py::arg("direction"),
      py::arg("up"),
      py::arg("left"),
      py::arg("right"),
      py::arg("bottom"),
      py::arg("top"),
      py::arg("near"));

  py::class_<CesiumGeometry::Transforms>(m, "Transforms")
      .def_property_readonly_static(
          "Y_UP_TO_Z_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::Y_UP_TO_Z_UP);
          })
      .def_property_readonly_static(
          "Z_UP_TO_Y_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::Z_UP_TO_Y_UP);
          })
      .def_property_readonly_static(
          "X_UP_TO_Z_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::X_UP_TO_Z_UP);
          })
      .def_property_readonly_static(
          "Z_UP_TO_X_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::Z_UP_TO_X_UP);
          })
      .def_property_readonly_static(
          "X_UP_TO_Y_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::X_UP_TO_Y_UP);
          })
      .def_property_readonly_static(
          "Y_UP_TO_X_UP",
          [](py::object) {
            return toNumpy(CesiumGeometry::Transforms::Y_UP_TO_X_UP);
          })
      .def_static(
          "create_translation_rotation_scale_matrix",
          [](const py::object& translation,
             const py::object& rotation,
             const py::object& scale) {
            return toNumpy(
                CesiumGeometry::Transforms::
                    createTranslationRotationScaleMatrix(
                        toDvec<3>(translation),
                        toDquat(rotation),
                        toDvec<3>(scale)));
          },
          py::arg("translation"),
          py::arg("rotation"),
          py::arg("scale"))
      .def_static(
          "compute_translation_rotation_scale_from_matrix",
          [](const py::object& matrix) {
            glm::dvec3 translation;
            glm::dquat rotation;
            glm::dvec3 scale;
            CesiumGeometry::Transforms::
                computeTranslationRotationScaleFromMatrix(
                    toDmat<4>(matrix),
                    &translation,
                    &rotation,
                    &scale);
            return py::make_tuple(
                toNumpy(translation),
                toNumpy(rotation),
                toNumpy(scale));
          },
          py::arg("matrix"))
      .def_static(
          "get_up_axis_transform",
          [](CesiumGeometry::Axis from, CesiumGeometry::Axis to) {
            return toNumpy(
                CesiumGeometry::Transforms::getUpAxisTransform(from, to));
          },
          py::arg("from_axis"),
          py::arg("to_axis"))
      .def_static(
          "create_view_matrix",
          [](const py::object& position,
             const py::object& direction,
             const py::object& up) {
            return toNumpy(
                CesiumGeometry::Transforms::createViewMatrix(
                    toDvec<3>(position),
                    toDvec<3>(direction),
                    toDvec<3>(up)));
          },
          py::arg("position"),
          py::arg("direction"),
          py::arg("up"))
      .def_static(
          "create_perspective_matrix",
          [](double fovx, double fovy, double zNear, double zFar) {
            return toNumpy(
                CesiumGeometry::Transforms::createPerspectiveMatrix(
                    fovx,
                    fovy,
                    zNear,
                    zFar));
          },
          py::arg("fovx"),
          py::arg("fovy"),
          py::arg("near"),
          py::arg("far"))
      .def_static(
          "create_perspective_matrix_from_extents",
          [](double left,
             double right,
             double bottom,
             double top,
             double zNear,
             double zFar) {
            return toNumpy(
                CesiumGeometry::Transforms::createPerspectiveMatrix(
                    left,
                    right,
                    bottom,
                    top,
                    zNear,
                    zFar));
          },
          py::arg("left"),
          py::arg("right"),
          py::arg("bottom"),
          py::arg("top"),
          py::arg("near"),
          py::arg("far"))
      .def_static(
          "create_orthographic_matrix",
          [](double left,
             double right,
             double bottom,
             double top,
             double zNear,
             double zFar) {
            return toNumpy(
                CesiumGeometry::Transforms::createOrthographicMatrix(
                    left,
                    right,
                    bottom,
                    top,
                    zNear,
                    zFar));
          },
          py::arg("left"),
          py::arg("right"),
          py::arg("bottom"),
          py::arg("top"),
          py::arg("near"),
          py::arg("far"));

  py::class_<CesiumGeometry::IntersectionTests>(m, "IntersectionTests")
      .def_static(
          "ray_plane",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::Plane& plane) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::rayPlane(ray, plane));
          },
          py::arg("ray"),
          py::arg("plane"))
      .def_static(
          "ray_ellipsoid",
          [](const CesiumGeometry::Ray& ray, const py::object& radii) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::rayEllipsoid(
                    ray,
                    toDvec<3>(radii)));
          },
          py::arg("ray"),
          py::arg("radii"))
      .def_static(
          "point_in_triangle_2d",
          [](const py::object& point,
             const py::object& a,
             const py::object& b,
             const py::object& c) -> py::object {
            auto av = toDvec<2>(a), bv = toDvec<2>(b), cv = toDvec<2>(c);
            if (isNumpyPointsArray(point, 2)) {
              return batchPointInTriangle2d(point, av, bv, cv);
            }
            return py::cast(
                CesiumGeometry::IntersectionTests::pointInTriangle(
                    toDvec<2>(point),
                    av,
                    bv,
                    cv));
          },
          py::arg("point"),
          py::arg("triangle_vert_a"),
          py::arg("triangle_vert_b"),
          py::arg("triangle_vert_c"))
      .def_static(
          "point_in_triangle_3d",
          [](const py::object& point,
             const py::object& a,
             const py::object& b,
             const py::object& c) -> py::object {
            auto av = toDvec<3>(a), bv = toDvec<3>(b), cv = toDvec<3>(c);
            if (isNumpyPointsArray(point, 3)) {
              return batchPointInTriangle3d(point, av, bv, cv);
            }
            return py::cast(
                CesiumGeometry::IntersectionTests::pointInTriangle(
                    toDvec<3>(point),
                    av,
                    bv,
                    cv));
          },
          py::arg("point"),
          py::arg("triangle_vert_a"),
          py::arg("triangle_vert_b"),
          py::arg("triangle_vert_c"))
      .def_static(
          "point_in_triangle_3d_with_barycentric",
          [](const py::object& point,
             const py::object& a,
             const py::object& b,
             const py::object& c) {
            glm::dvec3 barycentricCoordinates;
            bool inside = CesiumGeometry::IntersectionTests::pointInTriangle(
                toDvec<3>(point),
                toDvec<3>(a),
                toDvec<3>(b),
                toDvec<3>(c),
                barycentricCoordinates);
            return py::make_tuple(inside, toNumpy(barycentricCoordinates));
          },
          py::arg("point"),
          py::arg("triangle_vert_a"),
          py::arg("triangle_vert_b"),
          py::arg("triangle_vert_c"))
      .def_static(
          "ray_triangle",
          [](const CesiumGeometry::Ray& ray,
             const py::object& p0,
             const py::object& p1,
             const py::object& p2,
             bool cullBackFaces) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::rayTriangle(
                    ray,
                    toDvec<3>(p0),
                    toDvec<3>(p1),
                    toDvec<3>(p2),
                    cullBackFaces));
          },
          py::arg("ray"),
          py::arg("p0"),
          py::arg("p1"),
          py::arg("p2"),
          py::arg("cull_back_faces") = false)
      .def_static(
          "ray_triangle_parametric",
          [](const CesiumGeometry::Ray& ray,
             const py::object& p0,
             const py::object& p1,
             const py::object& p2,
             bool cullBackFaces) {
            return CesiumPython::optionalToPy(
                CesiumGeometry::IntersectionTests::rayTriangleParametric(
                    ray,
                    toDvec<3>(p0),
                    toDvec<3>(p1),
                    toDvec<3>(p2),
                    cullBackFaces));
          },
          py::arg("ray"),
          py::arg("p0"),
          py::arg("p1"),
          py::arg("p2"),
          py::arg("cull_back_faces") = false)
      .def_static(
          "ray_aabb",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::AxisAlignedBox& aabb) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::rayAABB(ray, aabb));
          },
          py::arg("ray"),
          py::arg("aabb"))
      .def_static(
          "ray_aabb_parametric",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::AxisAlignedBox& aabb) {
            return CesiumPython::optionalToPy(
                CesiumGeometry::IntersectionTests::rayAABBParametric(
                    ray,
                    aabb));
          },
          py::arg("ray"),
          py::arg("aabb"))
      .def_static(
          "ray_obb",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::OrientedBoundingBox& obb) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::rayOBB(ray, obb));
          },
          py::arg("ray"),
          py::arg("obb"))
      .def_static(
          "ray_obb",
          [](const py::handle& origins,
             const py::handle& directions,
             const CesiumGeometry::OrientedBoundingBox& obb) {
            return batchRayObb(origins, directions, obb);
          },
          py::arg("origins"),
          py::arg("directions"),
          py::arg("obb"),
          "Vectorized: (N,3) origins, (N,3) directions, OBB -> (N,3) hit "
          "points (NaN=miss).")
      .def_static(
          "ray_obb_parametric",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::OrientedBoundingBox& obb) {
            return CesiumPython::optionalToPy(
                CesiumGeometry::IntersectionTests::rayOBBParametric(ray, obb));
          },
          py::arg("ray"),
          py::arg("obb"))
      .def_static(
          "ray_obb_parametric",
          [](const py::handle& origins,
             const py::handle& directions,
             const CesiumGeometry::OrientedBoundingBox& obb) {
            return batchRayObbParametric(origins, directions, obb);
          },
          py::arg("origins"),
          py::arg("directions"),
          py::arg("obb"),
          "Vectorized: (N,3) origins, (N,3) directions, OBB -> (N,) float64 t "
          "(NaN=miss).")
      .def_static(
          "ray_sphere",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::BoundingSphere& sphere) {
            return CesiumPython::optionalToNumpy(
                CesiumGeometry::IntersectionTests::raySphere(ray, sphere));
          },
          py::arg("ray"),
          py::arg("sphere"))
      .def_static(
          "ray_sphere_parametric",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGeometry::BoundingSphere& sphere) {
            return CesiumPython::optionalToPy(
                CesiumGeometry::IntersectionTests::raySphereParametric(
                    ray,
                    sphere));
          },
          py::arg("ray"),
          py::arg("sphere"))
      .def_static(
          "ray_triangle_parametric",
          [](py::array_t<double, py::array::c_style> origins,
             py::array_t<double, py::array::c_style> directions,
             const py::object& p0,
             const py::object& p1,
             const py::object& p2,
             bool cullBackFaces) {
            auto origBuf = origins.unchecked<2>();
            auto dirBuf = directions.unchecked<2>();
            if (origBuf.shape(1) != 3 || dirBuf.shape(1) != 3) {
              throw py::value_error(
                  "Expected (N,3) arrays for origins and directions.");
            }
            if (origBuf.shape(0) != dirBuf.shape(0)) {
              throw py::value_error(
                  "origins and directions must have the same length.");
            }
            const py::ssize_t n = origBuf.shape(0);
            const auto v0 = toDvec<3>(p0);
            const auto v1 = toDvec<3>(p1);
            const auto v2 = toDvec<3>(p2);
            py::array_t<double> result(n);
            double* out = static_cast<double*>(result.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                CesiumGeometry::Ray ray(
                    glm::dvec3(origBuf(i, 0), origBuf(i, 1), origBuf(i, 2)),
                    glm::dvec3(dirBuf(i, 0), dirBuf(i, 1), dirBuf(i, 2)));
                auto t = CesiumGeometry::IntersectionTests::
                    rayTriangleParametric(ray, v0, v1, v2, cullBackFaces);
                out[i] = t.has_value() ? *t
                                       : std::numeric_limits<double>::quiet_NaN();
              }
            }
            return result;
          },
          py::arg("origins"),
          py::arg("directions"),
          py::arg("p0"),
          py::arg("p1"),
          py::arg("p2"),
          py::arg("cull_back_faces") = false);

  py::class_<CesiumGeometry::QuadtreeTileID>(m, "QuadtreeTileID")
      .def(
          py::init<uint32_t, uint32_t, uint32_t>(),
          py::arg("level"),
          py::arg("x"),
          py::arg("y"))
      .def_readwrite("level", &CesiumGeometry::QuadtreeTileID::level)
      .def_readwrite("x", &CesiumGeometry::QuadtreeTileID::x)
      .def_readwrite("y", &CesiumGeometry::QuadtreeTileID::y)
      .def_property_readonly(
          "parent",
          &CesiumGeometry::QuadtreeTileID::getParent)
      .def(
          "__eq__",
          &CesiumGeometry::QuadtreeTileID::operator==,
          py::is_operator())
      .def(
          "__ne__",
          &CesiumGeometry::QuadtreeTileID::operator!=,
          py::is_operator())
      .def(
          "__hash__",
          [](const CesiumGeometry::QuadtreeTileID& self) {
            std::size_t h = std::hash<uint32_t>{}(self.level);
            h ^= std::hash<uint32_t>{}(self.x) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            h ^= std::hash<uint32_t>{}(self.y) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            return h;
          })
      .def("__repr__", [](const CesiumGeometry::QuadtreeTileID& self) {
        return "QuadtreeTileID(level=" + std::to_string(self.level) +
               ", x=" + std::to_string(self.x) +
               ", y=" + std::to_string(self.y) + ")";
      });

  py::class_<CesiumGeometry::UpsampledQuadtreeNode>(m, "UpsampledQuadtreeNode")
      .def_readwrite("tile_id", &CesiumGeometry::UpsampledQuadtreeNode::tileID);

  py::class_<CesiumGeometry::OctreeTileID>(m, "OctreeTileID")
      .def(py::init<>())
      .def(
          py::init<uint32_t, uint32_t, uint32_t, uint32_t>(),
          py::arg("level"),
          py::arg("x"),
          py::arg("y"),
          py::arg("z"))
      .def_readwrite("level", &CesiumGeometry::OctreeTileID::level)
      .def_readwrite("x", &CesiumGeometry::OctreeTileID::x)
      .def_readwrite("y", &CesiumGeometry::OctreeTileID::y)
      .def_readwrite("z", &CesiumGeometry::OctreeTileID::z)
      .def(
          "__eq__",
          &CesiumGeometry::OctreeTileID::operator==,
          py::is_operator())
      .def(
          "__ne__",
          &CesiumGeometry::OctreeTileID::operator!=,
          py::is_operator())
      .def(
          "__hash__",
          [](const CesiumGeometry::OctreeTileID& self) {
            std::size_t h = std::hash<uint32_t>{}(self.level);
            h ^= std::hash<uint32_t>{}(self.x) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            h ^= std::hash<uint32_t>{}(self.y) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            h ^= std::hash<uint32_t>{}(self.z) + 0x9e3779b9 + (h << 6) +
                 (h >> 2);
            return h;
          })
      .def("__repr__", [](const CesiumGeometry::OctreeTileID& self) {
        return "OctreeTileID(level=" + std::to_string(self.level) +
               ", x=" + std::to_string(self.x) +
               ", y=" + std::to_string(self.y) +
               ", z=" + std::to_string(self.z) + ")";
      });

  py::class_<CesiumGeometry::QuadtreeTileRectangularRange>(
      m,
      "QuadtreeTileRectangularRange")
      .def(py::init<>())
      .def_readwrite(
          "level",
          &CesiumGeometry::QuadtreeTileRectangularRange::level)
      .def_readwrite(
          "minimum_x",
          &CesiumGeometry::QuadtreeTileRectangularRange::minimumX)
      .def_readwrite(
          "minimum_y",
          &CesiumGeometry::QuadtreeTileRectangularRange::minimumY)
      .def_readwrite(
          "maximum_x",
          &CesiumGeometry::QuadtreeTileRectangularRange::maximumX)
      .def_readwrite(
          "maximum_y",
          &CesiumGeometry::QuadtreeTileRectangularRange::maximumY);

  py::class_<CesiumGeometry::QuadtreeTilingScheme>(m, "QuadtreeTilingScheme")
      .def(
          py::init<const CesiumGeometry::Rectangle&, uint32_t, uint32_t>(),
          py::arg("rectangle"),
          py::arg("root_tiles_x"),
          py::arg("root_tiles_y"))
      .def_property_readonly(
          "rectangle",
          &CesiumGeometry::QuadtreeTilingScheme::getRectangle,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "root_tiles_x",
          &CesiumGeometry::QuadtreeTilingScheme::getRootTilesX)
      .def_property_readonly(
          "root_tiles_y",
          &CesiumGeometry::QuadtreeTilingScheme::getRootTilesY)
      .def(
          "get_number_of_x_tiles_at_level",
          &CesiumGeometry::QuadtreeTilingScheme::getNumberOfXTilesAtLevel,
          py::arg("level"))
      .def(
          "get_number_of_y_tiles_at_level",
          &CesiumGeometry::QuadtreeTilingScheme::getNumberOfYTilesAtLevel,
          py::arg("level"))
      .def(
          "position_to_tile",
          [](const CesiumGeometry::QuadtreeTilingScheme& self,
             const py::object& position,
             uint32_t level) -> py::object {
            if (isNumpyPointsArray(position, 2)) {
              return batchQuadtreePositionToTile(self, position, level);
            }
            return py::cast(self.positionToTile(toDvec<2>(position), level));
          },
          py::arg("position"),
          py::arg("level"))
      .def(
          "tile_to_rectangle",
          &CesiumGeometry::QuadtreeTilingScheme::tileToRectangle,
          py::arg("tile_id"));

  py::class_<CesiumGeometry::OctreeTilingScheme>(m, "OctreeTilingScheme")
      .def(
          py::init<
              const CesiumGeometry::AxisAlignedBox&,
              uint32_t,
              uint32_t,
              uint32_t>(),
          py::arg("box"),
          py::arg("root_tiles_x"),
          py::arg("root_tiles_y"),
          py::arg("root_tiles_z"))
      .def_property_readonly(
          "box",
          &CesiumGeometry::OctreeTilingScheme::getBox,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "root_tiles_x",
          &CesiumGeometry::OctreeTilingScheme::getRootTilesX)
      .def_property_readonly(
          "root_tiles_y",
          &CesiumGeometry::OctreeTilingScheme::getRootTilesY)
      .def_property_readonly(
          "root_tiles_z",
          &CesiumGeometry::OctreeTilingScheme::getRootTilesZ)
      .def(
          "get_number_of_x_tiles_at_level",
          &CesiumGeometry::OctreeTilingScheme::getNumberOfXTilesAtLevel,
          py::arg("level"))
      .def(
          "get_number_of_y_tiles_at_level",
          &CesiumGeometry::OctreeTilingScheme::getNumberOfYTilesAtLevel,
          py::arg("level"))
      .def(
          "get_number_of_z_tiles_at_level",
          &CesiumGeometry::OctreeTilingScheme::getNumberOfZTilesAtLevel,
          py::arg("level"))
      .def(
          "position_to_tile",
          [](const CesiumGeometry::OctreeTilingScheme& self,
             const py::object& position,
             uint32_t level) -> py::object {
            if (isNumpyPointsArray(position, 3)) {
              return batchOctreePositionToTile(self, position, level);
            }
            return py::cast(self.positionToTile(toDvec<3>(position), level));
          },
          py::arg("position"),
          py::arg("level"))
      .def(
          "tile_to_box",
          &CesiumGeometry::OctreeTilingScheme::tileToBox,
          py::arg("tile_id"));

  py::class_<CesiumGeometry::OctreeAvailability>(m, "OctreeAvailability")
      .def(
          py::init<uint32_t, uint32_t>(),
          py::arg("subtree_levels"),
          py::arg("maximum_level"))
      .def(
          "compute_availability",
          py::overload_cast<const CesiumGeometry::OctreeTileID&>(
              &CesiumGeometry::OctreeAvailability::computeAvailability,
              py::const_))
      .def_property_readonly(
          "subtree_levels",
          &CesiumGeometry::OctreeAvailability::getSubtreeLevels)
      .def_property_readonly(
          "maximum_level",
          &CesiumGeometry::OctreeAvailability::getMaximumLevel);

  py::class_<CesiumGeometry::QuadtreeAvailability>(m, "QuadtreeAvailability")
      .def(
          py::init<uint32_t, uint32_t>(),
          py::arg("subtree_levels"),
          py::arg("maximum_level"))
      .def(
          "compute_availability",
          py::overload_cast<const CesiumGeometry::QuadtreeTileID&>(
              &CesiumGeometry::QuadtreeAvailability::computeAvailability,
              py::const_))
      .def_property_readonly(
          "subtree_levels",
          &CesiumGeometry::QuadtreeAvailability::getSubtreeLevels)
      .def_property_readonly(
          "maximum_level",
          &CesiumGeometry::QuadtreeAvailability::getMaximumLevel);

  py::class_<CesiumGeometry::QuadtreeRectangleAvailability>(
      m,
      "QuadtreeRectangleAvailability")
      .def(
          py::init<const CesiumGeometry::QuadtreeTilingScheme&, uint32_t>(),
          py::arg("tiling_scheme"),
          py::arg("maximum_level"))
      .def(
          "add_available_tile_range",
          &CesiumGeometry::QuadtreeRectangleAvailability::addAvailableTileRange,
          py::arg("range"))
      .def(
          "compute_maximum_level_at_position",
          [](const CesiumGeometry::QuadtreeRectangleAvailability& self,
             const py::object& position) -> py::object {
            if (isNumpyPointsArray(position, 2)) {
              return batchQuadtreeMaxLevelAtPosition(self, position);
            }
            return py::cast(
                self.computeMaximumLevelAtPosition(toDvec<2>(position)));
          },
          py::arg("position"))
      .def(
          "is_tile_available",
          &CesiumGeometry::QuadtreeRectangleAvailability::isTileAvailable,
          py::arg("id"));

  // ── Module-level batch helpers ──────────────────────────────────────────
  m.def(
      "transform_points",
      [](const py::handle& points, const py::handle& matrix) {
        return CesiumPython::transformPointsByMatrix(points, toDmat<4>(matrix));
      },
      py::arg("points"),
      py::arg("matrix"),
      "Transform (N, 3) points by a 4x4 matrix (GIL-free). Returns (N, 3).");
}
