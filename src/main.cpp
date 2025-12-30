#include "groot/engine.hpp"
#include "groot/enums.hpp"
#include "src/include/shader_manager.hpp"
#include "src/include/ring_buffer.hpp"
#include "src/include/deletion_queue.hpp"
#include "src/include/random.hpp"

#include <groot/groot.hpp>

#include <format>
#include <future>

using namespace groot;

struct ShaderInfo {
  alignas(8) uvec2 dims;
  unsigned int blob_count;
  float blob_thickness;
  float liquidness;
  float blur_strength;
  alignas(16) vec3 chromatic_aberration;
};

struct PhysicsInfo {
  alignas(8) uvec2 dims;
  unsigned int blob_count;
  float delta_time;
  alignas(8) vec2 mouse_pos;
  unsigned int dragged_index;
  float friction;
  float tension_gamma;
  alignas(8) vec2 tension_bounds;
  float drag_spring;
};

struct Blob {
  float s;
  float r;
  alignas(16) vec3 col;
  alignas(8) vec2 pos;
  alignas(8) vec2 vel;
  alignas(8) vec2 accel;
};

struct State {
  RingBuffer& shader_info_buffer;
  RingBuffer& physics_info_buffer;
  RingBuffer& physics_set;
  RingBuffer& physics_pipeline;
  std::vector<Blob> blobs;
  RID blob_buffer;
  ShaderInfo shader_info;
  PhysicsInfo physics_info;
  std::vector<bool> updates_needed;
  bool is_dragging;
};

unsigned int get_dragged_blob(const Engine&, const std::vector<Blob>&);

