#include "FutureTemplates.h"
#include "JsonConversions.h"
#include "NumpyConversions.h"

#include <Cesium3DTilesSelection/BoundingVolume.h>
#include <Cesium3DTilesSelection/CesiumIonTilesetContentLoaderFactory.h>
#include <Cesium3DTilesSelection/DebugTileStateDatabase.h>
#include <Cesium3DTilesSelection/EllipsoidTilesetLoader.h>
#include <Cesium3DTilesSelection/GltfModifier.h>
#include <Cesium3DTilesSelection/GltfModifierState.h>
#include <Cesium3DTilesSelection/IModelMeshExportContentLoaderFactory.h>
#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <Cesium3DTilesSelection/ITileExcluder.h>
#include <Cesium3DTilesSelection/ITilesetHeightSampler.h>
#include <Cesium3DTilesSelection/ITwinCesiumCuratedContentLoaderFactory.h>
#include <Cesium3DTilesSelection/ITwinRealityDataContentLoaderFactory.h>
#include <Cesium3DTilesSelection/LoadedTileEnumerator.h>
#include <Cesium3DTilesSelection/RasterMappedTo3DTile.h>
#include <Cesium3DTilesSelection/RasterOverlayCollection.h>
#include <Cesium3DTilesSelection/RasterizedPolygonsTileExcluder.h>
#include <Cesium3DTilesSelection/SampleHeightResult.h>
#include <Cesium3DTilesSelection/Tile.h>
#include <Cesium3DTilesSelection/TileContent.h>
#include <Cesium3DTilesSelection/TileID.h>
#include <Cesium3DTilesSelection/TileLoadRequester.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <Cesium3DTilesSelection/TileLoadTask.h>
#include <Cesium3DTilesSelection/TileOcclusionRendererProxy.h>
#include <Cesium3DTilesSelection/TileRefine.h>
#include <Cesium3DTilesSelection/TileSelectionState.h>
#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetContentLoader.h>
#include <Cesium3DTilesSelection/TilesetContentLoaderFactory.h>
#include <Cesium3DTilesSelection/TilesetContentLoaderResult.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesSelection/TilesetFrameState.h>
#include <Cesium3DTilesSelection/TilesetLoadFailureDetails.h>
#include <Cesium3DTilesSelection/TilesetMetadata.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/TilesetSharedAssetSystem.h>
#include <Cesium3DTilesSelection/TilesetViewGroup.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumRasterOverlays/RasterOverlay.h>
#include <CesiumRasterOverlays/RasterOverlayTile.h>
#include <CesiumRasterOverlays/RasterizedPolygonsOverlay.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/IntrusivePointer.h>
#include <CesiumUtility/Result.h>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

// Force pybind11 to treat BoundingVolume (a std::variant typedef) as a
// registered py::class_ instead of using the automatic std::variant caster
// from pybind11/stl.h, which can't default-construct the variant.
namespace pybind11 {
namespace detail {
template <>
struct type_caster<Cesium3DTilesSelection::BoundingVolume>
    : public type_caster_base<Cesium3DTilesSelection::BoundingVolume> {};
} // namespace detail
} // namespace pybind11

namespace py = pybind11;
using CesiumPython::toDmat;
using CesiumPython::toDvec;
using CesiumPython::toNumpy;
using CesiumPython::toNumpyView;

namespace {

using FutureSampleHeightResult =
    CesiumAsync::Future<Cesium3DTilesSelection::SampleHeightResult>;
using SharedFutureSampleHeightResult =
    CesiumAsync::SharedFuture<Cesium3DTilesSelection::SampleHeightResult>;
using FutureTilesetMetadataPtr =
    CesiumAsync::Future<const Cesium3DTilesSelection::TilesetMetadata*>;
using SharedFutureTilesetMetadataPtr =
    CesiumAsync::SharedFuture<const Cesium3DTilesSelection::TilesetMetadata*>;
using TilesetContentLoaderResult =
    Cesium3DTilesSelection::TilesetContentLoaderResult<
        Cesium3DTilesSelection::TilesetContentLoader>;
using FutureTilesetContentLoaderResult =
    CesiumAsync::Future<TilesetContentLoaderResult>;
using FutureTileLoadResult =
    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResult>;

class PyTilesetContentLoaderFactory
    : public Cesium3DTilesSelection::TilesetContentLoaderFactory {
public:
  using Cesium3DTilesSelection::TilesetContentLoaderFactory::
      TilesetContentLoaderFactory;

  FutureTilesetContentLoaderResult createLoader(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      const AuthorizationHeaderChangeListener& headerChangeListener) override {
    PYBIND11_OVERRIDE_PURE_NAME(
        FutureTilesetContentLoaderResult,
        Cesium3DTilesSelection::TilesetContentLoaderFactory,
        "create_loader",
        createLoader,
        externals,
        tilesetOptions,
        headerChangeListener);
  }

  bool isValid() const override {
    PYBIND11_OVERRIDE_PURE_NAME(
        bool,
        Cesium3DTilesSelection::TilesetContentLoaderFactory,
        "is_valid",
        isValid);
  }
};

class PyITilesetHeightSampler
    : public Cesium3DTilesSelection::ITilesetHeightSampler {
public:
  using Cesium3DTilesSelection::ITilesetHeightSampler::ITilesetHeightSampler;

  FutureSampleHeightResult sampleHeights(
      const CesiumAsync::AsyncSystem& asyncSystem,
      std::vector<CesiumGeospatial::Cartographic>&& positions) override {
    // Convert positions to (N,3) numpy before calling into Python.
    py::gil_scoped_acquire gil;
    const py::ssize_t n = static_cast<py::ssize_t>(positions.size());
    py::array_t<double> arr({n, py::ssize_t(3)});
    auto buf = arr.mutable_unchecked<2>();
    for (py::ssize_t i = 0; i < n; ++i) {
      buf(i, 0) = positions[static_cast<size_t>(i)].longitude;
      buf(i, 1) = positions[static_cast<size_t>(i)].latitude;
      buf(i, 2) = positions[static_cast<size_t>(i)].height;
    }
    PYBIND11_OVERRIDE_PURE_NAME(
        FutureSampleHeightResult,
        Cesium3DTilesSelection::ITilesetHeightSampler,
        "sample_heights",
        sampleHeights,
        asyncSystem,
        std::move(arr));
  }
};

class SharedPtrTilesetContentLoaderFactory final
    : public Cesium3DTilesSelection::TilesetContentLoaderFactory {
public:
  explicit SharedPtrTilesetContentLoaderFactory(
      std::shared_ptr<Cesium3DTilesSelection::TilesetContentLoaderFactory>
          pFactory) noexcept
      : _pFactory(std::move(pFactory)) {}

  FutureTilesetContentLoaderResult createLoader(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      const AuthorizationHeaderChangeListener& headerChangeListener) override {
    if (!this->_pFactory) {
      throw std::runtime_error("TilesetContentLoaderFactory is null.");
    }
    return this->_pFactory->createLoader(
        externals,
        tilesetOptions,
        headerChangeListener);
  }

  bool isValid() const override {
    return this->_pFactory && this->_pFactory->isValid();
  }

private:
  std::shared_ptr<Cesium3DTilesSelection::TilesetContentLoaderFactory>
      _pFactory;
};

// ---------------------------------------------------------------------------
// Trampoline for IPrepareRendererResources – allows Python subclasses to
// implement all 8 pure-virtual methods (5 own + 3 from parent
// IPrepareRasterOverlayRendererResources).
//
// void* render-resource pointers are stored as PyObject* with an extra ref
// (via Py_INCREF).  They are unpacked back to py::object (stealing the ref)
// once the engine returns them to free / prepareInMainThread.
// ---------------------------------------------------------------------------
class PyIPrepareRendererResources
    : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
  using Cesium3DTilesSelection::IPrepareRendererResources::
      IPrepareRendererResources;

  // -- void* <-> py::object helpers -----------------------------------------
  static void* packPyObject(py::object obj) {
    if (obj.is_none())
      return nullptr;
    PyObject* raw = obj.ptr();
    Py_INCREF(raw);
    return static_cast<void*>(raw);
  }

  /// Steal: wraps the raw pointer as py::object **and** consumes the extra ref
  /// we created in packPyObject.  Use when the caller is taking ownership.
  static py::object stealPyObject(void* ptr) {
    if (!ptr)
      return py::none();
    return py::reinterpret_steal<py::object>(
        py::handle(static_cast<PyObject*>(ptr)));
  }

  /// Borrow: wraps without consuming the ref.  Use when the caller just needs
  /// to peek (e.g. attachRasterInMainThread).
  static py::object borrowPyObject(void* ptr) {
    if (!ptr)
      return py::none();
    return py::reinterpret_borrow<py::object>(
        py::handle(static_cast<PyObject*>(ptr)));
  }

  CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
  prepareInLoadThread(
      const CesiumAsync::AsyncSystem& asyncSystem,
      Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
      const glm::dmat4& transform,
      const std::any& /*rendererOptions*/) override {
    py::gil_scoped_acquire gil;
    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
            this),
        "prepare_in_load_thread");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function "
          "IPrepareRendererResources.prepare_in_load_thread");
    }

    // Pass the full TileLoadResult to Python as a reference.  The object
    // lives until we std::move it into the output below, so the reference
    // is valid for the entire duration of the Python callback.  The .model
    // property on TileLoadResult uses return_value_policy::reference (NOT
    // reference_internal) to avoid keep_alive lookup failures on this
    // temporary wrapper.
    py::object pyTLR =
        py::cast(&tileLoadResult, py::return_value_policy::reference);
    py::object pyAsync =
        py::cast(&asyncSystem, py::return_value_policy::reference);

    py::object renderResources = override_(pyAsync, pyTLR, toNumpy(transform));

    Cesium3DTilesSelection::TileLoadResultAndRenderResources out;
    out.result = std::move(tileLoadResult);
    out.pRenderResources = packPyObject(renderResources);
    return asyncSystem.createResolvedFuture(std::move(out));
  }

  void* prepareInMainThread(
      Cesium3DTilesSelection::Tile& tile,
      void* pLoadThreadResult) override {
    py::gil_scoped_acquire gil;
    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
            this),
        "prepare_in_main_thread");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function "
          "IPrepareRendererResources.prepare_in_main_thread");
    }
    // Steal the load-thread result — this method owns it per cesium-native
    // contract (it will NOT be passed to free later).
    py::object loadResult = stealPyObject(pLoadThreadResult);
    py::object mainResult = override_(
        py::cast(tile, py::return_value_policy::reference),
        loadResult);
    return packPyObject(mainResult);
  }

  void free(
      Cesium3DTilesSelection::Tile& tile,
      void* pLoadThreadResult,
      void* pMainThreadResult) noexcept override {
    py::gil_scoped_acquire gil;
    // Steal both refs so they are released when these locals go out of scope.
    py::object loadObj = stealPyObject(pLoadThreadResult);
    py::object mainObj = stealPyObject(pMainThreadResult);
    try {
      py::function override_ = pybind11::get_override(
          static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
              this),
          "free_tile");
      if (override_) {
        override_(
            py::cast(tile, py::return_value_policy::reference),
            loadObj,
            mainObj);
      }
    } catch (py::error_already_set& e) {
      e.discard_as_unraisable(__func__);
    } catch (...) {
    }
    // loadObj / mainObj destructor decrements refcounts automatically.
  }

  void attachRasterInMainThread(
      const Cesium3DTilesSelection::Tile& tile,
      int32_t overlayTextureCoordinateID,
      const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
      void* pMainThreadRendererResources,
      const glm::dvec2& translation,
      const glm::dvec2& scale) override {
    py::gil_scoped_acquire gil;
    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
            this),
        "attach_raster_in_main_thread");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function "
          "IPrepareRendererResources.attach_raster_in_main_thread");
    }
    override_(
        py::cast(tile, py::return_value_policy::reference),
        overlayTextureCoordinateID,
        py::cast(rasterTile, py::return_value_policy::reference),
        borrowPyObject(pMainThreadRendererResources),
        toNumpy(translation),
        toNumpy(scale));
  }

  void detachRasterInMainThread(
      const Cesium3DTilesSelection::Tile& tile,
      int32_t overlayTextureCoordinateID,
      const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
      void* pMainThreadRendererResources) noexcept override {
    try {
      py::gil_scoped_acquire gil;
      py::function override_ = pybind11::get_override(
          static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
              this),
          "detach_raster_in_main_thread");
      if (override_) {
        override_(
            py::cast(tile, py::return_value_policy::reference),
            overlayTextureCoordinateID,
            py::cast(rasterTile, py::return_value_policy::reference),
            borrowPyObject(pMainThreadRendererResources));
      }
    } catch (py::error_already_set& e) {
      py::gil_scoped_acquire gil;
      e.discard_as_unraisable(__func__);
    } catch (...) {
    }
  }

  void* prepareRasterInLoadThread(
      CesiumGltf::ImageAsset& image,
      const std::any& /*rendererOptions*/) override {
    py::gil_scoped_acquire gil;
    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
            this),
        "prepare_raster_in_load_thread");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function "
          "IPrepareRendererResources.prepare_raster_in_load_thread");
    }
    py::object result =
        override_(py::cast(image, py::return_value_policy::reference));
    return packPyObject(result);
  }

  void* prepareRasterInMainThread(
      CesiumRasterOverlays::RasterOverlayTile& rasterTile,
      void* pLoadThreadResult) override {
    py::gil_scoped_acquire gil;
    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
            this),
        "prepare_raster_in_main_thread");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function "
          "IPrepareRendererResources.prepare_raster_in_main_thread");
    }
    py::object loadResult = stealPyObject(pLoadThreadResult);
    py::object mainResult = override_(
        py::cast(rasterTile, py::return_value_policy::reference),
        loadResult);
    return packPyObject(mainResult);
  }

  void freeRaster(
      const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
      void* pLoadThreadResult,
      void* pMainThreadResult) noexcept override {
    py::gil_scoped_acquire gil;
    py::object loadObj = stealPyObject(pLoadThreadResult);
    py::object mainObj = stealPyObject(pMainThreadResult);
    try {
      py::function override_ = pybind11::get_override(
          static_cast<const Cesium3DTilesSelection::IPrepareRendererResources*>(
              this),
          "free_raster");
      if (override_) {
        override_(
            py::cast(rasterTile, py::return_value_policy::reference),
            loadObj,
            mainObj);
      }
    } catch (py::error_already_set& e) {
      e.discard_as_unraisable(__func__);
    } catch (...) {
    }
  }
};

