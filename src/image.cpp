#include "image.hpp"
using namespace lightrail;

Image::Image(
	const vk::ImageCreateInfo& image_create_info,
	const VmaAllocationCreateInfo& alloc_create_info,
	const VmaAllocator& allocator
) : allocator(&allocator) {
	vmaCreateImage(
		allocator, 
		reinterpret_cast<const VkImageCreateInfo*>(&image_create_info),
		&alloc_create_info,
		reinterpret_cast<VkImage*>(&image),
		&alloc,
		nullptr
	);
}

void Image::destroy() {
	vmaDestroyImage(*allocator, image, alloc);
}
