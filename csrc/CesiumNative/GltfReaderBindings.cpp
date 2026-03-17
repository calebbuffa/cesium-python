#include "FutureTemplates.h"
#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfReader/GltfSharedAssetSystem.h>
#include <CesiumGltfReader/ImageDecoder.h>
#include <CesiumGltfReader/NetworkImageAssetDescriptor.h>
#include <CesiumGltfReader/NetworkSchemaAssetDescriptor.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace py = pybind11;

void initGltfReaderBindings(py::module& m) {
  py::class_<CesiumGltfReader::GltfReaderResult>(m, "GltfReaderResult")
      .def(py::init<>())
      .def_property_readonly(
          "model",
          [](const CesiumGltfReader::GltfReaderResult& self) -> py::object {
            if (!self.model) {
              return py::none();
            }
            return py::cast(*self.model);
          })
      .def_readwrite("errors", &CesiumGltfReader::GltfReaderResult::errors)
      .def_readwrite("warnings", &CesiumGltfReader::GltfReaderResult::warnings)
      .def_property_readonly(
          "has_model",
          [](const CesiumGltfReader::GltfReaderResult& self) {
            return self.model.has_value();
          });

  py::class_<CesiumGltfReader::GltfReaderOptions>(m, "GltfReaderOptions")
      .def(py::init<>())
      .def_readwrite(
          "decode_data_urls",
          &CesiumGltfReader::GltfReaderOptions::decodeDataUrls)
      .def_readwrite(
          "clear_decoded_data_urls",
          &CesiumGltfReader::GltfReaderOptions::clearDecodedDataUrls)
      .def_readwrite(
          "decode_embedded_images",
          &CesiumGltfReader::GltfReaderOptions::decodeEmbeddedImages)
      .def_readwrite(
          "resolve_external_images",
          &CesiumGltfReader::GltfReaderOptions::resolveExternalImages)
      .def_readwrite(
          "decode_draco",
          &CesiumGltfReader::GltfReaderOptions::decodeDraco)
      .def_readwrite(
          "decode_mesh_opt_data",
          &CesiumGltfReader::GltfReaderOptions::decodeMeshOptData)
      .def_readwrite(
          "decode_spz",
          &CesiumGltfReader::GltfReaderOptions::decodeSpz)
      .def_readwrite(
          "dequantize_mesh_data",
          &CesiumGltfReader::GltfReaderOptions::dequantizeMeshData)
      .def_readwrite(
          "apply_texture_transform",
          &CesiumGltfReader::GltfReaderOptions::applyTextureTransform)
      .def_readwrite(
          "resolve_external_structural_metadata",
          &CesiumGltfReader::GltfReaderOptions::
              resolveExternalStructuralMetadata)
      .def_readwrite(
          "ktx2_transcode_targets",
          &CesiumGltfReader::GltfReaderOptions::ktx2TranscodeTargets);

  py::class_<
      CesiumGltfReader::GltfSharedAssetSystem,
      CesiumUtility::IntrusivePointer<CesiumGltfReader::GltfSharedAssetSystem>>(
      m,
      "GltfSharedAssetSystem")
      .def_static(
          "default_system",
          &CesiumGltfReader::GltfSharedAssetSystem::getDefault);

  py::class_<
      CesiumGltfReader::NetworkImageAssetDescriptor,
      CesiumAsync::NetworkAssetDescriptor>(m, "NetworkImageAssetDescriptor")
      .def(py::init<>())
      .def(
          "__eq__",
          &CesiumGltfReader::NetworkImageAssetDescriptor::operator==,
          py::is_operator());

  py::class_<
      CesiumGltfReader::NetworkSchemaAssetDescriptor,
      CesiumAsync::NetworkAssetDescriptor>(m, "NetworkSchemaAssetDescriptor")
      .def(py::init<>())
      .def(
          "__eq__",
          &CesiumGltfReader::NetworkSchemaAssetDescriptor::operator==,
          py::is_operator());

  // ---- ImageReaderResult ----------------------------------------------------
  py::class_<CesiumGltfReader::ImageReaderResult>(m, "ImageReaderResult")
      .def_readonly("image", &CesiumGltfReader::ImageReaderResult::pImage)
      .def_readonly("errors", &CesiumGltfReader::ImageReaderResult::errors)
      .def_readonly("warnings", &CesiumGltfReader::ImageReaderResult::warnings);

  // ---- ImageDecoder --------------------------------------------------------
  py::class_<CesiumGltfReader::ImageDecoder>(m, "ImageDecoder")
      .def_static(
          "read_image",
          [](const py::bytes& data,
             std::optional<CesiumGltf::Ktx2TranscodeTargets>
                 ktx2TranscodeTargets) {
            std::vector<std::byte> buffer = CesiumPython::bytesToVector(data);
            std::span<const std::byte> view(buffer.data(), buffer.size());
            return CesiumGltfReader::ImageDecoder::readImage(
                view,
                ktx2TranscodeTargets.value_or(
                    CesiumGltf::Ktx2TranscodeTargets()));
          },
          py::arg("data"),
          py::arg("ktx2_transcode_targets") = py::none(),
          "Decode an image from bytes (PNG, JPEG, KTX2, WebP, etc.).")
      .def_static(
          "generate_mip_maps",
          &CesiumGltfReader::ImageDecoder::generateMipMaps,
          py::arg("image"),
          "Generate mip maps for an ImageAsset. Returns an error string on "
          "failure, or None on success.");

  py::class_<CesiumGltfReader::GltfReader>(m, "GltfReader")
      .def(py::init<>())
      .def_property_readonly(
          "options",
          py::overload_cast<>(&CesiumGltfReader::GltfReader::getOptions),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "extensions",
          &CesiumGltfReader::GltfReader::getExtensions,
          py::return_value_policy::reference_internal)
      .def(
          "read_gltf",
          [](const CesiumGltfReader::GltfReader& self,
             const py::bytes& data,
             const CesiumGltfReader::GltfReaderOptions& options) {
            std::vector<std::byte> buffer = CesiumPython::bytesToVector(data);
            std::span<const std::byte> view(buffer.data(), buffer.size());
            return self.readGltf(view, options);
          },
          py::arg("data"),
          py::arg("options") = CesiumGltfReader::GltfReaderOptions())
      .def(
          "read_gltf_summary",
          [](const CesiumGltfReader::GltfReader& self,
             const py::bytes& data,
             const CesiumGltfReader::GltfReaderOptions& options) {
            std::vector<std::byte> buffer = CesiumPython::bytesToVector(data);
            std::span<const std::byte> view(buffer.data(), buffer.size());
            auto result = self.readGltf(view, options);
            py::dict out;
            out["has_model"] = py::bool_(result.model.has_value());
            out["errors"] = py::cast(result.errors);
            out["warnings"] = py::cast(result.warnings);
            return out;
          },
          py::arg("data"),
          py::arg("options") = CesiumGltfReader::GltfReaderOptions())
      .def(
          "load_gltf",
          [](const CesiumGltfReader::GltfReader& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::string& url,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const py::dict& headers,
             const CesiumGltfReader::GltfReaderOptions& options) {
            std::vector<CesiumAsync::IAssetAccessor::THeader> headerPairs;
            for (auto item : headers) {
              headerPairs.emplace_back(
                  py::cast<std::string>(item.first),
                  py::cast<std::string>(item.second));
            }
            return self.loadGltf(
                asyncSystem,
                url,
                headerPairs,
                pAssetAccessor,
                options);
          },
          py::arg("async_system"),
          py::arg("url"),
          py::arg("asset_accessor"),
          py::arg("headers") = py::dict(),
          py::arg("options") = CesiumGltfReader::GltfReaderOptions(),
          "Load a glTF or GLB from a URL asynchronously, resolving all "
          "external references.")
      .def(
          "postprocess_gltf",
          &CesiumGltfReader::GltfReader::postprocessGltf,
          py::arg("result"),
          py::arg("options") = CesiumGltfReader::GltfReaderOptions(),
          "Perform post-load processing on a GltfReaderResult.")
      .def(
          "read_gltf_and_external_data",
          [](const CesiumGltfReader::GltfReader& self,
             const py::bytes& data,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const py::dict& headers,
             const std::string& baseUrl,
             const CesiumGltfReader::GltfReaderOptions& options) {
            std::vector<std::byte> buffer = CesiumPython::bytesToVector(data);
            std::span<const std::byte> view(buffer.data(), buffer.size());
            std::vector<CesiumAsync::IAssetAccessor::THeader> headerPairs;
            for (auto item : headers) {
              headerPairs.emplace_back(
                  py::cast<std::string>(item.first),
                  py::cast<std::string>(item.second));
            }
            return self.readGltfAndExternalData(
                view,
                asyncSystem,
                headerPairs,
                pAssetAccessor,
                baseUrl,
                options);
          },
          py::arg("data"),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("headers") = py::dict(),
          py::arg("base_url") = std::string(),
          py::arg("options") = CesiumGltfReader::GltfReaderOptions(),
          "Read a glTF/GLB from bytes and resolve external references "
          "asynchronously.");

  py::class_<CesiumAsync::Future<CesiumGltfReader::GltfReaderResult>>
      futGltfReaderResult(m, "FutureGltfReaderResult");
  CesiumPython::bindFuture(futGltfReaderResult);
}
