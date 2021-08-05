#pragma once
#include <Eigen/Geometry>

namespace lightrail {
	class Camera {
		Eigen::Vector3f position;
		Eigen::Vector3f direction;
		Eigen::Vector3f up;
		float fov; //Vertical field of view (degrees)
		float aspect_ratio; //Width:height
		float near, far; //Clipping plane distances
		Eigen::Projective3f transform; //Transform world coords to clip space
		bool valid_transform = true;
		Eigen::Projective3f generate_transform();
		public:
		Camera(
			Eigen::Vector3f = Eigen::Vector3f(0.0f, 0.0f, 0.0f),
			Eigen::Vector3f = Eigen::Vector3f(1.0f, 0.0f, 0.0f),
			Eigen::Vector3f = Eigen::Vector3f(0.0f, 0.0f, 1.0f),
			float = 90,
			float = 1.0f,
			float = 1.0f,
			float = 32.0f
		);
		void look_at(Eigen::Vector3f);
		//Getters
		const Eigen::Vector3f get_position() const {return position;}
		const Eigen::Vector3f get_direction() const {return direction;}
		const Eigen::Vector3f get_up() const {return up;}
		const Eigen::Projective3f get_transform();
		//Setters
		void set_position(const Eigen::Vector3f&);
		void set_direction(const Eigen::Vector3f&);
		void set_up(const Eigen::Vector3f&);
		void set_aspect_ratio(const float x) {aspect_ratio = x;}
	};
}
