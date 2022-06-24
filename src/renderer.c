#include "renderer.h"
#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <SDL2/SDL_vulkan.h>
#include <cglm/mat4.h>

#define REQUIRED_EXT_COUNT 2

struct LocalMesh {
	unsigned vertex_offset;
	unsigned vertex_count;
	unsigned index_offset;
	unsigned index_count;
};

struct LocalNode {
	mat4 transformation;
};

static VkShaderModule create_shader_module(
	const struct Renderer* const r,
	const char* filename) {
	//Read shader file
	FILE* file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	const long size = ftell(file);
	rewind(file);
	uint32_t* buffer = malloc(size);
	fread(buffer, 1, size, file);
	fclose(file);
	//Create shader module
	VkShaderModuleCreateInfo shader_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
		size, buffer
	};
	VkShaderModule result;
	vkCreateShaderModule(r->device, &shader_info, NULL, &result);
	return result;
}

static VkResult create_pipeline(struct Renderer* const r) {
	//Shaders
	const VkShaderModule vertex_shader = create_shader_module(r, "shaders/basic.vert.spv"),
		fragment_shader = create_shader_module(r, "shaders/basic.frag.spv");
	const VkPipelineShaderStageCreateInfo shader_stages[2] = {
		//Vertex stage
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vertex_shader,
			"main"
		},
		//Fragment stage
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragment_shader,
			"main"
		}
	};

	//Fixed functions
	//Vertex input
	const VkVertexInputBindingDescription binding_description = {
		0,
		sizeof(struct Vertex),
		VK_VERTEX_INPUT_RATE_VERTEX
	};
	const VkVertexInputAttributeDescription attribute_descriptions[2] = {
		//Position
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, pos)},
		//Normal
		{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, normal)},
		//Texture coords
		//{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, tex)},
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
		1, &binding_description,
		2, attribute_descriptions
	};
	//Input assembly
	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false
	};
	//Viewport
	const VkViewport viewport_transform = {
		0, 0,
		r->resolution.width, r->resolution.height,
		0, 1
	};
	const VkRect2D scissor = {
		{0, 0},
		{r->resolution.width, r->resolution.height}
	};
	const VkPipelineViewportStateCreateInfo viewport = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0,
		1, &viewport_transform,
		1, &scissor
	};
	//Rasterizer
	//FIXME: Debug-mode settings
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0,
		false,
		false,
		VK_POLYGON_MODE_LINE, //VK_POLYGON_MODE_FILL,
		0, //VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		false,
		0, 0, 0,
		1
	};
	//Multisampling
	const VkPipelineMultisampleStateCreateInfo multisample = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0,
		VK_SAMPLE_COUNT_4_BIT,
		false,
		1,
		NULL,
		false,
		false
	};
	//Depth stencil
	//TODO: Stenciling
	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, NULL, 0,
		true,
		true,
		VK_COMPARE_OP_LESS,
		false,
		false
	};
	//Color blending
	const VkPipelineColorBlendAttachmentState color_blend_attachment = {
		true,
		//Color
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_ADD,
		//Alpha
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT
	};
	const VkPipelineColorBlendStateCreateInfo color_blend = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0,
		false,
		VK_LOGIC_OP_COPY,
		1, &color_blend_attachment,
		0 //?
	};

	//Create pipeline
	VkPipeline pipeline;
	const VkGraphicsPipelineCreateInfo pipeline_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0,
		2, shader_stages,
		&vertex_input,
		&input_assembly,
		NULL,
		&viewport,
		&rasterization,
		&multisample,
		&depth_stencil,
		&color_blend,
		NULL,
		r->pipeline_layout,
		r->render_pass,
		0
	};
	const VkResult result = vkCreateGraphicsPipelines(
		r->device, r->pipeline_cache, 1, &pipeline_info, NULL, &r->pipeline
	);

	//Cleanup
	vkDestroyShaderModule(r->device, vertex_shader, NULL);
	vkDestroyShaderModule(r->device, fragment_shader, NULL);
	return result;
}

