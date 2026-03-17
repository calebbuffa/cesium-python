#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <CesiumGeometry/Ray.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGltf/Buffer.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltf/Model.h>
#include <CesiumGltf/Node.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfContent/ImageManipulation.h>
#include <CesiumGltfContent/SkirtMeshMetadata.h>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace py = pybind11;

using CesiumPython::extractPoints;
using CesiumPython::toDmat;
using CesiumPython::toNumpy;

/// Batch ray-model intersection with GIL released for the heavy inner loop.
static py::list intersectRaysGltfModel(
    const py::handle& origins_handle,
    const py::handle& directions_handle,
    const CesiumGltf::Model& gltf,
    bool cullBackFaces,
    const glm::dmat4& gltfTransform) {
  auto originsArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      origins_handle);
  auto dirsArr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(
      directions_handle);
  auto origins = extractPoints(originsArr, 3);
  auto dirs = extractPoints(dirsArr, 3);
  if (origins.rows != dirs.rows) {
    throw py::value_error(
        "origins and directions must have the same number of rows.");
  }
  const py::ssize_t n = origins.rows;
  const double* o = origins.data;
  const double* d = dirs.data;

  // Run all intersections with GIL released — pure C++ work.
  std::vector<CesiumGltfContent::GltfUtilities::IntersectResult> results(
      static_cast<size_t>(n));
  {
    py::gil_scoped_release release;
    for (py::ssize_t i = 0; i < n; ++i) {
      CesiumGeometry::Ray ray(
          glm::dvec3(o[i * 3], o[i * 3 + 1], o[i * 3 + 2]),
          glm::normalize(glm::dvec3(d[i * 3], d[i * 3 + 1], d[i * 3 + 2])));
      results[static_cast<size_t>(i)] =
          CesiumGltfContent::GltfUtilities::intersectRayGltfModel(
              ray,
              gltf,
              cullBackFaces,
              gltfTransform);
    }
  }

  // Convert back to Python objects (GIL re-acquired above).
  py::list out(n);
  for (py::ssize_t i = 0; i < n; ++i) {
    out[i] = py::cast(std::move(results[static_cast<size_t>(i)]));
  }
  return out;
}

