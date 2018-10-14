#pragma once

#include "servers/window_server.h"

class GLFWWindowData;

class GLFWWindowServer : public WindowServer {
private:
	RID_owner<GLFWWindowData> windows_owner;

public:
	bool running;

public:
	GLFWWindowServer();
	virtual bool init_server();
	virtual void terminate_server();
	virtual RID create_window(const char *p_title, int p_width, int p_height);
	virtual void free_window();
	virtual bool isDrawable() const;
	virtual void set_drawable(bool p_state);
	virtual void getWindowSize(int *r_width, int *r_height);
	virtual void appendRequiredExtensions(std::vector<const char *> &r_extensions);
	virtual bool createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface);

	virtual void fetchEvents();
	virtual bool wantToQuit() const;
};
