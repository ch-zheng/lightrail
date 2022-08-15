#pragma once
#include <cglm/vec3.h>

enum Projection {ORTHOGRAPHIC, PERSPECTIVE};

struct Camera {
	vec3 position, direction, up;
	unsigned fov;
	float aspect_ratio, near, far;
	enum Projection projection;
};

struct Camera create_camera();
void camera_transform(struct Camera, mat4);
void camera_view(struct Camera, mat4); //View matrix
void camera_projection(struct Camera, mat4); //Projection matrix
void camera_look(struct Camera*, vec3);
