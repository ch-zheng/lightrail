#version 460

//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;

//Descriptors
layout(set=0, binding=0) uniform Uniforms {
	mat4 camera;
};
layout(set=0, binding=1) restrict readonly buffer NodeBuffer {
	mat4 transformations[];
};

//Outputs
//layout(location=0) out vec3 out_texCoords;

void main() {
	gl_Position = camera * transformations[gl_DrawID] * vec4(in_position, 1.0);
	//out_texCoords = in_texCoords;
}