class CallbackTilesetContentLoaderFactory final
    : public Cesium3DTilesSelection::TilesetContentLoaderFactory {
public:
  CallbackTilesetContentLoaderFactory(
      std::function<FutureTilesetContentLoaderResult(
          const Cesium3DTilesSelection::TilesetExternals&,
          const Cesium3DTilesSelection::TilesetOptions&,
          const AuthorizationHeaderChangeListener&)> createLoaderCallback,
      std::function<bool()> isValidCallback)
      : _createLoaderCallback(std::move(createLoaderCallback)),
        _isValidCallback(std::move(isValidCallback)) {}

  FutureTilesetContentLoaderResult createLoader(
      const Cesium3DTilesSelection::TilesetExternals& externals,
      const Cesium3DTilesSelection::TilesetOptions& tilesetOptions,
      const AuthorizationHeaderChangeListener& headerChangeListener) override {
    return this->_createLoaderCallback(
        externals,
        tilesetOptions,
        headerChangeListener);
  }

  bool isValid() const override {
    if (!this->_isValidCallback) {
      return true;
    }
    return this->_isValidCallback();
  }

private:
  std::function<FutureTilesetContentLoaderResult(
      const Cesium3DTilesSelection::TilesetExternals&,
      const Cesium3DTilesSelection::TilesetOptions&,
      const AuthorizationHeaderChangeListener&)>
      _createLoaderCallback;
  std::function<bool()> _isValidCallback;
};

class PyITileExcluder : public Cesium3DTilesSelection::ITileExcluder {
public:
  using Cesium3DTilesSelection::ITileExcluder::ITileExcluder;

  void startNewFrame() noexcept override {
    py::gil_scoped_acquire gil;
    py::function overload = py::get_override(
        static_cast<const Cesium3DTilesSelection::ITileExcluder*>(this),
        "start_new_frame");
    if (!overload) {
      return;
    }
    try {
      overload();
    } catch (py::error_already_set& e) {
      e.discard_as_unraisable("ITileExcluder.start_new_frame");
    }
  }

  bool shouldExclude(
      const Cesium3DTilesSelection::Tile& tile) const noexcept override {
    py::gil_scoped_acquire gil;
    py::function overload = py::get_override(
        static_cast<const Cesium3DTilesSelection::ITileExcluder*>(this),
        "should_exclude");
    if (!overload) {
      return false;
    }
    try {
      return py::cast<bool>(
          overload(py::cast(&tile, py::return_value_policy::reference)));
    } catch (py::error_already_set& e) {
      e.discard_as_unraisable("ITileExcluder.should_exclude");
      return false;
    }
  }
};

// ---------------------------------------------------------------------------
// Trampoline for GltfModifier – allows Python subclasses to override apply().
// The Python override returns a Model (as GltfModifierOutput) or None.
// The C++ trampoline wraps the result in a resolved Future.
// ---------------------------------------------------------------------------
class PyGltfModifier : public Cesium3DTilesSelection::GltfModifier {
public:
  using Cesium3DTilesSelection::GltfModifier::GltfModifier;

  CesiumAsync::Future<std::optional<Cesium3DTilesSelection::GltfModifierOutput>>
  apply(Cesium3DTilesSelection::GltfModifierInput&& input) override {
    py::gil_scoped_acquire gil;
    auto asyncSystem = input.asyncSystem;

    py::function override_ = pybind11::get_override(
        static_cast<const Cesium3DTilesSelection::GltfModifier*>(this),
        "apply");
    if (!override_) {
      throw py::type_error(
          "Tried to call pure virtual function GltfModifier.apply");
    }

    // Pass the input Model to Python as a reference (valid for duration of
    // call)
    py::object pyModel =
        py::cast(&input.previousModel, py::return_value_policy::reference);
    py::object pyTransform = toNumpy(input.tileTransform);

    py::object result = override_(pyModel, pyTransform);

    if (result.is_none()) {
      return asyncSystem.createResolvedFuture(
          std::optional<Cesium3DTilesSelection::GltfModifierOutput>(
              std::nullopt));
    }

    Cesium3DTilesSelection::GltfModifierOutput output;
    output.modifiedModel = result.cast<CesiumGltf::Model>();
    return asyncSystem.createResolvedFuture(
        std::optional<Cesium3DTilesSelection::GltfModifierOutput>(
            std::move(output)));
  }
};

py::array_t<double, py::array::c_style | py::array::forcecast>
require1dArray(const py::handle& value) {
  auto arr = py::reinterpret_borrow<
      py::array_t<double, py::array::c_style | py::array::forcecast>>(value);
  if (arr.ndim() != 1) {
    throw py::value_error("Expected 1D numpy array.");
  }
  return arr;
}

py::array_t<double> batchComputeScreenSpaceError(
    const Cesium3DTilesSelection::ViewState& viewState,
    const py::handle& geometricErrors,
    const py::handle& distances) {
  auto ge = require1dArray(geometricErrors);
  auto d = require1dArray(distances);
  if (ge.shape(0) != d.shape(0)) {
    throw py::value_error(
        "geometric_errors and distances must have equal length.");
  }
  auto geIn = ge.unchecked<1>();
  auto dIn = d.unchecked<1>();
  py::array_t<double> result(ge.shape(0));
  auto out = result.mutable_unchecked<1>();
  for (py::ssize_t i = 0; i < ge.shape(0); ++i) {
    out(i) = viewState.computeScreenSpaceError(geIn(i), dIn(i));
  }
  return result;
}

py::list loadedConstTilesToList(
    const Cesium3DTilesSelection::LoadedConstTileEnumerator& enumerator) {
  py::list out;
  for (const Cesium3DTilesSelection::Tile& tile : enumerator) {
    out.append(py::cast(&tile, py::return_value_policy::reference));
  }
  return out;
}

py::list loadedTilesToList(
    const Cesium3DTilesSelection::LoadedTileEnumerator& enumerator) {
  py::list out;
  for (Cesium3DTilesSelection::Tile& tile : enumerator) {
    out.append(py::cast(&tile, py::return_value_policy::reference));
  }
  return out;
}

py::list tileChildrenToList(Cesium3DTilesSelection::Tile& tile) {
  py::list out;
  for (Cesium3DTilesSelection::Tile& child : tile.getChildren()) {
    out.append(py::cast(&child, py::return_value_policy::reference));
  }
  return out;
}

} // namespace

