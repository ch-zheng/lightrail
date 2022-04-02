#version 460
struct Light {
	vec3 light_pos;
	vec3 light_color;
};
//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec2 in_texCoords;
layout(location=2) in vec3 in_normal;
layout(location=3) in vec3 in_tangent;
layout(location=4) in vec3 in_bitangent;

//Descriptors
layout(push_constant) uniform Constants {
	mat4 camera;
};

layout(set = 0, binding=0) uniform LightSet {
	Light lights[16];
} light_set;

//Outputs
layout(location=0) out vec2 out_texCoords;
layout(location=1) out vec3 out_color;

void main() {
	gl_Position = camera * vec4(in_position, 1.0);
	out_texCoords = in_texCoords;
	out_color = light_set.lights[0].light_color;
	// out_color = ubo.in_color;
}
