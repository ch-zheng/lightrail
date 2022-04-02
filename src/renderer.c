#include "renderer.h"
#include "alloc.h"
#include "camera.h"
#include "cglm/affine.h"
#include "scene.h"
#include "vertex_buffer.h"
#include "vulkan/vulkan_core.h"
#include <stdint.h>
#include <stdio.h>
#include <SDL2/SDL_vulkan.h>
#include <cglm/mat4.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define REQUIRED_EXT_COUNT 1

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
	const VkShaderModule vertex_shader = create_shader_module(r, "/Users/hang/code/lightrail/build/shaders/basic.vert.spv"),
		fragment_shader = create_shader_module(r, "/Users/hang/code/lightrail/build/shaders/basic.frag.spv");
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
	const VkVertexInputAttributeDescription attribute_descriptions[5] = {
		//Position
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, pos)},
		//Texture coords
		{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, tex)},
		//Normal
		{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, normal)},
		// Tangent
		{3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, tangent)},
		// Bitangent
		{4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, bitangent)},
	};
	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, NULL, 0,
		1, &binding_description,
		3, attribute_descriptions
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
	const VkPipelineRasterizationStateCreateInfo rasterization = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0,
		false,
		false,
		VK_POLYGON_MODE_FILL,
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
		VK_TRUE,
		.5f,
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


	const VkPushConstantRange camera_constant = {
		VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4)
	};
	const VkPushConstantRange texture_index_constant = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = sizeof(mat4),
		.size = sizeof(struct Material),
	};

	VkPushConstantRange push_contants[] = {camera_constant, texture_index_constant};

	// create descriptor layout
	// create_descriptor_layouts(r);
	const VkPipelineLayoutCreateInfo layout_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
		1, &r->descriptor_set_layout, //TODO: Descriptor layout
		2, push_contants
	};
	// const VkPipelineLayoutCreateInfo layout_info = {
	// 	VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
	// 	0, NULL, //TODO: Descriptor layout
	// 	1, &camera_constant
	// };
	vkCreatePipelineLayout(r->device, &layout_info, NULL, &r->pipeline_layout);

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
		r->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &r->pipeline
	);

	//Cleanup
	vkDestroyShaderModule(r->device, vertex_shader, NULL);
	vkDestroyShaderModule(r->device, fragment_shader, NULL);
	return result;
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
		VK_PRESENT_MODE_IMMEDIATE_KHR, //TODO: Present mode config
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

static void create_descriptors(struct Renderer* const r) {	
	VkDescriptorSetLayoutBinding ubo_layout_binding = {
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = NULL,
	};

	VkDescriptorSetLayoutBinding texture_sampler_binding = {
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = NULL,
	};
	
	VkDescriptorSetLayoutBinding texture_layout_binding = {
		.binding = 2,
		.descriptorCount = TEXTURE_ARRAY_SIZE,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.pImmutableSamplers = NULL,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	};


	VkDescriptorSetLayoutBinding layout_bindings[] = { ubo_layout_binding, texture_sampler_binding, texture_layout_binding };

	VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 3,
		.pBindings = layout_bindings,
	};

	vkCreateDescriptorSetLayout(r->device, &descriptor_layout_info, NULL, &r->descriptor_set_layout);

	// create the buffers
	// TODO: find a use for this uniform
	VkBufferCreateInfo uniform_buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(vec3),
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VkBufferCreateInfo uniform_buffer_infos[MAX_FRAMES_IN_FLIGHT];

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		uniform_buffer_infos[i] = uniform_buffer_info;
		VkResult res = create_buffers(&r->physical_device, &r->device, 1, &uniform_buffer_info, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&r->uniform_buffers[i], &r->uniform_mems[i]);
	}
	// TODO: figure out why this doesn't work
	// VkResult res = create_buffers(&r->physical_device, &r->device, MAX_FRAMES_IN_FLIGHT, uniform_buffer_infos, 
	// 	VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	// 	r->uniform_buffers, r->uniform_mems);
	
	VkDescriptorPoolSize pool_sizes[] = { { 
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	},
	{
		.type = VK_DESCRIPTOR_TYPE_SAMPLER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT,
	},
	{
		.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = TEXTURE_ARRAY_SIZE * MAX_FRAMES_IN_FLIGHT,
	},
	};

	VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 3,
		.pPoolSizes = pool_sizes,
		.maxSets = MAX_FRAMES_IN_FLIGHT,
	};

	vkCreateDescriptorPool(r->device, &pool_info, NULL, &r->descriptor_pool);
	
	VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		layouts[i] = r->descriptor_set_layout;
	}

	VkDescriptorSetAllocateInfo descriptor_alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = r->descriptor_pool,
		.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts,
	};

	vkAllocateDescriptorSets(r->device, &descriptor_alloc_info, r->descriptor_sets);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo buffer_info = {
			.buffer = r->uniform_buffers[i],
			.offset = 0,
			.range = sizeof(vec3),
		};

		VkDescriptorImageInfo sampler_info = {
			.sampler = r->texture_sampler,
		};

		VkDescriptorImageInfo image_infos[TEXTURE_ARRAY_SIZE];
		for (int j = 0; j < TEXTURE_ARRAY_SIZE; ++j) {
			VkImageView view = r->scene->textures[j % r->scene->texture_count].image_view;
			VkDescriptorImageInfo image_info = {
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.imageView = view,
				.sampler = r->texture_sampler,
			};
			image_infos[j] = image_info;
		}

		VkWriteDescriptorSet descriptor_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_sets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.pBufferInfo = &buffer_info,
		};

		VkWriteDescriptorSet sampler_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_sets[i],
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = 1,
			.pImageInfo = &sampler_info,
		};
		
		VkWriteDescriptorSet image_write = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = r->descriptor_sets[i],
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			.descriptorCount = TEXTURE_ARRAY_SIZE,
			.pImageInfo = image_infos,
		};
		VkWriteDescriptorSet sets[] = {descriptor_write, sampler_write, image_write};
		vkUpdateDescriptorSets(r->device, 3, sets, 0, NULL);
	}
}

