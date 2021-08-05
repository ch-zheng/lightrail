#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;

layout(push_constant) uniform constants {
	mat4 projection;
} pushConstants;

void main() {
	gl_Position = pushConstants.projection * vec4(inPosition, 1.0);
	fragColor = inColor;
	texCoord = inTexCoord;
}
