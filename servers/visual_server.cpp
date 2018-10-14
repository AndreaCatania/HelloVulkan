#include "visual_server.h"

#include "core/error_macros.h"

VisualServer *VisualServer::singleton = nullptr;

VisualServer::VisualServer() {
	CRASH_COND(singleton);

	singleton = this;
}
