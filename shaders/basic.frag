#version 450
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;
layout(binding = 0) uniform usampler2D texSampler;
layout(location = 0) out vec4 outColor;

void main() {
	//outColor = vec4(fragColor, 1.0);
	outColor = texture(texSampler, texCoord);
}
