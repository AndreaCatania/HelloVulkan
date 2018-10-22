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

void GLFWWindowServer::get_required_extensions(
		std::vector<const char *> &r_extensions) {

	uint32_t count = 0;
	const char **requiredExtensions;

	requiredExtensions = glfwGetRequiredInstanceExtensions(&count);
	r_extensions.insert(r_extensions.end(), &requiredExtensions[0], &requiredExtensions[count]);
}

RID GLFWWindowServer::window_create(
		const char *p_title,
		int p_width,
		int p_height) {

	GLFWWindowData *window = new GLFWWindowData(p_title, p_width, p_height);

	return window_owner.make_rid(window);
}

void GLFWWindowServer::window_free(RID p_window) {
	GLFWWindowData *window = window_owner.get(p_window);

	DEBUG_ONLY(ERR_FAIL_COND(!window));

	window_owner.release(p_window);
	delete window;
}

void GLFWWindowServer::window_set_vulkan_instance(
		RID p_window,
		VkInstance p_vulkan_instance) {

	GLFWWindowData *window = window_owner.get(p_window);
	window->set_vulkan_instance(p_vulkan_instance);
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

VkSurfaceKHR GLFWWindowServer::window_get_vulkan_surface(RID p_window) const {

	const GLFWWindowData *window = window_owner.get(p_window);
	DEBUG_ONLY(ERR_FAIL_COND_V(!window, VK_NULL_HANDLE));
	return window->surface;
}

void GLFWWindowServer::fetch_events() {
	glfwPollEvents();
	//if (want_to_quit())
	//	set_drawable(false);
}

bool GLFWWindowServer::want_to_quit() const {
	return false;
}
