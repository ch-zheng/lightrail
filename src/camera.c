#include "camera.h"
#include <cglm/affine.h>
#include <cglm/mat3.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <math.h>
#include <string.h>

void camera_transform(struct Camera camera, mat4 result) {
	//View matrix
	//Translation
	mat4 view = GLM_MAT4_IDENTITY_INIT;
	vec3 translation;
	glm_vec3_copy(camera.position, translation);
	glm_vec3_negate(translation);
	glm_translate(view, translation);
	//Change of basis
	vec3 horizon, up;
	glm_vec3_crossn(camera.direction, camera.up, horizon);
	glm_vec3_crossn(horizon, camera.direction, up);
	mat3 basis_change;
	memcpy(basis_change[0], horizon, sizeof(vec3));
	memcpy(basis_change[1], up, sizeof(vec3));
	memcpy(basis_change[2], camera.direction, sizeof(vec3));
	glm_mat4_ins3(basis_change, view);
	//Projection matrix
	float top = camera.near * tanf(((float) camera.fov / 2) * (3.14159 / 180));
	float right = camera.aspect_ratio * top;
	mat4 projection = {
		camera.near / camera.far, 0, 0, 0,
		0, camera.near / top, 0, 0,
		0, 0, camera.far / (camera.far - camera.near), 1,
		0, 0, -(camera.far * camera.near) / (camera.far - camera.near), 0
	};
	glm_mat4_mul(projection, view, result);
}

void camera_look(struct Camera* camera, vec3 target) {
	glm_vec3_sub(target, camera->position, camera->direction);
	glm_vec3_normalize(camera->direction);
}
