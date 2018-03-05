#include "VisualServer.h"

#include <iostream>

#define WIDTH 800
#define HEIGHT 600

void print(string c){
	cout << c << endl;
}

void print(const char * c){
	cout << c << endl;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackFnc(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objType,
	uint64_t obj,
	size_t location,
	int32_t code,
	const char* layerPrefix,
	const char* msg,
	void* userData) {

	print( string("[VL ] ") + string(msg) );

	return VK_FALSE;
}

VulkanServer::VulkanServer() :
	windowData(nullptr),
	instance(VK_NULL_HANDLE),
	debugCallback(VK_NULL_HANDLE),
	surface(VK_NULL_HANDLE),
	physicalDevice(VK_NULL_HANDLE),
	device(VK_NULL_HANDLE),
	graphicsQueue(VK_NULL_HANDLE),
	presentationQueue(VK_NULL_HANDLE),
	swapchain(VK_NULL_HANDLE)
{

	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

bool VulkanServer::enableValidationLayer(){
#ifdef NDEBUG
	return false;
#else
	return true;
#endif
}

bool VulkanServer::create(const WindowData* p_windowData){
	windowData = p_windowData;

	if( !createInstance() )
		return false;

	if( !createDebugCallback() )
		return false;

	if( !createSurface() )
		return false;

	if( !pickPhysicalDevice() )
		return false;

	if( !createLogicalDevice() )
		return false;

	if( !createSwapChain() )
		return false;

	if( !createImageView() )
		return false;

	return true;
}

void VulkanServer::destroy(){
	destroyImageView();
	destroySwapChain();
	destroyLogicalDevice();
	destroyDebugCallback();
	destroySurface();
	destroyInstance();
	windowData = nullptr;
}

bool VulkanServer::createInstance(){

	print("Instancing Vulkan");

	// Create instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.pEngineName = "HelloVulkanEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	layers.clear();
	vector<const char*> requiredExtensions;

	if(enableValidationLayer()){
		layers.push_back( "VK_LAYER_LUNARG_standard_validation" );
		if(!checkValidationLayersSupport(layers)){
			return false;
		}

		// Validation layer extension to enable validation
		requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	{ // Get GLFW required extensions
		uint32_t glfwExtensionsCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

		requiredExtensions.insert(requiredExtensions.end(), &glfwExtensions[0], &glfwExtensions[glfwExtensionsCount]);
	}

	if(!checkInstanceExtensionsSupport(requiredExtensions)){
		return false;
	}

	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.data();
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);

	if(VK_SUCCESS==res){
		print("Instancing Vulkan success");
		return true;
	}else{
		std::cout << "[ERROR] Instancing error: " << res << std::endl;
		return false;
	}
}

void VulkanServer::destroyInstance(){
	if(instance==VK_NULL_HANDLE)
		return;
	vkDestroyInstance(instance, nullptr);
	print("[INFO] Vulkan instance destroyed");
}

bool VulkanServer::createDebugCallback(){
	if(!enableValidationLayer())
		return true;

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallbackFnc;

	// Load the extension function to create the callback
	PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if(!func){
		print("[Error] Can't load function to create debug callback");
		return false;
	}

	VkResult res = func(instance, &createInfo, nullptr, &debugCallback);
	if(res == VK_SUCCESS){
		print("[INFO] Debug callback loaded");
		return true;
	}else{
		print("[ERROR] debug callback not created");
		return false;
	}
}

void VulkanServer::destroyDebugCallback(){
	if(debugCallback==VK_NULL_HANDLE)
		return;

	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if(!func){
		print("[ERROR] Destroy functin not loaded");
		return;
	}

	func(instance, debugCallback, nullptr);
	print("[INFO] Debug callback destroyed");
}

bool VulkanServer::createSurface(){
	VkResult res = glfwCreateWindowSurface(instance, windowData->window, nullptr, &surface);
	if(res == VK_SUCCESS){
		print("[INFO] surface created");
		return true;
	}else{
		print("[ERROR] surface not created");
		return false;
	}
}

void VulkanServer::destroySurface(){
	if(surface==VK_NULL_HANDLE)
		return;
	vkDestroySurfaceKHR(instance, surface, nullptr);
}

bool VulkanServer::pickPhysicalDevice(){

	uint32_t availableDevicesCount = 0;
	vkEnumeratePhysicalDevices(instance, &availableDevicesCount, nullptr);

	if(0==availableDevicesCount){
		print("[Error] No devices that supports Vulkan");
		return false;
	}
	vector<VkPhysicalDevice> availableDevices(availableDevicesCount);
	vkEnumeratePhysicalDevices(instance, &availableDevicesCount, availableDevices.data());

	int id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	if(id<0){
		id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
		if(id<0){
			id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_CPU);
			if(id<0){
				print("[Error] No suitable device");
				return false;
			}
		}
	}

	physicalDevice = availableDevices[id];
	print("[INFO] Physical Device selected");
	return true;
}

bool VulkanServer::createLogicalDevice(){

	float priority = 1.f;

	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);
	vector<VkDeviceQueueCreateInfo> queueCreateInfoArray;

	VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {};
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &priority;

	queueCreateInfoArray.push_back(graphicsQueueCreateInfo);

	if(queueIndices.graphicsFamilyIndex != queueIndices.presentationFamilyIndex){
		// Create dedicated presentation queue
		VkDeviceQueueCreateInfo presentationQueueCreateInfo = {};
		presentationQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		presentationQueueCreateInfo.queueFamilyIndex = queueIndices.presentationFamilyIndex;
		presentationQueueCreateInfo.queueCount = 1;
		presentationQueueCreateInfo.pQueuePriorities = &priority;
		queueCreateInfoArray.push_back(presentationQueueCreateInfo);
	}

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfos = {};
	deviceCreateInfos.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfos.queueCreateInfoCount = queueCreateInfoArray.size();
	deviceCreateInfos.pQueueCreateInfos = queueCreateInfoArray.data();
	deviceCreateInfos.pEnabledFeatures = &physicalDeviceFeatures;
	deviceCreateInfos.enabledExtensionCount = deviceExtensions.size();
	deviceCreateInfos.ppEnabledExtensionNames = deviceExtensions.data();

	if(enableValidationLayer()){

		deviceCreateInfos.enabledLayerCount = layers.size();
		deviceCreateInfos.ppEnabledLayerNames = layers.data();
	}else{
		deviceCreateInfos.enabledLayerCount = 0;
	}

	VkResult res = vkCreateDevice(physicalDevice, &deviceCreateInfos, nullptr, &device);
	if(res==VK_SUCCESS){
		print("[INFO] Local device created");
		return true;
	}else{
		print("[ERROR] Logical device creation failed");
		return false;
	}
}

