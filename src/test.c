#include "scene.h"
#include <stdio.h>

int main() {
	struct Mesh mesh;
	load_obj("test.obj", &mesh);
	printf("Mesh counts: %d, %d\n", mesh.vertex_count, mesh.index_count);
	printf("VERTICES\n");
	for (unsigned i = 0; i < mesh.vertex_count; ++i) {
		printf("%f %f %f\n",
			mesh.vertices[i].pos[0],
			mesh.vertices[i].pos[0],
			mesh.vertices[i].pos[0]
		);
	}
	printf("INDICES\n");
	for (unsigned i = 0; i < mesh.index_count; ++i) {
		printf("%d ", mesh.indices[i]);
	}
	printf("\n");
	return 0;
}
