#pragma once
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>

const char* PIPELINE_CACHE_FILENAME = "pipeline-cache.bin";

struct AllocBlock {
	VkDevice* device;
	VkDeviceMemory* memory;
	unsigned count;
	VkDeviceSize* offsets;
};

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
	VkSwapchainKHR swapchain;

	//Resolution-dependent structs
	VkExtent2D resolution;
	VkImage images[3];
	struct AllocBlock image_mem;
	VkImageView image_views[3];
	VkRenderPass render_pass;
	VkFramebuffer framebuffer;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
};

struct Vertex {
	float pos[3];
	float tex[3];
	float normal[3];
};

//Renderer methods
bool create_renderer(SDL_Window*, struct Renderer* const);
void destroy_renderer(struct Renderer* const);
VkResult renderer_set_resolution(struct Renderer* const, unsigned, unsigned);
void renderer_destroy_resolution(struct Renderer* const);
void renderer_draw(const struct Renderer* const);
