#include "camera.h"
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/mat3.h>
#include <cglm/mat4.h>
#include <cglm/vec3.h>
#include <math.h>
#include <string.h>

#define DEG_TO_RAD (M_PI / 180)

void camera_transform(struct Camera camera, mat4 result) {
	//View matrix
	mat4 scale;
	glm_mat4_identity(scale);
	// glm_mat4_scale(scale, .25);
	vec3 scale_factors = { .005, .005, .005 };
	mat4 rotate;
	glm_mat4_identity(rotate);
	vec3 rotate_axis = { 1, 0, 0};
	vec3 rotate_axis2 = { 0, 0, 1 };
	glm_rotate(scale, glm_rad(180), rotate_axis);
	glm_rotate(scale, glm_rad(90), rotate_axis2);
	glm_scale(scale, scale_factors);
	mat4 view;
	glm_look(camera.position, camera.direction, camera.up, view);
	//Change of basis
	mat4 basis = {
		{1, 0, 0, 0},
		{0, -1, 0, 0},
		{0, 0, -1, 0},
		{0, 0, 0, 1},
	};
	glm_mat4_mul(basis, view, view);
	//Projection matrix
	const float near = camera.near,
		far = camera.far,
		height = 2 * camera.near * tanf(((float) camera.fov / 2) * DEG_TO_RAD),
		width = camera.aspect_ratio * height;
	mat4 projection = {
		{2*near / width, 0, 0, 0},
		{0, 2*near / height, 0, 0},
		{0, 0, far / (far - near), 1},
		{0, 0, -(far * near) / (far - near), 0},
	};
	//Composition
	mat4 temp;
	glm_mat4_mul(view, scale, temp);
	glm_mat4_mul(projection, temp, result);
}

void camera_look(struct Camera* camera, vec3 target) {
	glm_vec3_sub(target, camera->position, camera->direction);
	glm_vec3_normalize(camera->direction);
}
