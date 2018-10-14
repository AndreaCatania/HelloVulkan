
#include "modules/glfw/glfw_window.h"

GLFWWindowData::GLFWWindowData() :
		ResourceData(),
		window(nullptr) {}

void GLFWWindowData::set_window(GLFWwindow *p_window) {
	window = p_window;
}
