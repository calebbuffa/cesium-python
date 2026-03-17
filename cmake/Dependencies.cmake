# Third-party dependency bootstrapping for cesium-python.

find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)

# Verify submodules are present
set(CESIUM_NATIVE_DIR "${CMAKE_SOURCE_DIR}/extern/cesium-native")
if(NOT EXISTS "${CESIUM_NATIVE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing submodule: extern/cesium-native. Run: git submodule update --init --recursive")
endif()

set(LIBZIP_DIR "${CMAKE_SOURCE_DIR}/extern/libzip")
if(NOT EXISTS "${LIBZIP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing submodule: extern/libzip. Run: git submodule update --init --recursive")
endif()


# Build all submodules as static libraries (no DLL at runtime).
# Use CACHE+FORCE to override stale values in an existing build directory.
if(DEFINED BUILD_SHARED_LIBS)
    set(_BUILD_SHARED_LIBS_ORIG ${BUILD_SHARED_LIBS})
endif()
cmake_policy(SET CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

set(LIBZIP_DO_INSTALL OFF CACHE BOOL "Skip libzip install")
set(ENABLE_BZIP2 OFF CACHE BOOL "Disable bzip2 support")
set(ENABLE_LZMA OFF CACHE BOOL "Disable lzma support")
set(ENABLE_ZSTD OFF CACHE BOOL "Disable zstd support")
add_subdirectory(extern/libzip EXCLUDE_FROM_ALL)

message(STATUS "Adding cesium-native subdirectory...")
add_subdirectory(extern/cesium-native EXCLUDE_FROM_ALL)

# glm::glm should be an imported target after cesium-native's ezvcpkg runs.
# However, when cmake is invoked without the vcpkg toolchain (e.g., via pip),
# the vcpkg toolchain is set only in the cache after project() has been called,
# so find_package(glm) inside cesium-native never actually creates the target.
# Fetch glm directly as a fallback — it is header-only so there is no conflict.
if(NOT TARGET glm::glm)
    include(FetchContent)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
        cmake_policy(SET CMP0135 NEW)
    endif()
    fetchcontent_declare(
        glm
        URL https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip
        URL_HASH SHA256=9a995de4da09723bd33ef194e6b79818950e5a8f2e154792f02e4615277cfb8d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    fetchcontent_makeavailable(glm)
endif()

if(DEFINED _BUILD_SHARED_LIBS_ORIG)
    set(BUILD_SHARED_LIBS ${_BUILD_SHARED_LIBS_ORIG} CACHE BOOL "Build shared libraries" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()

set(CESIUM_PYTHON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/extern/rapidjson/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/Cesium3DTilesWriter/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/Cesium3DTilesSelection/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumClientCommon/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumIonClient/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumITwinClient/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumQuantizedMeshTerrain/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumGltf/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumGltfWriter/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumAsync/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumCurl/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumGeometry/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumGeospatial/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumJsonWriter/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumUtility/include
    ${CMAKE_SOURCE_DIR}/extern/cesium-native/CesiumVectorData/include
)

set(CESIUM_PYTHON_LINK_LIBS
    zip
    Cesium3DTilesWriter
    Cesium3DTilesSelection
    CesiumClientCommon
    CesiumIonClient
    CesiumITwinClient
    CesiumQuantizedMeshTerrain
    CesiumGltf
    CesiumGltfWriter
    CesiumAsync
    CesiumCurl
    CesiumGeometry
    CesiumGeospatial
    CesiumJsonWriter
    CesiumUtility
    CesiumVectorData
    glm::glm
)
