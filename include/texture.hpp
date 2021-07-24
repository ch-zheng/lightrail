#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>

namespace lightrail {
	class Texture {
		vk::Image image;
		VmaAllocation alloc;
		vk::ImageView image_view;
		vk::Sampler sampler;
		const vk::Device* device;
		const VmaAllocator* allocator;

		public:
		Texture() = default;
		Texture(
			const char* filename,
			const vk::Device&,
			const VmaAllocator&,
			const vk::CommandBuffer&,
			const vk::Queue&
		);
		const vk::ImageView& get_image_view() {return image_view;}
		const vk::Sampler& get_sampler() {return sampler;}
		void destroy();
	};
}
