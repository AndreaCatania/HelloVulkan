#pragma once

#include "servers/visual_server.h"

#include "render_target.h"
#include "thirdparty/vulkan/vulkan.h"
#include <vector>

class VulkanVisualServer : public VisualServer {

	mutable RID_owner<RenderTarget> render_target_owner;

	VkInstance vulkan_instance;
	std::vector<const char *> layers;
	std::vector<const char *> device_extensions;
	VkDebugReportCallbackEXT debug_callback_handle;

	VkPhysicalDevice physical_device;
	VkDeviceSize physical_device_min_uniform_buffer_offset_alignment;

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

	bool select_physical_device();

	/// Returns the index in the array with best device,
	/// depending on device_type
	/// The r_device_props will contains the properties of device
	int filter_physical_devices(
			const std::vector<VkPhysicalDevice> &p_devices,
			VkSurfaceKHR p_surface,
			VkPhysicalDeviceType p_device_type);

	/** CREATE LOGICAL DEVICE */

	bool create_logical_device();

	/** MISCELLANEOUS */

	struct PhysicalDeviceSwapChainDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> present_modes;
	};

	static void get_physical_device_swap_chain_details(
			VkPhysicalDevice p_device,
			VkSurfaceKHR p_surface,
			PhysicalDeviceSwapChainDetails *r_details);

	struct QueueFamilyIndices {
		int graphicsFamilyIndex = -1;
		int presentationFamilyIndex = -1;

		bool isComplete() {

			if (graphicsFamilyIndex == -1)
				return false;

			if (presentationFamilyIndex == -1)
				return false;

			return true;
		}
	};

	static QueueFamilyIndices find_queue_families(
			VkPhysicalDevice p_device,
			VkSurfaceKHR p_surface);

	static bool check_extensions_support(
			const std::vector<const char *> &p_requiredExtensions,
			std::vector<VkExtensionProperties> &p_availableExtensions,
			bool verbose = true);

	static bool check_validation_layers_support(
			const std::vector<const char *> &p_layers);

	bool check_instance_extensions_support(
			const std::vector<const char *> &p_requiredExtensions);
};