void initGltfContentBindings(py::module& m) {
  py::class_<CesiumGltfContent::PixelRectangle>(m, "PixelRectangle")
      .def(py::init<>())
      .def_readwrite("x", &CesiumGltfContent::PixelRectangle::x)
      .def_readwrite("y", &CesiumGltfContent::PixelRectangle::y)
      .def_readwrite("width", &CesiumGltfContent::PixelRectangle::width)
      .def_readwrite("height", &CesiumGltfContent::PixelRectangle::height);

  py::class_<CesiumGltfContent::ImageManipulation>(m, "ImageManipulation")
      .def_static(
          "blit_image",
          &CesiumGltfContent::ImageManipulation::blitImage,
          py::arg("target"),
          py::arg("target_pixels"),
          py::arg("source"),
          py::arg("source_pixels"))
      .def_static("save_png", [](const CesiumGltf::ImageAsset& image) {
        return CesiumPython::vectorToBytes(
            CesiumGltfContent::ImageManipulation::savePng(image));
      });

  py::class_<CesiumGltfContent::GltfUtilities>(m, "GltfUtilities")
      .def_static(
          "collapse_to_single_buffer",
          &CesiumGltfContent::GltfUtilities::collapseToSingleBuffer)
      .def_static(
          "move_buffer_content",
          &CesiumGltfContent::GltfUtilities::moveBufferContent)
      .def_static(
          "remove_unused_textures",
          &CesiumGltfContent::GltfUtilities::removeUnusedTextures,
          py::arg("gltf"),
          py::arg("extra_used_texture_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_samplers",
          &CesiumGltfContent::GltfUtilities::removeUnusedSamplers,
          py::arg("gltf"),
          py::arg("extra_used_sampler_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_images",
          &CesiumGltfContent::GltfUtilities::removeUnusedImages,
          py::arg("gltf"),
          py::arg("extra_used_image_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_accessors",
          &CesiumGltfContent::GltfUtilities::removeUnusedAccessors,
          py::arg("gltf"),
          py::arg("extra_used_accessor_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_buffer_views",
          &CesiumGltfContent::GltfUtilities::removeUnusedBufferViews,
          py::arg("gltf"),
          py::arg("extra_used_buffer_view_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_buffers",
          &CesiumGltfContent::GltfUtilities::removeUnusedBuffers,
          py::arg("gltf"),
          py::arg("extra_used_buffer_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_meshes",
          &CesiumGltfContent::GltfUtilities::removeUnusedMeshes,
          py::arg("gltf"),
          py::arg("extra_used_mesh_indices") = std::vector<int32_t>{})
      .def_static(
          "remove_unused_materials",
          &CesiumGltfContent::GltfUtilities::removeUnusedMaterials,
          py::arg("gltf"),
          py::arg("extra_used_material_indices") = std::vector<int32_t>{})
      .def_static(
          "compact_buffers",
          &CesiumGltfContent::GltfUtilities::compactBuffers)
      .def_static(
          "compact_buffer",
          &CesiumGltfContent::GltfUtilities::compactBuffer)
      .def_static(
          "get_node_transform",
          [](const CesiumGltf::Node& node) -> py::object {
            auto result =
                CesiumGltfContent::GltfUtilities::getNodeTransform(node);
            if (!result)
              return py::none();
            return toNumpy(*result);
          },
          py::arg("node"),
          "Get the local transformation matrix for a node, or None if invalid.")
      .def_static(
          "set_node_transform",
          [](CesiumGltf::Node& node, const py::handle& transform) {
            CesiumGltfContent::GltfUtilities::setNodeTransform(
                node,
                toDmat<4>(transform));
          },
          py::arg("node"),
          py::arg("new_transform"),
          "Set the local transformation matrix for a node.")
      .def_static(
          "apply_rtc_center",
          [](const CesiumGltf::Model& gltf, const py::handle& rootTransform) {
            return toNumpy(
                CesiumGltfContent::GltfUtilities::applyRtcCenter(
                    gltf,
                    toDmat<4>(rootTransform)));
          },
          py::arg("gltf"),
          py::arg("root_transform"),
          "Apply the glTF's RTC_CENTER to the given transform.")
      .def_static(
          "apply_gltf_up_axis_transform",
          [](const CesiumGltf::Model& model, const py::handle& rootTransform) {
            return toNumpy(
                CesiumGltfContent::GltfUtilities::applyGltfUpAxisTransform(
                    model,
                    toDmat<4>(rootTransform)));
          },
          py::arg("model"),
          py::arg("root_transform"),
          "Apply the glTF's gltfUpAxis transform.")
      .def_static(
          "compute_bounding_region",
          [](const CesiumGltf::Model& gltf,
             const py::handle& transform,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumGltfContent::GltfUtilities::computeBoundingRegion(
                gltf,
                toDmat<4>(transform),
                ellipsoid);
          },
          py::arg("gltf"),
          py::arg("transform"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84,
          "Compute a bounding region from the vertex positions in a glTF "
          "model.")
      .def_static(
          "parse_gltf_copyright",
          [](const CesiumGltf::Model& gltf) {
            auto views =
                CesiumGltfContent::GltfUtilities::parseGltfCopyright(gltf);
            std::vector<std::string> result;
            result.reserve(views.size());
            for (auto& sv : views)
              result.emplace_back(sv);
            return result;
          },
          py::arg("gltf"),
          "Parse semicolon-separated copyright strings from a glTF model.")
      .def_static(
          "intersect_ray_gltf_model",
          [](const CesiumGeometry::Ray& ray,
             const CesiumGltf::Model& gltf,
             bool cullBackFaces,
             const py::handle& gltfTransform) {
            return CesiumGltfContent::GltfUtilities::intersectRayGltfModel(
                ray,
                gltf,
                cullBackFaces,
                toDmat<4>(gltfTransform));
          },
          py::arg("ray"),
          py::arg("gltf"),
          py::arg("cull_back_faces") = true,
          py::arg("gltf_transform") = toNumpy(glm::dmat4(1.0)),
          "Intersect a ray with a glTF model and return the first "
          "intersection.")
      .def_static(
          "intersect_ray_gltf_model",
          [](const py::handle& origins,
             const py::handle& directions,
             const CesiumGltf::Model& gltf,
             bool cullBackFaces,
             const py::handle& gltfTransform) {
            return intersectRaysGltfModel(
                origins,
                directions,
                gltf,
                cullBackFaces,
                toDmat<4>(gltfTransform));
          },
          py::arg("origins"),
          py::arg("directions"),
          py::arg("gltf"),
          py::arg("cull_back_faces") = true,
          py::arg("gltf_transform") = toNumpy(glm::dmat4(1.0)),
          "Vectorized: (N,3) origins, (N,3) directions -> "
          "list[IntersectResult]. GIL-free.");

  // RayGltfHit
  py::class_<CesiumGltfContent::GltfUtilities::RayGltfHit>(m, "RayGltfHit")
      .def_property_readonly(
          "primitive_point",
          [](const CesiumGltfContent::GltfUtilities::RayGltfHit& self) {
            return toNumpy(self.primitivePoint);
          })
      .def_property_readonly(
          "primitive_to_world",
          [](const CesiumGltfContent::GltfUtilities::RayGltfHit& self) {
            return toNumpy(self.primitiveToWorld);
          })
      .def_property_readonly(
          "world_point",
          [](const CesiumGltfContent::GltfUtilities::RayGltfHit& self) {
            return toNumpy(self.worldPoint);
          })
      .def_readonly(
          "ray_to_world_point_distance_sq",
          &CesiumGltfContent::GltfUtilities::RayGltfHit::
              rayToWorldPointDistanceSq)
      .def_readonly(
          "mesh_id",
          &CesiumGltfContent::GltfUtilities::RayGltfHit::meshId)
      .def_readonly(
          "primitive_id",
          &CesiumGltfContent::GltfUtilities::RayGltfHit::primitiveId);

  // IntersectResult
  py::class_<CesiumGltfContent::GltfUtilities::IntersectResult>(
      m,
      "IntersectResult")
      .def_property_readonly(
          "hit",
          [](const CesiumGltfContent::GltfUtilities::IntersectResult& self)
              -> py::object {
            if (!self.hit)
              return py::none();
            return py::cast(*self.hit);
          })
      .def_readonly(
          "warnings",
          &CesiumGltfContent::GltfUtilities::IntersectResult::warnings);

  // ── SkirtMeshMetadata (G10) ─────────────────────────────────────────────
  py::class_<CesiumGltfContent::SkirtMeshMetadata>(m, "SkirtMeshMetadata")
      .def(py::init<>())
      .def_readwrite(
          "no_skirt_indices_begin",
          &CesiumGltfContent::SkirtMeshMetadata::noSkirtIndicesBegin)
      .def_readwrite(
          "no_skirt_indices_count",
          &CesiumGltfContent::SkirtMeshMetadata::noSkirtIndicesCount)
      .def_readwrite(
          "no_skirt_vertices_begin",
          &CesiumGltfContent::SkirtMeshMetadata::noSkirtVerticesBegin)
      .def_readwrite(
          "no_skirt_vertices_count",
          &CesiumGltfContent::SkirtMeshMetadata::noSkirtVerticesCount)
      .def_property(
          "mesh_center",
          [](const CesiumGltfContent::SkirtMeshMetadata& self) {
            return toNumpy(self.meshCenter);
          },
          [](CesiumGltfContent::SkirtMeshMetadata& self, const py::handle& v) {
            self.meshCenter = CesiumPython::toDvec<3>(v);
          })
      .def_readwrite(
          "skirt_west_height",
          &CesiumGltfContent::SkirtMeshMetadata::skirtWestHeight)
      .def_readwrite(
          "skirt_south_height",
          &CesiumGltfContent::SkirtMeshMetadata::skirtSouthHeight)
      .def_readwrite(
          "skirt_east_height",
          &CesiumGltfContent::SkirtMeshMetadata::skirtEastHeight)
      .def_readwrite(
          "skirt_north_height",
          &CesiumGltfContent::SkirtMeshMetadata::skirtNorthHeight)
      .def_static(
          "parse_from_gltf_extras",
          [](const py::dict& extras) {
            auto obj = CesiumPython::pyToJsonValue(extras);
            if (!obj.isObject())
              return std::optional<CesiumGltfContent::SkirtMeshMetadata>{};
            return CesiumGltfContent::SkirtMeshMetadata::parseFromGltfExtras(
                obj.getObject());
          },
          py::arg("extras"),
          "Parse SkirtMeshMetadata from a glTF mesh extras dict.")
      .def_static(
          "create_gltf_extras",
          [](const CesiumGltfContent::SkirtMeshMetadata& skirt) {
            auto obj =
                CesiumGltfContent::SkirtMeshMetadata::createGltfExtras(skirt);
            return CesiumPython::jsonValueToPy(
                CesiumUtility::JsonValue(std::move(obj)));
          },
          py::arg("skirt"),
          "Create a glTF extras dict from SkirtMeshMetadata.");
}
