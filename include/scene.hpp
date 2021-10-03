#pragma once
#include "mesh.hpp"
#include <Eigen/Geometry>

namespace lightrail {
	struct Node {
		std::vector<Node> children;
		Eigen::Affine3f transformation = Eigen::Affine3f::Identity();
		ssize_t mesh = -1;
		//ssize_t texture = -1;
	};

	struct Scene {
		std::vector<Node> nodes;
		std::vector<Mesh> meshes;
		std::vector<std::string> textures;

		static std::vector<Scene> load_gltf(std::string);
	};
}
