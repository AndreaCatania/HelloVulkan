#pragma once

#include "servers/window_server.h"

#include "modules/glfw/glfw_window.h"

class GLFWWindowServer : public WindowServer {
private:
	RID_owner<GLFWWindowData> window_owner;

public:
	GLFWWindowServer();
	virtual void init_server();
	virtual void terminate_server();

	virtual void get_required_extensions(
			std::vector<const char *> &r_extensions);

	virtual RID create_window(
			VkInstance p_vulkan_instance,
			const char *p_title,
			int p_width,
			int p_height);

	virtual void free_window(RID p_window);

	virtual void set_drawable(RID p_window, bool p_state);
	virtual bool is_drawable(RID p_window) const;

	virtual void get_window_size(
			RID p_window,
			int *r_width,
			int *r_height);

	virtual VkSurfaceKHR get_vulkan_surface(RID p_window) const;

	virtual void fetch_events();
	virtual bool want_to_quit() const;
};
