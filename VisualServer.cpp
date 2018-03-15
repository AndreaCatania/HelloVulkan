#include "VisualServer.h"

#include <iostream>
#include <fstream>

// Implementation of VMA
#define VMA_IMPLEMENTATION
#include "libs/vma/vk_mem_alloc.h"

#define INITIAL_WINDOW_WIDTH 800
#define INITIAL_WINDOW_HEIGHT 600

#define MAX_MESH_COUNT 5

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

Mesh::Mesh(){
	meshHandle = unique_ptr<MeshHandle>(new MeshHandle);
	meshHandle->mesh = this;
	transformation = glm::mat4(1.f);
}

Mesh::~Mesh(){

}

void Mesh::setTransform(const glm::mat4 &p_transformation){
	transformation = p_transformation;
	meshHandle->hasTransformationChange = true;
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
	depthImage(VK_NULL_HANDLE),
	depthImageMemory(VK_NULL_HANDLE),
	depthImageView(VK_NULL_HANDLE),
	swapchain(VK_NULL_HANDLE),
	vertShaderModule(VK_NULL_HANDLE),
	fragShaderModule(VK_NULL_HANDLE),
	renderPass(VK_NULL_HANDLE),
	cameraDescriptorSetLayout(VK_NULL_HANDLE),
	cameraDescriptorPool(VK_NULL_HANDLE),
	meshesDescriptorSetLayout(VK_NULL_HANDLE),
	meshesDescriptorPool(VK_NULL_HANDLE),
	pipelineLayout(VK_NULL_HANDLE),
	graphicsPipeline(VK_NULL_HANDLE),
	bufferMemoryDeviceAllocator(VK_NULL_HANDLE),
	bufferMemoryHostAllocator(VK_NULL_HANDLE),
	cameraUniformBuffer(VK_NULL_HANDLE),
	cameraUniformBufferAllocation(VK_NULL_HANDLE),
	graphicsCommandPool(VK_NULL_HANDLE),
	imageAvailableSemaphore(VK_NULL_HANDLE),
	renderFinishedSemaphore(VK_NULL_HANDLE),
	copyFinishFence(VK_NULL_HANDLE),
	reloadDrawCommandBuffer(true)
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

	if( !createDescriptorSetLayouts() )
		return false;

	if( !createSwapchain() )
		return false;

	if( !createBufferMemoryDeviceAllocator() )
		return false;

	if( !createBufferMemoryHostAllocator() )
		return false;

	if( !createUniformBuffers() )
		return false;

	if( !createUniformPools() )
		return false;

	if( !allocateAndConfigureDescriptorSet() )
		return false;

	if( !createCommandPool() )
		return false;

	if( !allocateCommandBuffers() )
		return false;

	if( !createSyncObjects() )
		return false;

	return true;
}

