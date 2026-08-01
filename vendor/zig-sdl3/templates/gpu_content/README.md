# SDL3 GPU Content Template
Template for content shaders outside the compiled executable.
Debug mode can be enabled by setting `-Dgpu_debug=true`, but is default for debug modes.
This will load shaders from a directory.

## Shaders
The template provides a starting point for both zig, GLSL, and HLSL shaders.
This can be configured by passing `-Dshader_format=<format>` to decide which one to use at build time.
Your project will most likely only have one format and you can delete the code for the other formats.

## Requirements
* Zig Shaders - `spirv-tools` in order to optimize improperly produced SPIR-V bitcode due to some bugs in zig's generation preventing logical addressing mode (should be fixed in master soon hopefully).
* GLSL Shaders - `glslang` in order to compile GLSL to SPIR-V.
* HLSL Shaders - None, everything to use them is built in.
