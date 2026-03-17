#include "HttpHeaderConversions.h"
#include "NumpyConversions.h"

#include <Cesium3DTiles/BoundingVolume.h>
#include <Cesium3DTiles/Tile.h>
#include <Cesium3DTilesContent/B3dmToGltfConverter.h>
#include <Cesium3DTilesContent/BinaryToGltfConverter.h>
#include <Cesium3DTilesContent/CmptToGltfConverter.h>
#include <Cesium3DTilesContent/GltfConverterResult.h>
#include <Cesium3DTilesContent/GltfConverterUtility.h>
#include <Cesium3DTilesContent/GltfConverters.h>
#include <Cesium3DTilesContent/I3dmToGltfConverter.h>
#include <Cesium3DTilesContent/ImplicitTilingUtilities.h>
#include <Cesium3DTilesContent/PntsToGltfConverter.h>
#include <Cesium3DTilesContent/SubtreeAvailability.h>
#include <Cesium3DTilesContent/TileBoundingVolumes.h>
#include <Cesium3DTilesContent/TileTransform.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <rapidjson/document.h>

#include <array>
#include <cctype>
#include <cstring>
#include <vector>

namespace py = pybind11;

namespace {

using CesiumPython::toDmat;
using CesiumPython::toDvec;
using CesiumPython::toNumpy;
using CesiumPython::toNumpyView;

using FutureGltfConverterResult =
    CesiumAsync::Future<Cesium3DTilesContent::GltfConverterResult>;
using SharedFutureGltfConverterResult =
    CesiumAsync::SharedFuture<Cesium3DTilesContent::GltfConverterResult>;
using FutureAssetFetcherResult =
    CesiumAsync::Future<Cesium3DTilesContent::AssetFetcherResult>;
using SharedFutureAssetFetcherResult =
    CesiumAsync::SharedFuture<Cesium3DTilesContent::AssetFetcherResult>;

Cesium3DTilesContent::GltfConverters::ConverterFunction
converter_function_from_name(const std::string& converterName) {
  std::string key = converterName;
  for (char& ch : key) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (key == "binary" || key == "binarytogltfconverter" || key == "glb") {
    return &Cesium3DTilesContent::BinaryToGltfConverter::convert;
  }
  if (key == "b3dm" || key == "b3dmtogltfconverter") {
    return &Cesium3DTilesContent::B3dmToGltfConverter::convert;
  }
  if (key == "cmpt" || key == "cmpttogltfconverter") {
    return &Cesium3DTilesContent::CmptToGltfConverter::convert;
  }
  if (key == "i3dm" || key == "i3dmtogltfconverter") {
    return &Cesium3DTilesContent::I3dmToGltfConverter::convert;
  }
  if (key == "pnts" || key == "pntstogltfconverter") {
    return &Cesium3DTilesContent::PntsToGltfConverter::convert;
  }

  throw py::value_error(
      "Unknown converter_name. Expected one of: binary, b3dm, cmpt, i3dm, "
      "pnts.");
}

py::object converterNameFromFunction(
    Cesium3DTilesContent::GltfConverters::ConverterFunction converter) {
  if (!converter) {
    return py::none();
  }
  if (converter == &Cesium3DTilesContent::BinaryToGltfConverter::convert) {
    return py::str("binary");
  }
  if (converter == &Cesium3DTilesContent::B3dmToGltfConverter::convert) {
    return py::str("b3dm");
  }
  if (converter == &Cesium3DTilesContent::CmptToGltfConverter::convert) {
    return py::str("cmpt");
  }
  if (converter == &Cesium3DTilesContent::I3dmToGltfConverter::convert) {
    return py::str("i3dm");
  }
  if (converter == &Cesium3DTilesContent::PntsToGltfConverter::convert) {
    return py::str("pnts");
  }
  return py::str("custom");
}

std::vector<CesiumGeometry::QuadtreeTileID> quadtree_children_to_vector(
    const Cesium3DTilesContent::QuadtreeChildren& children) {
  std::vector<CesiumGeometry::QuadtreeTileID> out;
  out.reserve(static_cast<size_t>(children.size()));
  for (const auto& child : children) {
    out.push_back(child);
  }
  return out;
}

std::vector<CesiumGeometry::OctreeTileID> octree_children_to_vector(
    const Cesium3DTilesContent::OctreeChildren& children) {
  std::vector<CesiumGeometry::OctreeTileID> out;
  out.reserve(static_cast<size_t>(children.size()));
  for (const auto& child : children) {
    out.push_back(child);
  }
  return out;
}

} // namespace

