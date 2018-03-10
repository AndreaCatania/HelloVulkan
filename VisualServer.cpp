#include "VisualServer.h"

#include <iostream>
#include <fstream>

#define INITIAL_WINDOW_WIDTH 800
#define INITIAL_WINDOW_HEIGHT 600

void print(string c){
	cout << c << endl;
}

void print(const char * c){
	cout << c << endl;
}

bool readFile(const string &filename, vector<char> &r_out){
	// Read the file from the bottom in binary
	ifstream file(filename, ios::ate | ios::binary);

	if(!file.is_open()){
		return false;
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	r_out.resize(fileSize);

	file.seekg(0);
	file.read(r_out.data(), fileSize);
	file.close();
	return true;
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
	window(nullptr),
	instance(VK_NULL_HANDLE),
	debugCallback(VK_NULL_HANDLE),
	surface(VK_NULL_HANDLE),
	physicalDevice(VK_NULL_HANDLE),
	device(VK_NULL_HANDLE),
	graphicsQueue(VK_NULL_HANDLE),
	presentationQueue(VK_NULL_HANDLE),
	swapchain(VK_NULL_HANDLE),
	vertShaderModule(VK_NULL_HANDLE),
	fragShaderModule(VK_NULL_HANDLE),
	renderPass(VK_NULL_HANDLE),
	pipelineLayout(VK_NULL_HANDLE),
	graphicsPipeline(VK_NULL_HANDLE),
	vertexBuffer(VK_NULL_HANDLE),
	vertexBufferMemory(VK_NULL_HANDLE),
	commandPool(VK_NULL_HANDLE),
	imageAvailableSemaphore(VK_NULL_HANDLE),
	renderFinishedSemaphore(VK_NULL_HANDLE)
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

bool VulkanServer::create(GLFWwindow* p_window){

	window = p_window;

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

	lockupDeviceQueue();

	if( !createSwapchain() )
		return false;

	if( !createVertexBuffer() )
		return false;

	if( !createCommandPool() )
		return false;

	if( !allocateCommandBuffers() )
		return false;

	beginCommandBuffers();

	if( !createSemaphores() )
		return false;

	return true;
}

void VulkanServer::destroy(){

	waitIdle();

	destroySemaphores();
	destroyCommandPool();
	destroyVertexBuffer();
	destroySwapchain();
	destroyLogicalDevice();
	destroyDebugCallback();
	destroySurface();
	destroyInstance();
	window = nullptr;
}

void VulkanServer::waitIdle(){
	// assert that the device has finished all before cleanup
	if(device!=VK_NULL_HANDLE)
		vkDeviceWaitIdle(device);
}

#define TIMEOUT_NANOSEC 3.6e+12 // 1 hour

void VulkanServer::draw(){

	// wait the presentQueue is idle to avoid validation layer to memory leak when it's not syn with GPU
	// This is very bad approach and should be used semaphores
	vkQueueWaitIdle(presentationQueue);

	// Acquire the next image
	uint32_t imageIndex;
	VkResult acquireRes = vkAcquireNextImageKHR(device, swapchain, TIMEOUT_NANOSEC, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if(VK_ERROR_OUT_OF_DATE_KHR==acquireRes || VK_SUBOPTIMAL_KHR==acquireRes){
		// Vulkan tell me that the surface is no more compatible, so is mandatory recreate the swap chain
		recreateSwapchain();
		return;
	}

	// Submit draw commands
	VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if( VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) ){
		print("[ERROR not handled] Error during queue submission");
		return;
	}

	// Present
	VkPresentInfoKHR presInfo = {};
	presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presInfo.waitSemaphoreCount = 1;
	presInfo.pWaitSemaphores = signalSemaphores;
	presInfo.swapchainCount = 1;
	presInfo.pSwapchains = &swapchain;
	presInfo.pImageIndices = &imageIndex;

	VkResult presentRes = vkQueuePresentKHR(presentationQueue, &presInfo);
	if(VK_ERROR_OUT_OF_DATE_KHR==presentRes || VK_SUBOPTIMAL_KHR==presentRes){
		// Vulkan tell me that the surface is no more compatible, so is mandatory recreate the swap chain
		recreateSwapchain();
		return;
	}
}

void VulkanServer::add_vertices(const vector<Vertex>& p_vertices){

	void *data;
	VkDeviceSize offset = 0;
	size_t size = sizeof(Vertex) * p_vertices.size();
	VkResult res = vkMapMemory(device, vertexBufferMemory, offset, size, 0, &data);
	if(res != VK_SUCCESS){
		print("[ERROR - unhandled] - failed to map memory during vertices adding");
		return;
	}

	memcpy(data, p_vertices.data(), size);

	vkUnmapMemory(device, vertexBufferMemory);
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
	instance = VK_NULL_HANDLE;
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
	debugCallback = VK_NULL_HANDLE;
	print("[INFO] Debug callback destroyed");
}

bool VulkanServer::createSurface(){
	VkResult res = glfwCreateWindowSurface(instance, window, nullptr, &surface);
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
	surface = VK_NULL_HANDLE;
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
	device = VK_NULL_HANDLE;
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

VkSurfaceFormatKHR VulkanServer::chooseSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats){
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
		if(f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ){
			return f;
		}
	}

	// Choose the best from the available formats
	return p_formats[0]; // TODO create an algorithm to pick the best format
}

VkPresentModeKHR VulkanServer::choosePresentMode(const vector<VkPresentModeKHR> &p_modes){

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

VkExtent2D VulkanServer::chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities){
	// When the extent is set to max this mean that we can set any value
	// In this case we can set any value
	// So I set the size of WIDTH and HEIGHT used for initialize Window
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	VkExtent2D actualExtent = {(uint32_t)width, (uint32_t)height};

	actualExtent.width = max(capabilities.minImageExtent.width, min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height = max(capabilities.minImageExtent.height, min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

bool VulkanServer::createSwapchain(){

	if( !createRawSwapchain() )
		return false;

	lockupSwapchainImages();

	if( !createSwapchainImageViews() )
		return false;

	if( !createRenderPass() )
		return false;

	if( !createGraphicsPipelines() )
		return false;

	if( !createFramebuffers() )
		return false;

	return true;
}

void VulkanServer::destroySwapchain(){

	destroyFramebuffers();
	destroyGraphicsPipelines();
	destroyRenderPass();
	destroySwapchainImageViews();
	destroyRawSwapchain();
}

bool VulkanServer::createRawSwapchain(){

	SwapChainSupportDetails chainDetails = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR format = chooseSurfaceFormat(chainDetails.formats);
	VkPresentModeKHR pMode = choosePresentMode(chainDetails.presentModes);
	VkExtent2D extent2D = chooseExtent(chainDetails.capabilities);

	uint32_t imageCount = chainDetails.capabilities.minImageCount;
	// Check it we can use triple buffer
	// The mode MAILBOX is not required by vulkan to implement triple buffer
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
	uint32_t queueFamilyIndices[] = {(uint32_t)queueIndices.graphicsFamilyIndex, (uint32_t)queueIndices.presentationFamilyIndex};
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

	swapchainImageFormat = format.format;
	swapchainExtent = extent2D;

	print("[INFO] Created swap chain");
	return true;
}

void VulkanServer::destroyRawSwapchain(){
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

	print("[INFO] swapchain images lockup success");
}

bool VulkanServer::createSwapchainImageViews(){

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

void VulkanServer::destroySwapchainImageViews(){
	for(int i = swapchainImages.size() - 1; i>=0; --i){

		if( swapchainImageViews[i] == VK_NULL_HANDLE )
			continue;

		vkDestroyImageView(device, swapchainImageViews[i], nullptr );
	}
	swapchainImageViews.clear();
	print("[INFO] Destroyed image views");
}

bool VulkanServer::createRenderPass(){

	// Description of attachment
	VkAttachmentDescription colorAttachmentDesc = {};
	colorAttachmentDesc.format = swapchainImageFormat;
	colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Reference to attachment description
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0; // ID of description
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// The subpass describe how an attachment should be treated during execution
	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;

	// The dependency is something that lead the subpass order, is like a "barrier"
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &colorAttachmentDesc;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDesc;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkResult res = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
	if(VK_SUCCESS != res){
		print("[ERROR] Render pass creation fail");
		return false;
	}

	print("[INFO] Render pass created");
	return true;
}

void VulkanServer::destroyRenderPass(){
	if(VK_NULL_HANDLE==renderPass)
		return;
	vkDestroyRenderPass(device, renderPass, nullptr);
	renderPass = VK_NULL_HANDLE;
}

#define SHADER_VERTEX_PATH "./shaders/bin/vert.spv"
#define SHADER_FRAGMENT_PATH "shaders/bin/frag.spv"

bool VulkanServer::createGraphicsPipelines(){

/// Load shaders
	vector<char> vertexShaderBytecode;
	vector<char> fragmentShaderBytecode;

	if(!readFile(SHADER_VERTEX_PATH, vertexShaderBytecode)){
		print(string("[ERROR] Failed to load shader bytecode: ") + string(SHADER_VERTEX_PATH));
		return false;
	}

	if(!readFile(SHADER_FRAGMENT_PATH, fragmentShaderBytecode)){
		print(string("[ERROR] Failed to load shader bytecode: ") + string(SHADER_FRAGMENT_PATH));
		return false;
	}

	print(string("[INFO] vertex file byte loaded: ") + to_string(vertexShaderBytecode.size()));
	print(string("[INFO] fragment file byte loaded: ") + to_string(fragmentShaderBytecode.size()));

	vertShaderModule = createShaderModule(vertexShaderBytecode);
	fragShaderModule = createShaderModule(fragmentShaderBytecode);

	if(vertShaderModule == VK_NULL_HANDLE){
		print("[ERROR] Failed to create vertex shader module");
		return false;
	}

	if(fragShaderModule == VK_NULL_HANDLE){
		print("[ERROR] Failed to create fragment shader module");
		return false;
	}

	vector<VkPipelineShaderStageCreateInfo> shaderStages;
	{
		VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
		vertStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageCreateInfo.module = vertShaderModule;
		vertStageCreateInfo.pName = "main";
		shaderStages.push_back(vertStageCreateInfo);

		VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
		fragStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageCreateInfo.module = fragShaderModule;
		fragStageCreateInfo.pName = "main";
		shaderStages.push_back(fragStageCreateInfo);
	}

/// Vertex inputs
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription vertexInputBindingDescription = Vertex::getBindingDescription();

	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;

	array<VkVertexInputAttributeDescription, 2> vertexInputAttributesDescription = Vertex::getAttributesDescription();

	vertexInputCreateInfo.vertexAttributeDescriptionCount = vertexInputAttributesDescription.size();
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertexInputAttributesDescription.data();

/// Input Assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

/// Viewport
	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	VkViewport viewport = {};
	viewport.x = .0;
	viewport.y = .0;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = .0;
	viewport.maxDepth = 1.;

	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;

	// The scissor indicate the part of screen that we want crop, (it's not a transformation nor scaling)
	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchainExtent;

	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

/// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

/// Multisampling
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

/// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
	colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.pAttachments = &colorBlendAttachment;

/// Pipeline layout (used to specify uniform variables)
	{
		VkPipelineLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 0;

		if(VK_SUCCESS != vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout)){
			print("[ERROR] Failed to create pipeline layout");
			return false;
		}
	}

/// Create pipeline

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	pipelineCreateInfo.pViewportState = &viewportCreateInfo;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;

	// Specifies which subpass of render pass use this pipeline
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	VkResult res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if(res != VK_SUCCESS){
		print("[ERROR] Pipeline creation failed");
		return false;
	}

	print("[INFO] Pipeline created");
	return true;
}

void VulkanServer::destroyGraphicsPipelines(){

	if(graphicsPipeline != VK_NULL_HANDLE){
		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		graphicsPipeline = VK_NULL_HANDLE;
		print("[INFO] pipeline destroyed");
	}

	if(pipelineLayout != VK_NULL_HANDLE){
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
		print("[INFO] pipeline layout destroyed ");
	}

	destroyShaderModule(vertShaderModule);
	destroyShaderModule(fragShaderModule);
	vertShaderModule = VK_NULL_HANDLE;
	fragShaderModule = VK_NULL_HANDLE;

	print("[INFO] shader modules destroyed");
}

VkShaderModule VulkanServer::createShaderModule(vector<char> &shaderBytecode){

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	if(shaderBytecode.size()){
		createInfo.codeSize = shaderBytecode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderBytecode.data());
	}else{
		createInfo.codeSize = 0;
		createInfo.pCode = nullptr;
	}

	VkShaderModule shaderModule;
	if( vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) == VK_SUCCESS){
		print("[INFO] shader module created");
		return shaderModule;
	}else{
		return VK_NULL_HANDLE;
	}
}

void VulkanServer::destroyShaderModule(VkShaderModule &shaderModule){
	if(shaderModule==VK_NULL_HANDLE)
		return;

	vkDestroyShaderModule(device, shaderModule, nullptr);
	shaderModule = VK_NULL_HANDLE;
	print("[INFO] shader module destroyed");
}

bool VulkanServer::createFramebuffers(){

	swapchainFramebuffers.resize(swapchainImageViews.size());

	bool error = false;
	for(int i = swapchainFramebuffers.size() - 1; 0<=i; --i ){

		VkFramebufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		bufferCreateInfo.renderPass = renderPass;
		bufferCreateInfo.attachmentCount = 1;
		bufferCreateInfo.pAttachments = &swapchainImageViews[i];
		bufferCreateInfo.width = swapchainExtent.width;
		bufferCreateInfo.height = swapchainExtent.height;
		bufferCreateInfo.layers = 1;

		VkResult res = vkCreateFramebuffer(device, &bufferCreateInfo, nullptr, &swapchainFramebuffers[i]);
		if( res != VK_SUCCESS ){
			swapchainFramebuffers[i] = VK_NULL_HANDLE;
			error = true;
		}
	}

	print("[INFO] Framebuffers created");
	return true;
}

void VulkanServer::destroyFramebuffers(){

	for(int i = swapchainFramebuffers.size() - 1; 0<=i; --i ){

		if( swapchainFramebuffers[i] == VK_NULL_HANDLE )
			continue;

		vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
	}

	swapchainFramebuffers.clear();

}

bool VulkanServer::createVertexBuffer(){

	// Create buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(Vertex) * 1000; // Can store up to 1000 vertices
	bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult res = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &vertexBuffer);
	if(VK_SUCCESS!=res){
		print("[ERROR] Vertex buffer creation error");
		return false;
	}

	// Allocate memory of buffer
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, vertexBuffer, &memoryRequirements);

	// This flag: VK_MEMORY_PROPERTY_HOST_COHERENT_BIT is necessary to be sure that the written data are always available without any assertion
	int32_t memTypeId = chooseMemoryType( memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	if(0>memTypeId){
		print("[ERROR] No suitable memory found for vertex buffer");
		return false;
	}

	VkMemoryAllocateInfo memoryAllocationInfo = {};
	memoryAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocationInfo.allocationSize = memoryRequirements.size;
	memoryAllocationInfo.memoryTypeIndex = memTypeId;

	res = vkAllocateMemory(device, &memoryAllocationInfo, nullptr, &vertexBufferMemory);
	if( res!=VK_SUCCESS ){
		print("[ERROR] Vertex buffer memory allocation failed");
		return false;
	}

	// Bind buffer to memory
	vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, /*offset*/0);

	print("[INFO] Vertex buffer created");
	return true;
}

