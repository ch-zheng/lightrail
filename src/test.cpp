#include "mesh.hpp"
using namespace lightrail;

int main() {
	Mesh foo = Mesh::load_obj("thing.obj");
	std::cout << foo << std::endl;
}
