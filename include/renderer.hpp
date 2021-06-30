#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>

namespace lightrail {
	class Renderer {
		//Members
		SDL_Window* window;
		vk::Instance instance;
		vk::SurfaceKHR surface;
		vk::PhysicalDevice physical_device;
		size_t graphics_queue_family, present_queue_family;
		vk::Device device;
		vk::Queue graphics_queue, present_queue;
		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;
		vk::SwapchainKHR swapchain;
		std::vector<vk::Image> images;
		std::vector<vk::ImageView> image_views;
		vk::Pipeline pipeline;
		//Methods
		void create_swapchain(bool);
		void create_pipeline();
		vk::ShaderModule create_shader_module(std::string);
		public:
		Renderer(SDL_Window*);
		~Renderer();
	};
}
