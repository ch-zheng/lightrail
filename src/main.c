#include "renderer.h"

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <cglm/vec3.h>
#include <SDL2/SDL.h>
#include <unistd.h>

#define MICRO 1000000
#define NANO 1000000000

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
	if (!IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG)) {
		fprintf(stderr, "Error initializing SDL_image\n");
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
	//printf("Creating renderer\n");
	create_renderer(window, &renderer);
	//printf("Created renderer\n");
	struct Camera camera = create_camera();

	//Scene
	struct Scene scene;
	//printf("Loading scene\n");
	load_scene("BarramundiFish.glb", &scene);
	//printf("Scene loaded\n");
	//printf("Loading scene into renderer\n");
	renderer_load_scene(&renderer, scene);
	//printf("Loaded scene into renderer\n");

	//Main loop
	bool running = true;
	SDL_Event event;
	struct timespec previous;
	timespec_get(&previous, TIME_UTC);
	const unsigned max_framerate = 400;
	const float min_frame_time = 1.0f / max_framerate;
	while (running) {
		//Timing
		struct timespec now;
		timespec_get(&now, TIME_UTC);
		const float delta = difftime(now.tv_sec, previous.tv_sec)
			+ (float) now.tv_nsec / NANO
			- (float) previous.tv_nsec / NANO; //Seconds since last frame
		previous = now;
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
		glm_vec3_cross(camera.direction, camera.up, side);
		if (inputs.move_forward)
			glm_vec3_add(movement, camera.direction, movement);
		if (inputs.move_backward)
			glm_vec3_sub(movement, camera.direction, movement);
		if (inputs.move_left)
			glm_vec3_sub(movement, side, movement);
		if (inputs.move_right)
			glm_vec3_add(movement, side, movement);
		if (inputs.move_up)
			glm_vec3_add(movement, camera.up, movement);
		if (inputs.move_down)
			glm_vec3_sub(movement, camera.up, movement);
		glm_vec3_normalize(movement);
		glm_vec3_scale(movement, delta, movement);
		glm_vec3_add(movement, camera.position, camera.position);
		//Rotation
		const float angular_velocity = 0.5; //Radians per second
		if (inputs.rotate_up)
			glm_vec3_rotate(camera.direction, angular_velocity * delta, side);
		if (inputs.rotate_down)
			glm_vec3_rotate(camera.direction, -angular_velocity * delta, side);
		if (inputs.rotate_left)
			glm_vec3_rotate(camera.direction, angular_velocity * delta, camera.up);
		if (inputs.rotate_right)
			glm_vec3_rotate(camera.direction, -angular_velocity * delta, camera.up);

		//Rendering
		if (shown && !minimized) {
			renderer_update_camera(&renderer, camera);
			renderer_update_nodes(&renderer, scene);
			renderer_draw(&renderer);
			usleep(min_frame_time > delta ? (min_frame_time - delta) * MICRO : 0);
		} else {
			sleep(1);
		}
	}

	//Cleanup
	//printf("Destroying renderer\n");
	destroy_renderer(renderer);
	//printf("Renderer destroyed\n");
	//destroy_scene(scene);
	IMG_Quit();
	SDL_DestroyWindow(window);
	SDL_Quit();
}
