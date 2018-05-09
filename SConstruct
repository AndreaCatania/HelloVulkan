
# TODO make it platform agnostic
sdl_lib_path = r"C:\Program Files\SDL\lib\x64"
vulkan_SDK_path = r"C:\VulkanSDK\1.1.73.0"

vulkan_lib_path = vulkan_SDK_path + r"\Lib"
vulkan_glslangValidator_path = vulkan_SDK_path + r"/bin/glslangValidator"

# -----------

Execute(Mkdir('bin'))
Execute(Mkdir('shaders/bin'))

# Compile shaders
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.vert -o ./shaders/bin/vert.spv")
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.frag -o ./shaders/bin/frag.spv")

# Compile executable
Program('#bin/hello_vulkan', ['main.cpp','mesh.cpp', 'texture.cpp', 'VisualServer.cpp'], LIBS=['SDL2', 'vulkan-1'], LIBPATH=[sdl_lib_path, vulkan_lib_path], CPPPATH=[ '#libs' ])


