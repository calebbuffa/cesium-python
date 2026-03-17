#include "FutureTemplates.h"
#include "JsonConversions.h"

#include <Cesium3DTilesReader/SubtreeFileReader.h>
#include <Cesium3DTilesReader/SubtreeReader.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumJsonReader/JsonReader.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {
using ReadSubtreeResult =
    CesiumJsonReader::ReadJsonResult<Cesium3DTiles::Subtree>;
using FutureReadSubtreeResult = CesiumAsync::Future<ReadSubtreeResult>;
} // namespace

void initTiles3dReaderBindings(py::module& m) {
  py::class_<Cesium3DTilesReader::SubtreeReader>(m, "SubtreeReader")
      .def(py::init<>())
      .def_property_readonly(
          "options",
          py::overload_cast<>(&Cesium3DTilesReader::SubtreeReader::getOptions),
          py::return_value_policy::reference_internal)
      .def(
          "read_from_json_summary",
          [](const Cesium3DTilesReader::SubtreeReader& self,
             py::bytes subtreeBytes) {
            std::string bytes = subtreeBytes;
            std::vector<std::byte> data(bytes.size());
            for (size_t i = 0; i < bytes.size(); ++i) {
              data[i] =
                  static_cast<std::byte>(static_cast<unsigned char>(bytes[i]));
            }

            auto result = self.readFromJson(
                std::span<const std::byte>(data.data(), data.size()));
            py::dict out;
            out["has_value"] = result.value.has_value();
            out["errors"] = result.errors;
            out["warnings"] = result.warnings;
            return out;
          },
          py::arg("subtree_bytes"));

  py::class_<Cesium3DTilesReader::SubtreeFileReader>(m, "SubtreeFileReader")
      .def(py::init<>())
      .def_property_readonly(
          "options",
          py::overload_cast<>(
              &Cesium3DTilesReader::SubtreeFileReader::getOptions),
          py::return_value_policy::reference_internal)
      .def(
          "load",
          [](const Cesium3DTilesReader::SubtreeFileReader& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const std::string& url,
             const std::vector<std::pair<std::string, std::string>>& headers) {
            std::vector<CesiumAsync::IAssetAccessor::THeader> cppHeaders(
                headers.begin(),
                headers.end());
            return self.load(asyncSystem, pAssetAccessor, url, cppHeaders);
          },
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("url"),
          py::arg("headers") =
              std::vector<std::pair<std::string, std::string>>{});

  py::class_<ReadSubtreeResult>(m, "ReadSubtreeResult")
      .def_readwrite("value", &ReadSubtreeResult::value)
      .def_readwrite("errors", &ReadSubtreeResult::errors)
      .def_readwrite("warnings", &ReadSubtreeResult::warnings)
      .def("__bool__", [](const ReadSubtreeResult& self) {
        return self.value.has_value();
      });

  auto futReadSubtree =
      py::class_<FutureReadSubtreeResult>(m, "FutureReadSubtreeResult");
  CesiumPython::bindFuture(futReadSubtree);

  m.def(
      "read_subtree_json_summary",
      [](py::bytes subtreeBytes) {
        Cesium3DTilesReader::SubtreeReader reader;
        std::string bytes = subtreeBytes;
        std::vector<std::byte> data(bytes.size());
        for (size_t i = 0; i < bytes.size(); ++i) {
          data[i] =
              static_cast<std::byte>(static_cast<unsigned char>(bytes[i]));
        }

        auto result = reader.readFromJson(
            std::span<const std::byte>(data.data(), data.size()));
        py::dict out;
        out["has_value"] = result.value.has_value();
        out["errors"] = result.errors;
        out["warnings"] = result.warnings;
        return out;
      },
      py::arg("subtree_bytes"));
}
