#pragma once

#include "core/rid.h"

class GLFWwindow;

class GLFWWindowData : public ResourceData {

	GLFWwindow *window;

public:
	GLFWWindowData();

	void set_window(GLFWwindow *p_window);
	_FORCE_INLINE_ GLFWwindow *get_window() const { return window; }
};
