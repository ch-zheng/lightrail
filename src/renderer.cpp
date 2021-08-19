#define VMA_IMPLEMENTATION
#include "renderer.hpp"
#include <algorithm>
//#include <future>
#include <fstream>
#include <stack>
//#include <iostream>
using namespace lightrail;

Renderer::Renderer(SDL_Window *window) : window(window) {
	vk::ApplicationInfo app_info(
		"Lightrail", 1,
		"Lightrail", 1,
		VK_API_VERSION_1_2
	);
	//FIXME: Validation layer
	std::array<const char*, 1> layers {"VK_LAYER_KHRONOS_validation"};
	//Extensions
	uint32_t ext_count;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, nullptr))
		throw std::runtime_error("Error counting Vulkan instance extensions");
	std::vector<const char*> extensions(ext_count);
	if (!SDL_Vulkan_GetInstanceExtensions(window, &ext_count, extensions.data()))
		throw std::runtime_error("Error getting Vulkan instance extensions");
	instance = vk::createInstance(vk::InstanceCreateInfo(
		{}, &app_info,
		layers.size(), layers.data(),
		extensions.size(), extensions.data()
	));
	VkSurfaceKHR c_surface;
	SDL_Vulkan_CreateSurface(window, instance, &c_surface);
	surface = c_surface;

	//Physical device selection
	//TODO: Support for separate transfer queue
	bool device_found = false;
	auto physical_devices = instance.enumeratePhysicalDevices();
	for (auto& physical_device : physical_devices) {
		auto properties = physical_device.getProperties();
		//Queue families
		bool graphics_support = false, present_support = false;
		auto queue_family_properties = physical_device.getQueueFamilyProperties();
		for (size_t i = 0; i < queue_family_properties.size(); ++i) {
			//Graphics
			bool queue_graphics_support =
				(queue_family_properties[i].queueFlags & vk::QueueFlagBits::eGraphics)
				!= vk::QueueFlagBits(0);
			if (queue_graphics_support && !graphics_support) {
				graphics_support = true;
				graphics_queue_family = i;
			}
			//Present
			bool queue_present_support = physical_device.getSurfaceSupportKHR(i, surface);
			if (queue_present_support && !present_support) {
				present_support = true;
				present_queue_family = i;
			}
			if (graphics_support && present_support) break;
		}
		//Evaluation
		if (graphics_support && present_support) {
			this->physical_device = physical_device;
			device_found = true;
			break;
		}
	}
	if (!device_found) throw std::runtime_error("No graphics device found");

	//Device properties
	auto properties = physical_device.getProperties();
	//Multisampling
	vk::SampleCountFlags sample_count_flags =
		properties.limits.framebufferColorSampleCounts
		& properties.limits.framebufferDepthSampleCounts;
	if (sample_count_flags & vk::SampleCountFlagBits::e64)
		sample_count = vk::SampleCountFlagBits::e64;
	else if (sample_count_flags & vk::SampleCountFlagBits::e32)
		sample_count = vk::SampleCountFlagBits::e32;
	else if (sample_count_flags & vk::SampleCountFlagBits::e16)
		sample_count = vk::SampleCountFlagBits::e16;
	else if (sample_count_flags & vk::SampleCountFlagBits::e8)
		sample_count = vk::SampleCountFlagBits::e8;
	else if (sample_count_flags & vk::SampleCountFlagBits::e4)
		sample_count = vk::SampleCountFlagBits::e4;
	else if (sample_count_flags & vk::SampleCountFlagBits::e2)
		sample_count = vk::SampleCountFlagBits::e2;

	//Queues & Logical device
	float queue_priority = 0.0f;
	std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
	if (graphics_queue_family == present_queue_family) {
		queue_create_infos = {
			vk::DeviceQueueCreateInfo({}, graphics_queue_family, 1, &queue_priority)
		};
	} else {
		queue_create_infos = {
			vk::DeviceQueueCreateInfo({}, graphics_queue_family, 1, &queue_priority),
			vk::DeviceQueueCreateInfo({}, present_queue_family, 1, &queue_priority)
		};
	}
	const std::vector<char const*> device_extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME
	};
	device = physical_device.createDevice(vk::DeviceCreateInfo(
		{}, queue_create_infos, {}, device_extensions, {}
	));
	graphics_queue = device.getQueue(graphics_queue_family, 0);
	present_queue = device.getQueue(present_queue_family, 0);

	//Command pool
	command_pool = device.createCommandPool(vk::CommandPoolCreateInfo(
		vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		graphics_queue_family
	));
	//Command buffer
	command_buffer = device.allocateCommandBuffers(
		vk::CommandBufferAllocateInfo(
			command_pool,
			vk::CommandBufferLevel::ePrimary,
			1
		)
	).front();

	//Semaphores
	image_available_semaphore = device.createSemaphore({});
	render_finished_semaphore = device.createSemaphore({});

	//Pipeline cache
	std::ifstream pipeline_cache_file(
		PIPELINE_CACHE_FILENAME,
		std::ifstream::ate|std::ifstream::binary
	);
	if (pipeline_cache_file.good()) {
		size_t file_size = pipeline_cache_file.tellg();
		std::vector<char> buffer(file_size);
		pipeline_cache_file.seekg(0);
		pipeline_cache_file.read(buffer.data(), file_size);
		pipeline_cache = device.createPipelineCache(
			vk::PipelineCacheCreateInfo(
				{},
				file_size,
				buffer.data()
			)
		);
	} else {
		pipeline_cache = device.createPipelineCache(
			vk::PipelineCacheCreateInfo(
				{},
				0,
				nullptr
			)
		);
	}
	pipeline_cache_file.close();

	//Memory structures
	//Allocator
	VmaAllocatorCreateInfo allocator_create_info {};
	allocator_create_info.vulkanApiVersion = VK_API_VERSION_1_2;
	allocator_create_info.instance = instance;
	allocator_create_info.physicalDevice = physical_device;
	allocator_create_info.device = device;
	vmaCreateAllocator(&allocator_create_info, &allocator);
	//Texture
	texture = std::unique_ptr<Texture>(new Texture(
		"./assets/stone.bmp",
		device,
		allocator,
		command_buffer,
		graphics_queue
	));

	//Descriptor layout
	/* Binding | Type | Name
		0 | Combined Sampler | Texture
		1 | Storage | Transformations
		2 | Storage | Transformation offsets
		3 | Uniform | Indexed transformation offset
	*/
	const std::array<vk::DescriptorSetLayoutBinding, 4> descriptor_layout_bindings {
		//Combined texture sampler
		vk::DescriptorSetLayoutBinding(
			0,
			vk::DescriptorType::eCombinedImageSampler,
			1,
			vk::ShaderStageFlagBits::eFragment
		),
		//Transformations
		vk::DescriptorSetLayoutBinding(
			1,
			vk::DescriptorType::eStorageBuffer,
			1,
			vk::ShaderStageFlagBits::eVertex
		),
		//Transformation offsets
		vk::DescriptorSetLayoutBinding(
			2,
			vk::DescriptorType::eStorageBuffer,
			1,
			vk::ShaderStageFlagBits::eVertex
		),
		//Uniform buffer object
		vk::DescriptorSetLayoutBinding(
			3,
			vk::DescriptorType::eUniformBuffer,
			1,
			vk::ShaderStageFlagBits::eVertex
		)
	};
	descriptor_layout = device.createDescriptorSetLayout(
		vk::DescriptorSetLayoutCreateInfo(
			{},
			descriptor_layout_bindings
		)
	);
	//Descriptor pool
	const std::array<vk::DescriptorPoolSize, 3> descriptor_pool_sizes {
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1),
		vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 2),
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1)
	};
	descriptor_pool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo(
		{},
		1,
		descriptor_pool_sizes
	));
	//Descriptor set creation
	auto descriptor_sets = device.allocateDescriptorSets(
		vk::DescriptorSetAllocateInfo(
			descriptor_pool,
			descriptor_layout
		)
	);
	descriptor_set = descriptor_sets[0];

	//Update texture descriptor
	const vk::DescriptorImageInfo descriptor_image_info(
		texture->get_sampler(),
		texture->get_image_view(),
		vk::ImageLayout::eShaderReadOnlyOptimal
	);
	device.updateDescriptorSets(
		vk::WriteDescriptorSet(
			descriptor_set,
			0,
			0,
			vk::DescriptorType::eCombinedImageSampler,
			descriptor_image_info
		), {}
	);
	
	//Swapchain
	create_swapchain();
}