void VulkanServer::destroy(){

	waitIdle();

	removeAllMeshes();
	destroySyncObjects();
	destroyCommandPool();
	destroyUniformPools();
	destroyUniformBuffers();
	destroyBufferMemoryHostAllocator();
	destroyBufferMemoryDeviceAllocator();
	destroySwapchain();
	destroyDescriptorSetLayouts();
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

#define LONGTIMEOUT_NANOSEC 3.6e+12 // 1 hour

void VulkanServer::draw(){

	if(reloadDrawCommandBuffer){
		beginCommandBuffers();
		reloadDrawCommandBuffer = false;
	}

	processCopy();

	updateUniformBuffer();

	// Acquire the next image
	uint32_t imageIndex;
	VkResult acquireRes = vkAcquireNextImageKHR(device, swapchain, LONGTIMEOUT_NANOSEC, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if(VK_ERROR_OUT_OF_DATE_KHR==acquireRes || VK_SUBOPTIMAL_KHR==acquireRes){
		// Vulkan tell me that the surface is no more compatible, so is mandatory recreate the swap chain
		recreateSwapchain();
		return;
	}

	// This is used to be sure that the previous drawing has finished
	vkWaitForFences(device, 1, &drawFinishFences[imageIndex], VK_TRUE, LONGTIMEOUT_NANOSEC );
	vkResetFences(device, 1, &drawFinishFences[imageIndex]);

	// Submit draw commands
	VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	if( VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFinishFences[imageIndex]) ){
		print("[ERROR not handled] Error during queue submission");
		return;
	}

	// Present
	VkPresentInfoKHR presInfo = {};
	presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presInfo.waitSemaphoreCount = 1;
	presInfo.pWaitSemaphores = &renderFinishedSemaphore;
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

void VulkanServer::addMesh(const Mesh *p_mesh){

	if( meshUniformBufferData.count >= meshUniformBufferData.size){
		print("[ERROR] Max mesh limit reached, can't add more meshes");
		return;
	}

	if( !createBuffer(bufferMemoryDeviceAllocator,
					  p_mesh->verticesSizeInBytes(),
					  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_GPU_ONLY,
					  p_mesh->meshHandle->vertexBuffer,
					  p_mesh->meshHandle->vertexAllocation) ){

		print("[ERROR] Vertex buffer creation error, mesh not added");
		return;
	}

	if( !createBuffer(bufferMemoryDeviceAllocator,
					  p_mesh->indicesSizeInBytes(),
					  VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_GPU_ONLY,
					  p_mesh->meshHandle->indexBuffer,
					  p_mesh->meshHandle->indexAllocation) ){

		print("[ERROR] Index buffer creation error, mesh not added");
		return;
	}

	p_mesh->meshHandle->verticesSize = p_mesh->verticesSizeInBytes();
	p_mesh->meshHandle->verticesBufferOffset = 0;
	p_mesh->meshHandle->indicesSize = p_mesh->indicesSizeInBytes();
	p_mesh->meshHandle->indicesBufferOffset = 0;
	p_mesh->meshHandle->meshUniformBufferOffset = meshUniformBufferData.count++;
	p_mesh->meshHandle->hasTransformationChange = true;

	meshesCopyPending.push_back(p_mesh->meshHandle.get());
}

void VulkanServer::removeMesh_internal(MeshHandle *p_meshHandle){
	destroyBuffer(bufferMemoryDeviceAllocator, p_meshHandle->indexBuffer, p_meshHandle->indexAllocation);
	destroyBuffer(bufferMemoryDeviceAllocator, p_meshHandle->vertexBuffer, p_meshHandle->vertexAllocation);
}

void VulkanServer::processCopy(){
	VkResult fenceStatus = vkGetFenceStatus(device, copyFinishFence);
	if(fenceStatus!=VK_SUCCESS){
		return; // Copy is in progress
	}

	if(meshesCopyInProgress.size()>0){

		// Copy process end

		// Clear command buffer
		vkFreeCommandBuffers(device, graphicsCommandPool, 1,  &copyCommandBuffer);

		meshes.insert(meshes.end(), meshesCopyInProgress.begin(), meshesCopyInProgress.end());
		meshesCopyInProgress.clear();

		reloadDrawCommandBuffer = true;
	}

	// Check if there are meshes to copy in pending
	if(meshesCopyPending.size()>0){

		// Start new copy
		VkCommandBufferBeginInfo copyBeginInfo = {};
		copyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		copyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(copyCommandBuffer, &copyBeginInfo);
		for(int m = meshesCopyPending.size() -1; 0<=m; --m){

			vkCmdUpdateBuffer(copyCommandBuffer, meshesCopyPending[m]->vertexBuffer, 0, meshesCopyPending[m]->verticesSize, meshesCopyPending[m]->mesh->vertices.data());
			vkCmdUpdateBuffer(copyCommandBuffer, meshesCopyPending[m]->indexBuffer, 0, meshesCopyPending[m]->indicesSize, meshesCopyPending[m]->mesh->triangles.data());
		}

		if(VK_SUCCESS != vkEndCommandBuffer(copyCommandBuffer) ){
			print("[ERROR] Copy command buffer ending failed");
			return;
		}

		VkSubmitInfo copySubmitInfo = {};
		copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		copySubmitInfo.commandBufferCount = 1;
		copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

		if( VK_SUCCESS != vkResetFences(device, 1, &copyFinishFence) ){
			print("[ERROR] copy finish Fence reset error");
			return;
		}

		if(VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &copySubmitInfo, copyFinishFence) ){
			print("[ERROR] Copy command buffer submission failed");
			return;
		}

		meshesCopyInProgress.insert(meshesCopyInProgress.end(), meshesCopyPending.begin(), meshesCopyPending.end());
		meshesCopyPending.clear();
	}
}

void VulkanServer::updateUniformBuffer(){

	CameraUniformBufferObject sceneUBO = {};
	sceneUBO.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	sceneUBO.projection = glm::perspective(glm::radians(45.0f), swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f);

	void* data;
	vmaMapMemory(bufferMemoryHostAllocator, cameraUniformBufferAllocation, &data);
	memcpy(data, &sceneUBO, sizeof(CameraUniformBufferObject));
	vmaUnmapMemory(bufferMemoryHostAllocator, cameraUniformBufferAllocation);

	// Update mesh dynamic uniform buffers
	vmaMapMemory(bufferMemoryHostAllocator, meshUniformBufferData.meshUniformBufferAllocation, &data);
	MeshUniformBufferObject supportMeshUBO;
	for(int i = meshes.size() -1; 0<=i; --i){
		if( !meshes[i]->hasTransformationChange )
			continue;
		supportMeshUBO.model = meshes[i]->mesh->transformation;
		memcpy(data + meshes[i]->meshUniformBufferOffset * meshDynamicUniformBufferOffset, &supportMeshUBO, sizeof(MeshUniformBufferObject));
		meshes[i]->hasTransformationChange = false;
	}
	vmaUnmapMemory(bufferMemoryHostAllocator, meshUniformBufferData.meshUniformBufferAllocation);
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

	VkPhysicalDeviceProperties deviceProps;

	int id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, &deviceProps);
	if(id<0){
		id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &deviceProps);
		if(id<0){
			id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_CPU, &deviceProps);
			if(id<0){
				print("[Error] No suitable device");
				return false;
			}
		}
	}

	physicalDevice = availableDevices[id];

	physicalDeviceMinUniformBufferOffsetAlignment = deviceProps.limits.minUniformBufferOffsetAlignment;

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

	if( !createDepthTestResources() )
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
	destroyDepthTestResources();
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
		if(!createImageView(swapchainImages[i], swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImageViews[i])){
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

		destroyImageView(swapchainImageViews[i]);
	}
	swapchainImageViews.clear();
	print("[INFO] Destroyed image views");
}

