#include "vk_mem_alloc.h"
#include "renderer.hpp"
//#include <iostream>
#include <fstream>
using namespace lightrail;

Renderer::Renderer(SDL_Window *window) : window(window) {
	vk::ApplicationInfo app_info(
		"Lightrail", 1,
		"Lightrail", 1,
		VK_API_VERSION_1_2
	);
	std::array<const char*, 1> layers {"VK_LAYER_KHRONOS_validation"};
	//Extensions
	unsigned int ext_count;
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
	for (auto physical_device : physical_devices) {
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
			vk::DeviceQueueCreateInfo({}, graphics_queue_family, 1, &queue_priority),
		};
	} else {
		queue_create_infos = {
			vk::DeviceQueueCreateInfo({}, graphics_queue_family, 1, &queue_priority),
			vk::DeviceQueueCreateInfo({}, present_queue_family, 1, &queue_priority),
		};
	}
	std::vector<char const*> device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	device = physical_device.createDevice(vk::DeviceCreateInfo(
		{}, queue_create_infos, {}, device_extensions, {}
	));
	graphics_queue = device.getQueue(graphics_queue_family, 0);
	present_queue = device.getQueue(present_queue_family, 0);
	//Command pool
	command_pool = device.createCommandPool(
		vk::CommandPoolCreateInfo({}, graphics_queue_family)
	);
	//Command buffer
	command_buffer = device.allocateCommandBuffers(
		vk::CommandBufferAllocateInfo(
			command_pool,
			vk::CommandBufferLevel::ePrimary,
			1
		)
	).front();
}

void Renderer::create_swapchain(bool refresh) {
	auto surface_capabilities =
		physical_device.getSurfaceCapabilitiesKHR(surface);
	auto formats = physical_device.getSurfaceFormatsKHR(surface);
	vk::SurfaceFormatKHR image_format = formats.front();
	//TODO: Preferred format/colorspace?
	for (const auto& format : formats) {
		if (format.format == vk::Format::eB8G8R8A8Srgb
			&& format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			image_format = format;
			break;
		}
	}
	//TODO: Image extent refinement
	//Create swapchain
	auto new_swapchain = device.createSwapchainKHR(vk::SwapchainCreateInfoKHR(
		{},
		surface,
		surface_capabilities.minImageCount,
		image_format.format,
		image_format.colorSpace,
		surface_capabilities.currentExtent,
		1,
		vk::ImageUsageFlagBits::eColorAttachment,
		vk::SharingMode::eExclusive,
		{},
		surface_capabilities.currentTransform,
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::PresentModeKHR::eFifo,
		true,
		refresh ? swapchain : nullptr
	));
	//Free old swapchain resources
	if (refresh) {
		for (auto image_view : image_views)
			device.destroyImageView(image_view);
		device.destroySwapchainKHR(swapchain);
	}
	//Create swapchain resources
	swapchain = new_swapchain;
	images = device.getSwapchainImagesKHR(swapchain);
	image_views = std::vector<vk::ImageView>(images.size());
	vk::ComponentMapping component_mapping(
		vk::ComponentSwizzle::eR,
		vk::ComponentSwizzle::eG,
		vk::ComponentSwizzle::eB,
		vk::ComponentSwizzle::eA
	);
	vk::ImageSubresourceRange subresource_range(
		vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1
	);
	for (auto image : images) {
		image_views.push_back(device.createImageView(vk::ImageViewCreateInfo(
			{},
			image,
			vk::ImageViewType::e2D,
			image_format.format,
			component_mapping,
			subresource_range
		)));
	}
}

void Renderer::create_pipeline() {
	//Shaders
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

Renderer::~Renderer() {
	for (auto image_view : image_views)
		device.destroyImageView(image_view);
	device.destroySwapchainKHR(swapchain);
	device.freeCommandBuffers(command_pool, command_buffer);
	device.destroyCommandPool(command_pool);
	device.destroy();
	instance.destroy();
}
