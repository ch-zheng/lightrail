#include "scene.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "cglm/cglm.h"

enum ObjVertexFormat {P, PT, PN, PTN};

// load a gltf file
bool load_obj(const char* const filename, struct Scene* scene) {	
	const struct aiScene* ai_scene = aiImportFile(filename, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
	if (!ai_scene || ai_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !ai_scene->mRootNode) {
		return false;
	}
	scene->mesh_count = 0;

	process_node(ai_scene->mRootNode, ai_scene, scene);
	aiReleaseImport(ai_scene);
	return true;
}

void process_node(struct aiNode *node, const struct aiScene* ai_scene, struct Scene* scene) {
	if (scene->mesh_count) {
		struct Mesh* new_meshes = (struct Mesh*) realloc(scene->meshes, sizeof(struct Mesh)*(node->mNumMeshes+scene->mesh_count));
		scene->meshes = new_meshes;
	} else if (node->mNumMeshes > 0) {
		scene->meshes = (struct Mesh*) malloc(sizeof(struct Mesh)*(node->mNumMeshes));
	}
	scene->mesh_count = scene->mesh_count + node->mNumMeshes;
	
	for (int i = 0; i < node->mNumMeshes; ++i) {
		process_mesh(ai_scene->mMeshes[node->mMeshes[i]], ai_scene, &scene->meshes[i]);
	}
	
	for (int i = 0; i < node->mNumChildren; ++i) {
		process_node(node->mChildren[i], ai_scene, scene);
	}
}

void process_mesh(struct aiMesh* ai_mesh, const struct aiScene *scene, struct Mesh* mesh) {
	mesh->vertex_count = ai_mesh->mNumVertices;
	mesh->vertices = (struct Vertex*) malloc(sizeof(struct Vertex) * mesh->vertex_count);

	for (int i = 0; i < mesh->vertex_count; ++i) {
		struct Vertex vertex;
		struct aiVector3D ai_vertex = ai_mesh->mVertices[i];
		vec3 pos = { ai_vertex.x, ai_vertex.y, ai_vertex.z };
		memcpy(&vertex.pos, pos, sizeof(vec3));

		struct aiVector3D ai_normal = ai_mesh->mNormals[i];
		vec3 norm = { ai_normal.x, ai_normal.y, ai_normal.z };
		memcpy(&vertex.normal, norm, sizeof(vec3));

		if (ai_mesh->mTextureCoords[0]) {
            struct aiVector3D ai_texture_coord = ai_mesh->mTextureCoords[0][i];
            vec3 texture = { ai_texture_coord.x, ai_texture_coord.y, ai_texture_coord.z };
            memcpy(&vertex.tex, texture, sizeof(vec3));
        } else {
            vec3 texture = {};
            memcpy(&vertex.tex, texture, sizeof(vec3));
		}
	 	
		mesh->vertices[i] = vertex;
	}

	mesh->index_count = 0;
	mesh->indices = (unsigned*) malloc(sizeof(uint32_t) * mesh->vertex_count);
	unsigned indices_len = mesh->vertex_count;
	size_t index = 0;
	for (int i = 0; i < ai_mesh->mNumFaces; ++i) {
		struct aiFace face = ai_mesh->mFaces[i];
		mesh->index_count += face.mNumIndices;
		if (mesh->index_count >= indices_len) {
			mesh->indices = (unsigned*) realloc(mesh->indices, sizeof(uint32_t) * 2*indices_len);
			indices_len *= 2;
		}
		for (int j = 0; j < face.mNumIndices; ++j) {
			mesh->indices[index] = face.mIndices[j];
			index+=1;
		}
	}
} 

void destroy_mesh(struct Mesh* mesh) {
	if (mesh->vertex_count) free(mesh->vertices);
	if (mesh->index_count) free(mesh->indices);
}

void destroy_scene(struct Scene scene) {
	if (scene.mesh_count) free(scene.meshes);
}
