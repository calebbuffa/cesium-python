#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <CesiumGltf/Model.h>
#include <CesiumGltf/Schema.h>
#include <CesiumGltfWriter/GltfWriter.h>
#include <CesiumGltfWriter/SchemaWriter.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <vector>

namespace py = pybind11;

void initGltfWriterBindings(py::module& m) {
  py::class_<CesiumGltfWriter::GltfWriterResult>(m, "GltfWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "gltf_bytes",
          [](const CesiumGltfWriter::GltfWriterResult& self) {
            return CesiumPython::vectorToBytes(self.gltfBytes);
          })
      .def_readwrite("errors", &CesiumGltfWriter::GltfWriterResult::errors)
      .def_readwrite("warnings", &CesiumGltfWriter::GltfWriterResult::warnings);

  py::class_<CesiumGltfWriter::GltfWriterOptions>(m, "GltfWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &CesiumGltfWriter::GltfWriterOptions::prettyPrint)
      .def_readwrite(
          "binary_chunk_byte_alignment",
          &CesiumGltfWriter::GltfWriterOptions::binaryChunkByteAlignment);

  py::class_<CesiumGltfWriter::SchemaWriterResult>(m, "SchemaWriterResult")
      .def(py::init<>())
      .def_property_readonly(
          "schema_bytes",
          [](const CesiumGltfWriter::SchemaWriterResult& self) {
            return CesiumPython::vectorToBytes(self.schemaBytes);
          })
      .def_readwrite("errors", &CesiumGltfWriter::SchemaWriterResult::errors)
      .def_readwrite(
          "warnings",
          &CesiumGltfWriter::SchemaWriterResult::warnings);

  py::class_<CesiumGltfWriter::SchemaWriterOptions>(m, "SchemaWriterOptions")
      .def(py::init<>())
      .def_readwrite(
          "pretty_print",
          &CesiumGltfWriter::SchemaWriterOptions::prettyPrint);

  py::class_<CesiumGltfWriter::GltfWriter>(m, "GltfWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(&CesiumGltfWriter::GltfWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write_gltf",
          &CesiumGltfWriter::GltfWriter::writeGltf,
          py::arg("model"),
          py::arg("options") = CesiumGltfWriter::GltfWriterOptions())
      .def(
          "write_glb",
          [](const CesiumGltfWriter::GltfWriter& self,
             const CesiumGltf::Model& model,
             const py::bytes& buffer_data,
             const CesiumGltfWriter::GltfWriterOptions& options) {
            std::vector<std::byte> data =
                CesiumPython::bytesToVector(buffer_data);
            return self.writeGlb(
                model,
                std::span<const std::byte>(data.data(), data.size()),
                options);
          },
          py::arg("model"),
          py::arg("buffer_data"),
          py::arg("options") = CesiumGltfWriter::GltfWriterOptions());

  py::class_<CesiumGltfWriter::SchemaWriter>(m, "SchemaWriter")
      .def(py::init<>())
      .def_property_readonly(
          "extensions",
          py::overload_cast<>(&CesiumGltfWriter::SchemaWriter::getExtensions),
          py::return_value_policy::reference_internal)
      .def(
          "write_schema",
          &CesiumGltfWriter::SchemaWriter::writeSchema,
          py::arg("schema"),
          py::arg("options") = CesiumGltfWriter::SchemaWriterOptions());
}
