#version 460

//Inputs
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_texCoords;
layout(location=2) in vec3 in_normal;

//Descriptors
layout(set=0, binding=1) readonly buffer TransformBuffer {
	mat4 transformations[];
};
layout(set=0, binding=2) readonly buffer TransformOffsetBuffer {
	uint transformation_offsets[];
};
layout(set=0, binding=3) uniform UBO {
	uint indexed_vertex_offset;
	uint indexed_transformation_offset;
};
layout(push_constant) uniform Constants {
	mat4 projection;
};

//Outputs
layout(location=0) out vec3 out_texCoords;

void main() {
	mat4 transformation = transformations[
		transformation_offsets[
			gl_DrawID + uint(gl_VertexIndex >= indexed_vertex_offset) * indexed_transformation_offset
		] + gl_InstanceIndex
	];
	gl_Position = projection * transformation * vec4(in_position, 1.0);
	out_texCoords = in_texCoords;
}
