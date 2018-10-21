#include "render_target.h"

RenderTarget::RenderTarget() :
		ResourceData() {}

void RenderTarget::init(RID p_window) {
	window = p_window;
}

RID RenderTarget::get_window() {
	return window;
}
