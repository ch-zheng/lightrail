#version 460

//Inputs
layout(location=0) in vec2 in_texCoords;
layout(location=1) in vec3 in_color;

//Descriptors
layout(set=0, binding=0) uniform sampler2D texSampler;

//Outputs
layout(location=0) out vec4 out_color;

void main() {
	//out_color = texture(texSampler, in_texCoords.st);
	out_color = vec4(in_color, 1.0);
}
