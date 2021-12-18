#include "alloc.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

VkResult allocate_block(
	VkPhysicalDevice* physical_device,
	VkDevice* device,
	const uint32_t prop_flags,
	const unsigned req_count,
	const VkMemoryRequirements* const reqs,
	struct AllocBlock* alloc) {
	if (req_count < 1) return VK_ERROR_UNKNOWN;

	//Memory requirements
	VkDeviceSize alloc_size = 0;
	VkDeviceSize* offsets = malloc(req_count * sizeof(VkDeviceSize));
	uint32_t supported_mem_types = 0xFFFFFFFF;
	for (unsigned i = 0; i < req_count; ++i) {
		//Size & alignment
		const VkMemoryRequirements req = reqs[i];
		VkDeviceSize rem = alloc_size % req.alignment;
		if (rem) alloc_size += req.alignment - rem;
		offsets[i] = alloc_size;
		alloc_size += req.size;
		//Memory type
		supported_mem_types &= reqs[i].memoryTypeBits;
	}

	//Find valid memory type
	unsigned mem_type;
	bool success = false;
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(*physical_device, &mem_props);
	for (unsigned i = 0; i < mem_props.memoryTypeCount; ++i) {
		VkMemoryType type = mem_props.memoryTypes[i];
		bool supported = (supported_mem_types >> i) & 1;
		bool has_props = prop_flags & type.propertyFlags;
		if (supported && has_props) {
			mem_type = i;
			success = true;
			break;
		}
	}
	if (!success) {
		fprintf(stderr, "NO VALID MEMORY TYPE\n");
		return VK_ERROR_UNKNOWN;
	}

	//Allocation
	VkMemoryAllocateInfo alloc_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
		alloc_size,
		mem_type
	};
	*alloc = (struct AllocBlock) {
		.device = *device,
		.count = req_count,
		.offsets = offsets
	};
	VkResult result = vkAllocateMemory(*device, &alloc_info, NULL, &alloc->memory);
	return result;
}

void free_block(struct AllocBlock alloc) {
	vkFreeMemory(alloc.device, alloc.memory, NULL);
	free(alloc.offsets);
}
