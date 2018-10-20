#!/usr/bin/env python

import methods
import sys

executable_name = 'hello_vulkan'
executable_dir = '#bin'

available_platforms = methods.detect_platoforms()

# -----------


""" Get Arguments """
platform = ARGUMENTS.get('platform', 0)
target = ARGUMENTS.get('target', "debug")
verbose = ARGUMENTS.get('verbose', False)


""" Arguments check """
if platform not in available_platforms:
    string_platforms = ""
    for p in available_platforms:
        if string_platforms != "":
            string_platforms += " or "
        string_platforms += p
    print "The argument platform should be " + string_platforms
    Exit(9553) #Invalid property

debug = target == 'debug'


env = Environment()


""" Create directories """
Execute(Mkdir('bin'))


""" Setup lunarG """
lunarg_sdk_path = methods.setup_lunarg(platform)
if lunarg_sdk_path =="":
    print "LunarG SDK setup failed"
    Exit(126)


""" Set shaders builders"""
env.vulkan_glslangValidator_path = ""
if platform == 'windows':
    env.vulkan_glslangValidator_path = lunarg_sdk_path + r"\glslangValidator"
elif platform == 'x11':
    env.vulkan_glslangValidator_path = lunarg_sdk_path + r"/glslangValidator"


""" Project building """

env.executable_name = executable_name
env.executable_dir = executable_dir
env.platform = platform
env.debug = debug
env.lunarg_sdk_path = lunarg_sdk_path

env.__class__.add_source_files = methods.add_source_files
env.__class__.add_library = methods.add_library
env.__class__.add_program = methods.add_program
env.__class__.disable_warnings = methods.disable_warnings

# Load module list of engine
env.module_list = methods.detect_modules()

# default include path
env.Append(CPPPATH=[ '#libs', '#' ])

if not verbose:
    methods.no_verbose(sys, env)

if debug:
    env.Append(CPPDEFINES=['DEBUG_ENABLED'])
    env.Append(CCFLAGS=['-ggdb'])

Export('env')

""" Script executions """
SConscript("shaders/SCsub")
SConscript("servers/SCsub")
SConscript("core/SCsub")
SConscript("modules/SCsub")
SConscript("main/SCsub")

# build platform
SConscript("platforms/" + platform + "/SCsub")

