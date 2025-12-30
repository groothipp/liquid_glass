#pragma once

#include <random>

class Random {
  std::random_device m_rd;
  std::mt19937_64 m_gen = std::mt19937_64(m_rd());
  std::uniform_real_distribution<float> m_dist = std::uniform_real_distribution<float>(0.0f, 1.0f);

  public:
    Random() = default;
    float operator()(float min = 0.0f, float max = 1.0f);
};