bool create_renderer(SDL_Window* window, struct Renderer* const result, struct Scene* scene) {
	struct Renderer r;
	r.window = window;

	//Vulkan instance
	const VkApplicationInfo app_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
		"Lightrail", 1,
		"Lightrail", 1,
		VK_API_VERSION_1_2
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
	const char* required_extensions[REQUIRED_EXT_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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
	const VkDeviceCreateInfo device_info = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, NULL, 0,
		queue_info_count, queue_infos,
		0, NULL, //Layers
		REQUIRED_EXT_COUNT, required_extensions, //Extensions
		NULL //Features
	};
	vkCreateDevice(r.physical_device, &device_info, NULL, &r.device);
	//Queue handles
	vkGetDeviceQueue(r.device, r.graphics_queue_family, 0, &r.graphics_queue);
	vkGetDeviceQueue(r.device, r.present_queue_family, 0, &r.present_queue);
	
	//Command pool
	const VkCommandPoolCreateInfo pool_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL,
		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		r.graphics_queue_family
	};
	vkCreateCommandPool(r.device, &pool_info, NULL, &r.command_pool);

	//Command buffer allocation
	const VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r.command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		MAX_FRAMES_IN_FLIGHT,
	};
	vkAllocateCommandBuffers(r.device, &cmd_buffer_alloc_info, r.command_buffers);

	//Semaphores
	const VkSemaphoreCreateInfo semaphore_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0
	};

	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT,
	};

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkCreateSemaphore(r.device, &semaphore_info, NULL, &r.image_available_semaphores[i]);
		vkCreateSemaphore(r.device, &semaphore_info, NULL, &r.render_finished_semaphores[i]);
		vkCreateFence(r.device, &fence_info, NULL, &r.in_flight_fences[i]);
	}

	//TODO: Multisampling
	//TODO: Pipeline cache

	create_swapchain(&r, false);

	// load all of the textures in the scene provided. 
	// TODO some textures are loaded more than once
	// for (int i = 0; i < scene->texture_count; ++i) {
	// 	struct Texture t = scene->textures[i];
	// 	if (!t.loaded) {
	// 		load_texture(t.filepath, 0, 0, r.device, r.physical_device, r.command_buffers[0],r.graphics_queue, &scene->textures[i]);
	// 	}
	// }
	VkPhysicalDeviceProperties properties = {};	
	vkGetPhysicalDeviceProperties(r.physical_device, &properties);

	VkSamplerCreateInfo texture_sampler_info = {
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.anisotropyEnable = VK_TRUE,
		.maxAnisotropy = properties.limits.maxSamplerAnisotropy,
		.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
		.unnormalizedCoordinates = VK_FALSE,
		.compareEnable = VK_FALSE,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.mipLodBias = 0.0f,
		.minLod = 0.0f,
		.maxLod = 0.0f,
	};
	r.scene = scene;
	r.camera = r.scene->cam;
	vkCreateSampler(r.device, &texture_sampler_info, NULL, &r.texture_sampler);
	load_textures(r.device, r.physical_device, r.command_buffers[0], r.graphics_queue, r.scene);
	create_descriptors(&r);
	renderer_create_resolution(&r, 800, 600); //TODO: Resolution setting

	//Camera
	// r.camera = (struct Camera) {
	// 	{0, 5, 0},
	// 	{0, -1, 0},
	// 	{0, 0, 1},
	// 	80, 1, 1, 64,
	// 	PERSPECTIVE
	// };

	struct VertexIndexBuffer vertex_index_buffer = {};
	create_vertex_index_buffer(r.device, r.physical_device, &vertex_index_buffer, DEFAULT_VERTEX_BUFFER_SIZE, DEFAULT_INDEX_BUFFER_SIZE);
	r.vertex_index_buffer = vertex_index_buffer;

	for (int i = 0; i < scene->mesh_count; ++i) {
		struct Mesh mesh = scene->meshes[i];

		struct Vertex* vertices = (struct Vertex*) malloc(sizeof(struct Vertex) * mesh.vertex_count);
		uint32_t* indices = (uint32_t*) malloc(sizeof(uint32_t) * mesh.index_count);
		memcpy(vertices, mesh.vertices, sizeof(struct Vertex) * mesh.vertex_count);
		memcpy(indices, mesh.indices, sizeof(uint32_t) * mesh.index_count);
		write_vertex_index_buffer(
			r.device,
			r.physical_device,
			&r.command_buffers[0],
			&r.graphics_queue,
			mesh.vertex_count,
			vertices,
			mesh.index_count,
			indices,
			&r.vertex_index_buffer,
			&scene->meshes[i].vertex_block,
			&scene->meshes[i].index_block	
		);
	}
	r.mesh_count = scene->mesh_count;
	
	*result = r;
	return false;
}

