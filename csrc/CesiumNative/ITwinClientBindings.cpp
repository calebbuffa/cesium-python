#include "FutureTemplates.h"
#include "ResponseTemplates.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumClientCommon/OAuth2PKCE.h>
#include <CesiumITwinClient/AuthenticationToken.h>
#include <CesiumITwinClient/CesiumCuratedContent.h>
#include <CesiumITwinClient/Connection.h>
#include <CesiumITwinClient/GeospatialFeatureCollection.h>
#include <CesiumITwinClient/IModel.h>
#include <CesiumITwinClient/IModelMeshExport.h>
#include <CesiumITwinClient/ITwin.h>
#include <CesiumITwinClient/ITwinRealityData.h>
#include <CesiumITwinClient/PagedList.h>
#include <CesiumITwinClient/Profile.h>
#include <CesiumUtility/Result.h>
#include <CesiumVectorData/GeoJsonObject.h>
#include <CesiumVectorData/GeoJsonObjectTypes.h>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

// PagedList specializations
using PagedListITwin = CesiumITwinClient::PagedList<CesiumITwinClient::ITwin>;
using PagedListIModel = CesiumITwinClient::PagedList<CesiumITwinClient::IModel>;
using PagedListMeshExport =
    CesiumITwinClient::PagedList<CesiumITwinClient::IModelMeshExport>;
using PagedListRealityData =
    CesiumITwinClient::PagedList<CesiumITwinClient::ITwinRealityData>;

// Result specializations
using ResultUserProfile = CesiumUtility::Result<CesiumITwinClient::UserProfile>;
using ResultPagedListITwin = CesiumUtility::Result<PagedListITwin>;
using ResultPagedListIModel = CesiumUtility::Result<PagedListIModel>;
using ResultPagedListMeshExport = CesiumUtility::Result<PagedListMeshExport>;
using ResultPagedListRealityData = CesiumUtility::Result<PagedListRealityData>;
using ResultCuratedContent = CesiumUtility::Result<
    std::vector<CesiumITwinClient::CesiumCuratedContentAsset>>;
using ResultGeoCollections = CesiumUtility::Result<
    std::vector<CesiumITwinClient::GeospatialFeatureCollection>>;
using PagedListGeoJsonFeature =
    CesiumITwinClient::PagedList<CesiumVectorData::GeoJsonFeature>;
using ResultPagedListGeoJsonFeature =
    CesiumUtility::Result<PagedListGeoJsonFeature>;
using ResultConnection = CesiumUtility::Result<CesiumITwinClient::Connection>;

// Future specializations
using FutureResultUserProfile = CesiumAsync::Future<ResultUserProfile>;
using FutureResultPagedListITwin = CesiumAsync::Future<ResultPagedListITwin>;
using FutureResultPagedListIModel = CesiumAsync::Future<ResultPagedListIModel>;
using FutureResultPagedListMeshExport =
    CesiumAsync::Future<ResultPagedListMeshExport>;
using FutureResultPagedListRealityData =
    CesiumAsync::Future<ResultPagedListRealityData>;
using FutureResultCuratedContent = CesiumAsync::Future<ResultCuratedContent>;
using FutureResultGeoCollections = CesiumAsync::Future<ResultGeoCollections>;
using FutureResultPagedListGeoJsonFeature =
    CesiumAsync::Future<ResultPagedListGeoJsonFeature>;
using FutureResultConnection = CesiumAsync::Future<ResultConnection>;

