#include <Cesium3DTiles/Schema.h>
#include <Cesium3DTiles/Subtree.h>
#include <Cesium3DTiles/Tileset.h>
#include <Cesium3DTilesWriter/SchemaWriter.h>
#include <Cesium3DTilesWriter/SubtreeWriter.h>
#include <Cesium3DTilesWriter/TilesetWriter.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void initTiles3dWriterBindings(py::module& m) {
  py::class_<Cesium3DTilesWriter::TilesetWriterResult>(m, "TilesetWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "tileset_bytes",
          [](const Cesium3DTilesWriter::TilesetWriterResult& self) {
            return py::bytes(
                reinterpret_cast<const char*>(self.tilesetBytes.data()),
                self.tilesetBytes.size());
          })
      .def_readwrite(
          "errors",
          &Cesium3DTilesWriter::TilesetWriterResult::errors)
      .def_readwrite(
          "warnings",
          &Cesium3DTilesWriter::TilesetWriterResult::warnings);

  py::class_<Cesium3DTilesWriter::TilesetWriterOptions>(
      m,
      "TilesetWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &Cesium3DTilesWriter::TilesetWriterOptions::prettyPrint);

  py::class_<Cesium3DTilesWriter::SubtreeWriterResult>(m, "SubtreeWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "subtree_bytes",
          [](const Cesium3DTilesWriter::SubtreeWriterResult& self) {
            return py::bytes(
                reinterpret_cast<const char*>(self.subtreeBytes.data()),
                self.subtreeBytes.size());
          })
      .def_readwrite(
          "errors",
          &Cesium3DTilesWriter::SubtreeWriterResult::errors)
      .def_readwrite(
          "warnings",
          &Cesium3DTilesWriter::SubtreeWriterResult::warnings);

  py::class_<Cesium3DTilesWriter::SubtreeWriterOptions>(
      m,
      "SubtreeWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &Cesium3DTilesWriter::SubtreeWriterOptions::prettyPrint);

  py::class_<Cesium3DTilesWriter::SchemaWriterResult>(m, "SchemaWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "schema_bytes",
          [](const Cesium3DTilesWriter::SchemaWriterResult& self) {
            return py::bytes(
                reinterpret_cast<const char*>(self.schemaBytes.data()),
                self.schemaBytes.size());
          })
      .def_readwrite("errors", &Cesium3DTilesWriter::SchemaWriterResult::errors)
      .def_readwrite(
          "warnings",
          &Cesium3DTilesWriter::SchemaWriterResult::warnings);

  py::class_<Cesium3DTilesWriter::SchemaWriterOptions>(m, "SchemaWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &Cesium3DTilesWriter::SchemaWriterOptions::prettyPrint);

  py::class_<Cesium3DTilesWriter::TilesetWriter>(m, "TilesetWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(
              &Cesium3DTilesWriter::TilesetWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write_tileset",
          &Cesium3DTilesWriter::TilesetWriter::writeTileset,
          py::arg("tileset"),
          py::arg("options") = Cesium3DTilesWriter::TilesetWriterOptions());

  py::class_<Cesium3DTilesWriter::SubtreeWriter>(m, "SubtreeWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(
              &Cesium3DTilesWriter::SubtreeWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write_subtree_json",
          &Cesium3DTilesWriter::SubtreeWriter::writeSubtreeJson,
          py::arg("subtree"),
          py::arg("options") = Cesium3DTilesWriter::SubtreeWriterOptions())
      .def(
          "write_subtree_binary",
          [](const Cesium3DTilesWriter::SubtreeWriter& self,
             const Cesium3DTiles::Subtree& subtree,
             py::bytes bufferData,
             const Cesium3DTilesWriter::SubtreeWriterOptions& options) {
            std::string bytes = bufferData;
            std::vector<std::byte> data(bytes.size());
            for (size_t i = 0; i < bytes.size(); ++i) {
              data[i] =
                  static_cast<std::byte>(static_cast<unsigned char>(bytes[i]));
            }
            return self.writeSubtreeBinary(
                subtree,
                std::span<const std::byte>(data.data(), data.size()),
                options);
          },
          py::arg("subtree"),
          py::arg("buffer_data"),
          py::arg("options") = Cesium3DTilesWriter::SubtreeWriterOptions());

  py::class_<Cesium3DTilesWriter::SchemaWriter>(m, "SchemaWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(
              &Cesium3DTilesWriter::SchemaWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write_schema",
          &Cesium3DTilesWriter::SchemaWriter::writeSchema,
          py::arg("schema"),
          py::arg("options") = Cesium3DTilesWriter::SchemaWriterOptions());

  m.def(
      "write_tileset",
      [](const Cesium3DTiles::Tileset& tileset,
         const Cesium3DTilesWriter::TilesetWriterOptions& options) {
        Cesium3DTilesWriter::TilesetWriter writer;
        return writer.writeTileset(tileset, options);
      },
      py::arg("tileset"),
      py::arg("options") = Cesium3DTilesWriter::TilesetWriterOptions());

  m.def(
      "write_subtree_json",
      [](const Cesium3DTiles::Subtree& subtree,
         const Cesium3DTilesWriter::SubtreeWriterOptions& options) {
        Cesium3DTilesWriter::SubtreeWriter writer;
        return writer.writeSubtreeJson(subtree, options);
      },
      py::arg("subtree"),
      py::arg("options") = Cesium3DTilesWriter::SubtreeWriterOptions());

  m.def(
      "write_schema",
      [](const Cesium3DTiles::Schema& schema,
         const Cesium3DTilesWriter::SchemaWriterOptions& options) {
        Cesium3DTilesWriter::SchemaWriter writer;
        return writer.writeSchema(schema, options);
      },
      py::arg("schema"),
      py::arg("options") = Cesium3DTilesWriter::SchemaWriterOptions());
}
