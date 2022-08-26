#include "renderer.h"
#include "vulkan/vulkan_core.h"
#include <stdio.h>
#include <SDL2/SDL_vulkan.h>
#include <cglm/mat4.h>

#define REQUIRED_EXT_COUNT 2
#define MAX_TEXTURE_COUNT 8

struct LocalCamera {
	mat4 view, projection;
};

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
	const VkVertexInputAttributeDescription attribute_descriptions[] = {
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, pos)}, //Position
		{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, normal)}, //Normal
		{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(struct Vertex, tex)}, //Texture
		{3, 0, VK_FORMAT_R32_UINT, offsetof(struct Vertex, material)} //Material
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
		1, &binding_description,
		4, attribute_descriptions
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
		VK_POLYGON_MODE_FILL,
		//VK_POLYGON_MODE_LINE,
		VK_CULL_MODE_BACK_BIT,
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

static void create_resolution(struct Renderer* const r, unsigned width, unsigned height) {
	r->resolution = (VkExtent2D) {width, height};
	//Attachment descriptions
	const VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB,
		depth_format = VK_FORMAT_D32_SFLOAT;
	const VkAttachmentDescription attachments[] = {
		//Color attachment
		{
			0,
			color_format,
			VK_SAMPLE_COUNT_4_BIT, //TODO: Multisampling
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
			color_format,
			VK_SAMPLE_COUNT_1_BIT,
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
			depth_format,
			VK_SAMPLE_COUNT_4_BIT,
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
	//Pipeline
	create_pipeline(r);
}

static void destroy_resolution(struct Renderer* const r) {
	vkDestroyPipeline(r->device, r->pipeline, NULL);
	vkDestroyRenderPass(r->device, r->render_pass, NULL);
}

static void create_frames(struct Renderer* const r, unsigned frame_count) {
	r->frame_count = frame_count;
	r->current_frame = 0;
	//Images
	const VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB,
		depth_format = VK_FORMAT_D32_SFLOAT;
	const VkExtent3D extent_3d = {
		r->resolution.width,
		r->resolution.height,
		1
	};
	const VkImageCreateInfo image_infos[] = {
		//Color image
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			color_format,
			extent_3d,
			1,
			1,
			VK_SAMPLE_COUNT_4_BIT, //TODO: Multisampling config
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
	VkImageCreateInfo* restrict const all_image_infos = malloc(frame_count * sizeof(image_infos));
	for (unsigned i = 0; i < frame_count; ++i)
		memcpy(all_image_infos + 3 * i, image_infos, sizeof(image_infos));
	r->images = malloc(3 * frame_count * sizeof(VkImage));
	create_images(
		r->physical_device,
		r->device,
		3 * frame_count, all_image_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		r->images,
		&r->image_alloc
	);
	//Image views
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
	r->image_views = malloc(3 * frame_count * sizeof(VkImageView));
	for (unsigned i = 0; i < frame_count; ++i) {
		const VkImageViewCreateInfo image_view_infos[] = {
			//Color image
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
				r->images[3 * i],
				VK_IMAGE_VIEW_TYPE_2D,
				image_infos[0].format,
				component_mapping,
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
			},
			//Resolve image
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
				r->images[3 * i + 1],
				VK_IMAGE_VIEW_TYPE_2D,
				image_infos[1].format,
				component_mapping,
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
			},
			//Depth image
			{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
				r->images[3 * i + 2],
				VK_IMAGE_VIEW_TYPE_2D,
				image_infos[2].format,
				component_mapping,
				{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1}
			}
		};
		for (unsigned j = 0; j < 3; ++j)
			vkCreateImageView(r->device, image_view_infos + j, NULL, r->image_views + 3 * i + j);
	}
	//Framebuffers
	r->framebuffers = malloc(frame_count * sizeof(VkFramebuffer));
	for (unsigned i = 0; i < frame_count; ++i) {
		const VkFramebufferCreateInfo framebuffer_info = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0,
			r->render_pass,
			3, r->image_views + 3 * i,
			r->resolution.width, r->resolution.height, 1
		};
		vkCreateFramebuffer(r->device, &framebuffer_info, NULL, r->framebuffers + i);
	}
	//Command buffers
	const VkCommandBufferAllocateInfo command_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r->command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		frame_count
	};
	r->command_buffers = malloc(frame_count * sizeof(VkCommandBuffer));
	vkAllocateCommandBuffers(r->device, &command_buffer_alloc_info, r->command_buffers);
	//Descriptor pool
	const VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame_count},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * frame_count},
		{VK_DESCRIPTOR_TYPE_SAMPLER, frame_count},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_COUNT * frame_count}
	};
	const VkDescriptorPoolCreateInfo descriptor_pool_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0,
		frame_count,
		4, pool_sizes
	};
	vkCreateDescriptorPool(r->device, &descriptor_pool_info, NULL, &r->descriptor_pool);
	//Allocate descriptor sets
	VkDescriptorSetLayout* restrict const all_descriptor_set_layouts = malloc(frame_count * sizeof(VkDescriptorSetLayout));
	for (unsigned i = 0; i < frame_count; ++i)
		memcpy(all_descriptor_set_layouts + i, &r->descriptor_set_layout, sizeof(VkDescriptorSetLayout));
	const VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL,
		r->descriptor_pool,
		frame_count, all_descriptor_set_layouts
	};
	r->descriptor_sets = malloc(frame_count * sizeof(VkDescriptorSet));
	vkAllocateDescriptorSets(r->device, &descriptor_set_alloc_info, r->descriptor_sets);
	free(all_descriptor_set_layouts);
	//Semaphores
	const VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0};
	r->semaphores = malloc(2 * frame_count * sizeof(VkSemaphore));
	for (unsigned i = 0; i < 2 * frame_count; ++i)
		vkCreateSemaphore(r->device, &semaphore_info, NULL, r->semaphores + i);
	//Fences
	const VkFenceCreateInfo fence_info = {
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		NULL,
		VK_FENCE_CREATE_SIGNALED_BIT
	};
	r->fences = malloc(frame_count * sizeof(VkFence));
	for (unsigned i = 0; i < frame_count; ++i)
		vkCreateFence(r->device, &fence_info, NULL, r->fences + i);
}