bool VulkanServer::createDepthTestResources(){

	VkFormat depthFormat = findBestDepthFormat();

	if( !createImage(swapchainExtent.width, swapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory) ){
		print("[ERROR] Image not create");
		return false;
	}

	if( !createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView)){
		print("[ERROR] Failed to create depth image view");
		return false;
	}

	print("[INFO] Depth test resources created");
	return true;
}

void VulkanServer::destroyDepthTestResources(){
	print("[INFO] Depth test resources destroyed");
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

bool VulkanServer::createDescriptorSetLayouts(){

	VkDescriptorSetLayoutBinding cameraDescriptorBinding = {};
	cameraDescriptorBinding.binding = 0;
	cameraDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	// Since I can define an array of uniform with descriptorCount I can define the size of this array
	cameraDescriptorBinding.descriptorCount = 1;
	cameraDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = 1;
	layoutCreateInfo.pBindings = &cameraDescriptorBinding;

	VkResult res = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &cameraDescriptorSetLayout );
	if(VK_SUCCESS != res){
		print("[ERROR] Error during creation of camera descriptor layout");
		return false;
	}

	VkDescriptorSetLayoutBinding meshesDescriptorBinding = {};
	meshesDescriptorBinding.binding = 0;
	meshesDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	meshesDescriptorBinding.descriptorCount = 1;
	meshesDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	layoutCreateInfo.bindingCount = 1;
	layoutCreateInfo.pBindings = &meshesDescriptorBinding;

	res = vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &meshesDescriptorSetLayout );
	if(VK_SUCCESS != res){
		print("[ERROR] Error during creation of meshes descriptor layout");
		return false;
	}

	print("[INFO] Uniform descriptor layouts created");
	return true;
}

