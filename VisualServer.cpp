#include "VisualServer.h"

#include <iostream>

void print(string c){
	cout << c << endl;
}

void print(const char * c){
	cout << c << endl;
}

bool VulkanServer::createInstance(){

	print("Instancing Vulkan");

	uint32_t glfwExtensionsCount = 0;
	const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);

	if(!checkExtensionsSupport(glfwExtensionsCount, glfwExtensions)){
		return false;
	}

	if(!checkLunarGValidationLayerSupport()){
		return false;
	}

	// Create instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Hello Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
	appInfo.pEngineName = "HelloVulkanEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = glfwExtensionsCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledLayerCount = 0;

	VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);

	switch(res){
	case VK_SUCCESS:
		print("Instancing Vulkan success");
		return true;
	default:
		std::cout << "[ERROR] Instancing error" << std::endl;
		return false;
	}
}

void VulkanServer::destroyInstance(){
	vkDestroyInstance(instance, nullptr);
}

bool VulkanServer::checkExtensionsSupport(uint32_t requiredExtensionsCount, const char** requiredExtensions){
	uint32_t availableExtensionsCount = 0;
	vector<VkExtensionProperties> availableExtensions;
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr);
	availableExtensions.resize(availableExtensionsCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions.data());

	print("Checking if required extensions are available");
	bool missing = false;
	for(int i = 0; i<requiredExtensionsCount;++i){
		bool found = false;
		for(int j = 0; j<availableExtensionsCount; ++j){
			if( strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0 ){
				found = true;
				break;
			}
		}

		print(string(found ? "[Available]" : "[NOT AVAILABLE]") + string(" extension: ") + string(requiredExtensions[i]) );
		if(found)
			missing = true;
	}
	return missing;
}

bool VulkanServer::checkLunarGValidationLayerSupport(){
	vector<const char*> layers(1);
	layers.push_back("VK_LAYER_LUNARG_standard_validation");
	return checkValidationLayersSupport(layers);
}

bool VulkanServer::checkValidationLayersSupport(vector<const char*> p_layers){

	uint32_t availableLayersCount = 0;
	vector<VkLayerProperties> availableLayers;
	vkEnumerateInstanceLayerProperties(&availableLayersCount, nullptr);
	availableLayers.resize(availableLayersCount);
	vkEnumerateInstanceLayerProperties(&availableLayersCount, availableLayers.data());

	print("Checking if required validation layers are available");
	bool missing = false;
	for(int i = p_layers.size()-1; i>=0; --i){
		bool found = false;
		for(int j = 0; j<availableLayersCount;++j){
			if( strcmp(p_layers[i], availableLayers[j].layerName) == 0 ){
				found = true;
				break;
			}
		}

		print(string(found ? "[Available]" : "[NOT AVAILABLE]") + string(" layer: ") + string(p_layers[i]) );
		if(found)
			missing = true;
	}
	return missing;
}

VisualServer::VisualServer(){

}

VisualServer::~VisualServer(){

}

bool VisualServer::init(){
	createWindow();
	if( !vulkanServer.createInstance() ){
		return false;
	}
	return true;
}

void VisualServer::terminate(){
	vulkanServer.destroyInstance();
	freeWindow();
}

bool VisualServer::can_step(){
	return !glfwWindowShouldClose(windowData.window);
}

void VisualServer::step(){
	glfwPollEvents();
}

void VisualServer::createWindow(){
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	windowData.window = glfwCreateWindow(800, 600, "Hello Vulkan", nullptr, nullptr);
}

void VisualServer::freeWindow(){
	glfwDestroyWindow(windowData.window);
	glfwTerminate();
}
