#pragma once
#include "buffer.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "scene.hpp"
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>
#include <Eigen/Geometry>

namespace lightrail {
	struct Drawable {
		size_t mesh;
		//ssize_t texture; //Custom texture
		Eigen::Affine3f transformation = Eigen::Affine3f::Identity();
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
		bool scene_loaded = false;
		//Draw buffers
		Buffer vertices, indices;
		Buffer draw_commands, draw_indexed_commands;
		size_t draw_command_count, draw_indexed_command_count;
		//Descriptors
		Buffer transformations, transformation_offsets, uniforms;
		//Textures
		//std::vector<Texture> textures; TODO
		std::unique_ptr<Texture> texture;

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
		void destroy_scene_buffers();

		public:
		Camera camera;
		Renderer() = default;
		Renderer(SDL_Window*);
		void draw();
		void wait();
		void load_scene(const Scene&);
		~Renderer();
	};
}
