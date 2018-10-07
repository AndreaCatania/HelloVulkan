#include "VisualServer.h"

// Implementation of VMA
#define VMA_IMPLEMENTATION
#include "libs/vma/vk_mem_alloc.h"

#include "mesh.h"
#include "servers/window_server.h"
#include "texture.h"
#include <fstream>
#include <iostream>

#define INITIAL_WINDOW_WIDTH 800
#define INITIAL_WINDOW_HEIGHT 600

// This cap is necessary because I've no memory management yet
#define MAX_MESH_COUNT 50

// This rotate the camera view in order to make Coordinate system as:
// Y+ Up
// X+ Right
// Z+ Backward
const glm::mat4 VulkanServer::COORDSYSTEMROTATOR =
		glm::rotate(glm::mat4(1.f), glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f));

bool readFile(const string &filename, vector<char> &r_out) {
	// Read the file from the bottom in binary
	ifstream file(filename, ios::ate | ios::binary);

	if (!file.is_open()) {
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
		VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType,
		uint64_t obj, size_t location, int32_t code, const char *layerPrefix,
		const char *msg, void *userData) {

	print(string("[VL ] ") + string(msg));

	return VK_FALSE;
}

Camera::Camera() :
		transform(1.),
		projection(1.),
		aspect(1),
		FOV(glm::radians(60.)),
		near(0.1),
		far(100),
		isDirty(true),
		isProjectionDirty(true) {}

void Camera::lookAt(const glm::vec3 &p_pos, const glm::vec3 &p_target) {
	glm::vec3 y(0, 1.f, 0);
	glm::vec3 depth(glm::normalize(p_pos - p_target));
	glm::vec3 x(glm::normalize(glm::cross(y, depth)));
	y = glm::normalize(glm::cross(depth, x));

	glm::mat4 mat;
	mat[0] = glm::vec4(x, 0.);
	mat[1] = glm::vec4(y, 0.);
	mat[2] = glm::vec4(depth, 0);
	mat[3] = glm::vec4(p_pos, 1.);

	setTransform(mat);
}

void Camera::setTransform(const glm::mat4 &p_transform) {
	transform = p_transform;
	isDirty = true;
}

const glm::mat4 &Camera::getProjection() const {
	if (isProjectionDirty) {
		const_cast<Camera *>(this)->reloadProjection();
	}
	return projection;
}

void Camera::reloadProjection() {
	projection = glm::perspectiveRH_ZO(FOV, aspect, near, far);
	isProjectionDirty = false;
}

void Camera::setAspect(uint32_t p_width, uint32_t p_height) {
	aspect = p_width / (float)p_height;
	isDirty = true;
	isProjectionDirty = true;
}

void Camera::setFOV_deg(float p_FOV_deg) {
	FOV = glm::radians(p_FOV_deg);
	isDirty = true;
	isProjectionDirty = true;
}

void Camera::setNearFar(float p_near, float p_far) {
	near = p_near;
	far = p_far;
	isDirty = true;
	isProjectionDirty = true;
}

VulkanServer::VulkanServer(VisualServer *p_visualServer) :
		visualServer(p_visualServer),
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
		meshImagesDescriptorSetLayout(VK_NULL_HANDLE),
		pipelineLayout(VK_NULL_HANDLE),
		graphicsPipeline(VK_NULL_HANDLE),
		bufferMemoryDeviceAllocator(VK_NULL_HANDLE),
		bufferMemoryHostAllocator(VK_NULL_HANDLE),
		sceneUniformBuffer(VK_NULL_HANDLE),
		sceneUniformBufferAllocation(VK_NULL_HANDLE),
		graphicsCommandPool(VK_NULL_HANDLE),
		imageAvailableSemaphore(VK_NULL_HANDLE),
		copyFinishFence(VK_NULL_HANDLE),
		reloadDrawCommandBuffer(true) {
	deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

bool VulkanServer::enableValidationLayer() {
#ifdef DEBUG_ENABLED
	return true;
#else
	return false;
#endif
}

bool VulkanServer::create(WindowServer *p_window) {

	window = p_window;

	if (!createInstance())
		return false;

	if (!createDebugCallback())
		return false;

	if (!createSurface())
		return false;

	if (!pickPhysicalDevice())
		return false;

	if (!createLogicalDevice())
		return false;

	lockupDeviceQueue();

	if (!createCommandPool())
		return false;

	if (!createDescriptorSetLayouts())
		return false;

	if (!createSwapchain())
		return false;

	if (!createBufferMemoryDeviceAllocator())
		return false;

	if (!createBufferMemoryHostAllocator())
		return false;

	if (!createUniformBuffers())
		return false;

	if (!createUniformPools())
		return false;

	if (!allocateConfigureCameraDescriptorSet())
		return false;

	if (!allocateConfigureMeshesDescriptorSet())
		return false;

	if (!allocateCommandBuffers())
		return false;

	if (!createSyncObjects())
		return false;

	reloadCamera();

	return true;
}

void VulkanServer::destroy() {

	waitIdle();

	removeAllMeshes();
	destroySyncObjects();
	destroyUniformPools();
	destroyUniformBuffers();
	destroyBufferMemoryHostAllocator();
	destroyBufferMemoryDeviceAllocator();
	destroySwapchain();
	destroyCommandPool();
	destroyDescriptorSetLayouts();
	destroyLogicalDevice();
	destroyDebugCallback();
	destroySurface();
	destroyInstance();
	window = nullptr;
}

void VulkanServer::waitIdle() {
	// assert that the device has finished all before cleanup
	if (device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(device);
}

#define LONGTIMEOUT_NANOSEC 3.6e+12 // 1 hour

void VulkanServer::draw() {

	if (reloadDrawCommandBuffer) {
		beginCommandBuffers();
		reloadDrawCommandBuffer = false;
	}

	processCopy();

	updateUniformBuffers();

	// Acquire the next image
	uint32_t imageIndex;
	VkResult acquireRes = vkAcquireNextImageKHR(device, swapchain, LONGTIMEOUT_NANOSEC, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (VK_ERROR_OUT_OF_DATE_KHR == acquireRes || VK_SUBOPTIMAL_KHR == acquireRes) {
		// Vulkan tell me that the surface is no more compatible, so is mandatory
		// recreate the swap chain
		recreateSwapchain();
		return;
	}

	// This is used to be sure that the previous drawing has finished
	vkWaitForFences(device, 1, &drawFinishFences[imageIndex], VK_TRUE, LONGTIMEOUT_NANOSEC);
	vkResetFences(device, 1, &drawFinishFences[imageIndex]);

	// Submit draw commands
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[imageIndex];

	if (VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFinishFences[imageIndex])) {
		print("[ERROR not handled] Error during queue submission");
		return;
	}

	// Present
	VkPresentInfoKHR presInfo = {};
	presInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presInfo.waitSemaphoreCount = 1;
	presInfo.pWaitSemaphores = &renderFinishedSemaphores[imageIndex];
	presInfo.swapchainCount = 1;
	presInfo.pSwapchains = &swapchain;
	presInfo.pImageIndices = &imageIndex;

	VkResult presentRes = vkQueuePresentKHR(presentationQueue, &presInfo);
	if (VK_ERROR_OUT_OF_DATE_KHR == presentRes || VK_SUBOPTIMAL_KHR == presentRes) {
		// Vulkan tell me that the surface is no more compatible, so is mandatory
		// recreate the swap chain
		recreateSwapchain();
		return;
	}
}

void VulkanServer::addMesh(Mesh *p_mesh) {

	if (meshUniformBufferData.count >= meshUniformBufferData.size) {
		print("[ERROR] Max mesh limit reached, can't add more meshes");
		return;
	}

	if (p_mesh->meshHandle)
		return;

	p_mesh->meshHandle = new MeshHandle(p_mesh, this);
	if (p_mesh->meshHandle->prepare()) {
		meshesCopyPending.push_back(p_mesh->meshHandle);
	}
}

void VulkanServer::removeMesh(Mesh *p_mesh) {
	removeMesh(p_mesh->meshHandle);
}

void VulkanServer::removeMesh(MeshHandle *p_meshHandle) {

	// TODO Make the removal in a way that wait iddle is not required
	waitIdle();

	int item = -1;
	for (int i = meshes.size() - 1; 0 <= i; --i) {
		if (meshes[i] == p_meshHandle) {
			item = i;
			break;
		}
	}

	if (-1 == item)
		return;

	size_t s = meshes.size();
	if (1 < s) {
		meshes[item] = meshes[s - 1];
		meshes.resize(s - 1);
	} else {
		meshes.clear();
	}

	p_meshHandle->mesh->meshHandle = nullptr;
	delete p_meshHandle;
}

void VulkanServer::processCopy() {
	VkResult fenceStatus = vkGetFenceStatus(device, copyFinishFence);
	if (fenceStatus != VK_SUCCESS) {
		return; // Copy is in progress
	}

	if (meshesCopyInProgress.size() > 0) {

		// Copy process end

		// Clear command buffer
		freeCommand(copyCommandBuffer);

		meshes.insert(meshes.end(), meshesCopyInProgress.begin(), meshesCopyInProgress.end());
		meshesCopyInProgress.clear();

		reloadDrawCommandBuffer = true;
	}

	// Check if there are meshes to copy in pending
	if (meshesCopyPending.size() > 0) {

		// Start new copy
		beginOneTimeCommand(copyCommandBuffer);

		for (int m = meshesCopyPending.size() - 1; 0 <= m; --m) {
			vkCmdUpdateBuffer(copyCommandBuffer, meshesCopyPending[m]->vertexBuffer, 0, meshesCopyPending[m]->verticesSize, meshesCopyPending[m]->mesh->vertices.data());
			vkCmdUpdateBuffer(copyCommandBuffer, meshesCopyPending[m]->indexBuffer, 0, meshesCopyPending[m]->indicesSize, meshesCopyPending[m]->mesh->triangles.data());
		}

		if (!endCommand(copyCommandBuffer)) {
			print("[ERROR] Copy command buffer ending failed");
			return;
		}

		if (!submitCommand(copyCommandBuffer, copyFinishFence)) {
			print("[ERROR] Copy command buffer submission failed");
			return;
		}

		meshesCopyInProgress.insert(meshesCopyInProgress.end(), meshesCopyPending.begin(), meshesCopyPending.end());
		meshesCopyPending.clear();
	}
}

void VulkanServer::updateUniformBuffers() {

	void *data;

	// Update camera buffer
	if (camera.isDirty) {

		SceneUniformBufferObject sceneUBO = {};
		glm::mat4 t(camera.transform);
		// This is necessary since the Projection matrix invert Z (depth)
		// and from RightHanded the Coordinate system becomes Left Handed
		// This reset it to RightHanded but is also necessary to set Face
		// orientation As Counter Clockwise
		t[0] *= -1;
		sceneUBO.cameraView = t * COORDSYSTEMROTATOR;
		// Inverse is required since the camera should be moved inverselly to
		// simulate world space positioning
		sceneUBO.cameraViewInverse = glm::inverse(sceneUBO.cameraView);
		sceneUBO.cameraProjection = camera.getProjection();

		vmaMapMemory(bufferMemoryHostAllocator, sceneUniformBufferAllocation, &data);
		memcpy((SceneUniformBufferObject *)data, &sceneUBO, sizeof(SceneUniformBufferObject));
		vmaUnmapMemory(bufferMemoryHostAllocator, sceneUniformBufferAllocation);

		camera.isDirty = false;
	}

	// Update mesh dynamic uniform buffers
	vmaMapMemory(bufferMemoryHostAllocator, meshUniformBufferData.meshUniformBufferAllocation, &data);
	MeshUniformBufferObject supportMeshUBO;
	for (int i = meshes.size() - 1; 0 <= i; --i) {
		if (!meshes[i]->hasTransformationChange)
			continue;
		supportMeshUBO.model = meshes[i]->mesh->transformation;
		memcpy((MeshUniformBufferObject *)data + meshes[i]->meshUniformBufferOffset * meshDynamicUniformBufferOffset, &supportMeshUBO, sizeof(MeshUniformBufferObject));
		meshes[i]->hasTransformationChange = false;
	}
	vmaUnmapMemory(bufferMemoryHostAllocator, meshUniformBufferData.meshUniformBufferAllocation);
}

bool VulkanServer::createImageLoadBuffer(VkDeviceSize p_size, VkBuffer &r_buffer, VmaAllocation &r_allocation, VmaAllocator &r_allocator) {

	r_allocator = bufferMemoryHostAllocator;
	return createBuffer(bufferMemoryHostAllocator, p_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, VMA_MEMORY_USAGE_CPU_TO_GPU, r_buffer, r_allocation);
}

bool VulkanServer::createImageTexture(uint32_t p_width, uint32_t p_height, VkImage &r_image, VkDeviceMemory &r_memory) {
	return createImage(p_width, p_height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, r_image, r_memory);
}

bool VulkanServer::createImageViewTexture(VkImage p_image, VkImageView &r_imageView) {
	return createImageView(p_image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, r_imageView);
}

bool VulkanServer::createInstance() {

	print("Instancing Vulkan");

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
	vector<const char *> requiredExtensions;

	if (enableValidationLayer()) {
		layers.push_back("VK_LAYER_LUNARG_standard_validation");
		if (!checkValidationLayersSupport(layers)) {
			return false;
		}

		// Validation layer extension to enable validation
		requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	window->appendRequiredExtensions(requiredExtensions);

	if (!checkInstanceExtensionsSupport(requiredExtensions)) {
		return false;
	}

	createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	createInfo.ppEnabledLayerNames = layers.data();
	createInfo.enabledExtensionCount =
			static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);

	if (VK_SUCCESS == res) {
		print("Instancing Vulkan success");
		return true;
	} else {
		std::cout << "[ERROR] Instancing error: " << res << std::endl;
		return false;
	}
}

void VulkanServer::destroyInstance() {
	if (instance == VK_NULL_HANDLE)
		return;
	vkDestroyInstance(instance, nullptr);
	instance = VK_NULL_HANDLE;
	print("[INFO] Vulkan instance destroyed");
}

bool VulkanServer::createDebugCallback() {
	if (!enableValidationLayer())
		return true;

	VkDebugReportCallbackCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
	createInfo.pfnCallback = debugCallbackFnc;

	// Load the extension function to create the callback
	PFN_vkCreateDebugReportCallbackEXT func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (!func) {
		print("[Error] Can't load function to create debug callback");
		return false;
	}

	VkResult res = func(instance, &createInfo, nullptr, &debugCallback);
	if (res == VK_SUCCESS) {
		print("[INFO] Debug callback loaded");
		return true;
	} else {
		print("[ERROR] debug callback not created");
		return false;
	}
}

void VulkanServer::destroyDebugCallback() {
	if (debugCallback == VK_NULL_HANDLE)
		return;

	PFN_vkDestroyDebugReportCallbackEXT func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (!func) {
		print("[ERROR] Destroy functin not loaded");
		return;
	}

	func(instance, debugCallback, nullptr);
	debugCallback = VK_NULL_HANDLE;
	print("[INFO] Debug callback destroyed");
}

bool VulkanServer::createSurface() {

	if (window->createSurface(instance, &surface)) {
		print("[INFO] surface created");
		return true;
	} else {
		print("[ERROR] surface not created");
		return false;
	}
}

void VulkanServer::destroySurface() {
	if (surface == VK_NULL_HANDLE)
		return;
	vkDestroySurfaceKHR(instance, surface, nullptr);
	surface = VK_NULL_HANDLE;
}

bool VulkanServer::pickPhysicalDevice() {

	uint32_t availableDevicesCount = 0;
	vkEnumeratePhysicalDevices(instance, &availableDevicesCount, nullptr);

	if (0 == availableDevicesCount) {
		print("[Error] No devices that supports Vulkan");
		return false;
	}
	vector<VkPhysicalDevice> availableDevices(availableDevicesCount);
	vkEnumeratePhysicalDevices(instance, &availableDevicesCount, availableDevices.data());

	VkPhysicalDeviceProperties deviceProps;

	int id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, &deviceProps);
	if (id < 0) {
		id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &deviceProps);
		if (id < 0) {
			id = autoSelectPhysicalDevice(availableDevices, VK_PHYSICAL_DEVICE_TYPE_CPU, &deviceProps);
			if (id < 0) {
				print("[Error] No suitable device");
				return false;
			}
		}
	}

	physicalDevice = availableDevices[id];

	physicalDeviceMinUniformBufferOffsetAlignment =
			deviceProps.limits.minUniformBufferOffsetAlignment;

	print("[INFO] Physical Device selected");
	return true;
}

bool VulkanServer::createLogicalDevice() {

	float priority = 1.f;

	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);
	vector<VkDeviceQueueCreateInfo> queueCreateInfoArray;

	VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {};
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &priority;

	queueCreateInfoArray.push_back(graphicsQueueCreateInfo);

	if (queueIndices.graphicsFamilyIndex !=
			queueIndices.presentationFamilyIndex) {
		// Create dedicated presentation queue
		VkDeviceQueueCreateInfo presentationQueueCreateInfo = {};
		presentationQueueCreateInfo.sType =
				VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		presentationQueueCreateInfo.queueFamilyIndex =
				queueIndices.presentationFamilyIndex;
		presentationQueueCreateInfo.queueCount = 1;
		presentationQueueCreateInfo.pQueuePriorities = &priority;

		queueCreateInfoArray.push_back(presentationQueueCreateInfo);
	}

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfos = {};
	deviceCreateInfos.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfos.queueCreateInfoCount = queueCreateInfoArray.size();
	deviceCreateInfos.pQueueCreateInfos = queueCreateInfoArray.data();
	deviceCreateInfos.pEnabledFeatures = &physicalDeviceFeatures;
	deviceCreateInfos.enabledExtensionCount = deviceExtensions.size();
	deviceCreateInfos.ppEnabledExtensionNames = deviceExtensions.data();

	if (enableValidationLayer()) {
		deviceCreateInfos.enabledLayerCount = layers.size();
		deviceCreateInfos.ppEnabledLayerNames = layers.data();
	} else {
		deviceCreateInfos.enabledLayerCount = 0;
	}

	VkResult res =
			vkCreateDevice(physicalDevice, &deviceCreateInfos, nullptr, &device);
	if (res == VK_SUCCESS) {
		print("[INFO] Local device created");
		return true;
	} else {
		print("[ERROR] Logical device creation failed");
		return false;
	}
}

