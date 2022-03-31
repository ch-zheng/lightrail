#include <vulkan/vulkan.h>
#include "util.h"

void flush_command_buffer(VkDevice* device, VkCommandBuffer* command_buffer, VkQueue* const queue,
	VkSubmitInfo* submit_info) {
	vkEndCommandBuffer(*command_buffer);
	VkFence fence;
	VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, NULL, 0};
	vkCreateFence(*device, &fence_info, NULL, &fence);
	vkQueueSubmit(*queue, 1, submit_info, fence);
	vkWaitForFences(*device, 1, &fence, false, 1000000000); //FIXME: Reasonable timeout
	vkDestroyFence(*device, fence, NULL);
}