static void destroy_frames(struct Renderer* const r) {
	const unsigned frame_count = r->frame_count;
	//Fences
	for (unsigned i = 0; i < frame_count; ++i)
		vkDestroyFence(r->device, r->fences[i], NULL);
	free(r->fences);
	//Semaphores
	for (unsigned i = 0; i < 2 * frame_count; ++i)
		vkDestroySemaphore(r->device, r->semaphores[i], NULL);
	free(r->semaphores);
	//Descriptors
	vkDestroyDescriptorPool(r->device, r->descriptor_pool, NULL);
	free(r->descriptor_sets);
	//Command buffers
	vkFreeCommandBuffers(r->device, r->command_pool, frame_count, r->command_buffers);
	free(r->command_buffers);
	//Framebuffers
	for (unsigned i = 0; i < frame_count; ++i)
		vkDestroyFramebuffer(r->device, r->framebuffers[i], NULL);
	free(r->framebuffers);
	//Images & image views
	for (unsigned i = 0; i < 3 * frame_count; ++i) {
		vkDestroyImageView(r->device, r->image_views[i], NULL);
		vkDestroyImage(r->device, r->images[i], NULL);
	}
	free(r->image_views);
	free(r->images);
	free_allocation(r->device, r->image_alloc);
}

static void create_frame_data(
	struct Renderer* const r,
	const VkDeviceSize staging_size,
	const VkDeviceSize uniform_size,
	const VkDeviceSize storage_size) {
	//Set sizes
	r->staging_size = staging_size;
	r->uniform_size = uniform_size;
	r->storage_size = storage_size;
	//Create staging buffer
	const VkBufferCreateInfo staging_buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		r->frame_count * staging_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	create_buffers(
		r->physical_device,
		r->device,
		1, &staging_buffer_info,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		&r->staging_buffer,
		&r->staging_alloc
	);
	//Create frame buffers
	const VkBufferCreateInfo buffer_infos[] = {
		//Uniform buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			uniform_size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Storage buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			storage_size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
				| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
	};
	const unsigned buffer_count = 2 * r->frame_count;
	VkBufferCreateInfo* const all_buffer_infos = malloc(buffer_count * sizeof(VkBufferCreateInfo));
	for (unsigned i = 0; i < r->frame_count; ++i)
		memcpy(all_buffer_infos + 2 * i, buffer_infos, sizeof(buffer_infos));
	r->frame_buffers = malloc(buffer_count * sizeof(VkBuffer));
	create_buffers(
		r->physical_device,
		r->device,
		buffer_count, all_buffer_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		r->frame_buffers,
		&r->frame_buffer_alloc
	);
	free(all_buffer_infos);
}

