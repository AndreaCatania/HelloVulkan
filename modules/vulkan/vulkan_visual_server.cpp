#include "vulkan_visual_server.h"

#include "core/error_macros.h"
#include "core/print_string.h"
#include "core/string.h"
#include "servers/window_server.h"

VulkanVisualServer *VulkanVisualServer::singleton = nullptr;

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

	singleton = this;
	// TODO Please initialize all parameters here
}

void VulkanVisualServer::init() {

	CRASH_COND(!create_vulkan_instance());
	CRASH_COND(!initialize_debug_callback());

	// Create a surface just to get some information
	RID initialization_window = WindowServer::get_singleton()->window_create(
			"InitializationWindow",
			1,
			1);

	WindowServer::get_singleton()->window_set_vulkan_instance(
			initialization_window,
			vulkan_instance);

	VkSurfaceKHR initialization_surface =
			WindowServer::get_singleton()->window_get_vulkan_surface(
					initialization_window);

	CRASH_COND(!select_physical_device(initialization_surface));

	queue_families = filter_queue_families(
			physical_device,
			initialization_surface);

	WindowServer::get_singleton()->window_free(initialization_window);
}

void VulkanVisualServer::terminate() {
	free_debug_callback();
	free_vulkan_instance();
}

RID VulkanVisualServer::create_render_target(RID p_window) {

	RenderTarget *rt = new RenderTarget();

	WindowServer::get_singleton()->window_set_vulkan_instance(
			p_window,
			vulkan_instance);

	rt->init(p_window);

	return render_target_owner.make_rid(rt);
}

void VulkanVisualServer::destroy_render_target(RID p_render_target) {

	RenderTarget *rt = render_target_owner.get(p_render_target);
	ERR_FAIL_COND(!rt);

	WindowServer::get_singleton()->window_set_vulkan_instance(
			rt->get_window(),
			VK_NULL_HANDLE);

	render_target_owner.release(p_render_target);
	delete rt;
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

void VulkanVisualServer::free_vulkan_instance() {
	if (vulkan_instance == VK_NULL_HANDLE)
		return;
	vkDestroyInstance(vulkan_instance, nullptr);
	vulkan_instance = VK_NULL_HANDLE;
	print_verbose("Vulkan instance destroyed");
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

void VulkanVisualServer::free_debug_callback() {

	if (debug_callback_handle == VK_NULL_HANDLE)
		return;

	PFN_vkDestroyDebugReportCallbackEXT func =
			(PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
					vulkan_instance,
					"vkDestroyDebugReportCallbackEXT");

	ERR_FAIL_COND(!func);

	func(vulkan_instance, debug_callback_handle, nullptr);
	debug_callback_handle = VK_NULL_HANDLE;
	print_verbose("Debug callback destroyed");
}

bool VulkanVisualServer::select_physical_device(
		VkSurfaceKHR p_initialization_surface) {

	print_verbose("Select physical device");

	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(vulkan_instance, &device_count, nullptr);
	ERR_FAIL_COND_V(0 == device_count, false);

	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(
			vulkan_instance,
			&device_count,
			devices.data());

	int index = filter_physical_devices(
			devices,
			p_initialization_surface,
			VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

	if (index < 0) {

		WARN_PRINTS("No discrete GPU found, Fallback to integrated GPU.");

		index = filter_physical_devices(
				devices,
				p_initialization_surface,
				VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

		if (index < 0) {

			WARN_PRINTS("No integrated GPU found, Fallback to CPU.");

			index = filter_physical_devices(
					devices,
					p_initialization_surface,
					VK_PHYSICAL_DEVICE_TYPE_CPU);

			ERR_FAIL_COND_V(index < 0, false);
		}
	}

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

	print_verbose("Filter physical device");

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

		if (!filter_queue_families(p_devices[i], p_surface).is_complete())
			continue;

		if (!are_extensions_supported(
					device_extensions,
					available_extensions))
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

VulkanVisualServer::QueueFamilyIndices VulkanVisualServer::filter_queue_families(
		VkPhysicalDevice p_device,
		VkSurfaceKHR p_surface) {

	QueueFamilyIndices indices;

	uint32_t family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &family_count, nullptr);

	if (family_count <= 0)
		return indices;

	std::vector<VkQueueFamilyProperties> queue_props(family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(
			p_device,
			&family_count,
			queue_props.data());

	for (int i = queue_props.size() - 1; 0 <= i; --i) {

		if (queue_props[i].queueCount <= 0)
			continue;

		if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family_index = i;
		}

		VkBool32 supported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(
				p_device,
				i,
				p_surface,
				&supported);

		if (supported) {
			indices.presentation_family_index = i;
		}
	}

	return indices;
}

bool VulkanVisualServer::are_extensions_supported(
		const std::vector<const char *> &p_required_extensions,
		std::vector<VkExtensionProperties> &p_available_extensions) {

	bool missing = false;

	for (size_t i = 0; i < p_required_extensions.size(); ++i) {
		bool found = false;
		for (size_t j = 0; j < p_available_extensions.size(); ++j) {
			if (strcmp(
						p_required_extensions[i],
						p_available_extensions[j].extensionName) == 0) {
				found = true;
				break;
			}
		}

		print_verbose(
				std::string("\textension: ") +
				std::string(p_required_extensions[i]) +
				std::string(found ? " [Available]" : " [NOT AVAILABLE]"));

		if (!found) {
			missing = true;
			break;
		}
	}

	return !missing;
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

	return are_extensions_supported(
			p_requiredExtensions,
			availableExtensions);
}