void VulkanServer::destroyDescriptorSetLayouts(){
	if(VK_NULL_HANDLE != cameraDescriptorSetLayout){
		vkDestroyDescriptorSetLayout(device, cameraDescriptorSetLayout, nullptr);
		print("[INFO] camera uniform descriptor destroyed");
	}

	if(VK_NULL_HANDLE != meshesDescriptorSetLayout){
		vkDestroyDescriptorSetLayout(device, meshesDescriptorSetLayout, nullptr);
		print("[INFO] Meshe uniform descriptor destroyed");
	}
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
		VkDescriptorSetLayout layouts[] = {cameraDescriptorSetLayout, meshesDescriptorSetLayout};
		VkPipelineLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 2;
		layoutCreateInfo.pSetLayouts = layouts;

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

bool VulkanServer::createBufferMemoryDeviceAllocator(){

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = device;
	allocatorCreateInfo.preferredLargeHeapBlockSize = 1ull * 1024 * 1024 * 1024; // 1 GB

	if( VK_SUCCESS!=vmaCreateAllocator(&allocatorCreateInfo, &bufferMemoryDeviceAllocator)){
		print("[ERROR] Vertex buffer creation of VMA allocator failed");
		return false;
	}

	return true;
}

void VulkanServer::destroyBufferMemoryDeviceAllocator(){
	vmaDestroyAllocator(bufferMemoryDeviceAllocator);
	bufferMemoryDeviceAllocator = VK_NULL_HANDLE;
}

bool VulkanServer::createBufferMemoryHostAllocator(){

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = device;

	if( VK_SUCCESS!=vmaCreateAllocator(&allocatorCreateInfo, &bufferMemoryHostAllocator)){
		print("[ERROR] Vertex buffer creation of VMA allocator failed");
		return false;
	}

	return true;
}

void VulkanServer::destroyBufferMemoryHostAllocator(){
	vmaDestroyAllocator(bufferMemoryHostAllocator);
	bufferMemoryHostAllocator = VK_NULL_HANDLE;
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

bool VulkanServer::createUniformBuffers(){

	if( !createBuffer(bufferMemoryHostAllocator,
					  sizeof(CameraUniformBufferObject),
					  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_CPU_TO_GPU,
					  cameraUniformBuffer,
					  cameraUniformBufferAllocation) ){

		print("[ERROR] Scene uniform buffer allocation failed");
		return false;
	}

	// The below line of code is a bit operation that take correct multiple of physicalDeviceMinUniformBufferOffsetAlignment to use as alingment offset
	// It works only when the physicalDeviceMinUniformBufferOffsetAlignment is a multiple of 2 (That in this case we can stay sure about it)
	//
	// Takes only the most significant bit of "sizeof(MeshUniformBufferObject) + physicalDeviceMinUniformBufferOffsetAlignment -1" that is for sure a multiple of
	// physicalDeviceMinUniformBufferOffsetAlignment and a multiple of 2
	meshDynamicUniformBufferOffset = (sizeof(MeshUniformBufferObject) + physicalDeviceMinUniformBufferOffsetAlignment -1) & ~(physicalDeviceMinUniformBufferOffsetAlignment -1);

	if( !createBuffer(bufferMemoryHostAllocator,
					  meshDynamicUniformBufferOffset * MAX_MESH_COUNT,
					  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  VMA_MEMORY_USAGE_CPU_TO_GPU,
					  meshUniformBufferData.meshUniformBuffer,
					  meshUniformBufferData.meshUniformBufferAllocation) ){

		print("[ERROR] Mesh uniform buffer allocation failed, mesh not added");
		return false;
	}

	meshUniformBufferData.size = MAX_MESH_COUNT;
	meshUniformBufferData.count = 0;

	print("[INFO] uniform buffers allocation success");
	return true;
}

void VulkanServer::destroyUniformBuffers(){
	destroyBuffer(bufferMemoryHostAllocator, meshUniformBufferData.meshUniformBuffer, meshUniformBufferData.meshUniformBufferAllocation);
	destroyBuffer(bufferMemoryHostAllocator, cameraUniformBuffer, cameraUniformBufferAllocation);
	print("[INFO] All buffers was freed");
}

bool VulkanServer::createUniformPools(){

	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &poolSize;
	poolCreateInfo.maxSets = 1;

	VkResult res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &cameraDescriptorPool);
	if(res!=VK_SUCCESS){
		print("[ERROR] Error during creation of camera descriptor pool");
	}

	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSize.descriptorCount = 1;

	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.maxSets = 1;

	res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &meshesDescriptorPool);
	if(res!=VK_SUCCESS){
		print("[ERROR] Error during creation of meshes descriptor pool");
	}

	print("[INFO] Uniform pools created");
	return true;
}

void VulkanServer::destroyUniformPools(){
	if(cameraDescriptorPool!=VK_NULL_HANDLE){
		vkDestroyDescriptorPool(device, cameraDescriptorPool, nullptr);
		cameraDescriptorPool = VK_NULL_HANDLE;
		print("[INFO] Camera uniform pool destroyed");
	}

	if(meshesDescriptorPool!=VK_NULL_HANDLE){
		vkDestroyDescriptorPool(device, meshesDescriptorPool, nullptr);
		meshesDescriptorPool = VK_NULL_HANDLE;
		print("[INFO] Meshes uniform pool destroyed");
	}
}

