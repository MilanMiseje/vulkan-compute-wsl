# Creating and Compiling a Vulkan Compute Shader

This section documents a shader used by the Vulkan compute examples.
The goal is to start with the smallest possible compute shader,
compile it to SPIR-V, and inspect the generated module before
loading it from Vulkan.

The shader toolchain:

``` text
compute.comp
     |
     | glslc
     v
compute.spv
     |
     | spirv-dis
     v
human-readable SPIR-V
```

## 1. Create a minimal compute shader

Create `compute.comp`:

``` glsl
#version 450

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
}
```

This is a valid but intentionally empty compute shader.
`local_size_x = 1`, `local_size_y = 1`, and `local_size_z = 1` mean that
one workgroup contains exactly one shader invocation.

No buffers, descriptors, push constants, or calculations are used yet.
This keeps the first shader focused purely on verifying the shader
toolchain.

## 2. Install the GLSL compiler

`glslc` compiles the GLSL source into SPIR-V bytecode that Vulkan can
consume.

``` bash
sudo apt install glslc
glslc --version
```

## 3. Compile GLSL to SPIR-V

``` bash
glslc compute.comp -o compute.spv
```

The transformation is:

``` text
compute.comp  ->  glslc  ->  compute.spv
    GLSL                      SPIR-V
```

## 4. Install SPIR-V tools

```bash
sudo apt install spirv-tools
spirv-dis --version
```

## 5. Inspect the generated SPIR-V

SPIR-V is binary. `spirv-dis` converts it into a human-readable
representation:

``` bash
spirv-dis compute.spv
```
