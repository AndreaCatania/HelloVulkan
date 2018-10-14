#include "glfw_window_server.h"

#include "core/error_macros.h"

#define GLFW_INCLUDE_VULKAN
#include "thirdparty/glfw/include/GLFW/glfw3.h"

GLFWWindowServer::GLFWWindowServer() :
		WindowServer() {}

void GLFWWindowServer::init_server() {
	glfwInit();
}

void GLFWWindowServer::terminate_server() {
	glfwTerminate();
}

RID GLFWWindowServer::create_window(const char *p_title, int p_width, int p_height) {

	GLFWwindow *glfw_window;

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	glfw_window = glfwCreateWindow(p_width, p_height, p_title, nullptr, nullptr);

	if (!glfw_window) {
		const char *errorMessage;
		glfwGetError(&errorMessage);
		ERR_EXPLAIN(errorMessage);
		ERR_FAIL_V(RID());
	}

	GLFWWindowData *window = new GLFWWindowData;
	window->set_glfw_window(glfw_window);

	return window_owner.make_rid(window);
}

void GLFWWindowServer::free_window(RID p_window) {
	GLFWWindowData *window = window_owner.get(p_window);

	DEBUG_ONLY(ERR_FAIL_COND(!window));
	DEBUG_ONLY(ERR_FAIL_COND(!window->glfw_window));

	glfwDestroyWindow(window->glfw_window);
	window->glfw_window = nullptr;
	window_owner.release(p_window);
	delete window;
}

void GLFWWindowServer::set_drawable(RID p_window, bool p_state) {
	GLFWWindowData *window = window_owner.get(p_window);
	DEBUG_ONLY(ERR_FAIL_COND(!window));

	window->drawable = p_state;
}

bool GLFWWindowServer::is_drawable(RID p_window) const {
	const GLFWWindowData *window = window_owner.get(p_window);
	DEBUG_ONLY(ERR_FAIL_COND_V(!window, false));

	return window->drawable;
}

void GLFWWindowServer::get_window_size(RID p_window, int *r_width, int *r_height) {

	const GLFWWindowData *window = window_owner.get(p_window);
	DEBUG_ONLY(ERR_FAIL_COND(!window));
	DEBUG_ONLY(ERR_FAIL_COND(!window->glfw_window));

	glfwGetWindowSize(window->glfw_window, r_width, r_height);
}

void GLFWWindowServer::get_required_extensions(
		RID p_window,
		std::vector<const char *> &r_extensions) {

	//const GLFWWindowData *window = window_owner.get(p_window);
	//DEBUG_ONLY(ERR_FAIL_COND(!window));

	uint32_t count = 0;
	const char **requiredExtensions;

	requiredExtensions = glfwGetRequiredInstanceExtensions(&count);
	r_extensions.insert(r_extensions.end(), &requiredExtensions[0], &requiredExtensions[count]);
}

bool GLFWWindowServer::create_surface(
		RID p_window,
		VkInstance p_instance,
		VkSurfaceKHR *r_surface) {

	const GLFWWindowData *window = window_owner.get(p_window);
	DEBUG_ONLY(ERR_FAIL_COND_V(!window, false));
	DEBUG_ONLY(ERR_FAIL_COND_V(!window->glfw_window, false));

	return VK_SUCCESS == glfwCreateWindowSurface(
								 p_instance,
								 window->glfw_window,
								 nullptr,
								 r_surface);
}

void GLFWWindowServer::fetch_events() {
	glfwPollEvents();
	//if (want_to_quit())
	//	set_drawable(false);
}

bool GLFWWindowServer::want_to_quit() const {
	return false;
}
