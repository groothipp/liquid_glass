#include <groot/groot.hpp>

#include <format>
#include <random>

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
  RID& physics_comp;
  RID& glass_comp;
  RID& blit_comp;
  RID& post_process_target;
  RID& shader_info_buffer;
  RID& physics_info_buffer;
  RID blobs_buffer;
  RID physics_set;
  RID physics_pipeline;
  PhysicsInfo& physics_info;
  ShaderInfo& shader_info;
  uvec2 dims;
  bool is_dragging = false;
  unsigned int drag_index = 0;
  std::vector<Blob> blobs;
};

class Random {
  std::random_device m_rd;
  std::mt19937_64 m_gen = std::mt19937_64(m_rd());
  std::uniform_real_distribution<float> m_dist = std::uniform_real_distribution<float>(0.0f, 1.0f);

  public:
    Random() = default;
    float operator()(float min = 0.0f, float max = 1.0f) { return (max - min) * m_dist(m_gen) + min; }
};

int main() {
  Engine engine(Settings{
    .application_name = "Liquid Glass",
    .window_title     = "Groot Engine -- Liquid Glass",
    .render_mode      = RenderMode::VSync
  });

  RID display_vert = engine.compile_shader(ShaderType::Vertex, std::format("{}/display.vert", SHADER_DIR));
  RID display_frag = engine.compile_shader(ShaderType::Fragment, std::format("{}/display.frag", SHADER_DIR));
  RID glass_comp = engine.compile_shader(ShaderType::Compute, std::format("{}/glass.comp", SHADER_DIR));
  RID blit_comp = engine.compile_shader(ShaderType::Compute, std::format("{}/blit.comp", SHADER_DIR));
  RID physics_comp = engine.compile_shader(ShaderType::Compute, std::format("{}/physics.comp", SHADER_DIR));

  std::string shader_errors = "";
  if (!display_vert.is_valid())
    shader_errors += "\n\tdisplay.vert";
  if (!display_frag.is_valid())
    shader_errors += "\n\tdisplay.frag";
  if (!glass_comp.is_valid())
    shader_errors += "\n\tglass.comp";
  if (!blit_comp.is_valid())
    shader_errors += "\n\tblit.comp";
  if (!physics_comp.is_valid())
    shader_errors += "\n\tphysics.comp";

  if (shader_errors != "")
    Log::runtime_error(std::format("errors compiling shaders:{}", shader_errors));

  auto [width, height] = engine.viewport_dims();

  RID sampler = engine.create_sampler(SamplerSettings{ .anisotropic_filtering = false });
  RID post_process_target = engine.create_storage_image(width, height, ImageType::two_dim, Format::rgba8_srgb);
  RID cloud_texture = engine.create_texture(std::format("{}/background.jpg", ASSET_DIR), sampler);
  RID shader_info_buffer = engine.create_uniform_buffer(sizeof(ShaderInfo));
  RID physics_info_buffer = engine.create_uniform_buffer(sizeof(PhysicsInfo));

  RID display_set = engine.create_descriptor_set({ cloud_texture });

  RID display_pipeline = engine.create_graphics_pipeline(GraphicsPipelineShaders{
    .vertex   = display_vert,
    .fragment = display_frag
  }, display_set, GraphicsPipelineSettings{
    .cull_mode = CullMode::None
  });

  RID triangle_mesh = engine.load_mesh(std::format("{}/triangle.obj", ASSET_DIR));

  Object triangle;
  triangle.set_mesh(triangle_mesh);
  triangle.set_pipeline(display_pipeline);
  triangle.set_descriptor_set(display_set);

  engine.add_to_scene(triangle);

  ShaderInfo shader_info{
    .dims           = uvec2(width, height),
    .blob_thickness = 0.034f,
    .liquidness     = 0.3f,
    .blur_strength  = 0.4f,
    .chromatic_aberration = vec3(0.009f, 0.0101f, 0.013f)
  };

  engine.release_cursor();

  PhysicsInfo physics_info{
    .dims           = uvec2(width, height),
    .dragged_index  = 0xFFFFFFFFu,
    .friction       = 0.3f,
    .tension_gamma  = 0.04f,
    .tension_bounds = vec2(0.45f, 1.3f),
    .drag_spring    = 0.5f
  };

  State state{
    .physics_comp         = physics_comp,
    .glass_comp           = glass_comp,
    .blit_comp            = blit_comp,
    .post_process_target  = post_process_target,
    .shader_info_buffer   = shader_info_buffer,
    .physics_info_buffer  = physics_info_buffer,
    .physics_info         = physics_info,
    .shader_info          = shader_info,
    .dims                 = uvec2(width, height)
  };

  engine.run([&engine, &state](double dt){
    static Random rand;

    if (engine.just_pressed(Key::Escape))
      engine.close_window();

    float ar = static_cast<float>(state.dims.x) / static_cast<float>(state.dims.y);

    if (engine.just_pressed(Key::Space)) {
      state.blobs.emplace_back(Blob{
        .s      = rand(0.0f, 1.0f),
        .r      = rand(0.1f, 0.267f),
        .col    = vec3(rand(), rand(), rand()),
        .pos    = vec2(rand(-ar, ar), rand(-1.0f, 1.0f)),
        .vel    = vec2(0.0f),
        .accel  = vec2(0.0f)
      });

      state.physics_info.blob_count += 1;
      state.shader_info.blob_count += 1;

      engine.write_buffer(state.shader_info_buffer, state.shader_info);

      if (state.blobs_buffer.is_valid())
        engine.destroy_buffer(state.blobs_buffer);
      state.blobs_buffer = engine.create_storage_buffer(sizeof(Blob) * state.blobs.size());
      engine.write_buffer(state.blobs_buffer, state.blobs);

      if (state.physics_set.is_valid())
        engine.destroy_descriptor_set(state.physics_set);
      state.physics_set = engine.create_descriptor_set({ state.physics_info_buffer, state.blobs_buffer });

      if (state.physics_pipeline.is_valid())
        engine.destroy_pipeline(state.physics_pipeline);
      state.physics_pipeline = engine.create_compute_pipeline(state.physics_comp, state.physics_set);
    }

    if (state.blobs.empty()) return;

    if (engine.just_released(MouseButton::Left)) {
      state.is_dragging = false;
      state.physics_info.dragged_index = 0xFFFFFFFFu;
    }

    vec2 mouse_pos = engine.mouse_pos();
    vec2 mouse_ndc = 2.0f * (mouse_pos / vec2(state.dims)) - vec2(1.0f);
    mouse_ndc.x *= ar;
    mouse_ndc.y *= -1.0f;

    PhysicsInfo& pi = state.physics_info;
    pi.mouse_pos = mouse_ndc;
    pi.delta_time = dt;

    state.blobs = engine.read_buffer<Blob>(state.blobs_buffer);

    if (engine.is_pressed(MouseButton::Left) && !state.is_dragging) {
      unsigned int index = 0;
      for (auto& blob : state.blobs) {
        vec2 p = mouse_ndc - blob.pos;

        float x2 = p.x * p.x;
        float y2 = p.y * p.y;
        float r2 = x2 + y2 + blob.s * blob.s / (blob.r * blob.r) * x2 * y2;

        float sdf = sqrt(r2) - blob.r;
        if (sdf <= 0.0f) {
          state.is_dragging = true;
          state.drag_index = index;
          pi.dragged_index = index;
          break;
        }

        ++index;
      }
    }

    engine.write_buffer(state.physics_info_buffer, pi);

    engine.compute_command(ComputeCommand{
      .pipeline = state.physics_pipeline,
      .descriptor_set = state.physics_set,
      .work_groups    = { state.blobs.size(), 1, 1 }
    });
    engine.dispatch();

    RID render_target = engine.render_target();

    RID glass_set = engine.create_descriptor_set(
      { render_target, state.post_process_target, state.shader_info_buffer, state.blobs_buffer }
    );
    RID blit_set = engine.create_descriptor_set({ render_target, state.post_process_target });

    RID glass_pipeline = engine.create_compute_pipeline(state.glass_comp, glass_set);
    RID blit_pipeline = engine.create_compute_pipeline(state.blit_comp, blit_set);

    std::tuple<unsigned int, unsigned int, unsigned int> work_groups = {
      (state.dims.x + 15) / 16, (state.dims.y + 15) / 16, 1
    };

    engine.compute_command(ComputeCommand{
      .pipeline       = glass_pipeline,
      .descriptor_set = glass_set,
      .work_groups    = work_groups,
      .post_process   = true
    });

    engine.compute_command(ComputeCommand{
      .pipeline       = blit_pipeline,
      .descriptor_set = blit_set,
      .work_groups    = work_groups,
      .barrier        = true,
      .post_process   = true
    });
  });
}