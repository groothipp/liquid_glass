#pragma once

#include <groot/groot.hpp>

#include <string>
#include <unordered_map>

using namespace groot;

class ShaderManager {
  Engine& m_engine;
  const std::string m_dir;

  std::unordered_map<std::string, RID> m_shaders;

  public:
    ShaderManager(Engine& engine, const std::string& dir);

    RID& operator[](const std::string&);

    void compile(ShaderType, const std::string&);
};