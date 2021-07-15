#pragma once
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace lightrail {
	class BufferWrapper {
		const vk::Device& device;
		const VmaAllocator& allocator;
		size_t size;
		vk::Buffer buf;
		VmaAllocation alloc;
		public:
		BufferWrapper(
			const vk::Device&, const VmaAllocator&,
			const vk::BufferCreateInfo&, const VmaAllocationCreateInfo&
		);
		void write(const void*);
		void staged_write(const void*, const vk::CommandBuffer&, const vk::Queue&);
		const vk::Buffer& get_buf() const {return buf;}
		void destroy();
	};

	struct Vertex {
		glm::vec2 position;
		glm::vec3 color;
	};
}