void Renderer::create_swapchain() {
	device.waitIdle();
	//Surface capabilities
	const auto surface_capabilities =
		physical_device.getSurfaceCapabilitiesKHR(surface);
	const auto formats = physical_device.getSurfaceFormatsKHR(surface);
	auto surface_format = formats.front();
	for (const auto& format : formats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb
			&& format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			surface_format = format;
			break;
		}
	}
	surface_extent = surface_capabilities.currentExtent;
	if (surface_extent.width == UINT32_MAX
		&& surface_extent.height == UINT32_MAX) {
		int width, height;
		SDL_Vulkan_GetDrawableSize(window, &width, &height);
		surface_extent.setWidth(std::clamp<uint32_t>(
			width,
			surface_capabilities.minImageExtent.width,
			surface_capabilities.maxImageExtent.width
		));
		surface_extent.setHeight(std::clamp<uint32_t>(
			height,
			surface_capabilities.minImageExtent.height,
			surface_capabilities.maxImageExtent.height
		));
	}

	camera.set_aspect_ratio(
		static_cast<float>(surface_extent.width)
		/ static_cast<float>(surface_extent.height)
	);

	//Create swapchain
	vk::SharingMode sharing_mode;
	std::vector<uint32_t> swapchain_queue_families;
	if (graphics_queue_family == present_queue_family) {
		sharing_mode = vk::SharingMode::eExclusive;
		swapchain_queue_families = {graphics_queue_family};
	} else {
		sharing_mode = vk::SharingMode::eConcurrent;
		swapchain_queue_families = {graphics_queue_family, present_queue_family};
	}
	auto new_swapchain = device.createSwapchainKHR(vk::SwapchainCreateInfoKHR(
		{},
		surface,
		surface_capabilities.minImageCount,
		surface_format.format,
		surface_format.colorSpace,
		surface_extent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
		sharing_mode,
		swapchain_queue_families,
		surface_capabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::PresentModeKHR::eFifo,
		true,
		swapchain
	));
	//Free old swapchain resources
	if (swapchain) destroy_swapchain();
	swapchain = new_swapchain;

	//Images & image views
	const auto images = device.getSwapchainImagesKHR(swapchain);
	image_views.reserve(images.size());
	vk::ComponentMapping component_mapping(
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity
	);
	const vk::ImageSubresourceRange subresource_range(
		vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
	);
	for (auto& image : images) {
		auto image_view = device.createImageView(vk::ImageViewCreateInfo(
			{},
			image,
			vk::ImageViewType::e2D,
			surface_format.format,
			component_mapping,
			subresource_range
		));
		image_views.push_back(image_view);
	}

	//Multisampling buffer
	VmaAllocationCreateInfo color_image_alloc_create_info {};
	//TODO: Lazy allocation
	color_image_alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	color_image = Image(
		vk::ImageCreateInfo(
			{},
			vk::ImageType::e2D,
			surface_format.format,
			vk::Extent3D(surface_extent.width, surface_extent.height, 1),
			1,
			1,
			sample_count,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment
			| vk::ImageUsageFlagBits::eTransientAttachment
		),
		color_image_alloc_create_info,
		allocator
	);
	color_image_view = device.createImageView(vk::ImageViewCreateInfo(
		{},
		color_image,
		vk::ImageViewType::e2D,
		surface_format.format,
		vk::ComponentMapping(
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity
		),
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
	));

	//Depth buffer
	//TODO: Format support determination
	const vk::Format depth_format = vk::Format::eD32SfloatS8Uint;
	VmaAllocationCreateInfo depth_image_alloc_create_info {};
	depth_image_alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depth_image = Image(
		vk::ImageCreateInfo(
			{},
			vk::ImageType::e2D,
			depth_format,
			vk::Extent3D(surface_extent.width, surface_extent.height, 1),
			1,
			1,
			sample_count,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment
		),
		depth_image_alloc_create_info,
		allocator
	);
	depth_image_view = device.createImageView(vk::ImageViewCreateInfo(
		{},
		depth_image,
		vk::ImageViewType::e2D,
		depth_format,
		vk::ComponentMapping(
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity
		),
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1)
	));

	//Render pass
	const std::array<vk::AttachmentDescription, 3> attachments {
		//Color attachment
		vk::AttachmentDescription(
			{},
			surface_format.format,
			sample_count,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal
		),
		//Resolve attachment
		vk::AttachmentDescription(
			{},
			surface_format.format,
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::ePresentSrcKHR
		),
		//Depth attachment
		vk::AttachmentDescription(
			{},
			depth_format,
			sample_count,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eDontCare,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal
		)
	};
	const vk::AttachmentReference color_attachments(0, vk::ImageLayout::eAttachmentOptimalKHR);
	const vk::AttachmentReference resolve_attachments(1, vk::ImageLayout::eColorAttachmentOptimal);
	const vk::AttachmentReference depth_attachment(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	const vk::SubpassDescription subpasses(
		{},
		vk::PipelineBindPoint::eGraphics,
		{}, color_attachments, resolve_attachments, &depth_attachment, {}
	);
	const vk::SubpassDependency dependencies(
		VK_SUBPASS_EXTERNAL,
		0,
		vk::PipelineStageFlagBits::eEarlyFragmentTests,
		vk::PipelineStageFlagBits::eEarlyFragmentTests,
		vk::AccessFlagBits::eNoneKHR,
		vk::AccessFlagBits::eDepthStencilAttachmentWrite
	);
	render_pass = device.createRenderPass(vk::RenderPassCreateInfo(
		{},
		attachments,
		subpasses,
		dependencies
	));

	//Framebuffers
	framebuffers.reserve(image_views.size());
	for (auto& image_view : image_views) {
		std::array<vk::ImageView, 3> attachments {
			color_image_view,
			image_view,
			depth_image_view
		};
		framebuffers.push_back(device.createFramebuffer(vk::FramebufferCreateInfo(
			{},
			render_pass,
			attachments,
			surface_extent.width,
			surface_extent.height,
			1
		)));
	}

	//Pipelines
	create_pipelines();
}

