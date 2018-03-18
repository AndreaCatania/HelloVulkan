#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding=0) uniform SceneUniformBufferObject{
  mat4 cameraView;
  mat4 cameraProjection;
} scene;

layout(set = 1, binding=0) uniform MeshUniformBufferObject {
  mat4 model;
} meshUBO;

layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec4 vertexColor;

layout(location=0) out vec4 fragColor;

out gl_PerVertex{
  vec4 gl_Position;
};

void main(){
  gl_Position = scene.cameraProjection * scene.cameraView * meshUBO.model * vec4(vertexPosition, 1.0);
  fragColor = vertexColor;
}
