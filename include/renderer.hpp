#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>

namespace lightrail {
	class Renderer {
		SDL_Window* window;
		vk::Instance instance;
		vk::SurfaceKHR surface;
		vk::PhysicalDevice physical_device;
		size_t graphics_queue_family, present_queue_family;
		vk::Device device;
		vk::Queue graphics_queue;
		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;
		public:
		Renderer(SDL_Window*);
		~Renderer();
		vk::SwapchainKHR create_swapchain();
	};
}
