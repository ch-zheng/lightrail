#pragma once
#include <sstream>
#include <stdexcept>
#include <SDL2/SDL.h>
#include "renderer.hpp"

namespace lightrail {
	class App {
		SDL_Window *window;
		public:
		std::unique_ptr<lightrail::Renderer> renderer;
		App();
		~App();
	};
}
