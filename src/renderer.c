#include "renderer.h"
#include <SDL2/SDL_vulkan.h>

#define REQUIRED_EXT_COUNT 1

static VkResult allocate_block(
	struct Renderer* const r,
	uint32_t prop_flags,
	const unsigned req_count,
	const VkMemoryRequirements* const reqs,
	struct AllocBlock* alloc) {
	if (req_count < 1) return VK_ERROR_UNKNOWN;

	//Memory requirements
	VkDeviceSize alloc_size = 0;
	VkDeviceSize* offsets = malloc(req_count * sizeof(VkDeviceSize));
	uint32_t supported_mem_types = 0xFFFFFFFF;
	for (unsigned i = 0; i < req_count; ++i) {
		//Size & alignment
		VkMemoryRequirements req = reqs[i];
		VkDeviceSize rem = alloc_size % req.alignment;
		if (rem) alloc_size += req.alignment - rem;
		offsets[i] = alloc_size;
		alloc_size += req.size;
		//Memory type
		supported_mem_types &= reqs[i].memoryTypeBits;
	}

	//Find valid memory type
	unsigned mem_type;
	bool valid_type = false;
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(r->physical_device, &mem_props);
	for (unsigned i = 0; i < mem_props.memoryTypeCount; ++i) {
		VkMemoryType type = mem_props.memoryTypes[i];
		bool supported = (supported_mem_types >> i) & 1;
		bool has_props = prop_flags & type.propertyFlags;
		if (supported && has_props) {
			mem_type = i;
			valid_type = true;
			break;
		}
	}
	if (!valid_type) return VK_ERROR_UNKNOWN;

	//Allocation
	VkMemoryAllocateInfo alloc_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
		alloc_size,
		mem_type
	};
	VkDeviceMemory* memory = malloc(sizeof(VkDeviceMemory));
	VkResult result = vkAllocateMemory(r->device, &alloc_info, NULL, memory);
	*alloc = (struct AllocBlock) {
		&(r->device),
		memory,
		req_count,
		offsets
	};
	return result;
}

static void free_block(struct AllocBlock alloc) {
	vkFreeMemory(*alloc.device, *alloc.memory, NULL);
	free(alloc.offsets);
}

static VkShaderModule create_shader_module(
	const struct Renderer* const r,
	const char* filename) {
	//Read shader file
	FILE* file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	rewind(file);
	uint32_t* buffer = malloc(size);
	fread(buffer, 1, size, file);
	//Create shader module
	VkShaderModuleCreateInfo shader_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0,
		size
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
	const VkVertexInputAttributeDescription attribute_descriptions[3] = {
		//Position
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, pos)},
		//Texture coords
		{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, tex)},
		//Normal
		{2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, normal)},
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
		VK_SAMPLE_COUNT_1_BIT,
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

	//Layout
	const VkPushConstantRange projection_constant = {
		VK_SHADER_STAGE_VERTEX_BIT, 0, 16 * sizeof(float)
	};
	const VkPipelineLayoutCreateInfo layout_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0,
		0, NULL, //TODO: Descriptor layout
		1, &projection_constant
	};
	VkPipelineLayout pipeline_layout;
	vkCreatePipelineLayout(r->device, &layout_info, NULL, &pipeline_layout);

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
		pipeline_layout,
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
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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
	if (vkCreateSwapchainKHR(
		r->device,
		&swapchain_info,
		NULL,
		&swapchain)
		!= VK_SUCCESS) {
		printf("Error creating swapchain!\n");
		return true;
	}
	if (old) vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
	r->swapchain = swapchain;
	return false;
}

bool create_renderer(SDL_Window* window, struct Renderer* const result) {
	struct Renderer r;
	r.window = window;

	//Vulkan instance
	VkApplicationInfo app_info = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL,
		"Lightrail", 1,
		"Lightrail", 1,
		VK_API_VERSION_1_2
	};
	//Layers
	const char* layer_names[] = {"VK_LAYER_KHRONOS_Validation"};
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

		//Swapchain
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
	VkDeviceCreateInfo device_info = {
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
	VkCommandPoolCreateInfo pool_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, 0,
		r.graphics_queue_family
	};
	if (vkCreateCommandPool(r.device, &pool_info, NULL, &r.command_pool) != VK_SUCCESS) {
		fprintf(stderr, "Failed to create command pool!\n");
		return true;
	}

	//Command buffer allocation
	VkCommandBufferAllocateInfo cmd_buffer_alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL,
		r.command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		1
	};
	vkAllocateCommandBuffers(r.device, &cmd_buffer_alloc_info, &r.command_buffer);

	//Semaphores
	VkSemaphoreCreateInfo semaphore_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, NULL, 0
	};
	for (unsigned i = 0; i < 2; ++i)
		vkCreateSemaphore(r.device, &semaphore_info, NULL, r.semaphores + i);

	//TODO: Multisampling
	//TODO: Pipeline cache

	create_swapchain(&r, false);
	renderer_set_resolution(&r, 2, 2);

	*result = r;
	return false;
}

