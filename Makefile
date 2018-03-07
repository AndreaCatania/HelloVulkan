VULKAN_SDK_PATH=/opt/VulkanSDK/1.0.68.0/x86_64
GLFW3_LIB_PATH=/usr/local/lib/pkgconfig
CFLAGS = -std=c++11 -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

SOURCES = main.cpp VisualServer.cpp

shaders_compile:
	$(VULKAN_SDK_PATH)/bin/glslangValidator -V ./shaders/shader.vert -o ./shaders/bin/vert.spv
	$(VULKAN_SDK_PATH)/bin/glslangValidator -V ./shaders/shader.frag -o ./shaders/bin/frag.spv

# PKG_CONFIG_PATH Location where to find glfw3.pc
all: export PKG_CONFIG_PATH=$(GLFW3_LIB_PATH)
all: shaders_compile export_pkg main.cpp
	g++ $(CFLAGS) ${SOURCES} $(LDFLAGS) -o ./bin/HelloVulkan

# PKG_CONFIG_PATH Location where to find glfw3.pc
debug: export PKG_CONFIG_PATH=$(GLFW3_LIB_PATH)
debug: shaders_compile main.cpp
	g++ -ggdb $(CFLAGS) ${SOURCES} $(LDFLAGS) -o ./bin/HelloVulkan.debug

clean:
	rm -f ./bin/*
	rm -f ./shaders/bin/*

run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./bin/HelloVulkan

rundebug:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./bin/HelloVulkan.debug
