VULKAN_SDK_PATH=/opt/VulkanSDK/1.0.68.0/x86_64
GLFW3_LIB_PATH=/usr/local/lib/pkgconfig
CFLAGS = -std=c++11 -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

SOURCES = main.cpp VisualServer.cpp

export_pkg:
	export PKG_CONFIG_PATH=$(GLFW3_LIB_PATH) # Location where to find glfw3.pc

all: export_pkg main.cpp
	g++ $(CFLAGS) ${SOURCES} $(LDFLAGS) -o HelloVulkan

clean:
	rm -f HelloVulkan

run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./HelloVulkan
