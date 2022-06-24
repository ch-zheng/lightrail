#pragma once
#include "alloc.h"
#include "camera.h"
#include "scene.h"
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

#define COMMAND_BUFFER_COUNT 2
#define SCENE_BUFFER_COUNT 5

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
	/*
		Command buffers:
		1. Drawing
		2. Transfer
	*/
	VkCommandBuffer command_buffers[COMMAND_BUFFER_COUNT];
	/*
		Semaphores:
		1. Acquire swapchain image
		2. Presentation
	 */
	VkSemaphore semaphores[2];
	//Descriptors
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;
	//VkSampleCountFlagBits sample_count;
	VkPipelineCache pipeline_cache;

	//Swapchain
	VkExtent2D surface_extent;
	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage* swapchain_images;
	VkCommandPool blit_command_pool;
	VkCommandBuffer* blit_command_buffers;

	//Resolution-dependent structs
	VkExtent2D resolution;
	VkImage images[3];
	struct Allocation image_mem;
	VkImageView image_views[3];
	VkRenderPass render_pass;
	VkFramebuffer framebuffer;
	VkPipeline pipeline;

	//Scene management
	//Uniform buffer data
	VkBuffer uniform_buffer;
	struct Allocation uniform_alloc;
	//Storage buffer data
	/*
		Scene buffers:
		1. Vertices
		2. Indices
		3. Meshes
		4. Nodes
		5. Draw commands (one per node)
	*/
	VkBuffer storage_buffers[SCENE_BUFFER_COUNT];
	struct Allocation storage_alloc;
	//Staging buffer
	/*
		Staging buffer contents:
		1. Camera matrix
		2. Node transformations
		This buffer is copied to the render buffers every draw call.
	*/
	VkBuffer staging_buffer;
	struct Allocation staging_alloc;
};

//Renderer methods
bool create_renderer(SDL_Window*, struct Renderer* const);
void destroy_renderer(struct Renderer);
VkResult renderer_create_resolution(struct Renderer* const, unsigned, unsigned);
void renderer_destroy_resolution(struct Renderer* const);
void renderer_draw(struct Renderer* const);
void renderer_load_scene(struct Renderer* const, struct Scene);
void renderer_destroy_scene(struct Renderer* const);
void renderer_update_camera(struct Renderer* const, const struct Camera);
void renderer_update_nodes(struct Renderer* const, const struct Scene);
