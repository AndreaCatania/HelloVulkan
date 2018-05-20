#pragma once

#include "hellovulkan.h"
#include <chrono>

class VisualServer;
class Window;
class Mesh;
struct MeshHandle;
class Texture;

// RENDER IMAGE PROCESS
// |
// |-> Check if there are pending meshes to insert in the scene
// |-> Request next image from swapchain <-----------------------------<------------------\
// |-> Submit commandBuffer of image returned by swapchain to GraphicsQueue				  |
// |				^																	  |
// |				|---------------<---------------------------------------<-------------+-----\
// |																					  |     |
// |-> Present rendered image by putting it in PresentationQueue						  |     |
//																						  |     |
//																						  ^     |
//																						  |     |
//																						  |     ^
//																						  |     |
// DEPENDENCY SCHEME OF: Swapchain, Renderpass, Pipeline, Framebuffer, Commandbuffer	  |     |
//																						  |     |
// Command buffer                    Swapchain >------------------------------->----------/     |
//     |                                |														|
//     |-> Frame buffer         |-------/														|
//	   |        |               v																|
//     |		\-> Swapchain image view														|
//     |																						|
//     |-> Render pass <----------------\														|
//     |								|														|
//     |-> Pipelines >-----------> regulate execution of pipeline by subpass					^
//     |-> [User command]																		|
//     |																						|
//     V																						|
//   Ready to be submitted to																	|
//	 Graphycs queue >-------------------->----------------------------------------->------------/
//
//
//
// Memory allocation is managed using library VulkanMemoryAllocator:
// https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
//
//
//	VERTEX BUFFER
//     Contains all informations of vertices of a mesh
//
//	INDEX BUFFER
//		Contains the indices (ordered) that compose all triangles of
// mesh. 		The GPU submit the vertex data of VERTEX BUFFER to the
// shader according to the in	dex of INDEX BUFFER
//
//	IMAGE LAYOUT
//		The Layout of image tell the GPU how to store it in order to
// obtain the max performance for a particular operation, The layout of image
// can  be changed or better to say transitioned 		The transition can
// be performed using a barrier where the transition is specified in the
// VkImageMemoryBarrier structure
//
//	VULKAN COORDINATE SYSTEM
//		is right hand with positive Y that go down
//		The depth = 1 is Z positive (far is positive)
//
//       -Y
//        |
//   -X --Z-- +X
//        |
//       +Y
//
//	UNIFORM BUFFERS and IMAGES
//		Uniform buffer is the way how to submit resources / data to the
// shaders. 		The image can be submitted in the same way of Uniform
// buffer, but it's special object that is used specifically for textures.
//		The uniform buffer / image should be declared in the pipeline by
// useing set layout that is a description of it. 		Then is required
// to create a pool that is used to allocate descriptor set that is the object
// that refer to a particular resource
//		it's possible to change the resource by using
// vkUpdateDescriptorSets 			. VkDescriptorSetLayout - Opaque
// object that has some information about how is composed a uniform object
//			. VkDescriptorPool - The pool used to allocate
// DescriptorSet 			. VkDescriptorSet - Hold reference and
// description to buffers of set (buffer 0, buffer 1, etc..)

struct SceneUniformBufferObject {
	glm::mat4 cameraView;
	glm::mat4 cameraViewInverse;
	glm::mat4 cameraProjection;
};

struct MeshUniformBufferObject {
	glm::mat4 model;
};

/// Camera look along -Z
class Camera {
	friend class VulkanServer;

	glm::mat4 transform;
	glm::mat4 projection;
	float aspect;
	float FOV;
	float near;
	float far;

	bool isDirty;
	bool isProjectionDirty;

public:
	Camera();

	void lookAt(const glm::vec3 &p_pos, const glm::vec3 &p_target);
	void setTransform(const glm::mat4 &p_transform);
	const glm::mat4 &getTransform() const { return transform; }

	const glm::mat4 &getProjection() const;

	void reloadProjection();
	void setAspect(uint32_t p_width, uint32_t p_height);
	void setFOV_deg(float p_FOV_deg);
	void setNearFar(float p_near, float p_far);
};

class VulkanServer {
public:
	friend class Texture;
	friend class MeshHandle;

	static const glm::mat4 COORDSYSTEMROTATOR;

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

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		vector<VkSurfaceFormatKHR> formats;
		vector<VkPresentModeKHR> presentModes;
	};

	VulkanServer(VisualServer *p_visualServer);

	bool enableValidationLayer();

	bool create(Window *p_window);
	void destroy();

	void waitIdle();

	void draw();

	void addMesh(Mesh *p_mesh);
	void removeMesh(Mesh *p_mesh);
	void removeMesh(MeshHandle *p_meshHandle);

	Camera &getCamera() { return camera; }

public:
	void processCopy();
	void updateUniformBuffers();

	bool createImageLoadBuffer(VkDeviceSize p_size, VkBuffer &r_buffer, VmaAllocation &r_allocation, VmaAllocator &r_allocator);
	bool createImageTexture(uint32_t p_width, uint32_t p_height, VkImage &r_image, VkDeviceMemory &r_memory);
	bool createImageViewTexture(VkImage p_image, VkImageView &r_imageView);

