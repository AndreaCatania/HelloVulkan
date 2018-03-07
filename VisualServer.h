#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <cstring>

using namespace std;

struct WindowData{
	GLFWwindow* window;
};

class GLFWwindow;

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
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR> &p_formats);
	VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR> &p_modes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

	bool createSwapChain();
	void destroySwapChain();

	void lockupSwapchainImages();

	bool createImageView();
	void destroyImageView();

	bool createRenderPass();
	void destroyRenderPass();

	bool createGraphicsPipeline();
	void destroyGraphicsPipeline();

	VkShaderModule createShaderModule(vector<char> &shaderBytecode);
	void destroyShaderModule(VkShaderModule &shaderModule);

	bool createFramebuffers();
	void destroyFramebuffers();

	bool createCommandPool();
	void destroyCommandPool();

	bool allocateCommandBuffers();

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

private:
	VulkanServer vulkanServer;

	void createWindow();
	void freeWindow();
};
