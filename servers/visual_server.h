#pragma once

#include "core/rid.h"

class VisualServer {

	static VisualServer *singleton;

public:
	static VisualServer *get_singleton() { return singleton; }

	VisualServer();

	virtual void init() = 0;
	virtual void terminate() = 0;

	virtual RID create_render_target(RID p_window) = 0;
	virtual void destroy_render_target(RID p_render_target) = 0;
};