void VulkanServer::destroyLogicalDevice(){
	if(device==VK_NULL_HANDLE)
		return;
	vkDestroyDevice(device, nullptr);
	print("[INFO] Local device destroyed");
}

void VulkanServer::lockupDeviceQueue(){

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	vkGetDeviceQueue(device, indices.graphicsFamilyIndex, 0, &graphicsQueue);

	if(indices.graphicsFamilyIndex!=indices.presentationFamilyIndex){
		// Lockup dedicated presentation queue
		vkGetDeviceQueue(device, indices.presentationFamilyIndex, 0, &presentationQueue);
	}else{
		presentationQueue = graphicsQueue;
	}

	print("[INFO] Device queue lockup success");
}

VulkanServer::QueueFamilyIndices VulkanServer::findQueueFamilies(VkPhysicalDevice p_device){

	QueueFamilyIndices indices;

	uint32_t queueCounts = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCounts, nullptr);

	if(queueCounts<=0)
		return indices;

	vector<VkQueueFamilyProperties> queueProperties(queueCounts);
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCounts,  queueProperties.data());

	for(int i = queueProperties.size() - 1; 0<=i; --i){
		if( queueProperties[i].queueCount > 0 && queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ){
			indices.graphicsFamilyIndex = i;
		}

		VkBool32 supported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &supported);
		if( queueProperties[i].queueCount > 0 && supported ){
			indices.presentationFamilyIndex = i;
		}
	}

	return indices;
}

VulkanServer::SwapChainSupportDetails VulkanServer::querySwapChainSupport(VkPhysicalDevice p_device){
	SwapChainSupportDetails chainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface, &chainDetails.capabilities );

	uint32_t formatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatsCount, nullptr);

	if(formatsCount>0){
		chainDetails.formats.resize(formatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatsCount, chainDetails.formats.data());
	}

	uint32_t modesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &modesCount, nullptr);

	if(modesCount>0){
		chainDetails.presentModes.resize(modesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &modesCount, chainDetails.presentModes.data());
	}

	return chainDetails;
}

