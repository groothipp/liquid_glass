#include "src/include/ring_buffer.hpp"

RingBuffer::RingBuffer(const Engine& engine) : m_engine(engine) {
  m_resources.resize(engine.flight_frames());
}

RID& RingBuffer::operator[](unsigned int index) {
  return m_resources[index];
}

RID& RingBuffer::operator*() {
  return m_resources[m_engine.frame_index()];
}

RingBuffer::Iterator RingBuffer::begin() {
  return m_resources.begin();
}

RingBuffer::Iterator RingBuffer::end() {
  return m_resources.end();
}