void Renderer::destroy_swapchain() {
	device.destroyPipelineLayout(pipeline_layout);
	for (auto pipeline : pipelines)
		device.destroyPipeline(pipeline);
	pipelines.clear();
	for (auto framebuffer : framebuffers)
		device.destroyFramebuffer(framebuffer);
	framebuffers.clear();
	device.destroyRenderPass(render_pass);
	device.destroyImageView(depth_image_view);
	device.destroyImageView(color_image_view);
	for (auto image_view : image_views)
		device.destroyImageView(image_view);
	image_views.clear();
	depth_image.destroy();
	color_image.destroy();
	device.destroySwapchainKHR(swapchain);
}

void Renderer::create_pipelines() {
	//Shaders
	const auto vertex_shader = create_shader_module("shaders/basic.vert.spv");
	const auto fragment_shader = create_shader_module("shaders/basic.frag.spv");
	const std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {
		vk::PipelineShaderStageCreateInfo(
			{},
			vk::ShaderStageFlagBits::eVertex,
			vertex_shader,
			"main"
		),
		vk::PipelineShaderStageCreateInfo(
			{},
			vk::ShaderStageFlagBits::eFragment,
			fragment_shader,
			"main"
		)
	};

	//Fixed functions
	//Vertex input
	const vk::VertexInputBindingDescription binding_descriptions(0, sizeof(Vertex));
	const std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions {
		vk::VertexInputAttributeDescription(
			0,
			0,
			vk::Format::eR32G32B32Sfloat,
			offsetof(Vertex, position)
		),
		vk::VertexInputAttributeDescription(
			1,
			0,
			vk::Format::eR32G32B32Sfloat,
			offsetof(Vertex, texture_coords)
		),
		vk::VertexInputAttributeDescription(
			2,
			0,
			vk::Format::eR32G32B32Sfloat,
			offsetof(Vertex, normal)
		),
	};
	const vk::PipelineVertexInputStateCreateInfo vertex_input(
		{}, binding_descriptions, attribute_descriptions
	);
	//Input assembly
	const vk::PipelineInputAssemblyStateCreateInfo input_assembly(
		{}, vk::PrimitiveTopology::eTriangleList, false
	);
	//Viewport
	const vk::Viewport viewports(
		0.0f, 0.0f,
		surface_extent.width, surface_extent.height,
		0.0f, 1.0f
	);
	const vk::Rect2D scissors(vk::Offset2D(0, 0), surface_extent);
	const vk::PipelineViewportStateCreateInfo viewport(
		{},
		viewports,
		scissors
	);
	//Rasterizer
	const vk::PipelineRasterizationStateCreateInfo rasterization(
		{},
		false,
		false,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
		false, 0.0f, 0.0f, 0.0f,
		1.0f
	);
	//Multisampling
	const vk::PipelineMultisampleStateCreateInfo multisample(
		{},
		sample_count,
		false,
		1.0f,
		nullptr,
		false,
		false
	);
	//Depth stencil
	//TODO: Stencil
	const vk::PipelineDepthStencilStateCreateInfo depth_stencil(
		{},
		true,
		true,
		vk::CompareOp::eLess,
		false,
		false
	);
	//Color blending
	const vk::PipelineColorBlendAttachmentState color_blend_attachments(
		true,
		vk::BlendFactor::eSrcAlpha,
		vk::BlendFactor::eOneMinusSrcAlpha,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eOne,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR
		| vk::ColorComponentFlagBits::eG
		| vk::ColorComponentFlagBits::eB
		| vk::ColorComponentFlagBits::eA
	);
	const vk::PipelineColorBlendStateCreateInfo color_blend(
		{},
		false,
		vk::LogicOp::eCopy,
		color_blend_attachments,
		{0.0f, 0.0f, 0.0f, 0.0f}
	);
	//Layout
	const vk::PushConstantRange projection_constant(
		vk::ShaderStageFlagBits::eVertex,
		0,
		16 * sizeof(float)
	);
	pipeline_layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo(
		{},
		descriptor_layout,
		projection_constant
	));

	//Create pipeline
	const vk::GraphicsPipelineCreateInfo pipeline_create_infos(
		{},
		shader_stages,
		&vertex_input,
		&input_assembly,
		{},
		&viewport,
		&rasterization,
		&multisample,
		&depth_stencil,
		&color_blend,
		{},
		pipeline_layout,
		render_pass, 0,
		{}, {}
	);
	pipelines = device.createGraphicsPipelines(pipeline_cache, pipeline_create_infos).value;

	//Cleanup
	device.destroyShaderModule(vertex_shader);
	device.destroyShaderModule(fragment_shader);
}

