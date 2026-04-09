#include "FutureTemplates.h"
#include "NumpyConversions.h"

#include <CesiumAsync/Future.h>
#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/SharedFuture.h>
#include <CesiumGeometry/QuadtreeTileID.h>
#include <CesiumGeometry/Rectangle.h>
#include <CesiumGeospatial/BoundingRegion.h>
#include <CesiumGeospatial/CartographicPolygon.h>
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/Projection.h>
#include <CesiumGltf/Model.h>
#include <CesiumRasterOverlays/ActivatedRasterOverlay.h>
#include <CesiumRasterOverlays/AzureMapsRasterOverlay.h>
#include <CesiumRasterOverlays/BingMapsRasterOverlay.h>
#include <CesiumRasterOverlays/DebugColorizeTilesRasterOverlay.h>
#include <CesiumRasterOverlays/GeoJsonDocumentRasterOverlay.h>
#include <CesiumRasterOverlays/GoogleMapTilesRasterOverlay.h>
#include <CesiumRasterOverlays/ITwinCesiumCuratedContentRasterOverlay.h>
#include <CesiumRasterOverlays/IonRasterOverlay.h>
#include <CesiumRasterOverlays/QuadtreeRasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayDetails.h>
#include <CesiumRasterOverlays/RasterOverlayLoadFailureDetails.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumRasterOverlays/RasterOverlayTileProvider.h>
#include <CesiumRasterOverlays/RasterOverlayUtilities.h>
#include <CesiumRasterOverlays/RasterizedPolygonsOverlay.h>
#include <CesiumRasterOverlays/TileMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/UrlTemplateRasterOverlay.h>
#include <CesiumRasterOverlays/WebMapServiceRasterOverlay.h>
#include <CesiumRasterOverlays/WebMapTileServiceRasterOverlay.h>
#include <CesiumUtility/IntrusivePointer.h>
#include <CesiumVectorData/GeoJsonDocument.h>
#include <CesiumVectorData/VectorStyle.h>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

using FutureTileLoadResult =
    CesiumAsync::Future<CesiumRasterOverlays::RasterOverlayTileLoadResult>;
using SharedFutureVoid = CesiumAsync::SharedFuture<void>;

