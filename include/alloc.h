#pragma once
#include <vulkan/vulkan.h>

struct AllocBlock {
	VkDevice device;
	VkDeviceMemory memory;
	unsigned count;
	VkDeviceSize* offsets;
};

VkResult allocate_block(
	VkPhysicalDevice*,
	VkDevice*,
	const uint32_t,
	const unsigned,
	const VkMemoryRequirements* const,
	struct AllocBlock*
);

void free_block(struct AllocBlock);