void VulkanServer::destroyLogicalDevice() {
	if (device == VK_NULL_HANDLE)
		return;
	vkDestroyDevice(device, nullptr);
	device = VK_NULL_HANDLE;
	print("[INFO] Local device destroyed");
}

void VulkanServer::lockupDeviceQueue() {

	QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	vkGetDeviceQueue(device, indices.graphicsFamilyIndex, 0, &graphicsQueue);

	if (indices.graphicsFamilyIndex != indices.presentationFamilyIndex) {
		// Lockup dedicated presentation queue
		vkGetDeviceQueue(device, indices.presentationFamilyIndex, 0, &presentationQueue);
	} else {
		presentationQueue = graphicsQueue;
	}

	print("[INFO] Device queue lockup success");
}

VulkanServer::QueueFamilyIndices
VulkanServer::findQueueFamilies(VkPhysicalDevice p_device) {

	QueueFamilyIndices indices;

	uint32_t queueCounts = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCounts, nullptr);

	if (queueCounts <= 0)
		return indices;

	vector<VkQueueFamilyProperties> queueProperties(queueCounts);
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queueCounts, queueProperties.data());

	for (int i = queueProperties.size() - 1; 0 <= i; --i) {
		if (queueProperties[i].queueCount > 0 &&
				queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamilyIndex = i;
		}

		VkBool32 supported = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, surface, &supported);
		if (queueProperties[i].queueCount > 0 && supported) {
			indices.presentationFamilyIndex = i;
		}
	}

	return indices;
}

