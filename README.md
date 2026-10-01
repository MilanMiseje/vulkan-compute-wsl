# Vulkan on WSL2 with NVIDIA GPU

## Goal

Set up and verify Vulkan on **WSL2 / Ubuntu** using an **NVIDIA GPU**,
and create a minimal Vulkan C++ application.

The tested environment:

-   Windows with WSL2
-   Ubuntu 24.04 LTS
-   NVIDIA GPU
-   WSLg (Windows Subsystem for Linux GUI )
-   `/dev/dxg` available

The resulting Vulkan stack uses **Mesa Dozen (`dzn`)**, which implements
Vulkan on top of Direct3D 12.

## Vulkan Architecture

The application itself uses the standard Vulkan API:

``` text
C++ Vulkan application
        │
        │ Vulkan API
        ▼
libvulkan.so
        │
        ▼
Mesa Dozen (dzn)
        │
        │ translates Vulkan to D3D12
        ▼
WSL /dev/dxg
        │
        ▼
Direct3D 12
        │
        ▼
Windows NVIDIA driver
        │
        ▼
NVIDIA GPU
```

Therefore, application code still uses normal Vulkan objects and
commands such as:

``` text
VkInstance
VkPhysicalDevice
VkDevice
VkQueue
VkBuffer
VkCommandBuffer
VkPipeline
vkCmdDispatch()
```
