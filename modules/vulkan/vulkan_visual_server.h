#pragma once

#include "servers/visual_server.h"

#include "render_target.h"
#include "thirdparty/vulkan/vulkan.h"
#include <vector>

// Physical device and Queue Family
//
// For each GPU or CPU
//		|
//		Physical Device [The available queue families depends on the device]
//			|
//			|- Queue Family 1 (Different capabilities respect other queue families)
//			|	|- Queue 1
//			|	|- Queue 2
//			|
//			|- Queue Family 2 (Different capabilities respect other queue families)
//			|	|- Queue 1
//			|
//			|- Queue Family 3 (Different capabilities respect other queue families)
//			|	|- Queue 1
//			|	|- Queue 2
//			|	|- Queue 3
//			.
//
//
// Physical Device
//		The Physical Device provide different feature to use depending on the
//		application that we are developing.
//
// Logical Device
//		This object instead contains only the required device feature.
//
//		So:
//			The Logical Device is the most important object because
//			it represents the Physical Hardware with all the loaded extensions,
//			features, queue, etc...
//			This mean that all operations must be executed on the Logical Device
//			that know exactly which part of Physical Device to call.
//
//		Operations:
//			Create images and Buffers, set pipeline, Load shaders,
//			record commands, etc...
//

class VulkanVisualServer : public VisualServer {

	struct QueueFamilyIndices {
		int graphics_family_index = -1;
		int presentation_family_index = -1;

		bool is_complete() {

			if (graphics_family_index == -1)
				return false;

			if (presentation_family_index == -1)
				return false;

			return true;
		}
	};

	struct PhysicalDeviceSwapChainDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	mutable RID_owner<RenderTarget> render_target_owner;

	VkInstance vulkan_instance;
	std::vector<const char *> layers;
	std::vector<const char *> device_extensions;
	VkDebugReportCallbackEXT debug_callback_handle;

	VkPhysicalDevice physical_device;
	VkDeviceSize physical_device_min_uniform_buffer_offset_alignment;
	QueueFamilyIndices queue_families;

	VkDevice logical_device;
	VkQueue graphics_queue;
	VkQueue presentation_queue;

public:
	VulkanVisualServer();

	virtual void init();
	virtual void terminate();

	virtual RID create_render_target(RID p_window);
	virtual void destroy_render_target(RID p_render_target);

private:
	bool is_validation_layer_enabled() const;

	/** VULKAN INSTANCE */

	bool create_vulkan_instance();
	bool initialize_debug_callback();

	/** PHYSICAL DEVICE */

	bool select_physical_device(VkSurfaceKHR p_initialization_surface);

	/// Returns the index in the array with best device,
	/// depending on device_type
	/// The r_device_props will contains the properties of device
	int filter_physical_devices(
			const std::vector<VkPhysicalDevice> &p_devices,
			VkSurfaceKHR p_surface,
			VkPhysicalDeviceType p_device_type);

	/** LOGICAL DEVICE */

	bool create_logical_device();

	void lockup_queues();

	/** MISCELLANEOUS */

	static void get_physical_device_swap_chain_details(
			VkPhysicalDevice p_device,
			VkSurfaceKHR p_surface,
			PhysicalDeviceSwapChainDetails *r_details);

	/// Real the queue families of device and returns
	/// the indices of queue family if they provide the features
	/// required by this software
	static QueueFamilyIndices filter_queue_families(
			VkPhysicalDevice p_device,
			VkSurfaceKHR p_surface);

	static bool are_extensions_supported(
			const std::vector<const char *> &p_requiredExtensions,
			std::vector<VkExtensionProperties> &p_availableExtensions);

	static bool check_validation_layers_support(
			const std::vector<const char *> &p_layers);

	bool check_instance_extensions_support(
			const std::vector<const char *> &p_requiredExtensions);
};