VkResult renderer_set_resolution(
	struct Renderer* const r,
	unsigned width,
	unsigned height) {
	r->resolution = (VkExtent2D) {width, height};
	VkExtent3D extent_3d = {width, height, 1};

	//Render pass attachments
	VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB,
		depth_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
	VkImageCreateInfo image_infos[3] = {
		//Color image
		{
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0,
			VK_IMAGE_TYPE_2D,
			color_format,
			extent_3d,
			1,
			1,
			VK_SAMPLE_COUNT_1_BIT, //TODO: Multisampling
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
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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
			VK_SAMPLE_COUNT_1_BIT,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		}
	};

	//Create images
	for (unsigned i = 0; i < 3; ++i)
		vkCreateImage(r->device, image_infos + i, NULL, r->images + i);
	//Allocate memory for images
	VkMemoryRequirements mem_reqs[3];
	for (unsigned i = 0; i < 3; ++i)
		vkGetImageMemoryRequirements(r->device, r->images[i], mem_reqs + i);
	struct AllocBlock block; //TODO: Destroy
	allocate_block(r, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 3, mem_reqs, &block);
	//Bind memory to images
	VkBindImageMemoryInfo bind_infos[3];
	for (unsigned i = 0; i < 3; ++i)
		bind_infos[i] = (struct VkBindImageMemoryInfo) {
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, NULL,
			r->images[i],
			*block.memory,
			block.offsets[i]
		};
	vkBindImageMemory2(r->device, 3, bind_infos);

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
	VkImageViewCreateInfo image_view_infos[3] = {
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
	VkAttachmentDescription attachments[3] = {
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
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		}
	};

	//Attachment references
	VkAttachmentReference color_attachment = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference resolve_attachment = {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkAttachmentReference depth_attachment = {2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

	//Subpass
	VkSubpassDescription subpass_description = {
		0,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, NULL,
		1, &color_attachment,
		&resolve_attachment,
		&depth_attachment,
		0, NULL
	};

	//Subpass dependency
	VkSubpassDependency dependency = {
		VK_SUBPASS_EXTERNAL, 0,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
		VK_ACCESS_NONE_KHR,
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		0
	};

	//Create render pass
	VkRenderPassCreateInfo render_pass_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0,
		3, attachments,
		1, &subpass_description,
		1, &dependency
	};
	vkCreateRenderPass(r->device, &render_pass_info, NULL, &r->render_pass);

	//Create framebuffer
	VkFramebufferCreateInfo framebuffer_info = {
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
	vkDestroyFramebuffer(r->device, r->framebuffer, NULL);
	vkDestroyRenderPass(r->device, r->render_pass, NULL);
	for (unsigned i = 0; i < 3; ++i) {
		vkDestroyImageView(r->device, r->image_views[i], NULL);
		vkDestroyImage(r->device, r->images[i], NULL);
	}
}

void renderer_draw(const struct Renderer* const r) {
	//Record command buffer
	VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(r->command_buffer, &begin_info);
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
		r->command_buffer,
		&render_pass_begin_info,
		VK_SUBPASS_CONTENTS_INLINE
	);
	vkCmdBindPipeline(r->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, r->pipeline);
	//TODO: Bind descriptors
	//TODO: Push constants
	vkCmdDraw(r->command_buffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(r->command_buffer);
	vkEndCommandBuffer(r->command_buffer);

	//Acquire swapchain image
	unsigned image_index;
	vkAcquireNextImageKHR(r->device, r->swapchain, UINT64_MAX, r->semaphores[0], NULL, &image_index);

	//Submit command buffer to queue
	VkPipelineStageFlagBits wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		1, &r->semaphores[0],
		wait_stages,
		1, &r->command_buffer,
		1, &r->semaphores[1]
	};
	vkQueueSubmit(r->graphics_queue, 1, &submit_info, NULL);

	//Presentation
	VkPresentInfoKHR present_info = {
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, NULL,
		1, &r->semaphores[0],
		1, &r->swapchain,
		&image_index,
		NULL
	};
	vkQueuePresentKHR(r->present_queue, &present_info);
}
