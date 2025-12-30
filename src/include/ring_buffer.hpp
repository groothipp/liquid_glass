#pragma once

#include <groot/groot.hpp>

#include <vector>

using namespace groot;

class RingBuffer {
  using Iterator = std::vector<RID>::iterator;

  const Engine& m_engine;
  std::vector<RID> m_resources;

  public:
    explicit RingBuffer(const Engine&);

    RID& operator[](unsigned int);
    RID& operator*();

    Iterator begin();
    Iterator end();
};