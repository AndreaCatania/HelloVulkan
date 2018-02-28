#include "VisualServer.h"

#include <iostream>

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
	instance(VK_NULL_HANDLE),
	debugCallback(VK_NULL_HANDLE),
	physicalDevice(VK_NULL_HANDLE)
{

}

bool VulkanServer::create(){

	if( !createInstance() )
		return false;

	if( !createDebugCallback() )
		return false;

	if( !pickPhysicalDevice() )
		return false;

	if( !createLogicalDevice() )
		return false;

	return true;
}

void VulkanServer::destroy(){
	destroyLogicalDevice();
	destroyDebugCallback();
	destroyInstance();
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

	if(!checkExtensionsSupport(requiredExtensions)){
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
	print("[info] Vulkan instance destroyed");
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
		print("[Info] Debug callback loaded");
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
		print("[Error] Destroy functin not loaded");
		return;
	}

	func(instance, debugCallback, nullptr);
	print("[info] Debug callback destroyed");
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

	VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {};
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &priority;

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};

	VkDeviceCreateInfo deviceCreateInfos = {};
	deviceCreateInfos.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfos.queueCreateInfoCount = 1;
	deviceCreateInfos.pQueueCreateInfos = &graphicsQueueCreateInfo;
	deviceCreateInfos.pEnabledFeatures = &physicalDeviceFeatures;
	deviceCreateInfos.enabledExtensionCount = 0;

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

	for(int i = queueProperties.size()-1;0<=i;--i){
		if( queueProperties[i].queueCount > 0 && queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ){
			indices.graphicsFamilyIndex = i;
			continue;
		}
	}

	return indices;
}

bool VulkanServer::checkExtensionsSupport(const vector<const char*> &p_requiredExtensions){
	uint32_t availableExtensionsCount = 0;
	vector<VkExtensionProperties> availableExtensions;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr);
	availableExtensions.resize(availableExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions.data());

	print("Checking if required extensions are available");
	bool missing = false;
	for(int i = 0; i<p_requiredExtensions.size();++i){
		bool found = false;
		for(int j = 0; j<availableExtensionsCount; ++j){
			if( strcmp(p_requiredExtensions[i], availableExtensions[j].extensionName) == 0 ){
				found = true;
				break;
			}
		}

		print(string(found ? "[Available]" : "[NOT AVAILABLE]") + string(" extension: ") + string(p_requiredExtensions[i]) );
		if(found)
			missing = true;
	}
	return missing;
}

bool VulkanServer::checkValidationLayersSupport(const vector<const char*> &p_layers){

	uint32_t availableLayersCount = 0;
	vector<VkLayerProperties> availableLayers;
	vkEnumerateInstanceLayerProperties(&availableLayersCount, nullptr);
	availableLayers.resize(availableLayersCount);
	vkEnumerateInstanceLayerProperties(&availableLayersCount, availableLayers.data());

	print("Checking if required validation layers are available");
	bool missing = false;
	for(int i = p_layers.size()-1; i>=0; --i){
		bool found = false;
		for(int j = 0; j<availableLayersCount;++j){
			if( strcmp(p_layers[i], availableLayers[j].layerName) == 0 ){
				found = true;
				break;
			}
		}

		print(string(found ? "[Available]" : "[NOT AVAILABLE]") + string(" layer: ") + string(p_layers[i]) );
		if(found)
			missing = true;
	}
	return missing;
}

int VulkanServer::autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_deviceType){

	for(int i = p_devices.size()-1; 0<=i; --i){

		// Check here the device
		VkPhysicalDeviceProperties deviceProps;
		VkPhysicalDeviceFeatures deviceFeatures;

		vkGetPhysicalDeviceProperties(p_devices[i], &deviceProps);
		vkGetPhysicalDeviceFeatures(p_devices[i], &deviceFeatures);

		if( deviceProps.deviceType != p_deviceType)
			continue;

		if( !deviceFeatures.geometryShader )
			continue;

		if( !findQueueFamilies(p_devices[i]).isComplete() )
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
	vulkanServer.create();

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
	windowData.window = glfwCreateWindow(800, 600, "Hello Vulkan", nullptr, nullptr);
}

void VisualServer::freeWindow(){
	glfwDestroyWindow(windowData.window);
	glfwTerminate();
}
