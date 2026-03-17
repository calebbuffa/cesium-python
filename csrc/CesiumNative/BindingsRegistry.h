#pragma once

#include <pybind11/pybind11.h>

namespace py = pybind11;

void initAsyncBindings(py::module& m);
void initTiles3dBindings(py::module& m);
void initTiles3dContentBindings(py::module& m);
void initTiles3dReaderBindings(py::module& m);
void initTiles3dSelectionBindings(py::module& m);
void initTiles3dWriterBindings(py::module& m);
void initClientCommonBindings(py::module& m);
void initCurlBindings(py::module& m);
void initGeometryBindings(py::module& m);
void initGeospatialBindings(py::module& m);
void initGltfBindings(py::module& m);
void initGltfContentBindings(py::module& m);
void initGltfReaderBindings(py::module& m);
void initGltfWriterBindings(py::module& m);
void initIonClientBindings(py::module& m);
void initITwinClientBindings(py::module& m);
void initJsonReaderBindings(py::module& m);
void initJsonWriterBindings(py::module& m);
void initQuantizedMeshTerrainBindings(py::module& m);
void initRasterOverlaysBindings(py::module& m);
void initUtilityBindings(py::module& m);
void initVectorDataBindings(py::module& m);