VulkanServer::SwapChainSupportDetails VulkanServer::querySwapChainSupport(VkPhysicalDevice p_device) {
	SwapChainSupportDetails chainDetails;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(p_device, surface,
			&chainDetails.capabilities);

	uint32_t formatsCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatsCount, nullptr);

	if (formatsCount > 0) {
		chainDetails.formats.resize(formatsCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(p_device, surface, &formatsCount, chainDetails.formats.data());
	}

	uint32_t modesCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &modesCount, nullptr);

	if (modesCount > 0) {
		chainDetails.presentModes.resize(modesCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(p_device, surface, &modesCount,
				chainDetails.presentModes.data());
	}

	return chainDetails;
}

VkSurfaceFormatKHR VulkanServer::chooseSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats) {
	// If the surface has not a preferred format
	// That is the best case.
	// It will return just a format with format field set to VK_FORMAT_UNDEFINED
	if (p_formats.size() == 1 && p_formats[0].format == VK_FORMAT_UNDEFINED) {
		VkSurfaceFormatKHR sFormat;
		sFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		sFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return sFormat;
	}

	// Choose the best format to use
	for (VkSurfaceFormatKHR f : p_formats) {
		if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
				f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return f;
		}
	}

	// Choose the best from the available formats
	return p_formats[0]; // TODO create an algorithm to pick the best format
}

VkPresentModeKHR VulkanServer::choosePresentMode(const vector<VkPresentModeKHR> &p_modes) {

	// This is guaranteed to be supported, but doesn't works so good
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto &pm : p_modes) {
		if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
			return pm;
		} else if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			bestMode = pm;
		}
	}

	return bestMode;
}

VkExtent2D VulkanServer::chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
	// When the extent is set to max this mean that we can set any value
	// In this case we can set any value
	// So I set the size of WIDTH and HEIGHT used for initialize Window
	int width, height;
	window->getWindowSize(&width, &height);
	VkExtent2D actualExtent = { (uint32_t)width, (uint32_t)height };

	actualExtent.width =
			max(capabilities.minImageExtent.width,
					min(capabilities.maxImageExtent.width, actualExtent.width));
	actualExtent.height =
			max(capabilities.minImageExtent.height,
					min(capabilities.maxImageExtent.height, actualExtent.height));

	return actualExtent;
}

bool VulkanServer::createSwapchain() {

	if (!createRawSwapchain())
		return false;

	lockupSwapchainImages();

	if (!createSwapchainImageViews())
		return false;

	if (!createDepthTestResources())
		return false;

	if (!createRenderPass())
		return false;

	if (!createGraphicsPipelines())
		return false;

	if (!createFramebuffers())
		return false;

	return true;
}

void VulkanServer::destroySwapchain() {

	destroyFramebuffers();
	destroyGraphicsPipelines();
	destroyRenderPass();
	destroyDepthTestResources();
	destroySwapchainImageViews();
	destroyRawSwapchain();
}

bool VulkanServer::createRawSwapchain() {

	SwapChainSupportDetails chainDetails = querySwapChainSupport(physicalDevice);

	VkSurfaceFormatKHR format = chooseSurfaceFormat(chainDetails.formats);
	VkPresentModeKHR pMode = choosePresentMode(chainDetails.presentModes);
	VkExtent2D extent2D = chooseExtent(chainDetails.capabilities);

	uint32_t imageCount = chainDetails.capabilities.minImageCount;
	// Check it we can use triple buffer
	++imageCount;
	if (chainDetails.capabilities.maxImageCount > 0) // 0 means no limits
		imageCount = min(imageCount, chainDetails.capabilities.maxImageCount);

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
	uint32_t queueFamilyIndices[] = {
		(uint32_t)queueIndices.graphicsFamilyIndex,
		(uint32_t)queueIndices.presentationFamilyIndex
	};
	if (queueIndices.graphicsFamilyIndex !=
			queueIndices.presentationFamilyIndex) {
		// Since the queue families are different I want to use concurrent mode so
		// an object can be in multiples families and I don't need to handle
		// ownership myself
		chainCreate.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		chainCreate.queueFamilyIndexCount =
				2; // Define how much concurrent families I have
		chainCreate.pQueueFamilyIndices = queueFamilyIndices; // set queue families
	} else {
		// I've only one family so I don't need concurrent mode
		chainCreate.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	chainCreate.preTransform = chainDetails.capabilities.currentTransform;
	chainCreate.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	chainCreate.presentMode = pMode;
	chainCreate.clipped = VK_TRUE;
	chainCreate.oldSwapchain = VK_NULL_HANDLE;

	VkResult res =
			vkCreateSwapchainKHR(device, &chainCreate, nullptr, &swapchain);
	if (res != VK_SUCCESS) {
		print("[ERROR] Swap chain creation fail");
		return false;
	}

	swapchainImageFormat = format.format;
	swapchainExtent = extent2D;

	print("[INFO] Created swap chain");
	return true;
}

void VulkanServer::destroyRawSwapchain() {
	if (swapchain == VK_NULL_HANDLE)
		return;
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	swapchain = VK_NULL_HANDLE;
	print("[INFO] swapchain destroyed");
}

void VulkanServer::lockupSwapchainImages() {

	uint32_t imagesCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &imagesCount, nullptr);
	swapchainImages.resize(imagesCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imagesCount,
			swapchainImages.data());

	print("[INFO] swapchain images lockup success");
}

