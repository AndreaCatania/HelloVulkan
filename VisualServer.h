#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>
#include <cstring>

using namespace std;

class GLFWwindow;

class VulkanServer{
public:

	struct QueueFamilyIndices{
		int graphicsFamilyIndex = -1;

		bool isComplete(){

			if(graphicsFamilyIndex==-1)
				return false;

			return true;
		}
	};

	VulkanServer();

	bool create();
	void destroy();

	bool createInstance();
	void destroyInstance();

	bool createDebugCallback();
	void destroyDebugCallback();

	bool enableValidationLayer(){
#ifdef NDEBUG
	return false;
#else
	return true;
#endif
	}

	bool pickPhysicalDevice();

	bool createLogicalDevice();
	void destroyLogicalDevice();

	void lockupDeviceQueue();

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice p_device);

private:
	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkPhysicalDevice physicalDevice; // Destroyed automatically when instance is cleared
	VkDevice device;
	VkQueue graphicsQueue;

	vector<const char*> layers;

	bool checkExtensionsSupport(const vector<const char*> &p_required_extensions);
	bool checkValidationLayersSupport(const vector<const char *> &p_layers);

	// Return the ID in the array of selected device
	int autoSelectPhysicalDevice(const vector<VkPhysicalDevice> &p_devices, VkPhysicalDeviceType p_device);
};

class VisualServer
{
public:

	struct Window{
		GLFWwindow* window;
	} windowData;

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
