#pragma once
#include <vulkan/vulkan.h>

struct Allocation {
	VkDeviceMemory memory;
	unsigned count;
	VkDeviceSize* offsets;
};

enum BlockType {
	VALID,
	INVALID,
};
struct MemoryBlock {
	size_t start_byte;
	size_t length_in_bytes;
	enum BlockType block_type;
	struct MemoryBlock* next_block;
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

void staged_buffer_write_to_image(
	//Vulkan objects
	VkPhysicalDevice* const physical_device,
	VkDevice* const device,
	VkCommandBuffer* const command_buffer,
	VkQueue* const queue,
	//Parameters
	const unsigned count,
	VkImage* const dst_images,
	const void** const data,
	const VkDeviceSize* sizes,
	VkFormat* formats,
	VkImageLayout* old_layouts,
	VkImageLayout* new_layouts,
	uint32_t width,
	uint32_t height);

void free_allocation(VkDevice, struct Allocation);
