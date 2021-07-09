#pragma once
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>

namespace lightrail {
	class Renderer {
		SDL_Window* window;
		vk::Instance instance;
		vk::SurfaceKHR surface;
		vk::PhysicalDevice physical_device;
		uint32_t graphics_queue_family, present_queue_family;
		vk::Device device;
		vk::Queue graphics_queue, present_queue;
		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;

		//Swapchain & dependencies
		vk::Extent2D surface_extent;
		vk::SwapchainKHR swapchain;
		std::vector<vk::Image> images;
		std::vector<vk::ImageView> image_views;
		vk::RenderPass render_pass;
		vk::PipelineLayout pipeline_layout;
		std::vector<vk::Pipeline> pipelines;
		std::vector<vk::Framebuffer> framebuffers;

		vk::Semaphore image_available_semaphore, render_finished_semaphore;

		void create_swapchain();
		void destroy_swapchain();
		void create_pipelines();
		vk::ShaderModule create_shader_module(std::string);

		public:
		Renderer(SDL_Window*);
		void draw();
		~Renderer();
	};
}
