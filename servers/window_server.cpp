
#include "servers/window_server.h"

#include "core/error_macros.h"

WindowServer *WindowServer::singleton = nullptr;

WindowServer::WindowServer() {

	CRASH_COND(singleton);
	singleton = this;
}
