import os
import os.path
import glob

from compat import iteritems, isbasestring

def add_source_files(self, sources, filetype, lib_env=None, shared=False):

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
