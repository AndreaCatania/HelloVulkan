#include "render_target.h"

#include "vulkan_visual_server.h"

RenderTarget::RenderTarget() :
		ResourceData() {}

void RenderTarget::init(RID p_window) {
	window = p_window;

	create_logical_device();
	lockup_queues();
}

RID RenderTarget::get_window() {
	return window;
}

void RenderTarget::create_logical_device() {

	const VulkanVisualServer::QueueFamilyIndices &queue_families =
			VulkanVisualServer::get_singleton()->get_queue_families();

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfoArray;

	float priority = 1.f;

	// Information to create graphycs queue

	VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {};
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.queueFamilyIndex = queue_families.graphics_family_index;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &priority;

	queueCreateInfoArray.push_back(graphicsQueueCreateInfo);

	if (queue_families.graphics_family_index != queue_families.presentation_family_index) {

		// Create dedicated presentation queue in case the graphycs queue doesn't
		// support presentation

		VkDeviceQueueCreateInfo presentationQueueCreateInfo = {};
		presentationQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		presentationQueueCreateInfo.queueFamilyIndex = queue_families.presentation_family_index;
		presentationQueueCreateInfo.queueCount = 1;
		presentationQueueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfoArray.push_back(presentationQueueCreateInfo);
	}

	// Request physica device feature.
	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

	// Information to create logical device
	VkDeviceCreateInfo ldevice_create_infos = {};
	ldevice_create_infos.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	ldevice_create_infos.queueCreateInfoCount = queueCreateInfoArray.size();
	ldevice_create_infos.pQueueCreateInfos = queueCreateInfoArray.data();
	ldevice_create_infos.pEnabledFeatures = &physicalDeviceFeatures;

	ldevice_create_infos.enabledExtensionCount =
			VulkanVisualServer::get_singleton()->get_device_extensions().size();

	ldevice_create_infos.ppEnabledExtensionNames =
			VulkanVisualServer::get_singleton()->get_device_extensions().data();

	if (VulkanVisualServer::get_singleton()->is_validation_layer_enabled()) {

		ldevice_create_infos.enabledLayerCount =
				VulkanVisualServer::get_singleton()->get_layers().size();

		ldevice_create_infos.ppEnabledLayerNames =
				VulkanVisualServer::get_singleton()->get_layers().data();
	} else {
		ldevice_create_infos.enabledLayerCount = 0;
	}

	VkResult res = vkCreateDevice(
			VulkanVisualServer::get_singleton()->get_physical_device(),
			&ldevice_create_infos,
			nullptr,
			&logical_device);

	ERR_FAIL_COND(res != VK_SUCCESS);
}

void RenderTarget::lockup_queues() {

	const VulkanVisualServer::QueueFamilyIndices &queue_families =
			VulkanVisualServer::get_singleton()->get_queue_families();

	vkGetDeviceQueue(
			logical_device,
			queue_families.graphics_family_index,
			0,
			&graphics_queue);

	if (queue_families.graphics_family_index !=
			queue_families.presentation_family_index) {

		// Lockup dedicated presentation queue
		vkGetDeviceQueue(
				logical_device,
				queue_families.presentation_family_index,
				0,
				&presentation_queue);
	} else {
		presentation_queue = graphics_queue;
	}

	WARN_PRINT("Make sure to use a dedicated queue for presentation.");
}

void RenderTarget::create_command_pool() {

	const VulkanVisualServer::QueueFamilyIndices &queue_families =
			VulkanVisualServer::get_singleton()->get_queue_families();

	VkCommandPoolCreateInfo command_pool_create_info = {};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_families.graphics_family_index;
	command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkResult res = vkCreateCommandPool(
			VulkanVisualServer::get_singleton()->get_physical_device(),
			&command_pool_create_info,
			nullptr,
			&graphics_command_pool);

	ERR_FAIL_COND_V(VK_SUCCESS != res, false);
}
