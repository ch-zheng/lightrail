#pragma once
#include <vulkan/vulkan.h>

struct Allocation {
	VkDeviceMemory memory;
	unsigned count;
	VkDeviceSize* offsets;
};

VkResult create_allocation(
	VkPhysicalDevice* const,
	VkDevice* const,
	const VkMemoryPropertyFlags uint32_t,
	const unsigned,
	const VkMemoryRequirements* const,
	struct Allocation* const
);

VkResult create_buffers(
	VkPhysicalDevice* const,
	VkDevice* const,
	const unsigned,
	const VkBufferCreateInfo* const,
	const VkMemoryPropertyFlags,
	VkBuffer* const,
	struct Allocation* const
);

VkResult create_images(
	VkPhysicalDevice* const,
	VkDevice* const,
	const unsigned,
	const VkImageCreateInfo* const,
	const VkMemoryPropertyFlags,
	VkImage* const,
	struct Allocation* const
);

void staged_buffer_write(
	//Vulkan objects
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
	VkCommandBuffer* const command_buffer,
	VkQueue* const queue,
	//Parameters
	const unsigned count,
	VkBuffer* const dst_buffers,
	const void** const data,
	const VkDeviceSize* sizes,
	const VkDeviceSize* dest_offsets);

void free_allocation(VkDevice, struct Allocation);
