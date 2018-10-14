#pragma once

#include "core/hellovulkan.h"
#include "core/rid.h"

class WindowServer {
public:
	virtual bool init_server() = 0;
	virtual void terminate_server() = 0;

	virtual RID create_window(const char *p_title, int p_width, int p_height) = 0;
	virtual void free_window() = 0;

	virtual bool isDrawable() const = 0;
	virtual void set_drawable(bool p_state) = 0;
	virtual void getWindowSize(int *r_width, int *r_height) = 0;
	virtual void appendRequiredExtensions(std::vector<const char *> &r_extensions) = 0;
	virtual bool createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) = 0;

	virtual void fetchEvents() = 0;
	virtual bool wantToQuit() const = 0;
};
