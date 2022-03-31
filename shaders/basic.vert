#version 460

//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_texCoords;
layout(location=2) in vec3 in_normal;

//Descriptors
layout(push_constant) uniform Constants {
	mat4 camera;
};

// layout(binding=0) uniform UniformStuff {
// 	vec3 in_color;
// } ubo;

//Outputs
layout(location=0) out vec2 out_texCoords;
layout(location=1) out vec3 out_color;

void main() {
	gl_Position = camera * vec4(in_position, 1.0);
	out_texCoords = in_texCoords;
	out_color = vec3(1.0, 1.0, 1.0);
	// out_color = ubo.in_color;
}
