
#include "main/main.h"

#include <iostream>

#define INTERNAL_AS_STRING(s) #s
#define AS_STRING(s) INTERNAL_AS_STRING(s)

/// This is really handy to avoid to set environment variable from outside
void init_environment_variable() {
#ifdef DEBUG_ENABLED
	putenv(AS_STRING(VULKAN_LD_LIBRARY));
	putenv(AS_STRING(VULKAN_EXPLICIT_LAYERS));
#endif
}

int main() {

	init_environment_variable();

	Main main;
	main.start();
}
