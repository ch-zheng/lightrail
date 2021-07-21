#include "render-utils.hpp"
#include <SDL2/SDL_surface.h>
#include <iostream>
using namespace lightrail;

BufferWrapper::BufferWrapper(
	const vk::BufferCreateInfo& buffer_create_info,
	const VmaAllocationCreateInfo& alloc_create_info,
	const VmaAllocator& allocator)
	: allocator(allocator), size(buffer_create_info.size) {
	vmaCreateBuffer(
		allocator,
		reinterpret_cast<const VkBufferCreateInfo*>(&buffer_create_info),
		&alloc_create_info,
		reinterpret_cast<VkBuffer*>(&buf),
		&alloc,
		nullptr
	);
};

void BufferWrapper::write(const void* data) {
	//TODO: Flushing
	void *buffer_data;
	vmaMapMemory(allocator, alloc, &buffer_data);
	std::memcpy(buffer_data, data, size);
	vmaUnmapMemory(allocator, alloc);
}

void BufferWrapper::staged_write(
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
	auto staging_buffer = BufferWrapper(
		staging_buffer_create_info,
		staging_alloc_create_info,
		allocator
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
	//TODO: Use fence
	queue.submit(vk::SubmitInfo({}, {}, command_buffer));
	queue.waitIdle();
	staging_buffer.destroy();
}

void BufferWrapper::destroy() {
	vmaDestroyBuffer(allocator, buf, alloc);
}

Texture::Texture(
	const char* filename,
	const vk::Device& device,
	const VmaAllocator& allocator,
	const vk::CommandBuffer& command_buffer,
	const vk::Queue& queue)
	: device(device), allocator(allocator) {
	constexpr vk::Format image_format = vk::Format::eR8G8B8A8Srgb;
	//SDL Surface creation
	auto tmp_surface = SDL_LoadBMP(filename);
	auto surface = SDL_ConvertSurfaceFormat(tmp_surface, SDL_PIXELFORMAT_RGBA32, 0);
	SDL_FreeSurface(tmp_surface);
	const vk::Extent3D image_extent(surface->w, surface->h, 1);
	//Image creation
	const vk::ImageCreateInfo image_create_info(
		{},
		vk::ImageType::e2D,
		image_format,
		image_extent,
		1,
		1,
		vk::SampleCountFlagBits::e1,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransferDst
		| vk::ImageUsageFlagBits::eSampled,
		vk::SharingMode::eExclusive
	);
	VmaAllocationCreateInfo alloc_create_info {};
	alloc_create_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaCreateImage(
		allocator, 
		reinterpret_cast<const VkImageCreateInfo*>(&image_create_info),
		&alloc_create_info,
		reinterpret_cast<VkImage*>(&image),
		&alloc,
		nullptr
	);
	//Staging buffer creation
	const vk::BufferCreateInfo staging_buffer_create_info(
		{},
		surface->w * surface->h * surface->format->BytesPerPixel,
		vk::BufferUsageFlagBits::eTransferSrc
	);
	VmaAllocationCreateInfo staging_alloc_create_info {};
	staging_alloc_create_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	auto staging_buffer = BufferWrapper(
		staging_buffer_create_info,
		staging_alloc_create_info,
		allocator
	);
	staging_buffer.write(surface->pixels);
	SDL_FreeSurface(surface);
	//Transfer
	const vk::ImageSubresourceRange range(
		vk::ImageAspectFlagBits::eColor,
		0, 1,
		0, 1
	);
	const vk::ImageMemoryBarrier transfer_barrier(
		vk::AccessFlagBits::eNoneKHR,
		vk::AccessFlagBits::eTransferWrite,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		image,
		range
	);
	//Record commands
	command_buffer.begin(vk::CommandBufferBeginInfo(
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit
	));
	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe,
		vk::PipelineStageFlagBits::eTransfer,
		{},
		{},
		{},
		vk::ImageMemoryBarrier(
			vk::AccessFlagBits::eNoneKHR,
			vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			range
		)
	);
	command_buffer.copyBufferToImage(
		staging_buffer.get_buf(),
		image,
		vk::ImageLayout::eTransferDstOptimal,
		vk::BufferImageCopy(
			0, 0, 0,
			vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
			vk::Offset3D(0, 0, 0),
			image_extent
		)
	);
	command_buffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eFragmentShader,
		{},
		{},
		{},
		vk::ImageMemoryBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eShaderRead,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			image,
			range
		)
	);
	command_buffer.end();
	auto fence = device.createFence({});
	queue.submit(vk::SubmitInfo({}, {}, command_buffer), fence);
	//Image view creation
	const vk::ImageViewCreateInfo image_view_create_info(
		{},
		image,
		vk::ImageViewType::e2D,
		image_format,
		vk::ComponentMapping(
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity
		),
		vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
	);
	image_view = device.createImageView(image_view_create_info);
	//Sampler creation
	const vk::SamplerCreateInfo sampler_create_info(
		{},
		vk::Filter::eNearest,
		vk::Filter::eNearest,
		vk::SamplerMipmapMode::eLinear,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		vk::SamplerAddressMode::eRepeat,
		0.0f,
		false,
		0.0f,
		false,
		vk::CompareOp::eAlways,
		0.0f,
		0.0f,
		vk::BorderColor::eIntOpaqueBlack,
		false
	);
	sampler = device.createSampler(sampler_create_info);
	//Cleanup
	const auto result = device.waitForFences(fence, false, 1000000000);
	device.destroyFence(fence);
	staging_buffer.destroy();
}

void Texture::destroy() {
	device.destroySampler(sampler);
	device.destroyImageView(image_view);
	vmaDestroyImage(allocator, image, alloc);
}
