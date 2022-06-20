#define CGLTF_IMPLEMENTATION
#include "scene.h"
#include "cgltf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool load_scene(const char* const filename, struct Scene* output) {
	cgltf_options options = {};
	cgltf_data* data;
	cgltf_result result = cgltf_parse_file(&options, filename, &data);
	if (result == cgltf_result_success) {
		result = cgltf_load_buffers(&options, data, filename); //TODO: Error handling
		const struct Scene scene = {
			data->meshes_count,
			malloc(data->meshes_count * sizeof(struct Mesh)),
			data->nodes_count,
			malloc(data->nodes_count * sizeof(struct Node))
		};
		//Load meshes
		for (unsigned i = 0; i < data->meshes_count; ++i) {
			const cgltf_mesh gltf_mesh = data->meshes[i];
			const struct Mesh mesh = {
				gltf_mesh.primitives_count,
				malloc(gltf_mesh.primitives_count * sizeof(struct Primitive))
			};
			//Load primitives
			for (unsigned i = 0; i < gltf_mesh.primitives_count; ++i) {
				const cgltf_primitive gltf_primitive = gltf_mesh.primitives[i];
				struct Primitive primitive = {0, NULL, 0, NULL};
				//Load indices
				const cgltf_accessor indices = *gltf_primitive.indices;
				primitive.index_count = indices.count;
				primitive.indices = malloc(indices.count * sizeof(unsigned));
				for (unsigned i = 0; i < indices.count; ++i)
					primitive.indices[i] = cgltf_accessor_read_index(&indices, i);
				//Load vertices
				const unsigned vertex_count = gltf_primitive.attributes[0].data->count;
				primitive.vertex_count = vertex_count;
				primitive.vertices = calloc(vertex_count, sizeof(struct Primitive));
				//Load vertex attributes
				for (unsigned i = 0; i < gltf_primitive.attributes_count; ++i) {
					const cgltf_attribute attribute = gltf_primitive.attributes[i];
					const unsigned float_count = cgltf_accessor_unpack_floats(attribute.data, NULL, 0);
					float* const buffer = malloc(float_count * sizeof(float));
					cgltf_accessor_unpack_floats(attribute.data, buffer, float_count);
					const unsigned element_size = cgltf_num_components(attribute.data->type);
					switch (attribute.type) {
						case cgltf_attribute_type_position:
							{
								const unsigned components = element_size < 3 ? element_size : 3;
								for (unsigned i = 0; i < vertex_count; ++i) {
									const unsigned offset = i * element_size;
									for (unsigned j = 0; j < components; ++j)
										primitive.vertices[i].pos[j] = buffer[offset + j];
								}
							}
							break;
						case cgltf_attribute_type_normal:
							{
								const unsigned components = element_size < 3 ? element_size : 3;
								for (unsigned i = 0; i < vertex_count; ++i) {
									const unsigned offset = i * element_size;
									for (unsigned j = 0; j < components; ++j)
										primitive.vertices[i].normal[j] = buffer[offset + j];
								}
							}
							break;
						default:
							break;
					}
					free(buffer);
				}
				mesh.primitives[i] = primitive;
			}
			scene.meshes[i] = mesh;
		}
		//Load nodes
		for (unsigned i = 0; i < data->nodes_count; ++i) {
			const cgltf_node gltf_node = data->nodes[i];
			struct Node node = {
				gltf_node.children_count,
				malloc(gltf_node.children_count * sizeof(unsigned)),
				gltf_node.mesh,
				gltf_node.mesh ? gltf_node.mesh - data->meshes : 0,
				{gltf_node.translation[0], gltf_node.translation[1], gltf_node.translation[2]},
				{gltf_node.rotation[0], gltf_node.rotation[1], gltf_node.rotation[2], gltf_node.rotation[3]},
				{gltf_node.scale[0], gltf_node.scale[1], gltf_node.scale[2]},
				true
			};
			//Child nodes
			for (unsigned i = 0; i < node.child_count; ++i)
				node.children[i] = gltf_node.children[i] - data->nodes;
			//World transformation
			cgltf_node_transform_world(data->nodes + i, (cgltf_float*) node.transformation);
			scene.nodes[i] = node;
		}
		//Finish
		cgltf_free(data);
		*output = scene;
		return false;
	} else return true;
}

void scene_update_transformations(struct Scene* scene) {
	//Find root nodes
	bool* const root_mask = malloc(scene->node_count * sizeof(bool));
	memset(root_mask, true, scene->node_count);
	for (unsigned i = 0; i < scene->node_count; ++i) {
		const struct Node node = scene->nodes[i];
		for (unsigned i = 0; i < node.child_count; ++i)
			root_mask[node.children[i]] = false;
	}
	unsigned root_count = 0;
	for (unsigned i = 0; i < scene->node_count; ++i)
		root_count += root_mask[i];
	unsigned* const roots = malloc(root_count * sizeof(unsigned));
	root_count = 0;
	for (unsigned i = 0; i < scene->node_count; ++i)
		if (root_mask[i]) roots[root_count++] = i;
	//Initialize root nodes
	for (unsigned i = 0; i < root_count; ++i) {
		struct Node* const node = scene->nodes + roots[i];
		if (!node->valid_transform)
			glm_mat4_identity(node->transformation);
	}
	//Traverse scene hierarchy
	unsigned* const queue = malloc(scene->node_count * sizeof(unsigned));
	unsigned queue_start = 0, queue_count = root_count;
	memcpy(queue, roots, root_count * sizeof(unsigned));
	free(roots);
	while (queue_count--) {
		struct Node* const node = scene->nodes + queue[queue_start++];
		if (!node->valid_transform) {
			//Compute transformation
			mat4 transformation = GLM_MAT4_IDENTITY_INIT;
			glm_translate(transformation, node->translation);
			glm_quat_rotate(transformation, node->rotation, transformation);
			glm_scale(transformation, node->scaling);
			glm_mat4_mul(node->transformation, transformation, node->transformation);
		}
		//Enqueue children
		for (unsigned i = 0; i < node->child_count; ++i) {
			struct Node* const child = scene->nodes + node->children[i];
			if (!child->valid_transform)
				glm_mat4_copy(node->transformation, child->transformation);
			queue[queue_start++] = node->children[i];
		}
		queue_count += node->child_count;
	}
	free(queue);
}

static void destroy_primitive(struct Primitive* primitive) {
	free(primitive->vertices);
	free(primitive->indices);
}

static void destroy_mesh(struct Mesh* mesh) {
	for (unsigned i = 0; i < mesh->primitive_count; ++i)
		destroy_primitive(mesh->primitives + i);
	free(mesh->primitives);
}

static void destroy_node(struct Node* node) {
	free(node->children);
}

void destroy_scene(struct Scene scene) {
	for (unsigned i = 0; i < scene.mesh_count; ++i)
		destroy_mesh(scene.meshes+ i);
	free(scene.meshes);
	for (unsigned i = 0; i < scene.node_count; ++i)
		destroy_node(scene.nodes + i);
	free(scene.nodes);
}
