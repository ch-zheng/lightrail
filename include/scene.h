#pragma once

#include <stdbool.h>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "camera.h"
#include "cglm/cglm.h"

#include "vulkan/vulkan.h"
#include "alloc.h"
#include "vertex_buffer.h"

#define NUMBER_OF_LIGHTS 16

struct Light {
	vec3 pos;
	vec3 color;
};

struct LightSet {
	struct Light lights[NUMBER_OF_LIGHTS];
};

struct Texture {
	uint32_t width;
	uint32_t height;
	char filepath[1024];
	VkImage image;
	VkImageView image_view;
	struct Allocation texture_alloc;
	bool loaded;	
};

struct Material {
	unsigned diffuse;
	unsigned emissive;
	unsigned normal_map;
};

struct Mesh {
	unsigned vertex_count, index_count;
	struct Vertex* vertices;
	unsigned* indices;
	struct Material m;
	struct MemoryBlock vertex_block;
	struct MemoryBlock index_block;
};

struct Node {
	unsigned child_count;
	struct Node* children;
	unsigned mesh_count;
	unsigned* meshes;
	vec3 translation;
	vec3 scaling;
	vec3 rotation;
	mat4 transform;
};

struct Scene {
	unsigned mesh_count;
	struct Mesh* meshes;
	
	unsigned material_count;
	struct Material* materials;
	
	unsigned texture_count;
	struct Texture* textures;
	
	struct Node* root;

	struct Camera cam;
};

bool load_obj(const char* const, const char* const, struct Scene* scene);
void process_node(struct aiNode* ai_node, struct Node* node, const struct aiScene* ai_scene, struct Scene* scene);
void process_mesh(struct aiMesh* ai_mesh, const struct aiScene* ai_scene, struct Mesh* mesh, struct Scene* scene);
void destroy_scene(VkDevice dev, struct Scene);
void load_textures(VkDevice device, VkPhysicalDevice phys_dev, VkCommandBuffer comm_buff, VkQueue queue, struct Scene* scene);
void destroy_node(struct Node* node);