bool VulkanServer::createSwapchainImageViews() {

	swapchainImageViews.resize(swapchainImages.size());

	bool error = false;

	for (int i = swapchainImageViews.size() - 1; i >= 0; --i) {
		if (!createImageView(swapchainImages[i], swapchainImageFormat,
					VK_IMAGE_ASPECT_COLOR_BIT, swapchainImageViews[i])) {
			swapchainImageViews[i] = VK_NULL_HANDLE;
			error = true;
		}
	}

	if (error) {
		print("[ERROR] Error during creation of Immage Views");
		return false;
	} else {
		print("[INFO] All image view created");
		return true;
	}
}

void VulkanServer::destroySwapchainImageViews() {
	for (int i = swapchainImages.size() - 1; i >= 0; --i) {

		if (swapchainImageViews[i] == VK_NULL_HANDLE)
			continue;

		destroyImageView(swapchainImageViews[i]);
	}
	swapchainImageViews.clear();
	print("[INFO] Destroyed image views");
}

bool VulkanServer::createDepthTestResources() {

	VkFormat depthFormat = findBestDepthFormat();

	if (!createImage(
				swapchainExtent.width, swapchainExtent.height, depthFormat,
				VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory)) {
		print("[ERROR] Image not create");
		return false;
	}

	if (!createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT,
				depthImageView)) {
		print("[ERROR] Failed to create depth image view");
		return false;
	}

	if (!transitionImageLayout(
				depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)) {
		print("[ERROR] Failed to change image layout");
		return true;
	}

	print("[INFO] Depth test resources created");
	return true;
}

void VulkanServer::destroyDepthTestResources() {

	destroyImageView(depthImageView);
	destroyImage(depthImage, depthImageMemory);
	print("[INFO] Depth test resources destroyed");
}

bool VulkanServer::createRenderPass() {

	VkAttachmentDescription attachments[2];

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
	attachments[0] = colorAttachmentDesc;

	// Reference to attachment description
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0; // ID of description
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment
	VkAttachmentDescription depthAttachmentDesc = {};
	depthAttachmentDesc.format = findBestDepthFormat();
	depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentDesc.finalLayout =
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1] = depthAttachmentDesc;

	// Depth attachment reference
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// The subpass describe how an attachment should be treated during execution
	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;
	subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

	// The dependency is something that lead the subpass order, is like a
	// "barrier"
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
							   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 2;
	renderPassCreateInfo.pAttachments = attachments;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDesc;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VkResult res =
			vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
	if (VK_SUCCESS != res) {
		print("[ERROR] Render pass creation fail");
		return false;
	}

	print("[INFO] Render pass created");
	return true;
}

void VulkanServer::destroyRenderPass() {
	if (VK_NULL_HANDLE == device)
		return;
	vkDestroyRenderPass(device, renderPass, nullptr);
	renderPass = VK_NULL_HANDLE;
}

bool VulkanServer::createDescriptorSetLayouts() {

	{
		// Camera uniform buffer set layout
		VkDescriptorSetLayoutBinding cameraDescriptorBinding = {};
		cameraDescriptorBinding.binding = 0;
		cameraDescriptorBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		// Since I can define an array of uniform with descriptorCount I can define
		// the size of this array
		cameraDescriptorBinding.descriptorCount = 1;
		cameraDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &cameraDescriptorBinding;

		VkResult res = vkCreateDescriptorSetLayout(
				device, &layoutCreateInfo, nullptr, &cameraDescriptorSetLayout);
		if (VK_SUCCESS != res) {
			print("[ERROR] Error during creation of camera descriptor layout");
			return false;
		}
	}

	{
		// Mesh dynamic uniform buffer
		VkDescriptorSetLayoutBinding meshesDescriptorBinding = {};
		meshesDescriptorBinding.binding = 0;
		meshesDescriptorBinding.descriptorType =
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		meshesDescriptorBinding.descriptorCount = 1;
		meshesDescriptorBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &meshesDescriptorBinding;

		VkResult res = vkCreateDescriptorSetLayout(
				device, &layoutCreateInfo, nullptr, &meshesDescriptorSetLayout);
		if (VK_SUCCESS != res) {
			print("[ERROR] Error during creation of meshes descriptor layout");
			return false;
		}
	}

	{
		// Image + Sampler image set layout
		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 0;
		samplerLayoutBinding.descriptorType =
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType =
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = &samplerLayoutBinding;

		VkResult res = vkCreateDescriptorSetLayout(
				device, &layoutCreateInfo, nullptr, &meshImagesDescriptorSetLayout);
		if (VK_SUCCESS != res) {
			print("[ERROR] Error during creation of image descriptor layout");
			return false;
		}
	}

	print("[INFO] Uniform descriptors layouts created");
	return true;
}

void VulkanServer::destroyDescriptorSetLayouts() {

	if (VK_NULL_HANDLE != meshImagesDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(device, meshImagesDescriptorSetLayout,
				nullptr);
		meshImagesDescriptorSetLayout = VK_NULL_HANDLE;
		print("[INFO] Image descriptor layout destroyed");
	}

	if (VK_NULL_HANDLE != meshesDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(device, meshesDescriptorSetLayout, nullptr);
		meshesDescriptorSetLayout = VK_NULL_HANDLE;
		print("[INFO] Meshe uniform descriptor destroyed");
	}

	if (VK_NULL_HANDLE != cameraDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(device, cameraDescriptorSetLayout, nullptr);
		cameraDescriptorSetLayout = VK_NULL_HANDLE;
		print("[INFO] camera uniform descriptor destroyed");
	}
}

#define SHADER_VERTEX_PATH "shaders/bin/vert.spv"
#define SHADER_FRAGMENT_PATH "../shaders/bin/frag.spv"