static void destroy_frame_data(struct Renderer* const r) {
	//Staging buffer
	vkDestroyBuffer(r->device, r->staging_buffer, NULL);
	free_allocation(r->device, r->staging_alloc);
	//Frame buffers
	for (unsigned i = 0; i < 2 * r->frame_count; ++i)
		vkDestroyBuffer(r->device, r->frame_buffers[i], NULL);
	free(r->frame_buffers);
	free_allocation(r->device, r->frame_buffer_alloc);
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
		4, //surface_capabilities.minImageCount + 2, //Image count
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
		//VK_PRESENT_MODE_FIFO_KHR,
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
	return false;
}

static void destroy_swapchain(struct Renderer* const r, bool preserve) {
	if (!preserve) vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
	free(r->swapchain_images);
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
	const VkPhysicalDevice physical_device,
	const VkDevice device,
	const VkCommandBuffer command_buffer,
	const VkQueue queue,
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
		vkMapMemory(device, staging_alloc.memory, offsets[i], sizes[i], 0, &buffer_data);
		memcpy(buffer_data, data[i], sizes[i]);
		vkUnmapMemory(device, staging_alloc.memory);
	}
	//Record commands for transfer
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);
	for (unsigned i = 0; i < count; ++i) {
		const VkBufferCopy region = {offsets[i], 0, sizes[i]};
		vkCmdCopyBuffer(command_buffer, staging_buffer, dst_buffers[i], 1, &region);
	}
	vkEndCommandBuffer(command_buffer);
	//Submit to queue
	const VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		0, NULL,
		0,
		1, &command_buffer,
		0, NULL
	};
	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue); //TODO: Better synchronization
	free(offsets);
	vkDestroyBuffer(device, staging_buffer, NULL);
	free_allocation(device, staging_alloc);
}

static void copy_buffer_to_images(
	//Vulkan objects
	const VkCommandBuffer command_buffer,
	const VkQueue queue,
	//Parameters
	const VkBuffer src_buffer,
	const unsigned count,
	VkImage* const images,
	VkBufferImageCopy* const regions,
	const VkImageLayout layout) {
	//Record command buffer
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);
	//Image layout transition
	const VkImageSubresourceRange color_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1,
		0, 1
	};
	VkImageMemoryBarrier2* const before_image_barriers
		= malloc(count * sizeof(VkImageMemoryBarrier2));
	for (unsigned i = 0; i < count; ++i)
		before_image_barriers[i] = (VkImageMemoryBarrier2) {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			images[i],
			color_subresource_range
		};
	const VkDependencyInfo before_copy_dependency = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		0, NULL,
		count, before_image_barriers
	};
	vkCmdPipelineBarrier2(command_buffer, &before_copy_dependency);
	//Image copy command
	for (unsigned i = 0; i < count; ++i)
		vkCmdCopyBufferToImage(
			command_buffer,
			src_buffer,
			images[i],
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, regions + i
		);
	//Image layout transition
	VkImageMemoryBarrier2* const after_image_barriers
		= malloc(count * sizeof(VkImageMemoryBarrier2));
	for (unsigned i = 0; i < count; ++i)
		after_image_barriers[i] = (VkImageMemoryBarrier2) {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			layout,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			images[i],
			color_subresource_range
		};
	const VkDependencyInfo after_copy_dependency = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		0, NULL,
		count, after_image_barriers
	};
	vkCmdPipelineBarrier2(command_buffer, &after_copy_dependency);
	vkEndCommandBuffer(command_buffer);
	//Submit to queue
	const VkCommandBufferSubmitInfo command_buffer_submit = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, NULL,
		command_buffer,
		0
	};
	const VkSubmitInfo2 submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2, NULL, 0,
		0, NULL,
		1, &command_buffer_submit,
		0, NULL
	};
	vkQueueSubmit2(queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue); //TODO: Better synchronization
	//Cleanup
	free(before_image_barriers);
	free(after_image_barriers);
}

