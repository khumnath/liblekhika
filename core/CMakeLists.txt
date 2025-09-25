cmake_minimum_required(VERSION 3.16)

# Read version from version.txt
set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
if(NOT EXISTS ${VERSION_FILE})
    message(FATAL_ERROR "Version file not found at ${VERSION_FILE}")
endif()
file(STRINGS ${VERSION_FILE} PROJECT_VERSION LIMIT_COUNT 1)
if(NOT PROJECT_VERSION)
    message(FATAL_ERROR "Could not read version from ${VERSION_FILE}")
endif()
message(STATUS "Project version set to ${PROJECT_VERSION}")

project(liblekhika VERSION ${PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(SQLite3)

if(SQLite3_FOUND)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}, enabling dictionary.")
else()
    message(WARNING "SQLite3 not found, dictionary support will be disabled.")
endif()

# Library target
add_library(liblekhika SHARED
    src/lekhika_core.cpp
    include/liblekhika/lekhika_core.h
)

set_target_properties(liblekhika PROPERTIES
    PREFIX ""
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

target_include_directories(liblekhika PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_definitions(liblekhika PUBLIC "LEKHIKA_VERSION=\"${PROJECT_VERSION}\"")

target_link_libraries(liblekhika PUBLIC ICU::uc ICU::i18n)

if(SQLite3_FOUND)
    target_compile_definitions(liblekhika PUBLIC HAVE_SQLITE3)
    target_link_libraries(liblekhika PUBLIC SQLite::SQLite3)
endif()

# RPATH for relocatable builds
set_target_properties(liblekhika PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Install paths
set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(DATA_INSTALL_DIR share/liblekhika)
set(CMAKE_INSTALL_DIR lib/cmake/liblekhika)

# Install library and headers
install(TARGETS liblekhika
    EXPORT liblekhika-targets
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
)

install(DIRECTORY include/liblekhika/ DESTINATION ${INCLUDE_INSTALL_DIR}/liblekhika)
install(DIRECTORY data/ DESTINATION ${DATA_INSTALL_DIR})

# Export targets file (FIXED)
install(EXPORT liblekhika-targets
    FILE liblekhika-targets.cmake
    NAMESPACE liblekhika::
    DESTINATION ${CMAKE_INSTALL_DIR}
)

# Generate version file
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/liblekhika-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Generate config file
configure_package_config_file(
    "cmake/liblekhika-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/liblekhika-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/liblekhika-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/liblekhika-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DIR}
)
