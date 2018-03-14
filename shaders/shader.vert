#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=0) uniform SceneUniformBufferObject{
  mat4 view;
  mat4 projection;
} sceneUBO;

layout(binding=1) uniform MeshUniformBufferObject {
  mat4 model;
} meshUBO;

layout(location=0) in vec2 inPosition;
layout(location=1) in vec4 inColor;

layout(location=0) out vec4 fragColor;

out gl_PerVertex{
  vec4 gl_Position;
};

void main(){
  gl_Position = sceneUBO.projection * sceneUBO.view * meshUBO.model * vec4(inPosition, 0.0, 1.0);
  fragColor = inColor;
}
