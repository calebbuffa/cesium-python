# cesium-python

Python bindings for [Cesium Native](https://github.com/CesiumGS/cesium-native), the C++ SDK for 3D geospatial. This lets you load and process 3D Tiles, work with geospatial math, and stream terrain and imagery data from Python without having write C++.

Note, some functionality should be implemented in C++, so I hve done my best to make `cesium` extensible. You can create your own extension and compile it to `cesium.my_extension`. One example is the `ITaskProcessor`. Implementing this in python (unless you can figure out a better way) tends to lose a lot of performance due to GIL acquisitoin. For that reason I added a `NativeTaskProcessor` that is just a Thread Pool implementation.

## What's included

The bindings cover the major Cesium Native modules:

| Module | What you get |
|---|---|
| `cesium.geospatial` | `Ellipsoid`, `Cartographic`, coordinate conversions, bounding regions, projections |
| `cesium.geometry` | Rays, planes, bounding boxes/spheres, oriented bounding boxes, frustums, culling |
| `cesium.tiles` | 3D Tiles loading, tile selection, `Tileset`, `TilesetOptions`, metadata schemas |
| `cesium.gltf` | glTF model access — meshes, buffers, materials, extensions, EXT_mesh_features |
| `cesium.gltf.reader` / `.writer` | Read and write glTF/GLB files |
| `cesium.tiles.reader` / `.writer` | Read and write 3D Tiles tilesets and tile content |
| `cesium.raster_overlays` | Drape imagery over terrain — URL templates, Bing, ion, rasterized polygons |
| `cesium.quantized_mesh_terrain` | Quantized-mesh terrain tile loading |
| `cesium.client.ion` | Cesium ion REST API client (assets, tokens, geocoder) |
| `cesium.client.itwin` | iTwin asset access |
| `cesium.async_` | `AsyncSystem`, `Future`, asset accessor plumbing |
| `cesium.json` | Low-level JSON reader/writer |
| `cesium.utility` | `Uri`, `ErrorList`, `JsonValue`, `CreditSystem`, extensions |

NumPy arrays are used throughout for positions, transforms, and bulk data — no custom vector types.

## Requirements

- Python 3.12+
- CMake 3.15+
- A C++17 compiler (MSVC 2019+, GCC 11+, Clang 13+)
- NumPy

On Windows the build system uses the Visual Studio x64 generator automatically.

## Building from source

```bash
git clone --recurse-submodules https://github.com/calebbuffa/cesium-python.git
cd cesium-python
pip install .
```

The first build is slow — Cesium Native and its dependencies (abseil, draco, s2geometry, etc.) all compile from source. Subsequent builds are incremental.

If you already have the repo cloned without submodules:

```bash
git submodule update --init --recursive
```

## Quick start

```python
import cesium

# Coordinate conversion
wgs84 = cesium.Ellipsoid.WGS84
cart = cesium.Cartographic(longitude=-1.3194, latitude=0.6983, height=0.0)
ecef = wgs84.cartographic_to_cartesian(cart)

# Load a tileset from Cesium ion
from cesium.async_ import AsyncSystem, IAssetAccessor
from cesium.tiles.selection import Tileset, TilesetOptions

async_system = AsyncSystem(...)
options = TilesetOptions()
tileset = Tileset.from_cesium_ion(async_system, asset_accessor, asset_id, ion_token, options, ...)
```

## Project layout

```
csrc/CesiumNative/   # pybind11 binding sources, one file per module
python/cesium/       # Python package with .pyi stubs
extern/cesium-native # Cesium Native submodule
extern/libzip        # libzip submodule (used for 3D Tiles zip/3tz support)
```

## License

Apache 2.0 — see [LICENSE](LICENSE).
