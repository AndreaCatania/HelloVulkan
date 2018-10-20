import os
import os.path
import glob

from compat import iteritems, isbasestring


def add_source_files(self, sources, filetype):

    if isbasestring(filetype):
        dir_path = self.Dir('.').abspath
        filetype = sorted(glob.glob(dir_path + "/" + filetype))

    for path in filetype:
        sources.append(self.Object(path))


def disable_warnings(self):
    # 'self' is the environment
    #if self.msvc:
    #    self.Append(CCFLAGS=['/w'])
    #else:
    self.Append(CCFLAGS=['-w'])


def no_verbose(sys, env):

    colors = {}

    # Colors are disabled in non-TTY environments such as pipes. This means
    # that if output is redirected to a file, it will not contain color codes
    if sys.stdout.isatty():
        colors['cyan'] = '\033[96m'
        colors['purple'] = '\033[95m'
        colors['blue'] = '\033[94m'
        colors['green'] = '\033[92m'
        colors['yellow'] = '\033[93m'
        colors['red'] = '\033[91m'
        colors['end'] = '\033[0m'
    else:
        colors['cyan'] = ''
        colors['purple'] = ''
        colors['blue'] = ''
        colors['green'] = ''
        colors['yellow'] = ''
        colors['red'] = ''
        colors['end'] = ''

    compile_source_message = '%sCompiling %s==> %s$SOURCE%s' % (colors['blue'], colors['purple'], colors['yellow'], colors['end'])
    java_compile_source_message = '%sCompiling %s==> %s$SOURCE%s' % (colors['blue'], colors['purple'], colors['yellow'], colors['end'])
    compile_shared_source_message = '%sCompiling shared %s==> %s$SOURCE%s' % (colors['blue'], colors['purple'], colors['yellow'], colors['end'])
    link_program_message = '%sLinking Program        %s==> %s$TARGET%s' % (colors['red'], colors['purple'], colors['yellow'], colors['end'])
    link_library_message = '%sLinking Static Library %s==> %s$TARGET%s' % (colors['red'], colors['purple'], colors['yellow'], colors['end'])
    ranlib_library_message = '%sRanlib Library         %s==> %s$TARGET%s' % (colors['red'], colors['purple'], colors['yellow'], colors['end'])
    link_shared_library_message = '%sLinking Shared Library %s==> %s$TARGET%s' % (colors['red'], colors['purple'], colors['yellow'], colors['end'])
    java_library_message = '%sCreating Java Archive  %s==> %s$TARGET%s' % (colors['red'], colors['purple'], colors['yellow'], colors['end'])

    env.Append(CXXCOMSTR=[compile_source_message])
    env.Append(CCCOMSTR=[compile_source_message])
    env.Append(SHCCCOMSTR=[compile_shared_source_message])
    env.Append(SHCXXCOMSTR=[compile_shared_source_message])
    env.Append(ARCOMSTR=[link_library_message])
    env.Append(RANLIBCOMSTR=[ranlib_library_message])
    env.Append(SHLINKCOMSTR=[link_shared_library_message])
    env.Append(LINKCOMSTR=[link_program_message])
    env.Append(JARCOMSTR=[java_library_message])
    env.Append(JAVACCOMSTR=[java_compile_source_message])


def detect_platoforms():

    platforms_list = []

    files = glob.glob("platforms/*")
    files.sort()

    for x in files:
        if not os.path.isdir(x):
            continue
        x = x.replace("platforms\\", "") # win32
        x = x.replace("platforms/", "")  # rest of world
        platforms_list.append(x)

    return platforms_list


