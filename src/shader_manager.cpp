#include "src/include/shader_manager.hpp"

#include <format>

ShaderManager::ShaderManager(Engine& engine, const std::string& dir) : m_engine(engine), m_dir(dir) {}

RID& ShaderManager::operator[](const std::string& shader) {
  return m_shaders.at(shader);
}

void ShaderManager::compile(ShaderType type, const std::string& shader) {
  RID rid = m_engine.compile_shader(type, std::format("{}/{}", m_dir, shader));
  if (!rid.is_valid())
    Log::runtime_error(std::format("Failed to compile {}", shader));
  m_shaders.emplace(shader, rid);
}