void destroy_renderer(struct Renderer* const r) {

	//FIXME: Testing data
	// for (unsigned i = 0; i < 2; ++i)
	// 	vkDestroyBuffer(r->device, r->vertex_buffers[i], NULL);
	// free_allocation(r->device, r->vertex_alloc);
	vkDeviceWaitIdle(r->device);
	destroy_scene(r->device, *r->scene);
	renderer_destroy_resolution(r);
	vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
	vkDestroyDescriptorSetLayout(r->device, r->descriptor_set_layout, NULL);
	vkDestroyDescriptorPool(r->device, r->descriptor_pool, NULL);
	free(r->swapchain_images);
	vkDestroySampler(r->device, r->texture_sampler, NULL);
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(r->device, r->image_available_semaphores[i], NULL);
		vkDestroySemaphore(r->device, r->render_finished_semaphores[i], NULL);
		vkDestroyFence(r->device, r->in_flight_fences[i], NULL);
		vkDestroyBuffer(r->device, r->uniform_buffers[i], NULL);
		free_allocation(r->device, r->uniform_mems[i]);
	}
	vkDestroyCommandPool(r->device, r->command_pool, NULL);
	cleanup_vertex_index_buffer(r->device, r->physical_device, &r->vertex_index_buffer);
	vkDestroyDevice(r->device, NULL);
	vkDestroySurfaceKHR(r->instance, r->surface, NULL);
	vkDestroyInstance(r->instance, NULL);
}

VkResult renderer_create_resolution(
	struct Renderer* const r,
	unsigned width,
	unsigned height) {
	r->resolution = (VkExtent2D) {width, height};
	VkExtent3D extent_3d = {width, height, 1};

	//Render pass attachments
	const VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB,
		depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
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
	vkDestroyPipelineLayout(r->device, r->pipeline_layout, NULL);
	vkDestroyPipeline(r->device, r->pipeline, NULL);
	vkDestroyFramebuffer(r->device, r->framebuffer, NULL);
	vkDestroyRenderPass(r->device, r->render_pass, NULL);
	for (unsigned i = 0; i < 3; ++i) {
		vkDestroyImageView(r->device, r->image_views[i], NULL);
		vkDestroyImage(r->device, r->images[i], NULL);
	}
	free_allocation(r->device, r->image_mem);
}

static void update_uniform_buffers(struct Renderer* const r) {
	time_t t = time(NULL);
	float value = (float) (t % 3);
	vec3 color = { 0.4999*value, 0.4f, 0.4f };

	void* data;
	vkMapMemory(r->device, r->uniform_mems[r->current_frame].memory, 0, sizeof(color), 0, &data);
	memcpy(data, color, sizeof(vec3));
	vkUnmapMemory(r->device, r->uniform_mems[r->current_frame].memory);
}