private:
	Window *window;
	VisualServer *visualServer;

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice; // Destroyed automatically when instance is cleared
	VkDeviceSize physicalDeviceMinUniformBufferOffsetAlignment;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSwapchainKHR swapchain;

	vector<const char *> layers;
	vector<const char *> deviceExtensions;

	vector<VkImage> swapchainImages;

	VkFormat swapchainImageFormat;
	VkExtent2D swapchainExtent;

	vector<VkImageView> swapchainImageViews;

	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkRenderPass renderPass;

	// Used to store camera informations
	VkDescriptorSetLayout cameraDescriptorSetLayout;
	VkDescriptorPool cameraDescriptorPool;
	VkDescriptorSet cameraDescriptorSet;

	// Used to store mesh information in a dynamic buffer
	VkDescriptorSetLayout meshesDescriptorSetLayout;
	VkDescriptorPool meshesDescriptorPool;
	VkDescriptorSet meshesDescriptorSet;

	// Used to store mesh textures
	VkDescriptorSetLayout meshImagesDescriptorSetLayout;
	VkDescriptorPool meshImagesDescriptorPool;

	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	vector<VkFramebuffer> swapchainFramebuffers;

	VmaAllocator bufferMemoryDeviceAllocator;

	VmaAllocator bufferMemoryHostAllocator;

	VkBuffer sceneUniformBuffer;
	VmaAllocation sceneUniformBufferAllocation;

	struct DynamicMeshUniformBufferData {
		VkBuffer meshUniformBuffer;
		VmaAllocation meshUniformBufferAllocation;
		uint32_t count;
		uint32_t size;

		DynamicMeshUniformBufferData() :
				meshUniformBuffer(VK_NULL_HANDLE),
				meshUniformBufferAllocation(VK_NULL_HANDLE),
				count(0),
				size(0) {}
	};

	// Dynamic uniform buffer used to store all mesh informations
	uint32_t meshDynamicUniformBufferOffset;
	DynamicMeshUniformBufferData meshUniformBufferData;

	VkCommandPool graphicsCommandPool;

	vector<VkCommandBuffer> drawCommandBuffers; // Used to draw things
	vector<VkFence> drawFinishFences;
	VkCommandBuffer copyCommandBuffer; // Used to copy data to GPU

	VkSemaphore imageAvailableSemaphore;
	vector<VkSemaphore> renderFinishedSemaphores;

	VkFence copyFinishFence;

private:
	bool reloadDrawCommandBuffer;

	Camera camera;

	vector<MeshHandle *> meshes;
	vector<MeshHandle *> meshesCopyInProgress;
	vector<MeshHandle *> meshesCopyPending;

private:
	bool createInstance();
	void destroyInstance();

	bool createDebugCallback();
	void destroyDebugCallback();

	bool createSurface();
	void destroySurface();

	bool pickPhysicalDevice();

	bool createLogicalDevice();
	void destroyLogicalDevice();

	void lockupDeviceQueue();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice p_device);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice p_device);

	// Helper functions to create the swap chain
	VkSurfaceFormatKHR
	chooseSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats);
	VkPresentModeKHR choosePresentMode(const vector<VkPresentModeKHR> &p_modes);
	VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	// This method is used to create the usable swap chain object
	bool createSwapchain();
	void destroySwapchain();

	// This method is used to create the simple swap chain object
	// The swapchain is the array that is used to hold all information about the
	// image to show.
	bool createRawSwapchain();
	void destroyRawSwapchain();

	void lockupSwapchainImages();

	bool createSwapchainImageViews();
	void destroySwapchainImageViews();

	bool createDepthTestResources();
	void destroyDepthTestResources();

	// The render pass is an object that is used to organize the rendering process
	// It doesn't have any rendering command nor resources informations
	bool createRenderPass();
	void destroyRenderPass();

	// Is the object that describe the uniform buffer
	bool createDescriptorSetLayouts();
	void destroyDescriptorSetLayouts();

	// The pipeline is the object that contains all commands of a particular
	// rendering process Shader, Vertex inputs, viewports, etc.. Each pipiline has
	// a reference to a particular subpass of renderpass The pipeline will be
	// created using renderpass informations The order of execution of particular
	// pipeline depend on the renderpass
	bool createGraphicsPipelines();
	void destroyGraphicsPipelines();

	VkShaderModule createShaderModule(vector<char> &shaderBytecode);
	void destroyShaderModule(VkShaderModule &shaderModule);

	// The framebuffer object represent the memory that will be used by renderpass
	// The framebuffer is created using renderpass informations
	bool createFramebuffers();
	void destroyFramebuffers();

	// Create the allocator that handle the buffer memory VRAM
	bool createBufferMemoryDeviceAllocator();
	void destroyBufferMemoryDeviceAllocator();

	// Create the allocator that handle the buffer memory RAM
	bool createBufferMemoryHostAllocator();
	void destroyBufferMemoryHostAllocator();

	// typeBits indicate the suitable types of memory for the buffer
	int32_t chooseMemoryType(uint32_t p_typeBits, VkMemoryPropertyFlags p_propertyFlags);

	bool createUniformBuffers();
	void destroyUniformBuffers();

	bool createUniformPools();
	void destroyUniformPools();

	bool allocateConfigureCameraDescriptorSet();
	bool allocateConfigureMeshesDescriptorSet();

	// The command pool is an opaque object that is used to allocate command
	// buffers easily
	bool createCommandPool();
	void destroyCommandPool();

	// Command buffer is an object where are stored all commands, here are stored
	// all informations about renderpass, attachments, etc.. It's possible to have
	// two kind of command buffers primary and seconday The secondary can be
	// executed by primary only, the primary can be submitted directly into queue
	// This function only allocate a command buffer and doesn't initialize it
	bool allocateCommandBuffers();

	// This function take all commandBuffers and set it in executable state with
	// all commands to execute This store the renderpass, so it should be
	// submitted each time the swapchain is recreated
	void beginCommandBuffers();

	bool createSyncObjects();
	void destroySyncObjects();

	void reloadCamera();

	void removeAllMeshes();

	bool checkInstanceExtensionsSupport(const vector<const char *> &p_required_extensions);
	bool checkValidationLayersSupport(const vector<const char *> &p_layers);

	// Return the ID in the array of selected device
	int autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_device, VkPhysicalDeviceProperties *r_deviceProps);

