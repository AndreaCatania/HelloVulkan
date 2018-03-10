#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <cstring>

using namespace std;

struct WindowData{
	GLFWwindow* window;
	int width;
	int height;
};

class GLFWwindow;

// RENDER IMAGE PROCESS
// |
// |-> Request next image from swapchain <------------------------------------------------|
// |-> Submit commandBuffer of image returned by swapchain to GraphicsQueue               |
// |               ^                                                                      |
// |               |----------------------------------------------------------------------+-----|
// |                                                                                      |     |
// |-> Present rendered image by putting it in PresentationQueue                          |     |
//                                                                                        |     |
//                                                                                        |     |
// Dependency scheme of: Swapchain, Renderpass, Pipeline, Framebuffer, Commandbuffer      |     |
//                                                                                        |     |
// Command buffer                    Swapchain >------------------------------------------|     |
//     |                                |                                                       |
//     |-> Frame buffer         |-------|                                                       |
//     |       |                v                                                               |
//     |       |-> Swapchain image view                                                         |
//     |                                                                                        |
//     |-> Render pass <---------------|                                                        |
//     |                               |                                                        |
//     |-> Pipeline ------------- regulate execution of pipeline by subpass                     |
//     |-> [User command]                                                                       |
//     |                                                                                        |
//     V                                                                                        |
//   Ready to be submitted to                                                                   |
//   Graphycs queue >---------------------------------------------------------------------------|
//


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

	bool create(const WindowData* p_windowData);
	void destroy();

	void waitIdle();

	void onWindowResize();

	void draw();

private:
	const WindowData* windowData;

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

	VkCommandPool commandPool;

	vector<VkCommandBuffer> commandBuffers; // Doesn't required destruction

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
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

	bool createSemaphores();
	void destroySemaphores();

	bool checkInstanceExtensionsSupport(const vector<const char*> &p_required_extensions);
	bool checkValidationLayersSupport(const vector<const char *> &p_layers);

	// Return the ID in the array of selected device
	int autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_device);
};

class VisualServer
{
public:
	friend class VulkanServer;

	WindowData windowData;

	VisualServer();
	~VisualServer();

	bool init();
	void terminate();

	bool can_step();
	void step();

	static void windowResized(GLFWwindow* p_window, int p_width, int p_height);

private:
	VulkanServer vulkanServer;

	void createWindow();
	void freeWindow();
};
