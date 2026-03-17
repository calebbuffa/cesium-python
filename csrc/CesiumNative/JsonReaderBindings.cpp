#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <CesiumJsonReader/JsonObjectJsonHandler.h>
#include <CesiumJsonReader/JsonReader.h>
#include <CesiumJsonReader/JsonReaderOptions.h>
#include <CesiumUtility/JsonValue.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace py = pybind11;

void initJsonReaderBindings(py::module& m) {
  py::enum_<CesiumJsonReader::ExtensionState>(m, "ExtensionState")
      .value("Enabled", CesiumJsonReader::ExtensionState::Enabled)
      .value("JsonOnly", CesiumJsonReader::ExtensionState::JsonOnly)
      .value("Disabled", CesiumJsonReader::ExtensionState::Disabled)
      .export_values();

  py::class_<CesiumJsonReader::JsonReaderOptions>(m, "JsonReaderOptions")
      .def(py::init<>())
      .def_property(
          "capture_unknown_properties",
          &CesiumJsonReader::JsonReaderOptions::getCaptureUnknownProperties,
          &CesiumJsonReader::JsonReaderOptions::setCaptureUnknownProperties)
      .def(
          "extension_state",
          &CesiumJsonReader::JsonReaderOptions::getExtensionState,
          py::arg("extension_name"))
      .def(
          "set_extension_state",
          &CesiumJsonReader::JsonReaderOptions::setExtensionState,
          py::arg("extension_name"),
          py::arg("new_state"));

  m.def(
      "read_json_value",
      [](const py::bytes& data) {
        std::vector<std::byte> buffer = CesiumPython::bytesToVector(data);
        std::span<const std::byte> view(buffer.data(), buffer.size());

        CesiumJsonReader::JsonObjectJsonHandler handler;
        auto result = CesiumJsonReader::JsonReader::readJson(view, handler);

        py::dict out;
        out["value"] = result.value ? CesiumPython::jsonValueToPy(*result.value)
                                    : py::none();
        out["errors"] = py::cast(result.errors);
        out["warnings"] = py::cast(result.warnings);
        return out;
      },
      py::arg("data"));
}
