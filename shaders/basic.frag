#version 460

//Inputs
layout(location=0) in vec2 in_tex;
layout(location=1) out uint in_material;
layout(location=2) in float in_shade;

//Descriptors
struct Material {
	vec4 base_color;
	float metallic_factor;
	float roughness_factor;
	uint base_color_tex;
	uint met_rgh_tex;
	uint normal_tex;
};
layout(std140, set=0, binding=2) restrict readonly buffer MaterialBuffer {
	Material materials[];
};
layout(set=0, binding=3) uniform sampler s;
layout(set=0, binding=4) uniform texture2D textures[8];

//Outputs
layout(location=0) out vec4 out_color;

void main() {
	//const Material material = materials[in_material];
	const vec4 s = texture(sampler2D(textures[3], s), in_tex); //FIXME: Indexed texture
	out_color = s;
}
