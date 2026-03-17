# Third-party dependency bootstrapping for cesium-python.

# Ensure zlib is available before libzip config.
find_package(ZLIB 1.1.2 QUIET)
if(NOT ZLIB_FOUND AND CESIUM_PYTHON_FETCH_ZLIB)
    include(FetchContent)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
        cmake_policy(SET CMP0135 NEW)
    endif()
    FetchContent_Declare(
        zlib
        URL https://zlib.net/fossils/zlib-1.3.1.tar.gz
        URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(zlib)
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
        set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}")
        set(ZLIB_LIBRARY zlibstatic)
        set(ZLIB_LIBRARIES zlibstatic)
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
        set(ZLIB_INCLUDE_DIR "${zlib_SOURCE_DIR}")
        set(ZLIB_LIBRARY zlib)
        set(ZLIB_LIBRARIES zlib)
    endif()
endif()

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

if(DEFINED _BUILD_SHARED_LIBS_ORIG)
    set(BUILD_SHARED_LIBS ${_BUILD_SHARED_LIBS_ORIG} CACHE BOOL "Build shared libraries" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()

# Base toolchain and package dependencies.
find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)
if(NOT TARGET glm::glm)
    find_package(glm CONFIG REQUIRED)
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
