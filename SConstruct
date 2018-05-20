
# TODO make it platform agnostic
sdl_lib_path = r"C:\Program Files\SDL\lib\x64"
vulkan_SDK_path = r"C:\VulkanSDK\1.1.73.0"

vulkan_lib_path = vulkan_SDK_path + r"\Lib"
vulkan_glslangValidator_path = vulkan_SDK_path + r"/bin/glslangValidator"

# -----------

# Get Arguments
platform = ARGUMENTS.get('platform', 0)
target = ARGUMENTS.get('target', "debug")

# Arguments check
if platform != "windows" and platform != "linux":
    print "The argument platform should be windows or linux"
    Exit(9553) #Invalid property

# Make dirs
Execute(Mkdir('bin'))
Execute(Mkdir('shaders/bin'))

# Compile shaders
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.vert -o ./shaders/bin/vert.spv")
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.frag -o ./shaders/bin/frag.spv")

# Project building
env = Environment()

executable_name = '#bin/hello_vulkan'

if target=='debug':
    if platform=='windows':
        env.Append(LINKFLAGS=['/DEBUG'] )
        env.Append(CCFLAGS=['/Zi'] )
        executable_name += '.debug.exe'

# Compile executable
env.Program(executable_name, ['main.cpp','mesh.cpp', 'texture.cpp', 'VisualServer.cpp'], LIBS=['SDL2', 'vulkan-1'], LIBPATH=[sdl_lib_path, vulkan_lib_path], CPPPATH=[ '#libs' ])

