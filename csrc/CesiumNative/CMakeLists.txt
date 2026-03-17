set(CESIUM_PYTHON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ZipArchive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Bindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AsyncBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tiles3dBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tiles3dContentBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tiles3dReaderBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tiles3dSelectionBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Tiles3dWriterBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CurlBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeometryBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GeospatialBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfContentBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfReaderBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfWriterBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonReaderBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/JsonWriterBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/QuantizedMeshTerrainBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RasterOverlaysBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UtilityBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VectorDataBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ClientCommonBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/IonClientBindings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ITwinClientBindings.cpp)

pybind11_add_module(cesium_native ${CESIUM_PYTHON_SOURCES})
set_target_properties(cesium_native PROPERTIES OUTPUT_NAME _native)
target_include_directories(cesium_native PRIVATE
    ${CESIUM_PYTHON_INCLUDE_DIRS})
target_link_libraries(cesium_native PRIVATE ${CESIUM_PYTHON_LINK_LIBS})
target_compile_features(cesium_native PRIVATE cxx_std_20)
target_compile_definitions(cesium_native PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES)

if(MSVC)
    target_compile_options(cesium_native PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/PybindCommon.h")
else()
    target_compile_options(cesium_native PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/PybindCommon.h)
endif()

install(TARGETS cesium_native
    LIBRARY DESTINATION cesium
    RUNTIME DESTINATION cesium
    ARCHIVE DESTINATION cesium)
