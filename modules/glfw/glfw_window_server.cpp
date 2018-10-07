#include "glfw_window_server.h"

#define GLFW_INCLUDE_VULKAN
#include "thirdparty/glfw/include/GLFW/glfw3.h"

GLFWWindowServer::GLFWWindowServer() :
		window(nullptr),
		running(true) {}

bool GLFWWindowServer::init() {
	glfwInit();
}

void GLFWWindowServer::terminate() {
	glfwTerminate();
}

bool GLFWWindowServer::instanceWindow(const char *p_title, int p_width, int p_height) {
	if (window) {
		print(string("[ERROR] [GLFW] Window already exists."));
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(p_width, p_height, p_title, nullptr, nullptr);
	if (!window) {
		const char *errorMessage;
		glfwGetError(&errorMessage);
		print(string("[ERROR] [GLFW] ") + string(errorMessage));
		return false;
	}
	return true;
}

void GLFWWindowServer::freeWindow() {
	glfwDestroyWindow(window);
	window = nullptr;
}

bool GLFWWindowServer::isDrawable() const {
	return running;
}

void GLFWWindowServer::set_drawable(bool p_state) {
	running = p_state;
}

void GLFWWindowServer::getWindowSize(int *r_width, int *r_height) {
	glfwGetWindowSize(window, r_width, r_height);
}

void GLFWWindowServer::appendRequiredExtensions(vector<const char *> &r_extensions) {

	uint32_t count = 0;
	const char **requiredExtensions;

	requiredExtensions = glfwGetRequiredInstanceExtensions(&count);
	r_extensions.insert(r_extensions.end(), &requiredExtensions[0], &requiredExtensions[count]);
}

bool GLFWWindowServer::createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) {
	return VK_SUCCESS == glfwCreateWindowSurface(p_instance, window, nullptr, r_surface);
}

bool GLFWWindowServer::poolEvents() {
	glfwPollEvents();
	return true;
}

bool GLFWWindowServer::wantToQuit() const {
	return false;
}
