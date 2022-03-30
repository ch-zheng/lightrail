#version 460

//Inputs
layout(location=0) in vec2 in_tex_coords;
layout(location=1) in vec3 in_color;

//Descriptors
// layout(set=0, binding=1) uniform sampler2D tex_sampler;

//Outputs
layout(location=0) out vec4 out_color;

void main() {
	// out_color = 2.0 * texture(tex_sampler, in_tex_coords);
	out_color = vec4(1.0, 1.0, 0.0, 1.0);
}
