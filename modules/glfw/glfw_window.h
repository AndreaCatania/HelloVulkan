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
	GLFWWindowData(
			const char *p_title,
			int p_width,
			int p_height);

	virtual ~GLFWWindowData();

	void set_vulkan_instance(VkInstance p_vulkan_instance);
	_FORCE_INLINE_ GLFWwindow *get_glfw_window() const { return glfw_window; }

	void free_surface();
};