static void record_blit_commands(
	struct Renderer* const r,
	VkCommandBuffer command_buffer,
	unsigned image_index) {
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);
	//Blit render target to swapchain image
	const VkImageSubresourceRange color_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1,
		0, 1
	};
	const VkImageMemoryBarrier image_barriers[] = {
		//Render target
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->images[1],
			color_subresource_range
		},
		//Swapchain image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, //VK_ACCESS_NONE_KHR,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->swapchain_images[image_index],
			color_subresource_range
		},
	};
	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, NULL,
		0, NULL,
		2, image_barriers
	);
	const VkImageSubresourceLayers subresource = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 0, 1
	};
	const VkImageBlit region = {
		subresource,
		{{0, 0, 0}, {r->resolution.width, r->resolution.height, 1}},
		subresource,
		{{0, 0, 0}, {r->surface_extent.width, r->surface_extent.height, 1}}
	};
	vkCmdBlitImage(
		command_buffer,
		r->images[1],
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		r->swapchain_images[image_index],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region,
		VK_FILTER_NEAREST
	);
	const VkImageMemoryBarrier present_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_HOST_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		r->graphics_queue_family,
		r->graphics_queue_family,
		r->swapchain_images[image_index],
		color_subresource_range
	};
	vkCmdPipelineBarrier(
		command_buffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_HOST_BIT,
		0,
		0, NULL,
		0, NULL,
		1, &present_barrier
	);
	vkEndCommandBuffer(command_buffer);
}

static bool create_swapchain(struct Renderer* const r, bool old) {
	if (vkDeviceWaitIdle(r->device) != VK_SUCCESS) return true;

	//Surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		r->physical_device,
		r->surface,
		&surface_capabilities
	);
	//Surface format
	unsigned format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface, &format_count, NULL);
	VkSurfaceFormatKHR *formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(r->physical_device, r->surface, &format_count, formats);
	VkSurfaceFormatKHR format = formats[0];
	for (unsigned i = 0; i < format_count; ++i) {
		VkSurfaceFormatKHR f = formats[i];
		if (f.format == VK_FORMAT_B8G8R8A8_SRGB
			&& f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			format = format;
			break;
		}
	}
	free(formats);
	//Surface extent
	VkExtent2D extent = surface_capabilities.currentExtent;
	int drawable_width, drawable_height;
	SDL_Vulkan_GetDrawableSize(r->window, &drawable_width, &drawable_height);
	//Special case
	if (surface_capabilities.currentExtent.width == UINT32_MAX) {
		if (drawable_width > surface_capabilities.maxImageExtent.width)
			extent.width = surface_capabilities.maxImageExtent.width;
		else if (drawable_width < surface_capabilities.minImageExtent.width)
			extent.width = surface_capabilities.minImageExtent.width;
		else extent.width = drawable_width;
	}
	if (surface_capabilities.currentExtent.height == UINT32_MAX) {
		if (drawable_height > surface_capabilities.maxImageExtent.height)
			extent.height = surface_capabilities.maxImageExtent.height;
		else if (drawable_height < surface_capabilities.minImageExtent.height)
			extent.height = surface_capabilities.minImageExtent.height;
		else extent.height = drawable_height;
	}
	r->surface_extent = extent;

	//Swapchain
	unsigned queue_family_indices[2] = {
		r->graphics_queue_family,
		r->present_queue_family
	};
	VkSwapchainCreateInfoKHR swapchain_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0,
		r->surface,
		surface_capabilities.minImageCount,
		format.format,
		format.colorSpace,
		extent,
		1,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		1,
		queue_family_indices,
		surface_capabilities.currentTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		false,
		old ? r->swapchain : NULL
	};
	//Image sharing mode
	if (r->graphics_queue_family != r->present_queue_family) {
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = 2;
	}
	VkSwapchainKHR swapchain;
	vkCreateSwapchainKHR(
		r->device,
		&swapchain_info,
		NULL,
		&swapchain
	);
	if (old) vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
	r->swapchain = swapchain;

	//Swapchain images
	vkGetSwapchainImagesKHR(r->device, r->swapchain, &r->swapchain_image_count, NULL);
	r->swapchain_images = malloc(r->swapchain_image_count * sizeof(VkImage));
	vkGetSwapchainImagesKHR(
		r->device,
		r->swapchain,
		&r->swapchain_image_count,
		r->swapchain_images
	);

	//Blit command buffers
	r->blit_command_buffers = malloc(r->swapchain_image_count * sizeof(VkCommandBuffer));
	const VkCommandBufferAllocateInfo command_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r->blit_command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		r->swapchain_image_count
	};
	vkAllocateCommandBuffers(r->device, &command_buffer_alloc_info, r->blit_command_buffers);
	for (unsigned i = 0; i < r->swapchain_image_count; ++i)
		record_blit_commands(r, r->blit_command_buffers[i], i);

	return false;
}

static void destroy_swapchain(struct Renderer* const r, bool preserve) {
	if (!preserve) vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
	free(r->swapchain_images);
	vkFreeCommandBuffers(
		r->device,
		r->blit_command_pool,
		r->swapchain_image_count,
		r->blit_command_buffers
	);
	free(r->blit_command_buffers);
}

