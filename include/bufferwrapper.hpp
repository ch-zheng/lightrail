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
}
