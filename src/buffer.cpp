#include "buffer.hpp"
#include <SDL2/SDL_surface.h>
#include <iostream>
using namespace lightrail;

Buffer::Buffer(
	const vk::BufferCreateInfo& buffer_create_info,
	const VmaAllocationCreateInfo& alloc_create_info,
	const VmaAllocator& allocator)
	: allocator(&allocator), size(buffer_create_info.size) {
	vmaCreateBuffer(
		allocator,
		reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
		&alloc_create_info,
		reinterpret_cast<VkBuffer*>(&buffer),
		&alloc,
		nullptr
	);
};

void Buffer::write(const void* data) {
	void *buffer_data;
	vmaMapMemory(*allocator, alloc, &buffer_data);
	std::memcpy(buffer_data, data, size);
	vmaUnmapMemory(*allocator, alloc);
}

void Buffer::staged_write(
	const void* data,
	const vk::CommandBuffer& command_buffer,
	const vk::Queue& queue) {
	//Create staging buffer
	const vk::BufferCreateInfo staging_buffer_create_info(
		{},
		size,
		vk::BufferUsageFlagBits::eTransferSrc
	);
	VmaAllocationCreateInfo staging_alloc_create_info {};
	staging_alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	auto staging_buffer = Buffer(
		staging_buffer_create_info,
		staging_alloc_create_info,
		*allocator
	);
	staging_buffer.write(data);
	//Transfer
	command_buffer.begin(vk::CommandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	));
	command_buffer.copyBuffer(
		staging_buffer.buffer,
		buffer,
		vk::BufferCopy(0, 0, size)
	);
	command_buffer.end();
	//TODO: Use fence
	queue.submit(vk::SubmitInfo({}, {}, command_buffer));
	queue.waitIdle();
	staging_buffer.destroy();
}

void Buffer::destroy() {
	vmaDestroyBuffer(*allocator, buffer, alloc);
}
