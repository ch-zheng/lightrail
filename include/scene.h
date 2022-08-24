#pragma once
//#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <cglm/vec4.h>
#include <cglm/mat4.h>
#include <cglm/quat.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>

struct Vertex {
	vec3 pos; //Position
	vec3 normal; //Normal vector
	//vec3 tangent; //Tangent vector
	//vec3 bitangent; //Bitangent vector
	vec2 tex; //Texture coordinates
	unsigned material;
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
	vec3 pos;
	vec3 color;
	float radius;
};
*/

struct Material {
	vec4 base_color;
	float metallic_factor, roughness_factor;
	unsigned base_color_tex, met_rgh_tex, normal_tex;
};

struct Scene {
	unsigned mesh_count;
	struct Mesh* meshes;
	unsigned node_count;
	struct Node* nodes;
	unsigned material_count;
	struct Material* materials;
	unsigned texture_count;
	SDL_Surface** textures;
	/*
	unsigned light_count;
	struct Light* lights;
	*/
};

//bool load_obj(const char* const, struct Mesh*);
bool load_scene(const char* const, struct Scene*);
void scene_update_transformations(struct Scene*);
void destroy_scene(struct Scene);