static void write_textures_to_images(
	//Vulkan objects
	const VkPhysicalDevice physical_device,
	const VkDevice device,
	const VkCommandBuffer command_buffer,
	const VkQueue queue,
	//Parameters
	unsigned count,
	SDL_Surface** const textures,
	VkImage* const images,
	VkImageView* const image_views,
	struct Allocation* const image_alloc
) {
	//Textures
	VkImageCreateInfo* const image_create_infos = malloc(count * sizeof(VkImageCreateInfo));
	VkBufferImageCopy* const regions = malloc(count * sizeof(VkBufferImageCopy));
	unsigned buffer_size = 0;
	const VkFormat format = VK_FORMAT_B8G8R8A8_SRGB;
	for (unsigned i = 0; i < count; ++i) {
		const SDL_Surface* texture = textures[i];
		const VkExtent3D extent = {texture->w, texture->h, 1};
		//Image create info
		image_create_infos[i] = (VkImageCreateInfo) {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			format,
			extent,
			1,
			1,
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED
		};
		//Region
		regions[i] = (VkBufferImageCopy) {
			buffer_size,
			0,
			0,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
			{0, 0, 0},
			{texture->w, texture->h, 1}
		};
		buffer_size += texture->pitch * texture->h;
	}
	//Create staging buffer
	VkBuffer buffer;
	struct Allocation buffer_alloc;
	const VkBufferCreateInfo buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		buffer_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	create_buffers(
		physical_device,
		device,
		1,
		&buffer_info,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&buffer,
		&buffer_alloc
	);
	//Write to staging buffer
	void* buffer_data;
	vkMapMemory(device, buffer_alloc.memory, 0, buffer_size, 0, &buffer_data);
	for (unsigned i = 0; i < count; ++i) {
		const SDL_Surface* texture = textures[i];
		const VkBufferImageCopy region = regions[i];
		const unsigned texture_size = texture->pitch * texture->h;
		memcpy(buffer_data + region.bufferOffset, texture->pixels, texture_size);
	}
	vkUnmapMemory(device, buffer_alloc.memory);
	//Create images
	create_images(
		physical_device,
		device,
		count, image_create_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		images,
		image_alloc
	);
	//Write to images
	copy_buffer_to_images(
		command_buffer,
		queue,
		buffer,
		count,
		images,
		regions,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	//Image views
	const VkComponentMapping component_mapping = {
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY,
		VK_COMPONENT_SWIZZLE_IDENTITY
	};
	for (unsigned i = 0; i < count; ++i) {
		const VkImageViewCreateInfo image_view_info = {
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
			images[i],
			VK_IMAGE_VIEW_TYPE_2D,
			format,
			component_mapping,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		};
		vkCreateImageView(device, &image_view_info, NULL, image_views + i);
	}
	//Cleanup
	free(image_create_infos);
	free(regions);
	vkDestroyBuffer(device, buffer, NULL);
	free_allocation(device, buffer_alloc);
}

static void record_draw_commands(
	struct Renderer* const r,
	unsigned frame,
	unsigned swapchain_image_index,
	unsigned draw_count) {
	const VkCommandBuffer command_buffer = r->command_buffers[frame];
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);
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
		frame * r->staging_size, r->staging_size
	};
	const VkDependencyInfo staging_buffer_dependency = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		1, &staging_buffer_barrier,
		0, NULL
	};
	vkCmdPipelineBarrier2(r->command_buffers[frame], &staging_buffer_dependency);
	//Copy staging buffer to framebuffers
	//Uniform
	const VkBufferCopy uniform_region = {
		frame * r->staging_size,
		0,
		r->uniform_size
	};
	vkCmdCopyBuffer(command_buffer, r->staging_buffer, r->frame_buffers[2 * frame], 1, &uniform_region);
	//Storage
	const VkBufferCopy storage_region = {
		frame * r->staging_size + r->uniform_size,
		0,
		r->storage_size};
	vkCmdCopyBuffer(command_buffer, r->staging_buffer, r->frame_buffers[2 * frame + 1], 1, &storage_region);
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
			r->frame_buffers[2 * frame],
			0, VK_WHOLE_SIZE
		},
		//Storage buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_COPY_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
			VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->frame_buffers[2 * frame + 1],
			0, VK_WHOLE_SIZE
		}
	};
	const VkDependencyInfo shader_buffer_dependency= {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		2, shader_buffer_barriers,
		0, NULL
	};
	vkCmdPipelineBarrier2(command_buffer, &shader_buffer_dependency);

	//Render pass
	const VkClearValue clear_values[3] = {
		{0, 0, 0, 1}, //Color
		{0, 0, 0, 1}, //Resolve
		{1, 0} //Depth
	};
	const VkRenderPassBeginInfo render_pass_begin_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
		r->render_pass,
		r->framebuffers[frame],
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
		1, r->descriptor_sets + frame,
		0, NULL
	);
	//Vertex buffers
	const VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(
		command_buffer,
		0,
		1, r->static_buffers, offsets
	);
	vkCmdBindIndexBuffer(
		command_buffer,
		r->static_buffers[1],
		0,
		VK_INDEX_TYPE_UINT32
	);
	//Drawing
	vkCmdDrawIndexedIndirect(
		command_buffer,
		r->static_buffers[3],
		0,
		draw_count,
		sizeof(VkDrawIndexedIndirectCommand)
	);
	vkCmdEndRenderPass(command_buffer);

	//Blitting
	const VkImageSubresourceRange color_subresource_range = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 1,
		0, 1
	};
	const VkImageMemoryBarrier2 blit_image_barriers[] = {
		//Render target
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_2_BLIT_BIT,
			VK_ACCESS_2_TRANSFER_READ_BIT,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			r->graphics_queue_family,
			r->graphics_queue_family,
			r->images[3 * frame + 1],
			color_subresource_range
		},
		//Swapchain image
		{
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, NULL,
			VK_PIPELINE_STAGE_2_NONE,
			VK_ACCESS_2_NONE,
			VK_PIPELINE_STAGE_2_BLIT_BIT,
			VK_ACCESS_2_TRANSFER_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			r->present_queue_family,
			r->graphics_queue_family,
			r->swapchain_images[swapchain_image_index],
			color_subresource_range
		}
	};
	const VkDependencyInfo blit_dependency = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		0, NULL,
		2, blit_image_barriers
	};
	vkCmdPipelineBarrier2(command_buffer, &blit_dependency);
	//Blit operation
	const VkImageSubresourceLayers blit_subresource = {
		VK_IMAGE_ASPECT_COLOR_BIT,
		0, 0, 1
	};
	const VkImageBlit blit_region = {
		blit_subresource,
		{{0, 0, 0}, {r->resolution.width, r->resolution.height, 1}},
		blit_subresource,
		{{0, 0, 0}, {r->surface_extent.width, r->surface_extent.height, 1}}
	};
	vkCmdBlitImage(
		command_buffer,
		r->images[3 * frame + 1],
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		r->swapchain_images[swapchain_image_index],
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &blit_region,
		VK_FILTER_NEAREST
	);
	//Transition swapchain image
	const VkImageMemoryBarrier2 present_barrier = {
		VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, NULL,
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		VK_ACCESS_2_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_NONE,
		VK_ACCESS_2_NONE,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		r->graphics_queue_family,
		r->present_queue_family,
		r->swapchain_images[swapchain_image_index],
		color_subresource_range
	};
	const VkDependencyInfo present_dependency = {
		VK_STRUCTURE_TYPE_DEPENDENCY_INFO, NULL, 0,
		0, NULL,
		0, NULL,
		1, &present_barrier
	};
	vkCmdPipelineBarrier2(command_buffer, &present_dependency);
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
	const char* layer_names[] = {"VK_LAYER_KHRONOS_validation"}; //FIXME: Validation layer
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

	//Transfer command buffer
	const VkCommandBufferAllocateInfo command_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r.command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};
	vkAllocateCommandBuffers(r.device, &command_buffer_alloc_info, &r.transfer_command_buffer);
	
	//Sampler
	const VkSamplerCreateInfo sampler_info = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, NULL, 0,
		VK_FILTER_NEAREST,
		VK_FILTER_NEAREST,
		VK_SAMPLER_MIPMAP_MODE_NEAREST,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		VK_SAMPLER_ADDRESS_MODE_REPEAT,
		0,
		VK_FALSE,
		0,
		VK_FALSE,
		VK_COMPARE_OP_NEVER,
		0,
		VK_LOD_CLAMP_NONE,
		VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		VK_FALSE
	};
	vkCreateSampler(r.device, &sampler_info, NULL, &r.sampler);

	//Descriptor set layout
	const VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[] = {
		{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL}, //Camera
		{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL}, //Nodes
		{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}, //Materials
		{3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &r.sampler}, //Sampler
		{4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_TEXTURE_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT, NULL}, //Textures
	};
	const VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0,
		5, descriptor_set_layout_bindings
	};
	vkCreateDescriptorSetLayout(r.device, &descriptor_set_layout_info, NULL, &r.descriptor_set_layout);

	//Pipeline layout
	const VkPipelineLayoutCreateInfo layout_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
		1, &r.descriptor_set_layout,
		0, NULL
	};
	vkCreatePipelineLayout(r.device, &layout_info, NULL, &r.pipeline_layout);

	//Partytime
	create_resolution(&r, 1048, 1048);
	create_frames(&r, 2);
	create_swapchain(&r, false);

	*result = r;
	return false;
}

