#include <CesiumQuantizedMeshTerrain/AvailabilityRectangle.h>
#include <CesiumQuantizedMeshTerrain/Layer.h>
#include <CesiumQuantizedMeshTerrain/LayerReader.h>
#include <CesiumQuantizedMeshTerrain/LayerWriter.h>
#include <CesiumQuantizedMeshTerrain/QuantizedMeshLoader.h>

#include <CesiumGeospatial/Ellipsoid.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void initQuantizedMeshTerrainBindings(py::module& m) {
  py::class_<CesiumQuantizedMeshTerrain::AvailabilityRectangle>(
      m,
      "AvailabilityRectangle")
      .def(py::init<>())
      .def_readwrite(
          "start_x",
          &CesiumQuantizedMeshTerrain::AvailabilityRectangle::startX)
      .def_readwrite(
          "start_y",
          &CesiumQuantizedMeshTerrain::AvailabilityRectangle::startY)
      .def_readwrite(
          "end_x",
          &CesiumQuantizedMeshTerrain::AvailabilityRectangle::endX)
      .def_readwrite(
          "end_y",
          &CesiumQuantizedMeshTerrain::AvailabilityRectangle::endY)
      .def_property_readonly(
          "size_bytes",
          &CesiumQuantizedMeshTerrain::AvailabilityRectangle::getSizeBytes);

  py::class_<CesiumQuantizedMeshTerrain::Layer>(m, "Layer")
      .def(py::init<>())
      .def_readwrite(
          "attribution",
          &CesiumQuantizedMeshTerrain::Layer::attribution)
      .def_readwrite("available", &CesiumQuantizedMeshTerrain::Layer::available)
      .def_readwrite("bounds", &CesiumQuantizedMeshTerrain::Layer::bounds)
      .def_readwrite(
          "description",
          &CesiumQuantizedMeshTerrain::Layer::description)
      .def_readwrite(
          "extensions_property",
          &CesiumQuantizedMeshTerrain::Layer::extensionsProperty)
      .def_readwrite("format", &CesiumQuantizedMeshTerrain::Layer::format)
      .def_readwrite("maxzoom", &CesiumQuantizedMeshTerrain::Layer::maxzoom)
      .def_readwrite("minzoom", &CesiumQuantizedMeshTerrain::Layer::minzoom)
      .def_readwrite(
          "metadata_availability",
          &CesiumQuantizedMeshTerrain::Layer::metadataAvailability)
      .def_readwrite("name", &CesiumQuantizedMeshTerrain::Layer::name)
      .def_readwrite(
          "parent_url",
          &CesiumQuantizedMeshTerrain::Layer::parentUrl)
      .def_readwrite(
          "projection",
          &CesiumQuantizedMeshTerrain::Layer::projection)
      .def_readwrite("scheme", &CesiumQuantizedMeshTerrain::Layer::scheme)
      .def_readwrite("tiles", &CesiumQuantizedMeshTerrain::Layer::tiles)
      .def_readwrite("version", &CesiumQuantizedMeshTerrain::Layer::version)
      .def_property_readonly(
          "size_bytes",
          &CesiumQuantizedMeshTerrain::Layer::getSizeBytes)
      .def(
          "get_projection",
          &CesiumQuantizedMeshTerrain::Layer::getProjection,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "get_tiling_scheme",
          &CesiumQuantizedMeshTerrain::Layer::getTilingScheme,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "get_root_bounding_region",
          &CesiumQuantizedMeshTerrain::Layer::getRootBoundingRegion,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84);

  py::class_<CesiumQuantizedMeshTerrain::LayerWriterOptions>(
      m,
      "LayerWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &CesiumQuantizedMeshTerrain::LayerWriterOptions::prettyPrint);

  py::class_<CesiumQuantizedMeshTerrain::LayerWriterResult>(
      m,
      "LayerWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "bytes",
          [](const CesiumQuantizedMeshTerrain::LayerWriterResult& self) {
            return py::bytes(
                reinterpret_cast<const char*>(self.bytes.data()),
                self.bytes.size());
          })
      .def_readwrite(
          "errors",
          &CesiumQuantizedMeshTerrain::LayerWriterResult::errors)
      .def_readwrite(
          "warnings",
          &CesiumQuantizedMeshTerrain::LayerWriterResult::warnings);

  py::class_<CesiumQuantizedMeshTerrain::LayerWriter>(m, "LayerWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(
              &CesiumQuantizedMeshTerrain::LayerWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write",
          &CesiumQuantizedMeshTerrain::LayerWriter::write,
          py::arg("layer"),
          py::arg("options") =
              CesiumQuantizedMeshTerrain::LayerWriterOptions());

  py::class_<CesiumQuantizedMeshTerrain::LayerReader>(m, "LayerReader")
      .def(py::init<>())
      .def_property_readonly(
          "options",
          py::overload_cast<>(
              &CesiumQuantizedMeshTerrain::LayerReader::getOptions),
          py::return_value_policy::reference_internal)
      .def(
          "read_from_json_summary",
          [](const CesiumQuantizedMeshTerrain::LayerReader& self,
             py::bytes layerJsonBytes) {
            std::string bytes = layerJsonBytes;
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
            if (result.value) {
              out["tile_template_count"] = result.value->tiles.size();
              out["availability_levels"] = result.value->available.size();
              out["projection"] = result.value->projection;
              out["scheme"] = result.value->scheme;
            } else {
              out["tile_template_count"] = 0;
              out["availability_levels"] = 0;
              out["projection"] = py::none();
              out["scheme"] = py::none();
            }
            return out;
          },
          py::arg("layer_json_bytes"));

  m.def(
      "read_layer_json_summary",
      [](py::bytes layerJsonBytes) {
        CesiumQuantizedMeshTerrain::LayerReader reader;
        return py::cast(reader).attr("read_from_json_summary")(layerJsonBytes);
      },
      py::arg("layer_json_bytes"));

  py::class_<CesiumQuantizedMeshTerrain::QuantizedMeshLoadResult>(
      m,
      "QuantizedMeshLoadResult")
      .def(py::init<>())
      .def_readwrite(
          "model",
          &CesiumQuantizedMeshTerrain::QuantizedMeshLoadResult::model)
      .def_readwrite(
          "updated_bounding_volume",
          &CesiumQuantizedMeshTerrain::QuantizedMeshLoadResult::
              updatedBoundingVolume)
      .def_readwrite(
          "available_tile_rectangles",
          &CesiumQuantizedMeshTerrain::QuantizedMeshLoadResult::
              availableTileRectangles)
      .def_readwrite(
          "errors",
          &CesiumQuantizedMeshTerrain::QuantizedMeshLoadResult::errors);

  py::class_<CesiumQuantizedMeshTerrain::QuantizedMeshMetadataResult>(
      m,
      "QuantizedMeshMetadataResult")
      .def(py::init<>())
      .def_readwrite(
          "availability",
          &CesiumQuantizedMeshTerrain::QuantizedMeshMetadataResult::
              availability)
      .def_readwrite(
          "errors",
          &CesiumQuantizedMeshTerrain::QuantizedMeshMetadataResult::errors);

  py::class_<CesiumQuantizedMeshTerrain::QuantizedMeshLoader>(
      m,
      "QuantizedMeshLoader")
      .def_static(
          "load",
          [](const CesiumGeometry::QuadtreeTileID& tileID,
             const CesiumGeospatial::BoundingRegion& tileBoundingVolume,
             const std::string& url,
             py::bytes data,
             bool enableWaterMask,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            std::string bytes = data;
            return CesiumQuantizedMeshTerrain::QuantizedMeshLoader::load(
                tileID,
                tileBoundingVolume,
                url,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(bytes.data()),
                    bytes.size()),
                enableWaterMask,
                ellipsoid);
          },
          py::arg("tile_id"),
          py::arg("tile_bounding_volume"),
          py::arg("url"),
          py::arg("data"),
          py::arg("enable_water_mask") = false,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def_static(
          "load_metadata",
          [](py::bytes data,
             const CesiumGeometry::QuadtreeTileID& tileID) {
            std::string bytes = data;
            return CesiumQuantizedMeshTerrain::QuantizedMeshLoader::
                loadMetadata(
                    std::span<const std::byte>(
                        reinterpret_cast<const std::byte*>(bytes.data()),
                        bytes.size()),
                    tileID);
          },
          py::arg("data"),
          py::arg("tile_id"));
}
