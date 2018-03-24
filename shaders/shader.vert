#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding=0) uniform SceneUniformBufferObject{
  mat4 cameraView;
  mat4 cameraViewInverse;
  mat4 cameraProjection;
} scene;

layout(set = 1, binding=0) uniform MeshUniformBufferObject {
  mat4 model;
} meshUBO;

layout(location=0) in vec3 vertexPosition;
layout(location=2) in vec2 inTextCoord;

layout(location=0) out vec2 textCoord;

out gl_PerVertex{
  vec4 gl_Position;
};

void main(){
  gl_Position = scene.cameraProjection * scene.cameraViewInverse * meshUBO.model * vec4(vertexPosition, 1.0);
  textCoord = inTextCoord;
}