// Helper to bind PagedList<T> (without next/prev which may not compile for all
// T)
template <typename T>
auto bindPagedList(py::class_<CesiumITwinClient::PagedList<T>>& cls)
    -> py::class_<CesiumITwinClient::PagedList<T>>& {
  cls.def(
         "__getitem__",
         [](const CesiumITwinClient::PagedList<T>& self,
            size_t index) -> const T& {
           if (index >= self.size())
             throw py::index_error();
           return self[index];
         },
         py::return_value_policy::reference_internal)
      .def("__len__", &CesiumITwinClient::PagedList<T>::size)
      .def(
          "__iter__",
          [](const CesiumITwinClient::PagedList<T>& self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>())
      .def("has_next_url", &CesiumITwinClient::PagedList<T>::hasNextUrl)
      .def("has_prev_url", &CesiumITwinClient::PagedList<T>::hasPrevUrl)
      .def(
          "__contains__",
          [](const CesiumITwinClient::PagedList<T>& self, const T& item) {
            for (size_t i = 0; i < self.size(); ++i) {
              if (self[i].id == item.id)
                return true;
            }
            return false;
          })
      .def("__repr__", [](const CesiumITwinClient::PagedList<T>& self) {
        return "PagedList(size=" + std::to_string(self.size()) + ")";
      });
  return cls;
}

} // namespace

