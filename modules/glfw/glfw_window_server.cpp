#include "glfw_window_server.h"

#include "core/error_macros.h"
#include "modules/glfw/glfw_window.h"

#define GLFW_INCLUDE_VULKAN
#include "thirdparty/glfw/include/GLFW/glfw3.h"

GLFWWindowServer::GLFWWindowServer() :
		running(true) {}

bool GLFWWindowServer::init_server() {
	glfwInit();
}

void GLFWWindowServer::terminate_server() {
	glfwTerminate();
}

RID GLFWWindowServer::create_window(const char *p_title, int p_width, int p_height) {

	GLFWwindow *window;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(p_width, p_height, p_title, nullptr, nullptr);

	if (!window) {
		const char *errorMessage;
		glfwGetError(&errorMessage);
		ERR_EXPLAIN(errorMessage);
		ERR_FAIL_V(RID());
	}

	GLFWWindowData *window_data = new GLFWWindowData;
	window_data->set_window(window);

	return windows_owner.make_rid(window_data);
}

void GLFWWindowServer::free_window() {
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

void GLFWWindowServer::appendRequiredExtensions(std::vector<const char *> &r_extensions) {

	uint32_t count = 0;
	const char **requiredExtensions;

	requiredExtensions = glfwGetRequiredInstanceExtensions(&count);
	r_extensions.insert(r_extensions.end(), &requiredExtensions[0], &requiredExtensions[count]);
}

bool GLFWWindowServer::createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) {
	return VK_SUCCESS == glfwCreateWindowSurface(p_instance, window, nullptr, r_surface);
}

void GLFWWindowServer::fetchEvents() {
	glfwPollEvents();
	if (wantToQuit())
		set_drawable(false);
}

bool GLFWWindowServer::wantToQuit() const {
	return false;
}
