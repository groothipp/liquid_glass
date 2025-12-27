#version 460

layout(binding = 0) uniform sampler2D _BackgroundTexture;

layout(location = 0) in vec2 _UV;

layout(location = 0) out vec4 _FragColor;

void main() {
  vec3 albedo = texture(_BackgroundTexture, _UV).rgb;
  _FragColor = vec4(albedo, 1.0);
}