#pragma once

#include "core/rid.h"

class RenderTarget : public ResourceData {

	RID window;

public:
	RenderTarget();

	void init(RID p_window);

	RID get_window();
};