static void save_pipeline_cache(struct Renderer* const r) {
	FILE* const pipeline_cache_file = fopen(PIPELINE_CACHE_FILENAME, "wb");
	size_t data_size;
	vkGetPipelineCacheData(r->device, r->pipeline_cache, &data_size, NULL);
	void* const data = malloc(data_size);
	vkGetPipelineCacheData(r->device, r->pipeline_cache, &data_size, data);
	fwrite(data, data_size, 1, pipeline_cache_file);
	free(data);
	fclose(pipeline_cache_file);
}

static void staged_buffer_write(
	//Vulkan objects
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
	VkCommandBuffer* const command_buffer,
	VkQueue* const queue,
	//Parameters
	const unsigned count,
	VkBuffer* const dst_buffers,
	const void** const data,
	const VkDeviceSize* sizes) {
	VkDeviceSize total_size = 0;
	VkDeviceSize* const offsets = malloc(count * sizeof(VkDeviceSize));
	for (unsigned i = 0; i < count; ++i) {
		offsets[i] = total_size;
		total_size += sizes[i];
	}
	//Create staging buffer
	const VkBufferCreateInfo staging_buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		total_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	VkBuffer staging_buffer;
	struct Allocation staging_alloc;
	create_buffers(
		physical_device, device,
		1, &staging_buffer_info,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer,
		&staging_alloc
	);
	//Write to staging buffer
	void* buffer_data;
	for (unsigned i = 0; i < count; ++i) {
		vkMapMemory(*device, staging_alloc.memory, offsets[i], sizes[i], 0, &buffer_data);
		memcpy(buffer_data, data[i], sizes[i]);
		vkUnmapMemory(*device, staging_alloc.memory);
	}
	//Record commands for transfer
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(*command_buffer, &begin_info);
	for (unsigned i = 0; i < count; ++i) {
		const VkBufferCopy region = {offsets[i], 0, sizes[i]};
		vkCmdCopyBuffer(*command_buffer, staging_buffer, dst_buffers[i], 1, &region);
	}
	vkEndCommandBuffer(*command_buffer);
	//Submit to queue
	const VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		0, NULL,
		0,
		1, command_buffer,
		0, NULL
	};
	vkQueueSubmit(*queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(*queue); //TODO: Better synchronization
	free(offsets);
	vkDestroyBuffer(*device, staging_buffer, NULL);
	free_allocation(*device, staging_alloc);
}

static void record_draw_commands(
	struct Renderer* const r,
	VkCommandBuffer command_buffer,
	unsigned draw_count) {
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, 0
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);

	//Copy staging buffer to render buffers
	//Staging buffer memory barrier
	const VkBufferMemoryBarrier2 staging_buffer_barrier = {
		VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, NULL,
		VK_PIPELINE_STAGE_2_HOST_BIT,
		VK_ACCESS_2_HOST_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT,
		r->graphics_queue_family,
		r->graphics_queue_family,
		r->staging_buffer,
		0, VK_WHOLE_SIZE
	};
	const VkDependencyInfo staging_buffer_dependency_info = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		1, &staging_buffer_barrier,
		0, NULL
	};
	vkCmdPipelineBarrier2(command_buffer, &staging_buffer_dependency_info);
	//Camera
	const VkBufferCopy camera_region = {0, 0, sizeof(mat4)};
	vkCmdCopyBuffer(command_buffer, r->staging_buffer, r->uniform_buffer, 1, &camera_region);
	//Node transformations
	const VkBufferCopy nodes_region = {sizeof(mat4), 0, draw_count * sizeof(mat4)};
	vkCmdCopyBuffer(command_buffer, r->staging_buffer, r->storage_buffers[3], 1, &nodes_region);
	//Memory barrier
	const VkBufferMemoryBarrier2 shader_buffer_barriers[] = {
		//Uniform buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_UNIFORM_READ_BIT,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->uniform_buffer,
			0, VK_WHOLE_SIZE
		},
		//Node storage buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->storage_buffers[3],
			0, VK_WHOLE_SIZE
		}
	};
	const VkDependencyInfo shader_buffer_dependency_info = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		2, shader_buffer_barriers,
		0, NULL
	};
	vkCmdPipelineBarrier2(command_buffer, &shader_buffer_dependency_info);

	//Render pass
	const VkClearValue clear_values[3] = {
		{0, 0, 0, 1}, //Color
		{0, 0, 0, 1}, //Resolve
		{1, 0} //Depth
	};
	const VkRenderPassBeginInfo render_pass_begin_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		r->render_pass,
		r->framebuffer,
		{{0, 0}, r->resolution},
		3, clear_values
	};
	vkCmdBeginRenderPass(
		command_buffer,
		&render_pass_begin_info,
		VK_SUBPASS_CONTENTS_INLINE
	);
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);
	//Bind descriptors
	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		r->pipeline_layout,
		0,
		1, &r->descriptor_set,
		0, NULL
	);
	//Vertex buffers
	const VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(
		command_buffer,
		0,
		1, &r->storage_buffers[0], offsets
	);
	vkCmdBindIndexBuffer(
		command_buffer,
		r->storage_buffers[1],
		0,
		VK_INDEX_TYPE_UINT32
	);
	//Drawing
	vkCmdDrawIndexedIndirect(
		command_buffer,
		r->storage_buffers[4],
		0,
		draw_count,
		sizeof(VkDrawIndexedIndirectCommand)
	);
	vkCmdEndRenderPass(command_buffer);
	vkEndCommandBuffer(command_buffer);
}