void destroy_renderer(struct Renderer r) {
	vkDeviceWaitIdle(r.device);
	save_pipeline_cache(&r);
	renderer_destroy_scene(&r);
	destroy_frames(&r);
	destroy_swapchain(&r, false);
	destroy_resolution(&r);
	vkDestroyPipelineCache(r.device, r.pipeline_cache, NULL);
	vkDestroyPipelineLayout(r.device, r.pipeline_layout, NULL);
	vkDestroySampler(r.device, r.sampler, NULL);
	vkDestroyDescriptorSetLayout(r.device, r.descriptor_set_layout, NULL);
	vkDestroyCommandPool(r.device, r.command_pool, NULL);
	vkDestroyDevice(r.device, NULL);
	vkDestroySurfaceKHR(r.instance, r.surface, NULL);
	vkDestroyInstance(r.instance, NULL);
}

void renderer_draw(struct Renderer* const r) {
	const unsigned current_frame = (r->current_frame + 1) % r->frame_count;
	r->current_frame = current_frame;
	vkWaitForFences(r->device, 1, r->fences + current_frame, VK_FALSE, UINT64_MAX);
	vkResetFences(r->device, 1, r->fences + current_frame);
	//Acquire swapchain image
	unsigned image_index;
	VkResult swapchain_status = VK_ERROR_UNKNOWN;
	while (swapchain_status != VK_SUCCESS && swapchain_status != VK_SUBOPTIMAL_KHR) {
		swapchain_status = vkAcquireNextImageKHR(
			r->device,
			r->swapchain,
			0,
			r->semaphores[2 * current_frame],
			NULL,
			&image_index
		);
		if (swapchain_status == VK_ERROR_OUT_OF_DATE_KHR) {
			//Recreate swapchain
			vkQueueWaitIdle(r->present_queue);
			destroy_swapchain(r, true);
			create_swapchain(r, true);
		}
	}
	//Copy local data to staging buffer
	void* staging_data;
	const VkMappedMemoryRange memory_range = {
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, NULL,
		r->staging_alloc.memory,
		current_frame * r->staging_size,
		r->staging_size
	};
	vkMapMemory(
		r->device,
		memory_range.memory,
		memory_range.offset,
		memory_range.size,
		0,
		&staging_data
	);
	memcpy(staging_data, r->host_data, r->staging_size);
	vkFlushMappedMemoryRanges(r->device, 1, &memory_range);
	vkUnmapMemory(r->device, r->staging_alloc.memory);
	//Record command buffer
	record_draw_commands(r, current_frame, image_index, r->draw_count);
	//Submit command buffer to queue
	const VkSemaphoreSubmitInfo wait_semaphore = {
		VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, NULL,
		r->semaphores[2 * current_frame],
		0,
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		0
	};
	const VkCommandBufferSubmitInfo command_buffer = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, NULL,
		r->command_buffers[current_frame],
		0
	};
	const VkSemaphoreSubmitInfo signal_semaphore = {
		VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, NULL,
		r->semaphores[2 * current_frame + 1],
		0,
		VK_PIPELINE_STAGE_2_BLIT_BIT,
		0
	};
	const VkSubmitInfo2 submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO_2, NULL, 0,
		1, &wait_semaphore,
		1, &command_buffer,
		1, &signal_semaphore 
	};
	vkQueueSubmit2(r->graphics_queue, 1, &submit_info, r->fences[current_frame]);
	//Presentation
	const VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		1, &r->semaphores[2 * current_frame + 1],
		1, &r->swapchain,
		&image_index,
		NULL
	};
	vkQueuePresentKHR(r->present_queue, &present_info);
	vkQueueWaitIdle(r->graphics_queue);
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
	r->draw_count = scene.node_count;

	//Create static buffers
	const VkBufferCreateInfo buffer_infos[] = {
		//Vertices
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			vertex_count * sizeof(struct Vertex),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Indices
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			index_count * sizeof(unsigned),
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Meshes
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.mesh_count * sizeof(struct LocalMesh),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Draw commands
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.node_count * sizeof(VkDrawIndexedIndirectCommand),
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
		//Materials
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			scene.material_count * sizeof(struct Material),
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
	};
	if (create_buffers(
		r->physical_device,
		r->device,
		5, buffer_infos,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		r->static_buffers,
		&r->static_alloc
	)) fprintf(stderr, "Error creating static scene buffers!\n");
	//Write to static buffers
	const void* data[] = {vertices, indices, local_meshes, draw_commands, scene.materials};
	const size_t sizes[] = {
		buffer_infos[0].size, 
		buffer_infos[1].size, 
		buffer_infos[2].size, 
		buffer_infos[3].size,
		buffer_infos[4].size
	};
	staged_buffer_write(
		r->physical_device,
		r->device,
		r->transfer_command_buffer,
		r->graphics_queue,
		5, r->static_buffers, data, sizes
	);
	free(local_meshes);
	free(vertices);
	free(indices);
	free(draw_commands);
	free(local_nodes);

	//Textures
	r->texture_count = scene.texture_count;
	r->textures = malloc(scene.texture_count * sizeof(VkImage));
	r->texture_views = malloc(scene.texture_count * sizeof(VkImageView));
	write_textures_to_images(
		r->physical_device,
		r->device,
		r->transfer_command_buffer,
		r->graphics_queue,
		scene.texture_count,
		scene.textures,
		r->textures,
		r->texture_views,
		&r->texture_alloc
	);

	//Create dynamic buffers
	VkDeviceSize uniform_size = sizeof(struct LocalCamera),
		storage_size = scene.node_count * sizeof(struct LocalNode),
		staging_size = uniform_size + storage_size;
	r->host_data = malloc(staging_size);
	create_frame_data(r, staging_size, uniform_size, storage_size);

	//Texture descriptor information
	VkDescriptorImageInfo* const texture_descriptor_infos
		= malloc(MAX_TEXTURE_COUNT * sizeof(VkDescriptorImageInfo));
	for (unsigned i = 0; i < scene.texture_count; ++i)
		texture_descriptor_infos[i] = (VkDescriptorImageInfo) {
			VK_NULL_HANDLE,
			r->texture_views[i],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
	for (unsigned i = scene.texture_count; i < MAX_TEXTURE_COUNT; ++i) {
		texture_descriptor_infos[i] = (VkDescriptorImageInfo) {
			VK_NULL_HANDLE,
			r->texture_views[0],
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
	}
	//Update descriptors
	const unsigned descriptor_count = 4 * r->frame_count;
	VkWriteDescriptorSet* const descriptor_writes
		= malloc(descriptor_count * sizeof(VkWriteDescriptorSet));
	for (unsigned i = 0; i < r->frame_count; ++i) {
		//Uniform buffer
		const VkDescriptorBufferInfo uniform_buffer_info = {
			r->frame_buffers[2 * i],
			0,
			VK_WHOLE_SIZE
		};
		descriptor_writes[4 * i] = (VkWriteDescriptorSet) {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			r->descriptor_sets[i],
			0, //Binding
			0,
			1,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			NULL,
			&uniform_buffer_info,
			NULL
		};
		//Node buffer
		const VkDescriptorBufferInfo node_buffer_info = {
			r->frame_buffers[2 * i + 1],
			0,
			VK_WHOLE_SIZE
		};
		descriptor_writes[4 * i + 1] = (VkWriteDescriptorSet) {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			r->descriptor_sets[i],
			1, //Binding
			0,
			1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL,
			&node_buffer_info,
			NULL
		};
		//Material buffer
		const VkDescriptorBufferInfo material_buffer_info = {
			r->static_buffers[4],
			0,
			VK_WHOLE_SIZE
		};
		descriptor_writes[4 * i + 2] = (VkWriteDescriptorSet) {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			r->descriptor_sets[i],
			2, //Binding
			0,
			1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			NULL,
			&material_buffer_info,
			NULL
		};
		//Textures
		descriptor_writes[4 * i + 3] = (VkWriteDescriptorSet) {
			VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL,
			r->descriptor_sets[i],
			4, //Binding
			0,
			MAX_TEXTURE_COUNT,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			texture_descriptor_infos,
			NULL,
			NULL
		};
	}
	vkUpdateDescriptorSets(r->device, descriptor_count, descriptor_writes, 0, NULL);
	free(descriptor_writes);
	free(texture_descriptor_infos);
}

void renderer_destroy_scene(struct Renderer* const r) {
	vkQueueWaitIdle(r->graphics_queue);
	destroy_frame_data(r);
	free(r->host_data);
	//Static buffers
	for (unsigned i = 0; i < 5; ++i)
		vkDestroyBuffer(r->device, r->static_buffers[i], NULL);
	free_allocation(r->device, r->static_alloc);
	//Textures
	for (unsigned i = 0; i < r->texture_count; ++i) {
		vkDestroyImageView(r->device, r->texture_views[i], NULL);
		vkDestroyImage(r->device, r->textures[i], NULL);
	}
	free(r->textures);
	free(r->texture_views);
	free_allocation(r->device, r->texture_alloc);
}

void renderer_update_camera(struct Renderer* const r, const struct Camera camera) {
	struct LocalCamera local_camera;
	camera_view(camera, local_camera.view);
	camera_projection(camera, local_camera.projection);
	memcpy(r->host_data, &local_camera, sizeof(struct LocalCamera));
}

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
	//Write to buffer
	memcpy(r->host_data + sizeof(struct LocalCamera), local_nodes, local_nodes_size);
	free(local_nodes);
}
