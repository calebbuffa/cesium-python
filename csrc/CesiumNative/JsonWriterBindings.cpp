#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <CesiumJsonWriter/ExtensionWriterContext.h>
#include <CesiumJsonWriter/JsonObjectWriter.h>
#include <CesiumJsonWriter/JsonWriter.h>
#include <CesiumJsonWriter/PrettyJsonWriter.h>
#include <CesiumUtility/JsonValue.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace py = pybind11;

void initJsonWriterBindings(py::module& m) {
  py::enum_<CesiumJsonWriter::ExtensionState>(m, "ExtensionState")
      .value("Enabled", CesiumJsonWriter::ExtensionState::Enabled)
      .value("Disabled", CesiumJsonWriter::ExtensionState::Disabled)
      .export_values();

  py::class_<CesiumJsonWriter::ExtensionWriterContext>(
      m,
      "ExtensionWriterContext")
      .def(py::init<>())
      .def(
          "extension_state",
          &CesiumJsonWriter::ExtensionWriterContext::getExtensionState,
          py::arg("extension_name"))
      .def(
          "set_extension_state",
          &CesiumJsonWriter::ExtensionWriterContext::setExtensionState,
          py::arg("extension_name"),
          py::arg("new_state"));

  py::class_<CesiumJsonWriter::JsonWriter>(m, "JsonWriter")
      .def(py::init<>())
      .def("null", &CesiumJsonWriter::JsonWriter::Null)
      .def("bool", &CesiumJsonWriter::JsonWriter::Bool, py::arg("value"))
      .def("int", &CesiumJsonWriter::JsonWriter::Int, py::arg("value"))
      .def("uint", &CesiumJsonWriter::JsonWriter::Uint, py::arg("value"))
      .def("int64", &CesiumJsonWriter::JsonWriter::Int64, py::arg("value"))
      .def("uint64", &CesiumJsonWriter::JsonWriter::Uint64, py::arg("value"))
      .def("double", &CesiumJsonWriter::JsonWriter::Double, py::arg("value"))
      .def("key", &CesiumJsonWriter::JsonWriter::Key, py::arg("key"))
      .def("string", &CesiumJsonWriter::JsonWriter::String, py::arg("value"))
      .def("start_object", &CesiumJsonWriter::JsonWriter::StartObject)
      .def("end_object", &CesiumJsonWriter::JsonWriter::EndObject)
      .def("start_array", &CesiumJsonWriter::JsonWriter::StartArray)
      .def("end_array", &CesiumJsonWriter::JsonWriter::EndArray)
      .def("to_string", &CesiumJsonWriter::JsonWriter::toString)
      .def(
          "to_bytes",
          [](CesiumJsonWriter::JsonWriter& self) {
            return CesiumPython::vectorToBytes(self.toBytes());
          })
      .def("errors", &CesiumJsonWriter::JsonWriter::getErrors)
      .def("warnings", &CesiumJsonWriter::JsonWriter::getWarnings);

  py::class_<CesiumJsonWriter::PrettyJsonWriter, CesiumJsonWriter::JsonWriter>(
      m,
      "PrettyJsonWriter")
      .def(py::init<>());

  m.def(
      "write_json_value",
      [](const py::object& value, CesiumJsonWriter::JsonWriter& writer) {
        CesiumJsonWriter::writeJsonValue(
            CesiumPython::pyToJsonValue(value),
            writer);
      },
      py::arg("value"),
      py::arg("writer"));

  m.def(
      "dumps_pretty",
      [](const py::object& value) {
        CesiumJsonWriter::PrettyJsonWriter writer;
        CesiumJsonWriter::writeJsonValue(
            CesiumPython::pyToJsonValue(value),
            writer);
        return writer.toString();
      },
      py::arg("value"));
}
