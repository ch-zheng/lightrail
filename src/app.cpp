#include "app.hpp"
using namespace lightrail;

App::App() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		throw std::runtime_error("Error initializing SDL\n");
	window = SDL_CreateWindow(
		"Lightrail",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		128, 128,
		SDL_WINDOW_VULKAN|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE
	);
	if (!window) {
		std::ostringstream msg;
		msg << "Error creating window: " << SDL_GetError() << "\n";
		throw std::runtime_error(msg.str());
	}
	renderer = std::unique_ptr<Renderer>(new Renderer(window));
}

App::~App() {
	SDL_DestroyWindow(window);
}
