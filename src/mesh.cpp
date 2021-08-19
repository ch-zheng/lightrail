#include "mesh.hpp"
#include <cctype>
#include <cstring>
#include <fstream>
using namespace lightrail;

Mesh Mesh::load_obj(std::string filename) {
	//Mesh information
	std::vector<std::array<float, 3>> vertices;
	std::vector<std::array<float, 3>> texture_coords;
	std::vector<std::array<float, 3>> normals;
	Mesh result;
	//Read file
	std::ifstream file(filename);
	std::string line;
	while (std::getline(file, line)) {
		//Split string
		std::vector<std::string> parts;
		size_t start = 0, end = 0;
		while (start < line.length()) {
			if (end >= line.length() || isspace(line[end])) {
				auto part = line.substr(start, end - start);
				if (!part.empty()) parts.push_back(part);
				start = ++end;
			} else ++end;
		}
		//Split string
		//Process OBJ line
		if (parts.empty() || line[0] == '#') continue;
		else if (parts[0] == "v") {
			//Vertex position
			std::array<float, 3> position {
				std::stof(parts[1]),
				std::stof(parts[2]),
				std::stof(parts[3])
			};
			if (parts.size() == 5) {
				float w = std::stof(parts[4]);
				for (auto& p : position) p /= w;
			}
			vertices.push_back(position);
		} else if (parts[0] == "vt") {
			//Texture coordinate
			std::array<float, 3> texture_coord {
				std::stof(parts[1]),
				parts.size() >= 3 ? std::stof(parts[2]) : 0,
				parts.size() == 4 ? std::stof(parts[3]) : 0
			};
			texture_coords.push_back(texture_coord);
		} else if (parts[0] == "vn") {
			//Vertex normal
			std::array<float, 3> normal {
				std::stof(parts[1]),
				std::stof(parts[2]),
				std::stof(parts[3])
			};
			normals.push_back(normal);
		} else if (parts[0] == "f") {
			//Face
			std::vector<Vertex> face_vertices;
			face_vertices.reserve(parts.size() - 1);
			//Iterate over vertices
			for (size_t i = 1; i < parts.size(); ++i) {
				//Get indices
				const auto& part = parts[i];
				std::array<ssize_t, 3> indices {0, 0, 0};
				size_t start = 0, end = 0, j = 0;
				while (start < part.length()) {
					if (end >= part.length() || part[end] == '/') {
						indices[j] = std::stof(part.substr(start, end - start));
						start = ++end;
						++j;
					} else ++end;
				}
				//Generate vertex
				Vertex vertex;
				if (indices[0] > 0) vertex.position = vertices[indices[0] - 1];
				else if (indices[0] < 0) vertex.position = vertices[indices[0] + indices.size()];
				if (indices[1] > 0) vertex.texture_coords = texture_coords[indices[1] - 1];
				else if (indices[1] < 0) vertex.texture_coords = texture_coords[indices[1] + indices.size()];
				if (indices[2] > 0) vertex.normal = normals[indices[2] - 1];
				else if (indices[2] < 0) vertex.normal = normals[indices[2] + indices.size()];
				face_vertices.push_back(vertex);
			}
			//Generate triangle list
			for (size_t i = 1; i < face_vertices.size() - 1; ++i) {
				result.vertices.push_back(face_vertices[0]);
				result.vertices.push_back(face_vertices[i]);
				result.vertices.push_back(face_vertices[i + 1]);
			}
		}
	}
	return result;
}

std::ostream& lightrail::operator<<(std::ostream& os, const Vertex& v) {
	//Positions
	os << "P:";
	for (const auto& x : v.position) os << " " << x;
	os << std::endl;
	//Texture coords
	os << "T:";
	for (const auto& x : v.texture_coords) os << " " << x;
	os << std::endl;
	//Normals
	os << "N:";
	for (const auto& x : v.normal) os << " " << x;
	return os;
}

std::ostream& lightrail::operator<<(std::ostream& os, const Mesh& m) {
	//TODO: Prevent final newline & bar
	for (const auto& v : m.vertices) {
		os << v << std::endl;
		os << "--------" << std::endl;
	}
	return os;
}
