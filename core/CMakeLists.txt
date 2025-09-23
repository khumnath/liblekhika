cmake_minimum_required(VERSION 3.16)

# Read version from file
set(VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")
if(NOT EXISTS ${VERSION_FILE})
    message(FATAL_ERROR "Version file not found at ${VERSION_FILE}. Please create it and add a version number (e.g., 0.1.0) on the first line.")
endif()

# This reads only the first line of the version file, allowing for comments on subsequent lines.
file(STRINGS "${VERSION_FILE}" FIRST_LINE LIMIT_COUNT 1)
string(STRIP "${FIRST_LINE}" PROJECT_VERSION) # Remove potential whitespace

if(NOT PROJECT_VERSION)
    message(FATAL_ERROR "Could not read version from ${VERSION_FILE}. Please ensure it contains a version number on the first line.")
endif()

project(lekhika-core VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ICU REQUIRED COMPONENTS uc i18n)
find_package(SQLite3)

add_library(liblekhika SHARED
    src/lekhika_core.cpp
)

# Pass version string to C++ code
# This makes the version number available at runtime.
target_compile_definitions(liblekhika PUBLIC "LEKHIKA_VERSION=\"${PROJECT_VERSION}\"")


if(SQLite3_FOUND)
    message(STATUS "Found SQLite3: ${SQLite3_VERSION}, enabling dictionary.")
    target_compile_definitions(liblekhika PUBLIC HAVE_SQLITE3)
else()
    message(WARNING "SQLite3 not found, dictionary support will be disabled.")
endif()

# library properties: version, soname, and prevent 'lib' prefix
set_target_properties(liblekhika PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PREFIX "" # This prevents CMake from adding the default 'lib' prefix to the filename
)

target_include_directories(liblekhika PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(liblekhika PUBLIC
    ${SQLite3_LIBRARIES}
    ICU::uc
    ICU::i18n
)

# Installation and Packaging
set(INCLUDE_INSTALL_DIR include)
set(LIB_INSTALL_DIR lib)
set(DATA_INSTALL_DIR share/lekhika-core)
set(CMAKE_INSTALL_DIR cmake/lekhika-core)

install(TARGETS liblekhika
    EXPORT lekhika-core-targets
    LIBRARY DESTINATION ${LIB_INSTALL_DIR}
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION ${INCLUDE_INSTALL_DIR}
)
install(DIRECTORY include/lekhika/ DESTINATION ${INCLUDE_INSTALL_DIR}/lekhika)
install(DIRECTORY data/ DESTINATION ${DATA_INSTALL_DIR})

include(CMakePackageConfigHelpers)

install(EXPORT lekhika-core-targets
    FILE lekhika-core-targets.cmake
    NAMESPACE liblekhika::
    DESTINATION ${CMAKE_INSTALL_DIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "cmake/lekhika-core-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/lekhika-core-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_DIR}
)

