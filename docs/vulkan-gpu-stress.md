# Vulkan Compute Dispatch - Architecture and Execution Sequence

This document describes the architecture of the working Vulkan compute
example running under WSL2 through Mesa Dozen and Direct3D 12.
Related source code: `examples/gpustress.cpp`, `shaders/gpustress.comp`

The complete application can be divided into four layers.

### Shader layer

``` text
GLSL
  |
SPIR-V
  |
Shader Module
  |
Compute Pipeline
```

### Resource layer

``` text
Descriptor Set Layout
        |
Descriptor Set
        |
Storage Buffer
        |
Device Memory
```

### Execution layer

``` text
Command Pool
      |
Command Buffer
      |
Compute Queue
      |
GPU execution
```

### Platform layer

``` text
Vulkan application
       |
Vulkan Loader
       |
Mesa Dozen
       |
Direct3D 12 / WSL2 GPU path
       |
Windows NVIDIA driver
       |
NVIDIA GPU
```

The central architectural lesson is that Vulkan has no single operation
equivalent to "run this shader."

Instead, it explicitly separates:

``` text
hardware selection
execution queues
shader representation
pipeline state
resource interfaces
memory allocation
resource binding
command recording
submission
synchronization
resource lifetime
```

That separation is the source of much of Vulkan's complexity, but it is
also what gives an application precise control over GPU execution and
resource management.


## 1. End-to-end platform path

``` text
C++ application
      |
      v
Vulkan API / Loader
      |
      v
Mesa Dozen (Vulkan -> D3D12)
      |
      v
WSL2 GPU / D3D12 path
      |
      v
Windows NVIDIA driver
      |
      v
NVIDIA GPU
```

## 2. Complete sequence

``` text
Create Vulkan instance
          |
Enumerate and select physical GPU
          |
Find a compute-capable queue family
          |
Create logical device
          |
Obtain compute queue
          |
Load compiled SPIR-V
          |
Create shader module
          |
Define descriptor set layout
          |
Create pipeline layout
          |
Create compute pipeline
          |
Create storage buffer
          |
Query buffer memory requirements
          |
Select a compatible memory type
          |
Allocate device memory
          |
Bind memory to the buffer
          |
Map and initialize input data
          |
Create descriptor pool and descriptor set
          |
Connect the storage buffer to the shader binding
          |
Create command pool and command buffer
          |
Record pipeline + descriptors + dispatch
          |
Submit command buffer to compute queue
          |
GPU executes the shader
          |
Wait for completion
          |
Map memory and inspect results
          |
Destroy Vulkan resources
```

## 3. Vulkan instance

`VkInstance` is the application's entry point into Vulkan. It establishes
access to the Vulkan implementation but does not represent a particular GPU.

``` text
Application
    |
    v
VkInstance
    |
    v
Vulkan implementation
```

## 4. Physical device

Vulkan exposes available GPUs as physical devices. The application
enumerates these devices and selects the NVIDIA GPU using its vendor ID.

A physical device represents the capabilities and properties of the
hardware as exposed by the Vulkan implementation.

``` text
VkInstance
    |
    v
Physical devices
    |
    +-- NVIDIA GPU  <-- selected
    +-- other devices
```

## 5. Compute queue family

GPU commands are executed through queues. Vulkan groups queues into
queue families according to their capabilities.

The application searches for a family supporting compute operations.

``` text
Physical GPU
    |
    +-- Queue family
            |
            +-- COMPUTE
```

## 6. Logical device and compute queue

The logical device is the application's active Vulkan connection to the
selected physical GPU.

``` text
Physical Device
      |
      v
Logical Device
      |
      v
Compute Queue
```

## 7. Shader path: GLSL to pipeline

The compute shader is compiled before the application runs:

``` text
GLSL compute shader
        |
        | glslc
        v
      SPIR-V
```

The application loads that SPIR-V binary into CPU memory and creates a
Vulkan shader module:

``` text
SPIR-V file
     |
     v
CPU memory
     |
     v
VkShaderModule
```

A shader module contains shader code understood by Vulkan.

## 8. Descriptor set layout

The shader needs access to a storage buffer.

The descriptor set layout defines the resource interface expected by the
shader:

``` text
Descriptor Set 0
      |
      +-- Binding 0
              |
              +-- Storage Buffer
```

## 9. Pipeline layout

The pipeline layout connects the resource interface to the pipeline
architecture:

``` text
Pipeline Layout
      |
      v
Descriptor Set Layout
      |
      v
Set 0 / Binding 0 / Storage Buffer
```

## 10. Compute pipeline

The compute pipeline combines the executable compute shader with its
pipeline layout.

``` text
Shader Module --------+
                      |
                      v
               Compute Pipeline
                      ^
                      |
Pipeline Layout ------+
```

## 11. Storage buffer

The workload operates on 1,048,576 floating-point values.

``` text
1,048,576 floats
x 4 bytes
= 4 MiB
```

A Vulkan buffer is created with storage-buffer usage.

An important Vulkan concept is that creating a buffer does not allocate
its backing memory:

``` text
VkBuffer
   |
   +-- size
   +-- usage
   +-- sharing mode
   |
   X  no backing memory yet
```

## 12. Memory requirements

The application asks Vulkan what memory the buffer requires.

The implementation reports information including:

``` text
allocation size
alignment
compatible memory types
```

For the tested 4 MiB buffer, the reported requirements included a 64 KiB
alignment and a bitmask describing compatible memory types.

