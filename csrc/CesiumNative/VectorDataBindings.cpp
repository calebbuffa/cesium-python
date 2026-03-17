#include "FutureTemplates.h"
#include "JsonConversions.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumGeometry/AxisAlignedBox.h>
#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumUtility/Color.h>
#include <CesiumUtility/IntrusivePointer.h>
#include <CesiumUtility/Result.h>
#include <CesiumVectorData/GeoJsonDocument.h>
#include <CesiumVectorData/GeoJsonObject.h>
#include <CesiumVectorData/GeoJsonObjectTypes.h>
#include <CesiumVectorData/VectorRasterizer.h>
#include <CesiumVectorData/VectorStyle.h>

#include <glm/vec3.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstring>
#include <memory>
#include <variant>
#include <vector>

namespace py = pybind11;

namespace {

// Helper: Convert Feature id variant to Python (None | str | int)
py::object
featureIdToPy(const std::variant<std::monostate, std::string, int64_t>& id) {
  if (std::holds_alternative<std::monostate>(id))
    return py::none();
  if (std::holds_alternative<std::string>(id))
    return py::str(std::get<std::string>(id));
  return py::int_(std::get<int64_t>(id));
}

// Macro to bind the common fields shared by all geometry types.
#define BIND_GEOJSON_COMMON(cls, CppType)                                      \
  cls.def_property_readonly_static(                                            \
         "TYPE",                                                               \
         [](py::object) { return CppType::TYPE; })                             \
      .def_readwrite("bounding_box", &CppType::boundingBox)                    \
      .def_property(                                                           \
          "foreign_members",                                                   \
          [](const CppType& self) {                                            \
            return CesiumPython::jsonObjectToPy(self.foreignMembers);          \
          },                                                                   \
          [](CppType& self, const py::dict& d) {                               \
            self.foreignMembers = CesiumPython::pyToJsonObject(d);             \
          })                                                                   \
      .def_readwrite("style", &CppType::style)

} // namespace

