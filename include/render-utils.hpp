#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>

namespace lightrail {
	class BufferWrapper {
		const VmaAllocator& allocator;
		size_t size;
		vk::Buffer buf;
		VmaAllocation alloc;
		public:
		BufferWrapper(
			const vk::BufferCreateInfo&,
			const VmaAllocationCreateInfo&,
			const VmaAllocator&
		);
		void write(const void*);
		void staged_write(const void*, const vk::CommandBuffer&, const vk::Queue&);
		const vk::Buffer& get_buf() const {return buf;}
		void destroy();
	};

	class Texture {
		const vk::Device& device;
		const VmaAllocator& allocator;
		vk::Image image;
		VmaAllocation alloc;
		vk::ImageView image_view;
		vk::Sampler sampler;
		public:
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

	struct Vertex {
		std::array<float, 2> position;
		std::array<float, 3> color;
		std::array<float, 2> texture_pos;
	};
}