bool VulkanServer::allocateAndConfigureDescriptorSet(){

	// Define the structure of camera uniform buffer
	VkDescriptorSetAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocationInfo.descriptorPool = cameraDescriptorPool;
	allocationInfo.descriptorSetCount = 1;
	allocationInfo.pSetLayouts = &cameraDescriptorSetLayout;

	VkResult res = vkAllocateDescriptorSets(device, &allocationInfo, &cameraDescriptorSet);
	if(res!=VK_SUCCESS){
		print("[ERROR] Allocation of descriptor set failed");
		return false;
	}

	VkDescriptorBufferInfo cameraBufferInfo = {};
	cameraBufferInfo.buffer = cameraUniformBuffer;
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = sizeof(CameraUniformBufferObject);

	VkWriteDescriptorSet cameraWriteDescriptor = {};
	cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWriteDescriptor.dstSet = cameraDescriptorSet;
	cameraWriteDescriptor.dstBinding = 0;
	cameraWriteDescriptor.dstArrayElement = 0;
	cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWriteDescriptor.descriptorCount = 1;
	cameraWriteDescriptor.pBufferInfo = &cameraBufferInfo;

	vkUpdateDescriptorSets(device, 1, &cameraWriteDescriptor, 0, nullptr);

	// Define the structure of meshes uniform buffer
	allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocationInfo.descriptorPool = meshesDescriptorPool;
	allocationInfo.descriptorSetCount = 1;
	allocationInfo.pSetLayouts = &meshesDescriptorSetLayout;

	res = vkAllocateDescriptorSets(device, &allocationInfo, &meshesDescriptorSet);
	if(res!=VK_SUCCESS){
		print("[ERROR] Allocation of descriptor set failed");
		return false;
	}

	VkDescriptorBufferInfo meshBufferInfo = {};
	meshBufferInfo.buffer = meshUniformBufferData.meshUniformBuffer;
	meshBufferInfo.offset = 0;
	meshBufferInfo.range = meshDynamicUniformBufferOffset;

	VkWriteDescriptorSet meshesWriteDescriptor = {};
	meshesWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	meshesWriteDescriptor.dstSet = meshesDescriptorSet;
	meshesWriteDescriptor.dstBinding = 0;
	meshesWriteDescriptor.dstArrayElement = 0;
	meshesWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	meshesWriteDescriptor.descriptorCount = 1;
	meshesWriteDescriptor.pBufferInfo = &meshBufferInfo;

	vkUpdateDescriptorSets(device, 1, &meshesWriteDescriptor, 0, nullptr);

	print("[INFO] Descriptor set created");
	return true;
}

bool VulkanServer::createCommandPool(){
	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkResult res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &graphicsCommandPool);
	if(res!=VK_SUCCESS){
		print("[ERROR] Command pool creation failed");
		return false;
	}

	print("[INFO] Command pool craeted");
	return true;
}

void VulkanServer::destroyCommandPool(){
	if(graphicsCommandPool == VK_NULL_HANDLE)
		return;

	vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
	graphicsCommandPool = VK_NULL_HANDLE;
	print("[INFO] Command pool destroyed");

	// Since the command buffers are destroyed by this function here I want clear it
	drawCommandBuffers.clear();
}

bool VulkanServer::allocateCommandBuffers(){
	// Doesn't require destructions (it's performed automatically during the destruction of command pool)

	drawCommandBuffers.resize(swapchainImages.size());

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = graphicsCommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t) drawCommandBuffers.size();

	if(VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, drawCommandBuffers.data())){
		print("[ERROR] Draw Command buffer allocation fail");
		return false;
	}

	if(VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, &copyCommandBuffer)){
		print("[ERROR] Copy command buffer allocation failed");
		return false;
	}

	print("[INFO] command buffers allocated");
	return true;
}