void initTiles3dContentBindings(py::module& m) {
  py::class_<FutureGltfConverterResult>(m, "FutureGltfConverterResult")
      .def(
          "wait",
          &FutureGltfConverterResult::wait,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "wait_in_main_thread",
          &FutureGltfConverterResult::waitInMainThread,
          py::call_guard<py::gil_scoped_release>())
      .def("is_ready", &FutureGltfConverterResult::isReady)
      .def("share", [](FutureGltfConverterResult& self) {
        return std::move(self).share();
      });

  py::class_<SharedFutureGltfConverterResult>(
      m,
      "SharedFutureGltfConverterResult")
      .def(
          "wait",
          [](const SharedFutureGltfConverterResult& self) {
            return self.wait();
          },
          py::call_guard<py::gil_scoped_release>())
      .def(
          "wait_in_main_thread",
          &SharedFutureGltfConverterResult::waitInMainThread,
          py::call_guard<py::gil_scoped_release>())
      .def("is_ready", &SharedFutureGltfConverterResult::isReady);

  py::class_<FutureAssetFetcherResult>(m, "FutureAssetFetcherResult")
      .def(
          "wait",
          &FutureAssetFetcherResult::wait,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "wait_in_main_thread",
          &FutureAssetFetcherResult::waitInMainThread,
          py::call_guard<py::gil_scoped_release>())
      .def("is_ready", &FutureAssetFetcherResult::isReady)
      .def("share", [](FutureAssetFetcherResult& self) {
        return std::move(self).share();
      });

  py::class_<SharedFutureAssetFetcherResult>(
      m,
      "SharedFutureAssetFetcherResult")
      .def(
          "wait",
          [](const SharedFutureAssetFetcherResult& self) {
            return self.wait();
          },
          py::call_guard<py::gil_scoped_release>())
      .def(
          "wait_in_main_thread",
          &SharedFutureAssetFetcherResult::waitInMainThread,
          py::call_guard<py::gil_scoped_release>())
      .def("is_ready", &SharedFutureAssetFetcherResult::isReady);

  py::class_<Cesium3DTilesContent::AssetFetcher>(m, "AssetFetcher")
      .def(
          py::init<
              const CesiumAsync::AsyncSystem&,
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              const std::string&,
              const glm::dmat4,
              const std::vector<CesiumAsync::IAssetAccessor::THeader>&,
              CesiumGeometry::Axis>(),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("base_url"),
          py::arg("tile_transform"),
          py::arg("request_headers"),
          py::arg("up_axis"))
      .def(
          py::init([](const CesiumAsync::AsyncSystem& asyncSystem,
                      const std::shared_ptr<CesiumAsync::IAssetAccessor>&
                          pAssetAccessor,
                      const std::string& baseUrl,
                      const py::object& tileTransform,
                      const py::dict& requestHeaders,
                      CesiumGeometry::Axis upAxis) {
            return Cesium3DTilesContent::AssetFetcher(
                asyncSystem,
                pAssetAccessor,
                baseUrl,
                toDmat<4>(tileTransform),
                CesiumPython::dictToHeaderPairs(requestHeaders),
                upAxis);
          }),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("base_url"),
          py::arg("tile_transform"),
          py::arg("request_headers") = py::dict(),
          py::arg("up_axis") = CesiumGeometry::Axis::Y)
      .def(
          "get",
          &Cesium3DTilesContent::AssetFetcher::get,
          py::arg("relative_url"))
      .def_readwrite(
          "async_system",
          &Cesium3DTilesContent::AssetFetcher::asyncSystem)
      .def_readwrite(
          "asset_accessor",
          &Cesium3DTilesContent::AssetFetcher::pAssetAccessor)
      .def_readwrite("base_url", &Cesium3DTilesContent::AssetFetcher::baseUrl)
      .def_property(
          "tile_transform",
          [](const Cesium3DTilesContent::AssetFetcher& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return toNumpyView(self.tileTransform, base);
          },
          [](Cesium3DTilesContent::AssetFetcher& self,
             const py::object& values) {
            self.tileTransform = toDmat<4>(values);
          })
      .def_property(
          "request_headers",
          [](const Cesium3DTilesContent::AssetFetcher& self) {
            return CesiumPython::headerPairsToPyDict(self.requestHeaders);
          },
          [](Cesium3DTilesContent::AssetFetcher& self,
             const py::dict& headers) {
            self.requestHeaders = CesiumPython::dictToHeaderPairs(headers);
          })
      .def_readwrite("up_axis", &Cesium3DTilesContent::AssetFetcher::upAxis);

  py::class_<Cesium3DTilesContent::AssetFetcherResult>(m, "AssetFetcherResult")
      .def(py::init<>())
      .def_property(
          "bytes",
          [](const Cesium3DTilesContent::AssetFetcherResult& self) {
            py::handle base =
                py::cast(&self, py::return_value_policy::reference);
            return CesiumPython::bytesToNumpyView(
                reinterpret_cast<const std::byte*>(self.bytes.data()),
                self.bytes.size(),
                base);
          },
          [](Cesium3DTilesContent::AssetFetcherResult& self,
             const py::bytes& bytes) {
            self.bytes = CesiumPython::bytesToVector(bytes);
          })
      .def_readwrite(
          "error_list",
          &Cesium3DTilesContent::AssetFetcherResult::errorList);

  py::class_<Cesium3DTilesContent::GltfConverterResult>(
      m,
      "GltfConverterResult")
      .def(py::init<>())
      .def_readwrite("model", &Cesium3DTilesContent::GltfConverterResult::model)
      .def_readwrite(
          "errors",
          &Cesium3DTilesContent::GltfConverterResult::errors);

  py::class_<Cesium3DTilesContent::BinaryToGltfConverter>(
      m,
      "BinaryToGltfConverter")
      .def_static(
          "convert",
          [](const py::bytes& gltfBinary,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> content =
                CesiumPython::bytesToVector(gltfBinary);
            return Cesium3DTilesContent::BinaryToGltfConverter::convert(
                std::span<const std::byte>(content.data(), content.size()),
                options,
                assetFetcher);
          },
          py::arg("gltf_binary"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  py::class_<Cesium3DTilesContent::B3dmToGltfConverter>(
      m,
      "B3dmToGltfConverter")
      .def_static(
          "convert",
          [](const py::bytes& b3dmBinary,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> content =
                CesiumPython::bytesToVector(b3dmBinary);
            return Cesium3DTilesContent::B3dmToGltfConverter::convert(
                std::span<const std::byte>(content.data(), content.size()),
                options,
                assetFetcher);
          },
          py::arg("b3dm_binary"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  py::class_<Cesium3DTilesContent::CmptToGltfConverter>(
      m,
      "CmptToGltfConverter")
      .def_static(
          "convert",
          [](const py::bytes& cmptBinary,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> content =
                CesiumPython::bytesToVector(cmptBinary);
            return Cesium3DTilesContent::CmptToGltfConverter::convert(
                std::span<const std::byte>(content.data(), content.size()),
                options,
                assetFetcher);
          },
          py::arg("cmpt_binary"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  py::class_<Cesium3DTilesContent::I3dmToGltfConverter>(
      m,
      "I3dmToGltfConverter")
      .def_static(
          "convert",
          [](const py::bytes& instancesBinary,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> content =
                CesiumPython::bytesToVector(instancesBinary);
            return Cesium3DTilesContent::I3dmToGltfConverter::convert(
                std::span<const std::byte>(content.data(), content.size()),
                options,
                assetFetcher);
          },
          py::arg("instances_binary"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  py::class_<Cesium3DTilesContent::PntsToGltfConverter>(
      m,
      "PntsToGltfConverter")
      .def_static(
          "convert",
          [](const py::bytes& pntsBinary,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> content =
                CesiumPython::bytesToVector(pntsBinary);
            return Cesium3DTilesContent::PntsToGltfConverter::convert(
                std::span<const std::byte>(content.data(), content.size()),
                options,
                assetFetcher);
          },
          py::arg("pnts_binary"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  py::class_<Cesium3DTilesContent::GltfConverters>(m, "GltfConverters")
      .def_static(
          "register_magic",
          [](const std::string& magic, const std::string& converterName) {
            Cesium3DTilesContent::GltfConverters::registerMagic(
                magic,
                converter_function_from_name(converterName));
          },
          py::arg("magic"),
          py::arg("converter_name"))
      .def_static(
          "register_file_extension",
          [](const std::string& fileExtension,
             const std::string& converterName) {
            Cesium3DTilesContent::GltfConverters::registerFileExtension(
                fileExtension,
                converter_function_from_name(converterName));
          },
          py::arg("file_extension"),
          py::arg("converter_name"))
      .def_static(
          "converter_by_file_extension",
          [](const std::string& filePath) {
            return converterNameFromFunction(
                Cesium3DTilesContent::GltfConverters::
                    getConverterByFileExtension(filePath));
          },
          py::arg("file_path"))
      .def_static(
          "converter_by_magic",
          [](const py::bytes& content) {
            std::vector<std::byte> bytes = CesiumPython::bytesToVector(content);
            return converterNameFromFunction(
                Cesium3DTilesContent::GltfConverters::getConverterByMagic(
                    std::span<const std::byte>(bytes.data(), bytes.size())));
          },
          py::arg("content"))
      .def_static(
          "convert",
          [](const std::string& filePath,
             const py::bytes& content,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> bytes = CesiumPython::bytesToVector(content);
            return Cesium3DTilesContent::GltfConverters::convert(
                filePath,
                std::span<const std::byte>(bytes.data(), bytes.size()),
                options,
                assetFetcher);
          },
          py::arg("file_path"),
          py::arg("content"),
          py::arg("options"),
          py::arg("asset_fetcher"))
      .def_static(
          "convert",
          [](const py::bytes& content,
             const CesiumGltfReader::GltfReaderOptions& options,
             const Cesium3DTilesContent::AssetFetcher& assetFetcher) {
            std::vector<std::byte> bytes = CesiumPython::bytesToVector(content);
            return Cesium3DTilesContent::GltfConverters::convert(
                std::span<const std::byte>(bytes.data(), bytes.size()),
                options,
                assetFetcher);
          },
          py::arg("content"),
          py::arg("options"),
          py::arg("asset_fetcher"));

  m.def(
      "create_buffer_in_gltf",
      [](CesiumGltf::Model& gltf, const py::bytes& buffer) {
        return Cesium3DTilesContent::GltfConverterUtility::createBufferInGltf(
            gltf,
            CesiumPython::bytesToVector(buffer));
      },
      py::arg("gltf"),
      py::arg("buffer") = py::bytes());

  m.def(
      "create_buffer_view_in_gltf",
      &Cesium3DTilesContent::GltfConverterUtility::createBufferViewInGltf,
      py::arg("gltf"),
      py::arg("buffer_id"),
      py::arg("byte_length"),
      py::arg("byte_stride"));

  m.def(
      "create_accessor_in_gltf",
      &Cesium3DTilesContent::GltfConverterUtility::createAccessorInGltf,
      py::arg("gltf"),
      py::arg("buffer_view_id"),
      py::arg("component_type"),
      py::arg("count"),
      py::arg("type"));

  m.def(
      "apply_rtc_to_nodes",
      [](CesiumGltf::Model& gltf, const py::object& rtc) {
        Cesium3DTilesContent::GltfConverterUtility::applyRtcToNodes(
            gltf,
            toDvec<3>(rtc));
      },
      py::arg("gltf"),
      py::arg("rtc"));

  m.def(
      "parse_offset_for_semantic",
      [](const std::string& json, const std::string& semantic) {
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError()) {
          throw py::value_error("Invalid JSON input.");
        }

        CesiumUtility::ErrorList errorList;
        std::optional<uint32_t> offset =
            Cesium3DTilesContent::GltfConverterUtility::parseOffsetForSemantic(
                document,
                semantic.c_str(),
                errorList);

        py::dict out;
        out["offset"] = offset ? py::cast(*offset) : py::none();
        out["errors"] = py::cast(errorList);
        return out;
      },
      py::arg("json"),
      py::arg("semantic"));

  m.def(
      "parse_array_value_dvec3",
      [](const std::string& json, const std::string& name) -> py::object {
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError()) {
          throw py::value_error("Invalid JSON input.");
        }

        std::optional<glm::dvec3> value =
            Cesium3DTilesContent::GltfConverterUtility::parseArrayValueDVec3(
                document,
                name.c_str());
        if (!value) {
          return py::none();
        }
        return toNumpy(*value);
      },
      py::arg("json"),
      py::arg("name"));

  py::enum_<Cesium3DTilesContent::ImplicitTileSubdivisionScheme>(
      m,
      "ImplicitTileSubdivisionScheme")
      .value(
          "Quadtree",
          Cesium3DTilesContent::ImplicitTileSubdivisionScheme::Quadtree)
      .value(
          "Octree",
          Cesium3DTilesContent::ImplicitTileSubdivisionScheme::Octree)
      .export_values();

  py::class_<
      Cesium3DTilesContent::SubtreeAvailability::SubtreeConstantAvailability>(
      m,
      "SubtreeConstantAvailability")
      .def(py::init<>())
      .def_readwrite(
          "constant",
          &Cesium3DTilesContent::SubtreeAvailability::
              SubtreeConstantAvailability::constant);

  py::class_<Cesium3DTilesContent::SubtreeAvailability>(
      m,
      "SubtreeAvailability")
      .def_static(
          "from_subtree",
          [](Cesium3DTilesContent::ImplicitTileSubdivisionScheme
                 subdivisionScheme,
             uint32_t levelsInSubtree,
             const Cesium3DTiles::Subtree& subtree)
              -> std::optional<Cesium3DTilesContent::SubtreeAvailability> {
            return Cesium3DTilesContent::SubtreeAvailability::fromSubtree(
                subdivisionScheme,
                levelsInSubtree,
                Cesium3DTiles::Subtree(subtree));
          },
          py::arg("subdivision_scheme"),
          py::arg("levels_in_subtree"),
          py::arg("subtree"))
      .def_static(
          "create_empty",
          &Cesium3DTilesContent::SubtreeAvailability::createEmpty,
          py::arg("subdivision_scheme"),
          py::arg("levels_in_subtree"),
          py::arg("set_tiles_available"))
      .def(
          "is_tile_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::SubtreeAvailability::isTileAvailable,
              py::const_),
          py::arg("subtree_id"),
          py::arg("tile_id"))
      .def(
          "is_tile_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::SubtreeAvailability::isTileAvailable,
              py::const_),
          py::arg("subtree_id"),
          py::arg("tile_id"))
      .def(
          "is_tile_available",
          py::overload_cast<uint32_t, uint64_t>(
              &Cesium3DTilesContent::SubtreeAvailability::isTileAvailable,
              py::const_),
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_id"))
      .def(
          "set_tile_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setTileAvailable),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("is_available"))
      .def(
          "set_tile_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setTileAvailable),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("is_available"))
      .def(
          "set_tile_available",
          py::overload_cast<uint32_t, uint64_t, bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setTileAvailable),
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_id"),
          py::arg("is_available"))
      .def(
          "is_content_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&,
              size_t>(
              &Cesium3DTilesContent::SubtreeAvailability::isContentAvailable,
              py::const_),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("content_id"))
      .def(
          "is_content_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&,
              size_t>(
              &Cesium3DTilesContent::SubtreeAvailability::isContentAvailable,
              py::const_),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("content_id"))
      .def(
          "is_content_available",
          py::overload_cast<uint32_t, uint64_t, size_t>(
              &Cesium3DTilesContent::SubtreeAvailability::isContentAvailable,
              py::const_),
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_id"),
          py::arg("content_id"))
      .def(
          "set_content_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&,
              size_t,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setContentAvailable),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("content_id"),
          py::arg("is_available"))
      .def(
          "set_content_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&,
              size_t,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setContentAvailable),
          py::arg("subtree_id"),
          py::arg("tile_id"),
          py::arg("content_id"),
          py::arg("is_available"))
      .def(
          "set_content_available",
          py::overload_cast<uint32_t, uint64_t, size_t, bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setContentAvailable),
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_id"),
          py::arg("content_id"),
          py::arg("is_available"))
      .def(
          "is_subtree_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::SubtreeAvailability::isSubtreeAvailable,
              py::const_),
          py::arg("this_subtree_id"),
          py::arg("check_subtree_id"))
      .def(
          "is_subtree_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::SubtreeAvailability::isSubtreeAvailable,
              py::const_),
          py::arg("this_subtree_id"),
          py::arg("check_subtree_id"))
      .def(
          "is_subtree_available",
          py::overload_cast<uint64_t>(
              &Cesium3DTilesContent::SubtreeAvailability::isSubtreeAvailable,
              py::const_),
          py::arg("relative_subtree_morton_id"))
      .def(
          "set_subtree_available",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setSubtreeAvailable),
          py::arg("this_subtree_id"),
          py::arg("set_subtree_id"),
          py::arg("is_available"))
      .def(
          "set_subtree_available",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&,
              bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setSubtreeAvailable),
          py::arg("this_subtree_id"),
          py::arg("set_subtree_id"),
          py::arg("is_available"))
      .def(
          "set_subtree_available",
          py::overload_cast<uint64_t, bool>(
              &Cesium3DTilesContent::SubtreeAvailability::setSubtreeAvailable),
          py::arg("relative_subtree_morton_id"),
          py::arg("is_available"))
      .def(
          "subtree",
          &Cesium3DTilesContent::SubtreeAvailability::getSubtree,
          py::return_value_policy::reference_internal)
      .def(
          "is_tile_available",
          [](const Cesium3DTilesContent::SubtreeAvailability& self,
             uint32_t relativeTileLevel,
             py::array_t<uint64_t, py::array::c_style> mortonIds) {
            auto ids = mortonIds.unchecked<1>();
            const py::ssize_t n = ids.shape(0);
            py::array_t<bool> result(n);
            bool* out = static_cast<bool*>(result.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                out[i] = self.isTileAvailable(relativeTileLevel, ids(i));
              }
            }
            return result;
          },
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_ids"),
          "Batch check tile availability for an array of morton IDs "
          "at the same relative level. Returns np.ndarray[bool].")
      .def(
          "is_content_available",
          [](const Cesium3DTilesContent::SubtreeAvailability& self,
             uint32_t relativeTileLevel,
             py::array_t<uint64_t, py::array::c_style> mortonIds,
             size_t contentId) {
            auto ids = mortonIds.unchecked<1>();
            const py::ssize_t n = ids.shape(0);
            py::array_t<bool> result(n);
            bool* out = static_cast<bool*>(result.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                out[i] = self.isContentAvailable(
                    relativeTileLevel,
                    ids(i),
                    contentId);
              }
            }
            return result;
          },
          py::arg("relative_tile_level"),
          py::arg("relative_tile_morton_ids"),
          py::arg("content_id"),
          "Batch check content availability for an array of morton IDs "
          "at the same relative level. Returns np.ndarray[bool].");

  py::class_<Cesium3DTilesContent::QuadtreeChildren>(m, "QuadtreeChildren")
      .def(
          py::init<const CesiumGeometry::QuadtreeTileID&>(),
          py::arg("tile_id"))
      .def("size", &Cesium3DTilesContent::QuadtreeChildren::size)
      .def("to_list", [](const Cesium3DTilesContent::QuadtreeChildren& self) {
        return quadtree_children_to_vector(self);
      });

  py::class_<Cesium3DTilesContent::OctreeChildren>(m, "OctreeChildren")
      .def(py::init<const CesiumGeometry::OctreeTileID&>(), py::arg("tile_id"))
      .def("size", &Cesium3DTilesContent::OctreeChildren::size)
      .def("to_list", [](const Cesium3DTilesContent::OctreeChildren& self) {
        return octree_children_to_vector(self);
      });

  py::class_<Cesium3DTilesContent::TileTransform>(m, "TileTransform")
      .def_static(
          "transform",
          [](const Cesium3DTiles::Tile& tile) {
            return CesiumPython::optionalToNumpy(
                Cesium3DTilesContent::TileTransform::getTransform(tile));
          })
      .def_static(
          "set_transform",
          [](Cesium3DTiles::Tile& tile, const py::object& values) {
            Cesium3DTilesContent::TileTransform::setTransform(
                tile,
                toDmat<4>(values));
          });

  py::class_<Cesium3DTilesContent::TileBoundingVolumes>(
      m,
      "TileBoundingVolumes")
      .def_static(
          "oriented_bounding_box",
          &Cesium3DTilesContent::TileBoundingVolumes::getOrientedBoundingBox)
      .def_static(
          "set_oriented_bounding_box",
          &Cesium3DTilesContent::TileBoundingVolumes::setOrientedBoundingBox)
      .def_static(
          "bounding_region",
          &Cesium3DTilesContent::TileBoundingVolumes::getBoundingRegion,
          py::arg("bounding_volume"),
          py::arg("ellipsoid"))
      .def_static(
          "set_bounding_region",
          &Cesium3DTilesContent::TileBoundingVolumes::setBoundingRegion)
      .def_static(
          "bounding_sphere",
          &Cesium3DTilesContent::TileBoundingVolumes::getBoundingSphere)
      .def_static(
          "set_bounding_sphere",
          &Cesium3DTilesContent::TileBoundingVolumes::setBoundingSphere)
      .def_static(
          "s2_cell_bounding_volume",
          &Cesium3DTilesContent::TileBoundingVolumes::getS2CellBoundingVolume,
          py::arg("bounding_volume"),
          py::arg("ellipsoid"))
      .def_static(
          "set_s2_cell_bounding_volume",
          &Cesium3DTilesContent::TileBoundingVolumes::setS2CellBoundingVolume)
      .def_static(
          "bounding_cylinder_region",
          &Cesium3DTilesContent::TileBoundingVolumes::getBoundingCylinderRegion)
      .def_static(
          "set_bounding_cylinder_region",
          &Cesium3DTilesContent::TileBoundingVolumes::
              setBoundingCylinderRegion);

  py::class_<Cesium3DTilesContent::ImplicitTilingUtilities>(
      m,
      "ImplicitTilingUtilities")
      .def_static(
          "resolve_url",
          py::overload_cast<
              const std::string&,
              const std::string&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::resolveUrl))
      .def_static(
          "resolve_url",
          py::overload_cast<
              const std::string&,
              const std::string&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::resolveUrl))
      .def_static(
          "compute_level_denominator",
          &Cesium3DTilesContent::ImplicitTilingUtilities::
              computeLevelDenominator)
      .def_static(
          "compute_morton_index",
          py::overload_cast<const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeMortonIndex))
      .def_static(
          "compute_morton_index",
          py::overload_cast<const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeMortonIndex))
      .def_static(
          "compute_relative_morton_index",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeRelativeMortonIndex))
      .def_static(
          "compute_relative_morton_index",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeRelativeMortonIndex))
      .def_static(
          "subtree_root_id",
          py::overload_cast<uint32_t, const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getSubtreeRootID))
      .def_static(
          "subtree_root_id",
          py::overload_cast<uint32_t, const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getSubtreeRootID))
      .def_static(
          "absolute_tile_id_to_relative",
          py::overload_cast<
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  absoluteTileIDToRelative))
      .def_static(
          "absolute_tile_id_to_relative",
          py::overload_cast<
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  absoluteTileIDToRelative))
      .def_static(
          "parent_id",
          py::overload_cast<const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getParentID))
      .def_static(
          "parent_id",
          py::overload_cast<const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getParentID))
      .def_static(
          "children",
          py::overload_cast<const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getChildren))
      .def_static(
          "children",
          py::overload_cast<const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::getChildren))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const Cesium3DTiles::BoundingVolume&,
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const Cesium3DTiles::BoundingVolume&,
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeospatial::BoundingRegion&,
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeospatial::BoundingRegion&,
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeometry::OrientedBoundingBox&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeometry::OrientedBoundingBox&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeospatial::S2CellBoundingVolume&,
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeospatial::S2CellBoundingVolume&,
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeometry::BoundingCylinderRegion&,
              const CesiumGeometry::QuadtreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"))
      .def_static(
          "compute_bounding_volume",
          py::overload_cast<
              const CesiumGeometry::BoundingCylinderRegion&,
              const CesiumGeometry::OctreeTileID&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_volume"),
          py::arg("tile_id"))
      .def_static(
          "compute_bounding_region",
          py::overload_cast<
              const CesiumGeospatial::BoundingRegion&,
              const CesiumGeometry::QuadtreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_region"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_bounding_region",
          py::overload_cast<
              const CesiumGeospatial::BoundingRegion&,
              const CesiumGeometry::OctreeTileID&,
              const CesiumGeospatial::Ellipsoid&>(
              &Cesium3DTilesContent::ImplicitTilingUtilities::
                  computeBoundingVolume),
          py::arg("root_bounding_region"),
          py::arg("tile_id"),
          py::arg("ellipsoid"))
      .def_static(
          "compute_morton_index",
          [](py::array_t<uint32_t, py::array::c_style> tileIds) {
            auto buf = tileIds.unchecked<2>();
            if (buf.shape(1) != 3) {
              throw py::value_error(
                  "Expected (N,3) array of (level, x, y) for quadtree "
                  "tile IDs.");
            }
            const py::ssize_t n = buf.shape(0);
            py::array_t<uint64_t> result(n);
            uint64_t* out = static_cast<uint64_t*>(result.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                CesiumGeometry::QuadtreeTileID tid(
                    buf(i, 0),
                    buf(i, 1),
                    buf(i, 2));
                out[i] = Cesium3DTilesContent::ImplicitTilingUtilities::
                    computeMortonIndex(tid);
              }
            }
            return result;
          },
          py::arg("quadtree_tile_ids"),
          "Batch compute morton indices for an (N,3) array of "
          "(level, x, y) quadtree tile IDs. Returns np.ndarray[uint64].");

  m.def(
      "register_all_tile_content_types",
      &Cesium3DTilesContent::registerAllTileContentTypes);
}