void VulkanServer::destroyVertexBuffer(){

	if(vertexBuffer==VK_NULL_HANDLE)
		return;
	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vertexBuffer = VK_NULL_HANDLE;

	if(vertexBufferMemory==VK_NULL_HANDLE)
		return;
	vkFreeMemory(device, vertexBufferMemory, nullptr);
	vertexBufferMemory = VK_NULL_HANDLE;
}

int32_t VulkanServer::chooseMemoryType(uint32_t p_typeBits, VkMemoryPropertyFlags p_propertyFlags){

	VkPhysicalDeviceMemoryProperties memoryProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

	for(int i = memoryProps.memoryTypeCount - 1; 0<=i; --i){
		// Checks if the current type of memory is suitable for this buffer
		if(p_typeBits & (1<<i) ){
			if((memoryProps.memoryTypes[i].propertyFlags & p_propertyFlags) == p_propertyFlags){
				return i;
			}
		}
	}

	return -1;
}

bool VulkanServer::createCommandPool(){
	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;

	VkResult res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
	if(res!=VK_SUCCESS){
		print("[ERROR] Command pool creation failed");
		return false;
	}

	print("[INFO] Command pool craeted");
	return true;
}

void VulkanServer::destroyCommandPool(){
	if(commandPool == VK_NULL_HANDLE)
		return;

	vkDestroyCommandPool(device, commandPool, nullptr);
	commandPool = VK_NULL_HANDLE;
	print("[INFO] Command pool destroyed");

	// Since the command buffers are destroyed by this function here I want clear it
	commandBuffers.clear();
}

