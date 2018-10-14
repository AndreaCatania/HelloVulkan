#pragma once

#include "core/hellovulkan.h"
#include "core/rid.h"

class WindowServer {
	static WindowServer *singleton;

public:
	static WindowServer *get_singleton() { return singleton; }

	WindowServer();

	virtual void init_server() = 0;
	virtual void terminate_server() = 0;

	virtual RID create_window(
			const char *p_title,
			int p_width,
			int p_height) = 0;

	virtual void free_window(RID p_window) = 0;

	virtual void set_drawable(RID p_window, bool p_state) = 0;
	virtual bool is_drawable(RID p_window) const = 0;

	virtual void get_window_size(
			RID p_window,
			int *r_width,
			int *r_height) = 0;

	/** Don't clear the vector */
	virtual void get_required_extensions(
			RID p_window,
			std::vector<const char *> &r_extensions) = 0;

	virtual bool create_surface(
			RID p_window,
			VkInstance p_instance,
			VkSurfaceKHR *r_surface) = 0;

	// TODO not sure if should go here
	virtual void fetch_events() = 0;
	virtual bool want_to_quit() const = 0;
};
