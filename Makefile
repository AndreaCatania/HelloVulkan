VULKAN_SDK_PATH=/opt/vulkan/1.1.82.1/x86_64
GLFW3_LIB_PATH=/usr/local/lib/pkgconfig
CFLAGS = -std=c++11 -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan

SOURCES = main.cpp VisualServer.cpp mesh.cpp texture.cpp

make_bin_dirs:
	mkdir ./bin -p
	mkdir ./shaders/bin -p

shaders_compile:
	$(VULKAN_SDK_PATH)/bin/glslangValidator -V ./shaders/shader.vert -o ./shaders/bin/vert.spv
	$(VULKAN_SDK_PATH)/bin/glslangValidator -V ./shaders/shader.frag -o ./shaders/bin/frag.spv

# PKG_CONFIG_PATH Location where to find glfw3.pc
all: export PKG_CONFIG_PATH=$(GLFW3_LIB_PATH)
all: make_bin_dirs shaders_compile main.cpp
		g++ $(CFLAGS) ${SOURCES} $(LDFLAGS) -o ./bin/hello_vulkan

# PKG_CONFIG_PATH Location where to find glfw3.pc
debug: export PKG_CONFIG_PATH=$(GLFW3_LIB_PATH)
debug: make_bin_dirs shaders_compile main.cpp
	g++ -ggdb $(CFLAGS) ${SOURCES} $(LDFLAGS) -o ./bin/hello_vulkan.debug

clean:
	rm -f ./bin/*
	rm -f ./shaders/bin/*

run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./bin/hello_vulkan

rundebug:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./bin/hello_vulkan.debug
