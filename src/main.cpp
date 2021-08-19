#include "app.hpp"
#include <chrono>
#include <thread>
#include <iostream>
using namespace lightrail;
using namespace std::chrono_literals;

int main() {
	App app;
	bool running = true;
	auto window_flags = SDL_GetWindowFlags(app.get_window());
	bool shown = window_flags & SDL_WINDOW_SHOWN;
	bool minimized = window_flags & SDL_WINDOW_MINIMIZED;
	//Generate scene
	Scene scene;
	Node node;
	Mesh mesh;
	mesh.vertices = {
		{{0, 0, 0}, {0, 1, 0}, {0, 0, 0}},
		{{1, 0, 0}, {1, 0, 0}, {0, 0, 0}},
		{{0, 0, 1}, {0, 0, 0}, {0, 0, 0}}
	};
	node.mesh = 0;
	scene.nodes.push_back(node);
	scene.meshes.push_back(mesh);
	app.renderer->load_scene(scene);
	//Scene information
	Camera& camera = app.renderer->camera;
	float h_rotation = 0, v_rotation = 0;
	//Main loop
	auto start = std::chrono::system_clock::now();
	SDL_Event event;
	while (running) {
		//Timing
		auto end = std::chrono::system_clock::now();
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
		start = end;
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
		//Keyboard state
		auto keyboard_state = SDL_GetKeyboardState(nullptr);
		//Movement
		if (keyboard_state[SDL_SCANCODE_W]) v_rotation += 0.001f * delta.count();
		if (keyboard_state[SDL_SCANCODE_S]) v_rotation -= 0.001f * delta.count();
		if (keyboard_state[SDL_SCANCODE_A]) h_rotation -= 0.001f * delta.count();
		if (keyboard_state[SDL_SCANCODE_D]) h_rotation += 0.001f * delta.count();
		/* TODO: Angle rollover
		v_rotation = fmod(v_rotation, M_PI_2);
		h_rotation = fmod(h_rotation, M_PI_2);
		*/
		Eigen::Affine3f t = Eigen::Affine3f::Identity();
		t.prerotate(Eigen::AngleAxisf(v_rotation, Eigen::Vector3f(1.0f, 0.0f, 0.0f)));
		t.prerotate(Eigen::AngleAxisf(h_rotation, Eigen::Vector3f(0.0f, 0.0f, 1.0f)));
		camera.set_position(t * Eigen::Vector3f(0.0f, -4.0f, 0.0f));
		camera.look_at(Eigen::Vector3f::Zero());
		//Rendering
		if (shown && !minimized) {
			app.renderer->draw();
			app.renderer->wait();
		} else {
			std::this_thread::sleep_for(100ms);
		}
	}
}
