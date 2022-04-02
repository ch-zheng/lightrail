#version 460

struct Material {
	int base_color_index;
	int emissive_index;
	int normal_map_index;
};

//Inputs
layout(location=0) in vec2 in_tex_coords;
layout(location=1) in vec3 in_color;

//Descriptors
layout(set=0, binding=1) uniform sampler tex_sampler;
layout(set=0, binding=2) uniform texture2D textures[128];

//Outputs
layout(location=0) out vec4 out_color;
layout(push_constant) uniform FragConstants {
	layout(offset=64) Material mat;
};

void main() {
	vec4 base_color = vec4(1.0, 1.0, 0.0, 1.0);
	if (mat.base_color_index >= 0) {
		base_color = texture(sampler2D(textures[mat.base_color_index], tex_sampler), in_tex_coords);
		if (base_color.w < .1) {
			discard;
		}
	}	
	if (base_color.w < .1) {
		discard;
	}
	if (mat.emissive_index >= 0) {
		vec4 emitted_color = texture(sampler2D(textures[mat.emissive_index], tex_sampler), in_tex_coords);
		if (emitted_color.w < .1) {
			discard;
		}
		base_color += 1.0 * base_color;
	}
	if (mat.normal_map_index >= 0) {
		// just a test to make sure normal maps are loaded
		base_color = texture(sampler2D(textures[mat.normal_map_index], tex_sampler), in_tex_coords);
	}
	out_color = vec4(in_color, 1.0);
}