bool create_renderer(SDL_Window* window, struct Renderer* const result) {
	struct Renderer r;
	r.window = window;

	//Vulkan instance
	const VkApplicationInfo app_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
		"Lightrail", 1,
		"Lightrail", 1,
		VK_API_VERSION_1_3
	};
	//Layers
	const char* layer_names[] = {"VK_LAYER_KHRONOS_validation"};
	//Extensions
	unsigned ext_count;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, NULL)) {
		fprintf(stderr, "Error getting instance extension count!\n");
		return true;
	}
	const char** ext_names = malloc(ext_count * sizeof(char*));
	if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, ext_names)) {
		fprintf(stderr, "Error getting instance extension names!\n");
		return true;
	}
	VkInstanceCreateInfo instance_info = {
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL,
		0,
		&app_info,
		1, layer_names,
		ext_count, ext_names
	};
	vkCreateInstance(&instance_info, NULL, &r.instance);
	free(ext_names);

	//Surface
	SDL_Vulkan_CreateSurface(window, r.instance, &r.surface);

	//Physical device selection
	unsigned device_count;
	vkEnumeratePhysicalDevices(r.instance, &device_count, NULL);
	VkPhysicalDevice* devices = malloc(device_count * sizeof(VkPhysicalDevice));
	vkEnumeratePhysicalDevices(r.instance, &device_count, devices);
	//Device info
	bool device_found = false;
	const char* required_extensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME
	};
	unsigned format_count, present_mode_count;
	for (size_t i = 0; i < device_count; ++i) {
		const VkPhysicalDevice device = devices[i];
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);
		//Extension support
		unsigned ext_count;
		vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);
		VkExtensionProperties* extensions = malloc(ext_count * sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, extensions);
		unsigned supported_ext_count = 0;
		for (unsigned i = 0; i < ext_count; ++i) {
			const char* ext_name = extensions[i].extensionName;
			//Iterate over required extensions
			for (unsigned j = 0; j < REQUIRED_EXT_COUNT; ++j) {
				if (!strcmp(ext_name, required_extensions[j])) {
					++supported_ext_count;
					break;
				}
			}
		}
		free(extensions);
		bool extension_support = supported_ext_count == REQUIRED_EXT_COUNT;

		//Queue families
		unsigned queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
		VkQueueFamilyProperties* queue_families
			= malloc(queue_family_count * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(
			device,
			&queue_family_count,
			queue_families
		);
		//Graphics queue
		bool has_graphics_queue = false;
		for (unsigned i = 0; i < queue_family_count; ++i) {
			VkQueueFamilyProperties queue_family = queue_families[i];
			if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				has_graphics_queue = true;
				r.graphics_queue_family = i;
				break;
			}
		}
		//Present queue
		bool has_present_queue = false;
		for (unsigned i = 0; i < queue_family_count; ++i) {
			VkQueueFamilyProperties queue_family = queue_families[i];
			VkBool32 surface_support;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, r.surface, &surface_support);
			if (surface_support) {
				has_present_queue = true;
				r.present_queue_family = i;
				break;
			}
		}
		free(queue_families);

		//Swapchain support
		bool swapchain_support = false;
		if (extension_support) {
			vkGetPhysicalDeviceSurfaceFormatsKHR(
				device,
				r.surface,
				&format_count,
				NULL
			);
			vkGetPhysicalDeviceSurfacePresentModesKHR(
				device,
				r.surface,
				&present_mode_count,
				NULL
			);
			swapchain_support = format_count && present_mode_count;
		}

		//Judgement
		if (
			extension_support
			&& has_graphics_queue
			&& has_present_queue
			&& swapchain_support
		) {
			r.physical_device = device;
			device_found = true;
			break;
		}
	}
	if (!device_found) {
		fprintf(stderr, "No usable graphics device found!\n");
		return true;
	}
	free(devices);

	//Queues
	const float QUEUE_PRIORITY = 1.0f;
	VkDeviceQueueCreateInfo queue_info = {
		VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0,
		r.graphics_queue_family,
		1,
		&QUEUE_PRIORITY
	};
	VkDeviceQueueCreateInfo queue_infos[2];
	queue_infos[0] = queue_info;
	unsigned queue_info_count = 1;
	//Graphics & present queues are different
	if (r.graphics_queue_family != r.present_queue_family) {
		queue_info_count = 2;
		queue_infos[1] = queue_info;
		queue_infos[1].queueFamilyIndex = r.present_queue_family;
	}
	//Logical device
	const VkPhysicalDeviceFeatures features = {
		.multiDrawIndirect = true,
		.fillModeNonSolid = true //FIXME: Debug
	};
	const VkPhysicalDeviceVulkan13Features features_13 = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, NULL,
		.synchronization2 = true
	};
	const VkDeviceCreateInfo device_info = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		&features_13,
		0,
		queue_info_count, queue_infos,
		0, NULL, //Layers
		REQUIRED_EXT_COUNT, required_extensions, //Extensions
		&features //Features
	};
	vkCreateDevice(r.physical_device, &device_info, NULL, &r.device);
	//Queue handles
	vkGetDeviceQueue(r.device, r.graphics_queue_family, 0, &r.graphics_queue);
	vkGetDeviceQueue(r.device, r.present_queue_family, 0, &r.present_queue);

	//Pipeline cache
	FILE* const pipeline_cache_file = fopen(PIPELINE_CACHE_FILENAME, "rb");
	if (pipeline_cache_file) {
		fseek(pipeline_cache_file, 0, SEEK_END);
		const long data_size = ftell(pipeline_cache_file);
		void* const data = malloc(data_size);
		const VkPipelineCacheCreateInfo pipeline_cache_create_info = {
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, NULL, 0,
			data_size,
			data
		};
		vkCreatePipelineCache(r.device, &pipeline_cache_create_info, NULL, &r.pipeline_cache);
		free(data);
		fclose(pipeline_cache_file);
	} else {
		const VkPipelineCacheCreateInfo pipeline_cache_create_info = {
			VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, NULL, 0,
			0,
			0
		};
		vkCreatePipelineCache(r.device, &pipeline_cache_create_info, NULL, &r.pipeline_cache);
	}
	
	//Command pool
	const VkCommandPoolCreateInfo pool_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		r.graphics_queue_family
	};
	vkCreateCommandPool(r.device, &pool_info, NULL, &r.command_pool);

	//Blit command pool
	const VkCommandPoolCreateInfo blit_pool_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0,
		r.graphics_queue_family
	};
	vkCreateCommandPool(r.device, &blit_pool_info, NULL, &r.blit_command_pool);
	r.blit_command_buffers = NULL;

	//Command buffer allocation
	const VkCommandBufferAllocateInfo command_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r.command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		COMMAND_BUFFER_COUNT
	};
	vkAllocateCommandBuffers(r.device, &command_buffer_alloc_info, r.command_buffers);

	//Semaphores
	const VkSemaphoreCreateInfo semaphore_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0
	};
	for (unsigned i = 0; i < 2; ++i)
		vkCreateSemaphore(r.device, &semaphore_info, NULL, r.semaphores + i);

	//Descriptor set layout
	const VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {
		//Camera
		{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL},
		//Nodes
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL},
	};
	const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0,
		2, descriptor_set_layout_bindings
	};
	vkCreateDescriptorSetLayout(r.device, &descriptor_set_layout_info, NULL, &r.descriptor_set_layout);

	//Pipeline layout
	const VkPipelineLayoutCreateInfo layout_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
		1, &r.descriptor_set_layout,
		0, NULL
	};
	vkCreatePipelineLayout(r.device, &layout_info, NULL, &r.pipeline_layout);

	//Descriptor pool
	const VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
	};
	const VkDescriptorPoolCreateInfo descriptor_pool_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0,
		1, 2, pool_sizes
	};
	vkCreateDescriptorPool(r.device, &descriptor_pool_info, NULL, &r.descriptor_pool);
	
	//Allocate descriptor set
	const VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL,
		r.descriptor_pool,
		1, &r.descriptor_set_layout
	};
	vkAllocateDescriptorSets(r.device, &descriptor_set_alloc_info, &r.descriptor_set);

	//Uniform buffer
	const VkBufferCreateInfo uniform_buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		sizeof(mat4),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	create_buffers(
		&r.physical_device,
		&r.device,
		1, &uniform_buffer_info,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		&r.uniform_buffer,
		&r.uniform_alloc
	);

	//Update uniform descriptor
	const VkDescriptorBufferInfo buffer_info = {r.uniform_buffer, 0, VK_WHOLE_SIZE};
	const VkWriteDescriptorSet descriptor_write = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
		r.descriptor_set,
		0,
		0,
		1,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		NULL,
		&buffer_info,
		NULL
	};
	vkUpdateDescriptorSets(r.device, 1, &descriptor_write, 0, NULL);

	//TODO: Multisampling
	renderer_create_resolution(&r, 1024, 1024); //TODO: Resolution setting
	create_swapchain(&r, false);

	*result = r;
	return false;
}

