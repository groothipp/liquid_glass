cmake_minimum_required(VERSION 4.2)

project(LiquidGlass)

find_package(GrootEngine)

set(CMAKE_CXX_STANDARD 23)

add_executable(run ${CMAKE_SOURCE_DIR}/src/main.cpp)

target_compile_definitions(run PRIVATE
  "SHADER_DIR=\"${CMAKE_SOURCE_DIR}/shaders\""
  "ASSET_DIR=\"${CMAKE_SOURCE_DIR}/assets\""
)

target_link_libraries(run PRIVATE GrootEngine::groot)