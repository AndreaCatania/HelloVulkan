#pragma once

#include "core/rid.h"

class GLFWwindow;

class GLFWWindowData : public ResourceData {

	friend class GLFWWindowServer;

	GLFWwindow *glfw_window;
	bool drawable;

public:
	GLFWWindowData();

	void set_glfw_window(GLFWwindow *p_window);
	_FORCE_INLINE_ GLFWwindow *get_window() const { return glfw_window; }
};
