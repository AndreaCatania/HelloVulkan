#include "sdl_window_server.h"

#include "thirdparty/SDL2/include/SDL.h"
#include "thirdparty/SDL2/include/SDL_vulkan.h"

SDLWindowServer::SDLWindowServer() :
		window(nullptr),
		running(true) {}

bool SDLWindowServer::init() {
	return SDL_Init(SDL_INIT_VIDEO) == 0;
}

void SDLWindowServer::terminate() {
	SDL_Quit();
}

bool SDLWindowServer::instanceWindow(const char *p_title, int p_width, int p_height) {
	if (window) {
		print(string("[ERROR] [SDL] Window already exists."));
		return false;
	}

	window = SDL_CreateWindow(p_title, 50, 50, p_width, p_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!window) {
		print(string("[ERROR] [SDL] ") + string(SDL_GetError()));
		return false;
	}
	return true;
}

void SDLWindowServer::freeWindow() {
	SDL_DestroyWindow(window);
	window = nullptr;
}

bool SDLWindowServer::isDrawable() const {
	return running;
}

void SDLWindowServer::set_drawable(bool p_state) {
	running = p_state;
}

void SDLWindowServer::getWindowSize(int *r_width, int *r_height) {
	SDL_Vulkan_GetDrawableSize(window, r_width, r_height);
}

void SDLWindowServer::appendRequiredExtensions(vector<const char *> &r_extensions) {

	uint32_t count = 0;
	const char **requiredExtensions;

	SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr);
	requiredExtensions = new const char *[count];

	SDL_Vulkan_GetInstanceExtensions(window, &count, requiredExtensions);
	r_extensions.insert(r_extensions.end(), &requiredExtensions[0], &requiredExtensions[count]);

	delete[] requiredExtensions;
}

bool SDLWindowServer::createSurface(VkInstance p_instance, VkSurfaceKHR *r_surface) {
	return SDL_TRUE == SDL_Vulkan_CreateSurface(window, p_instance, r_surface);
}