private:
	// Helpers
	void recreateSwapchain();

	// return the size of allocated memory, or 0 if error.
	bool createBuffer(VmaAllocator p_allocator, VkDeviceSize p_size, VkBufferUsageFlags p_usage, VkSharingMode p_sharingMode, VmaMemoryUsage p_memoryUsage, VkBuffer &r_buffer, VmaAllocation &r_allocation);
	void destroyBuffer(VmaAllocator p_allocator, VkBuffer &r_buffer, VmaAllocation &r_allocation);

	bool hasStencilComponent(VkFormat p_format);
	VkFormat findBestDepthFormat();

	// Accept the list of format in an ordered list from the best to worst
	// and return one of it if it's supported by the Hardware
	bool chooseBestSupportedFormat(const vector<VkFormat> &p_formats, VkImageTiling p_tiling, VkFormatFeatureFlags p_features, VkFormat *r_format);

	bool createImage(uint32_t p_width, uint32_t p_height, VkFormat p_format, VkImageTiling p_tiling, VkImageUsageFlags p_usage, VkMemoryPropertyFlags p_memoryFlags, VkImage &r_image, VkDeviceMemory &r_memory);
	void destroyImage(VkImage &p_image, VkDeviceMemory &p_memory);

	bool createImageView(VkImage p_image, VkFormat p_format, VkImageAspectFlags p_aspectFlags, VkImageView &r_imageView);
	void destroyImageView(VkImageView &r_imageView);

	// Command Buffers helpers
	bool allocateCommand(VkCommandBuffer &r_commandBuffer);
	void beginOneTimeCommand(VkCommandBuffer p_command);
	bool endCommand(VkCommandBuffer p_command);
	bool submitCommand(VkCommandBuffer p_command, VkFence p_fence);
	bool submitWaitCommand(VkCommandBuffer p_command);
	void freeCommand(VkCommandBuffer &r_command);

	bool transitionImageLayout(VkImage p_image, VkFormat p_format, VkImageLayout p_oldLayout, VkImageLayout p_newLayout);
};

class Window {
public:
	virtual void instanceWindow(const char *p_title, int p_width, int p_height) = 0;
	virtual void freeWindow() = 0;
	virtual bool isDrawable() = 0;
	virtual void getWindowSize(int *r_width, int *r_height) = 0;
	virtual void appendRequiredExtensions(vector<const char *> &r_extensions) = 0;
	virtual bool createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) = 0;
};

class WindowSDL : public Window {
private:
	SDL_Window *window;

public:
	bool running;

public:
	WindowSDL();
	virtual void instanceWindow(const char *p_title, int p_width, int p_height);
	virtual void freeWindow();
	virtual bool isDrawable();
	virtual void getWindowSize(int *r_width, int *r_height);
	virtual void appendRequiredExtensions(vector<const char *> &r_extensions);
	virtual bool createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface);
};

class VisualServer {
	friend class VulkanServer;

	Texture *defaultTexture;
	Window *window;
	VulkanServer vulkanServer;

public:
	VisualServer();
	~VisualServer();

	bool init();
	void terminate();

	bool can_step();
	void step();

	void addMesh(Mesh *p_mesh);
	void removeMesh(Mesh *p_mesh);

	VulkanServer *getVulkanServer() { return &vulkanServer; }
	const Texture *getDefaultTeture() const { return defaultTexture; }

private:
	void createWindow();
	void freeWindow();
};
