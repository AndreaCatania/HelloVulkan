#ifndef HELLOVULKAN_H
#define HELLOVULKAN_H

//#define GLFW_INCLUDE_VULKAN
//#include "GLFW/glfw3.h"

#define SDL_MAIN_HANDLED
#include "SDL/SDL.h"

#include "libs/vma/vk_mem_alloc.h" // Includes only interfaces

#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include "libs/glm/glm.hpp"
#include "libs/glm/gtc/matrix_transform.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace std;

// Defined in main
void print(string c);
void print(const char *c);

#endif // HELLOVULKAN_H
