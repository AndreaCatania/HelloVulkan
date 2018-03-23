#ifndef HELLOVULKAN_H
#define HELLOVULKAN_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "libs/vma/vk_mem_alloc.h" // Includes only interfaces

#define GLM_FORCE_RIGHT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include "libs/glm/glm.hpp"
#include "libs/glm/gtc/matrix_transform.hpp"

#include <memory>
#include <string>
#include <vector>
#include <array>
#include <cstring>

using namespace std;

#endif // HELLOVULKAN_H
