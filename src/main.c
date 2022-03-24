#include "renderer.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <SDL.h>
#include <unistd.h>

int main() {
	//SDL Initialization
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window* window = SDL_CreateWindow(
		"Lightrail",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		128, 128,
		SDL_WINDOW_VULKAN|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE
	);
	if (!window) {
		fprintf(stderr, "Error creating window: %s\n", SDL_GetError());
		return 1;
	}

	//Window flags
	Uint32 window_flags = SDL_GetWindowFlags(window);
	bool shown = window_flags & SDL_WINDOW_SHOWN;
	bool minimized = window_flags & SDL_WINDOW_MINIMIZED;

	//Renderer
	struct Renderer renderer;
	create_renderer(window, &renderer);

	//Main loop
	bool running = true;
	SDL_Event event;
	clock_t start = clock();
	while (running) {
		//Timing
		float delta = (clock() - start) / (float) CLOCKS_PER_SEC;
		start = clock();
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
			renderer_draw(&renderer);
		} else {
			sleep(1);
		}
	}

	destroy_renderer(&renderer);
}