int main() {
  static unsigned int g_max_blobs = 3;
  static Random random;

  Engine engine(Settings{
    .application_name = "Liquid Glass",
    .window_title     = "Groot Engine -- Liquid Glass",
    .render_mode      = RenderMode::VSync
  });

  ShaderManager shaders(engine, SHADER_DIR);
  shaders.compile(ShaderType::Vertex, "display.vert");
  shaders.compile(ShaderType::Fragment, "display.frag");
  shaders.compile(ShaderType::Compute, "glass.comp");
  shaders.compile(ShaderType::Compute, "physics.comp");

  DeletionQueue deletion_queue(engine);

  RID sampler = engine.create_sampler(SamplerSettings{ .anisotropic_filtering = false });
  RID background_texture = engine.create_texture(std::format("{}/background.jpg", ASSET_DIR), sampler);

  RID triangle_mesh = engine.load_mesh(std::format("{}/triangle.obj", ASSET_DIR));
  RID display_set = engine.create_descriptor_set({ background_texture });
  RID display_pipeline = engine.create_graphics_pipeline(GraphicsPipelineShaders{
    .vertex   = shaders["display.vert"],
    .fragment = shaders["display.frag"]
  }, display_set, GraphicsPipelineSettings{
    .cull_mode = CullMode::None
  });

  Object triangle;
  triangle.set_mesh(triangle_mesh);
  triangle.set_descriptor_set(display_set);
  triangle.set_pipeline(display_pipeline);

  engine.add_to_scene(triangle);

  RingBuffer shader_info_buffer(engine);
  for (auto& resource : shader_info_buffer)
    resource = engine.create_uniform_buffer(sizeof(ShaderInfo));

  RingBuffer physics_info_buffer(engine);
  for (auto& resource : physics_info_buffer)
    resource = engine.create_uniform_buffer(sizeof(PhysicsInfo));

  RingBuffer physics_set(engine);
  RingBuffer physics_pipeline(engine);

  auto [width, height] = engine.viewport_dims();
  State state{
    .shader_info_buffer   = shader_info_buffer,
    .physics_info_buffer  = physics_info_buffer,
    .physics_set          = physics_set,
    .physics_pipeline     = physics_pipeline,
    .shader_info = {
      .dims                 = uvec2(width, height),
      .blob_thickness       = 0.03f,
      .liquidness           = 0.3f,
      .blur_strength        = 0.04f,
      .chromatic_aberration = vec3(0.043f, 0.101f, 0.103f)
    },
    .physics_info = {
      .dims           = uvec2(width, height),
      .dragged_index  = 0xFFFFFFFFu,
      .friction       = 0.3f,
      .tension_gamma  = 0.04f,
      .tension_bounds = vec2(0.45f, 1.3f),
      .drag_spring    = 0.5f,
    }
  };

  state.updates_needed.resize(engine.flight_frames());

  engine.release_cursor();

  engine.run(
  [&engine, &state, &deletion_queue, &shaders](double dt){
    deletion_queue.process_deletions();

    if (engine.just_pressed(Key::Escape))
      engine.close_window();

    if (state.blob_buffer.is_valid())
      state.blobs = engine.read_buffer<Blob>(state.blob_buffer);

    float ar = static_cast<float>(state.shader_info.dims.x) / static_cast<float>(state.shader_info.dims.y);

    if (engine.just_pressed(Key::Space)) {
      state.blobs.emplace_back(Blob{
        .s = random(0.0f, 1.0f),
        .r = random(0.1f, 0.5f),
        .col = vec3(random(), random(), random()),
        .pos = vec2(random(-ar, ar), random(-1.0f, 1.0f)),
        .vel = vec2(0.0f),
        .accel = vec2(0.0f)
      });

      if (state.blob_buffer.is_valid())
        deletion_queue.add(ResourceType::StorageBuffer, state.blob_buffer);
      state.blob_buffer = engine.create_storage_buffer(sizeof(Blob) * state.blobs.size());
      engine.write_buffer(state.blob_buffer, state.blobs);

      for (int i = 0; i < engine.flight_frames(); ++i)
        state.updates_needed[i] = true;
    }

    if (state.blobs.empty()) return;

    if (engine.just_released(MouseButton::Left)) {
      state.physics_info.dragged_index = 0xFFFFFFFFu;
      state.is_dragging = false;
    }

    RID& set = *state.physics_set;
    RID& pipeline = *state.physics_pipeline;
    RID& shader_info = *state.shader_info_buffer;
    RID& physics_info = *state.physics_info_buffer;

    if (state.updates_needed[engine.frame_index()]) {
      if (set.is_valid())
        deletion_queue.add(ResourceType::DescriptorSet, set);
      set = engine.create_descriptor_set({ physics_info, state.blob_buffer });

      if (pipeline.is_valid())
        deletion_queue.add(ResourceType::Pipeline, pipeline);
      pipeline = engine.create_compute_pipeline(shaders["physics.comp"], set);

      state.updates_needed[engine.frame_index()] = false;
    }

    state.shader_info.blob_count = state.blobs.size();

    state.physics_info.delta_time = dt;
    state.physics_info.blob_count = state.blobs.size();

    vec2 dims = vec2(state.shader_info.dims.x, state.shader_info.dims.y);
    vec2 mouse_ndc = 2.0f * engine.mouse_pos() / dims - vec2(1.0f);
    mouse_ndc = mouse_ndc * vec2(dims.x / dims.y, -1.0);
    state.physics_info.mouse_pos = mouse_ndc;

    if (engine.is_pressed(MouseButton::Left) && !state.is_dragging) {
      unsigned int index = 0;
      for (const auto& blob : state.blobs) {
      vec2 p = mouse_ndc - blob.pos;

        float x2 = p.x * p.x;
        float y2 = p.y * p.y;
        float r2 = x2 + y2 + blob.s * blob.s / blob.r * blob.r * x2 * y2;

        float sdf = std::sqrt(r2) - blob.r;
        if (sdf > 0.0f) {
          ++index;
          continue;
        }

        state.physics_info.dragged_index = index;
        state.is_dragging = true;
        break;
      }
    }

    engine.write_buffer(shader_info, state.shader_info);
    engine.write_buffer(physics_info, state.physics_info);

    engine.dispatch(ComputeCommand{
      .pipeline       = pipeline,
      .descriptor_set = set,
      .work_groups    = { state.blobs.size(), 1, 1 }
    });
  },
  [&engine, &state, &deletion_queue, &shaders](double){
    if (state.blobs.empty()) return;

    RID set = engine.create_descriptor_set({
      engine.render_target(), *state.shader_info_buffer, state.blob_buffer
    });
    RID pipeline = engine.create_compute_pipeline(shaders["glass.comp"], set);

    unsigned int width = state.shader_info.dims.x;
    unsigned int height = state.shader_info.dims.y;

    engine.dispatch(ComputeCommand{
      .pipeline       = pipeline,
      .descriptor_set = set,
      .work_groups    = { (width + 15) / 16, (height + 15) / 16, 1 }
    });
  });
}

unsigned int get_dragged_blob(const Engine& engine, const std::vector<Blob>& blobs) {
  if (blobs.empty()) return 0xFFFFFFFFu;

  auto [width, height] = engine.viewport_dims();
  vec2 dims = vec2(width, height);



  return 0xFFFFFFFFu;
}