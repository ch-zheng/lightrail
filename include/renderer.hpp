#pragma once
#include "render-utils.hpp"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>
#include <Eigen/Geometry>

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
		std::unique_ptr<BufferWrapper> vertex_buffer;
		std::unique_ptr<BufferWrapper> index_buffer;
		std::unique_ptr<Texture> texture;
		Eigen::Affine3f projection;

		//Descriptors
		vk::DescriptorSetLayout descriptor_layout;
		vk::DescriptorPool descriptor_pool;
		vk::DescriptorSet descriptor_set;

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
		void create_pipelines();
		vk::ShaderModule create_shader_module(std::string);
		BufferWrapper create_buffer_wrapper(vk::BufferCreateInfo&, VmaAllocationCreateInfo&);

		public:
		Renderer() = default;
		Renderer(SDL_Window*);
		void draw();
		void wait();
		~Renderer();
	};
}
