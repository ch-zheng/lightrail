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

void free_allocation(VkDevice, struct Allocation);
