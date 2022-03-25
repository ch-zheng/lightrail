#pragma once

#include <stdbool.h>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"


struct Vertex {
	float pos[3]; //Position
	float tex[3]; //Texture coordinates
	float normal[3]; //Normal vector
};

struct Mesh {
	unsigned vertex_count, index_count;
	struct Vertex* vertices;
	unsigned* indices;
};

struct Scene {
	unsigned mesh_count;
	struct Mesh* meshes;
};

bool load_obj(const char* const, struct Scene* scene);
void process_node(struct aiNode* node, const struct aiScene* ai_scene, struct Scene* scene);
void process_mesh(struct aiMesh* ai_mesh, const struct aiScene* scene, struct Mesh* mesh);
uint32_t count_total_vertices(struct Scene* scene);
uint32_t count_total_indices(struct Scene* scene);
void destroy_mesh(struct Mesh*);
void destroy_scene(struct Scene);

