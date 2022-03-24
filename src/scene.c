#include "scene.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

enum ObjVertexFormat {P, PT, PN, PTN};

bool load_obj(const char* const filename, struct Mesh* mesh) {
	//File objects
	FILE* file;
	file = fopen(filename, "r");
	char* line;
	size_t buffer_size = 0;

	//Counting
	unsigned v_count = 0, vt_count = 0, vn_count = 0;
	mesh->vertex_count = 0;
	mesh->index_count = 0;
	while (getline(&line, &buffer_size, file) >= 0) {
		if (!strncmp(line, "v ", 2)) ++v_count;
		else if (!strncmp(line, "vt", 2)) ++vt_count;
		else if (!strncmp(line, "vn", 2)) ++vn_count;
		else if (line[0] == 'f') {
			unsigned vertex_count = 0;
			char* token = strtok(line, " \t\n");
			while (token) {
				if (token[0] != 'f') ++vertex_count;
				token = strtok(NULL, " \t\n");
			}
			//if (vertex_count < 3) continue;
			mesh->vertex_count += vertex_count;
			mesh->index_count += 3 * (vertex_count - 2);
		}
	}
	//Allocate arrays
	float* const v = malloc(3 * v_count * sizeof(float)),
		* const vt = malloc(3 * vt_count * sizeof(float)),
		* const vn = malloc(3 * vn_count * sizeof(float));
	mesh->vertices = malloc(mesh->vertex_count * sizeof(struct Vertex));
	mesh->indices = malloc(mesh->index_count * sizeof(unsigned));
	//Size indices
	unsigned i_v = 0, i_vt = 0, i_vn = 0, i_vertices = 0, i_indices = 0;

	//Parsing
	rewind(file);
	while (getline(&line, &buffer_size, file) >= 0) {
		if (!strncmp(line, "v ", 2)) {
			float* const offset = v + i_v;
			sscanf(line, "v %f %f %f", offset, offset + 1, offset + 2);
			i_v += 3;
		} else if (!strncmp(line, "vt", 2)) {
			float* const offset = v + i_vt;
			sscanf(line, "vt %f %f %f", offset, offset + 1, offset + 2);
			i_vt += 3;
		} else if (!strncmp(line, "vn", 2)) {
			float* const offset = v + i_vn;
			sscanf(line, "vn %f %f %f", offset, offset + 1, offset + 2);
			i_vn += 3;
		} else if (line[0] == 'f') {
			bool format_known;
			enum ObjVertexFormat vertex_format;
			unsigned face_vertices = 0, base_vertex = i_vertices + 1;
			char *token = strtok(line, " \t\n");
			//Parse vertices
			while (token) {
				if (token[0] == 'f') {
					token = strtok(NULL, " \t\n");
					continue;
				}
				//Determine vertex format
				if (!format_known) {
					unsigned slash_count = 0;
					char* c = token;
					while (*c) {
						if (*c == '/') ++slash_count;
						++c;
					}
					switch (slash_count) {
						case 0:
							vertex_format = P;
							break;
						case 1:
							vertex_format = PT;
							break;
						case 2:
							vertex_format = PN;
							break;
						case 3:
							vertex_format = PTN;
							break;
					}
					format_known = true;
				}
				//Parse vertex
				struct Vertex* const vertex = mesh->vertices + i_vertices++;
				unsigned j_v, j_vt, j_vn;
				switch (vertex_format) {
					case P:
						sscanf(token, "%d", &j_v);
						--j_v;
						*vertex = (struct Vertex) {
							{v[3 * j_v], v[3 * j_v + 1], v[3 * j_v + 2]}
						};
						break;
					case PT:
						sscanf(token, "%d/%d", &j_v, &j_vt);
						--j_v;
						--j_vt;
						*vertex = (struct Vertex) {
							{v[3 * j_v], v[3 * j_v + 1], v[3 * j_v + 2]},
							{v[3 * j_vt], v[3 * j_vt + 1], v[3 * j_vt + 2]}
						};
						break;
					case PN:
						sscanf(token, "%d//%d", &j_v, &j_vn);
						--j_vt;
						--j_vn;
						*vertex = (struct Vertex) {
							.pos = {v[3 * j_v], v[3 * j_v + 1], v[3 * j_v + 2]},
							.normal = {v[3 * j_vn], v[3 * j_vn + 1], v[3 * j_vn + 2]}
						};
						break;
					case PTN:
						sscanf(token, "%d/%d/%d", &j_v, &j_vt, &j_vn);
						--j_v;
						--j_vt;
						--j_vn;
						*vertex = (struct Vertex) {
							{v[3 * j_v], v[3 * j_v + 1], v[3 * j_v + 2]},
							{v[3 * j_vt], v[3 * j_vt + 1], v[3 * j_vt + 2]},
							{v[3 * j_vn], v[3 * j_vn + 1], v[3 * j_vn + 2]}
						};
						break;
				}
				++face_vertices;
				token = strtok(NULL, " \t\n");
			}
			for (unsigned i = 0; i < face_vertices - 2; ++i) {
				mesh->indices[i_indices++] = base_vertex;
				mesh->indices[i_indices++] = base_vertex + i + 1;
				mesh->indices[i_indices++] = base_vertex + i + 2;
			}
		}
	}

	//Cleanup
	free(v);
	free(vt);
	free(vn);
	fclose(file);
	free(line);
	return false;
}

void destroy_mesh(struct Mesh* mesh) {
	if (mesh->vertex_count) free(mesh->vertices);
	if (mesh->index_count) free(mesh->indices);
}

void destroy_scene(struct Scene* scene) {
	if (scene->mesh_count) free(scene->meshes);
}
