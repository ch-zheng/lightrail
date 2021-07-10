#include "vk_mem_alloc.h"
#include "renderer.hpp"
#include <algorithm>
#include <fstream>
//#include <iostream> //FIXME
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
	std::vector<char const*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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
	//Swapchain
	create_swapchain();
}

void Renderer::create_swapchain() {
	//Surface capabilities
	auto surface_capabilities =
		physical_device.getSurfaceCapabilitiesKHR(surface);
	auto formats = physical_device.getSurfaceFormatsKHR(surface);
	auto surface_format = formats.front();
	//TODO: Preferred format/colorspace?
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
	destroy_swapchain();
	swapchain = new_swapchain;

	//Images & image views
	auto images = device.getSwapchainImagesKHR(swapchain);
	image_views.reserve(images.size());
	vk::ComponentMapping component_mapping(
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity,
		vk::ComponentSwizzle::eIdentity
	);
	vk::ImageSubresourceRange subresource_range(
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

	//Render pass
	vk::AttachmentDescription attachments(
		{},
		surface_format.format,
		vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eClear,
		vk::AttachmentStoreOp::eStore,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::ePresentSrcKHR
	);
	vk::AttachmentReference color_attachments(0, vk::ImageLayout::eAttachmentOptimalKHR);
	vk::SubpassDescription subpasses(
		{},
		vk::PipelineBindPoint::eGraphics,
		{}, color_attachments, {}, {}, {}
	);
	render_pass = device.createRenderPass(vk::RenderPassCreateInfo(
		{},
		attachments,
		subpasses,
		{}
	));

	//Framebuffers
	framebuffers.reserve(image_views.size());
	for (auto& image_view : image_views) {
		framebuffers.push_back(device.createFramebuffer(vk::FramebufferCreateInfo(
			{},
			render_pass,
			image_view,
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
	for (auto image_view : image_views)
		device.destroyImageView(image_view);
	image_views.clear();
	device.destroySwapchainKHR(swapchain);
}

//TODO: Inline into create_swapchain()?
void Renderer::create_pipelines() {
	//Shaders
	auto vertex_shader = create_shader_module("shaders/basic.vert.spv");
	auto fragment_shader = create_shader_module("shaders/basic.frag.spv");
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages {
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
	vk::PipelineVertexInputStateCreateInfo vertex_input({}, {}, {});
	//Input assembly
	vk::PipelineInputAssemblyStateCreateInfo input_assembly(
		{}, vk::PrimitiveTopology::eTriangleList, false
	);
	//Viewport
	vk::Viewport viewports(
		0.0f, 0.0f,
		surface_extent.width, surface_extent.height,
		0.0f, 1.0f
	);
	vk::Rect2D scissors(vk::Offset2D(0, 0), surface_extent);
	vk::PipelineViewportStateCreateInfo viewport(
		{},
		viewports,
		scissors
	);
	//Rasterizer
	vk::PipelineRasterizationStateCreateInfo rasterization(
		{},
		false,
		false,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
		false, 0.0f, 0.0f, 0.0f,
		1.0f
	);
	//Multisampling
	vk::PipelineMultisampleStateCreateInfo multisample(
		{},
		vk::SampleCountFlagBits::e1,
		false,
		1.0f,
		nullptr,
		false,
		false
	);
	//TODO: Depth stencil
	//Color blending
	vk::PipelineColorBlendAttachmentState color_blend_attachments(
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
	vk::PipelineColorBlendStateCreateInfo color_blend(
		{},
		false,
		vk::LogicOp::eCopy,
		color_blend_attachments,
		{0.0f, 0.0f, 0.0f, 0.0f}
	);
	//TODO: Dynamic state?
	//TODO: Descriptor sets
	pipeline_layout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo(
		{}, {}, {}
	));

	//Create pipeline
	vk::GraphicsPipelineCreateInfo pipeline_create_infos(
		{},
		shader_stages,
		&vertex_input,
		&input_assembly,
		{},
		&viewport,
		&rasterization,
		&multisample,
		{},
		&color_blend,
		{},
		pipeline_layout,
		render_pass, 0,
		{}, {}
	);
	pipelines = device.createGraphicsPipelines({}, pipeline_create_infos).value;

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

void Renderer::draw() {
	uint32_t image_index = device.acquireNextImageKHR(
		swapchain, UINT64_MAX, image_available_semaphore, nullptr
	).value;
	//Record command buffer
	command_buffer.begin(vk::CommandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		nullptr
	));
	vk::ClearValue clear_values(std::array<float, 4> {0.0f, 0.0f, 0.0f, 1.0f});
	command_buffer.beginRenderPass(vk::RenderPassBeginInfo(
		render_pass,
		framebuffers[image_index],
		vk::Rect2D(vk::Offset2D(0, 0), surface_extent),
		clear_values
	), vk::SubpassContents::eInline);
	command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelines[0]);
	command_buffer.draw(3, 1, 0, 0);
	command_buffer.endRenderPass();
	command_buffer.end();
	//Submit & present
	vk::PipelineStageFlags wait_stage_flags = vk::PipelineStageFlagBits::eTopOfPipe;
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
	present_queue.waitIdle();
}

Renderer::~Renderer() {
	device.waitIdle();
	destroy_swapchain();
	device.destroySemaphore(image_available_semaphore);
	device.destroySemaphore(render_finished_semaphore);
	device.freeCommandBuffers(command_pool, command_buffer);
	device.destroyCommandPool(command_pool);
	device.destroy();
	instance.destroySurfaceKHR(surface);
	instance.destroy();
}
