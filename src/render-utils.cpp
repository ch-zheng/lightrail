#include "render-utils.hpp"
using namespace lightrail;

BufferWrapper::BufferWrapper(
	const vk::Device& device,
	const VmaAllocator& allocator,
	const vk::BufferCreateInfo& buffer_create_info,
	const VmaAllocationCreateInfo& alloc_create_info)
	: device(device), allocator(allocator), size(buffer_create_info.size) {
	vmaCreateBuffer(
		allocator,
		reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
		&alloc_create_info,
		reinterpret_cast<VkBuffer*>(&buf),
		&alloc,
		nullptr
	);
};

void BufferWrapper::write(const void *data) {
	//TODO: Flushing
	void *buffer_data;
	vmaMapMemory(allocator, alloc, &buffer_data);
	std::memcpy(buffer_data, data, size);
	vmaUnmapMemory(allocator, alloc);
}

void BufferWrapper::staged_write(
	const void *data,
	const vk::CommandBuffer& command_buffer,
	const vk::Queue& queue) {
	//Create staging buffer
	vk::BufferCreateInfo staging_buffer_create_info(
		{},
		size,
		vk::BufferUsageFlagBits::eTransferSrc
	);
	VmaAllocationCreateInfo staging_alloc_create_info {};
	staging_alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	auto staging_buffer = BufferWrapper(
		device, allocator,
		staging_buffer_create_info, staging_alloc_create_info
	);
	staging_buffer.write(data);
	//Transfer
	command_buffer.begin(vk::CommandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	));
	command_buffer.copyBuffer(
		staging_buffer.buf,
		buf,
		vk::BufferCopy(0, 0, size)
	);
	command_buffer.end();
	queue.submit(vk::SubmitInfo({}, {}, command_buffer));
	queue.waitIdle();
	staging_buffer.destroy();
}

void BufferWrapper::destroy() {
	vmaDestroyBuffer(allocator, buf, alloc);
}
