#include "alloc.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Note: Do not put buffers & images in the same allocation
VkResult create_allocation(
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
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
	vkGetPhysicalDeviceMemoryProperties(*physical_device, &mem_props);
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
	VkResult result = vkAllocateMemory(*device, &alloc_info, NULL, &alloc->memory);
	return result;
}

VkResult create_buffers(
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
	const unsigned count,
	const VkBufferCreateInfo* const create_infos,
	const VkMemoryPropertyFlags props,
	VkBuffer* const buffers,
	struct Allocation* const alloc) {
	VkResult result;
	//Create buffers
	VkMemoryRequirements* const reqs = malloc(count * sizeof(VkMemoryRequirements));
	for (unsigned i = 0; i < count; ++i) {
		result = vkCreateBuffer(*device, create_infos + i, NULL, buffers + i);
		if (result) {
			free(reqs);
			return result;
		}
		vkGetBufferMemoryRequirements(*device, buffers[i], reqs + i);
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
	result = vkBindBufferMemory2(*device, count, bind_infos);
	free(bind_infos);
	return result;
}

VkResult create_images(
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
	const unsigned count,
	const VkImageCreateInfo* const create_infos,
	const VkMemoryPropertyFlags props,
	VkImage* const images,
	struct Allocation* const alloc) {
	VkResult result;
	//Create images
	VkMemoryRequirements* const reqs = malloc(count * sizeof(VkMemoryRequirements));
	for (unsigned i = 0; i < count; ++i) {
		result = vkCreateImage(*device, create_infos + i, NULL, images + i);
		if (result) {
			free(reqs);
			return result;
		}
		vkGetImageMemoryRequirements(*device, images[i], reqs + i);
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
	result = vkBindImageMemory2(*device, count, bind_infos);
	free(bind_infos);
	return result;
}

/*! \brief Write data to buffers using an intermediate staging buffer.
 *
 * This function is generally used to write to buffers in device-local memory.
*/
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
	const VkDeviceSize* dest_offsets) {
	VkDeviceSize total_size = 0;
	VkDeviceSize* const offsets = malloc(count * sizeof(VkDeviceSize));
	for (unsigned i = 0; i < count; ++i) {
		offsets[i] = total_size;
		total_size += sizes[i];
	}
	//Create staging buffer
	const VkBufferCreateInfo staging_buffer_info = {
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
		total_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		0, NULL
	};
	VkBuffer staging_buffer;
	struct Allocation staging_alloc;
	create_buffers(
		physical_device, device,
		1, &staging_buffer_info,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
		| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&staging_buffer,
		&staging_alloc
	);
	//Write to staging buffer
	void* buffer_data;
	for (unsigned i = 0; i < count; ++i) {
		vkMapMemory(*device, staging_alloc.memory, offsets[i], sizes[i], 0, &buffer_data);
		memcpy(buffer_data, data[i], sizes[i]);
		vkUnmapMemory(*device, staging_alloc.memory);
	}
	//Execute transfer
	const VkCommandBufferBeginInfo begin_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(*command_buffer, &begin_info);
	for (unsigned i = 0; i < count; ++i) {
		// const VkBufferCopy region = {offsets[i], 0, sizes[i]};
		VkBufferCopy r = {
			.srcOffset = offsets[i],
			.dstOffset = dest_offsets[i],
			.size = sizes[i],
		};
		vkCmdCopyBuffer(*command_buffer, staging_buffer, dst_buffers[i], 1, &r);
	}
	vkEndCommandBuffer(*command_buffer);
	const VkSubmitInfo submit_info = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL,
		0, NULL,
		0,
		1, command_buffer,
		0, NULL
	};
	VkFence fence;
	VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0};
	vkCreateFence(*device, &fence_info, NULL, &fence);
	vkQueueSubmit(*queue, 1, &submit_info, fence);
	vkWaitForFences(*device, 1, &fence, false, 1000000000); //FIXME: Reasonable timeout
	//Cleanup
	free(offsets);
	vkDestroyFence(*device, fence, NULL);
	vkDestroyBuffer(*device, staging_buffer, NULL);
	free_allocation(*device, staging_alloc);
}

void free_allocation(VkDevice device, struct Allocation alloc) {
	vkFreeMemory(device, alloc.memory, NULL);
	free(alloc.offsets);
}

