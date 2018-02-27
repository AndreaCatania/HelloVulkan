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
	bool createInstance();
	void destroyInstance();

private:
	VkInstance instance;

	bool checkExtensionsSupport(uint32_t, const char**);
	bool checkLunarGValidationLayerSupport();
	bool checkValidationLayersSupport(vector<const char *> p_layers);
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
