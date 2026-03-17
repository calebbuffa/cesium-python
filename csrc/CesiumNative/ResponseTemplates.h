#pragma once
/// Template helpers for binding CesiumIonClient::Response<T>,
/// CesiumUtility::Result<T>, and CesiumITwinClient::PagedList<T>.

#include <CesiumIonClient/Response.h>
#include <CesiumUtility/Result.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace CesiumPython {

// ---- Response<T> ----------------------------------------------------------

template <typename T>
auto bindResponse(py::class_<CesiumIonClient::Response<T>>& cls)
    -> py::class_<CesiumIonClient::Response<T>>& {
  cls.def(py::init<>())
      .def_readwrite("value", &CesiumIonClient::Response<T>::value)
      .def_readwrite(
          "http_status_code",
          &CesiumIonClient::Response<T>::httpStatusCode)
      .def_readwrite("error_code", &CesiumIonClient::Response<T>::errorCode)
      .def_readwrite(
          "error_message",
          &CesiumIonClient::Response<T>::errorMessage)
      .def_readwrite(
          "next_page_url",
          &CesiumIonClient::Response<T>::nextPageUrl)
      .def_readwrite(
          "previous_page_url",
          &CesiumIonClient::Response<T>::previousPageUrl)
      .def("__bool__", [](const CesiumIonClient::Response<T>& self) {
        return self.value.has_value();
      });
  return cls;
}

// ---- Result<T> ------------------------------------------------------------

template <typename T>
auto bindResult(py::class_<CesiumUtility::Result<T>>& cls)
    -> py::class_<CesiumUtility::Result<T>>& {
  cls.def_readwrite("value", &CesiumUtility::Result<T>::value)
      .def_readwrite("errors", &CesiumUtility::Result<T>::errors)
      .def("__bool__", [](const CesiumUtility::Result<T>& self) {
        return self.value.has_value();
      });
  return cls;
}

} // namespace CesiumPython
