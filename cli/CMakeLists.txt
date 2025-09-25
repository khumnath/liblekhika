set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lekhika-cli lekhika_cli.cpp)

get_filename_component(CORE_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../" ABSOLUTE)
target_compile_definitions(lekhika-cli PRIVATE
    "LEKHIKA_SRC_DIR=\"${CORE_SRC_DIR}\""
)

target_link_libraries(lekhika-cli PRIVATE liblekhika)

install(TARGETS lekhika-cli DESTINATION bin)

