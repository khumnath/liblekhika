# Main CMake script for the combined lekhika project

cmake_minimum_required(VERSION 3.16)
project(lekhika)

#  'liblekhika' library target.
add_subdirectory(core)

# The CLI tool can link directly to the 'liblekhika' target.
add_subdirectory(cli)

# project-wide uninstall target
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        COMMENT "Uninstalling the entire project (library and CLI)..."
    )
endif()