VkSurfaceFormatKHR VulkanServer::chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats){
	// If the surface has not a preferred format
	// That is the best case.
	// It will return just a format with format field set to VK_FORMAT_UNDEFINED
	if(p_formats.size() == 1 && p_formats[0].format == VK_FORMAT_UNDEFINED){
		VkSurfaceFormatKHR sFormat;
		sFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		sFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return sFormat;
	}

	// Choose the best format to use
	for(VkSurfaceFormatKHR f : p_formats){
		if(f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
			return f;
		}
	}

	// Choose the best from the available formats
	return p_formats[0]; // TODO create an algorithm to pick the best format
}

VkPresentModeKHR VulkanServer::chooseSwapPresentMode(const vector<VkPresentModeKHR> &p_modes){

	// This is guaranteed to be supported, but doesn't works so good
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& pm : p_modes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			// This mode allow me to use triple buffer
			return pm;
		} else if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = pm;
		}
	}

	return bestMode;
}

VkExtent2D VulkanServer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities){
	// When the extent is set to max this mean that we can set any value
	// In this case we can set any value
	// So I set the size of WIDTH and HEIGHT used for initialize Window
	VkExtent2D actualExtent = {WIDTH, HEIGHT};

	actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

bool VulkanServer::createSwapChain(){

	SwapChainSupportDetails chainDetails = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR format = chooseSwapSurfaceFormat(chainDetails.formats);
	VkPresentModeKHR pMode = chooseSwapPresentMode(chainDetails.presentModes);
	VkExtent2D extent2D = chooseSwapExtent(chainDetails.capabilities);

	uint32_t imageCount = chainDetails.capabilities.minImageCount;
	// Check it we can use triple buffer first
	if(pMode == VK_PRESENT_MODE_MAILBOX_KHR){
		++imageCount;
		if(chainDetails.capabilities.maxImageCount > 0) // 0 means no limits
			imageCount = min(imageCount, chainDetails.capabilities.maxImageCount);
	}

	VkSwapchainCreateInfoKHR chainCreate = {};
	chainCreate.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	chainCreate.surface = surface;
	chainCreate.minImageCount = imageCount;
	chainCreate.imageFormat = format.format;
	chainCreate.imageColorSpace = format.colorSpace;
	chainCreate.imageExtent = extent2D;
	chainCreate.imageArrayLayers = 1;
	chainCreate.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Define how to handle object ownership between queue families
	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[] = {queueIndices.graphicsFamilyIndex, queueIndices.presentationFamilyIndex};
	if(queueIndices.graphicsFamilyIndex!=queueIndices.presentationFamilyIndex){
		// Since the queue families are different I want to use concurrent mode so an object can be in multiples families
		// and I don't need to handle ownership myself
		chainCreate.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		chainCreate.queueFamilyIndexCount = 2; // Define how much concurrent families I have
		chainCreate.pQueueFamilyIndices = queueFamilyIndices; // set queue families
	}else{
		// I've only one family so I don't need concurrent mode
		chainCreate.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	chainCreate.preTransform = chainDetails.capabilities.currentTransform;
	chainCreate.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	chainCreate.presentMode = pMode;
	chainCreate.clipped = VK_TRUE;
	chainCreate.oldSwapchain = VK_NULL_HANDLE;

	VkResult res = vkCreateSwapchainKHR(device, &chainCreate, nullptr, &swapchain);
	if(res != VK_SUCCESS){
		print("[ERROR] Swap chain creation fail");
		return false;
	}

	lockupSwapchainImages();

	swapchainImageFormat = format.format;
	swapchainExtent = extent2D;

	print("[INFO] Created swap chain");
	return true;
}

void VulkanServer::destroySwapChain(){
	if(swapchain==VK_NULL_HANDLE)
		return;
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	swapchain = VK_NULL_HANDLE;
	print("[INFO] swapchain destroyed");
}

void VulkanServer::lockupSwapchainImages(){

	uint32_t imagesCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
	swapchainImages.resize(imagesCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, swapchainImages.data());

	print("[INFO] lockup images success");
}

bool VulkanServer::createImageView(){

	swapchainImageViews.resize(swapchainImages.size());

	bool error = false;

	for(int i = swapchainImageViews.size() - 1; i>=0; --i){
		VkImageViewCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = swapchainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = swapchainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkResult res = vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]);
		if(res != VK_SUCCESS){
			swapchainImageViews[i] = VK_NULL_HANDLE;
			error = true;
		}
	}

	if(error){
		print("[ERROR] Error during creation of Immage Views");
		return false;
	}else{
		print("[INFO] All image view created");
		return true;
	}
}

