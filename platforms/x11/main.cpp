
#include "main/main.h"

#include "core/typedefs.h"
#include <iostream>

/// This is really handy to avoid to set environment variable from outside
void init_environment_variable() {
#ifdef DEBUG_ENABLED
	putenv(__STR(VULKAN_LD_LIBRARY));
	putenv(__STR(VULKAN_EXPLICIT_LAYERS));
#endif
}

int main() {

	init_environment_variable();

	Main main;
	main.start();
}
