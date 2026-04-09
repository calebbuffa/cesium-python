#include "JsonConversions.h"

#include <CesiumUtility/AttributeCompression.h>
#include <CesiumUtility/Color.h>
#include <CesiumUtility/CreditReferencer.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/ErrorList.h>
#include <CesiumUtility/ExtensibleObject.h>
#include <CesiumUtility/Gzip.h>
#include <CesiumUtility/JsonValue.h>
#include <CesiumUtility/Math.h>
#include <CesiumUtility/Uri.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace py = pybind11;

void initUtilityBindings(py::module& m) {
  py::class_<CesiumUtility::ErrorList>(m, "ErrorList")
      .def(py::init<>())
      .def_static(
          "error",
          &CesiumUtility::ErrorList::error,
          py::arg("error_message"))
      .def_static(
          "warning",
          &CesiumUtility::ErrorList::warning,
          py::arg("warning_message"))
      .def(
          "merge",
          py::overload_cast<const CesiumUtility::ErrorList&>(
              &CesiumUtility::ErrorList::merge),
          py::arg("error_list"))
      .def(
          "emplace_error",
          [](CesiumUtility::ErrorList& self, const std::string& error) {
            self.emplaceError(error);
          },
          py::arg("error"))
      .def(
          "emplace_warning",
          [](CesiumUtility::ErrorList& self, const std::string& warning) {
            self.emplaceWarning(warning);
          },
          py::arg("warning"))
      .def("has_errors", &CesiumUtility::ErrorList::hasErrors)
      .def(
          "format",
          [](const CesiumUtility::ErrorList& self, const std::string& prompt) {
            return self.format(prompt);
          },
          py::arg("prompt"))
      .def(
          "__bool__",
          [](const CesiumUtility::ErrorList& self) {
            return static_cast<bool>(self);
          })
      .def_readwrite("errors", &CesiumUtility::ErrorList::errors)
      .def_readwrite("warnings", &CesiumUtility::ErrorList::warnings);

  py::class_<CesiumUtility::JsonValue>(m, "JsonValue")
      .def(py::init<>())
      .def(
          py::init([](const py::object& value) {
            return CesiumPython::pyToJsonValue(value);
          }),
          py::arg("value"))
      .def_property_readonly(
          "value",
          [](const CesiumUtility::JsonValue& self) {
            return CesiumPython::jsonValueToPy(self);
          })
      .def(
          "__repr__",
          [](const CesiumUtility::JsonValue& self) {
            return py::str("JsonValue(") +
                   py::repr(CesiumPython::jsonValueToPy(self)) + py::str(")");
          })
      .def(
          "__len__",
          [](const CesiumUtility::JsonValue& self) -> py::ssize_t {
            if (self.isObject()) {
              return static_cast<py::ssize_t>(self.getObject().size());
            }
            if (self.isArray()) {
              return static_cast<py::ssize_t>(self.getArray().size());
            }
            throw py::type_error(
                "len() is only supported for object and array JsonValue");
          })
      .def(
          "__iter__",
          [](const CesiumUtility::JsonValue& self) {
            if (!self.isObject()) {
              throw py::type_error(
                  "Iteration is only supported for object JsonValue");
            }
            const auto& objectValue = self.getObject();
            return py::make_key_iterator(
                objectValue.begin(),
                objectValue.end());
          },
          py::keep_alive<0, 1>())
      .def(
          "get",
          [](const CesiumUtility::JsonValue& self,
             const std::string& key,
             py::object defaultValue) -> py::object {
            const CesiumUtility::JsonValue* found = self.getValuePtrForKey(key);
            if (!found) {
              return defaultValue;
            }
            return CesiumPython::jsonValueToPy(*found);
          },
          py::arg("key"),
          py::arg("default") = py::none())
      .def("__contains__", &CesiumUtility::JsonValue::hasKey)
      .def(
          "__getitem__",
          [](const CesiumUtility::JsonValue& self,
             const py::object& keyOrIndex) -> py::object {
            if (py::isinstance<py::str>(keyOrIndex)) {
              if (!self.isObject()) {
                throw py::type_error(
                    "String key access is only supported for object JsonValue");
              }
              const std::string key = py::cast<std::string>(keyOrIndex);
              const CesiumUtility::JsonValue* found =
                  self.getValuePtrForKey(key);
              if (!found) {
                throw py::key_error(key);
              }
              return CesiumPython::jsonValueToPy(*found);
            }
            if (py::isinstance<py::int_>(keyOrIndex)) {
              if (!self.isArray()) {
                throw py::type_error(
                    "Integer index access is only supported for array "
                    "JsonValue");
              }
              py::ssize_t index = py::cast<py::ssize_t>(keyOrIndex);
              const auto& arrayValue = self.getArray();
              py::ssize_t size = static_cast<py::ssize_t>(arrayValue.size());
              if (index < 0) {
                index += size;
              }
              if (index < 0 || index >= size) {
                throw py::index_error("JsonValue array index out of range");
              }
              return CesiumPython::jsonValueToPy(
                  arrayValue[static_cast<size_t>(index)]);
            }
            throw py::type_error(
                "JsonValue indices must be str (object) or int (array)");
          })
      .def(
          "keys",
          [](const CesiumUtility::JsonValue& self) -> py::object {
            if (!self.isObject()) {
              throw py::type_error(
                  "keys() is only supported for object JsonValue");
            }
            py::dict out;
            for (const auto& kv : self.getObject()) {
              out[py::str(kv.first)] = CesiumPython::jsonValueToPy(kv.second);
            }
            return out.attr("keys")();
          })
      .def(
          "items",
          [](const CesiumUtility::JsonValue& self) -> py::object {
            if (!self.isObject()) {
              throw py::type_error(
                  "items() is only supported for object JsonValue");
            }
            py::dict out;
            for (const auto& kv : self.getObject()) {
              out[py::str(kv.first)] = CesiumPython::jsonValueToPy(kv.second);
            }
            return out.attr("items")();
          })
      .def(
          "values",
          [](const CesiumUtility::JsonValue& self) -> py::object {
            if (!self.isObject()) {
              throw py::type_error(
                  "values() is only supported for object JsonValue");
            }
            py::dict out;
            for (const auto& kv : self.getObject()) {
              out[py::str(kv.first)] = CesiumPython::jsonValueToPy(kv.second);
            }
            return out.attr("values")();
          })
      .def("is_null", &CesiumUtility::JsonValue::isNull)
      .def("is_number", &CesiumUtility::JsonValue::isNumber)
      .def("is_bool", &CesiumUtility::JsonValue::isBool)
      .def("is_string", &CesiumUtility::JsonValue::isString)
      .def("is_object", &CesiumUtility::JsonValue::isObject)
      .def("is_array", &CesiumUtility::JsonValue::isArray)
      .def("is_double", &CesiumUtility::JsonValue::isDouble)
      .def("is_uint64", &CesiumUtility::JsonValue::isUint64)
      .def("is_int64", &CesiumUtility::JsonValue::isInt64)
      .def("__eq__", &CesiumUtility::JsonValue::operator==, py::is_operator());

  // ── ExtensibleObject ─────────────────────────────────────────────────────
  py::class_<CesiumUtility::ExtensibleObject>(m, "ExtensibleObject")
      .def_property(
          "extras",
          [](const CesiumUtility::ExtensibleObject& self) {
            py::dict out;
            for (const auto& [k, v] : self.extras) {
              out[py::str(k)] = CesiumPython::jsonValueToPy(v);
            }
            return out;
          },
          [](CesiumUtility::ExtensibleObject& self, const py::dict& d) {
            CesiumUtility::JsonValue::Object obj;
            for (auto item : d) {
              obj[py::cast<std::string>(item.first)] =
                  CesiumPython::pyToJsonValue(item.second);
            }
            self.extras = std::move(obj);
          })
      .def_property(
          "unknown_properties",
          [](const CesiumUtility::ExtensibleObject& self) {
            py::dict out;
            for (const auto& [k, v] : self.unknownProperties) {
              out[py::str(k)] = CesiumPython::jsonValueToPy(v);
            }
            return out;
          },
          [](CesiumUtility::ExtensibleObject& self, const py::dict& d) {
            CesiumUtility::JsonValue::Object obj;
            for (auto item : d) {
              obj[py::cast<std::string>(item.first)] =
                  CesiumPython::pyToJsonValue(item.second);
            }
            self.unknownProperties = std::move(obj);
          })
      .def(
          "get_generic_extension",
          [](const CesiumUtility::ExtensibleObject& self,
             const std::string& name) -> py::object {
            const CesiumUtility::JsonValue* ext =
                self.getGenericExtension(name);
            if (!ext) {
              return py::none();
            }
            return CesiumPython::jsonValueToPy(*ext);
          },
          py::arg("name"))
      .def_property_readonly(
          "extension_names",
          [](const CesiumUtility::ExtensibleObject& self) {
            py::list out;
            for (const auto& [k, _] : self.extensions) {
              out.append(py::str(k));
            }
            return out;
          })
      .def_property_readonly(
          "size_bytes",
          &CesiumUtility::ExtensibleObject::getSizeBytes);

  py::class_<CesiumUtility::Uri>(m, "Uri")
      .def(py::init<const std::string&>(), py::arg("uri"))
      .def(
          py::init<const CesiumUtility::Uri&, const std::string&, bool>(),
          py::arg("base"),
          py::arg("relative"),
          py::arg("use_base_query") = false)
      .def(
          "__str__",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.toString());
          })
      .def(
          "__repr__",
          [](const CesiumUtility::Uri& self) {
            return "Uri('" + std::string(self.toString()) + "')";
          })
      .def("__bool__", &CesiumUtility::Uri::isValid)
      .def_property_readonly(
          "string",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.toString());
          })
      .def_property_readonly("valid", &CesiumUtility::Uri::isValid)
      .def_property_readonly(
          "scheme",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getScheme());
          })
      .def_property_readonly(
          "host",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getHost());
          })
      .def_property(
          "path",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getPath());
          },
          [](CesiumUtility::Uri& self, const std::string_view& path) {
            self.setPath(path);
          })
      .def_property_readonly(
          "file_name",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getFileName());
          })
      .def_property_readonly(
          "stem",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getStem());
          })
      .def_property_readonly(
          "extension",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getExtension());
          })
      .def_property(
          "query",
          [](const CesiumUtility::Uri& self) {
            return std::string(self.getQuery());
          },
          &CesiumUtility::Uri::setQuery)
      .def("ensure_trailing_slash", &CesiumUtility::Uri::ensureTrailingSlash)
      .def_static(
          "resolve",
          &CesiumUtility::Uri::resolve,
          py::arg("base"),
          py::arg("relative"),
          py::arg("use_base_query") = false,
          py::arg("assume_https_default") = true)
      .def_static("add_query", &CesiumUtility::Uri::addQuery)
      .def_static("query_value", &CesiumUtility::Uri::getQueryValue)
      .def_static(
          "substitute_template_parameters",
          &CesiumUtility::Uri::substituteTemplateParameters)
      .def_static("escape", &CesiumUtility::Uri::escape)
      .def_static("unescape", &CesiumUtility::Uri::unescape)
      .def_static(
          "unix_path_to_uri_path",
          &CesiumUtility::Uri::unixPathToUriPath)
      .def_static(
          "windows_path_to_uri_path",
          &CesiumUtility::Uri::windowsPathToUriPath)
      .def_static(
          "native_path_to_uri_path",
          &CesiumUtility::Uri::nativePathToUriPath)
      .def_static(
          "uri_path_to_unix_path",
          &CesiumUtility::Uri::uriPathToUnixPath)
      .def_static(
          "uri_path_to_windows_path",
          &CesiumUtility::Uri::uriPathToWindowsPath)
      .def_static(
          "uri_path_to_native_path",
          &CesiumUtility::Uri::uriPathToNativePath)
      .def_static(
          "path_from_uri",
          static_cast<std::string (*)(const std::string&)>(
              &CesiumUtility::Uri::getPath))
      .def_static(
          "with_path",
          [](const std::string& uri, const std::string& newPath) {
            return CesiumUtility::Uri::setPath(uri, newPath);
          });

  py::class_<CesiumUtility::UriQuery>(m, "UriQuery")
      .def(py::init<>())
      .def(py::init<const std::string_view&>(), py::arg("query_string"))
      .def(py::init<const CesiumUtility::Uri&>(), py::arg("uri"))
      .def(
          "__str__",
          [](const CesiumUtility::UriQuery& self) {
            return std::string(self.toQueryString());
          })
      .def(
          "__repr__",
          [](const CesiumUtility::UriQuery& self) {
            return "UriQuery('" + std::string(self.toQueryString()) + "')";
          })
      .def_property_readonly(
          "query_string",
          [](const CesiumUtility::UriQuery& self) {
            return std::string(self.toQueryString());
          })
      .def(
          "get",
          [](CesiumUtility::UriQuery& self,
             const std::string& key,
             py::object defaultValue) -> py::object {
            auto maybeValue = self.getValue(key);
            if (!maybeValue) {
              return defaultValue;
            }
            return py::str(std::string(*maybeValue));
          },
          py::arg("key"),
          py::arg("default") = py::none())
      .def("__contains__", &CesiumUtility::UriQuery::hasValue)
      .def(
          "__getitem__",
          [](CesiumUtility::UriQuery& self,
             const std::string& key) -> py::object {
            auto maybeValue = self.getValue(key);
            if (!maybeValue) {
              throw py::key_error(key);
            }
            return py::str(std::string(*maybeValue));
          })
      .def(
          "__setitem__",
          [](CesiumUtility::UriQuery& self,
             const std::string& key,
             const std::string& value) { self.setValue(key, value); });

  // ── CesiumUtility::Math ──────────────────────────────────────────────────
  py::class_<CesiumUtility::Math>(m, "Math")
      // Epsilon constants
      .def_property_readonly_static(
          "EPSILON1",
          [](py::object) { return CesiumUtility::Math::Epsilon1; })
      .def_property_readonly_static(
          "EPSILON2",
          [](py::object) { return CesiumUtility::Math::Epsilon2; })
      .def_property_readonly_static(
          "EPSILON3",
          [](py::object) { return CesiumUtility::Math::Epsilon3; })
      .def_property_readonly_static(
          "EPSILON4",
          [](py::object) { return CesiumUtility::Math::Epsilon4; })
      .def_property_readonly_static(
          "EPSILON5",
          [](py::object) { return CesiumUtility::Math::Epsilon5; })
      .def_property_readonly_static(
          "EPSILON6",
          [](py::object) { return CesiumUtility::Math::Epsilon6; })
      .def_property_readonly_static(
          "EPSILON7",
          [](py::object) { return CesiumUtility::Math::Epsilon7; })
      .def_property_readonly_static(
          "EPSILON8",
          [](py::object) { return CesiumUtility::Math::Epsilon8; })
      .def_property_readonly_static(
          "EPSILON9",
          [](py::object) { return CesiumUtility::Math::Epsilon9; })
      .def_property_readonly_static(
          "EPSILON10",
          [](py::object) { return CesiumUtility::Math::Epsilon10; })
      .def_property_readonly_static(
          "EPSILON11",
          [](py::object) { return CesiumUtility::Math::Epsilon11; })
      .def_property_readonly_static(
          "EPSILON12",
          [](py::object) { return CesiumUtility::Math::Epsilon12; })
      .def_property_readonly_static(
          "EPSILON13",
          [](py::object) { return CesiumUtility::Math::Epsilon13; })
      .def_property_readonly_static(
          "EPSILON14",
          [](py::object) { return CesiumUtility::Math::Epsilon14; })
      .def_property_readonly_static(
          "EPSILON15",
          [](py::object) { return CesiumUtility::Math::Epsilon15; })
      .def_property_readonly_static(
          "EPSILON16",
          [](py::object) { return CesiumUtility::Math::Epsilon16; })
      .def_property_readonly_static(
          "EPSILON17",
          [](py::object) { return CesiumUtility::Math::Epsilon17; })
      .def_property_readonly_static(
          "EPSILON18",
          [](py::object) { return CesiumUtility::Math::Epsilon18; })
      .def_property_readonly_static(
          "EPSILON19",
          [](py::object) { return CesiumUtility::Math::Epsilon19; })
      .def_property_readonly_static(
          "EPSILON20",
          [](py::object) { return CesiumUtility::Math::Epsilon20; })
      .def_property_readonly_static(
          "EPSILON21",
          [](py::object) { return CesiumUtility::Math::Epsilon21; })
      // Pi / ratio constants
      .def_property_readonly_static(
          "ONE_PI",
          [](py::object) { return CesiumUtility::Math::OnePi; })
      .def_property_readonly_static(
          "TWO_PI",
          [](py::object) { return CesiumUtility::Math::TwoPi; })
      .def_property_readonly_static(
          "PI_OVER_TWO",
          [](py::object) { return CesiumUtility::Math::PiOverTwo; })
      .def_property_readonly_static(
          "PI_OVER_FOUR",
          [](py::object) { return CesiumUtility::Math::PiOverFour; })
      .def_property_readonly_static(
          "GOLDEN_RATIO",
          [](py::object) { return CesiumUtility::Math::GoldenRatio; })
      // Static methods (scalar overloads only)
      .def_static(
          "equals_epsilon",
          [](double left,
             double right,
             double relativeEpsilon,
             double absoluteEpsilon) {
            return CesiumUtility::Math::equalsEpsilon(
                left,
                right,
                relativeEpsilon,
                absoluteEpsilon);
          },
          py::arg("left"),
          py::arg("right"),
          py::arg("relative_epsilon"),
          py::arg("absolute_epsilon"))
      .def_static("sign", &CesiumUtility::Math::sign, py::arg("value"))
      .def_static(
          "sign_not_zero",
          &CesiumUtility::Math::signNotZero,
          py::arg("value"))
      .def_static(
          "negative_pi_to_pi",
          &CesiumUtility::Math::negativePiToPi,
          py::arg("angle"))
      .def_static(
          "zero_to_two_pi",
          &CesiumUtility::Math::zeroToTwoPi,
          py::arg("angle"))
      .def_static("mod", &CesiumUtility::Math::mod, py::arg("m"), py::arg("n"))
      .def_static(
          "degrees_to_radians",
          &CesiumUtility::Math::degreesToRadians,
          py::arg("angle_degrees"))
      .def_static(
          "radians_to_degrees",
          &CesiumUtility::Math::radiansToDegrees,
          py::arg("angle_radians"))
      .def_static(
          "lerp",
          &CesiumUtility::Math::lerp,
          py::arg("p"),
          py::arg("q"),
          py::arg("time"))
      .def_static(
          "clamp",
          &CesiumUtility::Math::clamp,
          py::arg("value"),
          py::arg("min"),
          py::arg("max"))
      .def_static(
          "to_snorm",
          &CesiumUtility::Math::toSNorm,
          py::arg("value"),
          py::arg("range_maximum") = 255.0)
      .def_static(
          "from_snorm",
          &CesiumUtility::Math::fromSNorm,
          py::arg("value"),
          py::arg("range_maximum") = 255.0)
      .def_static(
          "convert_longitude_range",
          &CesiumUtility::Math::convertLongitudeRange,
          py::arg("angle"))
      .def_static(
          "round_up",
          &CesiumUtility::Math::roundUp,
          py::arg("value"),
          py::arg("tolerance"))
      .def_static(
          "round_down",
          &CesiumUtility::Math::roundDown,
          py::arg("value"),
          py::arg("tolerance"));

  // ── Gzip ─────────────────────────────────────────────────────────────────
  m.def(
      "is_gzip",
      [](py::bytes data) {
        auto sv = static_cast<std::string_view>(data);
        std::span<const std::byte> span(
            reinterpret_cast<const std::byte*>(sv.data()),
            sv.size());
        return CesiumUtility::isGzip(span);
      },
      py::arg("data"),
      "Check whether data is gzipped.");

  m.def(
      "gzip",
      [](py::bytes data) -> py::bytes {
        auto sv = static_cast<std::string_view>(data);
        std::span<const std::byte> span(
            reinterpret_cast<const std::byte*>(sv.data()),
            sv.size());
        std::vector<std::byte> out;
        if (!CesiumUtility::gzip(span, out)) {
          throw std::runtime_error("gzip compression failed");
        }
        return py::bytes(reinterpret_cast<const char*>(out.data()), out.size());
      },
      py::arg("data"),
      "Gzip compress data.");

  m.def(
      "gunzip",
      [](py::bytes data) -> py::bytes {
        auto sv = static_cast<std::string_view>(data);
        std::span<const std::byte> span(
            reinterpret_cast<const std::byte*>(sv.data()),
            sv.size());
        std::vector<std::byte> out;
        if (!CesiumUtility::gunzip(span, out)) {
          throw std::runtime_error("gunzip decompression failed");
        }
        return py::bytes(reinterpret_cast<const char*>(out.data()), out.size());
      },
      py::arg("data"),
      "Gunzip decompress data.");

  // ── Color ─────────────────────────────────────────────────────────────────
  py::class_<CesiumUtility::Color>(m, "Color")
      .def(
          py::init<uint8_t, uint8_t, uint8_t, uint8_t>(),
          py::arg("r"),
          py::arg("g"),
          py::arg("b"),
          py::arg("a") = 0xff)
      .def_readwrite("r", &CesiumUtility::Color::r)
      .def_readwrite("g", &CesiumUtility::Color::g)
      .def_readwrite("b", &CesiumUtility::Color::b)
      .def_readwrite("a", &CesiumUtility::Color::a)
      .def("to_rgba32", &CesiumUtility::Color::toRgba32)
      .def(
          "__repr__",
          [](const CesiumUtility::Color& self) {
            return "Color(r=" + std::to_string(self.r) +
                   ", g=" + std::to_string(self.g) +
                   ", b=" + std::to_string(self.b) +
                   ", a=" + std::to_string(self.a) + ")";
          })
      .def(
          "__eq__",
          [](const CesiumUtility::Color& a, const CesiumUtility::Color& b) {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
          },
          py::is_operator())
      .def(
          "__ne__",
          [](const CesiumUtility::Color& a, const CesiumUtility::Color& b) {
            return a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
          },
          py::is_operator())
      .def("__hash__", [](const CesiumUtility::Color& self) {
        return std::hash<uint32_t>{}(self.toRgba32());
      });

  // ── Credit types ──────────────────────────────────────────────────────────
  py::enum_<CesiumUtility::CreditFilteringMode>(m, "CreditFilteringMode")
      .value("NONE", CesiumUtility::CreditFilteringMode::None)
      .value(
          "UNIQUE_HTML_AND_SHOW_ON_SCREEN",
          CesiumUtility::CreditFilteringMode::UniqueHtmlAndShowOnScreen)
      .value("UNIQUE_HTML", CesiumUtility::CreditFilteringMode::UniqueHtml);

  py::class_<CesiumUtility::Credit>(m, "Credit")
      .def("__eq__", &CesiumUtility::Credit::operator==, py::is_operator());

  py::class_<CesiumUtility::CreditsSnapshot>(m, "CreditsSnapshot")
      .def_readonly(
          "current_credits",
          &CesiumUtility::CreditsSnapshot::currentCredits)
      .def_readonly(
          "removed_credits",
          &CesiumUtility::CreditsSnapshot::removedCredits);

  py::class_<
      CesiumUtility::CreditSystem,
      std::shared_ptr<CesiumUtility::CreditSystem>>(m, "CreditSystem")
      .def(py::init<>())
      .def(
          "should_be_shown_on_screen",
          &CesiumUtility::CreditSystem::shouldBeShownOnScreen,
          py::arg("credit"))
      .def(
          "set_show_on_screen",
          &CesiumUtility::CreditSystem::setShowOnScreen,
          py::arg("credit"),
          py::arg("show_on_screen"))
      .def(
          "html",
          &CesiumUtility::CreditSystem::getHtml,
          py::arg("credit"),
          py::return_value_policy::reference_internal)
      .def(
          "create_credit",
          [](CesiumUtility::CreditSystem& self,
             CesiumUtility::CreditSource& source,
             const std::string& html,
             bool showOnScreen) {
            return self.createCredit(source, html, showOnScreen);
          },
          py::arg("source"),
          py::arg("html"),
          py::arg("show_on_screen") = false,
          "Create a credit associated with a source.")
      .def(
          "credit_source",
          [](const CesiumUtility::CreditSystem& self,
             CesiumUtility::Credit credit) -> py::object {
            const auto* p = self.getCreditSource(credit);
            if (!p)
              return py::none();
            return py::cast(p, py::return_value_policy::reference);
          },
          py::arg("credit"),
          "Get the source of a credit, or None if invalid.")
      .def(
          "add_credit_reference",
          &CesiumUtility::CreditSystem::addCreditReference,
          py::arg("credit"))
      .def(
          "remove_credit_reference",
          &CesiumUtility::CreditSystem::removeCreditReference,
          py::arg("credit"))
      .def(
          "snapshot",
          &CesiumUtility::CreditSystem::getSnapshot,
          py::arg("filtering_mode") =
              CesiumUtility::CreditFilteringMode::UniqueHtml,
          py::return_value_policy::reference_internal,
          "Get a snapshot of active and removed credits.")
      .def_property_readonly(
          "default_credit_source",
          &CesiumUtility::CreditSystem::getDefaultCreditSource,
          py::return_value_policy::reference_internal,
          "The default CreditSource used when none is specified.");

  py::class_<CesiumUtility::CreditSource>(m, "CreditSource")
      .def(py::init<CesiumUtility::CreditSystem&>(), py::arg("credit_system"))
      .def(
          py::init<const std::shared_ptr<CesiumUtility::CreditSystem>&>(),
          py::arg("credit_system"))
      .def_property_readonly(
          "credit_system",
          [](CesiumUtility::CreditSource& self) -> py::object {
            CesiumUtility::CreditSystem* p = self.getCreditSystem();
            if (!p)
              return py::none();
            return py::cast(p, py::return_value_policy::reference);
          });

  py::class_<CesiumUtility::CreditReferencer>(m, "CreditReferencer")
      .def(py::init<>())
      .def(
          py::init<const std::shared_ptr<CesiumUtility::CreditSystem>&>(),
          py::arg("credit_system"))
      .def_property_readonly(
          "credit_system",
          &CesiumUtility::CreditReferencer::getCreditSystem)
      .def(
          "set_credit_system",
          &CesiumUtility::CreditReferencer::setCreditSystem,
          py::arg("credit_system"))
      .def(
          "add_credit_reference",
          &CesiumUtility::CreditReferencer::addCreditReference,
          py::arg("credit"))
      .def(
          "release_all_references",
          &CesiumUtility::CreditReferencer::releaseAllReferences)
      .def(
          "is_credit_referenced",
          &CesiumUtility::CreditReferencer::isCreditReferenced,
          py::arg("credit"));

  // ── AttributeCompression ─────────────────────────────────────────────────
  py::class_<CesiumUtility::AttributeCompression>(m, "AttributeCompression")
      .def_static(
          "oct_decode",
          &CesiumUtility::AttributeCompression::octDecode,
          py::arg("x"),
          py::arg("y"),
          "Decode a 2-byte oct-encoded unit vector to a normalized dvec3.")
      .def_static(
          "decode_rgb565",
          &CesiumUtility::AttributeCompression::decodeRGB565,
          py::arg("value"),
          "Decode an RGB565-encoded color to normalized (r,g,b).");
}
