
import methods

executable_name = 'hello_vulkan'
executable_dir = '#bin'

# -----------

""" Get Arguments """
platform = ARGUMENTS.get('platform', 0)
target = ARGUMENTS.get('target', "debug")
vulkan_SDK_path = ARGUMENTS.get('vulkan_lib_path', "")


""" Arguments check """
if platform != "windows" and platform != "linux":
    print "The argument platform should be windows or linux"
    Exit(9553) #Invalid property

# TODO improve compilation by removing this requirment
if vulkan_SDK_path == "":
    print "Please set vulkan_lib_path. EG: C:\VulkanSDK\1.1.73.0"
    Exit(9553) #Invalid property


vulkan_lib_path = ""
vulkan_glslangValidator_path = ""
vulkan_library_name = ""

if platform == 'windows':
    vulkan_lib_path = vulkan_SDK_path + r"\Lib"
    vulkan_glslangValidator_path = vulkan_SDK_path + r"/bin/glslangValidator"
    vulkan_library_name = "vulkan-1"
elif platform == 'linux':
    vulkan_lib_path = vulkan_SDK_path + r"/lib"
    vulkan_glslangValidator_path = vulkan_SDK_path + r"/bin/glslangValidator"
    vulkan_library_name = "vulkan"


""" Create directories """
Execute(Mkdir('bin'))
Execute(Mkdir('shaders/bin'))

""" Compile shaders """
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.vert -o ./shaders/bin/vert.spv")
Execute(vulkan_glslangValidator_path + " -V ./shaders/shader.frag -o ./shaders/bin/frag.spv")

""" Project building """
env = Environment()

env.__class__.add_source_files = methods.add_source_files
env.__class__.add_library = methods.add_library
env.__class__.disable_warnings = methods.disable_warnings

# Load module list of engine
env.module_list = methods.detect_modules()

# default include path
env.Append(CPPPATH=[ '#libs', '#' ])

if target=='debug':

    env.Append(CPPDEFINES=['DEBUG_ENABLED'])
    env.Append(CPPDEFINES={'VULKAN_LD_LIBRARY' : 'LD_LIBRARY_PATH=' + vulkan_lib_path})
    env.Append(CPPDEFINES={'VULKAN_EXPLICIT_LAYERS' : 'VK_LAYER_PATH=' + vulkan_SDK_path + '/etc/explicit_layer.d'})

    if platform=='windows':
        env.Append(LINKFLAGS=['/DEBUG'] )
        env.Append(CCFLAGS=['/Zi'] )
        executable_name += '.debug.exe'
    elif platform=='linux':
        env.Append(CCFLAGS=['-ggdb'])
        executable_name += '.debug'

Export('env')

""" Script executions """
SConscript("servers/SCsub")
SConscript("modules/SCsub")

env.Prepend(LIBS=[vulkan_library_name])
env.Prepend(LIBPATH=[vulkan_lib_path])

# Compile executable
env.Program(executable_dir + '/' + executable_name,
            ['main.cpp', 'mesh.cpp', 'texture.cpp', 'VisualServer.cpp'])

