#version 460

//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_texCoords;
layout(location=2) in vec3 in_normal;

//Descriptors
layout(push_constant) uniform Constants {
	mat4 camera;
};

//Outputs
//layout(location=0) out vec3 out_texCoords;

void main() {
	gl_Position = camera * vec4(in_position, 1.0);
	//out_texCoords = in_texCoords;
}