void destroy_renderer(struct Renderer r) {
	save_pipeline_cache(&r);
	renderer_destroy_scene(&r);
	//Uniform data
	vkDestroyBuffer(r.device, r.uniform_buffer, NULL);
	free_allocation(r.device, r.uniform_alloc);
	//Vulkan objects
	vkDestroyPipelineCache(r.device, r.pipeline_cache, NULL);
	destroy_swapchain(&r, false);
	renderer_destroy_resolution(&r);
	vkDestroyDescriptorPool(r.device, r.descriptor_pool, NULL);
	vkDestroyPipelineLayout(r.device, r.pipeline_layout, NULL);
	vkDestroyDescriptorSetLayout(r.device, r.descriptor_set_layout, NULL);
	for (unsigned i = 0; i < 2; ++i)
		vkDestroySemaphore(r.device, r.semaphores[i], NULL);
	vkDestroyCommandPool(r.device, r.command_pool, NULL);
	vkDestroyCommandPool(r.device, r.blit_command_pool, NULL);
	vkDestroyDevice(r.device, NULL);
	vkDestroySurfaceKHR(r.instance, r.surface, NULL);
	vkDestroyInstance(r.instance, NULL);
}

VkResult renderer_create_resolution(
	struct Renderer* const r,
	unsigned width,
	unsigned height) {
	r->resolution = (VkExtent2D) {width, height};
	VkExtent3D extent_3d = {width, height, 1};

	//Render pass attachments
	const VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB,
		depth_format = VK_FORMAT_D32_SFLOAT;
	const VkImageCreateInfo image_infos[3] = {
		//Color image
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			color_format,
			extent_3d,
			1,
			1,
			VK_SAMPLE_COUNT_4_BIT, //TODO: Multisampling
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		},
		//Resolve image
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			color_format,
			extent_3d,
			1,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		},
		//Depth image
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			depth_format,
			extent_3d,
			1,
			1,
			VK_SAMPLE_COUNT_4_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		}
	};
	create_images(
		&r->physical_device,
		&r->device,
		3, image_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		r->images,
		&r->image_mem
	);

	//Create image views
	const VkComponentMapping component_mapping = {
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY
	};
	const VkImageSubresourceRange color_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1,
		0, 1
	};
	const VkImageViewCreateInfo image_view_infos[] = {
		//Color image
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
			r->images[0],
			VK_IMAGE_VIEW_TYPE_2D,
			image_infos[0].format,
			component_mapping,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		},
		//Resolve image
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
			r->images[1],
			VK_IMAGE_VIEW_TYPE_2D,
			image_infos[1].format,
			component_mapping,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		},
		//Depth image
		{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
			r->images[2],
			VK_IMAGE_VIEW_TYPE_2D,
			image_infos[2].format,
			component_mapping,
			{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
		}
	};
	for (unsigned i = 0; i < 3; ++i)
		vkCreateImageView(r->device, image_view_infos + i, NULL, r->image_views + i);

	//Attachment descriptions
	const VkAttachmentDescription attachments[] = {
		//Color attachment
		{
			0,
			image_infos[0].format,
			image_infos[0].samples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		},
		//Resolve attachment
		{
			0,
			image_infos[1].format,
			image_infos[1].samples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		},
		//Depth attachment
		{
			0,
			image_infos[2].format,
			image_infos[2].samples,
			VK_ATTACHMENT_LOAD_OP_CLEAR,
			VK_ATTACHMENT_STORE_OP_STORE,
			VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VK_ATTACHMENT_STORE_OP_DONT_CARE,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		}
	};

	//Attachment references
	const VkAttachmentReference color_attachment = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	const VkAttachmentReference resolve_attachment = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	const VkAttachmentReference depth_attachment = {2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	//Subpass
	const VkSubpassDescription subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, NULL,
		1, &color_attachment,
		&resolve_attachment,
		&depth_attachment,
		0, NULL
	};

	//Create render pass
	const VkRenderPassCreateInfo render_pass_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0,
		3, attachments,
		1, &subpass_description,
		0, NULL
	};
	vkCreateRenderPass(r->device, &render_pass_info, NULL, &r->render_pass);

	//Create framebuffer
	const VkFramebufferCreateInfo framebuffer_info = {
		VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
		r->render_pass,
		3, r->image_views,
		width, height, 1
	};
	vkCreateFramebuffer(r->device, &framebuffer_info, NULL, &r->framebuffer);

	create_pipeline(r);
	return VK_SUCCESS;
}

