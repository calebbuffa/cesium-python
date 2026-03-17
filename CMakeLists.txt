cmake_minimum_required(VERSION 3.15)
project(cesium-python VERSION 0.1.0 LANGUAGES CXX)

include(cmake/Dependencies.cmake)
add_subdirectory(csrc)

# Clang-format target for code formatting
file(GLOB_RECURSE ALL_SOURCE_FILES
    "csrc/*.cpp"
    "csrc/*.h"
)

find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${ALL_SOURCE_FILES}
        COMMENT "Formatting code with clang-format (local .clang-format)"
    )

    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run -Werror ${ALL_SOURCE_FILES}
        COMMENT "Checking code formatting with clang-format (local .clang-format)"
    )
else()
    message(WARNING "clang-format not found. Format targets will not be available.")
endif()
