
#include "modules/glfw/glfw_window.h"

#define GLFW_INCLUDE_VULKAN
#include "thirdparty/glfw/include/GLFW/glfw3.h"

GLFWWindowData::GLFWWindowData(VkInstance p_vulkan_instance) :
		ResourceData(),
		vulkan_instance(p_vulkan_instance),
		glfw_window(nullptr),
		drawable(false),
		surface(VK_NULL_HANDLE) {}

GLFWWindowData::~GLFWWindowData() {
	free_surface();
}

void GLFWWindowData::set_glfw_window(GLFWwindow *p_window) {
	free_surface();

	glfw_window = p_window;

	if (!glfw_window)
		return;

	ERR_FAIL_COND(VK_SUCCESS != glfwCreateWindowSurface(
										vulkan_instance,
										glfw_window,
										nullptr,
										&surface));
}

void GLFWWindowData::free_surface() {
	if (surface == VK_NULL_HANDLE)
		return;

	vkDestroySurfaceKHR(vulkan_instance, surface, nullptr);

	surface = VK_NULL_HANDLE;
}