void renderer_draw(struct Renderer* const r) {
	// vec3 look = {12.0f, 0.0f, 0.0f };
	// camera_look(&r->camera, look);
	r->current_frame = (r->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

	vkWaitForFences(r->device, 1, &r->in_flight_fences[r->current_frame], VK_TRUE, UINT64_MAX);
	vkResetFences(r->device, 1, &r->in_flight_fences[r->current_frame]);
	VkCommandBuffer command_buffer = r->command_buffers[r->current_frame];
	VkSemaphore image_available_semaphore = r->image_available_semaphores[r->current_frame];
	VkSemaphore render_finished_semaphore = r->render_finished_semaphores[r->current_frame];

	//Acquire swapchain image
	unsigned image_index;
	VkResult swapchain_status = VK_ERROR_UNKNOWN;
	while (swapchain_status != VK_SUCCESS && swapchain_status != VK_SUBOPTIMAL_KHR) {
		swapchain_status = vkAcquireNextImageKHR(
			r->device, r->swapchain, 0, image_available_semaphore, NULL, &image_index
		);
		if (swapchain_status == VK_ERROR_OUT_OF_DATE_KHR) {
			//Recreate swapchain
			vkQueueWaitIdle(r->present_queue);
			create_swapchain(r, true);
		}
	}

	// update_uniform_buffers(r);

	//Record command buffer
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(command_buffer, &begin_info);
	//Render pass
	const VkClearValue clear_values[3] = {
		{0, 0, 0, 1}, //Color
		{0, 0, 0, 1}, //Resolve
		{1, 0} //Depth
	};
	VkRenderPassBeginInfo render_pass_begin_info = {
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
	//TODO: Bind descriptors
	//Push constants
	//TODO: Camera transformation
	// mat4 camera;
	// camera_transform(r->camera, camera);
	// vkCmdPushConstants(
	// 	command_buffer,
	// 	r->pipeline_layout,
	// 	VK_SHADER_STAGE_VERTEX_BIT,
	// 	0, sizeof(mat4), camera
	// );
	//Vertex buffers
	const VkDeviceSize offsets[1] = {0};
	vkCmdBindVertexBuffers(
		command_buffer,
		0,
		1, &r->vertex_index_buffer.vertex_buffer, offsets
	);
	vkCmdBindIndexBuffer(
		command_buffer,
		r->vertex_index_buffer.index_buffer,
		0,
		VK_INDEX_TYPE_UINT32
	);

	vkCmdBindDescriptorSets(
		command_buffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		r->pipeline_layout,
		0,
		1,
		&r->descriptor_sets[r->current_frame],
		0,
		NULL
	);
	struct MemoryBlock* block = r->vertex_index_buffer.index_blocks;
	struct MemoryBlock* v_block = r->vertex_index_buffer.vertex_blocks;
	int count = 0;

	mat4 ident;
	glm_mat4_identity(ident);
	// glm_scale(ident, (vec3) { 0.018, 0.018, 0.018 });
	// glm_rotate(ident, glm_rad(45), (vec3) { 0, 1, 0});
	// glm_rotate(ident, glm_rad(180), (vec3) { 0, 1, 0});
	// glm_rotate(ident, glm_rad(-90), (vec3) { 1, 0, 0});
	render_scene(r->scene, r->scene->root, ident, r);
// 
	vkCmdEndRenderPass(command_buffer);
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

	//Submit command buffer to queue
	VkPipelineStageFlagBits wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		1, &image_available_semaphore,
		wait_stages,
		1, &command_buffer,
		1, &render_finished_semaphore
	};
	vkQueueSubmit(r->graphics_queue, 1, &submit_info, r->in_flight_fences[r->current_frame]);

	//Presentation
	VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		1, &render_finished_semaphore,
		1, &r->swapchain,
		&image_index,
		NULL,
	};
	vkQueuePresentKHR(r->present_queue, &present_info);
}

void render_scene(struct Scene* scene, struct Node* node, mat4 parent_transform, struct Renderer* const r) {
	mat4 model;
	glm_mat4_mul(parent_transform, node->transform, model);
	// glm_mat4_mul(node->transform, *parent_transform, model);
	mat4 mvp;
	camera_transform(r->camera, model, mvp);

	vkCmdPushConstants(
		r->command_buffers[r->current_frame],
		r->pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(mat4), mvp
	);

	for (int i = 0; i < node->mesh_count; ++i) {
		struct Mesh m = scene->meshes[node->meshes[i]];
		struct MemoryBlock block = m.index_block;
		struct MemoryBlock v_block = m.vertex_block;
		int text_index = m.m.diffuse;
		vkCmdPushConstants(
			r->command_buffers[r->current_frame],
			r->pipeline_layout,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			sizeof(mat4), sizeof(struct Material), &m.m
		);
		vkCmdDrawIndexed(
			r->command_buffers[r->current_frame], 
			block.length_in_bytes / sizeof(uint32_t), 
			1, 
			block.start_byte / sizeof(uint32_t), 
			((int)v_block.start_byte / sizeof(struct Vertex)), 
			0
		);
	}
	for (int i = 0; i < node->child_count; ++i) {
		render_scene(scene, &node->children[i], model, r);
	}	
}