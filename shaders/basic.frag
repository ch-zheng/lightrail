#version 460

//Inputs
layout(location=0) in float in_shade;

//Descriptors
//layout(set=0, binding=0) uniform sampler2D texSampler;

//Outputs
layout(location=0) out vec4 out_color;

void main() {
	//out_color = texture(texSampler, in_texCoords.st);
	out_color = in_shade * vec4(1.0, 0.0, 0.0, 1.0);
}
