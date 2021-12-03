#include "renderer.h"
#include <SDL2/SDL_vulkan.h>

static bool create_swapchain(
	const struct Renderer* const r,
	struct Swapchain* const old_swapchain,
	struct Swapchain* const result) {
	struct Swapchain s;
	if (vkDeviceWaitIdle(r->device) != VK_SUCCESS) return true;

	//Surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		r->physical_device,
		r->surface,
		&surface_capabilities
	);
	//Surface format
	uint32_t format_count;
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
	s.extent = surface_capabilities.currentExtent;
	int drawable_width, drawable_height;
	SDL_Vulkan_GetDrawableSize(r->window, &drawable_width, &drawable_height);
	//Special case
	if (surface_capabilities.currentExtent.width == UINT32_MAX) {
		if (drawable_width > surface_capabilities.maxImageExtent.width)
			s.extent.width = surface_capabilities.maxImageExtent.width;
		else if (drawable_width < surface_capabilities.minImageExtent.width)
			s.extent.width = surface_capabilities.minImageExtent.width;
		else s.extent.width = drawable_width;
	}
	if (surface_capabilities.currentExtent.height == UINT32_MAX) {
		if (drawable_height > surface_capabilities.maxImageExtent.height)
			s.extent.height = surface_capabilities.maxImageExtent.height;
		else if (drawable_height < surface_capabilities.minImageExtent.height)
			s.extent.height = surface_capabilities.minImageExtent.height;
		else s.extent.height = drawable_height;
	}

	//Swapchain
	uint32_t queue_family_indices[2] = {
		r->graphics_queue_family,
		r->present_queue_family
	};
	VkSwapchainCreateInfoKHR swapchain_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, NULL, 0,
		r->surface,
		surface_capabilities.minImageCount,
		format.format,
		format.colorSpace,
		s.extent,
		1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		1,
		queue_family_indices,
		surface_capabilities.currentTransform,
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		false,
		NULL
	};
	//Image sharing mode
	if (r->graphics_queue_family != r->present_queue_family) {
		swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = 2;
	}
	//TODO: Present mode
	if (vkCreateSwapchainKHR(
		r->device,
		&swapchain_info,
		NULL,
		&s.swapchain)
		!= VK_SUCCESS) {
		printf("Error creating swapchain!\n");
		return true;
	}

	*result = s;
	return false;
}
