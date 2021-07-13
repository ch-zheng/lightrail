#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace lightrail {
	class Renderer {
		const std::string PIPELINE_CACHE_FILENAME = "pipeline-cache.bin";

		SDL_Window* window;
		vk::Instance instance;
		vk::SurfaceKHR surface;
		vk::PhysicalDevice physical_device;
		uint32_t graphics_queue_family, present_queue_family;
		vk::Device device;
		vk::Queue graphics_queue, present_queue;
		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;
		vk::Semaphore image_available_semaphore, render_finished_semaphore;
		vk::PipelineCache pipeline_cache;
		
		//Memory structures
		VmaAllocator allocator;
		vk::Buffer vertex_buffer;
		VmaAllocation vertex_alloc;

		//Swapchain & dependencies
		vk::Extent2D surface_extent;
		vk::SwapchainKHR swapchain;
		std::vector<vk::ImageView> image_views;
		vk::RenderPass render_pass;
		vk::PipelineLayout pipeline_layout;
		std::vector<vk::Pipeline> pipelines;
		std::vector<vk::Framebuffer> framebuffers;

		void create_swapchain();
		void destroy_swapchain();
		//Pipelines
		void create_pipelines();
		vk::ShaderModule create_shader_module(std::string);

		public:
		Renderer(SDL_Window*);
		void draw();
		~Renderer();
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec3 color;
	};
}
