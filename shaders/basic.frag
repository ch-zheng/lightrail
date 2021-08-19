#version 450

//Inputs
layout(location=0) in vec3 in_texCoords;

//Descriptors
layout(set=0, binding=0) uniform sampler2D texSampler;

//Outputs
layout(location=0) out vec4 out_color;

void main() {
	out_color = texture(texSampler, in_texCoords.st);
	//out_color = vec4(1.0, 0.0, 0.0, 1.0); //DEBUG
	//out_color = vec4(in_texCoords.st, 1.0, 1.0); //DEBUG
}