bool VulkanServer::createGraphicsPipelines() {

	/// Load shaders
	vector<char> vertexShaderBytecode;
	vector<char> fragmentShaderBytecode;

	if (!readFile(SHADER_VERTEX_PATH, vertexShaderBytecode)) {
		print(string("[ERROR] Failed to load shader bytecode: ") +
				string(SHADER_VERTEX_PATH));
		return false;
	}

	if (!readFile(SHADER_FRAGMENT_PATH, fragmentShaderBytecode)) {
		print(string("[ERROR] Failed to load shader bytecode: ") +
				string(SHADER_FRAGMENT_PATH));
		return false;
	}

	print(string("[INFO] vertex file byte loaded: ") +
			to_string(vertexShaderBytecode.size()));
	print(string("[INFO] fragment file byte loaded: ") +
			to_string(fragmentShaderBytecode.size()));

	vertShaderModule = createShaderModule(vertexShaderBytecode);
	fragShaderModule = createShaderModule(fragmentShaderBytecode);

	if (vertShaderModule == VK_NULL_HANDLE) {
		print("[ERROR] Failed to create vertex shader module");
		return false;
	}

	if (fragShaderModule == VK_NULL_HANDLE) {
		print("[ERROR] Failed to create fragment shader module");
		return false;
	}

	vector<VkPipelineShaderStageCreateInfo> shaderStages;
	{
		VkPipelineShaderStageCreateInfo vertStageCreateInfo = {};
		vertStageCreateInfo.sType =
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertStageCreateInfo.module = vertShaderModule;
		vertStageCreateInfo.pName = "main";
		shaderStages.push_back(vertStageCreateInfo);

		VkPipelineShaderStageCreateInfo fragStageCreateInfo = {};
		fragStageCreateInfo.sType =
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStageCreateInfo.module = fragShaderModule;
		fragStageCreateInfo.pName = "main";
		shaderStages.push_back(fragStageCreateInfo);
	}

	/// Vertex inputs
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
	vertexInputCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription vertexInputBindingDescription =
			Vertex::getBindingDescription();
	vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputCreateInfo.pVertexBindingDescriptions =
			&vertexInputBindingDescription;

	array<VkVertexInputAttributeDescription, 2> vertexInputAttributesDescription =
			Vertex::getAttributesDescription();
	vertexInputCreateInfo.vertexAttributeDescriptionCount =
			vertexInputAttributesDescription.size();
	vertexInputCreateInfo.pVertexAttributeDescriptions =
			vertexInputAttributesDescription.data();

	/// Input Assembly
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {};
	inputAssemblyCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	/// Viewport
	VkPipelineViewportStateCreateInfo viewportCreateInfo = {};
	viewportCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	VkViewport viewport = {};
	viewport.x = .0;
	viewport.y = .0;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = .0;
	viewport.maxDepth = 1.;

	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;

	// The scissor indicate the part of screen that we want crop, (it's not a
	// transformation nor scaling)
	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchainExtent;

	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	/// Active depth test
	VkPipelineDepthStencilStateCreateInfo depthCreateInfo = {};
	depthCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthCreateInfo.depthTestEnable = VK_TRUE;
	depthCreateInfo.depthWriteEnable = VK_TRUE;
	depthCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS; // less = close
	depthCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthCreateInfo.minDepthBounds = 0.;
	depthCreateInfo.maxDepthBounds = 1.;
	// Used to activate stencil buffer operations
	depthCreateInfo.stencilTestEnable = VK_FALSE;
	depthCreateInfo.front = {};
	depthCreateInfo.back = {};

	/// Rasterizer
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizerCreateInfo.lineWidth = 1.;
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizerCreateInfo.frontFace =
			VK_FRONT_FACE_COUNTER_CLOCKWISE; // Counter clockwise is necessary to
	// change Coordinate system
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;

	/// Multisampling
	VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
	multisamplingCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
	multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	/// Color blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {};
	colorBlendCreateInfo.sType =
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.attachmentCount = 1;
	colorBlendCreateInfo.pAttachments = &colorBlendAttachment;

	/// Pipeline layout (used to specify uniform data)
	{
		VkDescriptorSetLayout layouts[] = { cameraDescriptorSetLayout,
			meshesDescriptorSetLayout,
			meshImagesDescriptorSetLayout };
		VkPipelineLayoutCreateInfo layoutCreateInfo = {};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 3;
		layoutCreateInfo.pSetLayouts = layouts;

		if (VK_SUCCESS != vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr,
								  &pipelineLayout)) {
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
	pipelineCreateInfo.pDepthStencilState = &depthCreateInfo;

	// Specifies which subpass of render pass use this pipeline
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	VkResult res =
			vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
					nullptr, &graphicsPipeline);
	if (res != VK_SUCCESS) {
		print("[ERROR] Pipeline creation failed");
		return false;
	}

	print("[INFO] Pipeline created");
	return true;
}

void VulkanServer::destroyGraphicsPipelines() {

	if (graphicsPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		graphicsPipeline = VK_NULL_HANDLE;
		print("[INFO] pipeline destroyed");
	}

	if (pipelineLayout != VK_NULL_HANDLE) {
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

VkShaderModule VulkanServer::createShaderModule(vector<char> &shaderBytecode) {

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	if (shaderBytecode.size()) {
		createInfo.codeSize = shaderBytecode.size();
		createInfo.pCode =
				reinterpret_cast<const uint32_t *>(shaderBytecode.data());
	} else {
		createInfo.codeSize = 0;
		createInfo.pCode = nullptr;
	}

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) ==
			VK_SUCCESS) {
		print("[INFO] shader module created");
		return shaderModule;
	} else {
		return VK_NULL_HANDLE;
	}
}

void VulkanServer::destroyShaderModule(VkShaderModule &shaderModule) {
	if (shaderModule == VK_NULL_HANDLE)
		return;

	vkDestroyShaderModule(device, shaderModule, nullptr);
	shaderModule = VK_NULL_HANDLE;
	print("[INFO] shader module destroyed");
}

bool VulkanServer::createFramebuffers() {

	swapchainFramebuffers.resize(swapchainImageViews.size());

	bool error = false;
	VkImageView attachments[2];
	for (int i = swapchainFramebuffers.size() - 1; 0 <= i; --i) {

		// I'm using a single depthImageView since only one depth is drawen at a
		// time due to semaphore set
		attachments[0] = swapchainImageViews[i];
		attachments[1] = depthImageView;

		VkFramebufferCreateInfo bufferCreateInfo = {};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		bufferCreateInfo.renderPass = renderPass;
		bufferCreateInfo.attachmentCount = 2;
		bufferCreateInfo.pAttachments = attachments;
		bufferCreateInfo.width = swapchainExtent.width;
		bufferCreateInfo.height = swapchainExtent.height;
		bufferCreateInfo.layers = 1;

		VkResult res = vkCreateFramebuffer(device, &bufferCreateInfo, nullptr,
				&swapchainFramebuffers[i]);
		if (res != VK_SUCCESS) {
			swapchainFramebuffers[i] = VK_NULL_HANDLE;
			error = true;
		}
	}

	print("[INFO] Framebuffers created");
	return true;
}

void VulkanServer::destroyFramebuffers() {

	for (int i = swapchainFramebuffers.size() - 1; 0 <= i; --i) {

		if (swapchainFramebuffers[i] == VK_NULL_HANDLE)
			continue;

		vkDestroyFramebuffer(device, swapchainFramebuffers[i], nullptr);
	}

	swapchainFramebuffers.clear();
}

bool VulkanServer::createBufferMemoryDeviceAllocator() {

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = device;
	allocatorCreateInfo.preferredLargeHeapBlockSize =
			1ull * 1024 * 1024 * 1024; // 1 GB

	if (VK_SUCCESS !=
			vmaCreateAllocator(&allocatorCreateInfo, &bufferMemoryDeviceAllocator)) {
		print("[ERROR] Vertex buffer creation of VMA allocator failed");
		return false;
	}

	return true;
}

void VulkanServer::destroyBufferMemoryDeviceAllocator() {
	vmaDestroyAllocator(bufferMemoryDeviceAllocator);
	bufferMemoryDeviceAllocator = VK_NULL_HANDLE;
}

bool VulkanServer::createBufferMemoryHostAllocator() {

	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.physicalDevice = physicalDevice;
	allocatorCreateInfo.device = device;

	if (VK_SUCCESS !=
			vmaCreateAllocator(&allocatorCreateInfo, &bufferMemoryHostAllocator)) {
		print("[ERROR] Vertex buffer creation of VMA allocator failed");
		return false;
	}

	return true;
}

void VulkanServer::destroyBufferMemoryHostAllocator() {
	vmaDestroyAllocator(bufferMemoryHostAllocator);
	bufferMemoryHostAllocator = VK_NULL_HANDLE;
}

int32_t VulkanServer::chooseMemoryType(uint32_t p_typeBits,
		VkMemoryPropertyFlags p_propertyFlags) {

	VkPhysicalDeviceMemoryProperties memoryProps;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProps);

	for (int i = memoryProps.memoryTypeCount - 1; 0 <= i; --i) {
		// Checks if the current type of memory is suitable for this buffer
		if (p_typeBits & (1 << i)) {
			if ((memoryProps.memoryTypes[i].propertyFlags & p_propertyFlags) ==
					p_propertyFlags) {
				return i;
			}
		}
	}

	return -1;
}