void renderer_destroy_resolution(struct Renderer* const r) {
	vkDestroyPipeline(r->device, r->pipeline, NULL);
	vkDestroyFramebuffer(r->device, r->framebuffer, NULL);
	vkDestroyRenderPass(r->device, r->render_pass, NULL);
	for (unsigned i = 0; i < 3; ++i) {
		vkDestroyImageView(r->device, r->image_views[i], NULL);
		vkDestroyImage(r->device, r->images[i], NULL);
	}
	free_allocation(r->device, r->image_mem);
}

void renderer_draw(struct Renderer* const r) {
	//Acquire swapchain image
	unsigned image_index;
	VkResult swapchain_status = VK_ERROR_UNKNOWN;
	while (swapchain_status != VK_SUCCESS && swapchain_status != VK_SUBOPTIMAL_KHR) {
		swapchain_status = vkAcquireNextImageKHR(
			r->device, r->swapchain, 0, r->semaphores[0], NULL, &image_index
		);
		if (swapchain_status == VK_ERROR_OUT_OF_DATE_KHR) {
			//Recreate swapchain
			vkQueueWaitIdle(r->present_queue);
			destroy_swapchain(r, true);
			create_swapchain(r, true);
		}
	}
	//Submit command buffer to queue
	const VkPipelineStageFlagBits wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	const VkCommandBuffer command_buffers[] = {
		r->command_buffers[0],
		r->blit_command_buffers[image_index]
	};
	const VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		1, &r->semaphores[0],
		wait_stages,
		2, command_buffers,
		1, &r->semaphores[1]
	};
	vkQueueSubmit(r->graphics_queue, 1, &submit_info, NULL);
	//Presentation
	const VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		1, &r->semaphores[1],
		1, &r->swapchain,
		&image_index,
		NULL
	};
	vkQueuePresentKHR(r->present_queue, &present_info);
	//Wait
	//TODO: Frames-in-flight
	vkQueueWaitIdle(r->present_queue);
}