void initRasterOverlaysBindings(py::module& m) {
  py::enum_<CesiumRasterOverlays::RasterOverlayLoadType>(
      m,
      "RasterOverlayLoadType")
      .value("Unknown", CesiumRasterOverlays::RasterOverlayLoadType::Unknown)
      .value(
          "CesiumIon",
          CesiumRasterOverlays::RasterOverlayLoadType::CesiumIon)
      .value(
          "TileProvider",
          CesiumRasterOverlays::RasterOverlayLoadType::TileProvider)
      .export_values();

  py::class_<CesiumRasterOverlays::RasterOverlayLoadFailureDetails>(
      m,
      "RasterOverlayLoadFailureDetails")
      .def(py::init<>())
      .def_readwrite(
          "type",
          &CesiumRasterOverlays::RasterOverlayLoadFailureDetails::type)
      .def_readwrite(
          "request",
          &CesiumRasterOverlays::RasterOverlayLoadFailureDetails::pRequest)
      .def_readwrite(
          "message",
          &CesiumRasterOverlays::RasterOverlayLoadFailureDetails::message);

  py::class_<CesiumRasterOverlays::RasterOverlayOptions>(
      m,
      "RasterOverlayOptions")
      .def(py::init<>())
      .def_readwrite(
          "maximum_simultaneous_tile_loads",
          &CesiumRasterOverlays::RasterOverlayOptions::
              maximumSimultaneousTileLoads)
      .def_readwrite(
          "sub_tile_cache_bytes",
          &CesiumRasterOverlays::RasterOverlayOptions::subTileCacheBytes)
      .def_readwrite(
          "maximum_texture_size",
          &CesiumRasterOverlays::RasterOverlayOptions::maximumTextureSize)
      .def_readwrite(
          "maximum_screen_space_error",
          &CesiumRasterOverlays::RasterOverlayOptions::maximumScreenSpaceError)
      .def_readwrite(
          "ktx2_transcode_targets",
          &CesiumRasterOverlays::RasterOverlayOptions::ktx2TranscodeTargets)
      .def_readwrite(
          "show_credits_on_screen",
          &CesiumRasterOverlays::RasterOverlayOptions::showCreditsOnScreen)
      .def_readwrite(
          "ellipsoid",
          &CesiumRasterOverlays::RasterOverlayOptions::ellipsoid)
      .def_readwrite(
          "load_error_callback",
          &CesiumRasterOverlays::RasterOverlayOptions::loadErrorCallback);

  py::enum_<CesiumRasterOverlays::RasterOverlayTile::LoadState>(
      m,
      "RasterOverlayTileLoadState")
      .value(
          "Placeholder",
          CesiumRasterOverlays::RasterOverlayTile::LoadState::Placeholder)
      .value(
          "Failed",
          CesiumRasterOverlays::RasterOverlayTile::LoadState::Failed)
      .value(
          "Unloaded",
          CesiumRasterOverlays::RasterOverlayTile::LoadState::Unloaded)
      .value(
          "Loading",
          CesiumRasterOverlays::RasterOverlayTile::LoadState::Loading)
      .value(
          "Loaded",
          CesiumRasterOverlays::RasterOverlayTile::LoadState::Loaded)
      .value("Done", CesiumRasterOverlays::RasterOverlayTile::LoadState::Done)
      .export_values();

  py::enum_<CesiumRasterOverlays::RasterOverlayTile::MoreDetailAvailable>(
      m,
      "RasterOverlayMoreDetailAvailable")
      .value(
          "No",
          CesiumRasterOverlays::RasterOverlayTile::MoreDetailAvailable::No)
      .value(
          "Yes",
          CesiumRasterOverlays::RasterOverlayTile::MoreDetailAvailable::Yes)
      .value(
          "Unknown",
          CesiumRasterOverlays::RasterOverlayTile::MoreDetailAvailable::Unknown)
      .export_values();

  // ---- RasterOverlayTileProvider (read-only, abstract) ---------------------
  py::class_<
      CesiumRasterOverlays::RasterOverlayTileProvider,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::RasterOverlayTileProvider>>(
      m,
      "RasterOverlayTileProvider")
      .def_property_readonly(
          "projection",
          &CesiumRasterOverlays::RasterOverlayTileProvider::getProjection)
      .def_property_readonly(
          "coverage_rectangle",
          &CesiumRasterOverlays::RasterOverlayTileProvider::
              getCoverageRectangle)
      .def_property_readonly(
          "owner",
          [](const CesiumRasterOverlays::RasterOverlayTileProvider& self) {
            return py::cast(
                self.getOwner(),
                py::return_value_policy::reference);
          });

  // ---- QuadtreeRasterOverlayTileProvider (read-only, abstract) -------------
  py::class_<
      CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider,
      CesiumRasterOverlays::RasterOverlayTileProvider,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider>>(
      m,
      "QuadtreeRasterOverlayTileProvider")
      .def_property_readonly(
          "minimum_level",
          &CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider::
              getMinimumLevel)
      .def_property_readonly(
          "maximum_level",
          &CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider::
              getMaximumLevel)
      .def_property_readonly(
          "width",
          &CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider::getWidth)
      .def_property_readonly(
          "height",
          &CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider::getHeight)
      .def_property_readonly(
          "tiling_scheme",
          &CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider::
              getTilingScheme)
      .def(
          "compute_level_from_target_screen_pixels",
          [](CesiumRasterOverlays::QuadtreeRasterOverlayTileProvider& self,
             const CesiumGeometry::Rectangle& rectangle,
             double screenPixelsX,
             double screenPixelsY) {
            return self.computeLevelFromTargetScreenPixels(
                rectangle,
                glm::dvec2(screenPixelsX, screenPixelsY));
          },
          py::arg("rectangle"),
          py::arg("screen_pixels_x"),
          py::arg("screen_pixels_y"));

  py::class_<
      CesiumRasterOverlays::ActivatedRasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::ActivatedRasterOverlay>>(
      m,
      "ActivatedRasterOverlay")
      .def_property_readonly(
          "tile_data_bytes",
          &CesiumRasterOverlays::ActivatedRasterOverlay::getTileDataBytes)
      .def_property_readonly(
          "number_of_tiles_loading",
          &CesiumRasterOverlays::ActivatedRasterOverlay::
              getNumberOfTilesLoading)
      .def_property_readonly(
          "ready_event",
          [](CesiumRasterOverlays::ActivatedRasterOverlay& self)
              -> SharedFutureVoid& { return self.getReadyEvent(); },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "overlay",
          [](const CesiumRasterOverlays::ActivatedRasterOverlay& self) {
            return py::cast(
                self.getOverlay(),
                py::return_value_policy::reference);
          })
      .def_property_readonly(
          "tile_provider_ready",
          [](const CesiumRasterOverlays::ActivatedRasterOverlay& self) {
            return self.getTileProvider() != nullptr;
          })
      .def_property_readonly(
          "tile_provider",
          [](const CesiumRasterOverlays::ActivatedRasterOverlay& self)
              -> py::object {
            auto* ptr = self.getTileProvider();
            if (!ptr)
              return py::none();
            return py::cast(ptr, py::return_value_policy::reference_internal);
          })
      .def(
          "get_tile",
          [](CesiumRasterOverlays::ActivatedRasterOverlay& self,
             const CesiumGeometry::Rectangle& rectangle,
             double targetScreenPixelsX,
             double targetScreenPixelsY) {
            return self.getTile(
                rectangle,
                glm::dvec2(targetScreenPixelsX, targetScreenPixelsY));
          },
          py::arg("rectangle"),
          py::arg("target_screen_pixels_x"),
          py::arg("target_screen_pixels_y"))
      .def(
          "load_tile",
          [](CesiumRasterOverlays::ActivatedRasterOverlay& self,
             CesiumRasterOverlays::RasterOverlayTile& tile) {
            return self.loadTile(tile);
          },
          py::arg("tile"))
      .def(
          "load_tile_throttled",
          [](CesiumRasterOverlays::ActivatedRasterOverlay& self,
             CesiumRasterOverlays::RasterOverlayTile& tile) {
            return self.loadTileThrottled(tile);
          },
          py::arg("tile"));

  py::class_<
      CesiumRasterOverlays::RasterOverlayTile,
      CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlayTile>>(
      m,
      "RasterOverlayTile")
      .def_property_readonly(
          "state",
          &CesiumRasterOverlays::RasterOverlayTile::getState)
      .def(
          "load_in_main_thread",
          &CesiumRasterOverlays::RasterOverlayTile::loadInMainThread)
      .def(
          "is_more_detail_available",
          &CesiumRasterOverlays::RasterOverlayTile::isMoreDetailAvailable)
      .def_property_readonly(
          "target_screen_pixels",
          [](const CesiumRasterOverlays::RasterOverlayTile& self) {
            auto pixels = self.getTargetScreenPixels();
            return CesiumPython::toNumpy(glm::dvec2(pixels.x, pixels.y));
          })
      .def_property(
          "renderer_resources",
          [](const CesiumRasterOverlays::RasterOverlayTile& self) {
            return reinterpret_cast<uintptr_t>(self.getRendererResources());
          },
          [](CesiumRasterOverlays::RasterOverlayTile& self,
             uintptr_t pointerValue) {
            self.setRendererResources(reinterpret_cast<void*>(pointerValue));
          })
      .def_property_readonly(
          "rectangle",
          &CesiumRasterOverlays::RasterOverlayTile::getRectangle)
      .def_property_readonly(
          "image",
          [](const CesiumRasterOverlays::RasterOverlayTile& self) {
            return self.getImage();
          })
      .def_property_readonly(
          "credits",
          &CesiumRasterOverlays::RasterOverlayTile::getCredits)
      .def_property_readonly(
          "activated_overlay",
          [](CesiumRasterOverlays::RasterOverlayTile& self) {
            return py::cast(
                &self.getActivatedOverlay(),
                py::return_value_policy::reference_internal);
          })
      .def_property_readonly(
          "tile_provider",
          [](CesiumRasterOverlays::RasterOverlayTile& self) {
            return py::cast(
                &self.getTileProvider(),
                py::return_value_policy::reference_internal);
          })
      .def_property_readonly(
          "overlay",
          [](const CesiumRasterOverlays::RasterOverlayTile& self) {
            return py::cast(
                &self.getOverlay(),
                py::return_value_policy::reference);
          });

  py::class_<CesiumRasterOverlays::RasterOverlayTileLoadResult>(
      m,
      "RasterOverlayTileLoadResult")
      .def(
          py::init<
              const CesiumUtility::IntrusivePointer<
                  CesiumRasterOverlays::ActivatedRasterOverlay>&,
              const CesiumUtility::IntrusivePointer<
                  CesiumRasterOverlays::RasterOverlayTile>&>())
      .def_readwrite(
          "activated",
          &CesiumRasterOverlays::RasterOverlayTileLoadResult::pActivated)
      .def_readwrite(
          "tile",
          &CesiumRasterOverlays::RasterOverlayTileLoadResult::pTile);

  // Future<RasterOverlayTileLoadResult>
  auto futureTileLoadResult =
      py::class_<FutureTileLoadResult>(m, "FutureTileLoadResult");
  CesiumPython::bindFuture(futureTileLoadResult);

  // Base RasterOverlay
  py::class_<
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<CesiumRasterOverlays::RasterOverlay>>(
      m,
      "RasterOverlay")
      .def_property_readonly(
          "name",
          &CesiumRasterOverlays::RasterOverlay::getName)
      .def_property_readonly(
          "options",
          [](CesiumRasterOverlays::RasterOverlay& self)
              -> CesiumRasterOverlays::RasterOverlayOptions& {
            return self.getOptions();
          },
          py::return_value_policy::reference_internal);

  // UrlTemplateRasterOverlayOptions
  py::class_<CesiumRasterOverlays::UrlTemplateRasterOverlayOptions>(
      m,
      "UrlTemplateRasterOverlayOptions")
      .def(py::init<>())
      .def_readwrite(
          "credit",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::credit)
      .def_readwrite(
          "projection",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::projection)
      .def_readwrite(
          "tiling_scheme",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::tilingScheme)
      .def_readwrite(
          "minimum_level",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::minimumLevel)
      .def_readwrite(
          "maximum_level",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::maximumLevel)
      .def_readwrite(
          "tile_width",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::tileWidth)
      .def_readwrite(
          "tile_height",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::tileHeight)
      .def_readwrite(
          "coverage_rectangle",
          &CesiumRasterOverlays::UrlTemplateRasterOverlayOptions::
              coverageRectangle);

  // UrlTemplateRasterOverlay
  py::class_<
      CesiumRasterOverlays::UrlTemplateRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::UrlTemplateRasterOverlay>>(
      m,
      "UrlTemplateRasterOverlay")
      .def(
          py::init(
              [](const std::string& name,
                 const std::string& url,
                 const std::vector<std::pair<std::string, std::string>>&
                     headers,
                 const CesiumRasterOverlays::UrlTemplateRasterOverlayOptions&
                     urlTemplateOptions,
                 const CesiumRasterOverlays::RasterOverlayOptions&
                     overlayOptions) {
                std::vector<CesiumAsync::IAssetAccessor::THeader> hdrs(
                    headers.begin(),
                    headers.end());
                return new CesiumRasterOverlays::UrlTemplateRasterOverlay(
                    name,
                    url,
                    hdrs,
                    urlTemplateOptions,
                    overlayOptions);
              }),
          py::arg("name"),
          py::arg("url"),
          py::arg("headers") =
              std::vector<std::pair<std::string, std::string>>{},
          py::arg("url_template_options") =
              CesiumRasterOverlays::UrlTemplateRasterOverlayOptions{},
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a URL-template raster overlay (e.g. {z}/{x}/{y} tile "
          "servers).\n"
          "Pass headers as a list of (key, value) tuples, e.g. [('User-Agent', "
          "'myapp/1.0')].");

  // TileMapServiceRasterOverlayOptions
  py::class_<CesiumRasterOverlays::TileMapServiceRasterOverlayOptions>(
      m,
      "TileMapServiceRasterOverlayOptions")
      .def(py::init<>())
      .def_readwrite(
          "file_extension",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::
              fileExtension)
      .def_readwrite(
          "credit",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::credit)
      .def_readwrite(
          "minimum_level",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::
              minimumLevel)
      .def_readwrite(
          "maximum_level",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::
              maximumLevel)
      .def_readwrite(
          "coverage_rectangle",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::
              coverageRectangle)
      .def_readwrite(
          "projection",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::projection)
      .def_readwrite(
          "tiling_scheme",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::
              tilingScheme)
      .def_readwrite(
          "tile_width",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::tileWidth)
      .def_readwrite(
          "tile_height",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::tileHeight)
      .def_readwrite(
          "flip_xy",
          &CesiumRasterOverlays::TileMapServiceRasterOverlayOptions::flipXY);

  // TileMapServiceRasterOverlay
  py::class_<
      CesiumRasterOverlays::TileMapServiceRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::TileMapServiceRasterOverlay>>(
      m,
      "TileMapServiceRasterOverlay")
      .def(
          py::init(
              [](const std::string& name,
                 const std::string& url,
                 const std::vector<std::pair<std::string, std::string>>&
                     headers,
                 const CesiumRasterOverlays::TileMapServiceRasterOverlayOptions&
                     tmsOptions,
                 const CesiumRasterOverlays::RasterOverlayOptions&
                     overlayOptions) {
                std::vector<CesiumAsync::IAssetAccessor::THeader> hdrs(
                    headers.begin(),
                    headers.end());
                return new CesiumRasterOverlays::TileMapServiceRasterOverlay(
                    name,
                    url,
                    hdrs,
                    tmsOptions,
                    overlayOptions);
              }),
          py::arg("name"),
          py::arg("url"),
          py::arg("headers") =
              std::vector<std::pair<std::string, std::string>>{},
          py::arg("tms_options") =
              CesiumRasterOverlays::TileMapServiceRasterOverlayOptions{},
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a TMS raster overlay.");

  // BingMapsRasterOverlay
  py::class_<
      CesiumRasterOverlays::BingMapsRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::BingMapsRasterOverlay>>(
      m,
      "BingMapsRasterOverlay")
      .def(
          py::init([](const std::string& name,
                      const std::string& url,
                      const std::string& key,
                      const std::string& mapStyle,
                      const std::string& culture,
                      const CesiumRasterOverlays::RasterOverlayOptions&
                          overlayOptions) {
            return new CesiumRasterOverlays::BingMapsRasterOverlay(
                name,
                url,
                key,
                mapStyle,
                culture,
                overlayOptions);
          }),
          py::arg("name"),
          py::arg("url"),
          py::arg("key"),
          py::arg("map_style") = "Aerial",
          py::arg("culture") = "",
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a Bing Maps raster overlay.");

  // IonRasterOverlay
  py::class_<
      CesiumRasterOverlays::IonRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<CesiumRasterOverlays::IonRasterOverlay>>(
      m,
      "IonRasterOverlay")
      .def(
          py::init<
              const std::string&,
              int64_t,
              const std::string&,
              const CesiumRasterOverlays::RasterOverlayOptions&,
              const std::string&>(),
          py::arg("name"),
          py::arg("ion_asset_id"),
          py::arg("ion_access_token"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          py::arg("ion_asset_endpoint_url") = "https://api.cesium.com/",
          "Create a Cesium ion raster overlay.")
      .def_property(
          "asset_options",
          &CesiumRasterOverlays::IonRasterOverlay::getAssetOptions,
          &CesiumRasterOverlays::IonRasterOverlay::setAssetOptions);

  // DebugColorizeTilesRasterOverlay
  py::class_<
      CesiumRasterOverlays::DebugColorizeTilesRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::DebugColorizeTilesRasterOverlay>>(
      m,
      "DebugColorizeTilesRasterOverlay")
      .def(
          py::init<
              const std::string&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a debug colorize tiles raster overlay.");

  // WebMapServiceRasterOverlayOptions
  py::class_<CesiumRasterOverlays::WebMapServiceRasterOverlayOptions>(
      m,
      "WebMapServiceRasterOverlayOptions")
      .def(py::init<>())
      .def_readwrite(
          "version",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::version)
      .def_readwrite(
          "layers",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::layers)
      .def_readwrite(
          "format",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::format)
      .def_readwrite(
          "credit",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::credit)
      .def_readwrite(
          "minimum_level",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::
              minimumLevel)
      .def_readwrite(
          "maximum_level",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::
              maximumLevel)
      .def_readwrite(
          "tile_width",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::tileWidth)
      .def_readwrite(
          "tile_height",
          &CesiumRasterOverlays::WebMapServiceRasterOverlayOptions::tileHeight);

  // WebMapServiceRasterOverlay
  py::class_<
      CesiumRasterOverlays::WebMapServiceRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::WebMapServiceRasterOverlay>>(
      m,
      "WebMapServiceRasterOverlay")
      .def(
          py::init(
              [](const std::string& name,
                 const std::string& url,
                 const std::vector<std::pair<std::string, std::string>>&
                     headers,
                 const CesiumRasterOverlays::WebMapServiceRasterOverlayOptions&
                     wmsOptions,
                 const CesiumRasterOverlays::RasterOverlayOptions&
                     overlayOptions) {
                std::vector<CesiumAsync::IAssetAccessor::THeader> hdrs(
                    headers.begin(),
                    headers.end());
                return new CesiumRasterOverlays::WebMapServiceRasterOverlay(
                    name,
                    url,
                    hdrs,
                    wmsOptions,
                    overlayOptions);
              }),
          py::arg("name"),
          py::arg("url"),
          py::arg("headers") =
              std::vector<std::pair<std::string, std::string>>{},
          py::arg("wms_options") =
              CesiumRasterOverlays::WebMapServiceRasterOverlayOptions{},
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a Web Map Service (WMS) raster overlay.");

  // WebMapTileServiceRasterOverlayOptions
  py::class_<CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions>(
      m,
      "WebMapTileServiceRasterOverlayOptions")
      .def(py::init<>())
      .def_readwrite(
          "format",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::format)
      .def_readwrite(
          "subdomains",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              subdomains)
      .def_readwrite(
          "credit",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::credit)
      .def_readwrite(
          "layer",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::layer)
      .def_readwrite(
          "style",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::style)
      .def_readwrite(
          "tile_matrix_set_id",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              tileMatrixSetID)
      .def_readwrite(
          "tile_matrix_labels",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              tileMatrixLabels)
      .def_readwrite(
          "minimum_level",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              minimumLevel)
      .def_readwrite(
          "maximum_level",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              maximumLevel)
      .def_readwrite(
          "coverage_rectangle",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              coverageRectangle)
      .def_readwrite(
          "projection",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              projection)
      .def_readwrite(
          "tiling_scheme",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              tilingScheme)
      .def_readwrite(
          "dimensions",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              dimensions)
      .def_readwrite(
          "tile_width",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              tileWidth)
      .def_readwrite(
          "tile_height",
          &CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions::
              tileHeight);

  // WebMapTileServiceRasterOverlay
  py::class_<
      CesiumRasterOverlays::WebMapTileServiceRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::WebMapTileServiceRasterOverlay>>(
      m,
      "WebMapTileServiceRasterOverlay")
      .def(
          py::init([](const std::string& name,
                      const std::string& url,
                      const std::vector<std::pair<std::string, std::string>>&
                          headers,
                      const CesiumRasterOverlays::
                          WebMapTileServiceRasterOverlayOptions& tmsOptions,
                      const CesiumRasterOverlays::RasterOverlayOptions&
                          overlayOptions) {
            std::vector<CesiumAsync::IAssetAccessor::THeader> hdrs(
                headers.begin(),
                headers.end());
            return new CesiumRasterOverlays::WebMapTileServiceRasterOverlay(
                name,
                url,
                hdrs,
                tmsOptions,
                overlayOptions);
          }),
          py::arg("name"),
          py::arg("url"),
          py::arg("headers") =
              std::vector<std::pair<std::string, std::string>>{},
          py::arg("wmts_options") =
              CesiumRasterOverlays::WebMapTileServiceRasterOverlayOptions{},
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a Web Map Tile Service (WMTS) raster overlay.");

  // RasterizedPolygonsOverlay
  py::class_<
      CesiumRasterOverlays::RasterizedPolygonsOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::RasterizedPolygonsOverlay>>(
      m,
      "RasterizedPolygonsOverlay")
      .def(
          py::init<
              const std::string&,
              const std::vector<CesiumGeospatial::CartographicPolygon>&,
              bool,
              const CesiumGeospatial::Ellipsoid&,
              const CesiumGeospatial::Projection&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("polygons"),
          py::arg("invert_selection"),
          py::arg("ellipsoid"),
          py::arg("projection"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a rasterized polygons overlay.")
      .def_property_readonly(
          "polygons",
          &CesiumRasterOverlays::RasterizedPolygonsOverlay::getPolygons)
      .def_property_readonly(
          "invert_selection",
          &CesiumRasterOverlays::RasterizedPolygonsOverlay::getInvertSelection)
      .def_property_readonly(
          "ellipsoid",
          &CesiumRasterOverlays::RasterizedPolygonsOverlay::getEllipsoid);

  // GoogleMapTilesNewSessionParameters
  py::class_<CesiumRasterOverlays::GoogleMapTilesNewSessionParameters>(
      m,
      "GoogleMapTilesNewSessionParameters")
      .def(py::init<>())
      .def_readwrite(
          "key",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::key)
      .def_readwrite(
          "map_type",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::mapType)
      .def_readwrite(
          "language",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::language)
      .def_readwrite(
          "region",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::region)
      .def_readwrite(
          "image_format",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::
              imageFormat)
      .def_readwrite(
          "scale",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::scale)
      .def_readwrite(
          "high_dpi",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::highDpi)
      .def_readwrite(
          "layer_types",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::layerTypes)
      .def_readwrite(
          "overlay",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::overlay)
      .def_readwrite(
          "api_base_url",
          &CesiumRasterOverlays::GoogleMapTilesNewSessionParameters::
              apiBaseUrl);

  // GoogleMapTilesExistingSession
  py::class_<CesiumRasterOverlays::GoogleMapTilesExistingSession>(
      m,
      "GoogleMapTilesExistingSession")
      .def(py::init<>())
      .def_readwrite(
          "key",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::key)
      .def_readwrite(
          "session",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::session)
      .def_readwrite(
          "expiry",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::expiry)
      .def_readwrite(
          "tile_width",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::tileWidth)
      .def_readwrite(
          "tile_height",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::tileHeight)
      .def_readwrite(
          "image_format",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::imageFormat)
      .def_readwrite(
          "show_logo",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::showLogo)
      .def_readwrite(
          "api_base_url",
          &CesiumRasterOverlays::GoogleMapTilesExistingSession::apiBaseUrl);

  // GoogleMapTilesRasterOverlay
  py::class_<
      CesiumRasterOverlays::GoogleMapTilesRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::GoogleMapTilesRasterOverlay>>(
      m,
      "GoogleMapTilesRasterOverlay")
      .def(
          py::init<
              const std::string&,
              const CesiumRasterOverlays::GoogleMapTilesNewSessionParameters&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("new_session_parameters"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a Google Map Tiles raster overlay with a new session.")
      .def(
          py::init<
              const std::string&,
              const CesiumRasterOverlays::GoogleMapTilesExistingSession&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("existing_session"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a Google Map Tiles raster overlay with an existing session.");

  // AzureMapsTilesetId — string constants
  py::class_<CesiumRasterOverlays::AzureMapsTilesetId>(m, "AzureMapsTilesetId")
      .def_readonly_static(
          "base_road",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseRoad)
      .def_readonly_static(
          "base_dark_grey",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseDarkGrey)
      .def_readonly_static(
          "base_labels_road",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseLabelsRoad)
      .def_readonly_static(
          "base_labels_dark_grey",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseLabelsDarkGrey)
      .def_readonly_static(
          "base_hybrid_road",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseHybridRoad)
      .def_readonly_static(
          "base_hybrid_dark_grey",
          &CesiumRasterOverlays::AzureMapsTilesetId::baseHybridDarkGrey)
      .def_readonly_static(
          "imagery",
          &CesiumRasterOverlays::AzureMapsTilesetId::imagery)
      .def_readonly_static(
          "terra",
          &CesiumRasterOverlays::AzureMapsTilesetId::terra)
      .def_readonly_static(
          "weather_radar",
          &CesiumRasterOverlays::AzureMapsTilesetId::weatherRadar)
      .def_readonly_static(
          "weather_infrared",
          &CesiumRasterOverlays::AzureMapsTilesetId::weatherInfrared)
      .def_readonly_static(
          "traffic_absolute",
          &CesiumRasterOverlays::AzureMapsTilesetId::trafficAbsolute)
      .def_readonly_static(
          "traffic_relative_main",
          &CesiumRasterOverlays::AzureMapsTilesetId::trafficRelativeMain)
      .def_readonly_static(
          "traffic_relative_dark",
          &CesiumRasterOverlays::AzureMapsTilesetId::trafficRelativeDark)
      .def_readonly_static(
          "traffic_delay",
          &CesiumRasterOverlays::AzureMapsTilesetId::trafficDelay)
      .def_readonly_static(
          "traffic_reduced",
          &CesiumRasterOverlays::AzureMapsTilesetId::trafficReduced);

  // AzureMapsSessionParameters
  py::class_<CesiumRasterOverlays::AzureMapsSessionParameters>(
      m,
      "AzureMapsSessionParameters")
      .def(py::init<>())
      .def_readwrite(
          "key",
          &CesiumRasterOverlays::AzureMapsSessionParameters::key)
      .def_readwrite(
          "api_version",
          &CesiumRasterOverlays::AzureMapsSessionParameters::apiVersion)
      .def_readwrite(
          "tileset_id",
          &CesiumRasterOverlays::AzureMapsSessionParameters::tilesetId)
      .def_readwrite(
          "language",
          &CesiumRasterOverlays::AzureMapsSessionParameters::language)
      .def_readwrite(
          "view",
          &CesiumRasterOverlays::AzureMapsSessionParameters::view)
      .def_readwrite(
          "show_logo",
          &CesiumRasterOverlays::AzureMapsSessionParameters::showLogo)
      .def_readwrite(
          "api_base_url",
          &CesiumRasterOverlays::AzureMapsSessionParameters::apiBaseUrl);

  // AzureMapsRasterOverlay
  py::class_<
      CesiumRasterOverlays::AzureMapsRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::AzureMapsRasterOverlay>>(
      m,
      "AzureMapsRasterOverlay")
      .def(
          py::init<
              const std::string&,
              const CesiumRasterOverlays::AzureMapsSessionParameters&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("session_parameters"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create an Azure Maps raster overlay.");

  // GeoJsonDocumentRasterOverlayOptions
  py::class_<CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions>(
      m,
      "GeoJsonDocumentRasterOverlayOptions")
      .def(py::init([]() {
        return CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions{
            CesiumVectorData::VectorStyle{},
            CesiumGeospatial::Ellipsoid::WGS84,
            0};
      }))
      .def_readwrite(
          "default_style",
          &CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions::
              defaultStyle)
      .def_readwrite(
          "ellipsoid",
          &CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions::ellipsoid)
      .def_readwrite(
          "mip_levels",
          &CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions::
              mipLevels);

  // GeoJsonDocumentRasterOverlay
  py::class_<
      CesiumRasterOverlays::GeoJsonDocumentRasterOverlay,
      CesiumRasterOverlays::RasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::GeoJsonDocumentRasterOverlay>>(
      m,
      "GeoJsonDocumentRasterOverlay")
      .def(
          py::init(
              [](const CesiumAsync::AsyncSystem& asyncSystem,
                 const std::string& name,
                 const std::shared_ptr<CesiumVectorData::GeoJsonDocument>&
                     document,
                 const CesiumRasterOverlays::
                     GeoJsonDocumentRasterOverlayOptions& vectorOverlayOptions,
                 const CesiumRasterOverlays::RasterOverlayOptions&
                     overlayOptions) {
                return new CesiumRasterOverlays::GeoJsonDocumentRasterOverlay(
                    asyncSystem,
                    name,
                    document,
                    vectorOverlayOptions,
                    overlayOptions);
              }),
          py::arg("async_system"),
          py::arg("name"),
          py::arg("document"),
          py::arg("vector_overlay_options") =
              CesiumRasterOverlays::GeoJsonDocumentRasterOverlayOptions{
                  CesiumVectorData::VectorStyle{},
                  CesiumGeospatial::Ellipsoid::WGS84,
                  0},
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create a GeoJSON document raster overlay.");

  // ITwinCesiumCuratedContentRasterOverlay
  py::class_<
      CesiumRasterOverlays::ITwinCesiumCuratedContentRasterOverlay,
      CesiumRasterOverlays::IonRasterOverlay,
      CesiumUtility::IntrusivePointer<
          CesiumRasterOverlays::ITwinCesiumCuratedContentRasterOverlay>>(
      m,
      "ITwinCesiumCuratedContentRasterOverlay")
      .def(
          py::init<
              const std::string&,
              int64_t,
              const std::string&,
              const CesiumRasterOverlays::RasterOverlayOptions&>(),
          py::arg("name"),
          py::arg("asset_id"),
          py::arg("itwin_access_token"),
          py::arg("overlay_options") =
              CesiumRasterOverlays::RasterOverlayOptions{},
          "Create an iTwin Cesium Curated Content raster overlay.");

  // RasterOverlayDetails (G9)
  py::class_<CesiumRasterOverlays::RasterOverlayDetails>(
      m,
      "RasterOverlayDetails")
      .def(py::init<>())
      .def(
          py::init([](std::vector<CesiumGeospatial::Projection> projections,
                      std::vector<CesiumGeometry::Rectangle> rectangles,
                      const CesiumGeospatial::BoundingRegion& boundingRegion) {
            return CesiumRasterOverlays::RasterOverlayDetails(
                std::move(projections),
                std::move(rectangles),
                boundingRegion);
          }),
          py::arg("raster_overlay_projections"),
          py::arg("raster_overlay_rectangles"),
          py::arg("bounding_region"))
      .def_readwrite(
          "raster_overlay_projections",
          &CesiumRasterOverlays::RasterOverlayDetails::rasterOverlayProjections)
      .def_readwrite(
          "raster_overlay_rectangles",
          &CesiumRasterOverlays::RasterOverlayDetails::rasterOverlayRectangles)
      .def_readwrite(
          "bounding_region",
          &CesiumRasterOverlays::RasterOverlayDetails::boundingRegion)
      .def(
          "find_rectangle_for_overlay_projection",
          [](const CesiumRasterOverlays::RasterOverlayDetails& self,
             const CesiumGeospatial::Projection& projection)
              -> std::optional<CesiumGeometry::Rectangle> {
            const auto* ptr =
                self.findRectangleForOverlayProjection(projection);
            if (ptr)
              return *ptr;
            return std::nullopt;
          },
          py::arg("projection"),
          "Find the rectangle for a given overlay projection, or None if not "
          "found.")
      .def(
          "merge",
          &CesiumRasterOverlays::RasterOverlayDetails::merge,
          py::arg("other"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84,
          "Merge another RasterOverlayDetails into this one.");

  // RasterOverlayUtilities (G8)
  py::class_<CesiumRasterOverlays::RasterOverlayUtilities>(
      m,
      "RasterOverlayUtilities")
      .def_property_readonly_static(
          "DEFAULT_TEXTURE_COORDINATE_BASE_NAME",
          [](py::object /*self*/) {
            return std::string(
                CesiumRasterOverlays::RasterOverlayUtilities::
                    DEFAULT_TEXTURE_COORDINATE_BASE_NAME);
          })
      .def_static(
          "create_raster_overlay_texture_coordinates",
          [](CesiumGltf::Model& gltf,
             py::array_t<double> modelToEcefTransform,
             const std::optional<CesiumGeospatial::GlobeRectangle>&
                 globeRectangle,
             std::vector<CesiumGeospatial::Projection> projections,
             bool invertVCoordinate,
             const std::string& texCoordBaseName,
             int32_t firstTexCoordID) {
            return CesiumRasterOverlays::RasterOverlayUtilities::
                createRasterOverlayTextureCoordinates(
                    gltf,
                    CesiumPython::toDmat<4>(modelToEcefTransform),
                    globeRectangle,
                    std::move(projections),
                    invertVCoordinate,
                    texCoordBaseName,
                    firstTexCoordID);
          },
          py::arg("gltf"),
          py::arg("model_to_ecef_transform"),
          py::arg("globe_rectangle"),
          py::arg("projections"),
          py::arg("invert_v_coordinate") = false,
          py::arg("texture_coordinate_attribute_base_name") = std::string(
              CesiumRasterOverlays::RasterOverlayUtilities::
                  DEFAULT_TEXTURE_COORDINATE_BASE_NAME),
          py::arg("first_texture_coordinate_id") = 0,
          "Create raster overlay texture coordinates for a glTF model.")
      .def_static(
          "upsample_gltf_for_raster_overlays",
          [](const CesiumGltf::Model& parentModel,
             CesiumGeometry::UpsampledQuadtreeNode childID,
             bool hasInvertedVCoordinate,
             const std::string& texCoordBaseName,
             int32_t texCoordIndex,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumRasterOverlays::RasterOverlayUtilities::
                upsampleGltfForRasterOverlays(
                    parentModel,
                    childID,
                    hasInvertedVCoordinate,
                    texCoordBaseName,
                    texCoordIndex,
                    ellipsoid);
          },
          py::arg("parent_model"),
          py::arg("child_id"),
          py::arg("has_inverted_v_coordinate") = false,
          py::arg("texture_coordinate_attribute_base_name") = std::string(
              CesiumRasterOverlays::RasterOverlayUtilities::
                  DEFAULT_TEXTURE_COORDINATE_BASE_NAME),
          py::arg("texture_coordinate_index") = 0,
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84,
          "Upsample a glTF model for raster overlays.")
      .def_static(
          "compute_desired_screen_pixels",
          [](double geometricError,
             double maximumScreenSpaceError,
             const CesiumGeospatial::Projection& projection,
             const CesiumGeometry::Rectangle& rectangle,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return CesiumPython::toNumpy(
                CesiumRasterOverlays::RasterOverlayUtilities::
                    computeDesiredScreenPixels(
                        geometricError,
                        maximumScreenSpaceError,
                        projection,
                        rectangle,
                        ellipsoid));
          },
          py::arg("geometric_error"),
          py::arg("maximum_screen_space_error"),
          py::arg("projection"),
          py::arg("rectangle"),
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84,
          "Compute the desired screen pixels for a raster overlay texture.")
      .def_static(
          "compute_translation_and_scale",
          [](const CesiumGeometry::Rectangle& geometryRectangle,
             const CesiumGeometry::Rectangle& overlayRectangle) {
            return CesiumPython::toNumpy(
                CesiumRasterOverlays::RasterOverlayUtilities::
                    computeTranslationAndScale(
                        geometryRectangle,
                        overlayRectangle));
          },
          py::arg("geometry_rectangle"),
          py::arg("overlay_rectangle"),
          "Compute texture translation and scale to align an overlay with "
          "geometry.");
}
