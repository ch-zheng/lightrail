#pragma once
#include <vulkan/vulkan.h>

struct Allocation {
	VkDeviceMemory memory;
	unsigned count;
	VkDeviceSize* offsets;
};

VkResult create_allocation(
	const VkPhysicalDevice,
	const VkDevice,
	const VkMemoryPropertyFlags uint32_t,
	const unsigned,
	const VkMemoryRequirements* const,
	struct Allocation* const
);

VkResult create_buffers(
	const VkPhysicalDevice,
	const VkDevice,
	const unsigned,
	const VkBufferCreateInfo* const,
	const VkMemoryPropertyFlags,
	VkBuffer* const,
	struct Allocation* const
);

VkResult create_images(
	const VkPhysicalDevice,
	const VkDevice,
	const unsigned,
	const VkImageCreateInfo* const,
	const VkMemoryPropertyFlags,
	VkImage* const,
	struct Allocation* const
);

void free_allocation(VkDevice, struct Allocation);