This step exists because Vulkan separates resource creation from memory
allocation.

## 13. Memory type selection

The example deliberately selects memory that is both:

``` text
HOST_VISIBLE
HOST_COHERENT
```

`HOST_VISIBLE` allows the CPU to map the allocation.

`HOST_COHERENT` simplifies synchronization of CPU-visible memory because
explicit host cache flushes and invalidations are not required for this
simple usage pattern.

This is convenient for a demonstration. A performance-oriented
application may instead use device-local memory together with staging
buffers.

## 14. Allocation and binding

Device memory is explicitly allocated and then bound to the buffer:

``` text
VkBuffer
    |
    | bind
    v
VkDeviceMemory
```

Only after this operation does the buffer have backing storage.

This explicit relationship is central to Vulkan's memory model.

## 15. CPU initialization

The memory is mapped into the application's address space:

``` text
VkDeviceMemory
      |
      | map
      v
CPU pointer
      |
      | write input
      v
Buffer contents
```

The example initializes all 1,048,576 floating-point elements.

The first values before GPU execution were:

``` text
0.001, 0.002, 0.003
```

## 16. Descriptor set

The descriptor set turns the abstract resource interface into a concrete
binding.

``` text
Descriptor Set Layout
    = what the shader expects

Descriptor Set
    = which resource the shader actually receives
```

The resulting relationship is:

``` text
Compute Shader
      |
      | set 0 / binding 0
      v
Descriptor Set
      |
      v
Storage Buffer
      |
      v
Device Memory
```

The shader can now access the actual 4 MiB buffer.

## 17. Command pool and command buffer

Vulkan GPU work is normally recorded before it is executed.

A command pool is associated with the compute queue family and is used
to allocate command buffers:

``` text
Compute Queue Family
        |
        v
Command Pool
        |
        v
Command Buffer
```

The command buffer is effectively a recorded package of GPU
instructions.

## 18. Recording the workload

The command buffer records three essential operations:

``` text
Bind Compute Pipeline
         |
         v
Bind Descriptor Set
         |
         v
Dispatch Workgroups
```

These answer three different questions:

``` text
Pipeline       -> What code should execute?
Descriptor Set -> What data should it use?
Dispatch       -> How much parallel work should execute?
```

## 19. Workgroup geometry

The shader uses a local workgroup size of 256 invocations.

The application dispatches 4096 workgroups:

``` text
4096 workgroups
x 256 invocations per workgroup
= 1,048,576 shader invocations
```

This matches the number of elements in the storage buffer.

Conceptually:

``` text
Dispatch
   |
   +-- Workgroup 0   -> 256 invocations
   +-- Workgroup 1   -> 256 invocations
   +-- Workgroup 2   -> 256 invocations
   +-- ...
   +-- Workgroup 4095 -> 256 invocations
                         |
                         v
                 1,048,576 total
```

Each invocation operates on one buffer element.

## 20. Queue submission

Recording a command buffer still does not execute it.

The command buffer must be submitted to the compute queue:

``` text
Recorded Command Buffer
          |
          | submit
          v
     Compute Queue
          |
          v
         GPU
```

This is the transition from Vulkan setup/recording to actual GPU
execution.

## 21. GPU workload

Each of the 1,048,576 shader invocations performs 10,000 iterations of
the mathematical workload.

That gives approximately:

``` text
1,048,576 x 10,000
= 10.49 billion loop iterations
```

The workload is intentionally compute-heavy so that GPU execution is
measurable rather than an almost instantaneous test dispatch.

On the Quadro M2200 test system, the observed host-side submit-and-wait
duration was:

``` text
0.167399 seconds
```

This is useful as a functional observation, but it is not a precise GPU
benchmark. Accurate GPU execution timing should use Vulkan timestamp
queries.

## 22. Synchronization

GPU execution is asynchronous relative to the CPU.

Conceptually:

``` text
CPU records commands
        |
CPU submits commands
        |
        +----------------------+
        |                      |
        v                      v
CPU continues             GPU executes
        |                      |
        +------ wait ----------+
                 |
                 v
          GPU is finished
```

The demonstration waits until the compute queue becomes idle before
reading the result.

This is intentionally simple. More advanced Vulkan applications normally
use finer-grained synchronization such as fences, semaphores, and
pipeline barriers.

## 23. Reading the result

Once GPU execution is complete, the host-visible memory is mapped again.

The observed values were:

``` text
Before GPU:
0.001, 0.002, 0.003

After GPU:
1.59009, 1.59009, 1.59009
```

The changed values provide an end-to-end functional confirmation that
the shader actually executed and wrote into the storage buffer.

The complete data path is:

``` text
CPU
 |
 | initialize
 v
Device Memory / Storage Buffer
 |
 | descriptor binding
 v
Compute Shader
 |
 | GPU computation
 v
Storage Buffer / Device Memory
 |
 | map
 v
CPU
```

## 24. Cleanup and object lifetime

Vulkan object lifetime is explicit.

Once execution is complete, resources are destroyed in an order that
respects their dependencies.

Conceptually, cleanup reverses setup:

``` text
Command resources
       |
Descriptor resources
       |
Storage buffer
       |
Device memory
       |
Compute pipeline
       |
Pipeline layout
       |
Descriptor set layout
       |
Shader module
       |
Logical device
       |
Vulkan instance
```

This explicit ownership and lifetime management contributes
significantly to Vulkan's verbosity.