vk::ShaderModule Renderer::create_shader_module(std::string filename) {
	//Read shader file
	std::ifstream shader_file(filename, std::ifstream::ate|std::ifstream::binary);
	size_t file_size = shader_file.tellg();
	std::vector<char> buffer(file_size);
	shader_file.seekg(0);
	shader_file.read(buffer.data(), file_size);
	shader_file.close();
	//Create shader module
	return device.createShaderModule(vk::ShaderModuleCreateInfo(
		{}, file_size, reinterpret_cast<const uint32_t*>(buffer.data())
	));
}

void Renderer::destroy_scene_buffers() {
	//Draw buffers
	std::cout << "A" << std::endl;
	vertices.destroy();
	std::cout << "B" << std::endl;
	indices.destroy();
	std::cout << "C" << std::endl;
	draw_commands.destroy();
	std::cout << "D" << std::endl;
	draw_indexed_commands.destroy();
	std::cout << "E" << std::endl;
	//Descriptors
	transformations.destroy();
	std::cout << "F" << std::endl;
	transformation_offsets.destroy();
	std::cout << "G" << std::endl;
	uniforms.destroy();
	std::cout << "H" << std::endl;
	//TODO: Textures
}

void Renderer::draw() {
	//TODO: Deal with empty buffers
	if (!scene_loaded) return;
	const uint32_t image_index = device.acquireNextImageKHR(
		swapchain, UINT64_MAX, image_available_semaphore, nullptr
	).value;
	//Record command buffer
	command_buffer.begin(vk::CommandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	));
	const std::array<vk::ClearValue, 3> clear_values {
		vk::ClearValue(std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f}),
		vk::ClearValue(std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f}),
		vk::ClearDepthStencilValue(1.0f, 0.0f)
	};
	command_buffer.beginRenderPass(vk::RenderPassBeginInfo(
		render_pass,
		framebuffers[image_index],
		vk::Rect2D(vk::Offset2D(0, 0), surface_extent),
		clear_values
	), vk::SubpassContents::eInline);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines[0]);
	//Descriptors
	command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		pipeline_layout,
		0,
		descriptor_set,
		{}
	);
	command_buffer.pushConstants(
		pipeline_layout,
		vk::ShaderStageFlagBits::eVertex,
		0,
		16 * sizeof(float),
		camera.get_transform().data()
	);
	//Drawing
	const vk::Buffer vertex_buffer(static_cast<const vk::Buffer>(vertices));
	if (vertex_buffer) {
		command_buffer.bindVertexBuffers(0, vertex_buffer, {0});
		command_buffer.drawIndirect(
			static_cast<const vk::Buffer>(draw_commands),
			0,
			draw_command_count,
			sizeof(vk::DrawIndirectCommand)
		);
	}
	const vk::Buffer index_buffer(static_cast<const vk::Buffer>(indices));
	if (index_buffer) {
		command_buffer.bindIndexBuffer(index_buffer, 0, vk::IndexType::eUint16);
		command_buffer.drawIndexedIndirect(
			static_cast<const vk::Buffer>(draw_commands),
			0,
			draw_indexed_command_count,
			sizeof(vk::DrawIndexedIndirectCommand)
		);
	}
	command_buffer.endRenderPass();
	command_buffer.end();
	//Submit & present
	const vk::PipelineStageFlags wait_stage_flags = vk::PipelineStageFlagBits::eTopOfPipe;
	graphics_queue.submit(vk::SubmitInfo(
		image_available_semaphore,
		wait_stage_flags,
		command_buffer,
		render_finished_semaphore
	), nullptr);
	try {
		auto present_result = present_queue.presentKHR(vk::PresentInfoKHR(
			render_finished_semaphore, swapchain, image_index
		));
	} catch (const vk::OutOfDateKHRError& error) {
		present_queue.waitIdle();
		create_swapchain();
	}
}

