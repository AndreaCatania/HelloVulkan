#pragma once

#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include "libs/glm/glm.hpp"
#include "libs/glm/gtc/matrix_transform.hpp"
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <chrono>

#include "libs/vma/vk_mem_alloc.h" // Includes only interfaces

using namespace std;

class GLFWwindow;

// RENDER IMAGE PROCESS
// |
// |-> Check if there are pending meshes to insert in the scene
// |-> Request next image from swapchain <------------------------------------------------\
// |-> Submit commandBuffer of image returned by swapchain to GraphicsQueue               |
// |               ^                                                                      |
// |               |----------------------------------------------------------------------+-----\
// |                                                                                      |     |
// |-> Present rendered image by putting it in PresentationQueue                          |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
// DEPENDENCY SCHEME OF: Swapchain, Renderpass, Pipeline, Framebuffer, Commandbuffer      |     |
//                                                                                        |     |
// Command buffer                    Swapchain >------------------------------------------/     |
//     |                                |                                                       |
//     |-> Frame buffer         |-------/                                                       |
//     |       |                v                                                               |
//     |       \-> Swapchain image view                                                         |
//     |                                                                                        |
//     |-> Render pass <---------------\                                                        |
//     |                               |                                                        |
//     |-> Pipelines >-----------> regulate execution of pipeline by subpass                    |
//     |-> [User command]                                                                       |
//     |                                                                                        |
//     V                                                                                        |
//   Ready to be submitted to                                                                   |
//   Graphycs queue >---------------------------------------------------------------------------/
//
//
//
// Memory allocation is managed using library VulkanMemoryAllocator: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
//
//
//	VERTEX BUFFER
//     Contains all informations of vertices of a mesh
//
//	INDEX BUFFER
//		Contains the indices (ordered) that compose all triangles of mesh.
//		The GPU submit the vertex data of VERTEX BUFFER to the shader according to the in	dex of INDEX BUFFER
//
//	IMAGE LAYOUT
//		The Layout of image tell the GPU how to store it in order to obtain the max performance for a particular operation, The layout of image can be changed or better to say transitioned
//		The transition can be performed using a barrier where the transition is specified in the VkImageMemoryBarrier structure
//

struct Vertex{
	glm::vec3 pos;
	glm::vec4 color;

	static VkVertexInputBindingDescription getBindingDescription(){
		VkVertexInputBindingDescription desc = {};
		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return desc;
	}

