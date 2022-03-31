#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "vertex_buffer.h"
#include "alloc.h"
#include "vulkan/vulkan_core.h"

VkResult create_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, struct VertexIndexBuffer *buff,
	size_t num_vertices, size_t num_indices) {
	buff->vertex_size = num_vertices * sizeof(struct Vertex);
	buff->index_size = num_indices * sizeof(uint32_t);
	buff->index_count = 0;
	buff->vertex_count = 0;

	// buff->vertex_blocks = NULL;
	// buff->index_blocks = NULL;

	const VkBufferCreateInfo vertex_buffer_infos[2] = {
		//Vertex buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			NULL, 
			0,
			buff->vertex_size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, 
			NULL
		},
		//Index buffer
		{
			VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0,
			buff->index_size,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_SHARING_MODE_EXCLUSIVE,
			0, NULL
		},
	};
	VkResult result = create_buffers(
		&physical_device,
		&device,
		1, &vertex_buffer_infos[0],
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &buff->vertex_buffer,
        &buff->vertex_alloc
	);
    if (result != VK_SUCCESS) {
        return result;
    }
	
    result = create_buffers(
		&physical_device,
		&device,
		1, &vertex_buffer_infos[1],
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &buff->index_buffer,
        &buff->index_alloc
	);
	return result;
}

void resize_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, VkQueue* queue, VkCommandBuffer* command_buffer, struct VertexIndexBuffer* buff) {
	struct VertexIndexBuffer new_buff;
	size_t new_vertex_size = buff->vertex_count;
	VkBufferCopy vertex_copy = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = new_vertex_size * sizeof(struct Vertex),
	};
	
	size_t new_index_size = buff->index_count;
	VkBufferCopy index_copy = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = new_index_size * sizeof(uint32_t),
	};
	
	VkBuffer new_vertex;
	VkBuffer new_index;

	struct Allocation new_vertex_alloc;
	struct Allocation new_index_alloc;

	new_buff.vertex_size = new_vertex_size;
	new_buff.index_size = new_index_size;
	new_buff.vertex_buffer = new_vertex;
	new_buff.index_buffer = new_index;
	new_buff.vertex_alloc = new_vertex_alloc;
	new_buff.index_alloc = new_index_alloc;
	new_buff.index_count = buff->index_count;
	new_buff.vertex_count = buff->vertex_count;
	new_buff.vertex_blocks = buff->vertex_blocks;
	new_buff.index_blocks = buff->index_blocks;

	create_vertex_index_buffer(device, physical_device, &new_buff, new_buff.vertex_size, new_buff.index_size);	
	cleanup_vertex_index_buffer(device, physical_device, buff);
	*buff = new_buff;
}


void write_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device,
    VkCommandBuffer* command_buffer, VkQueue* queue, size_t vertex_count, struct Vertex* const vertices,
    size_t index_count, uint32_t* const indices, struct VertexIndexBuffer* buff, struct MemoryBlock* vertex_block, struct MemoryBlock* index_block) {

	// VkDeviceSize offsets[] = { buff->vertex_count * sizeof(struct Vertex), buff->index_count * sizeof(uint32_t)};
	VkDeviceSize offsets[] = { 0, 0};
	if (buff->index_count + index_count > buff->index_size / sizeof(uint32_t)
		|| buff->vertex_count + vertex_count > buff->vertex_size / sizeof(struct Vertex)) {
		buff->index_count += index_count;
		buff->vertex_count += vertex_count;
		resize_vertex_index_buffer(device, physical_device, queue, command_buffer, buff);
	} else {
		
		// abort();
		// struct MemoryBlock* v_block = buff->vertex_blocks;
		// while(v_block) {
		// 	if (v_block->block_type == INVALID && v_block->length_in_bytes >= vertex_count * sizeof(struct Vertex)) {
		// 		offsets[0] = v_block->start_byte;
		// 		v_block->block_type = VALID;
		// 		break;
		// 	}
		// 	v_block = v_block->next_block;
		// }
		// struct MemoryBlock* i_block = buff->index_blocks;
		// while(i_block) {
		// 	if (i_block->length_in_bytes >= index_count * sizeof(uint32_t)) {
		// 		v_block->block_type = VALID;
		// 		offsets[1] = i_block->start_byte;
		// 		break;
		// 	}
		// 	i_block = i_block->next_block;
		// }

		// buff->index_count += index_count;
		// buff->vertex_count += vertex_count;
	}

	struct MemoryBlock* old_vertex_block = buff->vertex_blocks;
	VkDeviceSize old_start_byte = 0;
    VkDeviceSize old_length_in_bytes = 0;
	if (old_vertex_block) {
		old_start_byte = old_vertex_block->start_byte;
		old_length_in_bytes = old_vertex_block->length_in_bytes;
	}
	struct MemoryBlock* new_vertex_block = (struct MemoryBlock*) malloc(sizeof(struct MemoryBlock));
	new_vertex_block->start_byte = (old_start_byte + old_length_in_bytes);
	new_vertex_block->length_in_bytes = sizeof(struct Vertex) * vertex_count;
	new_vertex_block->block_type = VALID;
	new_vertex_block->next_block = old_vertex_block;
	buff->vertex_blocks = new_vertex_block;
	
	struct MemoryBlock* old_index_block = buff->index_blocks;
	old_start_byte = 0; old_length_in_bytes = 0;
	if (old_index_block) {
		old_start_byte = old_index_block->start_byte;
		old_length_in_bytes = old_index_block->length_in_bytes;
	}

	struct MemoryBlock* new_index_block = (struct MemoryBlock*) malloc(sizeof(struct MemoryBlock));
	new_index_block->start_byte = old_start_byte+old_length_in_bytes;
	new_index_block->length_in_bytes = sizeof(uint32_t) * index_count;
	// new_index_block->block_type = VALID;
	new_index_block->next_block = old_index_block;
	buff->index_blocks = new_index_block;
	
	offsets[0] = new_vertex_block->start_byte;
	offsets[1] = new_index_block->start_byte;

	VkDeviceSize vertex_size = sizeof(struct Vertex) * vertex_count;
	VkDeviceSize index_size = sizeof(uint32_t) * index_count;
	
	VkBuffer dst_buffers[] = { buff->vertex_buffer, buff->index_buffer };
	void* data[] = { (void*) vertices, (void*) indices };
	VkDeviceSize sizes[] = { vertex_size, index_size };


	// make sure these aren't used after free
	*vertex_block = *new_vertex_block;
	*index_block = *new_index_block;

	staged_buffer_write(&physical_device, &device, command_buffer, queue, 2, dst_buffers, data, sizes, offsets);
}

void cleanup_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, struct VertexIndexBuffer* buff) {
	free_allocation(device, buff->vertex_alloc);
	free_allocation(device, buff->index_alloc);
	vkDestroyBuffer(device, buff->vertex_buffer, NULL);
	vkDestroyBuffer(device, buff->index_buffer, NULL);
	
	// struct MemoryBlock* b =  buff->vertex_blocks;
	// while (b) {
	// 	struct MemoryBlock* a = b;
	// 	b = a->next_block;
	// 	free(a);
	// }
	// b =  buff->index_blocks;
	// while (b) {
	// 	struct MemoryBlock* a = b;
	// 	b = a->next_block;
	// 	free(a);
	// }
}
