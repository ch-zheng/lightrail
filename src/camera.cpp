#include "camera.hpp"
#include <cmath>
#include <iostream>
using namespace lightrail;

Camera::Camera(
	Eigen::Vector3f position,
	Eigen::Vector3f direction,
	Eigen::Vector3f up,
	float fov,
	float aspect_ratio,
	float near,
	float far 
) : position(position), direction(direction), up(up),
	fov(fov), aspect_ratio(aspect_ratio), near(near), far(far) {
	transform = generate_transform();
}

Eigen::Projective3f Camera::generate_transform() {
	//View transform
	Eigen::Translation3f translation(-1 * position);
	Eigen::Vector3f h = direction.cross(up);
	h.normalize();
	Eigen::Vector3f norm_up = direction.cross(h);
	Eigen::Projective3f basis_change = Eigen::Projective3f::Identity();
	basis_change.matrix().block<1, 3>(0, 0) = h;
	basis_change.matrix().block<1, 3>(1, 0) = norm_up;
	basis_change.matrix().block<1, 3>(2, 0) = direction;
	//Perspective projection
	float top = near * std::tan((fov / 2) * (3.14159 / 180));
	float right = aspect_ratio * top;
	Eigen::Projective3f projection;
	projection.matrix() <<
		near / right, 0, 0, 0,
		0, near / top, 0, 0,
		0, 0, far / (far - near), -(far * near) / (far - near),
		0, 0, 1, 0;
	return projection * basis_change * translation;
}

void Camera::look_at(Eigen::Vector3f target) {
	direction = target - position;
	direction.normalize();
}

const Eigen::Projective3f Camera::get_transform() {
	if (!valid_transform) {
		transform = generate_transform();
		valid_transform = true;
	}
	return transform;
}

void Camera::set_position(const Eigen::Vector3f& other) {
	position = other;
	valid_transform = false;
}

void Camera::set_direction(const Eigen::Vector3f& other) {
	direction = other;
	direction.normalize();
	valid_transform = false;
}

void Camera::set_up(const Eigen::Vector3f& other) {
	up = other;
	up.normalize();
	valid_transform = false;
}
