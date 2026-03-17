#include <CesiumCurl/CurlAssetAccessor.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void initCurlBindings(py::module& m) {
  py::class_<CesiumCurl::CurlAssetAccessorOptions>(
      m,
      "CurlAssetAccessorOptions")
      .def(py::init<>())
      .def_readwrite(
          "user_agent",
          &CesiumCurl::CurlAssetAccessorOptions::userAgent)
      .def_readwrite(
          "request_headers",
          &CesiumCurl::CurlAssetAccessorOptions::requestHeaders)
      .def_readwrite(
          "allow_directory_creation",
          &CesiumCurl::CurlAssetAccessorOptions::allowDirectoryCreation)
      .def_readwrite(
          "certificate_path",
          &CesiumCurl::CurlAssetAccessorOptions::certificatePath)
      .def_readwrite(
          "certificate_file",
          &CesiumCurl::CurlAssetAccessorOptions::certificateFile)
      .def_readwrite(
          "do_global_init",
          &CesiumCurl::CurlAssetAccessorOptions::doGlobalInit);

  py::class_<
      CesiumCurl::CurlAssetAccessor,
      CesiumAsync::IAssetAccessor,
      std::shared_ptr<CesiumCurl::CurlAssetAccessor>>(m, "CurlAssetAccessor")
      .def(
          py::init<const CesiumCurl::CurlAssetAccessorOptions&>(),
          py::arg("options") = CesiumCurl::CurlAssetAccessorOptions())
      .def_property_readonly(
          "options",
          &CesiumCurl::CurlAssetAccessor::getOptions,
          py::return_value_policy::reference_internal);
}