void VulkanServer::destroyImageView(){
	for(int i = swapchainImages.size() - 1; i>=0; --i){

		if( swapchainImageViews[i] == VK_NULL_HANDLE )
			continue;

		vkDestroyImageView(device, swapchainImageViews[i], nullptr );
	}
	print("[INFO] Destroyed image views");
}

bool VulkanServer::createGraphicsPipeline(){

}

void VulkanServer::destroyGraphicsPipeline(){

}

bool checkExtensionsSupport(const vector<const char*> &p_requiredExtensions, vector<VkExtensionProperties> availableExtensions, bool verbose = true){
	bool missing = false;
	for(size_t i = 0; i<p_requiredExtensions.size();++i){
		bool found = false;
		for(size_t j = 0; j<availableExtensions.size(); ++j){
			if( strcmp(p_requiredExtensions[i], availableExtensions[j].extensionName) == 0 ){
				found = true;
				break;
			}
		}

		if(verbose)
			print(string("	extension: ") + string(p_requiredExtensions[i]) + string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if(found)
			missing = true;
	}
	return missing;
}

bool VulkanServer::checkInstanceExtensionsSupport(const vector<const char*> &p_requiredExtensions){
	uint32_t availableExtensionsCount = 0;
	vector<VkExtensionProperties> availableExtensions;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr);
	availableExtensions.resize(availableExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions.data());

	print("Checking if required extensions are available");
	return checkExtensionsSupport(p_requiredExtensions, availableExtensions);
}

bool VulkanServer::checkValidationLayersSupport(const vector<const char*> &p_layers){

	uint32_t availableLayersCount = 0;
	vector<VkLayerProperties> availableLayers;
	vkEnumerateInstanceLayerProperties(&availableLayersCount, nullptr);
	availableLayers.resize(availableLayersCount);
	vkEnumerateInstanceLayerProperties(&availableLayersCount, availableLayers.data());

	print("Checking if required validation layers are available");
	bool missing = false;
	for(int i = p_layers.size() - 1; i>=0; --i){
		bool found = false;
		for(size_t j = 0; j<availableLayersCount; ++j){
			if( strcmp(p_layers[i], availableLayers[j].layerName) == 0 ){
				found = true;
				break;
			}
		}

		print(string("	layer: ") + string(p_layers[i]) + string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if(found)
			missing = true;
	}
	return missing;
}

int VulkanServer::autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_deviceType){

	print("[INFO] Select physical device");

	for(int i = p_devices.size()-1; 0<=i; --i){

		// Check here the device
		VkPhysicalDeviceProperties deviceProps;
		VkPhysicalDeviceFeatures deviceFeatures;
		uint32_t extensionsCount;

		vkGetPhysicalDeviceProperties(p_devices[i], &deviceProps);
		vkGetPhysicalDeviceFeatures(p_devices[i], &deviceFeatures);

		vkEnumerateDeviceExtensionProperties(p_devices[i], nullptr, &extensionsCount, nullptr);

		vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkEnumerateDeviceExtensionProperties(p_devices[i], nullptr, &extensionsCount, availableExtensions.data());

		if( deviceProps.deviceType != p_deviceType)
			continue;

		if( !deviceFeatures.geometryShader )
			continue;

		if( !findQueueFamilies(p_devices[i]).isComplete() )
			continue;

		if( !checkExtensionsSupport(deviceExtensions, availableExtensions, false) )
			continue;

		// Here we are sure that the extensions are available in that device, so now we can check swap chain
		SwapChainSupportDetails swapChainDetails = querySwapChainSupport(p_devices[i]);
		if(swapChainDetails.formats.empty() || swapChainDetails.presentModes.empty())
			continue;

		return i;
	}
	return -1;
}

VisualServer::VisualServer(){
}

VisualServer::~VisualServer(){

}

bool VisualServer::init(){

	createWindow();
	vulkanServer.create(&windowData);

	return true;
}

void VisualServer::terminate(){
	vulkanServer.destroy();
	freeWindow();
}

bool VisualServer::can_step(){
	return !glfwWindowShouldClose(windowData.window);
}

void VisualServer::step(){
	glfwPollEvents();
}

void VisualServer::createWindow(){
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	windowData.window = glfwCreateWindow(WIDTH, HEIGHT, "Hello Vulkan", nullptr, nullptr);
}

void VisualServer::freeWindow(){
	glfwDestroyWindow(windowData.window);
	glfwTerminate();
}