void initTiles3dSelectionBindings(py::module& m) {
  auto futureSampleHeightResult =
      py::class_<FutureSampleHeightResult>(m, "FutureSampleHeightResult");
  CesiumPython::bindFuture(futureSampleHeightResult);

  auto sharedFutureSampleHeightResult =
      py::class_<SharedFutureSampleHeightResult>(
          m,
          "SharedFutureSampleHeightResult");
  CesiumPython::bindSharedFuture(sharedFutureSampleHeightResult);

  auto futureTilesetMetadata =
      py::class_<FutureTilesetMetadataPtr>(m, "FutureTilesetMetadata");
  CesiumPython::bindFuture(
      futureTilesetMetadata,
      py::return_value_policy::reference);

  auto sharedFutureTilesetMetadata = py::class_<SharedFutureTilesetMetadataPtr>(
      m,
      "SharedFutureTilesetMetadata");
  CesiumPython::bindSharedFuture(
      sharedFutureTilesetMetadata,
      py::return_value_policy::reference);

  auto futureTilesetContentLoaderResult =
      py::class_<FutureTilesetContentLoaderResult>(
          m,
          "FutureTilesetContentLoaderResult");
  CesiumPython::bindFuture(futureTilesetContentLoaderResult);

  auto futureTileLoadResult =
      py::class_<FutureTileLoadResult>(m, "FutureTileLoadResult");
  CesiumPython::bindFuture(futureTileLoadResult);

  m.def(
      "create_resolved_future_tile_load_result",
      [](const CesiumAsync::AsyncSystem& asyncSystem,
         Cesium3DTilesSelection::TileLoadResult value) {
        return asyncSystem
            .createResolvedFuture<Cesium3DTilesSelection::TileLoadResult>(
                std::move(value));
      },
      py::arg("async_system"),
      py::arg("value"));

  m.def(
      "create_resolved_future_sample_height_result",
      [](const CesiumAsync::AsyncSystem& asyncSystem,
         Cesium3DTilesSelection::SampleHeightResult value) {
        return asyncSystem
            .createResolvedFuture<Cesium3DTilesSelection::SampleHeightResult>(
                std::move(value));
      },
      py::arg("async_system"),
      py::arg("value"));

  m.def(
      "create_tileset_content_loader_factory_from_callbacks",
      [](std::function<FutureTilesetContentLoaderResult(
             const Cesium3DTilesSelection::TilesetExternals&,
             const Cesium3DTilesSelection::TilesetOptions&,
             const Cesium3DTilesSelection::TilesetContentLoaderFactory::
                 AuthorizationHeaderChangeListener&)> createLoaderCallback,
         const py::object& isValidCallbackObj) {
        std::function<bool()> isValidCallback;
        if (!isValidCallbackObj.is_none()) {
          isValidCallback = isValidCallbackObj.cast<std::function<bool()>>();
        }
        return std::static_pointer_cast<
            Cesium3DTilesSelection::TilesetContentLoaderFactory>(
            std::make_shared<CallbackTilesetContentLoaderFactory>(
                std::move(createLoaderCallback),
                std::move(isValidCallback)));
      },
      py::arg("create_loader"),
      py::arg("is_valid") = py::none());

  py::class_<TilesetContentLoaderResult>(m, "TilesetContentLoaderResult")
      .def(py::init<>())
      .def(
          "has_loader",
          [](const TilesetContentLoaderResult& self) {
            return static_cast<bool>(self.pLoader);
          })
      .def(
          "has_root_tile",
          [](const TilesetContentLoaderResult& self) {
            return static_cast<bool>(self.pRootTile);
          })
      .def_property_readonly(
          "root_tile",
          [](TilesetContentLoaderResult& self) -> py::object {
            if (!self.pRootTile) {
              return py::none();
            }
            return py::cast(
                self.pRootTile.get(),
                py::return_value_policy::reference_internal);
          })
      .def_readwrite("credits", &TilesetContentLoaderResult::credits)
      .def_readwrite(
          "request_headers",
          &TilesetContentLoaderResult::requestHeaders)
      .def_readwrite("errors", &TilesetContentLoaderResult::errors)
      .def_readwrite("status_code", &TilesetContentLoaderResult::statusCode)
      .def_property_readonly(
          "loader",
          [](TilesetContentLoaderResult& self) -> py::object {
            if (!self.pLoader) {
              return py::none();
            }
            return py::cast(
                self.pLoader.get(),
                py::return_value_policy::reference_internal);
          });

  py::class_<Cesium3DTilesSelection::FogDensityAtHeight>(
      m,
      "FogDensityAtHeight")
      .def(py::init<>())
      .def_readwrite(
          "camera_height",
          &Cesium3DTilesSelection::FogDensityAtHeight::cameraHeight)
      .def_readwrite(
          "fog_density",
          &Cesium3DTilesSelection::FogDensityAtHeight::fogDensity);

  py::enum_<Cesium3DTilesSelection::GltfModifierState>(m, "GltfModifierState")
      .value("Idle", Cesium3DTilesSelection::GltfModifierState::Idle)
      .value(
          "WorkerRunning",
          Cesium3DTilesSelection::GltfModifierState::WorkerRunning)
      .value(
          "WorkerDone",
          Cesium3DTilesSelection::GltfModifierState::WorkerDone)
      .export_values();

  // GltfModifierInput / Output / GltfModifier
  py::class_<Cesium3DTilesSelection::GltfModifierInput>(m, "GltfModifierInput")
      .def_readonly(
          "version",
          &Cesium3DTilesSelection::GltfModifierInput::version)
      .def_property_readonly(
          "previous_model",
          [](const Cesium3DTilesSelection::GltfModifierInput& self) {
            return py::cast(
                &self.previousModel,
                py::return_value_policy::reference);
          })
      .def_property_readonly(
          "tile_transform",
          [](const Cesium3DTilesSelection::GltfModifierInput& self) {
            return toNumpy(self.tileTransform);
          });

  py::class_<Cesium3DTilesSelection::GltfModifierOutput>(
      m,
      "GltfModifierOutput")
      .def(py::init<>())
      .def_readwrite(
          "modified_model",
          &Cesium3DTilesSelection::GltfModifierOutput::modifiedModel);

  py::class_<PyGltfModifier, std::shared_ptr<PyGltfModifier>>(m, "GltfModifier")
      .def(py::init<>())
      .def("get_current_version", &PyGltfModifier::getCurrentVersion)
      .def("is_active", &PyGltfModifier::isActive)
      .def("trigger", &PyGltfModifier::trigger)
      .def(
          "apply",
          [](PyGltfModifier& /* self */,
             CesiumGltf::Model& /* model */,
             py::array_t<double> /* tile_transform */) -> py::object {
            throw py::type_error(
                "GltfModifier.apply() must be overridden in a subclass");
          },
          py::arg("model"),
          py::arg("tile_transform"),
          "Override in subclass: receive (Model, transform_4x4) and return a "
          "modified Model or None.");

  py::enum_<Cesium3DTilesSelection::TileOcclusionState>(m, "TileOcclusionState")
      .value(
          "OcclusionUnavailable",
          Cesium3DTilesSelection::TileOcclusionState::OcclusionUnavailable)
      .value(
          "NotOccluded",
          Cesium3DTilesSelection::TileOcclusionState::NotOccluded)
      .value("Occluded", Cesium3DTilesSelection::TileOcclusionState::Occluded)
      .export_values();

  py::enum_<Cesium3DTilesSelection::TileLoadPriorityGroup>(
      m,
      "TileLoadPriorityGroup")
      .value("Preload", Cesium3DTilesSelection::TileLoadPriorityGroup::Preload)
      .value("Normal", Cesium3DTilesSelection::TileLoadPriorityGroup::Normal)
      .value("Urgent", Cesium3DTilesSelection::TileLoadPriorityGroup::Urgent)
      .export_values();

  py::class_<Cesium3DTilesSelection::TileLoadTask>(m, "TileLoadTask")
      .def(py::init<>())
      .def_readwrite("tile", &Cesium3DTilesSelection::TileLoadTask::pTile)
      .def_readwrite("group", &Cesium3DTilesSelection::TileLoadTask::group)
      .def_readwrite(
          "priority",
          &Cesium3DTilesSelection::TileLoadTask::priority);

  py::class_<Cesium3DTilesSelection::BoundingVolume>(m, "BoundingVolume")
      .def_property_readonly(
          "center",
          [](const Cesium3DTilesSelection::BoundingVolume& self) {
            return toNumpy(
                Cesium3DTilesSelection::getBoundingVolumeCenter(self));
          })
      .def(
          "as_oriented_bounding_box",
          [](const Cesium3DTilesSelection::BoundingVolume& self,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return Cesium3DTilesSelection::
                getOrientedBoundingBoxFromBoundingVolume(self, ellipsoid);
          },
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84)
      .def(
          "transform",
          [](const Cesium3DTilesSelection::BoundingVolume& self,
             const py::handle& transform) {
            return Cesium3DTilesSelection::transformBoundingVolume(
                toDmat<4>(transform),
                self);
          },
          py::arg("transform"),
          "Transform this bounding volume by the given 4x4 matrix.")
      .def(
          "estimate_globe_rectangle",
          [](const Cesium3DTilesSelection::BoundingVolume& self,
             const CesiumGeospatial::Ellipsoid& ellipsoid) {
            return Cesium3DTilesSelection::estimateGlobeRectangle(
                self,
                ellipsoid);
          },
          py::arg("ellipsoid") = CesiumGeospatial::Ellipsoid::WGS84,
          "Estimate the bounding GlobeRectangle. Returns None if not "
          "estimable.")
      .def(
          "get_bounding_region",
          [](const Cesium3DTilesSelection::BoundingVolume& self)
              -> std::optional<CesiumGeospatial::BoundingRegion> {
            const auto* ptr =
                Cesium3DTilesSelection::getBoundingRegionFromBoundingVolume(
                    self);
            if (ptr)
              return *ptr;
            return std::nullopt;
          },
          "Get the BoundingRegion if this volume is one, else None.");

  py::class_<Cesium3DTilesSelection::TilesetContentOptions>(
      m,
      "TilesetContentOptions")
      .def(py::init<>())
      .def_readwrite(
          "enable_water_mask",
          &Cesium3DTilesSelection::TilesetContentOptions::enableWaterMask)
      .def_readwrite(
          "generate_missing_normals_smooth",
          &Cesium3DTilesSelection::TilesetContentOptions::
              generateMissingNormalsSmooth)
      .def_readwrite(
          "ktx2_transcode_targets",
          &Cesium3DTilesSelection::TilesetContentOptions::ktx2TranscodeTargets)
      .def_readwrite(
          "apply_texture_transform",
          &Cesium3DTilesSelection::TilesetContentOptions::
              applyTextureTransform);

  py::class_<Cesium3DTilesSelection::TileLoadInput>(m, "TileLoadInput")
      .def_property_readonly(
          "tile",
          [](const Cesium3DTilesSelection::TileLoadInput& self)
              -> const Cesium3DTilesSelection::Tile& { return self.tile; },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "content_options",
          [](const Cesium3DTilesSelection::TileLoadInput& self)
              -> const Cesium3DTilesSelection::TilesetContentOptions& {
            return self.contentOptions;
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "async_system",
          [](const Cesium3DTilesSelection::TileLoadInput& self)
              -> const CesiumAsync::AsyncSystem& { return self.asyncSystem; },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "asset_accessor",
          [](const Cesium3DTilesSelection::TileLoadInput& self) {
            return self.pAssetAccessor;
          })
      .def_property_readonly(
          "request_headers",
          [](const Cesium3DTilesSelection::TileLoadInput& self) {
            return self.requestHeaders;
          })
      .def_property_readonly(
          "ellipsoid",
          [](const Cesium3DTilesSelection::TileLoadInput& self)
              -> const CesiumGeospatial::Ellipsoid& { return self.ellipsoid; },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "shared_asset_system",
          [](const Cesium3DTilesSelection::TileLoadInput& self) {
            return self.pSharedAssetSystem;
          })
      .def_property_readonly(
          "has_logger",
          [](const Cesium3DTilesSelection::TileLoadInput& self) {
            return static_cast<bool>(self.pLogger);
          });

  py::class_<
      Cesium3DTilesSelection::IPrepareRendererResources,
      PyIPrepareRendererResources,
      std::shared_ptr<Cesium3DTilesSelection::IPrepareRendererResources>>(
      m,
      "IPrepareRendererResources")
      .def(py::init<>());

  py::class_<Cesium3DTilesSelection::TilesetExternals>(m, "TilesetExternals")
      .def(
          py::init([](const std::shared_ptr<CesiumAsync::IAssetAccessor>&
                          pAssetAccessor,
                      const std::shared_ptr<
                          Cesium3DTilesSelection::IPrepareRendererResources>&
                          pPrepare,
                      const CesiumAsync::AsyncSystem& asyncSystem,
                      const std::shared_ptr<CesiumUtility::CreditSystem>&
                          pCreditSystem) {
            return Cesium3DTilesSelection::TilesetExternals{
                pAssetAccessor,
                pPrepare,
                asyncSystem,
                pCreditSystem};
          }),
          py::arg("asset_accessor"),
          py::arg("prepare_renderer_resources"),
          py::arg("async_system"),
          py::arg("credit_system") = nullptr,
          // prevent Python GC of shared_ptr-wrapped args while this struct
          // lives
          py::keep_alive<1, 2>(), // asset_accessor
          py::keep_alive<1, 3>()) // prepare_renderer_resources
      .def_readwrite(
          "asset_accessor",
          &Cesium3DTilesSelection::TilesetExternals::pAssetAccessor)
      .def_readwrite(
          "prepare_renderer_resources",
          &Cesium3DTilesSelection::TilesetExternals::pPrepareRendererResources)
      .def_readwrite(
          "async_system",
          &Cesium3DTilesSelection::TilesetExternals::asyncSystem)
      .def_readwrite(
          "credit_system",
          &Cesium3DTilesSelection::TilesetExternals::pCreditSystem)
      .def_readwrite(
          "shared_asset_system",
          &Cesium3DTilesSelection::TilesetExternals::pSharedAssetSystem)
      .def_property(
          "gltf_modifier",
          [](const Cesium3DTilesSelection::TilesetExternals& self)
              -> py::object {
            if (!self.pGltfModifier)
              return py::none();
            // The actual type is always PyGltfModifier from Python.
            auto pyPtr =
                std::static_pointer_cast<PyGltfModifier>(self.pGltfModifier);
            return py::cast(pyPtr);
          },
          [](Cesium3DTilesSelection::TilesetExternals& self, py::object obj) {
            if (obj.is_none()) {
              self.pGltfModifier.reset();
            } else {
              auto pyPtr = obj.cast<std::shared_ptr<PyGltfModifier>>();
              self.pGltfModifier = std::static_pointer_cast<
                  Cesium3DTilesSelection::GltfModifier>(pyPtr);
            }
          });

  py::class_<Cesium3DTilesSelection::RasterOverlayCollection>(
      m,
      "RasterOverlayCollection")
      .def("size", &Cesium3DTilesSelection::RasterOverlayCollection::size)
      .def(
          "add",
          [](Cesium3DTilesSelection::RasterOverlayCollection& self,
             const CesiumUtility::IntrusivePointer<
                 CesiumRasterOverlays::RasterOverlay>& pOverlay) {
            self.add(pOverlay);
          },
          py::arg("overlay"),
          "Add a raster overlay to this tileset.")
      .def(
          "remove",
          [](Cesium3DTilesSelection::RasterOverlayCollection& self,
             const CesiumUtility::IntrusivePointer<
                 CesiumRasterOverlays::RasterOverlay>& pOverlay) {
            self.remove(pOverlay);
          },
          py::arg("overlay"),
          "Remove a raster overlay from this tileset.")
      .def(
          "__repr__",
          [](const Cesium3DTilesSelection::RasterOverlayCollection& self) {
            return "RasterOverlayCollection(size=" +
                   std::to_string(self.size()) + ")";
          })
      .def("__len__", &Cesium3DTilesSelection::RasterOverlayCollection::size)
      .def(
          "__iter__",
          [](const Cesium3DTilesSelection::RasterOverlayCollection& self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>())
      .def_property_readonly(
          "activated_overlays",
          [](const Cesium3DTilesSelection::RasterOverlayCollection& self) {
            return self.getActivatedOverlays();
          });

  py::class_<Cesium3DTilesSelection::TilesetOptions>(m, "TilesetOptions")
      .def(py::init<>())
      .def_readwrite("credit", &Cesium3DTilesSelection::TilesetOptions::credit)
      .def_readwrite(
          "show_credits_on_screen",
          &Cesium3DTilesSelection::TilesetOptions::showCreditsOnScreen)
      .def_readwrite(
          "maximum_screen_space_error",
          &Cesium3DTilesSelection::TilesetOptions::maximumScreenSpaceError)
      .def_readwrite(
          "maximum_simultaneous_tile_loads",
          &Cesium3DTilesSelection::TilesetOptions::maximumSimultaneousTileLoads)
      .def_readwrite(
          "preload_ancestors",
          &Cesium3DTilesSelection::TilesetOptions::preloadAncestors)
      .def_readwrite(
          "preload_siblings",
          &Cesium3DTilesSelection::TilesetOptions::preloadSiblings)
      .def_readwrite(
          "loading_descendant_limit",
          &Cesium3DTilesSelection::TilesetOptions::loadingDescendantLimit)
      .def_readwrite(
          "forbid_holes",
          &Cesium3DTilesSelection::TilesetOptions::forbidHoles)
      .def_readwrite(
          "enable_frustum_culling",
          &Cesium3DTilesSelection::TilesetOptions::enableFrustumCulling)
      .def_readwrite(
          "enable_occlusion_culling",
          &Cesium3DTilesSelection::TilesetOptions::enableOcclusionCulling)
      .def_readwrite(
          "delay_refinement_for_occlusion",
          &Cesium3DTilesSelection::TilesetOptions::delayRefinementForOcclusion)
      .def_readwrite(
          "enable_fog_culling",
          &Cesium3DTilesSelection::TilesetOptions::enableFogCulling)
      .def_readwrite(
          "enforce_culled_screen_space_error",
          &Cesium3DTilesSelection::TilesetOptions::
              enforceCulledScreenSpaceError)
      .def_readwrite(
          "culled_screen_space_error",
          &Cesium3DTilesSelection::TilesetOptions::culledScreenSpaceError)
      .def_readwrite(
          "maximum_cached_bytes",
          &Cesium3DTilesSelection::TilesetOptions::maximumCachedBytes)
      .def_readwrite(
          "fog_density_table",
          &Cesium3DTilesSelection::TilesetOptions::fogDensityTable)
      .def_readwrite(
          "render_tiles_under_camera",
          &Cesium3DTilesSelection::TilesetOptions::renderTilesUnderCamera)
      .def_readwrite(
          "enable_lod_transition_period",
          &Cesium3DTilesSelection::TilesetOptions::enableLodTransitionPeriod)
      .def_readwrite(
          "lod_transition_length",
          &Cesium3DTilesSelection::TilesetOptions::lodTransitionLength)
      .def_readwrite(
          "kick_descendants_while_fading_in",
          &Cesium3DTilesSelection::TilesetOptions::kickDescendantsWhileFadingIn)
      .def_readwrite(
          "main_thread_loading_time_limit",
          &Cesium3DTilesSelection::TilesetOptions::mainThreadLoadingTimeLimit)
      .def_readwrite(
          "tile_cache_unload_time_limit",
          &Cesium3DTilesSelection::TilesetOptions::tileCacheUnloadTimeLimit)
      .def_readwrite(
          "content_options",
          &Cesium3DTilesSelection::TilesetOptions::contentOptions)
      .def_readwrite(
          "ellipsoid",
          &Cesium3DTilesSelection::TilesetOptions::ellipsoid)
      .def_readwrite(
          "request_headers",
          &Cesium3DTilesSelection::TilesetOptions::requestHeaders)
      .def_readwrite(
          "excluders",
          &Cesium3DTilesSelection::TilesetOptions::excluders)
      .def_readwrite(
          "load_error_callback",
          &Cesium3DTilesSelection::TilesetOptions::loadErrorCallback)
      .def("__repr__", [](const Cesium3DTilesSelection::TilesetOptions& self) {
        return "TilesetOptions(max_sse=" +
               std::to_string(self.maximumScreenSpaceError) +
               ", max_cached_bytes=" + std::to_string(self.maximumCachedBytes) +
               ")";
      });

  py::class_<
      Cesium3DTilesSelection::ITileExcluder,
      std::shared_ptr<Cesium3DTilesSelection::ITileExcluder>,
      PyITileExcluder>(m, "ITileExcluder")
      .def(py::init<>())
      .def(
          "start_new_frame",
          &Cesium3DTilesSelection::ITileExcluder::startNewFrame)
      .def(
          "should_exclude",
          &Cesium3DTilesSelection::ITileExcluder::shouldExclude);

  py::class_<
      Cesium3DTilesSelection::RasterizedPolygonsTileExcluder,
      Cesium3DTilesSelection::ITileExcluder,
      std::shared_ptr<Cesium3DTilesSelection::RasterizedPolygonsTileExcluder>>(
      m,
      "RasterizedPolygonsTileExcluder")
      .def(
          py::init(
              [](const CesiumUtility::IntrusivePointer<
                  CesiumRasterOverlays::RasterizedPolygonsOverlay>& overlay) {
                CesiumUtility::IntrusivePointer<
                    const CesiumRasterOverlays::RasterizedPolygonsOverlay>
                    constOverlay = overlay;
                return std::make_shared<
                    Cesium3DTilesSelection::RasterizedPolygonsTileExcluder>(
                    constOverlay);
              }),
          py::arg("overlay"),
          "Create a tile excluder using a RasterizedPolygonsOverlay.");

  py::class_<
      Cesium3DTilesSelection::TilesetContentLoaderFactory,
      std::shared_ptr<Cesium3DTilesSelection::TilesetContentLoaderFactory>,
      PyTilesetContentLoaderFactory>(m, "TilesetContentLoaderFactory")
      .def(py::init<>())
      .def(
          "create_loader",
          &Cesium3DTilesSelection::TilesetContentLoaderFactory::createLoader,
          py::arg("externals"),
          py::arg("tileset_options"),
          py::arg("header_change_listener"))
      .def(
          "is_valid",
          &Cesium3DTilesSelection::TilesetContentLoaderFactory::isValid);

  py::class_<
      Cesium3DTilesSelection::ITilesetHeightSampler,
      std::shared_ptr<Cesium3DTilesSelection::ITilesetHeightSampler>,
      PyITilesetHeightSampler>(m, "ITilesetHeightSampler")
      .def(py::init<>())
      .def(
          "sample_heights",
          [](Cesium3DTilesSelection::ITilesetHeightSampler& self,
             const CesiumAsync::AsyncSystem& asyncSystem,
             py::array_t<double, py::array::c_style | py::array::forcecast>
                 positions) {
            auto view = CesiumPython::extractPoints(positions, 3);
            std::vector<CesiumGeospatial::Cartographic> carts;
            carts.reserve(static_cast<size_t>(view.rows));
            for (py::ssize_t i = 0; i < view.rows; ++i) {
              const double* row = view.data + i * 3;
              carts.emplace_back(row[0], row[1], row[2]);
            }
            return self.sampleHeights(asyncSystem, std::move(carts));
          },
          py::arg("async_system"),
          py::arg("positions"));

  py::class_<
      Cesium3DTilesSelection::TilesetContentLoader,
      std::shared_ptr<Cesium3DTilesSelection::TilesetContentLoader>>(
      m,
      "TilesetContentLoader")
      .def(
          "load_tile_content",
          &Cesium3DTilesSelection::TilesetContentLoader::loadTileContent,
          py::arg("input"))
      .def(
          "has_height_sampler",
          [](Cesium3DTilesSelection::TilesetContentLoader& self) {
            return self.getHeightSampler() != nullptr;
          })
      .def_property_readonly(
          "height_sampler",
          [](Cesium3DTilesSelection::TilesetContentLoader& self) -> py::object {
            Cesium3DTilesSelection::ITilesetHeightSampler* pSampler =
                self.getHeightSampler();
            if (!pSampler) {
              return py::none();
            }
            return py::cast(pSampler, py::return_value_policy::reference);
          })
      .def(
          "has_owner",
          [](const Cesium3DTilesSelection::TilesetContentLoader& self) {
            return self.getOwner() != nullptr;
          });

  py::class_<
      Cesium3DTilesSelection::CesiumIonTilesetContentLoaderFactory,
      Cesium3DTilesSelection::TilesetContentLoaderFactory,
      std::shared_ptr<
          Cesium3DTilesSelection::CesiumIonTilesetContentLoaderFactory>>(
      m,
      "CesiumIonTilesetContentLoaderFactory")
      .def(
          py::init<uint32_t, const std::string&, const std::string&>(),
          py::arg("ion_asset_id"),
          py::arg("ion_access_token"),
          py::arg("ion_asset_endpoint_url") = "https://api.cesium.com/");

  py::class_<
      Cesium3DTilesSelection::IModelMeshExportContentLoaderFactory,
      Cesium3DTilesSelection::TilesetContentLoaderFactory,
      std::shared_ptr<
          Cesium3DTilesSelection::IModelMeshExportContentLoaderFactory>>(
      m,
      "IModelMeshExportContentLoaderFactory")
      .def(
          py::init<
              const std::string&,
              const std::optional<std::string>&,
              const std::string&>(),
          py::arg("imodel_id"),
          py::arg("export_id") = std::nullopt,
          py::arg("itwin_access_token"));

  py::class_<
      Cesium3DTilesSelection::ITwinCesiumCuratedContentLoaderFactory,
      Cesium3DTilesSelection::TilesetContentLoaderFactory,
      std::shared_ptr<
          Cesium3DTilesSelection::ITwinCesiumCuratedContentLoaderFactory>>(
      m,
      "ITwinCesiumCuratedContentLoaderFactory")
      .def(
          py::init<uint32_t, const std::string&>(),
          py::arg("itwin_cesium_content_id"),
          py::arg("itwin_access_token"));

  py::class_<
      Cesium3DTilesSelection::ITwinRealityDataContentLoaderFactory,
      Cesium3DTilesSelection::TilesetContentLoaderFactory,
      std::shared_ptr<
          Cesium3DTilesSelection::ITwinRealityDataContentLoaderFactory>>(
      m,
      "ITwinRealityDataContentLoaderFactory")
      .def(
          py::init([](const std::string& realityDataId,
                      const std::optional<std::string>& iTwinId,
                      const std::string& iTwinAccessToken,
                      const CesiumAsync::AsyncSystem& asyncSystem,
                      py::function tokenRefreshCallback) {
            auto cb = [asyncSystem,
                       pyCallback = std::move(tokenRefreshCallback)](
                          const std::string& previousToken)
                -> CesiumAsync::Future<CesiumUtility::Result<std::string>> {
              py::gil_scoped_acquire gil;
              try {
                py::str result = pyCallback(previousToken);
                return asyncSystem.createResolvedFuture(
                    CesiumUtility::Result<std::string>(
                        result.cast<std::string>()));
              } catch (const std::exception& e) {
                return asyncSystem.createResolvedFuture(
                    CesiumUtility::Result<std::string>(
                        CesiumUtility::ErrorList::error(e.what())));
              }
            };
            return std::make_shared<
                Cesium3DTilesSelection::ITwinRealityDataContentLoaderFactory>(
                realityDataId,
                iTwinId,
                iTwinAccessToken,
                std::move(cb));
          }),
          py::arg("reality_data_id"),
          py::arg("itwin_id") = std::nullopt,
          py::arg("itwin_access_token"),
          py::arg("async_system"),
          py::arg("token_refresh_callback"));

  py::enum_<Cesium3DTilesSelection::TileSelectionState::Result>(
      m,
      "TileSelectionResult")
      .value("None", Cesium3DTilesSelection::TileSelectionState::Result::None)
      .value(
          "Culled",
          Cesium3DTilesSelection::TileSelectionState::Result::Culled)
      .value(
          "Rendered",
          Cesium3DTilesSelection::TileSelectionState::Result::Rendered)
      .value(
          "Refined",
          Cesium3DTilesSelection::TileSelectionState::Result::Refined)
      .value(
          "RenderedAndKicked",
          Cesium3DTilesSelection::TileSelectionState::Result::RenderedAndKicked)
      .value(
          "RefinedAndKicked",
          Cesium3DTilesSelection::TileSelectionState::Result::RefinedAndKicked)
      .export_values();

  py::class_<Cesium3DTilesSelection::TileSelectionState>(
      m,
      "TileSelectionState")
      .def(py::init<>())
      .def(
          py::init<Cesium3DTilesSelection::TileSelectionState::Result>(),
          py::arg("result"))
      .def_property_readonly(
          "result",
          &Cesium3DTilesSelection::TileSelectionState::getResult)
      .def("was_kicked", &Cesium3DTilesSelection::TileSelectionState::wasKicked)
      .def_property_readonly(
          "original_result",
          &Cesium3DTilesSelection::TileSelectionState::getOriginalResult)
      .def("kick", &Cesium3DTilesSelection::TileSelectionState::kick);

  py::enum_<Cesium3DTilesSelection::TilesetLoadType>(m, "TilesetLoadType")
      .value("Unknown", Cesium3DTilesSelection::TilesetLoadType::Unknown)
      .value("CesiumIon", Cesium3DTilesSelection::TilesetLoadType::CesiumIon)
      .value(
          "TilesetJson",
          Cesium3DTilesSelection::TilesetLoadType::TilesetJson)
      .export_values();

  py::class_<Cesium3DTilesSelection::TilesetLoadFailureDetails>(
      m,
      "TilesetLoadFailureDetails")
      .def(py::init<>())
      .def(
          "has_tileset",
          [](const Cesium3DTilesSelection::TilesetLoadFailureDetails& self) {
            return self.pTileset != nullptr;
          })
      .def_property_readonly(
          "tileset",
          [](const Cesium3DTilesSelection::TilesetLoadFailureDetails& self)
              -> py::object {
            if (!self.pTileset) {
              return py::none();
            }
            return py::cast(self.pTileset, py::return_value_policy::reference);
          })
      .def_readwrite(
          "type",
          &Cesium3DTilesSelection::TilesetLoadFailureDetails::type)
      .def_readwrite(
          "status_code",
          &Cesium3DTilesSelection::TilesetLoadFailureDetails::statusCode)
      .def_readwrite(
          "message",
          &Cesium3DTilesSelection::TilesetLoadFailureDetails::message);

  py::enum_<Cesium3DTilesSelection::TileRefine>(m, "TileRefine")
      .value("Add", Cesium3DTilesSelection::TileRefine::Add)
      .value("Replace", Cesium3DTilesSelection::TileRefine::Replace)
      .export_values();

  py::class_<Cesium3DTilesSelection::SampleHeightResult>(
      m,
      "SampleHeightResult")
      .def(py::init<>())
      .def_property_readonly(
          "positions",
          [](const Cesium3DTilesSelection::SampleHeightResult& self) {
            const auto& pos = self.positions;
            const py::ssize_t n = static_cast<py::ssize_t>(pos.size());
            py::array_t<double> arr({n, py::ssize_t(3)});
            auto out = arr.mutable_unchecked<2>();
            for (py::ssize_t i = 0; i < n; ++i) {
              out(i, 0) = pos[static_cast<size_t>(i)].longitude;
              out(i, 1) = pos[static_cast<size_t>(i)].latitude;
              out(i, 2) = pos[static_cast<size_t>(i)].height;
            }
            return arr;
          })
      .def_property_readonly(
          "sample_success",
          [](const Cesium3DTilesSelection::SampleHeightResult& self) {
            const auto& ss = self.sampleSuccess;
            const py::ssize_t n = static_cast<py::ssize_t>(ss.size());
            py::array_t<bool> arr(n);
            auto out = arr.mutable_unchecked<1>();
            for (py::ssize_t i = 0; i < n; ++i) {
              out(i) = ss[static_cast<size_t>(i)];
            }
            return arr;
          })
      .def_readwrite(
          "warnings",
          &Cesium3DTilesSelection::SampleHeightResult::warnings);

  py::class_<Cesium3DTilesSelection::LoaderCreditResult>(
      m,
      "LoaderCreditResult")
      .def(py::init<>())
      .def_readwrite(
          "credit_text",
          &Cesium3DTilesSelection::LoaderCreditResult::creditText)
      .def_readwrite(
          "show_on_screen",
          &Cesium3DTilesSelection::LoaderCreditResult::showOnScreen);

  py::class_<Cesium3DTilesSelection::TilesetMetadata>(m, "TilesetMetadata")
      .def(py::init<>())
      .def_readwrite("asset", &Cesium3DTilesSelection::TilesetMetadata::asset)
      .def_readwrite(
          "properties",
          &Cesium3DTilesSelection::TilesetMetadata::properties)
      .def_readwrite("schema", &Cesium3DTilesSelection::TilesetMetadata::schema)
      .def_readwrite(
          "schema_uri",
          &Cesium3DTilesSelection::TilesetMetadata::schemaUri)
      .def_readwrite(
          "statistics",
          &Cesium3DTilesSelection::TilesetMetadata::statistics)
      .def_readwrite("groups", &Cesium3DTilesSelection::TilesetMetadata::groups)
      .def_readwrite(
          "metadata",
          &Cesium3DTilesSelection::TilesetMetadata::metadata)
      .def_readwrite(
          "geometric_error",
          &Cesium3DTilesSelection::TilesetMetadata::geometricError)
      .def_readwrite(
          "extensions_used",
          &Cesium3DTilesSelection::TilesetMetadata::extensionsUsed)
      .def_readwrite(
          "extensions_required",
          &Cesium3DTilesSelection::TilesetMetadata::extensionsRequired)
      .def(
          "has_generic_extension",
          [](const Cesium3DTilesSelection::TilesetMetadata& self,
             const std::string& extension_name) {
            return self.getGenericExtension(extension_name) != nullptr;
          },
          py::arg("extension_name"))
      .def(
          "get_generic_extension",
          [](const Cesium3DTilesSelection::TilesetMetadata& self,
             const std::string& extension_name) -> py::object {
            const CesiumUtility::JsonValue* pValue =
                self.getGenericExtension(extension_name);
            if (!pValue) {
              return py::none();
            }
            return CesiumPython::jsonValueToPy(*pValue);
          },
          py::arg("extension_name"))
      .def(
          "load_schema_uri",
          &Cesium3DTilesSelection::TilesetMetadata::loadSchemaUri,
          py::arg("async_system"),
          py::arg("asset_accessor"),
          py::return_value_policy::reference_internal);

  py::class_<
      Cesium3DTilesSelection::TilesetSharedAssetSystem,
      CesiumGltfReader::GltfSharedAssetSystem,
      CesiumUtility::IntrusivePointer<
          Cesium3DTilesSelection::TilesetSharedAssetSystem>>(
      m,
      "TilesetSharedAssetSystem")
      .def_static(
          "get_default",
          &Cesium3DTilesSelection::TilesetSharedAssetSystem::getDefault);

  m.def(
      "create_tile_id_string",
      [](const std::string& tile_content_url) {
        Cesium3DTilesSelection::TileID tileId = tile_content_url;
        return Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
            tileId);
      },
      py::arg("tile_content_url"));

  m.def(
      "is_loadable_tile_id",
      [](const std::string& tile_content_url) {
        Cesium3DTilesSelection::TileID tileId = tile_content_url;
        return Cesium3DTilesSelection::TileIdUtilities::isLoadable(tileId);
      },
      py::arg("tile_content_url"));

  py::class_<Cesium3DTilesSelection::TileUnknownContent>(
      m,
      "TileUnknownContent")
      .def(py::init<>());

  py::class_<Cesium3DTilesSelection::TileEmptyContent>(m, "TileEmptyContent")
      .def(py::init<>());

  py::class_<Cesium3DTilesSelection::TileExternalContent>(
      m,
      "TileExternalContent")
      .def(py::init<>())
      .def_readwrite(
          "metadata",
          &Cesium3DTilesSelection::TileExternalContent::metadata);

  py::class_<Cesium3DTilesSelection::TileRenderContent>(m, "TileRenderContent")
      .def_property_readonly(
          "model",
          [](Cesium3DTilesSelection::TileRenderContent& self)
              -> CesiumGltf::Model& { return self.getModel(); },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "lod_transition_fade_percentage",
          &Cesium3DTilesSelection::TileRenderContent::
              getLodTransitionFadePercentage)
      .def_property_readonly(
          "render_resources",
          [](Cesium3DTilesSelection::TileRenderContent& self) -> py::object {
            void* ptr = self.getRenderResources();
            return PyIPrepareRendererResources::borrowPyObject(ptr);
          });

  py::class_<Cesium3DTilesSelection::TileContent>(m, "TileContent")
      .def(py::init<>())
      .def(
          py::init<Cesium3DTilesSelection::TileEmptyContent>(),
          py::arg("content"))
      .def(
          "set_unknown_content",
          [](Cesium3DTilesSelection::TileContent& self) {
            self.setContentKind(Cesium3DTilesSelection::TileUnknownContent{});
          })
      .def(
          "set_empty_content",
          [](Cesium3DTilesSelection::TileContent& self) {
            self.setContentKind(Cesium3DTilesSelection::TileEmptyContent{});
          })
      .def(
          "is_unknown_content",
          &Cesium3DTilesSelection::TileContent::isUnknownContent)
      .def(
          "is_empty_content",
          &Cesium3DTilesSelection::TileContent::isEmptyContent)
      .def(
          "is_external_content",
          &Cesium3DTilesSelection::TileContent::isExternalContent)
      .def(
          "is_render_content",
          &Cesium3DTilesSelection::TileContent::isRenderContent)
      .def_property_readonly(
          "render_content",
          [](Cesium3DTilesSelection::TileContent& self)
              -> Cesium3DTilesSelection::TileRenderContent* {
            return self.getRenderContent();
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "content_kind_name",
          [](const Cesium3DTilesSelection::TileContent& self) {
            if (self.isUnknownContent()) {
              return std::string("unknown");
            }
            if (self.isEmptyContent()) {
              return std::string("empty");
            }
            if (self.isExternalContent()) {
              return std::string("external");
            }
            if (self.isRenderContent()) {
              return std::string("render");
            }
            return std::string("unrecognized");
          });

  py::enum_<Cesium3DTilesSelection::TileLoadResultState>(
      m,
      "TileLoadResultState")
      .value("Success", Cesium3DTilesSelection::TileLoadResultState::Success)
      .value("Failed", Cesium3DTilesSelection::TileLoadResultState::Failed)
      .value(
          "RetryLater",
          Cesium3DTilesSelection::TileLoadResultState::RetryLater)
      .export_values();

  py::class_<Cesium3DTilesSelection::TileLoadResult>(m, "TileLoadResult")
      .def(py::init<>())
      .def_readwrite(
          "gltf_up_axis",
          &Cesium3DTilesSelection::TileLoadResult::glTFUpAxis)
      .def_readonly(
          "updated_bounding_volume",
          &Cesium3DTilesSelection::TileLoadResult::updatedBoundingVolume)
      .def_readonly(
          "updated_content_bounding_volume",
          &Cesium3DTilesSelection::TileLoadResult::updatedContentBoundingVolume)
      .def_readonly(
          "initial_bounding_volume",
          &Cesium3DTilesSelection::TileLoadResult::initialBoundingVolume)
      .def_readonly(
          "initial_content_bounding_volume",
          &Cesium3DTilesSelection::TileLoadResult::initialContentBoundingVolume)
      .def_readwrite(
          "asset_accessor",
          &Cesium3DTilesSelection::TileLoadResult::pAssetAccessor)
      .def_readwrite(
          "completed_request",
          &Cesium3DTilesSelection::TileLoadResult::pCompletedRequest)
      .def_readwrite("state", &Cesium3DTilesSelection::TileLoadResult::state)
      .def_readwrite(
          "ellipsoid",
          &Cesium3DTilesSelection::TileLoadResult::ellipsoid)
      .def_static(
          "create_failed_result",
          &Cesium3DTilesSelection::TileLoadResult::createFailedResult,
          py::arg("asset_accessor"),
          py::arg("completed_request"))
      .def_static(
          "create_retry_later_result",
          &Cesium3DTilesSelection::TileLoadResult::createRetryLaterResult,
          py::arg("asset_accessor"),
          py::arg("completed_request"))
      .def_property_readonly(
          "content_kind_name",
          [](const Cesium3DTilesSelection::TileLoadResult& self) {
            if (std::holds_alternative<
                    Cesium3DTilesSelection::TileUnknownContent>(
                    self.contentKind)) {
              return std::string("unknown");
            }
            if (std::holds_alternative<
                    Cesium3DTilesSelection::TileEmptyContent>(
                    self.contentKind)) {
              return std::string("empty");
            }
            if (std::holds_alternative<
                    Cesium3DTilesSelection::TileExternalContent>(
                    self.contentKind)) {
              return std::string("external");
            }
            if (std::holds_alternative<CesiumGltf::Model>(self.contentKind)) {
              return std::string("gltf_model");
            }
            return std::string("unrecognized");
          })
      .def_property_readonly(
          "model",
          [](Cesium3DTilesSelection::TileLoadResult& self) -> py::object {
            if (auto* pModel =
                    std::get_if<CesiumGltf::Model>(&self.contentKind)) {
              return py::cast(pModel, py::return_value_policy::reference);
            }
            return py::none();
          });

  py::enum_<Cesium3DTilesSelection::TileLoadState>(m, "TileLoadState")
      .value("Unloading", Cesium3DTilesSelection::TileLoadState::Unloading)
      .value(
          "FailedTemporarily",
          Cesium3DTilesSelection::TileLoadState::FailedTemporarily)
      .value("Unloaded", Cesium3DTilesSelection::TileLoadState::Unloaded)
      .value(
          "ContentLoading",
          Cesium3DTilesSelection::TileLoadState::ContentLoading)
      .value(
          "ContentLoaded",
          Cesium3DTilesSelection::TileLoadState::ContentLoaded)
      .value("Done", Cesium3DTilesSelection::TileLoadState::Done)
      .value("Failed", Cesium3DTilesSelection::TileLoadState::Failed)
      .export_values();

  py::class_<Cesium3DTilesSelection::Tile>(m, "Tile")
      .def_property_readonly(
          "parent",
          py::overload_cast<>(&Cesium3DTilesSelection::Tile::getParent),
          py::return_value_policy::reference)
      .def_property_readonly("children", &tileChildrenToList)
      .def_property_readonly(
          "child_count",
          [](const Cesium3DTilesSelection::Tile& self) {
            return self.getChildren().size();
          })
      .def_property_readonly(
          "bounding_volume",
          &Cesium3DTilesSelection::Tile::getBoundingVolume,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "viewer_request_volume",
          &Cesium3DTilesSelection::Tile::getViewerRequestVolume,
          py::return_value_policy::reference_internal)
      .def_property(
          "geometric_error",
          &Cesium3DTilesSelection::Tile::getGeometricError,
          &Cesium3DTilesSelection::Tile::setGeometricError)
      .def_property_readonly(
          "non_zero_geometric_error",
          &Cesium3DTilesSelection::Tile::getNonZeroGeometricError)
      .def_property(
          "unconditionally_refine",
          &Cesium3DTilesSelection::Tile::getUnconditionallyRefine,
          &Cesium3DTilesSelection::Tile::setUnconditionallyRefine)
      .def_property(
          "refine",
          &Cesium3DTilesSelection::Tile::getRefine,
          &Cesium3DTilesSelection::Tile::setRefine)
      .def_property(
          "transform",
          [](py::object self_obj) {
            auto& self = self_obj.cast<const Cesium3DTilesSelection::Tile&>();
            return toNumpyView(self.getTransform(), self_obj);
          },
          [](Cesium3DTilesSelection::Tile& self, const py::object& transform) {
            self.setTransform(toDmat<4>(transform));
          })
      .def_property(
          "tile_id_string",
          [](const Cesium3DTilesSelection::Tile& self) {
            return Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
                self.getTileID());
          },
          [](Cesium3DTilesSelection::Tile& self, const std::string& tileId) {
            self.setTileID(Cesium3DTilesSelection::TileID(tileId));
          })
      .def_property_readonly(
          "content_bounding_volume",
          &Cesium3DTilesSelection::Tile::getContentBoundingVolume,
          py::return_value_policy::reference_internal)
      .def("compute_byte_size", &Cesium3DTilesSelection::Tile::computeByteSize)
      .def_property_readonly(
          "content",
          py::overload_cast<>(&Cesium3DTilesSelection::Tile::getContent),
          py::return_value_policy::reference_internal)
      .def("is_renderable", &Cesium3DTilesSelection::Tile::isRenderable)
      .def("is_render_content", &Cesium3DTilesSelection::Tile::isRenderContent)
      .def(
          "is_external_content",
          &Cesium3DTilesSelection::Tile::isExternalContent)
      .def("is_empty_content", &Cesium3DTilesSelection::Tile::isEmptyContent)
      .def(
          "add_reference",
          [](const Cesium3DTilesSelection::Tile& self,
             const py::object& reason) {
            if (reason.is_none()) {
              self.addReference(nullptr);
            } else {
              std::string reasonText = py::cast<std::string>(reason);
              self.addReference(reasonText.c_str());
            }
          },
          py::arg("reason") = py::none())
      .def(
          "release_reference",
          [](const Cesium3DTilesSelection::Tile& self,
             const py::object& reason) {
            if (reason.is_none()) {
              self.releaseReference(nullptr);
            } else {
              std::string reasonText = py::cast<std::string>(reason);
              self.releaseReference(reasonText.c_str());
            }
          },
          py::arg("reason") = py::none())
      .def_property_readonly("state", &Cesium3DTilesSelection::Tile::getState)
      .def_property_readonly(
          "reference_count",
          &Cesium3DTilesSelection::Tile::getReferenceCount)
      .def(
          "has_referencing_content",
          &Cesium3DTilesSelection::Tile::hasReferencingContent)
      .def_property_readonly(
          "mapped_raster_tiles",
          py::overload_cast<>(
              &Cesium3DTilesSelection::Tile::getMappedRasterTiles),
          py::return_value_policy::reference_internal)
      .def("__repr__", [](const Cesium3DTilesSelection::Tile& self) {
        return "Tile(geometric_error=" +
               std::to_string(self.getGeometricError()) +
               ", children=" + std::to_string(self.getChildren().size()) +
               ", renderable=" +
               std::string(self.isRenderable() ? "True" : "False") + ")";
      });

  py::class_<Cesium3DTilesSelection::LoadedConstTileEnumerator>(
      m,
      "LoadedConstTileEnumerator")
      .def(
          py::init([](const py::object& root_tile) {
            const Cesium3DTilesSelection::Tile* pRoot = nullptr;
            if (!root_tile.is_none()) {
              pRoot = &py::cast<const Cesium3DTilesSelection::Tile&>(root_tile);
            }
            return Cesium3DTilesSelection::LoadedConstTileEnumerator(pRoot);
          }),
          py::arg("root_tile") = py::none())
      .def("to_list", &loadedConstTilesToList)
      .def(
          "__iter__",
          [](const Cesium3DTilesSelection::LoadedConstTileEnumerator& self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>());

  py::class_<Cesium3DTilesSelection::LoadedTileEnumerator>(
      m,
      "LoadedTileEnumerator")
      .def(
          py::init([](const py::object& root_tile) {
            Cesium3DTilesSelection::Tile* pRoot = nullptr;
            if (!root_tile.is_none()) {
              pRoot = &py::cast<Cesium3DTilesSelection::Tile&>(root_tile);
            }
            return Cesium3DTilesSelection::LoadedTileEnumerator(pRoot);
          }),
          py::arg("root_tile") = py::none())
      .def("to_list", &loadedTilesToList)
      .def(
          "__iter__",
          [](const Cesium3DTilesSelection::LoadedTileEnumerator& self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>());

  py::class_<Cesium3DTilesSelection::DebugTileStateDatabase>(
      m,
      "DebugTileStateDatabase")
      .def(py::init<const std::string&>(), py::arg("database_filename"))
      .def(
          "record_all_tile_states",
          &Cesium3DTilesSelection::DebugTileStateDatabase::recordAllTileStates,
          py::arg("frame_number"),
          py::arg("tileset"),
          py::arg("view_group"))
      .def(
          "record_tile_state",
          py::overload_cast<
              int32_t,
              const Cesium3DTilesSelection::TilesetViewGroup&,
              const Cesium3DTilesSelection::Tile&>(
              &Cesium3DTilesSelection::DebugTileStateDatabase::recordTileState),
          py::arg("frame_number"),
          py::arg("view_group"),
          py::arg("tile"));

  py::class_<Cesium3DTilesSelection::ViewUpdateResult>(m, "ViewUpdateResult")
      .def(py::init<>())
      .def_property_readonly(
          "tiles_to_render",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            py::list out;
            for (const auto& ptr : self.tilesToRenderThisFrame) {
              if (ptr)
                out.append(
                    py::cast(&(*ptr), py::return_value_policy::reference));
            }
            return out;
          },
          "Tiles selected for rendering this frame (list[Tile]).")
      .def_property_readonly(
          "tiles_fading_out",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            py::list out;
            for (const auto& ptr : self.tilesFadingOut) {
              if (ptr)
                out.append(
                    py::cast(&(*ptr), py::return_value_policy::reference));
            }
            return out;
          },
          "Tiles fading out from the previous LOD (list[Tile]).")
      .def_readwrite(
          "worker_thread_tile_load_queue_length",
          &Cesium3DTilesSelection::ViewUpdateResult::
              workerThreadTileLoadQueueLength)
      .def_readwrite(
          "main_thread_tile_load_queue_length",
          &Cesium3DTilesSelection::ViewUpdateResult::
              mainThreadTileLoadQueueLength)
      .def_readwrite(
          "tiles_visited",
          &Cesium3DTilesSelection::ViewUpdateResult::tilesVisited)
      .def_readwrite(
          "culled_tiles_visited",
          &Cesium3DTilesSelection::ViewUpdateResult::culledTilesVisited)
      .def_readwrite(
          "tiles_culled",
          &Cesium3DTilesSelection::ViewUpdateResult::tilesCulled)
      .def_readwrite(
          "tiles_occluded",
          &Cesium3DTilesSelection::ViewUpdateResult::tilesOccluded)
      .def_readwrite(
          "tiles_waiting_for_occlusion_results",
          &Cesium3DTilesSelection::ViewUpdateResult::
              tilesWaitingForOcclusionResults)
      .def_readwrite(
          "tiles_kicked",
          &Cesium3DTilesSelection::ViewUpdateResult::tilesKicked)
      .def_readwrite(
          "max_depth_visited",
          &Cesium3DTilesSelection::ViewUpdateResult::maxDepthVisited)
      .def_readwrite(
          "frame_number",
          &Cesium3DTilesSelection::ViewUpdateResult::frameNumber)
      .def_property_readonly(
          "tile_positions",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            const auto& tiles = self.tilesToRenderThisFrame;
            const auto n = static_cast<py::ssize_t>(tiles.size());
            py::array_t<double> out({n, py::ssize_t{3}});
            double* buf = static_cast<double*>(out.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                const auto& t = tiles[static_cast<size_t>(i)]->getTransform();
                buf[i * 3 + 0] = t[3][0];
                buf[i * 3 + 1] = t[3][1];
                buf[i * 3 + 2] = t[3][2];
              }
            }
            return out;
          },
          "(N,3) float64 translation column of each rendered tile's "
          "transform.")
      .def_property_readonly(
          "tile_bounding_volume_centers",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            const auto& tiles = self.tilesToRenderThisFrame;
            const auto n = static_cast<py::ssize_t>(tiles.size());
            py::array_t<double> out({n, py::ssize_t{3}});
            double* buf = static_cast<double*>(out.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                const auto center =
                    Cesium3DTilesSelection::getBoundingVolumeCenter(
                        tiles[static_cast<size_t>(i)]->getBoundingVolume());
                buf[i * 3 + 0] = center.x;
                buf[i * 3 + 1] = center.y;
                buf[i * 3 + 2] = center.z;
              }
            }
            return out;
          },
          "(N,3) float64 bounding-volume center for each rendered tile.")
      .def_property_readonly(
          "tile_geometric_errors",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            const auto& tiles = self.tilesToRenderThisFrame;
            const auto n = static_cast<py::ssize_t>(tiles.size());
            py::array_t<double> out(n);
            double* buf = static_cast<double*>(out.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                buf[i] = tiles[static_cast<size_t>(i)]->getGeometricError();
              }
            }
            return out;
          },
          "(N,) float64 geometric error for each rendered tile.")
      .def_property_readonly(
          "tile_transforms",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            const auto& tiles = self.tilesToRenderThisFrame;
            const auto n = static_cast<py::ssize_t>(tiles.size());
            py::array_t<double> out({n, py::ssize_t{4}, py::ssize_t{4}});
            double* buf = static_cast<double*>(out.mutable_data());
            {
              py::gil_scoped_release release;
              for (py::ssize_t i = 0; i < n; ++i) {
                const auto& t = tiles[static_cast<size_t>(i)]->getTransform();
                std::memcpy(buf + i * 16, &t[0][0], 16 * sizeof(double));
              }
            }
            return out;
          },
          "(N,4,4) float64 full transform matrix for each rendered tile.")
      .def(
          "__len__",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            return self.tilesToRenderThisFrame.size();
          })
      .def(
          "__iter__",
          [](const Cesium3DTilesSelection::ViewUpdateResult& self) {
            py::list out;
            for (const auto& ptr : self.tilesToRenderThisFrame) {
              if (ptr)
                out.append(
                    py::cast(&(*ptr), py::return_value_policy::reference));
            }
            return py::iter(out);
          });

  py::class_<Cesium3DTilesSelection::ViewState>(m, "ViewState")
      .def(
          py::init([](const py::object& position,
                      const py::object& direction,
                      const py::object& up,
                      const py::object& viewport_size,
                      double horizontalFieldOfView,
                      double verticalFieldOfView) {
            return Cesium3DTilesSelection::ViewState(
                toDvec<3>(position),
                toDvec<3>(direction),
                toDvec<3>(up),
                toDvec<2>(viewport_size),
                horizontalFieldOfView,
                verticalFieldOfView);
          }),
          py::arg("position"),
          py::arg("direction"),
          py::arg("up"),
          py::arg("viewport_size"),
          py::arg("horizontal_field_of_view"),
          py::arg("vertical_field_of_view"))
      .def(
          py::init([](const py::object& view_matrix,
                      const py::object& projection_matrix,
                      const py::object& viewport_size) {
            return Cesium3DTilesSelection::ViewState(
                toDmat<4>(view_matrix),
                toDmat<4>(projection_matrix),
                toDvec<2>(viewport_size));
          }),
          py::arg("view_matrix"),
          py::arg("projection_matrix"),
          py::arg("viewport_size"))
      .def_property_readonly(
          "position",
          [](py::object self_obj) {
            auto& self =
                self_obj.cast<const Cesium3DTilesSelection::ViewState&>();
            return toNumpyView(self.getPosition(), self_obj);
          })
      .def_property_readonly(
          "direction",
          [](py::object self_obj) {
            auto& self =
                self_obj.cast<const Cesium3DTilesSelection::ViewState&>();
            return toNumpyView(self.getDirection(), self_obj);
          })
      .def_property_readonly(
          "up",
          [](const Cesium3DTilesSelection::ViewState& self) {
            return toNumpy(self.getUp()); // getUp() returns by value; must copy
          })
      .def_property_readonly(
          "viewport_size",
          [](py::object self_obj) {
            auto& self =
                self_obj.cast<const Cesium3DTilesSelection::ViewState&>();
            return toNumpyView(self.getViewportSize(), self_obj);
          })
      .def_property_readonly(
          "horizontalFieldOfView",
          &Cesium3DTilesSelection::ViewState::getHorizontalFieldOfView)
      .def_property_readonly(
          "verticalFieldOfView",
          &Cesium3DTilesSelection::ViewState::getVerticalFieldOfView)
      .def_property_readonly(
          "view_matrix",
          [](py::object self_obj) {
            auto& self =
                self_obj.cast<const Cesium3DTilesSelection::ViewState&>();
            return toNumpyView(self.getViewMatrix(), self_obj);
          })
      .def_property_readonly(
          "projection_matrix",
          [](py::object self_obj) {
            auto& self =
                self_obj.cast<const Cesium3DTilesSelection::ViewState&>();
            return toNumpyView(self.getProjectionMatrix(), self_obj);
          })
      .def(
          "compute_screen_space_error",
          [](const Cesium3DTilesSelection::ViewState& self,
             const py::object& geometricError,
             const py::object& distance) -> py::object {
            if (py::isinstance<py::array>(geometricError) ||
                py::isinstance<py::array>(distance)) {
              return batchComputeScreenSpaceError(
                  self,
                  geometricError,
                  distance);
            }
            return py::cast(self.computeScreenSpaceError(
                py::cast<double>(geometricError),
                py::cast<double>(distance)));
          },
          py::arg("geometric_error"),
          py::arg("distance"))
      .def(
          "__repr__",
          [](const Cesium3DTilesSelection::ViewState& self) {
            const auto& p = self.getPosition();
            return "ViewState(position=[" + std::to_string(p.x) + ", " +
                   std::to_string(p.y) + ", " + std::to_string(p.z) +
                   "], hfov=" +
                   std::to_string(self.getHorizontalFieldOfView()) +
                   ", vfov=" + std::to_string(self.getVerticalFieldOfView()) +
                   ")";
          })
      .def(
          "is_bounding_volume_visible",
          &Cesium3DTilesSelection::ViewState::isBoundingVolumeVisible,
          py::arg("bounding_volume"))
      .def(
          "compute_distance_squared_to_bounding_volume",
          &Cesium3DTilesSelection::ViewState::
              computeDistanceSquaredToBoundingVolume,
          py::arg("bounding_volume"))
      .def_property_readonly(
          "position_cartographic",
          [](const Cesium3DTilesSelection::ViewState& self) -> py::object {
            const auto& cart = self.getPositionCartographic();
            if (!cart.has_value())
              return py::none();
            return py::cast(*cart);
          });

  py::class_<Cesium3DTilesSelection::TilesetViewGroup::LoadQueueCheckpoint>(
      m,
      "TilesetViewGroupLoadQueueCheckpoint")
      .def(py::init<>());

  py::class_<Cesium3DTilesSelection::TilesetFrameState>(m, "TilesetFrameState")
      .def_property_readonly(
          "view_group",
          [](Cesium3DTilesSelection::TilesetFrameState& self)
              -> Cesium3DTilesSelection::TilesetViewGroup& {
            return self.viewGroup;
          },
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "frustums",
          [](const Cesium3DTilesSelection::TilesetFrameState& self)
              -> const std::vector<Cesium3DTilesSelection::ViewState>& {
            return self.frustums;
          },
          py::return_value_policy::reference_internal)
      .def_readwrite(
          "fog_densities",
          &Cesium3DTilesSelection::TilesetFrameState::fogDensities);

  py::class_<
      Cesium3DTilesSelection::TileLoadRequester,
      std::unique_ptr<Cesium3DTilesSelection::TileLoadRequester, py::nodelete>>(
      m,
      "TileLoadRequester")
      .def("unregister", &Cesium3DTilesSelection::TileLoadRequester::unregister)
      .def(
          "is_registered",
          &Cesium3DTilesSelection::TileLoadRequester::isRegistered);

  py::class_<
      Cesium3DTilesSelection::TilesetViewGroup,
      Cesium3DTilesSelection::TileLoadRequester,
      std::unique_ptr<Cesium3DTilesSelection::TilesetViewGroup, py::nodelete>>(
      m,
      "TilesetViewGroup")
      .def(py::init<>())
      .def_property_readonly(
          "view_update_result",
          py::overload_cast<>(
              &Cesium3DTilesSelection::TilesetViewGroup::getViewUpdateResult),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "worker_thread_load_queue_length",
          &Cesium3DTilesSelection::TilesetViewGroup::
              getWorkerThreadLoadQueueLength)
      .def_property_readonly(
          "main_thread_load_queue_length",
          &Cesium3DTilesSelection::TilesetViewGroup::
              getMainThreadLoadQueueLength)
      .def_property_readonly(
          "previous_load_progress_percentage",
          &Cesium3DTilesSelection::TilesetViewGroup::
              getPreviousLoadProgressPercentage)
      .def_property(
          "weight",
          &Cesium3DTilesSelection::TilesetViewGroup::getWeight,
          &Cesium3DTilesSelection::TilesetViewGroup::setWeight)
      .def(
          "save_tile_load_queue_checkpoint",
          &Cesium3DTilesSelection::TilesetViewGroup::
              saveTileLoadQueueCheckpoint)
      .def(
          "restore_tile_load_queue_checkpoint",
          &Cesium3DTilesSelection::TilesetViewGroup::
              restoreTileLoadQueueCheckpoint,
          py::arg("checkpoint"))
      .def(
          "has_more_tiles_to_load_in_worker_thread",
          &Cesium3DTilesSelection::TilesetViewGroup::
              hasMoreTilesToLoadInWorkerThread)
      .def(
          "next_tile_to_load_in_worker_thread",
          &Cesium3DTilesSelection::TilesetViewGroup::
              getNextTileToLoadInWorkerThread,
          py::return_value_policy::reference)
      .def(
          "has_more_tiles_to_load_in_main_thread",
          &Cesium3DTilesSelection::TilesetViewGroup::
              hasMoreTilesToLoadInMainThread)
      .def(
          "next_tile_to_load_in_main_thread",
          &Cesium3DTilesSelection::TilesetViewGroup::
              getNextTileToLoadInMainThread,
          py::return_value_policy::reference)
      .def(
          "is_credit_referenced",
          &Cesium3DTilesSelection::TilesetViewGroup::isCreditReferenced,
          py::arg("credit"));

  // ── EllipsoidTilesetLoader ────────────────────────────────────────────────
  py::class_<Cesium3DTilesSelection::EllipsoidTilesetLoader>(
      m,
      "EllipsoidTilesetLoader")
      .def_static(
          "create_tileset",
          [](const Cesium3DTilesSelection::TilesetExternals& externals,
             const Cesium3DTilesSelection::TilesetOptions& options) {
            return Cesium3DTilesSelection::EllipsoidTilesetLoader::
                createTileset(externals, options);
          },
          py::arg("externals"),
          py::arg("options") = Cesium3DTilesSelection::TilesetOptions{},
          "Create a tileset from a procedural ellipsoid.",
          py::keep_alive<0, 1>()); // return value keeps externals alive

  py::class_<Cesium3DTilesSelection::Tileset>(m, "Tileset")

      .def(
          py::init([](const Cesium3DTilesSelection::TilesetExternals& externals,
                      const std::shared_ptr<
                          Cesium3DTilesSelection::TilesetContentLoaderFactory>&
                          pFactory,
                      const Cesium3DTilesSelection::TilesetOptions& options) {
            SharedPtrTilesetContentLoaderFactory factoryAdapter(pFactory);
            return std::make_unique<Cesium3DTilesSelection::Tileset>(
                externals,
                std::move(factoryAdapter),
                options);
          }),
          py::arg("externals"),
          py::arg("loader_factory"),
          py::arg("options") = Cesium3DTilesSelection::TilesetOptions(),
          py::keep_alive<1, 2>(), // Tileset keeps externals alive
          py::keep_alive<1, 3>()) // Tileset keeps loader_factory alive
      .def(
          py::init<
              const Cesium3DTilesSelection::TilesetExternals&,
              const std::string&,
              const Cesium3DTilesSelection::TilesetOptions&>(),
          py::arg("externals"),
          py::arg("url"),
          py::arg("options") = Cesium3DTilesSelection::TilesetOptions(),
          py::keep_alive<1, 2>()) // Tileset keeps externals alive
      .def(
          py::init<
              const Cesium3DTilesSelection::TilesetExternals&,
              int64_t,
              const std::string&,
              const Cesium3DTilesSelection::TilesetOptions&,
              const std::string&>(),
          py::arg("externals"),
          py::arg("ion_asset_id"),
          py::arg("ion_access_token"),
          py::arg("options") = Cesium3DTilesSelection::TilesetOptions(),
          py::arg("ion_asset_endpoint_url") = "https://api.cesium.com/",
          py::keep_alive<1, 2>()) // Tileset keeps externals alive
      .def_property_readonly(
          "async_system",
          py::overload_cast<>(&Cesium3DTilesSelection::Tileset::getAsyncSystem),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "root_tile_available_event",
          &Cesium3DTilesSelection::Tileset::getRootTileAvailableEvent,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "async_destruction_complete_event",
          &Cesium3DTilesSelection::Tileset::getAsyncDestructionCompleteEvent,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "options",
          py::overload_cast<>(&Cesium3DTilesSelection::Tileset::getOptions),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "ellipsoid",
          py::overload_cast<>(&Cesium3DTilesSelection::Tileset::getEllipsoid),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "user_credit",
          &Cesium3DTilesSelection::Tileset::getUserCredit)
      .def_property_readonly(
          "tileset_credits",
          &Cesium3DTilesSelection::Tileset::getTilesetCredits,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "credit_source",
          &Cesium3DTilesSelection::Tileset::getCreditSource,
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "shared_asset_system",
          py::overload_cast<>(
              &Cesium3DTilesSelection::Tileset::getSharedAssetSystem),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "externals",
          py::overload_cast<>(&Cesium3DTilesSelection::Tileset::getExternals),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "overlays",
          py::overload_cast<>(&Cesium3DTilesSelection::Tileset::getOverlays),
          py::return_value_policy::reference_internal)
      .def(
          "set_show_credits_on_screen",
          &Cesium3DTilesSelection::Tileset::setShowCreditsOnScreen,
          py::arg("show_credits_on_screen"))
      .def_property_readonly(
          "root_tile",
          &Cesium3DTilesSelection::Tileset::getRootTile,
          py::return_value_policy::reference)
      .def_property_readonly(
          "number_of_tiles_loaded",
          &Cesium3DTilesSelection::Tileset::getNumberOfTilesLoaded)
      .def(
          "compute_load_progress",
          &Cesium3DTilesSelection::Tileset::computeLoadProgress)
      .def_property_readonly(
          "total_data_bytes",
          &Cesium3DTilesSelection::Tileset::getTotalDataBytes)
      .def("load_metadata", &Cesium3DTilesSelection::Tileset::loadMetadata)
      .def(
          "sample_height_most_detailed",
          [](Cesium3DTilesSelection::Tileset& self,
             py::array_t<double, py::array::c_style | py::array::forcecast>
                 positions) {
            auto view = CesiumPython::extractPoints(positions, 3);
            std::vector<CesiumGeospatial::Cartographic> carts;
            carts.reserve(static_cast<size_t>(view.rows));
            for (py::ssize_t i = 0; i < view.rows; ++i) {
              const double* row = view.data + i * 3;
              carts.emplace_back(row[0], row[1], row[2]);
            }
            return self.sampleHeightMostDetailed(std::move(carts));
          },
          py::arg("positions"))
      .def(
          "load_tiles",
          &Cesium3DTilesSelection::Tileset::loadTiles,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "register_load_requester",
          &Cesium3DTilesSelection::Tileset::registerLoadRequester,
          py::arg("requester"))
      .def(
          "wait_for_all_loads_to_complete",
          &Cesium3DTilesSelection::Tileset::waitForAllLoadsToComplete,
          py::arg("maximum_wait_time_in_milliseconds"),
          py::call_guard<py::gil_scoped_release>())
      .def_property_readonly(
          "default_view_group",
          py::overload_cast<>(
              &Cesium3DTilesSelection::Tileset::getDefaultViewGroup),
          py::return_value_policy::reference_internal)
      .def(
          "update_view_group",
          &Cesium3DTilesSelection::Tileset::updateViewGroup,
          py::arg("view_group"),
          py::arg("frustums"),
          py::arg("delta_time") = 0.0f,
          py::return_value_policy::reference_internal,
          py::call_guard<py::gil_scoped_release>())
      .def(
          "update_view_group_offline",
          &Cesium3DTilesSelection::Tileset::updateViewGroupOffline,
          py::arg("view_group"),
          py::arg("frustums"),
          py::return_value_policy::reference_internal,
          py::call_guard<py::gil_scoped_release>())
      .def("loaded_tiles", &Cesium3DTilesSelection::Tileset::loadedTiles)
      .def(
          "for_each_loaded_tile",
          [](const Cesium3DTilesSelection::Tileset& self,
             py::function callback) {
            self.forEachLoadedTile(
                [&callback](const Cesium3DTilesSelection::Tile& tile) {
                  py::gil_scoped_acquire gil;
                  callback(py::cast(&tile, py::return_value_policy::reference));
                });
          },
          py::arg("callback"))
      .def_property_readonly(
          "metadata",
          [](const Cesium3DTilesSelection::Tileset& self) {
            return self.getMetadata(nullptr);
          },
          py::return_value_policy::reference)
      .def(
          "metadata_for_tile",
          [](const Cesium3DTilesSelection::Tileset& self,
             const Cesium3DTilesSelection::Tile& tile) {
            return self.getMetadata(&tile);
          },
          py::arg("tile"),
          py::return_value_policy::reference)
      .def(
          "summarize_loaded_tiles",
          [](const Cesium3DTilesSelection::Tileset& self) {
            py::list out;
            self.forEachLoadedTile([&out](
                                       const Cesium3DTilesSelection::Tile&
                                           tile) {
              py::dict entry;
              entry["tile_id"] =
                  Cesium3DTilesSelection::TileIdUtilities::createTileIdString(
                      tile.getTileID());
              entry["state"] = static_cast<int>(tile.getState());
              entry["is_renderable"] = tile.isRenderable();
              entry["is_render_content"] = tile.isRenderContent();
              entry["is_external_content"] = tile.isExternalContent();
              entry["geometric_error"] = tile.getGeometricError();
              out.append(entry);
            });
            return out;
          })
      .def(
          "__enter__",
          [](Cesium3DTilesSelection::Tileset& self)
              -> Cesium3DTilesSelection::Tileset& { return self; },
          py::return_value_policy::reference_internal)
      .def(
          "__exit__",
          [](Cesium3DTilesSelection::Tileset& /*self*/,
             py::object /*exc_type*/,
             py::object /*exc_val*/,
             py::object /*exc_tb*/) {
            // Tileset destructor handles async cleanup.
            // Nothing explicit needed here — Python ref-count drop
            // will trigger destruction. We just provide the protocol.
          });

  py::enum_<Cesium3DTilesSelection::RasterMappedTo3DTile::AttachmentState>(
      m,
      "RasterMappedTo3DTileAttachmentState")
      .value(
          "Unattached",
          Cesium3DTilesSelection::RasterMappedTo3DTile::AttachmentState::
              Unattached)
      .value(
          "TemporarilyAttached",
          Cesium3DTilesSelection::RasterMappedTo3DTile::AttachmentState::
              TemporarilyAttached)
      .value(
          "Attached",
          Cesium3DTilesSelection::RasterMappedTo3DTile::AttachmentState::
              Attached);

  py::class_<Cesium3DTilesSelection::RasterMappedTo3DTile>(
      m,
      "RasterMappedTo3DTile")
      .def_property_readonly(
          "loading_tile",
          py::overload_cast<>(
              &Cesium3DTilesSelection::RasterMappedTo3DTile::getLoadingTile,
              py::const_),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "ready_tile",
          py::overload_cast<>(
              &Cesium3DTilesSelection::RasterMappedTo3DTile::getReadyTile,
              py::const_),
          py::return_value_policy::reference_internal)
      .def_property_readonly(
          "texture_coordinate_id",
          &Cesium3DTilesSelection::RasterMappedTo3DTile::getTextureCoordinateID)
      .def_property_readonly(
          "translation",
          [](py::object self_obj) {
            auto& self = self_obj.cast<
                const Cesium3DTilesSelection::RasterMappedTo3DTile&>();
            return toNumpyView(self.getTranslation(), self_obj);
          })
      .def_property_readonly(
          "scale",
          [](py::object self_obj) {
            auto& self = self_obj.cast<
                const Cesium3DTilesSelection::RasterMappedTo3DTile&>();
            return toNumpyView(self.getScale(), self_obj);
          })
      .def_property_readonly(
          "state",
          &Cesium3DTilesSelection::RasterMappedTo3DTile::getState)
      .def(
          "is_more_detail_available",
          &Cesium3DTilesSelection::RasterMappedTo3DTile::isMoreDetailAvailable)
      .def(
          "load_throttled",
          &Cesium3DTilesSelection::RasterMappedTo3DTile::loadThrottled);
}
