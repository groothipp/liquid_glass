#include <groot/groot.hpp>

#include <format>

using namespace groot;

struct ShaderInfo {
  uvec2 dims;
  unsigned int blob_count;
  float blob_thickness;
  float liquidness;
};

struct Blob {
  float s;
  float r;
  vec2 pos;
};

struct Physics {
  vec2 a;
  vec2 v;
};

struct State {
  uvec2 dims;
  RID& glass_comp;
  RID& blit_comp;
  RID& post_process_target;
  RID& shader_info_buffer;
  RID& blobs_buffer;
  bool is_dragging = false;
  unsigned int drag_index = 0;
};

std::vector<Blob> g_blobs = {
  { 0.0f, 0.3f, vec2(0.0f, 0.5f) },
  { 1.0f, 0.3f, vec2(-0.8667f, -0.5f) },
  { 2.0f, 0.3f, vec2(0.8667f, -0.5f) }
};

std::vector<Physics> g_physics = {{}, {}, {}};

const float g_friction = 0.02f;
const float g_max_accel = 0.2f;

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

  std::string shader_errors = "";
  if (!display_vert.is_valid())
    shader_errors += "\n\tdisplay.vert";
  if (!display_frag.is_valid())
    shader_errors += "\n\tdisplay.frag";
  if (!glass_comp.is_valid())
    shader_errors += "\n\tglass.comp";
  if (!blit_comp.is_valid())
    shader_errors += "\n\tblit.comp";

  if (shader_errors != "")
    Log::runtime_error(std::format("errors compiling shaders:{}", shader_errors));

  auto [width, height] = engine.viewport_dims();

  RID sampler = engine.create_sampler(SamplerSettings{ .anisotropic_filtering = false });
  RID post_process_target = engine.create_storage_image(width, height, ImageType::two_dim, Format::rgba8_srgb);
  RID cloud_texture =engine.create_texture(std::format("{}/background.png", ASSET_DIR), sampler);
  RID shader_info_buffer = engine.create_uniform_buffer(sizeof(ShaderInfo));
  RID blobs_buffer = engine.create_storage_buffer(sizeof(Blob) * g_blobs.size());

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
    .blob_count     = static_cast<unsigned int>(g_blobs.size()),
    .blob_thickness = 0.034f,
    .liquidness     = 0.3f
  };

  engine.write_buffer(shader_info_buffer, shader_info);
  engine.write_buffer(blobs_buffer, g_blobs);

  engine.release_cursor();

  State state{
    .dims                 = uvec2(width, height),
    .glass_comp           = glass_comp,
    .blit_comp            = blit_comp,
    .post_process_target  = post_process_target,
    .shader_info_buffer   = shader_info_buffer,
    .blobs_buffer         = blobs_buffer
  };

  engine.run([&engine, &state](double dt){
    if (engine.just_pressed(Key::Escape))
      engine.close_window();

    if (engine.just_released(MouseButton::Left))
      state.is_dragging = false;

    float ar = static_cast<float>(state.dims.x) / static_cast<float>(state.dims.y);

    vec2 mouse_pos = engine.mouse_pos();
    vec2 mouse_ndc = 2.0f * (mouse_pos / vec2(state.dims)) - vec2(1.0f);
    mouse_ndc.x *= ar;
    mouse_ndc.y *= -1.0f;

    unsigned int index = 0;
    if (engine.is_pressed(MouseButton::Left) && !state.is_dragging) {
      for (auto& blob : g_blobs) {
        vec2 p = mouse_ndc - blob.pos;

        float x2 = p.x * p.x;
        float y2 = p.y * p.y;
        float R2 = x2 + y2 + blob.s * blob.s / (blob.r * blob.r) * x2 * y2;

        float sdf = std::sqrt(R2) - blob.r;
        if (sdf > 0.0)  {
          ++index;
          continue;
        }
        state.is_dragging = true;
        state.drag_index = index;
        break;
      }
    }

    if (state.is_dragging) {
      Blob& b = g_blobs[state.drag_index];
      Physics& phys = g_physics[state.drag_index];

      vec2 gamma = mouse_ndc - b.pos;
      phys.a = std::min(gamma.mag(), g_max_accel) * gamma.normalized() * dt;
    }

    bool update_buffer = false;
    for (unsigned int i = 0; i < g_blobs.size(); ++i) {
      Blob& b = g_blobs[i];
      Physics& phys = g_physics[i];

      vec2 dv = phys.a * dt;
      phys.v = phys.v + dv;

      if (phys.v.mag() > 0.0f) {
        vec2 v_dir = phys.v.normalized();
        float v_mag = phys.v.mag();
        phys.v = std::max(0.0f, v_mag - g_friction * static_cast<float>(dt)) * v_dir;
      }

      if (!state.is_dragging || state.drag_index != i)
        phys.a = vec2(0.0f);

      if (phys.v.mag() > 0.0f) {
        update_buffer = true;
        b.pos = b.pos + phys.v * dt;

        if (b.pos.x - b.r < -ar) b.pos.x = -ar + b.r;
        else if (b.pos.x + b.r > ar) b.pos.x = ar - b.r;

        if (b.pos.y - b.r < -1.0f) b.pos.y = -1.0f + b.r;
        if (b.pos.y + b.r > 1.0f) b.pos.y = 1.0f - b.r;
      }
    }

    if (update_buffer)
      engine.write_buffer(state.blobs_buffer, g_blobs);

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