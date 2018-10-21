#include "vulkan_visual_server.h"

#include "core/error_macros.h"
#include "core/print_string.h"
#include "core/string.h"
#include "servers/window_server.h"

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackFnc(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char *layerPrefix,
		const char *msg,
		void *userData) {

	print_error(std::string("[Vulkan VL] ") + msg);
	return VK_FALSE;
}

VulkanVisualServer::VulkanVisualServer() :
		VisualServer() {
}

void VulkanVisualServer::init() {

	CRASH_COND(!create_vulkan_instance());
	CRASH_COND(!initialize_debug_callback());
	CRASH_COND(!select_physical_device());
	CRASH_COND(!create_logical_device());
}

void VulkanVisualServer::terminate() {
}

RID VulkanVisualServer::create_render_target(RID p_window) {

	RenderTarget *rt = new RenderTarget();

	rt->init(p_window);

	return render_target_owner.make_rid(rt);
}

void VulkanVisualServer::destroy_render_target(RID p_render_target) {
}

bool VulkanVisualServer::is_validation_layer_enabled() const {
#ifdef DEBUG_ENABLED
	return true;
#else
	return false;
#endif
}

bool VulkanVisualServer::create_vulkan_instance() {
	print_verbose("Instancing vulkan");

	// Create instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "HelloVulkanEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	layers.clear();
	std::vector<const char *> requiredExtensions;

	if (is_validation_layer_enabled()) {
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
		if (!check_validation_layers_support(layers)) {
			return false;
		}

		// Validation layer extension to enable validation
		requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	WindowServer::get_singleton()->get_required_extensions(requiredExtensions);

	if (!check_instance_extensions_support(requiredExtensions)) {
		return false;
	}

	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkResult res = vkCreateInstance(&createInfo, nullptr, &vulkan_instance);

	ERR_FAIL_COND_V(VK_SUCCESS != res, false);
	print_verbose("Instancing Vulkan success");

	return true;
}

bool VulkanVisualServer::initialize_debug_callback() {
	if (!is_validation_layer_enabled())
		return true;

	print_verbose("Debug callback inizialization");

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallbackFnc;

	// Load the extension function to create the callback
	PFN_vkCreateDebugReportCallbackEXT func =
			(PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
					vulkan_instance,
					"vkCreateDebugReportCallbackEXT");

	ERR_FAIL_COND_V(!func, false);

	VkResult res = func(
			vulkan_instance,
			&createInfo,
			nullptr,
			&debug_callback_handle);
	ERR_FAIL_COND_V(res != VK_SUCCESS, false);

	print_verbose("Debug callback initialized");
	return true;
}

bool VulkanVisualServer::select_physical_device() {

	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(vulkan_instance, &device_count, nullptr);
	ERR_FAIL_COND_V(0 == device_count, false);

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(
			vulkan_instance,
			&device_count,
			devices.data());

	// Create a test surface to get some information and destroy it
	RID test_window = WindowServer::get_singleton()->create_window(
			vulkan_instance,
			"TestWindow",
			1,
			1);

	VkSurfaceKHR test_surface =
			WindowServer::get_singleton()->get_vulkan_surface(
					test_window);

	print_verbose("Filter devices");

	int index = filter_physical_devices(
			devices,
			test_surface,
			VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

	if (index < 0) {

		WARN_PRINTS("No discrete GPU found, Fallback to integrated GPU.");

		index = filter_physical_devices(
				devices,
				test_surface,
				VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

		if (index < 0) {

			WARN_PRINTS("No integrated GPU found, Fallback to CPU.");

			index = filter_physical_devices(
					devices,
					test_surface,
					VK_PHYSICAL_DEVICE_TYPE_CPU);

			ERR_FAIL_COND_V(index < 0, false);
		}
	}

	WindowServer::get_singleton()->free_window(test_window);

	physical_device = devices[index];

	VkPhysicalDeviceProperties device_props;
	vkGetPhysicalDeviceProperties(physical_device, &device_props);

	physical_device_min_uniform_buffer_offset_alignment =
			device_props.limits.minUniformBufferOffsetAlignment;

	print_verbose("Physical Device selected");
	return true;
}

int VulkanVisualServer::filter_physical_devices(
		const std::vector<VkPhysicalDevice> &p_devices,
		VkSurfaceKHR p_surface,
		VkPhysicalDeviceType p_device_type) {

	for (int i = p_devices.size() - 1; 0 <= i; --i) {

		// Check here the device
		VkPhysicalDeviceProperties device_props;
		VkPhysicalDeviceFeatures device_features;
		uint32_t extensionsCount;

		vkGetPhysicalDeviceProperties(p_devices[i], &device_props);
		vkGetPhysicalDeviceFeatures(p_devices[i], &device_features);

		vkEnumerateDeviceExtensionProperties(
				p_devices[i],
				nullptr,
				&extensionsCount,
				nullptr);

		std::vector<VkExtensionProperties> available_extensions(
				extensionsCount);

		vkEnumerateDeviceExtensionProperties(
				p_devices[i],
				nullptr,
				&extensionsCount,
				available_extensions.data());

		if (device_props.deviceType != p_device_type)
			continue;

		if (!device_features.geometryShader || !device_features.samplerAnisotropy)
			continue;

		if (!find_queue_families(p_devices[i], p_surface).isComplete())
			continue;

		if (!check_extensions_support(
					device_extensions,
					available_extensions,
					false))
			continue;

		// Check if swap chain is supported by this device
		PhysicalDeviceSwapChainDetails details;
		get_physical_device_swap_chain_details(
				p_devices[i],
				p_surface,
				&details);

		if (details.formats.empty() || details.present_modes.empty())
			continue;

		return i;
	}
	return -1;
}

bool VulkanVisualServer::create_logical_device() {
}

void VulkanVisualServer::get_physical_device_swap_chain_details(
		VkPhysicalDevice p_device,
		VkSurfaceKHR p_surface,
		PhysicalDeviceSwapChainDetails *r_details) {

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			p_device,
			p_surface,
			&r_details->capabilities);

	uint32_t formatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(
			p_device,
			p_surface,
			&formatsCount,
			nullptr);

	if (formatsCount > 0) {
		r_details->formats.resize(formatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
				p_device,
				p_surface,
				&formatsCount,
				r_details->formats.data());
	} else {
		r_details->formats.clear();
	}

	uint32_t modesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			p_device,
			p_surface,
			&modesCount,
			nullptr);

	if (modesCount > 0) {
		r_details->present_modes.resize(modesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
				p_device,
				p_surface,
				&modesCount,
				r_details->present_modes.data());
	} else {
		r_details->present_modes.clear();
	}
}

VulkanVisualServer::QueueFamilyIndices VulkanVisualServer::find_queue_families(
		VkPhysicalDevice p_device,
		VkSurfaceKHR p_surface) {

	QueueFamilyIndices indices;

	uint32_t queueCounts = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCounts, nullptr);

	if (queueCounts <= 0)
		return indices;

	std::vector<VkQueueFamilyProperties> queueProperties(queueCounts);
	vkGetPhysicalDeviceQueueFamilyProperties(
			p_device,
			&queueCounts,
			queueProperties.data());

	for (int i = queueProperties.size() - 1; 0 <= i; --i) {
		if (
				queueProperties[i].queueCount > 0 &&
				queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

			indices.graphicsFamilyIndex = i;
		}

		VkBool32 supported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(
				p_device,
				i,
				p_surface,
				&supported);

		if (queueProperties[i].queueCount > 0 && supported) {
			indices.presentationFamilyIndex = i;
		}
	}

	return indices;
}