void initVectorDataBindings(py::module& m) {
  py::enum_<CesiumVectorData::ColorMode>(m, "ColorMode")
      .value("NORMAL", CesiumVectorData::ColorMode::Normal)
      .value("RANDOM", CesiumVectorData::ColorMode::Random)
      .export_values();

  py::enum_<CesiumVectorData::LineWidthMode>(m, "LineWidthMode")
      .value("PIXELS", CesiumVectorData::LineWidthMode::Pixels)
      .value("METERS", CesiumVectorData::LineWidthMode::Meters)
      .export_values();

  py::class_<CesiumVectorData::ColorStyle>(m, "ColorStyle")
      .def(py::init<>())
      .def_readwrite("color", &CesiumVectorData::ColorStyle::color)
      .def_readwrite("color_mode", &CesiumVectorData::ColorStyle::colorMode)
      .def(
          "color_for_seed",
          &CesiumVectorData::ColorStyle::getColor,
          py::arg("random_color_seed") = 0);

  py::class_<CesiumVectorData::LineStyle, CesiumVectorData::ColorStyle>(
      m,
      "LineStyle")
      .def(py::init<>())
      .def_readwrite("width", &CesiumVectorData::LineStyle::width)
      .def_readwrite("width_mode", &CesiumVectorData::LineStyle::widthMode);

  py::class_<CesiumVectorData::PolygonStyle>(m, "PolygonStyle")
      .def(py::init<>())
      .def_readwrite("fill", &CesiumVectorData::PolygonStyle::fill)
      .def_readwrite("outline", &CesiumVectorData::PolygonStyle::outline);

  py::class_<CesiumVectorData::VectorStyle>(m, "VectorStyle")
      .def(py::init<>())
      .def(
          py::init<
              const CesiumVectorData::LineStyle&,
              const CesiumVectorData::PolygonStyle&>(),
          py::arg("line_style"),
          py::arg("polygon_style"))
      .def(py::init<const CesiumUtility::Color&>(), py::arg("color"))
      .def_readwrite("line", &CesiumVectorData::VectorStyle::line)
      .def_readwrite("polygon", &CesiumVectorData::VectorStyle::polygon);

  py::enum_<CesiumVectorData::GeoJsonObjectType>(m, "GeoJsonObjectType")
      .value("POINT", CesiumVectorData::GeoJsonObjectType::Point)
      .value("MULTI_POINT", CesiumVectorData::GeoJsonObjectType::MultiPoint)
      .value("LINE_STRING", CesiumVectorData::GeoJsonObjectType::LineString)
      .value(
          "MULTI_LINE_STRING",
          CesiumVectorData::GeoJsonObjectType::MultiLineString)
      .value("POLYGON", CesiumVectorData::GeoJsonObjectType::Polygon)
      .value("MULTI_POLYGON", CesiumVectorData::GeoJsonObjectType::MultiPolygon)
      .value(
          "GEOMETRY_COLLECTION",
          CesiumVectorData::GeoJsonObjectType::GeometryCollection)
      .value("FEATURE", CesiumVectorData::GeoJsonObjectType::Feature)
      .value(
          "FEATURE_COLLECTION",
          CesiumVectorData::GeoJsonObjectType::FeatureCollection)
      .export_values();

  m.def(
      "geojson_object_type_to_string",
      [](CesiumVectorData::GeoJsonObjectType type) {
        return std::string(CesiumVectorData::geoJsonObjectTypeToString(type));
      },
      py::arg("type"));

  // ── GeoJSON Geometry Types ──────────────────────────────────────────────

  auto pyPoint = py::class_<CesiumVectorData::GeoJsonPoint>(m, "GeoJsonPoint")
                     .def(py::init<>())
                     .def(
                         py::init([](const glm::dvec3& coords) {
                           CesiumVectorData::GeoJsonPoint p;
                           p.coordinates = coords;
                           return p;
                         }),
                         py::arg("coordinates"))
                     .def_readwrite(
                         "coordinates",
                         &CesiumVectorData::GeoJsonPoint::coordinates);
  BIND_GEOJSON_COMMON(pyPoint, CesiumVectorData::GeoJsonPoint);

  auto pyMultiPoint =
      py::class_<CesiumVectorData::GeoJsonMultiPoint>(m, "GeoJsonMultiPoint")
          .def(py::init<>())
          .def_readwrite(
              "coordinates",
              &CesiumVectorData::GeoJsonMultiPoint::coordinates);
  BIND_GEOJSON_COMMON(pyMultiPoint, CesiumVectorData::GeoJsonMultiPoint);

  auto pyLineString =
      py::class_<CesiumVectorData::GeoJsonLineString>(m, "GeoJsonLineString")
          .def(py::init<>())
          .def_readwrite(
              "coordinates",
              &CesiumVectorData::GeoJsonLineString::coordinates);
  BIND_GEOJSON_COMMON(pyLineString, CesiumVectorData::GeoJsonLineString);

  auto pyMultiLineString =
      py::class_<CesiumVectorData::GeoJsonMultiLineString>(
          m,
          "GeoJsonMultiLineString")
          .def(py::init<>())
          .def_readwrite(
              "coordinates",
              &CesiumVectorData::GeoJsonMultiLineString::coordinates);
  BIND_GEOJSON_COMMON(
      pyMultiLineString,
      CesiumVectorData::GeoJsonMultiLineString);

  auto pyPolygon =
      py::class_<CesiumVectorData::GeoJsonPolygon>(m, "GeoJsonPolygon")
          .def(py::init<>())
          .def_readwrite(
              "coordinates",
              &CesiumVectorData::GeoJsonPolygon::coordinates);
  BIND_GEOJSON_COMMON(pyPolygon, CesiumVectorData::GeoJsonPolygon);

  auto pyMultiPolygon =
      py::class_<CesiumVectorData::GeoJsonMultiPolygon>(
          m,
          "GeoJsonMultiPolygon")
          .def(py::init<>())
          .def_readwrite(
              "coordinates",
              &CesiumVectorData::GeoJsonMultiPolygon::coordinates);
  BIND_GEOJSON_COMMON(pyMultiPolygon, CesiumVectorData::GeoJsonMultiPolygon);

  // ── GeoJsonObject (variant wrapper) ─────────────────────────────────────

  py::class_<CesiumVectorData::GeoJsonObject>(m, "GeoJsonObject")
      .def(
          py::init([](const CesiumVectorData::GeoJsonPoint& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("point"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonMultiPoint& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("multi_point"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonLineString& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("line_string"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonMultiLineString& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("multi_line_string"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonPolygon& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("polygon"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonMultiPolygon& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("multi_polygon"))
      .def(
          py::init([](const CesiumVectorData::GeoJsonGeometryCollection& v) {
            return CesiumVectorData::GeoJsonObject{v};
          }),
          py::arg("geometry_collection"))
      .def(
          py::init([](CesiumVectorData::GeoJsonFeature v) {
            return CesiumVectorData::GeoJsonObject{std::move(v)};
          }),
          py::arg("feature"))
      .def(
          py::init([](CesiumVectorData::GeoJsonFeatureCollection v) {
            return CesiumVectorData::GeoJsonObject{std::move(v)};
          }),
          py::arg("feature_collection"))
      .def_property_readonly("type", &CesiumVectorData::GeoJsonObject::getType)
      .def_property(
          "bounding_box",
          [](const CesiumVectorData::GeoJsonObject& self)
              -> const std::optional<CesiumGeometry::AxisAlignedBox>& {
            return self.getBoundingBox();
          },
          [](CesiumVectorData::GeoJsonObject& self,
             const std::optional<CesiumGeometry::AxisAlignedBox>& box) {
            self.getBoundingBox() = box;
          })
      .def_property(
          "foreign_members",
          [](const CesiumVectorData::GeoJsonObject& self) {
            return CesiumPython::jsonObjectToPy(self.getForeignMembers());
          },
          [](CesiumVectorData::GeoJsonObject& self, const py::dict& d) {
            self.getForeignMembers() = CesiumPython::pyToJsonObject(d);
          })
      .def_property(
          "style",
          [](const CesiumVectorData::GeoJsonObject& self)
              -> const std::optional<CesiumVectorData::VectorStyle>& {
            return self.getStyle();
          },
          [](CesiumVectorData::GeoJsonObject& self,
             const std::optional<CesiumVectorData::VectorStyle>& s) {
            self.getStyle() = s;
          })
      // Type checking
      .def(
          "is_point",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonPoint>)
      .def(
          "is_multi_point",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonMultiPoint>)
      .def(
          "is_line_string",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonLineString>)
      .def(
          "is_multi_line_string",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonMultiLineString>)
      .def(
          "is_polygon",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonPolygon>)
      .def(
          "is_multi_polygon",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonMultiPolygon>)
      .def(
          "is_geometry_collection",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonGeometryCollection>)
      .def(
          "is_feature",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonFeature>)
      .def(
          "is_feature_collection",
          &CesiumVectorData::GeoJsonObject::isType<
              CesiumVectorData::GeoJsonFeatureCollection>)
      // Typed accessors (return None on mismatch)
      .def(
          "as_point",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonPoint>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_multi_point",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonMultiPoint>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_line_string",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonLineString>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_multi_line_string",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonMultiLineString>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_polygon",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonPolygon>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_multi_polygon",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonMultiPolygon>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_geometry_collection",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonGeometryCollection>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_feature",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonFeature>();
            return p ? py::cast(*p) : py::none();
          })
      .def(
          "as_feature_collection",
          [](const CesiumVectorData::GeoJsonObject& self) -> py::object {
            auto* p = self.getIf<CesiumVectorData::GeoJsonFeatureCollection>();
            return p ? py::cast(*p) : py::none();
          })
      // Primitive iterators — materialise as Python lists for simplicity
      .def(
          "points",
          [](const CesiumVectorData::GeoJsonObject& self) {
            py::list out;
            for (const auto& pt : self.points())
              out.append(pt);
            return out;
          })
      .def(
          "lines",
          [](const CesiumVectorData::GeoJsonObject& self) {
            py::list out;
            for (const auto& ln : self.lines())
              out.append(ln);
            return out;
          })
      .def(
          "polygons",
          [](const CesiumVectorData::GeoJsonObject& self) {
            py::list out;
            for (const auto& pg : self.polygons())
              out.append(pg);
            return out;
          })
      // Deep iteration — yields child GeoJsonObjects
      .def(
          "__iter__",
          [](const CesiumVectorData::GeoJsonObject& self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>())
      .def("__repr__", [](const CesiumVectorData::GeoJsonObject& self) {
        return "GeoJsonObject(type=" +
               std::string(
                   CesiumVectorData::geoJsonObjectTypeToString(
                       self.getType())) +
               ")";
      });

  // ── GeometryCollection (needs GeoJsonObject defined first) ──────────────

  auto pyGeomCollection =
      py::class_<CesiumVectorData::GeoJsonGeometryCollection>(
          m,
          "GeoJsonGeometryCollection")
          .def(py::init<>())
          .def_readwrite(
              "geometries",
              &CesiumVectorData::GeoJsonGeometryCollection::geometries);
  BIND_GEOJSON_COMMON(
      pyGeomCollection,
      CesiumVectorData::GeoJsonGeometryCollection);

  // ── GeoJsonFeature ──────────────────────────────────────────────────────

  auto pyFeature =
      py::class_<CesiumVectorData::GeoJsonFeature>(m, "GeoJsonFeature")
          .def(py::init<>())
          .def_property(
              "id",
              [](const CesiumVectorData::GeoJsonFeature& self) {
                return featureIdToPy(self.id);
              },
              [](CesiumVectorData::GeoJsonFeature& self,
                 const py::object& val) {
                if (val.is_none())
                  self.id = std::monostate{};
                else if (py::isinstance<py::str>(val))
                  self.id = py::cast<std::string>(val);
                else
                  self.id = py::cast<int64_t>(val);
              })
          .def_property(
              "geometry",
              [](const CesiumVectorData::GeoJsonFeature& self) -> py::object {
                if (self.geometry)
                  return py::cast(*self.geometry);
                return py::none();
              },
              [](CesiumVectorData::GeoJsonFeature& self,
                 const py::object& val) {
                if (val.is_none())
                  self.geometry.reset();
                else
                  self.geometry =
                      std::make_unique<CesiumVectorData::GeoJsonObject>(
                          py::cast<CesiumVectorData::GeoJsonObject>(val));
              })
          .def_property(
              "properties",
              [](const CesiumVectorData::GeoJsonFeature& self) -> py::object {
                if (self.properties)
                  return CesiumPython::jsonObjectToPy(*self.properties);
                return py::none();
              },
              [](CesiumVectorData::GeoJsonFeature& self,
                 const py::object& val) {
                if (val.is_none())
                  self.properties.reset();
                else
                  self.properties =
                      CesiumPython::pyToJsonObject(py::cast<py::dict>(val));
              });
  BIND_GEOJSON_COMMON(pyFeature, CesiumVectorData::GeoJsonFeature);

  // ── GeoJsonFeatureCollection ────────────────────────────────────────────

  auto pyFeatureCollection =
      py::class_<CesiumVectorData::GeoJsonFeatureCollection>(
          m,
          "GeoJsonFeatureCollection")
          .def(py::init<>())
          .def_readwrite(
              "features",
              &CesiumVectorData::GeoJsonFeatureCollection::features);
  BIND_GEOJSON_COMMON(
      pyFeatureCollection,
      CesiumVectorData::GeoJsonFeatureCollection);

  // ── FutureResultGeoJsonDocument ─────────────────────────────────────────

  using ResultGeoJsonDocument =
      CesiumUtility::Result<CesiumVectorData::GeoJsonDocument>;
  using FutureResultGeoJsonDocument =
      CesiumAsync::Future<ResultGeoJsonDocument>;

  py::class_<ResultGeoJsonDocument>(m, "ResultGeoJsonDocument")
      .def_property_readonly(
          "value",
          [](const ResultGeoJsonDocument& self) -> py::object {
            if (self.value)
              return py::cast(
                  std::make_shared<CesiumVectorData::GeoJsonDocument>(std::move(
                      const_cast<ResultGeoJsonDocument&>(self).value.value())));
            return py::none();
          })
      .def_property_readonly(
          "errors",
          [](const ResultGeoJsonDocument& self) {
            py::list out;
            for (const auto& e : self.errors.errors)
              out.append(e);
            return out;
          })
      .def_property_readonly("warnings", [](const ResultGeoJsonDocument& self) {
        py::list out;
        for (const auto& w : self.errors.warnings)
          out.append(w);
        return out;
      });

  auto futResultGeoJsonDoc =
      py::class_<FutureResultGeoJsonDocument>(m, "FutureResultGeoJsonDocument");
  CesiumPython::bindFuture(futResultGeoJsonDoc);

  py::class_<CesiumVectorData::VectorDocumentAttribution>(
      m,
      "VectorDocumentAttribution")
      .def(py::init<>())
      .def_readwrite("html", &CesiumVectorData::VectorDocumentAttribution::html)
      .def_readwrite(
          "show_on_screen",
          &CesiumVectorData::VectorDocumentAttribution::showOnScreen);

  m.def(
      "parse_geojson_document_summary",
      [](py::bytes geojsonBytes,
         std::vector<CesiumVectorData::VectorDocumentAttribution>
             attributions) {
        std::string bytes = geojsonBytes;
        std::vector<std::byte> byteBuffer(bytes.size());
        for (size_t i = 0; i < bytes.size(); ++i) {
          byteBuffer[i] =
              static_cast<std::byte>(static_cast<unsigned char>(bytes[i]));
        }

        auto result = CesiumVectorData::GeoJsonDocument::fromGeoJson(
            std::span<const std::byte>(byteBuffer.data(), byteBuffer.size()),
            std::move(attributions));

        py::dict out;
        out["has_value"] = result.value.has_value();

        py::list errors;
        for (const auto& error : result.errors.errors) {
          errors.append(error);
        }
        out["errors"] = errors;

        py::list warnings;
        for (const auto& warning : result.errors.warnings) {
          warnings.append(warning);
        }
        out["warnings"] = warnings;

        if (result.value) {
          out["root_type"] = std::string(
              CesiumVectorData::geoJsonObjectTypeToString(
                  result.value->rootObject.getType()));
          out["attribution_count"] = result.value->attributions.size();
        } else {
          out["root_type"] = py::none();
          out["attribution_count"] = 0;
        }

        return out;
      },
      py::arg("geojson_bytes"),
      py::arg("attributions") =
          std::vector<CesiumVectorData::VectorDocumentAttribution>());

  // GeoJsonDocument — held by shared_ptr so it can be passed to
  // GeoJsonDocumentRasterOverlay
  py::class_<
      CesiumVectorData::GeoJsonDocument,
      std::shared_ptr<CesiumVectorData::GeoJsonDocument>>(m, "GeoJsonDocument")
      .def_static(
          "from_geojson",
          [](py::bytes data,
             std::vector<CesiumVectorData::VectorDocumentAttribution>
                 attributions) {
            std::string raw = data;
            std::vector<std::byte> buf(raw.size());
            std::memcpy(buf.data(), raw.data(), raw.size());
            auto result = CesiumVectorData::GeoJsonDocument::fromGeoJson(
                std::span<const std::byte>(buf.data(), buf.size()),
                std::move(attributions));
            if (!result.value) {
              std::string msg = "Failed to parse GeoJSON";
              if (!result.errors.errors.empty())
                msg += ": " + result.errors.errors[0];
              throw std::runtime_error(msg);
            }
            return std::make_shared<CesiumVectorData::GeoJsonDocument>(
                std::move(*result.value));
          },
          py::arg("data"),
          py::arg("attributions") =
              std::vector<CesiumVectorData::VectorDocumentAttribution>{},
          "Parse a GeoJSON document from bytes.")
      .def_static(
          "from_url",
          [](const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             const std::string& url,
             const py::dict& headers) {
            std::vector<CesiumAsync::IAssetAccessor::THeader> headerPairs;
            for (auto item : headers) {
              headerPairs.emplace_back(
                  py::cast<std::string>(item.first),
                  py::cast<std::string>(item.second));
            }
            return CesiumVectorData::GeoJsonDocument::fromUrl(
                asyncSystem,
                pAssetAccessor,
                url,
                headerPairs);
          },
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("url"),
          py::arg("headers") = py::dict(),
          "Load a GeoJSON document from a URL asynchronously.")
      .def_static(
          "from_cesium_ion_asset",
          [](const CesiumAsync::AsyncSystem& asyncSystem,
             const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
             int64_t ionAssetID,
             const std::string& ionAccessToken,
             const std::string& ionAssetEndpointUrl) {
            return CesiumVectorData::GeoJsonDocument::fromCesiumIonAsset(
                asyncSystem,
                pAssetAccessor,
                ionAssetID,
                ionAccessToken,
                ionAssetEndpointUrl);
          },
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::arg("ion_asset_id"),
          py::arg("ion_access_token"),
          py::arg("ion_asset_endpoint_url") =
              std::string("https://api.cesium.com/"),
          "Load a GeoJSON document from a Cesium Ion asset asynchronously.")
      .def_readonly(
          "attributions",
          &CesiumVectorData::GeoJsonDocument::attributions)
      .def_readwrite(
          "root_object",
          &CesiumVectorData::GeoJsonDocument::rootObject)
      .def(
          "__len__",
          [](const CesiumVectorData::GeoJsonDocument& self) {
            auto* fc = self.rootObject
                           .getIf<CesiumVectorData::GeoJsonFeatureCollection>();
            if (!fc)
              throw py::type_error(
                  "GeoJsonDocument root is not a FeatureCollection");
            return fc->features.size();
          })
      .def(
          "__iter__",
          [](const CesiumVectorData::GeoJsonDocument& self) {
            return py::make_iterator(
                self.rootObject.begin(),
                self.rootObject.end());
          },
          py::keep_alive<0, 1>())
      .def(
          "__getitem__",
          [](const CesiumVectorData::GeoJsonDocument& self, int64_t index) {
            auto* fc = self.rootObject
                           .getIf<CesiumVectorData::GeoJsonFeatureCollection>();
            if (!fc)
              throw py::type_error(
                  "GeoJsonDocument root is not a FeatureCollection");
            if (index < 0)
              index += static_cast<int64_t>(fc->features.size());
            if (index < 0 || index >= static_cast<int64_t>(fc->features.size()))
              throw py::index_error("Feature index out of range");
            return fc->features[static_cast<size_t>(index)];
          },
          py::arg("index"))
      .def("__repr__", [](const CesiumVectorData::GeoJsonDocument& self) {
        return "<GeoJsonDocument root_type=" +
               std::string(
                   CesiumVectorData::geoJsonObjectTypeToString(
                       self.rootObject.getType())) +
               ">";
      });

  // ── VectorRasterizer ─────────────────────────────────────────────────────
  py::class_<CesiumVectorData::VectorRasterizer>(m, "VectorRasterizer")
      .def(
          py::init([](const CesiumGeospatial::GlobeRectangle& bounds,
                      CesiumUtility::IntrusivePointer<CesiumGltf::ImageAsset>
                          imageAsset,
                      uint32_t mipLevel,
                      const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumVectorData::VectorRasterizer(
                bounds,
                imageAsset,
                mipLevel,
                ellipsoid);
          }),
          py::arg("bounds"),
          py::arg("image_asset"),
          py::arg("mip_level") = 0,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "draw_polygon",
          py::overload_cast<
              const CesiumGeospatial::CartographicPolygon&,
              const CesiumVectorData::PolygonStyle&>(
              &CesiumVectorData::VectorRasterizer::drawPolygon),
          py::arg("polygon"),
          py::arg("style"))
      .def(
          "draw_polygon_rings",
          py::overload_cast<
              const std::vector<std::vector<glm::dvec3>>&,
              const CesiumVectorData::PolygonStyle&>(
              &CesiumVectorData::VectorRasterizer::drawPolygon),
          py::arg("rings"),
          py::arg("style"),
          "Draw a polygon from linear rings (list of list of [lon, lat, "
          "height] in degrees).")
      .def(
          "draw_polyline",
          &CesiumVectorData::VectorRasterizer::drawPolyline,
          py::arg("points"),
          py::arg("style"))
      .def(
          "draw_geojson_object",
          &CesiumVectorData::VectorRasterizer::drawGeoJsonObject,
          py::arg("geojson_object"),
          py::arg("style"))
      .def(
          "clear",
          &CesiumVectorData::VectorRasterizer::clear,
          py::arg("clear_color"))
      .def("finalize", &CesiumVectorData::VectorRasterizer::finalize);
}
