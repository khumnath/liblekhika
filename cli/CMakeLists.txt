# This subdirectory is part of a larger project.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable
add_executable(lekhika-cli lekhika_cli.cpp)

# Pass the source directory path to the C++ code for test mode.
get_filename_component(CORE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../" ABSOLUTE)
target_compile_definitions(lekhika-cli PRIVATE
    "LEKHIKA_SRC_DIR=\"${CORE_SRC_DIR}\""
)

# Link directly to the 'liblekhika' target.
target_link_libraries(lekhika-cli PRIVATE liblekhika)

# Installation rules
install(TARGETS lekhika-cli DESTINATION bin)