bool VulkanVisualServer::check_extensions_support(
		const std::vector<const char *> &p_requiredExtensions,
		std::vector<VkExtensionProperties> &p_availableExtensions,
		bool verbose) {

	bool missing = false;
	for (size_t i = 0; i < p_requiredExtensions.size(); ++i) {
		bool found = false;
		for (size_t j = 0; j < p_availableExtensions.size(); ++j) {
			if (strcmp(p_requiredExtensions[i],
						p_availableExtensions[j].extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (verbose)
			print_verbose(
					std::string("\textension: ") +
					std::string(p_requiredExtensions[i]) +
					std::string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if (found)
			missing = true;
	}
	return missing;
}

bool VulkanVisualServer::check_validation_layers_support(
		const std::vector<const char *> &p_layers) {

	uint32_t availableLayersCount = 0;
	std::vector<VkLayerProperties> availableLayers;
	vkEnumerateInstanceLayerProperties(
			&availableLayersCount,
			nullptr);

	availableLayers.resize(availableLayersCount);

	vkEnumerateInstanceLayerProperties(
			&availableLayersCount,
			availableLayers.data());

	print_verbose("Checking if required validation layers are available");
	bool missing = false;
	for (int i = p_layers.size() - 1; i >= 0; --i) {
		bool found = false;
		for (size_t j = 0; j < availableLayersCount; ++j) {
			if (strcmp(p_layers[i], availableLayers[j].layerName) == 0) {
				found = true;
				break;
			}
		}

		print_verbose(
				std::string("	layer: ") +
				std::string(p_layers[i]) +
				std::string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if (found)
			missing = true;
	}
	return missing;
}

bool VulkanVisualServer::check_instance_extensions_support(
		const std::vector<const char *> &p_requiredExtensions) {

	uint32_t availableExtensionsCount = 0;
	std::vector<VkExtensionProperties> availableExtensions;
	vkEnumerateInstanceExtensionProperties(
			nullptr,
			&availableExtensionsCount,
			nullptr);

	availableExtensions.resize(availableExtensionsCount);
	vkEnumerateInstanceExtensionProperties(
			nullptr,
			&availableExtensionsCount,
			availableExtensions.data());

	print_verbose("Checking if required extensions are available");

	return check_extensions_support(
			p_requiredExtensions,
			availableExtensions);
}