void renderer_load_scene(struct Renderer* const r, struct Scene scene) {
	unsigned vertex_count = 0, index_count = 0;
	//Create local meshes
	struct LocalMesh* const local_meshes = malloc(scene.mesh_count * sizeof(struct LocalMesh));
	VkDrawIndexedIndirectCommand* const mesh_draw_commands =
		malloc(scene.mesh_count * sizeof(VkDrawIndexedIndirectCommand));
	for (unsigned i = 0; i < scene.mesh_count; ++i) {
		const struct Mesh mesh = scene.meshes[i];
		struct LocalMesh local_mesh = {vertex_count, 0, index_count, 0};
		for (unsigned i = 0; i < mesh.primitive_count; ++i) {
			const struct Primitive primitive = mesh.primitives[i];
			local_mesh.vertex_count += primitive.vertex_count;
			local_mesh.index_count += primitive.index_count;
		}
		vertex_count += local_mesh.vertex_count;
		index_count += local_mesh.index_count;
		//Mesh draw command
		mesh_draw_commands[i] = (VkDrawIndexedIndirectCommand) {
			local_mesh.index_count,
			1,
			local_mesh.index_offset,
			local_mesh.vertex_offset,
			0
		};
		local_meshes[i] = local_mesh;
	}
	//Fill vertex & index buffers
	struct Vertex* const vertices = malloc(vertex_count * sizeof(struct Vertex));
	unsigned* const indices = malloc(index_count * sizeof(unsigned));
	vertex_count = 0;
	index_count = 0;
	for (unsigned i = 0; i < scene.mesh_count; ++i) {
		const struct Mesh mesh = scene.meshes[i];
		for (unsigned i = 0; i < mesh.primitive_count; ++i) {
			const struct Primitive primitive = mesh.primitives[i];
			memcpy(
				vertices + vertex_count,
				primitive.vertices,
				primitive.vertex_count * sizeof(struct Vertex)
			);
			memcpy(
				indices + index_count,
				primitive.indices,
				primitive.index_count * sizeof(unsigned)
			);
			vertex_count += primitive.vertex_count;
			index_count += primitive.index_count;
		}
	}
	//Create local nodes & draw commands
	struct LocalNode* const local_nodes = malloc(scene.node_count * sizeof(struct LocalNode));
	VkDrawIndexedIndirectCommand* const draw_commands =
		malloc(scene.node_count * sizeof(VkDrawIndexedIndirectCommand));
	for (unsigned i = 0; i < scene.node_count; ++i) {
		struct Node node = scene.nodes[i];
		struct LocalNode local_node;
		glm_mat4_copy(node.transformation, local_node.transformation);
		local_nodes[i] = local_node;
		draw_commands[i] = mesh_draw_commands[node.mesh];
	}
	free(mesh_draw_commands);
	//Allocate storage buffers
	const VkBufferCreateInfo buffer_infos[] = {
		//Vertex buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			vertex_count * sizeof(struct Vertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Index buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			index_count * sizeof(unsigned),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Mesh buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.mesh_count * sizeof(struct LocalMesh),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Node buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.node_count * sizeof(struct LocalNode),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Draw command buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.node_count * sizeof(VkDrawIndexedIndirectCommand),
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		}
	};
	create_buffers(
		&r->physical_device,
		&r->device,
		SCENE_BUFFER_COUNT, buffer_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		r->storage_buffers,
		&r->storage_alloc
	);
	//Write to storage buffers
	const void* data[] = {vertices, indices, local_meshes, local_nodes, draw_commands};
	const size_t sizes[] = {
		buffer_infos[0].size, 
		buffer_infos[1].size, 
		buffer_infos[2].size, 
		buffer_infos[3].size,
		buffer_infos[4].size
	};
	staged_buffer_write(
		&r->physical_device,
		&r->device,
		&r->command_buffers[1],
		&r->graphics_queue,
		SCENE_BUFFER_COUNT, r->storage_buffers, data, sizes
	);
	//Cleanup
	free(local_meshes);
	free(vertices);
	free(indices);
	free(local_nodes);
	free(draw_commands);
	//Create staging buffer
	const VkBufferCreateInfo staging_buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		sizeof(mat4) + scene.node_count * sizeof(struct LocalNode),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	create_buffers(
		&r->physical_device,
		&r->device,
		1, &staging_buffer_info,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&r->staging_buffer,
		&r->staging_alloc
	);
	//Update descriptors
	const VkDescriptorBufferInfo buffer_info = {r->storage_buffers[3], 0, VK_WHOLE_SIZE};
	const VkWriteDescriptorSet descriptor_write = {
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
		r->descriptor_set,
		1,
		0,
		1,
		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		NULL,
		&buffer_info,
		NULL
	};
	vkUpdateDescriptorSets(r->device, 1, &descriptor_write, 0, NULL);
	//Prime staging buffer
	renderer_update_nodes(r, scene);
	//Re-record draw commands
	record_draw_commands(r, r->command_buffers[0], scene.node_count);
}