void initITwinClientBindings(py::module& m) {
  py::enum_<CesiumITwinClient::ITwinStatus>(m, "ITwinStatus")
      .value("Unknown", CesiumITwinClient::ITwinStatus::Unknown)
      .value("Active", CesiumITwinClient::ITwinStatus::Active)
      .value("Inactive", CesiumITwinClient::ITwinStatus::Inactive)
      .value("Trial", CesiumITwinClient::ITwinStatus::Trial)
      .export_values();

  py::enum_<CesiumITwinClient::IModelState>(m, "IModelState")
      .value("Unknown", CesiumITwinClient::IModelState::Unknown)
      .value("Initialized", CesiumITwinClient::IModelState::Initialized)
      .value("NotInitialized", CesiumITwinClient::IModelState::NotInitialized)
      .export_values();

  py::enum_<CesiumITwinClient::IModelMeshExportStatus>(
      m,
      "IModelMeshExportStatus")
      .value("Unknown", CesiumITwinClient::IModelMeshExportStatus::Unknown)
      .value(
          "NotStarted",
          CesiumITwinClient::IModelMeshExportStatus::NotStarted)
      .value(
          "InProgress",
          CesiumITwinClient::IModelMeshExportStatus::InProgress)
      .value("Complete", CesiumITwinClient::IModelMeshExportStatus::Complete)
      .value("Invalid", CesiumITwinClient::IModelMeshExportStatus::Invalid)
      .export_values();

  py::enum_<CesiumITwinClient::IModelMeshExportType>(m, "IModelMeshExportType")
      .value("Unknown", CesiumITwinClient::IModelMeshExportType::Unknown)
      .value("ITwin3DFT", CesiumITwinClient::IModelMeshExportType::ITwin3DFT)
      .value("IModel", CesiumITwinClient::IModelMeshExportType::IModel)
      .value("Cesium", CesiumITwinClient::IModelMeshExportType::Cesium)
      .value(
          "Cesium3DTiles",
          CesiumITwinClient::IModelMeshExportType::Cesium3DTiles);

  py::enum_<CesiumITwinClient::ITwinRealityDataClassification>(
      m,
      "ITwinRealityDataClassification")
      .value(
          "Unknown",
          CesiumITwinClient::ITwinRealityDataClassification::Unknown)
      .value(
          "Terrain",
          CesiumITwinClient::ITwinRealityDataClassification::Terrain)
      .value(
          "Imagery",
          CesiumITwinClient::ITwinRealityDataClassification::Imagery)
      .value(
          "Pinned",
          CesiumITwinClient::ITwinRealityDataClassification::Pinned)
      .value("Model", CesiumITwinClient::ITwinRealityDataClassification::Model)
      .value(
          "Undefined",
          CesiumITwinClient::ITwinRealityDataClassification::Undefined)
      .export_values();

  py::enum_<CesiumITwinClient::CesiumCuratedContentType>(
      m,
      "CesiumCuratedContentType")
      .value("Unknown", CesiumITwinClient::CesiumCuratedContentType::Unknown)
      .value(
          "Cesium3DTiles",
          CesiumITwinClient::CesiumCuratedContentType::Cesium3DTiles)
      .value("Gltf", CesiumITwinClient::CesiumCuratedContentType::Gltf)
      .value("Imagery", CesiumITwinClient::CesiumCuratedContentType::Imagery)
      .value("Terrain", CesiumITwinClient::CesiumCuratedContentType::Terrain)
      .value("Kml", CesiumITwinClient::CesiumCuratedContentType::Kml)
      .value("Czml", CesiumITwinClient::CesiumCuratedContentType::Czml)
      .value("GeoJson", CesiumITwinClient::CesiumCuratedContentType::GeoJson)
      .export_values();

  py::enum_<CesiumITwinClient::CesiumCuratedContentStatus>(
      m,
      "CesiumCuratedContentStatus")
      .value("Unknown", CesiumITwinClient::CesiumCuratedContentStatus::Unknown)
      .value(
          "AwaitingFiles",
          CesiumITwinClient::CesiumCuratedContentStatus::AwaitingFiles)
      .value(
          "NotStarted",
          CesiumITwinClient::CesiumCuratedContentStatus::NotStarted)
      .value(
          "InProgress",
          CesiumITwinClient::CesiumCuratedContentStatus::InProgress)
      .value(
          "Complete",
          CesiumITwinClient::CesiumCuratedContentStatus::Complete)
      .value("Error", CesiumITwinClient::CesiumCuratedContentStatus::Error)
      .value(
          "DataError",
          CesiumITwinClient::CesiumCuratedContentStatus::DataError)
      .export_values();

  py::class_<CesiumITwinClient::UserProfile>(m, "UserProfile")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::UserProfile::id)
      .def_readwrite(
          "display_name",
          &CesiumITwinClient::UserProfile::displayName)
      .def_readwrite("given_name", &CesiumITwinClient::UserProfile::givenName)
      .def_readwrite("surname", &CesiumITwinClient::UserProfile::surname)
      .def_readwrite("email", &CesiumITwinClient::UserProfile::email);

  py::class_<CesiumITwinClient::ITwin>(m, "ITwin")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::ITwin::id)
      .def_readwrite("itwin_class", &CesiumITwinClient::ITwin::iTwinClass)
      .def_readwrite("sub_class", &CesiumITwinClient::ITwin::subClass)
      .def_readwrite("type", &CesiumITwinClient::ITwin::type)
      .def_readwrite("number", &CesiumITwinClient::ITwin::number)
      .def_readwrite("display_name", &CesiumITwinClient::ITwin::displayName)
      .def_readwrite("status", &CesiumITwinClient::ITwin::status)
      .def(
          "__repr__",
          [](const CesiumITwinClient::ITwin& self) {
            return "ITwin(id=" + self.id +
                   ", display_name=" + self.displayName + ")";
          })
      .def(
          "__eq__",
          [](const CesiumITwinClient::ITwin& a,
             const CesiumITwinClient::ITwin& b) { return a.id == b.id; },
          py::is_operator())
      .def(
          "__ne__",
          [](const CesiumITwinClient::ITwin& a,
             const CesiumITwinClient::ITwin& b) { return a.id != b.id; },
          py::is_operator())
      .def("__hash__", [](const CesiumITwinClient::ITwin& self) {
        return std::hash<std::string>{}(self.id);
      });

  py::class_<CesiumITwinClient::IModel>(m, "IModel")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::IModel::id)
      .def_readwrite("display_name", &CesiumITwinClient::IModel::displayName)
      .def_readwrite("name", &CesiumITwinClient::IModel::name)
      .def_readwrite("description", &CesiumITwinClient::IModel::description)
      .def_readwrite("state", &CesiumITwinClient::IModel::state)
      .def_readwrite("extent", &CesiumITwinClient::IModel::extent)
      .def(
          "__repr__",
          [](const CesiumITwinClient::IModel& self) {
            return "IModel(id=" + self.id +
                   ", display_name=" + self.displayName + ")";
          })
      .def(
          "__eq__",
          [](const CesiumITwinClient::IModel& a,
             const CesiumITwinClient::IModel& b) { return a.id == b.id; },
          py::is_operator())
      .def(
          "__ne__",
          [](const CesiumITwinClient::IModel& a,
             const CesiumITwinClient::IModel& b) { return a.id != b.id; },
          py::is_operator())
      .def("__hash__", [](const CesiumITwinClient::IModel& self) {
        return std::hash<std::string>{}(self.id);
      });

  py::class_<CesiumITwinClient::IModelMeshExport>(m, "IModelMeshExport")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::IModelMeshExport::id)
      .def_readwrite(
          "display_name",
          &CesiumITwinClient::IModelMeshExport::displayName)
      .def_readwrite("status", &CesiumITwinClient::IModelMeshExport::status)
      .def_readwrite(
          "export_type",
          &CesiumITwinClient::IModelMeshExport::exportType)
      .def(
          "__repr__",
          [](const CesiumITwinClient::IModelMeshExport& self) {
            return "IModelMeshExport(id=" + self.id +
                   ", display_name=" + self.displayName + ")";
          })
      .def(
          "__eq__",
          [](const CesiumITwinClient::IModelMeshExport& a,
             const CesiumITwinClient::IModelMeshExport& b) {
            return a.id == b.id;
          },
          py::is_operator())
      .def(
          "__ne__",
          [](const CesiumITwinClient::IModelMeshExport& a,
             const CesiumITwinClient::IModelMeshExport& b) {
            return a.id != b.id;
          },
          py::is_operator())
      .def("__hash__", [](const CesiumITwinClient::IModelMeshExport& self) {
        return std::hash<std::string>{}(self.id);
      });

  py::class_<CesiumITwinClient::ITwinRealityData>(m, "ITwinRealityData")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::ITwinRealityData::id)
      .def_readwrite(
          "display_name",
          &CesiumITwinClient::ITwinRealityData::displayName)
      .def_readwrite(
          "description",
          &CesiumITwinClient::ITwinRealityData::description)
      .def_readwrite(
          "classification",
          &CesiumITwinClient::ITwinRealityData::classification)
      .def_readwrite("type", &CesiumITwinClient::ITwinRealityData::type)
      .def_readwrite("extent", &CesiumITwinClient::ITwinRealityData::extent)
      .def_readwrite(
          "authoring",
          &CesiumITwinClient::ITwinRealityData::authoring)
      .def(
          "__repr__",
          [](const CesiumITwinClient::ITwinRealityData& self) {
            return "ITwinRealityData(id=" + self.id +
                   ", display_name=" + self.displayName + ")";
          })
      .def(
          "__eq__",
          [](const CesiumITwinClient::ITwinRealityData& a,
             const CesiumITwinClient::ITwinRealityData& b) {
            return a.id == b.id;
          },
          py::is_operator())
      .def(
          "__ne__",
          [](const CesiumITwinClient::ITwinRealityData& a,
             const CesiumITwinClient::ITwinRealityData& b) {
            return a.id != b.id;
          },
          py::is_operator())
      .def("__hash__", [](const CesiumITwinClient::ITwinRealityData& self) {
        return std::hash<std::string>{}(self.id);
      });

  py::class_<CesiumITwinClient::CesiumCuratedContentAsset>(
      m,
      "CesiumCuratedContentAsset")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::CesiumCuratedContentAsset::id)
      .def_readwrite(
          "type",
          &CesiumITwinClient::CesiumCuratedContentAsset::type)
      .def_readwrite(
          "name",
          &CesiumITwinClient::CesiumCuratedContentAsset::name)
      .def_readwrite(
          "description",
          &CesiumITwinClient::CesiumCuratedContentAsset::description)
      .def_readwrite(
          "attribution",
          &CesiumITwinClient::CesiumCuratedContentAsset::attribution)
      .def_readwrite(
          "status",
          &CesiumITwinClient::CesiumCuratedContentAsset::status);

  py::class_<CesiumITwinClient::GeospatialFeatureCollectionExtents>(
      m,
      "GeospatialFeatureCollectionExtents")
      .def(py::init<>())
      .def_readwrite(
          "spatial",
          &CesiumITwinClient::GeospatialFeatureCollectionExtents::spatial)
      .def_readwrite(
          "coordinate_reference_system",
          &CesiumITwinClient::GeospatialFeatureCollectionExtents::
              coordinateReferenceSystem)
      .def_readwrite(
          "temporal",
          &CesiumITwinClient::GeospatialFeatureCollectionExtents::temporal)
      .def_readwrite(
          "temporal_reference_system",
          &CesiumITwinClient::GeospatialFeatureCollectionExtents::
              temporalReferenceSystem);

  py::class_<CesiumITwinClient::GeospatialFeatureCollection>(
      m,
      "GeospatialFeatureCollection")
      .def(py::init<>())
      .def_readwrite("id", &CesiumITwinClient::GeospatialFeatureCollection::id)
      .def_readwrite(
          "title",
          &CesiumITwinClient::GeospatialFeatureCollection::title)
      .def_readwrite(
          "description",
          &CesiumITwinClient::GeospatialFeatureCollection::description)
      .def_readwrite(
          "extents",
          &CesiumITwinClient::GeospatialFeatureCollection::extents)
      .def_readwrite(
          "crs",
          &CesiumITwinClient::GeospatialFeatureCollection::crs)
      .def_readwrite(
          "storage_crs",
          &CesiumITwinClient::GeospatialFeatureCollection::storageCrs)
      .def_readwrite(
          "storage_crs_coordinate_epoch",
          &CesiumITwinClient::GeospatialFeatureCollection::
              storageCrsCoordinateEpoch);

  py::class_<CesiumITwinClient::QueryParameters>(m, "QueryParameters")
      .def(py::init<>())
      .def_readwrite("search", &CesiumITwinClient::QueryParameters::search)
      .def_readwrite("order_by", &CesiumITwinClient::QueryParameters::orderBy)
      .def_readwrite("top", &CesiumITwinClient::QueryParameters::top)
      .def_readwrite("skip", &CesiumITwinClient::QueryParameters::skip)
      .def("add_to_query", &CesiumITwinClient::QueryParameters::addToQuery)
      .def("add_to_uri", &CesiumITwinClient::QueryParameters::addToUri);

  py::class_<CesiumITwinClient::AuthenticationToken>(m, "AuthenticationToken")
      .def(
          py::init<
              const std::string&,
              std::string&&,
              std::string&&,
              std::vector<std::string>&&,
              int64_t,
              int64_t>(),
          py::arg("token"),
          py::arg("name"),
          py::arg("user_name"),
          py::arg("scopes"),
          py::arg("not_valid_before"),
          py::arg("expires"))
      .def(
          py::init<const std::string&, std::string&&, int64_t>(),
          py::arg("token"),
          py::arg("itwin_id"),
          py::arg("expires"))
      .def("is_valid", &CesiumITwinClient::AuthenticationToken::isValid)
      .def_property_readonly(
          "expiration_time",
          &CesiumITwinClient::AuthenticationToken::getExpirationTime)
      .def_property_readonly(
          "token",
          &CesiumITwinClient::AuthenticationToken::getToken)
      .def_property_readonly(
          "token_header",
          &CesiumITwinClient::AuthenticationToken::getTokenHeader)
      .def_static(
          "parse_summary",
          [](const std::string& token_str) {
            auto result =
                CesiumITwinClient::AuthenticationToken::parse(token_str);
            py::dict out;
            out["has_value"] = py::bool_(result.value.has_value());
            out["errors"] = py::cast(result.errors.errors);
            out["warnings"] = py::cast(result.errors.warnings);
            if (result.value) {
              out["token"] = py::cast(result.value->getToken());
              out["expiration_time"] =
                  py::cast(result.value->getExpirationTime());
            } else {
              out["token"] = py::none();
              out["expiration_time"] = py::none();
            }
            return out;
          })
      .def("__bool__", [](const CesiumITwinClient::AuthenticationToken& self) {
        return self.isValid();
      });

  // --- PagedList<T> specializations ---
  // NOTE: bind_paged_list_pagination (next/prev) is intentionally omitted for
  // ALL types.  cesium-native's PagedList<T>::next/prev contain
  // Result<PagedList<T>>(std::nullopt) which is ill-formed because Result has
  // no std::nullopt_t constructor.  MSVC eagerly instantiates these members
  // via std::is_polymorphic in pybind11, causing a hard compile error.
  // See upstream issue: CesiumGS/cesium-native PagedList.h
  auto plITwin = py::class_<PagedListITwin>(m, "PagedListITwin");
  bindPagedList(plITwin);

  auto plIModel = py::class_<PagedListIModel>(m, "PagedListIModel");
  bindPagedList(plIModel);

  auto plMeshExport = py::class_<PagedListMeshExport>(m, "PagedListMeshExport");
  bindPagedList(plMeshExport);

  auto plRealityData =
      py::class_<PagedListRealityData>(m, "PagedListRealityData");
  bindPagedList(plRealityData);

  auto plGeoFeatures =
      py::class_<PagedListGeoJsonFeature>(m, "PagedListGeoJsonFeature");
  bindPagedList(plGeoFeatures);

  // --- Result<T> specializations ---
  auto resUserProfile = py::class_<ResultUserProfile>(m, "ResultUserProfile");
  CesiumPython::bindResult(resUserProfile);

  auto resPlITwin = py::class_<ResultPagedListITwin>(m, "ResultPagedListITwin");
  CesiumPython::bindResult(resPlITwin);

  auto resPlIModel =
      py::class_<ResultPagedListIModel>(m, "ResultPagedListIModel");
  CesiumPython::bindResult(resPlIModel);

  auto resPlMeshExport =
      py::class_<ResultPagedListMeshExport>(m, "ResultPagedListMeshExport");
  CesiumPython::bindResult(resPlMeshExport);

  auto resPlRealityData =
      py::class_<ResultPagedListRealityData>(m, "ResultPagedListRealityData");
  CesiumPython::bindResult(resPlRealityData);

  auto resCuratedContent =
      py::class_<ResultCuratedContent>(m, "ResultCuratedContentList");
  CesiumPython::bindResult(resCuratedContent);

  auto resGeoCollections =
      py::class_<ResultGeoCollections>(m, "ResultGeoCollectionList");
  CesiumPython::bindResult(resGeoCollections);

  auto resPlGeoFeatures = py::class_<ResultPagedListGeoJsonFeature>(
      m,
      "ResultPagedListGeoJsonFeature");
  CesiumPython::bindResult(resPlGeoFeatures);

  auto resConn = py::class_<ResultConnection>(m, "ResultITwinConnection");
  CesiumPython::bindResult(resConn);

  // --- Future<Result<T>> specializations ---
  auto futResUserProfile =
      py::class_<FutureResultUserProfile>(m, "FutureResultUserProfile");
  CesiumPython::bindFuture(futResUserProfile);

  auto futResPlITwin =
      py::class_<FutureResultPagedListITwin>(m, "FutureResultPagedListITwin");
  CesiumPython::bindFuture(futResPlITwin);

  auto futResPlIModel =
      py::class_<FutureResultPagedListIModel>(m, "FutureResultPagedListIModel");
  CesiumPython::bindFuture(futResPlIModel);

  auto futResPlMeshExport = py::class_<FutureResultPagedListMeshExport>(
      m,
      "FutureResultPagedListMeshExport");
  CesiumPython::bindFuture(futResPlMeshExport);

  auto futResPlRealityData = py::class_<FutureResultPagedListRealityData>(
      m,
      "FutureResultPagedListRealityData");
  CesiumPython::bindFuture(futResPlRealityData);

  auto futResCuratedContent = py::class_<FutureResultCuratedContent>(
      m,
      "FutureResultCuratedContentList");
  CesiumPython::bindFuture(futResCuratedContent);

  auto futResGeoCollections = py::class_<FutureResultGeoCollections>(
      m,
      "FutureResultGeoCollectionList");
  CesiumPython::bindFuture(futResGeoCollections);

  auto futResPlGeoFeatures = py::class_<FutureResultPagedListGeoJsonFeature>(
      m,
      "FutureResultPagedListGeoJsonFeature");
  CesiumPython::bindFuture(futResPlGeoFeatures);

  auto futResConn =
      py::class_<FutureResultConnection>(m, "FutureResultITwinConnection");
  CesiumPython::bindFuture(futResConn);

  // --- Connection ---
  py::class_<CesiumITwinClient::Connection>(m, "ITwinConnection")
      .def(
          py::init<
              const CesiumAsync::AsyncSystem&,
              const std::shared_ptr<CesiumAsync::IAssetAccessor>&,
              const CesiumITwinClient::AuthenticationToken&,
              const std::optional<std::string>&,
              const CesiumClientCommon::OAuth2ClientOptions&>(),
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("authentication_token"),
          py::arg("refresh_token"),
          py::arg("client_options"))
      .def_property(
          "authentication_token",
          &CesiumITwinClient::Connection::getAuthenticationToken,
          &CesiumITwinClient::Connection::setAuthenticationToken)
      .def_property(
          "refresh_token",
          &CesiumITwinClient::Connection::getRefreshToken,
          &CesiumITwinClient::Connection::setRefreshToken)
      // --- Static methods ---
      .def_static(
          "authorize",
          &CesiumITwinClient::Connection::authorize,
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("friendly_application_name"),
          py::arg("client_id"),
          py::arg("redirect_path"),
          py::arg("redirect_port"),
          py::arg("scopes"),
          py::arg("open_url_callback"))
      // --- Instance async methods ---
      .def("me", &CesiumITwinClient::Connection::me)
      .def(
          "itwins",
          &CesiumITwinClient::Connection::itwins,
          py::arg("params") = CesiumITwinClient::QueryParameters{})
      .def(
          "imodels",
          &CesiumITwinClient::Connection::imodels,
          py::arg("itwin_id"),
          py::arg("params") = CesiumITwinClient::QueryParameters{})
      .def(
          "mesh_exports",
          &CesiumITwinClient::Connection::meshExports,
          py::arg("imodel_id"),
          py::arg("params") = CesiumITwinClient::QueryParameters{})
      .def(
          "reality_data",
          &CesiumITwinClient::Connection::realityData,
          py::arg("itwin_id"),
          py::arg("params") = CesiumITwinClient::QueryParameters{})
      .def(
          "cesium_curated_content",
          &CesiumITwinClient::Connection::cesiumCuratedContent)
      .def(
          "geospatial_feature_collections",
          &CesiumITwinClient::Connection::geospatialFeatureCollections,
          py::arg("itwin_id"))
      .def(
          "geospatial_features",
          &CesiumITwinClient::Connection::geospatialFeatures,
          py::arg("itwin_id"),
          py::arg("collection_id"),
          py::arg("limit") = 10000);

  m.def("itwin_status_from_string", &CesiumITwinClient::iTwinStatusFromString);
  m.def("imodel_state_from_string", &CesiumITwinClient::iModelStateFromString);
  m.def(
      "imodel_mesh_export_status_from_string",
      &CesiumITwinClient::iModelMeshExportStatusFromString);
  m.def(
      "imodel_mesh_export_type_from_string",
      &CesiumITwinClient::iModelMeshExportTypeFromString);
  m.def(
      "itwin_reality_data_classification_from_string",
      &CesiumITwinClient::iTwinRealityDataClassificationFromString);
  m.def(
      "cesium_curated_content_type_from_string",
      &CesiumITwinClient::cesiumCuratedContentTypeFromString);
  m.def(
      "cesium_curated_content_status_from_string",
      &CesiumITwinClient::cesiumCuratedContentStatusFromString);
}
