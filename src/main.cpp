#include "app.hpp"
#include <chrono>
#include <thread>
using namespace lightrail;
using namespace std::chrono_literals;

int main() {
	App app;
	//Main loop
	bool running = true;
	auto window_flags = SDL_GetWindowFlags(app.get_window());
	bool shown = window_flags & SDL_WINDOW_SHOWN;
	bool minimized = window_flags & SDL_WINDOW_MINIMIZED;
	SDL_Event event;
	while (running) {
		//Event handling
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
						case SDLK_q:
							running = false;
							break;
					}
					break;
				case SDL_QUIT:
					running = false;
					break;
				case SDL_WINDOWEVENT:
					switch (event.window.type) {
						case SDL_WINDOWEVENT_SHOWN:
							shown = true;
							break;
						case SDL_WINDOWEVENT_HIDDEN:
							shown = false;
							break;
						case SDL_WINDOWEVENT_MINIMIZED:
							minimized = true;
							break;
						case SDL_WINDOWEVENT_RESTORED:
							minimized = false;
					}
					break;
			}
		}
		//Rendering
		if (shown && !minimized) {
			app.renderer->draw();
			app.renderer->wait();
		} else {
			std::this_thread::sleep_for(100ms);
		}
	}
}
