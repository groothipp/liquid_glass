#pragma once

#include <groot/groot.hpp>

#include <vector>

using namespace groot;

class DeletionQueue {
  struct Info {
    ResourceType type;
    RID resource;
    unsigned int frame;
  };

  Engine& m_engine;
  std::vector<Info> m_resourceInfo;

  public:
    explicit DeletionQueue(Engine& engine);

    void add(ResourceType, RID);
    void process_deletions();
};