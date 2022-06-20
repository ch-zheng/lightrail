#pragma once
#include <cglm/vec3.h>
#include <cglm/mat4.h>
#include <cglm/quat.h>
#include <stdbool.h>

struct Vertex {
	float pos[3]; //Position
	//Normal vectors
	float normal[3]; //Normal vector
	//float tangent[3]; //Tangent vector
	//float bitangent[3]; //Bitangent vector
	//float tex[2]; //Texture coordinates
	//unsigned material;
};

struct Primitive {
	unsigned vertex_count;
	struct Vertex* vertices;
	unsigned index_count;
	unsigned* indices;
	//unsigned material;
};

struct Mesh {
	unsigned primitive_count;
	struct Primitive* primitives;
	//float box_start[3], box_end[3]; //Bounding box
};

struct Node {
	unsigned child_count;
	unsigned* children;
	bool has_mesh;
	unsigned mesh;
	//bool enabled;
	//Local transformations
	vec3 translation;
	versor rotation;
	vec3 scaling;
	//World transformation
	bool valid_transform;
	mat4 transformation;
};

/*
struct Light {
	float pos[3];
	float color[3];
	float radius;
};
*/

/*
struct Material {
	float base_color[3];
	float metallic_factor, roughness_factor;
	unsigned base_color_tex, occ_met_rgh_tex, normal_tex;
};
*/

struct Scene {
	unsigned mesh_count;
	struct Mesh* meshes;
	unsigned node_count;
	struct Node* nodes;
	/*
	unsigned light_count;
	struct Light* lights;
	unsigned material_count;
	struct Material* materials;
	*/
};

//bool load_obj(const char* const, struct Mesh*);
bool load_scene(const char* const, struct Scene*);
void scene_update_transformations(struct Scene*);
void destroy_scene(struct Scene);