void renderer_destroy_scene(struct Renderer* const r) {
	vkResetCommandBuffer(r->command_buffers[0], 0);
	//Staging buffer
	vkDestroyBuffer(r->device, r->staging_buffer, NULL);
	free_allocation(r->device, r->staging_alloc);
	//Storage buffers
	for (unsigned i = 0; i < SCENE_BUFFER_COUNT; ++i)
		vkDestroyBuffer(r->device, r->storage_buffers[i], NULL);
	free_allocation(r->device, r->storage_alloc);
}

//Write camera matrix to staging buffer
void renderer_update_camera(struct Renderer* const r, const struct Camera camera) {
	mat4 transformation;
	camera_transform(camera, transformation);
	void* data;
	vkMapMemory(r->device, r->staging_alloc.memory, 0, sizeof(mat4), 0, &data);
	memcpy(data, transformation, sizeof(mat4));
	vkUnmapMemory(r->device, r->staging_alloc.memory);
}

//Write node transformations to staging buffer
void renderer_update_nodes(struct Renderer* const r, const struct Scene scene) {
	//Create local nodes
	const size_t local_nodes_size = scene.node_count * sizeof(struct LocalNode);
	struct LocalNode* const local_nodes = malloc(local_nodes_size);
	for (unsigned i = 0; i < scene.node_count; ++i) {
		struct Node node = scene.nodes[i];
		struct LocalNode local_node;
		glm_mat4_copy(node.transformation, local_node.transformation);
		local_nodes[i] = local_node;
	}
	//Write to staging buffer
	void* data;
	vkMapMemory(
		r->device,
		r->staging_alloc.memory,
		sizeof(mat4),
		scene.node_count * sizeof(mat4),
		0,
		&data
	);
	memcpy(data, local_nodes, local_nodes_size);
	vkUnmapMemory(r->device, r->staging_alloc.memory);
	free(local_nodes);
}
