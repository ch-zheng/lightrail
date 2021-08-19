#pragma once
#include <array>
#include <iostream>
#include <vector>

namespace lightrail {
	struct Vertex {
		std::array<float, 3> position {0, 0, 0};
		//std::array<float, 4> color;
		std::array<float, 3> texture_coords {0, 0, 0};
		std::array<float, 3> normal {0, 0, 0};
	};

	struct Mesh {
		std::vector<Vertex> vertices;
		std::vector<uint16_t> indices; //Non-empty => Indexed drawing
		size_t texture;
		static Mesh load_obj(std::string);
	};

	std::ostream& operator<<(std::ostream&, const Vertex&);
	std::ostream& operator<<(std::ostream&, const Mesh&);
}