void Renderer::wait() {
	//FIXME: This is stupid
	present_queue.waitIdle();
}

void Renderer::load_scene(const Scene& scene) {
	//Collect drawable objects
	std::vector<Drawable> drawables;
	std::stack<std::pair<Node, Eigen::Affine3f>> pending;
	for (const auto& node : scene.nodes)
		pending.push({node, Eigen::Affine3f::Identity()});
	while (!pending.empty()) {
		const auto& current = pending.top();
		const Eigen::Affine3f t = current.second * current.first.transformation;
		if (current.first.mesh >= 0) {
			Drawable d;
			d.mesh = current.first.mesh;
			d.transformation = t;
			drawables.push_back(d);
		}
		for (const auto& node : current.first.children)
			pending.push({node, t});
		pending.pop();
	}
	//Sort drawables (ascending by mesh, non-indexed before indexed)
	std::sort(
		drawables.begin(),
		drawables.end(),
		[&scene](Drawable a, Drawable b) {
			bool a_ni = scene.meshes[a.mesh].indices.empty(),
				b_ni = scene.meshes[b.mesh].indices.empty();
			if (a_ni && !b_ni) return true;
			else if (!a_ni && b_ni) return false;
			else return a.mesh < b.mesh;
		}
	);
	//Catalog drawables
	std::vector<Eigen::Affine3f> transformations;
	transformations.reserve(drawables.size());
	std::vector<uint32_t> mesh_counts(scene.meshes.size(), 0);
	uint32_t indexed_transformation_offset = 0;
	for (const auto& drawable : drawables) {
		transformations.push_back(drawable.transformation);
		mesh_counts[drawable.mesh] += 1;
		if (!scene.meshes[drawable.mesh].indices.empty())
			++indexed_transformation_offset;
	}

	//Load meshes
	std::vector<Vertex> vertices, indexed_vertices;
	std::vector<uint16_t> indices;
	std::vector<vk::DrawIndirectCommand> draw_commands;
	std::vector<vk::DrawIndexedIndirectCommand> draw_indexed_commands;
	for (size_t i = 0; i < scene.meshes.size(); ++i) {
		const auto& mesh = scene.meshes[i];
		if (mesh.indices.empty()) {
			//Non-indexed mesh
			//Append command
			draw_commands.push_back(vk::DrawIndirectCommand(
				mesh.vertices.size(),
				mesh_counts[i],
				vertices.size()
			));
			//Append vertices
			vertices.reserve(mesh.vertices.size());
			vertices.insert(
				vertices.end(),
				mesh.vertices.begin(),
				mesh.vertices.end()
			);
		} else {
			//Indexed mesh
			//Append command
			draw_indexed_commands.push_back(vk::DrawIndexedIndirectCommand(
				mesh.indices.size(),
				mesh_counts[i],
				indices.size(),
				indexed_vertices.size()
			));
			//Append vertices
			indexed_vertices.reserve(mesh.vertices.size());
			indexed_vertices.insert(
				indexed_vertices.end(),
				mesh.vertices.begin(),
				mesh.vertices.end()
			);
			//Append indices
			indices.reserve(mesh.indices.size());
			indices.insert(
				indices.end(),
				mesh.indices.begin(),
				mesh.indices.end()
			);
		}
	}
	for (auto& cmd : draw_indexed_commands)
		cmd.vertexOffset += vertices.size();

	//Generate offsets
	std::vector<uint32_t> transformation_offsets;
	transformation_offsets.reserve(scene.meshes.size());
	uint32_t sum = 0;
	for (const auto& cmd : draw_commands) {
		transformation_offsets.push_back(sum);
		sum += cmd.instanceCount;
	}
	for (const auto& cmd : draw_indexed_commands) {
		transformation_offsets.push_back(sum);
		sum += cmd.instanceCount;
	}
	draw_command_count = draw_commands.size();
	draw_indexed_command_count = draw_indexed_commands.size();
	//Uniforms
	uint32_t indexed_vertex_offset = vertices.size();
	std::array<uint32_t, 2> uniforms {
		indexed_vertex_offset,
		indexed_transformation_offset
	};

	//Write to buffers
	//TODO: Parallelization
	if (scene_loaded) destroy_scene_buffers(); //Destroy previous scene
	//Shared allocation create info
	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	//Vertex buffer
	//Merge vertices
	vertices.reserve(indexed_vertices.size());
	vertices.insert(
		vertices.end(),
		indexed_vertices.begin(),
		indexed_vertices.end()
	);
	const vk::BufferCreateInfo vertex_buffer_create_info(
		{},
		vertices.size() * sizeof(Vertex),
		vk::BufferUsageFlagBits::eVertexBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	this->vertices = Buffer(
		vertex_buffer_create_info,
		alloc_create_info,
		allocator
	);
	this->vertices.staged_write(
		vertices.data(),
		command_buffer,
		graphics_queue
	);
	//Index buffer
	const vk::BufferCreateInfo index_buffer_create_info(
		{},
		indices.size() * sizeof(uint16_t),
		vk::BufferUsageFlagBits::eIndexBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	this->indices = Buffer(
		index_buffer_create_info,
		alloc_create_info,
		allocator
	);
	this->indices.staged_write(
		indices.data(),
		command_buffer,
		graphics_queue
	);
	//Draw commands
	const vk::BufferCreateInfo draw_command_buffer_create_info(
		{},
		draw_commands.size() * sizeof(vk::DrawIndirectCommand),
		vk::BufferUsageFlagBits::eIndirectBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	this->draw_commands = Buffer(
		draw_command_buffer_create_info,
		alloc_create_info,
		allocator
	);
	this->draw_commands.staged_write(
		draw_commands.data(),
		command_buffer,
		graphics_queue
	);
	//Indexed draw commands
	const vk::BufferCreateInfo draw_indexed_command_buffer_create_info(
		{},
		draw_indexed_commands.size() * sizeof(vk::DrawIndexedIndirectCommand),
		vk::BufferUsageFlagBits::eIndirectBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	this->draw_indexed_commands = Buffer(
		draw_indexed_command_buffer_create_info,
		alloc_create_info,
		allocator
	);
	this->draw_indexed_commands.staged_write(
		draw_indexed_commands.data(),
		command_buffer,
		graphics_queue
	);

	//Descriptor buffers
	//Transformation buffer
	const vk::BufferCreateInfo transform_buffer_create_info(
		{},
		transformations.size() * sizeof(Eigen::Affine3f),
		vk::BufferUsageFlagBits::eStorageBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	this->transformations = Buffer(
		transform_buffer_create_info,
		alloc_create_info,
		allocator
	);
	this->transformations.staged_write(
		transformations.data(),
		command_buffer,
		graphics_queue
	);
	//Transformation offsets
	const vk::BufferCreateInfo transform_offset_buffer_create_info(
		{},
		transformation_offsets.size() * sizeof(uint32_t),
		vk::BufferUsageFlagBits::eStorageBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	Buffer transform_offset_buffer(
		transform_offset_buffer_create_info,
		alloc_create_info,
		allocator
	);
	transform_offset_buffer.staged_write(
		transformation_offsets.data(),
		command_buffer,
		graphics_queue
	);
	//Uniforms
	const vk::BufferCreateInfo indexed_offset_buffer_create_info(
		{},
		uniforms.size() * sizeof(uint32_t),
		vk::BufferUsageFlagBits::eUniformBuffer
		| vk::BufferUsageFlagBits::eTransferDst
	);
	Buffer indexed_offset_buffer(
		indexed_offset_buffer_create_info,
		alloc_create_info,
		allocator
	);
	indexed_offset_buffer.staged_write(
		uniforms.data(),
		command_buffer,
		graphics_queue
	);

	//Update descriptor set
	const vk::DescriptorBufferInfo transform_descriptor_info(
		this->transformations,
		0,
		VK_WHOLE_SIZE
	);
	const vk::DescriptorBufferInfo transform_offset_descriptor_info(
		transform_offset_buffer,
		0,
		VK_WHOLE_SIZE
	);
	const vk::DescriptorBufferInfo indexed_offset_descriptor_info(
		indexed_offset_buffer,
		0,
		VK_WHOLE_SIZE
	);
	const std::array<vk::WriteDescriptorSet, 3> descriptor_set_write {
		vk::WriteDescriptorSet(
			descriptor_set,
			1,
			0,
			vk::DescriptorType::eStorageBuffer,
			{},
			transform_descriptor_info
		),
		vk::WriteDescriptorSet(
			descriptor_set,
			2,
			0,
			vk::DescriptorType::eStorageBuffer,
			{},
			transform_offset_descriptor_info
		),
		vk::WriteDescriptorSet(
			descriptor_set,
			3,
			0,
			vk::DescriptorType::eUniformBuffer,
			{},
			indexed_offset_descriptor_info
		),
	};
	device.updateDescriptorSets(descriptor_set_write, {});

	//TODO: Load textures
	scene_loaded = true;
}

Renderer::~Renderer() {
	device.waitIdle();
	//Descriptors
	device.destroyDescriptorPool(descriptor_pool);
	device.destroyDescriptorSetLayout(descriptor_layout);
	//Memory structures
	texture->destroy();
	std::cout << "Alpha" << std::endl;
	destroy_scene_buffers();
	std::cout << "Bravo" << std::endl;
	//Save pipeline cache
	std::ofstream pipeline_cache_file(PIPELINE_CACHE_FILENAME, std::ofstream::binary);
	auto pipeline_cache_data = device.getPipelineCacheData(pipeline_cache);
	pipeline_cache_file.write(
		reinterpret_cast<const char*>(pipeline_cache_data.data()),
		sizeof(uint8_t) * pipeline_cache_data.size()
	);
	pipeline_cache_file.close();
	//Destructors
	destroy_swapchain();
	device.destroyPipelineCache(pipeline_cache);
	device.destroySemaphore(image_available_semaphore);
	device.destroySemaphore(render_finished_semaphore);
	device.freeCommandBuffers(command_pool, command_buffer);
	device.destroyCommandPool(command_pool);
	vmaDestroyAllocator(allocator);
	device.destroy();
	instance.destroySurfaceKHR(surface);
	instance.destroy();
}
