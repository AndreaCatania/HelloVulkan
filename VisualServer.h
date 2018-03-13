#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "libs/glm/glm.hpp"
#include <string>
#include <vector>
#include <array>
#include <cstring>

using namespace std;

class GLFWwindow;

// RENDER IMAGE PROCESS
// |
// |-> Check if there are pending meshes to insert in the scene
// |-> Request next image from swapchain <------------------------------------------------|
// |-> Submit commandBuffer of image returned by swapchain to GraphicsQueue               |
// |               ^                                                                      |
// |               |----------------------------------------------------------------------+-----|
// |                                                                                      |     |
// |-> Present rendered image by putting it in PresentationQueue                          |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
//                                                                                        |     |
// DEPENDENCY SCHEME OF: Swapchain, Renderpass, Pipeline, Framebuffer, Commandbuffer      |     |
//                                                                                        |     |
// Command buffer                    Swapchain >------------------------------------------|     |
//     |                                |                                                       |
//     |-> Frame buffer         |-------|                                                       |
//     |       |                v                                                               |
//     |       |-> Swapchain image view                                                         |
//     |                                                                                        |
//     |-> Render pass <---------------|                                                        |
//     |                               |                                                        |
//     |-> Pipelines >-----------> regulate execution of pipeline by subpass                    |
//     |-> [User command]                                                                       |
//     |                                                                                        |
//     V                                                                                        |
//   Ready to be submitted to                                                                   |
//   Graphycs queue >---------------------------------------------------------------------------|
//


struct Vertex{
	glm::vec2 pos;
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
		attr[0].format = VK_FORMAT_R32G32_SFLOAT;
		attr[0].offset = offsetof(Vertex, pos);

		attr[1].binding = 0;
		attr[1].location = 1;
		attr[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attr[1].offset = offsetof(Vertex, color);
		return attr;
	}
};

struct Triangle{
	Vertex vertices[3];

};

struct Mesh{
	vector<Triangle> triangles;

	// Return the size in bytes of this mesh
	const size_t size() const {
		return sizeof(Vertex) * 3 * triangles.size();
	}

	const Triangle* data() const{
		return triangles.data();
	}

	const int getCountVertices() const{
		return triangles.size() * 3;
	}
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

	void add_mesh(const Mesh *p_mesh);
	void processCopy();

private:
	GLFWwindow* window;

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice; // Destroyed automatically when instance is cleared
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

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	vector<VkFramebuffer> swapchainFramebuffers;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;

	VkCommandPool graphicsCommandPool;

	vector<VkCommandBuffer> drawCommandBuffers; // Used to draw things
	vector<VkFence> drawFinishFences;
	VkCommandBuffer copyCommandBuffer; // Used to copy data to GPU

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	VkFence copyFinishFence;
private:

	bool reloadDrawCommandBuffer;

	// This struct is used to know handle the memory of mesh
	struct MeshHandle{
		const Mesh *mesh;
		size_t size;
		uint32_t offset;
		VkBuffer vertexBuffer;
	};

	vector<MeshHandle> meshes;
	vector<MeshHandle> meshesCopyInProgress;
	vector<MeshHandle> meshesCopyPending;

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

	// The render pass is an object that is used to organize the rendering process
	// It doesn't have any rendering command nor resources informations
	bool createRenderPass();
	void destroyRenderPass();

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

	// TODO the vertex buffer is not handled correctly, it's necessary to change it
	// Each "mesh" should have its vertex buffer
	bool createVertexBuffer();
	void destroyVertexBuffer();

	// typeBits indicate the suitable types of memory for the buffer
	int32_t chooseMemoryType(uint32_t p_typeBits, VkMemoryPropertyFlags p_propertyFlags);

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

	bool checkInstanceExtensionsSupport(const vector<const char*> &p_required_extensions);
	bool checkValidationLayersSupport(const vector<const char *> &p_layers);

	// Return the ID in the array of selected device
	int autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_device);

private:
	void recreateSwapchain();

	// return the size of allocated memory, or 0 if error.
	size_t createBuffer(VkDeviceSize p_size, VkMemoryPropertyFlags p_usage, VkSharingMode p_sharingMode, VkMemoryPropertyFlags p_memoryTypeFlags, VkBuffer &r_buffer, VkDeviceMemory &r_memory);
	void destroyBuffer(VkBuffer &r_buffer, VkDeviceMemory &r_memory);
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

	void add_mesh(const Mesh *p_mesh);

private:
	VulkanServer vulkanServer;

	void createWindow();
	void freeWindow();
};
