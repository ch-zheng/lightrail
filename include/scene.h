#include <stdbool.h>

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

bool load_obj(const char* const, struct Mesh*);
void destroy_mesh(struct Mesh*);
void destroy_scene(struct Scene*);