bool VulkanServer::allocateCommandBuffers(){
	// Doesn't require destructions (it's performed automatically during the destruction of command pool)

	commandBuffers.resize(swapchainImages.size());

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t) commandBuffers.size();

	if(VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data())){
		print("[ERROR] Command buffer allocation fail");
		return false;
	}
	print("[INFO] command buffers allocated");
	return true;
}

void VulkanServer::beginCommandBuffers(){
	for(int i = commandBuffers.size() - 1; 0<=i; --i){
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		// Begin command buffer
		vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
		renderPassBeginInfo.renderArea.offset = {0,0};
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		renderPassBeginInfo.clearValueCount = 1;
		VkClearValue clearColor = {0.,0.,0.,1.};
		renderPassBeginInfo.pClearValues = &clearColor;

		// Begin render pass
		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind graphics pipeline
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind vertex data
		VkBuffer vertexBuffers[] = {vertexBuffer};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
		// Set draw call
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		if(VK_SUCCESS != vkEndCommandBuffer(commandBuffers[i])){
			print("[ERROR - not handled] end render pass failed");
		}
	}
	print("[INFO] Command buffers initializated");
}

bool VulkanServer::createSemaphores(){

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult imgRes = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore);
	VkResult rendRes = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore);

	if(imgRes!=VK_SUCCESS || rendRes!=VK_SUCCESS){
		print("[ERROR] Semaphore creation failed");
		return false;
	}

	print("[INFO] semaphores created");
	return true;
}

void VulkanServer::destroySemaphores(){

	if(imageAvailableSemaphore != VK_NULL_HANDLE){
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
	}

	if(renderFinishedSemaphore != VK_NULL_HANDLE){
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
	}

	print("[INFO] Semaphores destroyed");
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

void VulkanServer::recreateSwapchain(){
	waitIdle();

	// Recreate swapchain
	destroySwapchain();
	createSwapchain();
	beginCommandBuffers();
}

VisualServer::VisualServer()
	: window(nullptr) {
}

VisualServer::~VisualServer(){

}

bool VisualServer::init(){

	createWindow();
	return vulkanServer.create(window);
}

void VisualServer::terminate(){
	vulkanServer.destroy();
	freeWindow();
}

bool VisualServer::can_step(){
	return !glfwWindowShouldClose(window);
}

void VisualServer::step(){
	glfwPollEvents();
	vulkanServer.draw();
}

void VisualServer::add_vertices(const vector<Vertex> &p_vertices){
	vulkanServer.add_vertices(p_vertices);
}

void VisualServer::createWindow(){

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	// Windows resize
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Hello Vulkan", nullptr, nullptr);
}

void VisualServer::freeWindow(){
	glfwDestroyWindow(window);
	glfwTerminate();
}