bool VulkanServer::createUniformBuffers() {

	if (!createBuffer(bufferMemoryHostAllocator, sizeof(SceneUniformBufferObject),
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_SHARING_MODE_EXCLUSIVE, VMA_MEMORY_USAGE_CPU_TO_GPU,
				sceneUniformBuffer, sceneUniformBufferAllocation)) {

		print("[ERROR] Scene uniform buffer allocation failed");
		return false;
	}

	// The below line of code is a bit operation that take correct multiple of
	// physicalDeviceMinUniformBufferOffsetAlignment to use as alingment offset It
	// works only when the physicalDeviceMinUniformBufferOffsetAlignment is a
	// multiple of 2 (That in this case we can stay sure about it)
	//
	// Takes only the most significant bit of "sizeof(MeshUniformBufferObject) +
	// physicalDeviceMinUniformBufferOffsetAlignment -1" that is for sure a
	// multiple of physicalDeviceMinUniformBufferOffsetAlignment and a multiple of
	// 2
	meshDynamicUniformBufferOffset =
			(sizeof(MeshUniformBufferObject) +
					physicalDeviceMinUniformBufferOffsetAlignment - 1) &
			~(physicalDeviceMinUniformBufferOffsetAlignment - 1);

	if (!createBuffer(bufferMemoryHostAllocator,
				meshDynamicUniformBufferOffset * MAX_MESH_COUNT,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_SHARING_MODE_EXCLUSIVE, VMA_MEMORY_USAGE_CPU_TO_GPU,
				meshUniformBufferData.meshUniformBuffer,
				meshUniformBufferData.meshUniformBufferAllocation)) {

		print("[ERROR] Mesh uniform buffer allocation failed, mesh not added");
		return false;
	}

	meshUniformBufferData.size = MAX_MESH_COUNT;
	meshUniformBufferData.count = 0;

	print("[INFO] uniform buffers allocation success");
	return true;
}

void VulkanServer::destroyUniformBuffers() {
	destroyBuffer(bufferMemoryHostAllocator,
			meshUniformBufferData.meshUniformBuffer,
			meshUniformBufferData.meshUniformBufferAllocation);
	destroyBuffer(bufferMemoryHostAllocator, sceneUniformBuffer,
			sceneUniformBufferAllocation);
	print("[INFO] All buffers was freed");
}

bool VulkanServer::createUniformPools() {

	{ // Camera uniform buffer pool
		VkDescriptorPoolSize poolSize = {};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = &poolSize;
		poolCreateInfo.maxSets = 1;

		VkResult res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr,
				&cameraDescriptorPool);
		if (res != VK_SUCCESS) {
			print("[ERROR] Error during creation of camera descriptor pool");
			return false;
		}
	}

	{ // Per Mesh uniform buffer pool
		VkDescriptorPoolSize poolSize = {};
		// Mesh dynamic buffer uniform
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = &poolSize;
		poolCreateInfo.maxSets = 1;

		VkResult res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr,
				&meshesDescriptorPool);
		if (res != VK_SUCCESS) {
			print("[ERROR] Error during creation of meshes descriptor pool");
			return false;
		}
	}

	{ // Per image uniform buffer pool
		VkDescriptorPoolSize poolSize = {};
		// Image and sampler uniform
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = MAX_MESH_COUNT; // one texture for mesh

		VkDescriptorPoolCreateInfo poolCreateInfo = {};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = &poolSize;
		poolCreateInfo.maxSets = MAX_MESH_COUNT;

		VkResult res = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr,
				&meshImagesDescriptorPool);
		if (res != VK_SUCCESS) {
			print("[ERROR] Error during creation of meshes descriptor pool");
			return false;
		}
	}

	print("[INFO] Uniform pools created");
	return true;
}

void VulkanServer::destroyUniformPools() {
	if (meshImagesDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, meshImagesDescriptorPool, nullptr);
		meshImagesDescriptorPool = VK_NULL_HANDLE;
		print("[INFO] Mesh images uniform pool destroyed");
	}
	if (cameraDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, cameraDescriptorPool, nullptr);
		cameraDescriptorPool = VK_NULL_HANDLE;
		print("[INFO] Camera uniform pool destroyed");
	}

	if (meshesDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, meshesDescriptorPool, nullptr);
		meshesDescriptorPool = VK_NULL_HANDLE;
		print("[INFO] Meshes uniform pool destroyed");
	}
}

bool VulkanServer::allocateConfigureCameraDescriptorSet() {

	// Define the structure of camera uniform buffer
	VkDescriptorSetAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocationInfo.descriptorPool = cameraDescriptorPool;
	allocationInfo.descriptorSetCount = 1;
	allocationInfo.pSetLayouts = &cameraDescriptorSetLayout;

	VkResult res =
			vkAllocateDescriptorSets(device, &allocationInfo, &cameraDescriptorSet);
	if (res != VK_SUCCESS) {
		print("[ERROR] Allocation of descriptor set failed");
		return false;
	}

	// Update descriptor allocated with buffer
	VkDescriptorBufferInfo cameraBufferInfo = {};
	cameraBufferInfo.buffer = sceneUniformBuffer;
	cameraBufferInfo.offset = 0;
	cameraBufferInfo.range = sizeof(SceneUniformBufferObject);

	VkWriteDescriptorSet cameraWriteDescriptor = {};
	cameraWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cameraWriteDescriptor.dstSet = cameraDescriptorSet;
	cameraWriteDescriptor.dstBinding = 0;
	cameraWriteDescriptor.dstArrayElement = 0;
	cameraWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraWriteDescriptor.descriptorCount = 1;
	cameraWriteDescriptor.pBufferInfo = &cameraBufferInfo;

	vkUpdateDescriptorSets(device, 1, &cameraWriteDescriptor, 0, nullptr);

	print("[INFO] camera descriptor set created");
	return true;
}

bool VulkanServer::allocateConfigureMeshesDescriptorSet() {

	// Allocate dynamic buffer of meshes
	VkDescriptorSetAllocateInfo allocationInfo = {};
	allocationInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocationInfo.descriptorPool = meshesDescriptorPool;
	allocationInfo.descriptorSetCount = 1;
	allocationInfo.pSetLayouts = &meshesDescriptorSetLayout;

	VkResult res = vkAllocateDescriptorSets(device, &allocationInfo, &meshesDescriptorSet);
	if (res != VK_SUCCESS) {
		print("[ERROR] Allocation of descriptor set failed");
		return false;
	}

	// Update the descriptor with dynamic buffer
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

bool VulkanServer::createCommandPool() {
	QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkResult res = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr,
			&graphicsCommandPool);
	if (res != VK_SUCCESS) {
		print("[ERROR] Command pool creation failed");
		return false;
	}

	print("[INFO] Command pool craeted");
	return true;
}

void VulkanServer::destroyCommandPool() {
	if (graphicsCommandPool == VK_NULL_HANDLE)
		return;

	vkDestroyCommandPool(device, graphicsCommandPool, nullptr);
	graphicsCommandPool = VK_NULL_HANDLE;
	print("[INFO] Command pool destroyed");

	// Since the command buffers are destroyed by this function here I want clear
	// it
	drawCommandBuffers.clear();
	copyCommandBuffer = VK_NULL_HANDLE;
}

bool VulkanServer::allocateCommandBuffers() {
	// Doesn't require destructions (it's performed automatically during the
	// destruction of command pool)

	drawCommandBuffers.resize(swapchainImages.size());

	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = graphicsCommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t)drawCommandBuffers.size();

	if (VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, drawCommandBuffers.data())) {
		print("[ERROR] Draw Command buffer allocation fail");
		return false;
	}

	allocateInfo.commandBufferCount = 1;
	if (VK_SUCCESS != vkAllocateCommandBuffers(device, &allocateInfo, &copyCommandBuffer)) {
		print("[ERROR] Copy command buffer allocation failed");
		return false;
	}

	print("[INFO] command buffers allocated");
	return true;
}

