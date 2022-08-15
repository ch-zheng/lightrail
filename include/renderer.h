#pragma once
#include "alloc.h"
#include "camera.h"
#include "scene.h"
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

static const char* const PIPELINE_CACHE_FILENAME = "pipeline-cache.bin";

struct Renderer {
	SDL_Window* window;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device;
	uint32_t graphics_queue_family, present_queue_family;
	VkDevice device;
	VkQueue graphics_queue, present_queue;
	VkCommandPool command_pool;
	VkCommandBuffer transfer_command_buffer;
	//Descriptors
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;
	//VkSampleCountFlagBits sample_count;
	VkPipelineCache pipeline_cache;

	//Resolution-dependent
	VkExtent2D resolution;
	VkRenderPass render_pass;
	VkPipeline pipeline;

	//Swapchain
	VkExtent2D surface_extent;
	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage* swapchain_images;

	//Frames
	unsigned current_frame;
	unsigned frame_count;
	//Framebuffers
	/*
		Images (per frame):
		1. Color
		2. Resolve
		3. Depth
	*/
	VkImage* images;
	struct Allocation image_alloc;
	VkImageView* image_views;
	VkFramebuffer* framebuffers;
	//Rendering
	VkCommandBuffer* command_buffers;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet* descriptor_sets;
	//Synchronization
	/*
		Semaphores (per frame):
		1. Swapchain image acquired
		2. Presentation
	*/
	VkSemaphore* semaphores;
	VkFence* fences;
	//Frame buffers
	VkBuffer staging_buffer;
	VkDeviceSize staging_size;
	struct Allocation staging_alloc;
	/*
		Frame buffers (per frame):
		1. Uniform
		2. Storage
	*/
	VkBuffer* frame_buffers;
	VkDeviceSize uniform_size, storage_size;
	struct Allocation frame_buffer_alloc;

	//Host-local dynamic scene data (size = staging_size)
	/*
		Contents:
		1. Camera
		2. Nodes
	*/
	void* restrict host_data;
	//Static scene data
	/*
		1. Vertices
		2. Indices
		3. Meshes
		4. Draw calls
	*/
	VkBuffer static_buffers[4];
	struct Allocation static_alloc;
	unsigned draw_count;
};

//Renderer methods
bool create_renderer(SDL_Window*, struct Renderer* const);
void destroy_renderer(struct Renderer);
void renderer_draw(struct Renderer* const);
void renderer_load_scene(struct Renderer* const, struct Scene);
void renderer_destroy_scene(struct Renderer* const);
void renderer_update_camera(struct Renderer* const, const struct Camera);
void renderer_update_nodes(struct Renderer* const, const struct Scene);