void VulkanServer::beginCommandBuffers(){

	vkWaitForFences(device, drawFinishFences.size(), drawFinishFences.data(), VK_TRUE, LONGTIMEOUT_NANOSEC);
	for(int i = drawCommandBuffers.size() - 1; 0<=i; --i){

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		// Begin command buffer
		vkBeginCommandBuffer(drawCommandBuffers[i], &beginInfo);

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
		vkCmdBeginRenderPass(drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind graphics pipeline
		vkCmdBindPipeline(drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		if(meshes.size()>0){
			VkDescriptorSet descriptorsSets[] = {cameraDescriptorSet, VK_NULL_HANDLE};
			// Bind buffers
			for(int m = 0, s = meshes.size(); m<s; ++m){

				descriptorsSets[1] = meshesDescriptorSet; // TODOD set here the right descriptor set
				uint32_t dynamicOffset = meshes[m]->meshUniformBufferOffset * meshDynamicUniformBufferOffset;

				vkCmdBindDescriptorSets(drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 2, descriptorsSets, 1, &dynamicOffset);
				vkCmdBindVertexBuffers(drawCommandBuffers[i], 0, 1, &meshes[m]->vertexBuffer, &meshes[m]->verticesBufferOffset);
				vkCmdBindIndexBuffer(drawCommandBuffers[i], meshes[m]->indexBuffer, meshes[m]->indicesBufferOffset, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCommandBuffers[i], meshes[m]->mesh->getCountIndices(), 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(drawCommandBuffers[i]);

		if(VK_SUCCESS != vkEndCommandBuffer(drawCommandBuffers[i])){
			print("[ERROR - not handled] end render pass failed");
		}
	}
	print("[INFO] Command buffers initializated");
}

bool VulkanServer::createSyncObjects(){

	// Create semaphores
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult res = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &imageAvailableSemaphore);
	if(res!=VK_SUCCESS){
		print("[ERROR] Semaphore creation failed");
		return false;
	}
	res = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphore);
	if(res!=VK_SUCCESS){
		print("[ERROR] Semaphore creation failed");
		return false;
	}

	print("[INFO] Semaphores created");

	// Create fences
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	res = vkCreateFence(device, &fenceCreateInfo, nullptr, &copyFinishFence);
	if(res!=VK_SUCCESS){
		print("[ERROR] Copy fences creation failed");
		return false;
	}

	drawFinishFences.resize(drawCommandBuffers.size());
	for(int i = drawFinishFences.size()-1; 0<=i; --i){
		res = vkCreateFence(device, &fenceCreateInfo, nullptr, &drawFinishFences[i]);
		if(res!=VK_SUCCESS){
			print("[ERROR] Draw fences creation failed");
			return false;
		}
	}

	print("[INFO] Fences created");
	return true;
}

void VulkanServer::destroySyncObjects(){

	if(imageAvailableSemaphore != VK_NULL_HANDLE){
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		imageAvailableSemaphore = VK_NULL_HANDLE;
	}

	if(renderFinishedSemaphore != VK_NULL_HANDLE){
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		renderFinishedSemaphore = VK_NULL_HANDLE;
	}

	if(copyFinishFence != VK_NULL_HANDLE){
		vkDestroyFence(device, copyFinishFence, nullptr);
		copyFinishFence = VK_NULL_HANDLE;
	}

	print("[INFO] Semaphores and Fences destroyed");
}

void VulkanServer::removeAllMeshes(){
	meshesCopyPending.clear();

	for(int m = meshes.size() -1; 0<=m; --m){
		removeMesh_internal(meshes[m]);
	}
	meshes.clear();
	print("[INFO] All meshes removed from scene");
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

int VulkanServer::autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_deviceType, VkPhysicalDeviceProperties *r_deviceProps){

	print("[INFO] Select physical device");

	for(int i = p_devices.size()-1; 0<=i; --i){

		// Check here the device
		VkPhysicalDeviceFeatures deviceFeatures;
		uint32_t extensionsCount;

		vkGetPhysicalDeviceProperties(p_devices[i], r_deviceProps);
		vkGetPhysicalDeviceFeatures(p_devices[i], &deviceFeatures);

		vkEnumerateDeviceExtensionProperties(p_devices[i], nullptr, &extensionsCount, nullptr);

		vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkEnumerateDeviceExtensionProperties(p_devices[i], nullptr, &extensionsCount, availableExtensions.data());

		if( r_deviceProps->deviceType != p_deviceType)
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
	reloadDrawCommandBuffer = true;
}

bool VulkanServer::createBuffer(VmaAllocator p_allocator, VkDeviceSize p_size, VkMemoryPropertyFlags p_usage, VkSharingMode p_sharingMode, VmaMemoryUsage p_memoryUsage, VkBuffer &r_buffer, VmaAllocation &r_allocation){

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = p_size;
	bufferCreateInfo.usage = p_usage;
	bufferCreateInfo.sharingMode = p_sharingMode;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = p_memoryUsage;

	VmaAllocationInfo allocationInfo = {};

	if(VK_SUCCESS!=vmaCreateBuffer(p_allocator, &bufferCreateInfo, &allocationCreateInfo, &r_buffer, &r_allocation, &allocationInfo)){
		print("[ERROR] failed to allocate memory");
		return false;
	}

	return allocationInfo.size>=p_size;
}

void VulkanServer::destroyBuffer(VmaAllocator p_allocator, VkBuffer &r_buffer, VmaAllocation &r_allocation){
	vmaDestroyBuffer(p_allocator, r_buffer, r_allocation);
	r_buffer = VK_NULL_HANDLE;
	r_allocation = VK_NULL_HANDLE;
}


bool VulkanServer::hasStencilComponent(VkFormat p_format) {
	return p_format == VK_FORMAT_D32_SFLOAT_S8_UINT || p_format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat VulkanServer::findBestDepthFormat(){
	VkFormat out;
	if(chooseBestSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, &out)){
		return out;
	}

	// TODO Really bad error handling
	return VK_FORMAT_D32_SFLOAT;
}

bool VulkanServer::chooseBestSupportedFormat(const vector<VkFormat> &p_formats, VkImageTiling p_tiling, VkFormatFeatureFlags p_features, VkFormat *r_format){
	VkFormatProperties props;
	for(int i = 0, s = p_formats.size(); i<s; ++i){
		vkGetPhysicalDeviceFormatProperties(physicalDevice, p_formats[i], &props);

		if(p_tiling == VK_IMAGE_TILING_LINEAR){
			if((props.linearTilingFeatures & p_features) == p_features){
				*r_format = p_formats[i];
				return true;
			}
		}else if(p_tiling == VK_IMAGE_TILING_OPTIMAL){
			if((props.optimalTilingFeatures & p_features) == p_features){
				*r_format = p_formats[i];
				return true;
			}
		}
	}
	print("[ERROR - not handled] Not able to find supported format");
	return false;
}

bool VulkanServer::createImage(uint32_t p_width, uint32_t p_height, VkFormat &p_format, VkImageTiling p_tiling, VkImageUsageFlags p_usage, VkMemoryPropertyFlags p_memoryFlags, VkImage &p_image, VkDeviceMemory &p_memory){
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = p_format;
	imageCreateInfo.extent.width = p_width;
	imageCreateInfo.extent.height = p_height;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.tiling = p_tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = p_usage;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if(VK_SUCCESS!=vkCreateImage(device, &imageCreateInfo, nullptr, &p_image)){
		return false;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, p_image, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = chooseMemoryType(memoryRequirements.memoryTypeBits, p_memoryFlags);

	if( VK_SUCCESS != vkAllocateMemory(device, &memoryAllocInfo, nullptr, &p_memory)){
		return false;
	}

	vkBindImageMemory(device, p_image, p_memory, /*Offset*/0);

	return true;
}

void VulkanServer::destroyImage(VkImage &p_image, VkDeviceMemory &p_memory){
	vkDestroyImage(device, p_image, nullptr);
	vkFreeMemory(device, p_memory, nullptr);
	p_image = VK_NULL_HANDLE;
	p_memory = VK_NULL_HANDLE;
}

bool VulkanServer::createImageView(VkImage p_image, VkFormat p_format, VkImageAspectFlags p_aspectFlags, VkImageView &r_imageView){

	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = p_image;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = p_format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.subresourceRange.aspectMask = p_aspectFlags;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;

	if(VK_SUCCESS != vkCreateImageView(device, &viewCreateInfo, nullptr, &r_imageView)){
		return false;
	}

	return true;
}

void VulkanServer::destroyImageView(VkImageView &r_imageView){
	vkDestroyImageView(device, r_imageView, nullptr );
	r_imageView = VK_NULL_HANDLE;
}

VisualServer::VisualServer()
	: window(nullptr) {
}

VisualServer::~VisualServer(){

}

bool VisualServer::init(){

	createWindow();
	const bool vulkanState = vulkanServer.create(window);
	return vulkanState;
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

void VisualServer::addMesh(const Mesh *p_mesh){
	vulkanServer.addMesh(p_mesh);
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
