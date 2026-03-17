#pragma once

/**
 * @brief Conversions between CesiumUtility::JsonValue and Python objects.
 */

#include <CesiumUtility/JsonValue.h>

#include <pybind11/pybind11.h>

#include <cstdint>
#include <string>

namespace CesiumPython {

namespace py = pybind11;

/**
 * @brief Convert a CesiumUtility::JsonValue to a native Python object.
 */
inline py::object jsonValueToPy(const CesiumUtility::JsonValue& value) {
  if (value.isNull()) {
    return py::none();
  }
  if (value.isBool()) {
    return py::bool_(value.getBool());
  }
  if (value.isString()) {
    return py::str(value.getString());
  }
  if (value.isInt64()) {
    return py::int_(value.getInt64());
  }
  if (value.isUint64()) {
    return py::int_(value.getUint64());
  }
  if (value.isDouble()) {
    return py::float_(value.getDouble());
  }
  if (value.isArray()) {
    py::list out;
    for (const auto& item : value.getArray()) {
      out.append(jsonValueToPy(item));
    }
    return out;
  }
  if (value.isObject()) {
    py::dict out;
    for (const auto& kv : value.getObject()) {
      out[py::str(kv.first)] = jsonValueToPy(kv.second);
    }
    return out;
  }
  return py::none();
}

/**
 * @brief Convert a native Python object to a CesiumUtility::JsonValue.
 */
inline CesiumUtility::JsonValue pyToJsonValue(const py::handle& value) {
  if (value.is_none()) {
    return CesiumUtility::JsonValue(nullptr);
  }
  if (py::isinstance<py::bool_>(value)) {
    return CesiumUtility::JsonValue(py::cast<bool>(value));
  }
  if (py::isinstance<py::int_>(value)) {
    return CesiumUtility::JsonValue(py::cast<std::int64_t>(value));
  }
  if (py::isinstance<py::float_>(value)) {
    return CesiumUtility::JsonValue(py::cast<double>(value));
  }
  if (py::isinstance<py::str>(value)) {
    return CesiumUtility::JsonValue(py::cast<std::string>(value));
  }
  if (py::isinstance<py::dict>(value)) {
    CesiumUtility::JsonValue::Object out;
    py::dict dictValue = py::reinterpret_borrow<py::dict>(value);
    for (auto item : dictValue) {
      out[py::cast<std::string>(item.first)] = pyToJsonValue(item.second);
    }
    return CesiumUtility::JsonValue(std::move(out));
  }
  if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value)) {
    CesiumUtility::JsonValue::Array out;
    py::sequence seq = py::reinterpret_borrow<py::sequence>(value);
    out.reserve(seq.size());
    for (auto item : seq) {
      out.emplace_back(pyToJsonValue(item));
    }
    return CesiumUtility::JsonValue(std::move(out));
  }

  throw py::type_error(
      "Unsupported Python value for CesiumUtility::JsonValue conversion");
}

/**
 * @brief Convert a JsonValue::Object to a Python dict.
 */
inline py::dict jsonObjectToPy(const CesiumUtility::JsonValue::Object& obj) {
  py::dict out;
  for (const auto& kv : obj) {
    out[py::str(kv.first)] = jsonValueToPy(CesiumUtility::JsonValue(kv.second));
  }
  return out;
}

/**
 * @brief Convert a Python dict to a JsonValue::Object.
 */
inline CesiumUtility::JsonValue::Object pyToJsonObject(const py::dict& d) {
  CesiumUtility::JsonValue::Object out;
  for (auto item : d) {
    out[py::cast<std::string>(item.first)] = pyToJsonValue(item.second);
  }
  return out;
}

} // namespace CesiumPython
