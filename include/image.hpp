#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>

namespace lightrail {
	class Image {
		vk::Image image;
		VmaAllocation alloc;
		const VmaAllocator* allocator;

		public:
		Image() = default;
		Image(
			const vk::ImageCreateInfo&,
			const VmaAllocationCreateInfo&,
			const VmaAllocator&
		);
		operator const vk::Image&() {return image;}
		void destroy();
	};
}
