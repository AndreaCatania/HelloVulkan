#pragma once

#include "core/rid.h"
#include "thirdparty/vulkan/vulkan.h"

class GLFWwindow;

class GLFWWindowData : public ResourceData {

	friend class GLFWWindowServer;

	VkInstance vulkan_instance;
	GLFWwindow *glfw_window;
	bool drawable;
	VkSurfaceKHR surface;

public:
	GLFWWindowData(VkInstance p_vulkan_instance);
	virtual ~GLFWWindowData();

	void set_glfw_window(GLFWwindow *p_window);
	_FORCE_INLINE_ GLFWwindow *get_window() const { return glfw_window; }

	void free_surface();
};
