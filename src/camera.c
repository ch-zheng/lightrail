#include "camera.h"
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/mat3.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <math.h>
#include <string.h>

#define DEG_TO_RAD (3.14159 / 180)

struct Camera create_camera() {
	//TODO: Better default parameters
	struct Camera camera = {
		{0, -2, 0},
		{0, 1, 0},
		{0, 0, 1},
		90, 1, 0.1, 64,
		PERSPECTIVE
	};
	return camera;
};

//Note: Camera space has negative z-axis going into screen
void camera_view(struct Camera camera, mat4 result) {
	glm_look(camera.position, camera.direction, camera.up, result);
}

void camera_projection(struct Camera camera, mat4 result) {
	glm_perspective(camera.fov, camera.aspect_ratio, camera.near, camera.far, result);
	result[1][1] *= -1;
}

void camera_look(struct Camera* camera, vec3 target) {
	glm_vec3_sub(target, camera->position, camera->direction);
	glm_vec3_normalize(camera->direction);
}
