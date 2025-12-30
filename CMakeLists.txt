cmake_minimum_required(VERSION 4.2)

project(LiquidGlass)

find_package(GrootEngine)

set(CMAKE_CXX_STANDARD 23)

set(INCLUDES
  ${CMAKE_SOURCE_DIR}/src/include/deletion_queue.hpp
  ${CMAKE_SOURCE_DIR}/src/include/random.hpp
  ${CMAKE_SOURCE_DIR}/src/include/ring_buffer.hpp
  ${CMAKE_SOURCE_DIR}/src/include/shader_manager.hpp
)

set(SOURCES
  ${CMAKE_SOURCE_DIR}/src/deletion_queue.cpp
  ${CMAKE_SOURCE_DIR}/src/main.cpp
  ${CMAKE_SOURCE_DIR}/src/random.cpp
  ${CMAKE_SOURCE_DIR}/src/ring_buffer.cpp
  ${CMAKE_SOURCE_DIR}/src/shader_manager.cpp
)

add_executable(run ${INCLUDES} ${SOURCES})

target_compile_definitions(run PRIVATE
  "SHADER_DIR=\"${CMAKE_SOURCE_DIR}/shaders\""
  "ASSET_DIR=\"${CMAKE_SOURCE_DIR}/assets\""
)

target_include_directories(run PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(run PRIVATE GrootEngine::groot)