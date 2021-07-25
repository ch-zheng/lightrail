#pragma once
#include "buffer.hpp"
#include "texture.hpp"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>
#include <Eigen/Geometry>

namespace lightrail {
	struct Vertex {
		std::array<float, 3> position;
		std::array<float, 3> color;
		std::array<float, 2> texture_pos;
	};

	class Renderer {
		const std::string PIPELINE_CACHE_FILENAME = "pipeline-cache.bin";

		SDL_Window* window;
		vk::Instance instance;
		vk::SurfaceKHR surface;
		vk::PhysicalDevice physical_device;
		uint32_t graphics_queue_family, present_queue_family;
		vk::Device device;
		vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
		vk::Queue graphics_queue, present_queue;
		vk::CommandPool command_pool;
		vk::CommandBuffer command_buffer;
		vk::Semaphore image_available_semaphore, render_finished_semaphore;
		vk::PipelineCache pipeline_cache;
		
		//Memory structures
		VmaAllocator allocator;
		//Mesh
		Buffer vertex_buffer;
		Buffer index_buffer;
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
		Image color_image;
		vk::ImageView color_image_view;
		Image depth_image;
		vk::ImageView depth_image_view;
		vk::RenderPass render_pass;
		std::vector<vk::Framebuffer> framebuffers;
		vk::PipelineLayout pipeline_layout;
		std::vector<vk::Pipeline> pipelines;

		void create_swapchain();
		void destroy_swapchain();
		void create_pipelines();
		vk::ShaderModule create_shader_module(std::string);

		public:
		Renderer() = default;
		Renderer(SDL_Window*);
		void draw();
		void wait();
		~Renderer();
	};
}
