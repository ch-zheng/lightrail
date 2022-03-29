#include "camera.h"
#include "cglm/cglm.h"
#include "cglm/vec3.h"
#include "renderer.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <SDL.h>
#include <unistd.h>

#include "scene.h"

int main() {
	struct Scene scene;
	load_obj("/Users/hang/code/lightrail/models/AntiqueCamera/glTF/AntiqueCamera.gltf", &scene);
	
	//SDL Initialization
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window* window = SDL_CreateWindow(
		"Lightrail",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		800, 600,
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
	create_renderer(window, &renderer, &scene);

	vec3 camera_speed = {10.11, 10.11, 10.11};
	vec3 camera_front = { 0.0, 0.0, 1.0 };

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
				vec3 delta;
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
	destroy_scene(scene);
}
