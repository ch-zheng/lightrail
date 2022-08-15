#version 460

//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;

//Descriptors
layout(set=0, binding=0) uniform Uniforms {
	mat4 view;
	mat4 projection;
};
layout(set=0, binding=1) restrict readonly buffer NodeBuffer {
	mat4 transformations[];
};

//Outputs
layout(location=0) out float out_shade;

void main() {
	const vec4 pos = vec4(in_position, 1.0); //Model-space position
	const vec4 world_pos = transformations[gl_DrawID] * pos; //World-space position
	const vec4 cam_pos = view * world_pos; //Camera-space position
	const vec4 clip_pos = projection * cam_pos; //Clip-space position
	gl_Position = clip_pos;
	//Shading
	const vec4 eye = vec4(0.0, 0.0, 1.0, 0.0);
	const vec4 n = view * transformations[gl_DrawID] * vec4(in_normal, 0.0);
	out_shade = dot(eye, n) / length(n);
}
