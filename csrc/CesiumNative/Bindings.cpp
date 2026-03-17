#include "BindingsRegistry.h"

#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
  m.doc() = "Compiled C++ bindings for cesium-native";

  // Canonical Python-facing submodules.
  auto async_ = m.def_submodule("async_", "CesiumAsync");
  auto tiles = m.def_submodule("tiles", "Cesium3DTiles");
  auto tilesContent = tiles.def_submodule("content", "Cesium3DTilesContent");
  auto tilesReader = tiles.def_submodule("reader", "Cesium3DTilesReader");
  auto tilesSelection =
      tiles.def_submodule("selection", "Cesium3DTilesSelection");
  auto tilesWriter = tiles.def_submodule("writer", "Cesium3DTilesWriter");
  auto curl = m.def_submodule("curl", "CesiumCurl");
  auto geometry = m.def_submodule("geometry", "CesiumGeometry");
  auto geospatial = m.def_submodule("geospatial", "CesiumGeospatial");
  auto gltf = m.def_submodule("gltf", "CesiumGltf");
  auto gltfContent = gltf.def_submodule("content", "CesiumGltfContent");
  auto gltfReader = gltf.def_submodule("reader", "CesiumGltfReader");
  auto gltfWriter = gltf.def_submodule("writer", "CesiumGltfWriter");
  auto json = m.def_submodule("json", "CesiumJson");
  auto jsonReader = json.def_submodule("reader", "CesiumJsonReader");
  auto jsonWriter = json.def_submodule("writer", "CesiumJsonWriter");
  auto quantizedMeshTerrain =
      m.def_submodule("quantized_mesh_terrain", "CesiumQuantizedMeshTerrain");
  auto rasterOverlays =
      m.def_submodule("raster_overlays", "CesiumRasterOverlays");
  auto utility = m.def_submodule("utility", "CesiumUtility");
  auto vectorData = m.def_submodule("vector_data", "CesiumVectorData");
  auto client = m.def_submodule("client", "Cesium client APIs");
  auto clientCommon = client.def_submodule("common", "CesiumClientCommon");
  auto clientIon = client.def_submodule("ion", "CesiumIonClient");
  auto clientItwin = client.def_submodule("itwin", "CesiumITwinClient");

  // Register submodules in sys.modules so `from cesium.X import Y` works.
  auto modules = py::module_::import("sys").attr("modules");
  modules["cesium.async_"] = async_;
  modules["cesium.tiles"] = tiles;
  modules["cesium.tiles.content"] = tilesContent;
  modules["cesium.tiles.reader"] = tilesReader;
  modules["cesium.tiles.selection"] = tilesSelection;
  modules["cesium.tiles.writer"] = tilesWriter;
  modules["cesium.curl"] = curl;
  modules["cesium.geometry"] = geometry;
  modules["cesium.geospatial"] = geospatial;
  modules["cesium.gltf"] = gltf;
  modules["cesium.gltf.content"] = gltfContent;
  modules["cesium.gltf.reader"] = gltfReader;
  modules["cesium.gltf.writer"] = gltfWriter;
  modules["cesium.json"] = json;
  modules["cesium.json.reader"] = jsonReader;
  modules["cesium.json.writer"] = jsonWriter;
  modules["cesium.quantized_mesh_terrain"] = quantizedMeshTerrain;
  modules["cesium.raster_overlays"] = rasterOverlays;
  modules["cesium.utility"] = utility;
  modules["cesium.vector_data"] = vectorData;
  modules["cesium.client"] = client;
  modules["cesium.client.common"] = clientCommon;
  modules["cesium.client.ion"] = clientIon;
  modules["cesium.client.itwin"] = clientItwin;

  // utility must come before gltf so ExtensibleObject is registered first.
  initUtilityBindings(utility);
  initAsyncBindings(async_);
  initTiles3dBindings(tiles);
  initGeometryBindings(geometry);
  initGeospatialBindings(geospatial);
  initTiles3dContentBindings(tilesContent);
  initTiles3dReaderBindings(tilesReader);
  initTiles3dWriterBindings(tilesWriter);
  initGltfReaderBindings(gltfReader);
  initTiles3dSelectionBindings(tilesSelection);
  initCurlBindings(curl);
  initGltfBindings(gltf);
  initGltfContentBindings(gltfContent);
  initGltfWriterBindings(gltfWriter);
  initJsonReaderBindings(jsonReader);
  initJsonWriterBindings(jsonWriter);
  initQuantizedMeshTerrainBindings(quantizedMeshTerrain);
  initRasterOverlaysBindings(rasterOverlays);
  initVectorDataBindings(vectorData);
  initClientCommonBindings(clientCommon);
  initIonClientBindings(clientIon);
  initITwinClientBindings(clientItwin);
}
