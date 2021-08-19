#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>

namespace lightrail {
	class Buffer {
		vk::Buffer buffer;
		VmaAllocation alloc;
		size_t size;
		const VmaAllocator* allocator = nullptr;

		public:
		Buffer() = default;
		Buffer(
			const vk::BufferCreateInfo&,
			const VmaAllocationCreateInfo&,
			const VmaAllocator&
		);
		operator const vk::Buffer&() const {return buffer;}
		void write(const void*);
		void staged_write(const void*, const vk::CommandBuffer&, const vk::Queue&);
		void destroy();
	};
}
