
#include "modules/glfw/glfw_window.h"

#define GLFW_INCLUDE_VULKAN
#include "thirdparty/glfw/include/GLFW/glfw3.h"

GLFWWindowData::GLFWWindowData(
		const char *p_title,
		int p_width,
		int p_height) :

		ResourceData(),
		vulkan_instance(VK_NULL_HANDLE),
		glfw_window(nullptr),
		drawable(false),
		surface(VK_NULL_HANDLE) {

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	glfw_window = glfwCreateWindow(p_width, p_height, p_title, nullptr, nullptr);
	ERR_FAIL_COND(!glfw_window);
}

GLFWWindowData::~GLFWWindowData() {

	glfwDestroyWindow(glfw_window);
	free_surface();
}

void GLFWWindowData::set_vulkan_instance(VkInstance p_vulkan_instance) {

	free_surface();

	vulkan_instance = p_vulkan_instance;

	if (vulkan_instance == VK_NULL_HANDLE)
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
