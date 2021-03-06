#pragma once

#include "core/rid.h"
#include "thirdparty/vulkan/vulkan.h"
#include <vector>

class RenderTarget : public ResourceData {

	RID window;

	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue presentation_queue;

	VkCommandPool graphics_command_pool;

public:
	RenderTarget();

	void init(RID p_window);

	RID get_window();

private:
	void create_logical_device();
	void lockup_queues();

	void create_command_pool();
};
