#pragma once
#include "alloc.h"
#include "camera.h"
#include "scene.h"
#include "vertex_buffer.h"
#include "vulkan/vulkan_core.h"
#include <stdbool.h>
#include <SDL.h>
#include <vulkan/vulkan.h>

#define MAX_FRAMES_IN_FLIGHT 2

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
	VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
	VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
	VkSemaphore semaphores[2];
	unsigned current_frame;
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
	struct VertexIndexBuffer vertex_index_buffer;

	uint32_t mesh_count;
	uint32_t* index_indices;
	uint32_t* vertex_indices;

	// textures
	VkBuffer texture_buffer;
	struct Allocation texture_mem;
	VkImage texture_image;
	VkImageView texture_image_view;
	VkSampler texture_sampler;

	// uniforms and other shader data access
	VkDescriptorSetLayout descriptor_set_layout;
	
	VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
	struct Allocation uniform_mems[MAX_FRAMES_IN_FLIGHT];

	VkImage texture_images[MAX_FRAMES_IN_FLIGHT];
	struct Allocation texture_mems[MAX_FRAMES_IN_FLIGHT];
	VkImageView texture_imageviews[MAX_FRAMES_IN_FLIGHT];

	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];

	struct Scene* scene;
};

//Renderer methods
bool create_renderer(SDL_Window*, struct Renderer* const, struct Scene* scene);
void destroy_renderer(struct Renderer* const);
VkResult renderer_create_resolution(struct Renderer* const, unsigned, unsigned);
void renderer_destroy_resolution(struct Renderer* const);
void renderer_draw(struct Renderer* const);
void render_scene(struct Scene* scene, struct Node* node, mat4 transform, struct Renderer* const r);	
