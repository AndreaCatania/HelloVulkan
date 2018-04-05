#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 textCoord;

layout(set = 2, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = texture(colorTexture, textCoord);
	//outColor = vec4(1,0,0,1); // Just a test
}
