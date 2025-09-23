# Main build script for the lekhika-core shared library
cmake_minimum_required(VERSION 3.16)

# Read the version from a file to keep it in one place.
set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
if(NOT EXISTS ${VERSION_FILE})
    message(FATAL_ERROR "Version file not found at ${VERSION_FILE}")
endif()
file(STRINGS ${VERSION_FILE} PROJECT_VERSION LIMIT_COUNT 1)
if(NOT PROJECT_VERSION)
    message(FATAL_ERROR "Could not read version from ${VERSION_FILE}")
endif()
message(STATUS "Project version set to ${PROJECT_VERSION}")

# Project name and version
project(lekhika-core VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#  Dependencies 
find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(SQLite3)
if(SQLite3_FOUND)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}, enabling dictionary.")
else()
    message(WARNING "SQLite3 not found, dictionary support will be disabled.")
endif()

# Library Target Definition 
add_library(liblekhika SHARED
    src/lekhika_core.cpp
)
# Do not add the "lib" prefix to the library file name.
set_target_properties(liblekhika PROPERTIES PREFIX "")

# Add versioning to the shared library
# This creates liblekhika.so.0.1.0, liblekhika.so.0, and liblekhika.so
set_target_properties(liblekhika PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Public Interface Configuration
target_include_directories(liblekhika PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
# Pass the project version to the source code.
target_compile_definitions(liblekhika PUBLIC "LEKHIKA_VERSION=\"${PROJECT_VERSION}\"")

# If SQLite3 was found, add the compile definition and link to it publicly.
if(SQLite3_FOUND)
    target_compile_definitions(liblekhika PUBLIC HAVE_SQLITE3)
    target_link_libraries(liblekhika PUBLIC ${SQLite3_LIBRARIES})
endif()

target_link_libraries(liblekhika PUBLIC
    ICU::uc
    #ICU::i18n
)

# Installation and Packaging 

set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(DATA_INSTALL_DIR share/lekhika-core)
set(CMAKE_INSTALL_DIR lib/cmake/lekhika-core)

# Install the library and headers
install(TARGETS liblekhika
    EXPORT lekhika-core-targets
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
)
install(DIRECTORY include/lekhika/ DESTINATION ${INCLUDE_INSTALL_DIR}/lekhika)

# Install data files
install(DIRECTORY data/ DESTINATION ${DATA_INSTALL_DIR})

#  CMake Package Configuration 
# Generate and install the package configuration files so other projects can find this library.
include(CMakePackageConfigHelpers)

# Generate the targets file
install(EXPORT lekhika-core-targets
    FILE lekhika-core-targets.cmake
    NAMESPACE liblekhika::
    DESTINATION ${CMAKE_INSTALL_DIR}
)

# Generate the version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Generate the main config file
configure_package_config_file(
    "cmake/lekhika-core-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
)

# Install the generated config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DIR}
)