void VulkanServer::beginCommandBuffers() {

	vkWaitForFences(device, drawFinishFences.size(), drawFinishFences.data(), VK_TRUE, LONGTIMEOUT_NANOSEC);
	for (int i = drawCommandBuffers.size() - 1; 0 <= i; --i) {

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		// Begin command buffer
		vkBeginCommandBuffer(drawCommandBuffers[i], &beginInfo);

		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = swapchainFramebuffers[i];
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = swapchainExtent;
		VkClearValue clearValues[2];
		clearValues[0].color = { 0., 0., 0., 1. };
		clearValues[1].depthStencil = { 1., 0 }; // 1. Mean the furthest distance
				// possible in the depth buffer that
				// go from 0 to 1
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		// Begin render pass
		vkCmdBeginRenderPass(drawCommandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Bind graphics pipeline
		vkCmdBindPipeline(drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		if (meshes.size() > 0) {
			// 0 camera, 1 mesh, 2 mesh images
			VkDescriptorSet descriptorSets[] = { cameraDescriptorSet, VK_NULL_HANDLE, VK_NULL_HANDLE };
			// Bind buffers
			for (int m = 0, s = meshes.size(); m < s; ++m) {
				MeshHandle *mh = meshes[m];
				descriptorSets[1] = meshesDescriptorSet; // TODOD set here the right descriptor set
				descriptorSets[2] = mh->imageDescriptorSet;
				uint32_t dynamicOffset = mh->meshUniformBufferOffset * meshDynamicUniformBufferOffset;

				vkCmdBindDescriptorSets(drawCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 3, descriptorSets, 1, &dynamicOffset);
				vkCmdBindVertexBuffers(drawCommandBuffers[i], 0, 1, &mh->vertexBuffer, &mh->verticesBufferOffset);
				vkCmdBindIndexBuffer(drawCommandBuffers[i], mh->indexBuffer, mh->indicesBufferOffset, VK_INDEX_TYPE_UINT32);
				vkCmdDrawIndexed(drawCommandBuffers[i], mh->mesh->getCountIndices(), 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(drawCommandBuffers[i]);

		if (VK_SUCCESS != vkEndCommandBuffer(drawCommandBuffers[i])) {
			print("[ERROR - not handled] end render pass failed");
		}
	}
	print("[INFO] Command buffers initializated");
}

bool VulkanServer::createSyncObjects() {

	VkResult res;

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Create fences
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	renderFinishedSemaphores.resize(swapchainImages.size());
	drawFinishFences.resize(swapchainImages.size());

	bool success = true;
	for (int i = swapchainImages.size() - 1; 0 <= i; --i) {
		// Create semaphores

		res = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
				&renderFinishedSemaphores[i]);
		if (res != VK_SUCCESS) {
			print("[ERROR] Semaphore creation failed");
			success = false;
			renderFinishedSemaphores[i] = VK_NULL_HANDLE;
		}

		res =
				vkCreateFence(device, &fenceCreateInfo, nullptr, &drawFinishFences[i]);
		if (res != VK_SUCCESS) {
			print("[ERROR] Draw fences creation failed");
			success = false;
			drawFinishFences[i] = VK_NULL_HANDLE;
		}
	}

	if (!success) {
		return false;
	}

	res = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr,
			&imageAvailableSemaphore);
	if (res != VK_SUCCESS) {
		print("[ERROR] Semaphore creation failed");
		return false;
	}

	res = vkCreateFence(device, &fenceCreateInfo, nullptr, &copyFinishFence);
	if (res != VK_SUCCESS) {
		print("[ERROR] Copy fences creation failed");
		return false;
	}

	print("[INFO] Semaphores and Fences created");

	return true;
}

void VulkanServer::destroySyncObjects() {

	for (int i = swapchainImages.size() - 1; 0 <= i; --i) {

		if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);

		if (drawFinishFences[i] != VK_NULL_HANDLE)
			vkDestroyFence(device, drawFinishFences[i], nullptr);
	}

	renderFinishedSemaphores.clear();
	drawFinishFences.clear();

	if (imageAvailableSemaphore != VK_NULL_HANDLE) {
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
		imageAvailableSemaphore = VK_NULL_HANDLE;
	}

	if (copyFinishFence != VK_NULL_HANDLE) {
		vkDestroyFence(device, copyFinishFence, nullptr);
		copyFinishFence = VK_NULL_HANDLE;
	}

	print("[INFO] Semaphores and Fences destroyed");
}

void VulkanServer::reloadCamera() {
	camera.setAspect(swapchainExtent.width, swapchainExtent.height);
}

void VulkanServer::removeAllMeshes() {
	meshesCopyPending.clear();

	for (int m = meshes.size() - 1; 0 <= m; --m) {
		removeMesh(meshes[m]);
	}
	meshes.clear();
	print("[INFO] All meshes removed from scene");
}

bool checkExtensionsSupport(const vector<const char *> &p_requiredExtensions,
		vector<VkExtensionProperties> availableExtensions,
		bool verbose = true) {
	bool missing = false;
	for (size_t i = 0; i < p_requiredExtensions.size(); ++i) {
		bool found = false;
		for (size_t j = 0; j < availableExtensions.size(); ++j) {
			if (strcmp(p_requiredExtensions[i],
						availableExtensions[j].extensionName) == 0) {
				found = true;
				break;
			}
		}

		if (verbose)
			print(string("	extension: ") + string(p_requiredExtensions[i]) +
					string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if (found)
			missing = true;
	}
	return missing;
}

bool VulkanServer::checkInstanceExtensionsSupport(
		const vector<const char *> &p_requiredExtensions) {
	uint32_t availableExtensionsCount = 0;
	vector<VkExtensionProperties> availableExtensions;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount,
			nullptr);
	availableExtensions.resize(availableExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount,
			availableExtensions.data());

	print("Checking if required extensions are available");
	return checkExtensionsSupport(p_requiredExtensions, availableExtensions);
}

bool VulkanServer::checkValidationLayersSupport(
		const vector<const char *> &p_layers) {

	uint32_t availableLayersCount = 0;
	vector<VkLayerProperties> availableLayers;
	vkEnumerateInstanceLayerProperties(&availableLayersCount, nullptr);
	availableLayers.resize(availableLayersCount);
	vkEnumerateInstanceLayerProperties(&availableLayersCount,
			availableLayers.data());

	print("Checking if required validation layers are available");
	bool missing = false;
	for (int i = p_layers.size() - 1; i >= 0; --i) {
		bool found = false;
		for (size_t j = 0; j < availableLayersCount; ++j) {
			if (strcmp(p_layers[i], availableLayers[j].layerName) == 0) {
				found = true;
				break;
			}
		}

		print(string("	layer: ") + string(p_layers[i]) +
				string(found ? " [Available]" : " [NOT AVAILABLE]"));
		if (found)
			missing = true;
	}
	return missing;
}

int VulkanServer::autoSelectPhysicalDevice(
		const vector<VkPhysicalDevice> &p_devices,
		VkPhysicalDeviceType p_deviceType,
		VkPhysicalDeviceProperties *r_deviceProps) {

	print("[INFO] Select physical device");

	for (int i = p_devices.size() - 1; 0 <= i; --i) {

		// Check here the device
		VkPhysicalDeviceFeatures deviceFeatures;
		uint32_t extensionsCount;

		vkGetPhysicalDeviceProperties(p_devices[i], r_deviceProps);
		vkGetPhysicalDeviceFeatures(p_devices[i], &deviceFeatures);

		vkEnumerateDeviceExtensionProperties(p_devices[i], nullptr,
				&extensionsCount, nullptr);

		vector<VkExtensionProperties> availableExtensions(extensionsCount);
		vkEnumerateDeviceExtensionProperties(
				p_devices[i], nullptr, &extensionsCount, availableExtensions.data());

		if (r_deviceProps->deviceType != p_deviceType)
			continue;

		if (!deviceFeatures.geometryShader || !deviceFeatures.samplerAnisotropy)
			continue;

		if (!findQueueFamilies(p_devices[i]).isComplete())
			continue;

		if (!checkExtensionsSupport(deviceExtensions, availableExtensions, false))
			continue;

		// Here we are sure that the extensions are available in that device, so now
		// we can check swap chain
		SwapChainSupportDetails swapChainDetails =
				querySwapChainSupport(p_devices[i]);
		if (swapChainDetails.formats.empty() ||
				swapChainDetails.presentModes.empty())
			continue;

		return i;
	}
	return -1;
}

void VulkanServer::recreateSwapchain() {
	waitIdle();

	// Recreate swapchain
	destroySwapchain();
	createSwapchain();
	reloadCamera();
	reloadDrawCommandBuffer = true;
}

bool VulkanServer::createBuffer(VmaAllocator p_allocator, VkDeviceSize p_size,
		VkBufferUsageFlags p_usage,
		VkSharingMode p_sharingMode,
		VmaMemoryUsage p_memoryUsage,
		VkBuffer &r_buffer,
		VmaAllocation &r_allocation) {

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = p_size;
	bufferCreateInfo.usage = p_usage;
	bufferCreateInfo.sharingMode = p_sharingMode;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = p_memoryUsage;

	VmaAllocationInfo allocationInfo = {};

	if (VK_SUCCESS != vmaCreateBuffer(p_allocator, &bufferCreateInfo,
							  &allocationCreateInfo, &r_buffer,
							  &r_allocation, &allocationInfo)) {
		print("[ERROR] failed to allocate memory");
		return false;
	}

	return allocationInfo.size >= p_size;
}

void VulkanServer::destroyBuffer(VmaAllocator p_allocator, VkBuffer &r_buffer,
		VmaAllocation &r_allocation) {
	vmaDestroyBuffer(p_allocator, r_buffer, r_allocation);
	r_buffer = VK_NULL_HANDLE;
	r_allocation = VK_NULL_HANDLE;
}

bool VulkanServer::hasStencilComponent(VkFormat p_format) {
	return p_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
		   p_format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat VulkanServer::findBestDepthFormat() {
	VkFormat out;
	if (chooseBestSupportedFormat(
				{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
						VK_FORMAT_D24_UNORM_S8_UINT },
				VK_IMAGE_TILING_OPTIMAL,
				VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, &out)) {
		return out;
	}

	// TODO Really bad error handling
	return VK_FORMAT_D32_SFLOAT;
}

bool VulkanServer::chooseBestSupportedFormat(const vector<VkFormat> &p_formats,
		VkImageTiling p_tiling,
		VkFormatFeatureFlags p_features,
		VkFormat *r_format) {
	VkFormatProperties props;
	for (int i = 0, s = p_formats.size(); i < s; ++i) {
		vkGetPhysicalDeviceFormatProperties(physicalDevice, p_formats[i], &props);

		if (p_tiling == VK_IMAGE_TILING_LINEAR) {
			if ((props.linearTilingFeatures & p_features) == p_features) {
				*r_format = p_formats[i];
				return true;
			}
		} else if (p_tiling == VK_IMAGE_TILING_OPTIMAL) {
			if ((props.optimalTilingFeatures & p_features) == p_features) {
				*r_format = p_formats[i];
				return true;
			}
		}
	}
	print("[ERROR - not handled] Not able to find supported format");
	return false;
}

bool VulkanServer::createImage(uint32_t p_width, uint32_t p_height,
		VkFormat p_format, VkImageTiling p_tiling,
		VkImageUsageFlags p_usage,
		VkMemoryPropertyFlags p_memoryFlags,
		VkImage &r_image, VkDeviceMemory &r_memory) {
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = p_format;
	imageCreateInfo.extent.width = p_width;
	imageCreateInfo.extent.height = p_height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.tiling = p_tiling;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = p_usage;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (VK_SUCCESS !=
			vkCreateImage(device, &imageCreateInfo, nullptr, &r_image)) {
		return false;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, r_image, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex =
			chooseMemoryType(memoryRequirements.memoryTypeBits, p_memoryFlags);

	if (VK_SUCCESS !=
			vkAllocateMemory(device, &memoryAllocInfo, nullptr, &r_memory)) {
		return false;
	}

	vkBindImageMemory(device, r_image, r_memory, /*Offset*/ 0);
	return true;
}

void VulkanServer::destroyImage(VkImage &p_image, VkDeviceMemory &p_memory) {
	if (VK_NULL_HANDLE == device)
		return;
	vkDestroyImage(device, p_image, nullptr);
	vkFreeMemory(device, p_memory, nullptr);
	p_image = VK_NULL_HANDLE;
	p_memory = VK_NULL_HANDLE;
}

bool VulkanServer::createImageView(VkImage p_image, VkFormat p_format,
		VkImageAspectFlags p_aspectFlags,
		VkImageView &r_imageView) {

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

	if (VK_SUCCESS !=
			vkCreateImageView(device, &viewCreateInfo, nullptr, &r_imageView)) {
		return false;
	}

	return true;
}

void VulkanServer::destroyImageView(VkImageView &r_imageView) {
	if (VK_NULL_HANDLE == device)
		return;
	vkDestroyImageView(device, r_imageView, nullptr);
	r_imageView = VK_NULL_HANDLE;
}

bool VulkanServer::allocateCommand(VkCommandBuffer &r_command) {
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = graphicsCommandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	if (VK_SUCCESS !=
			vkAllocateCommandBuffers(device, &allocateInfo, &r_command)) {
		return false;
	}

	return true;
}

void VulkanServer::beginOneTimeCommand(VkCommandBuffer p_command) {
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(p_command, &beginInfo);
}

bool VulkanServer::endCommand(VkCommandBuffer p_command) {
	if (VK_SUCCESS != vkEndCommandBuffer(p_command)) {
		return false;
	}
	return true;
}

bool VulkanServer::submitCommand(VkCommandBuffer p_command, VkFence p_fence) {

	if (VK_NULL_HANDLE != p_fence) {
		if (VK_SUCCESS != vkResetFences(device, 1, &p_fence)) {
			return false;
		}
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &p_command;

	if (VK_SUCCESS != vkQueueSubmit(graphicsQueue, 1, &submitInfo, p_fence)) {
		return false;
	}
	return true;
}

bool VulkanServer::submitWaitCommand(VkCommandBuffer p_command) {

	VkFence fence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	if (VK_SUCCESS != vkCreateFence(device, &fenceCreateInfo, nullptr, &fence)) {
		print("[ERROR] fence creation failed");
		return false;
	}

	bool success = true;
	if (submitCommand(p_command, fence)) {
		if (VK_SUCCESS !=
				vkWaitForFences(device, 1, &fence, VK_TRUE, LONGTIMEOUT_NANOSEC)) {
			print("[ERROR] Fence wait error");
			success = false;
		}
	} else {
		print("[ERROR] submit command error");
		success = false;
	}

	vkDestroyFence(device, fence, nullptr);

	return success;
}

void VulkanServer::freeCommand(VkCommandBuffer &r_command) {
	if (VK_NULL_HANDLE == device)
		return;
	vkFreeCommandBuffers(device, graphicsCommandPool, 1, &r_command);
	r_command = VK_NULL_HANDLE;
}

bool VulkanServer::transitionImageLayout(VkImage p_image, VkFormat p_format,
		VkImageLayout p_oldLayout,
		VkImageLayout p_newLayout) {

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = p_oldLayout;
	barrier.newLayout = p_newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = p_image;

	if (p_newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(p_format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	} else {
		barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	if (p_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			p_newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

		barrier.srcAccessMask = 0; // Because from undefined
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	} else if (p_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			   p_newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {

		barrier.srcAccessMask = 0; // Because from undefined
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
								VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

	} else if (p_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			   p_newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	} else {
		print("[ERROR] layout transfer not supported ");
		return false;
	}

	VkCommandBuffer command;
	if (!allocateCommand(command)) {
		print("[ERROR] command allocation failed, Image transition layout failed");
		return false;
	}

	beginOneTimeCommand(command);

	vkCmdPipelineBarrier(command, srcStage, dstStage, 0, 0, nullptr, 0, nullptr,
			1, &barrier);

	bool success = true;
	if (endCommand(command)) {
		if (!submitWaitCommand(command)) {
			print("[ERROR] command submit failed, Image transition layout failed");
			success = false;
		}
	} else {
		print("[ERROR] command ending failed, Image transition layout failed");
		success = false;
	}
	freeCommand(command);
	return success;
}

VisualServer::VisualServer(WindowServer *p_window) :
		defaultTexture(nullptr),
		window_server(p_window),
		vulkanServer(this) {}

VisualServer::~VisualServer() {}

bool VisualServer::init() {

	if (!createWindow())
		return false;

	const bool vulkanState = vulkanServer.create(window_server);

	if (vulkanState) {
		defaultTexture = new Texture(&vulkanServer);
		defaultTexture->load("assets/default.png");
	}

	return vulkanState;
}

void VisualServer::terminate() {
	delete defaultTexture;
	vulkanServer.destroy();
	freeWindow();
}

bool VisualServer::can_step() {
	return window_server->isDrawable();
}

void VisualServer::step() {
	{
		while (window_server->poolEvents()) {
			if (window_server->wantToQuit())
				window_server->set_drawable(false);
		}
	}

	vulkanServer.draw();
}

void VisualServer::addMesh(Mesh *p_mesh) {
	vulkanServer.addMesh(p_mesh);
}

void VisualServer::removeMesh(Mesh *p_mesh) {
	vulkanServer.removeMesh(p_mesh);
}

bool VisualServer::createWindow() {
	return window_server->instanceWindow("Hello Vulkan", INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
}

void VisualServer::freeWindow() {
	window_server->freeWindow();
}
