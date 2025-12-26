#version 460

layout(location = 0) in vec3 _VertexPosition;
layout(location = 1) in vec2 _VertexUV;

layout(location = 0) out vec2 _UV;

void main() {
  gl_Position = vec4(_VertexPosition, 1.0);
  _UV = _VertexUV;
}