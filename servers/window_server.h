#pragma once

#include "core/hellovulkan.h"

class WindowServer {
public:
	virtual bool init() = 0;
	virtual void terminate() = 0;
	virtual bool instanceWindow(const char *p_title, int p_width, int p_height) = 0;
	virtual void freeWindow() = 0;
	virtual bool isDrawable() const = 0;
	virtual void set_drawable(bool p_state) = 0;
	virtual void getWindowSize(int *r_width, int *r_height) = 0;
	virtual void appendRequiredExtensions(vector<const char *> &r_extensions) = 0;
	virtual bool createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) = 0;

	virtual bool poolEvents() = 0;
	virtual bool wantToQuit() const = 0;
};
