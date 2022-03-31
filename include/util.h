#pragma once

#include "stdbool.h"
#include "vulkan/vulkan.h"

void flush_command_buffer(VkDevice* device, VkCommandBuffer* command_buffer, VkQueue* const queue,
	VkSubmitInfo* submit_info);