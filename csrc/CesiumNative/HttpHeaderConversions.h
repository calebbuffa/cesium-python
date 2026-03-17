#pragma once

#include <CesiumAsync/HttpHeaders.h>
#include <CesiumAsync/IAssetAccessor.h>

#include <pybind11/pybind11.h>

#include <string>
#include <vector>

namespace CesiumPython {

namespace py = pybind11;

inline CesiumAsync::HttpHeaders dictToHeaders(const py::dict& headers) {
  CesiumAsync::HttpHeaders out;
  for (auto item : headers) {
    out[py::cast<std::string>(item.first)] = py::cast<std::string>(item.second);
  }
  return out;
}

inline py::dict headersToPyDict(const CesiumAsync::HttpHeaders& headers) {
  py::dict out;
  for (const auto& kv : headers) {
    out[py::str(kv.first)] = py::str(kv.second);
  }
  return out;
}

inline std::vector<CesiumAsync::IAssetAccessor::THeader>
dictToHeaderPairs(const py::dict& headers) {
  std::vector<CesiumAsync::IAssetAccessor::THeader> out;
  out.reserve(headers.size());
  for (auto item : headers) {
    out.emplace_back(
        py::cast<std::string>(item.first),
        py::cast<std::string>(item.second));
  }
  return out;
}

inline py::dict headerPairsToPyDict(
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers) {
  py::dict out;
  for (const auto& kv : headers) {
    out[py::str(kv.first)] = py::str(kv.second);
  }
  return out;
}

} // namespace CesiumPython
