#include <app.hpp>
using namespace lightrail;

int main() {
	App app;
	//Main loop
	bool running = true;
	while (running) {
		//Event handling
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_q) running = false;
					break;
				case SDL_QUIT:
					running = false;
					break;
			}
		}
		//Rendering
		app.renderer->draw();
		app.renderer->wait();
	}
}
