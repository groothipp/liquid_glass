#include "src/include/random.hpp"

float Random::operator()(float min, float max) {
  return (max - min) * m_dist(m_gen) + min;
}