# -*- Python -*-

import os

# Setup config name.
config.name = "MIP" + config.name_suffix

# Setup source root.
config.test_source_root = os.path.dirname(__file__)

def build_invocation(compile_flags):
    return " " + " ".join([config.clang] + compile_flags) + " "


# Assume that llvm-mipdata is in the config.llvm_tools_dir.
llvm_mipdata = os.path.join(config.llvm_tools_dir, "llvm-mipdata")

config.substitutions.append(("%clang ", build_invocation([config.target_cflags])))
config.substitutions.append(
    ("%clangxx ", build_invocation(config.cxx_mode_flags + [config.target_cflags]))
)
config.substitutions.append(("%llvm_mipdata", llvm_mipdata))

config.suffixes = [".c", ".cpp"]
config.excludes = ['Inputs']

if config.host_os not in ["Darwin", "Linux"]:
    config.unsupported = True
elif config.host_arch not in ["x86_64", "aarch64", "arm"]:
    config.unsupported = True
