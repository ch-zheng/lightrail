#include "alloc.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

//Note: Do not put buffers & images in the same allocation
VkResult create_allocation(
	const VkPhysicalDevice physical_device,
	const VkDevice device,
	const VkMemoryPropertyFlags props,
	const unsigned count,
	const VkMemoryRequirements* const reqs,
	struct Allocation* const alloc) {
	//Memory requirements
	VkDeviceSize alloc_size = 0;
	VkDeviceSize* offsets = malloc(count * sizeof(VkDeviceSize));
	uint32_t supported_mem_types = 0xFFFFFFFF;
	for (unsigned i = 0; i < count; ++i) {
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
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
	for (unsigned i = 0; i < mem_props.memoryTypeCount; ++i) {
		VkMemoryType type = mem_props.memoryTypes[i];
		bool supported = (supported_mem_types >> i) & 1;
		bool has_props = props & type.propertyFlags;
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
	*alloc = (struct Allocation) {
		.count = count,
		.offsets = offsets
	};
	VkMemoryAllocateInfo alloc_info = {
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL,
		alloc_size,
		mem_type
	};
	VkResult result = vkAllocateMemory(device, &alloc_info, NULL, &alloc->memory);
	return result;
}

VkResult create_buffers(
	const VkPhysicalDevice physical_device,
	const VkDevice device,
	const unsigned count,
	const VkBufferCreateInfo* const create_infos,
	const VkMemoryPropertyFlags props,
	VkBuffer* const buffers,
	struct Allocation* const alloc) {
	VkResult result;
	//Create buffers
	VkMemoryRequirements* const reqs = malloc(count * sizeof(VkMemoryRequirements));
	for (unsigned i = 0; i < count; ++i) {
		result = vkCreateBuffer(device, create_infos + i, NULL, buffers + i);
		if (result) {
			free(reqs);
			return result;
		}
		vkGetBufferMemoryRequirements(device, buffers[i], reqs + i);
	}
	//Allocate memory
	result = create_allocation(
		physical_device, device,
		props,
		count, reqs,
		alloc
	);
	free(reqs);
	if (result) return result;
	//Bind buffers to memory
	VkBindBufferMemoryInfo* const bind_infos = malloc(count * sizeof(VkBindBufferMemoryInfo));
	for (unsigned i = 0; i < count; ++i)
		bind_infos[i] = (VkBindBufferMemoryInfo) {
			VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO, NULL,
			buffers[i],
			alloc->memory,
			alloc->offsets[i]
		};
	result = vkBindBufferMemory2(device, count, bind_infos);
	free(bind_infos);
	return result;
}

VkResult create_images(
	const VkPhysicalDevice physical_device,
	const VkDevice device,
	const unsigned count,
	const VkImageCreateInfo* const create_infos,
	const VkMemoryPropertyFlags props,
	VkImage* const images,
	struct Allocation* const alloc) {
	VkResult result;
	//Create images
	VkMemoryRequirements* const reqs = malloc(count * sizeof(VkMemoryRequirements));
	for (unsigned i = 0; i < count; ++i) {
		result = vkCreateImage(device, create_infos + i, NULL, images + i);
		if (result) {
			free(reqs);
			return result;
		}
		vkGetImageMemoryRequirements(device, images[i], reqs + i);
	}
	//Allocate memory
	result = create_allocation(
		physical_device, device,
		props,
		count, reqs,
		alloc
	);
	free(reqs);
	if (result) return result;
	//Bind images to memory
	VkBindImageMemoryInfo* const bind_infos = malloc(count * sizeof(VkBindImageMemoryInfo));
	for (unsigned i = 0; i < count; ++i)
		bind_infos[i] = (struct VkBindImageMemoryInfo) {
			VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO, NULL,
			images[i],
			alloc->memory,
			alloc->offsets[i]
		};
	result = vkBindImageMemory2(device, count, bind_infos);
	free(bind_infos);
	return result;
}

void free_allocation(VkDevice device, struct Allocation alloc) {
	vkFreeMemory(device, alloc.memory, NULL);
	free(alloc.offsets);
}