def detect_modules():

    module_list = []
    includes_cpp = ""
    register_cpp = ""
    unregister_cpp = ""

    files = glob.glob("modules/*")
    files.sort()  # so register_module_types does not change that often, and also plugins are registered in alphabetic order
    for x in files:
        if not os.path.isdir(x):
            continue
        if not os.path.exists(x + "/config.py"):
            continue
        x = x.replace("modules/", "")  # rest of world
        x = x.replace("modules\\", "")  # win32
        module_list.append(x)
        try:
            with open("modules/" + x + "/register_types.h"):
                includes_cpp += '#include "modules/' + x + '/register_types.h"\n'
                register_cpp += '#ifdef MODULE_' + x.upper() + '_ENABLED\n'
                register_cpp += '\tregister_' + x + '_types();\n'
                register_cpp += '#endif\n'
                unregister_cpp += '#ifdef MODULE_' + x.upper() + '_ENABLED\n'
                unregister_cpp += '\tunregister_' + x + '_types();\n'
                unregister_cpp += '#endif\n'
        except IOError:
            pass

    modules_cpp = """
// modules.cpp - THIS FILE IS GENERATED, DO NOT EDIT!!!!!!!
#include "register_module_types.h"

""" + includes_cpp + """

void register_module_types() {
""" + register_cpp + """
}

void unregister_module_types() {
""" + unregister_cpp + """
}
"""

    # NOTE: It is safe to generate this file here, since this is still executed serially
    with open("modules/register_module_types.gen.cpp", "w") as f:
        f.write(modules_cpp)

    return module_list


def detect_files(base_path, excludes = [], extensions = []):
    out_files = []
    files = glob.glob(base_path + "/*")
    for f in files:
        if os.path.isdir(f):
            out_files.extend(detect_files(f, excludes, extensions))
        else:
            excluded = False
            for exclude in excludes:
                if f.find(exclude) >= 0:
                    excluded = True
                    break;

            if excluded:
                continue

            if len(extensions) == 0:
                out_files.append(f)
            else:
                for e in extensions:
                    if f.endswith('.' + e):
                        out_files.append(f)
                        break
    return out_files


def add_library(env, name, sources, **args):
    library = env.Library(name, sources, **args)
    env.NoCache(library)
    return library


def add_program(env, name, sources, **args):
    program = env.Program(name, sources, **args)
    env.NoCache(program)
    return program


import urllib2
import tarfile


def setup_lunarg(platform):

    sdk_archive_path = download_lunarg(platform)
    sdk_base_path = os.path.dirname(sdk_archive_path)
    sdk_setup_proof_path = sdk_base_path + "/SETUP_SUCCESS"

    if os.path.exists(sdk_setup_proof_path):
        print "LunarG SDK Already installed." + \
            " To re-install it remove this file: " + \
            sdk_setup_proof_path

        return sdk_base_path

    if sdk_archive_path == "":
        return ""

    if sdk_archive_path.endswith('.tar.gz'):

        base_source_path = "1.1.85.0/x86_64"

        subdir = [
            "bin",
            "etc",
            "lib",
            "shared"
        ]

        archive = tarfile.open(
            sdk_archive_path,
            "r")

        members = archive.getmembers()

        for member in members:

            if member.isdir():
                continue

            extract = False
            for sd in subdir:
                if member.name.startswith(base_source_path  + "/" + sd):
                    extract = True

            if not extract:
                continue

            file_name = os.path.basename(member.name)
            member.name = file_name
            archive.extract(
                member,
                sdk_base_path)

    else:
        print "Downloaded LunarG SDK extension not supported"
        return ""

    with open(sdk_setup_proof_path, "w") as f:
        f.write("success")

    print "LunarG SDK installation success"
    return sdk_base_path


def download_lunarg(platform):
    repository_url = ""
    file_name = ""
    base_path = "./bin/sdk"

    if platform=="x11":
        repository_url = "https://sdk.lunarg.com/sdk/download/latest/linux/vulkan-sdk.tar.gz?u="
        file_name = "lunarg_sdk.tar.gz"
        base_path += "/x11"
    else:
        return ""

    sdk_archive_path = base_path + "/" + file_name

    if os.path.exists(sdk_archive_path):
        return sdk_archive_path

    if not os.path.exists(base_path):
        os.makedirs(base_path)

    print 'Start downloading of LunarG SDK, Wait please :)'

    filedata = urllib2.urlopen(repository_url)
    datatowrite = filedata.read()

    with open(sdk_archive_path, 'wb') as f:
        f.write(datatowrite)

    print 'Download of LunarG SDK is done.'
    return sdk_archive_path

