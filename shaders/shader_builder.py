#!/usr/bin/env python

import os


def build_vulkan_shader(env, source_path):

    file_basename = os.path.basename(source_path)
    file_dir = source_path.replace(file_basename, "")

    out_file_name = "shader_" + file_basename.replace(".", "_")
    spv_file_path = file_dir + out_file_name + ".spv"
    header_file_path = file_dir + out_file_name + ".gen.h"

    class_name = out_file_name.title().replace("_", "")

    # Compile shader using spirv
    if env.Execute(env.vulkan_glslangValidator_path + " -V " + source_path + " -o " + spv_file_path):
        print "Error during shader compilatin: " + source_path
        env.Exit(126)

    # Create header
    fh = open(header_file_path, "w")
    fh.write("/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */\n")
    fh.write("#pragma once\n\n");

    # Create class
    fh.write("class " + class_name + "{\n")
    fh.write("public:\n\n")

    characters = 0

    # Add code variable
    fh.write("\tstatic const char code[];\n")

    # Set bytecode inside array
    bytecode_array = "const char " + class_name + "::code[] = {\n\t\t"

    spv_bytecode = open(source_path, "r")
    line = spv_bytecode.readline()

    while line:
        for c in line:
            bytecode_array += str(ord(c)) + ","
            characters += 1
        bytecode_array += str(ord('\n')) + ","
        characters += 1
        line = spv_bytecode.readline()

    bytecode_array += str(0) # Instead to remove last comma add a zero
    characters += 1

    spv_bytecode.close()

    # Finalize code variable
    bytecode_array += "\n};\n\n"

    # Add code_size variable
    fh.write("\tstatic const int code_size = " + str(characters) + ";\n\n")
    # Finalize code_size variable

    # Finalize class
    fh.write("};\n\n")

    fh.write(bytecode_array)
    fh.write("\n")

    fh.close()

    # Remove spv generated file
    env.Execute("rm " + spv_file_path)


def build_vulkan_shaders_header(target, source, env):
    for s in source:
        build_vulkan_shader(env, str(s))