	static array<VkVertexInputAttributeDescription, 2> getAttributesDescription(){
		array<VkVertexInputAttributeDescription, 2> attr;
		attr[0].binding = 0;
		attr[0].location = 0;
		attr[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attr[0].offset = offsetof(Vertex, pos);

		attr[1].binding = 0;
		attr[1].location = 1;
		attr[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attr[1].offset = offsetof(Vertex, color);
		return attr;
	}
};

struct Triangle{
	uint32_t vertices[3];
};

class VulkanServer;
struct MeshHandle;

class Mesh{
	friend class VulkanServer;
	unique_ptr<MeshHandle> meshHandle;
	glm::mat4 transformation;

public:
	vector<Vertex> vertices;
	vector<Triangle> triangles;

public:
	Mesh();
	~Mesh();

	// Return the size in bytes of vertices
	const VkDeviceSize verticesSizeInBytes() const {
		return sizeof(Vertex) * vertices.size();
	}

	// return the size in bytes of triangles indices
	const VkDeviceSize indicesSizeInBytes() const {
		return sizeof(Triangle) * triangles.size();
	}

	const uint32_t getCountIndices() const{
		return triangles.size() * 3;
	}

	void setTransform(const glm::mat4 &p_transformation);
	const glm::mat4& getTransform() const {
		return transformation;
	}
};

struct CameraUniformBufferObject{
	glm::mat4 view;
	glm::mat4 projection;
};

struct MeshUniformBufferObject{
	glm::mat4 model;
};

// This struct is used to know handle the memory of mesh
struct MeshHandle{
	const Mesh *mesh;

	size_t verticesSize;
	VkDeviceSize verticesBufferOffset;
	VkBuffer vertexBuffer;
	VmaAllocation vertexAllocation;

	size_t indicesSize;
	VkDeviceSize indicesBufferOffset;
	VkBuffer indexBuffer;
	VmaAllocation indexAllocation;

	uint32_t meshUniformBufferOffset;
	bool hasTransformationChange;
};

class VulkanServer{
public:

	struct QueueFamilyIndices{
		int graphicsFamilyIndex = -1;
		int presentationFamilyIndex = -1;

		bool isComplete(){

			if(graphicsFamilyIndex==-1)
				return false;

			if(presentationFamilyIndex==-1)
				return false;

			return true;
		}
	};

	struct SwapChainSupportDetails{
		VkSurfaceCapabilitiesKHR capabilities;
		vector<VkSurfaceFormatKHR> formats;
		vector<VkPresentModeKHR> presentModes;
	};

	VulkanServer();

	bool enableValidationLayer();

	bool create(GLFWwindow* p_window);
	void destroy();

	void waitIdle();

	void draw();

	void addMesh(const Mesh *p_mesh);
	void removeMesh_internal(MeshHandle* p_meshHandle);

public:
	void processCopy();
	void updateUniformBuffer();

private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice; // Destroyed automatically when instance is cleared
	VkDeviceSize physicalDeviceMinUniformBufferOffsetAlignment;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentationQueue;
	VkSwapchainKHR swapchain;

	vector<const char*> layers;
	vector<const char*> deviceExtensions;

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
	VkDescriptorSetLayout cameraDescriptorSetLayout; // Opaque object that has some information about how is composed a uniform datas
	VkDescriptorPool cameraDescriptorPool; // The pool where to create the VkDescriptorPool
	VkDescriptorSet cameraDescriptorSet; // Hold all reference to buffers and other data of description
	VkDescriptorSetLayout meshesDescriptorSetLayout;
	VkDescriptorPool meshesDescriptorPool;
	VkDescriptorSet meshesDescriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	vector<VkFramebuffer> swapchainFramebuffers;

	VmaAllocator bufferMemoryDeviceAllocator;

	VmaAllocator bufferMemoryHostAllocator;

	VkBuffer cameraUniformBuffer;
	VmaAllocation cameraUniformBufferAllocation;

	struct DynamicMeshUniformBufferData{
		VkBuffer meshUniformBuffer;
		VmaAllocation meshUniformBufferAllocation;
		uint32_t count;
		uint32_t size;

		DynamicMeshUniformBufferData()
			: meshUniformBuffer(VK_NULL_HANDLE),
			  meshUniformBufferAllocation(VK_NULL_HANDLE),
			  count(0),
			  size(0)
		{}
	};

	// Dynamic uniform buffer used to store all mesh informations
	uint32_t meshDynamicUniformBufferOffset;
	DynamicMeshUniformBufferData meshUniformBufferData;

	VkCommandPool graphicsCommandPool;

	vector<VkCommandBuffer> drawCommandBuffers; // Used to draw things
	vector<VkFence> drawFinishFences;
	VkCommandBuffer copyCommandBuffer; // Used to copy data to GPU

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	VkFence copyFinishFence;
private:

	bool reloadDrawCommandBuffer;

	vector<MeshHandle*> meshes;
	vector<MeshHandle*> meshesCopyInProgress;
	vector<MeshHandle*> meshesCopyPending;

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
	VkSurfaceFormatKHR chooseSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats);
	VkPresentModeKHR choosePresentMode(const vector<VkPresentModeKHR> &p_modes);
	VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	// This method is used to create the usable swap chain object
	bool createSwapchain();
	void destroySwapchain();

	// This method is used to create the simple swap chain object
	// The swapchain is the array that is used to hold all information about the image to show.
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

	// The pipeline is the object that contains all commands of a particular rendering process
	// Shader, Vertex inputs, viewports, etc..
	// Each pipiline has a reference to a particular subpass of renderpass
	// The pipeline will be created using renderpass informations
	// The order of execution of particular pipeline depend on the renderpass
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

	bool allocateAndConfigureDescriptorSet();

	// The command pool is an opaque object that is used to allocate command buffers easily
	bool createCommandPool();
	void destroyCommandPool();

	// Command buffer is an object where are stored all commands, here are stored all informations about
	// renderpass, attachments, etc..
	// It's possible to have two kind of command buffers primary and seconday
	// The secondary can be executed by primary only, the primary can be submitted directly into queue
	// This function only allocate a command buffer and doesn't initialize it
	bool allocateCommandBuffers();

	// This function take all commandBuffers and set it in executable state with all commands
	// to execute
	// This store the renderpass, so it should be submitted each time the swapchain is recreated
	void beginCommandBuffers();

	bool createSyncObjects();
	void destroySyncObjects();

	void removeAllMeshes();

	bool checkInstanceExtensionsSupport(const vector<const char*> &p_required_extensions);
	bool checkValidationLayersSupport(const vector<const char *> &p_layers);

	// Return the ID in the array of selected device
	int autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_device, VkPhysicalDeviceProperties *r_deviceProps);

private:
	void recreateSwapchain();

	// return the size of allocated memory, or 0 if error.
	bool createBuffer(VmaAllocator p_allocator, VkDeviceSize p_size, VkMemoryPropertyFlags p_usage, VkSharingMode p_sharingMode, VmaMemoryUsage p_memoryUsage, VkBuffer &r_buffer, VmaAllocation &r_allocation);
	void destroyBuffer(VmaAllocator p_allocator, VkBuffer &r_buffer, VmaAllocation &r_allocation);

	bool hasStencilComponent(VkFormat p_format);
	VkFormat findBestDepthFormat();

	// Accept the list of format in an ordered list from the best to worst
	// and return one of it if it's supported by the Hardware
	bool chooseBestSupportedFormat(const vector<VkFormat> &p_formats, VkImageTiling p_tiling, VkFormatFeatureFlags p_features, VkFormat *r_format);

	bool createImage(uint32_t p_width, uint32_t p_height, VkFormat &p_format, VkImageTiling p_tiling, VkImageUsageFlags p_usage, VkMemoryPropertyFlags p_memoryFlags, VkImage &p_image, VkDeviceMemory &p_memory);
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

class VisualServer
{
public:
	friend class VulkanServer;

	GLFWwindow* window;

	VisualServer();
	~VisualServer();

	bool init();
	void terminate();

	bool can_step();
	void step();

	void addMesh(const Mesh *p_mesh);

private:
	VulkanServer vulkanServer;

	void createWindow();
	void freeWindow();
};
