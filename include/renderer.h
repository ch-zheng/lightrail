#pragma once
#include "alloc.h"
#include "camera.h"
#include "scene.h"
#include <stdbool.h>
#include <SDL.h>
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
	VkCommandBuffer command_buffer;
	VkSemaphore semaphores[2];
	//VkSampleCountFlagBits sample_count;
	//VkPipelineCache pipeline_cache;

	//Swapchain
	VkExtent2D surface_extent;
	VkSwapchainKHR swapchain;
	uint32_t swapchain_image_count;
	VkImage* swapchain_images;

	//Resolution-dependent structs
	VkExtent2D resolution;
	VkImage images[3];
	struct Allocation image_mem;
	VkImageView image_views[3];
	VkRenderPass render_pass;
	VkFramebuffer framebuffer;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;

	//FIXME: Testing data
	struct Camera camera;
	VkBuffer vertex_buffers[2];
	struct Allocation vertex_alloc;
};

//Renderer methods
bool create_renderer(SDL_Window*, struct Renderer* const);
void destroy_renderer(struct Renderer* const);
VkResult renderer_create_resolution(struct Renderer* const, unsigned, unsigned);
void renderer_destroy_resolution(struct Renderer* const);
void renderer_draw(struct Renderer* const);
