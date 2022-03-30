#pragma once
#include "vulkan/vulkan.h"

#include "alloc.h"
#include "scene.h"

#define DEFAULT_VERTEX_BUFFER_SIZE 1000000
#define DEFAULT_INDEX_BUFFER_SIZE 25000000

struct Vertex {
	float pos[3]; //Position
	float tex[2]; //Texture coordinates
	float normal[3]; //Normal vector
};

struct VertexIndexBuffer {
    size_t vertex_size;
    VkBuffer vertex_buffer;
    struct Allocation vertex_alloc;
    size_t vertex_count;
    struct MemoryBlock* vertex_blocks;

    size_t index_size;
    VkBuffer index_buffer;
    struct Allocation index_alloc;
    size_t index_count;
    struct MemoryBlock* index_blocks;
};

VkResult create_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, struct VertexIndexBuffer* buff,
    size_t num_vertices, size_t num_indices);

void resize_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, VkQueue* queue, VkCommandBuffer* command_buffer,
    struct VertexIndexBuffer* const buff);

void write_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device,
    VkCommandBuffer* command_buffer, VkQueue* queue, size_t vertex_count, struct Vertex* const vertices,
    size_t index_count, uint32_t* const indices, struct VertexIndexBuffer* const buff, struct MemoryBlock* vertex_block, struct MemoryBlock* index_block);

void cleanup_vertex_index_buffer(VkDevice device, VkPhysicalDevice physical_device, struct VertexIndexBuffer* buff);