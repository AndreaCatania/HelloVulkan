
#include "modules/glfw/glfw_window.h"

GLFWWindowData::GLFWWindowData() :
		ResourceData(),
		glfw_window(nullptr) {}

void GLFWWindowData::set_glfw_window(GLFWwindow *p_window) {
	glfw_window = p_window;
}
