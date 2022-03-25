#include "renderer.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <cglm/vec3.h>
#include <SDL2/SDL.h>
#include <unistd.h>

struct Inputs {
	//Movement
	bool move_forward, move_backward, move_left, move_right, move_up, move_down;
	//Rotation
	bool rotate_up, rotate_down, rotate_left, rotate_right;
};

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

	//Inputs
	struct Inputs inputs = {
		//Movement
		false, false, false, false, false, false,
		//Rotation
		false, false, false, false
	};

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
						//Movement
						case SDLK_w:
							inputs.move_forward = true;
							break;
						case SDLK_s:
							inputs.move_backward = true;
							break;
						case SDLK_a:
							inputs.move_left = true;
							break;
						case SDLK_d:
							inputs.move_right = true;
							break;
						case SDLK_z:
							inputs.move_up = true;
							break;
						case SDLK_x:
							inputs.move_down = true;
							break;
						//Rotation
						case SDLK_UP:
							inputs.rotate_up = true;
							break;
						case SDLK_DOWN:
							inputs.rotate_down = true;
							break;
						case SDLK_LEFT:
							inputs.rotate_left = true;
							break;
						case SDLK_RIGHT:
							inputs.rotate_right = true;
							break;
						//UI
						case SDLK_q:
							running = false;
							break;
					}
					break;
				case SDL_KEYUP:
					switch (event.key.keysym.sym) {
						//Movement
						case SDLK_w:
							inputs.move_forward = false;
							break;
						case SDLK_s:
							inputs.move_backward = false;
							break;
						case SDLK_a:
							inputs.move_left = false;
							break;
						case SDLK_d:
							inputs.move_right = false;
							break;
						case SDLK_z:
							inputs.move_up = false;
							break;
						case SDLK_x:
							inputs.move_down = false;
							break;
						//Rotation
						case SDLK_UP:
							inputs.rotate_up = false;
							break;
						case SDLK_DOWN:
							inputs.rotate_down = false;
							break;
						case SDLK_LEFT:
							inputs.rotate_left = false;
							break;
						case SDLK_RIGHT:
							inputs.rotate_right = false;
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

		//Input handling
		//Movement
		vec3 movement = {0, 0, 0}, side;
		glm_vec3_cross(renderer.camera.direction, renderer.camera.up, side);
		if (inputs.move_forward)
			glm_vec3_add(movement, renderer.camera.direction, movement);
		if (inputs.move_backward)
			glm_vec3_sub(movement, renderer.camera.direction, movement);
		if (inputs.move_left)
			glm_vec3_sub(movement, side, movement);
		if (inputs.move_right)
			glm_vec3_add(movement, side, movement);
		if (inputs.move_up)
			glm_vec3_add(movement, renderer.camera.up, movement);
		if (inputs.move_down)
			glm_vec3_sub(movement, renderer.camera.up, movement);
		glm_vec3_normalize(movement);
		glm_vec3_scale(movement, 2*delta, movement);
		glm_vec3_add(movement, renderer.camera.position, renderer.camera.position);
		//Rotation
		const float angular_velocity = 2; //Radians per second
		if (inputs.rotate_up)
			glm_vec3_rotate(renderer.camera.direction, angular_velocity * delta, side);
		if (inputs.rotate_down)
			glm_vec3_rotate(renderer.camera.direction, -angular_velocity * delta, side);
		if (inputs.rotate_left)
			glm_vec3_rotate(renderer.camera.direction, angular_velocity * delta, renderer.camera.up);
		if (inputs.rotate_right)
			glm_vec3_rotate(renderer.camera.direction, -angular_velocity * delta, renderer.camera.up);

		//Rendering
		if (shown && !minimized) {
			renderer_draw(&renderer);
		} else {
			sleep(1);
		}
	}

	destroy_renderer(&renderer);
}
