#include "src/include/deletion_queue.hpp"

DeletionQueue::DeletionQueue(Engine& engine) : m_engine(engine) {}

void DeletionQueue::add(ResourceType type, RID resource) {
  m_resourceInfo.emplace_back(Info{
    .type     = type,
    .resource = resource,
    .frame    = 0
  });
}

void DeletionQueue::process_deletions() {
  std::vector<Info> resourceInfo;
  for (auto& info : m_resourceInfo) {
    if (info.frame++ != m_engine.flight_frames()) {
      resourceInfo.emplace_back(info);
      continue;
    }

    switch (info.type) {
      case ResourceType::StorageBuffer:
      case ResourceType::UniformBuffer:
        m_engine.destroy_buffer(info.resource);
        break;
      case ResourceType::Image:
        m_engine.destroy_image(info.resource);
        break;
      case ResourceType::Pipeline:
        m_engine.destroy_pipeline(info.resource);
        break;
      case ResourceType::DescriptorSet:
        m_engine.destroy_descriptor_set(info.resource);
      default: break;
    }
  }
  m_resourceInfo = resourceInfo;
}