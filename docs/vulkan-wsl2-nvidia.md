## 1. Install Vulkan Tools

``` bash
sudo apt update
sudo apt install -y vulkan-tools libvulkan-dev mesa-vulkan-drivers
```

Check Vulkan:

``` bash
vulkaninfo --summary
```

Initially, Vulkan only detected the software renderer:

``` text
deviceName = llvmpipe
deviceType = PHYSICAL_DEVICE_TYPE_CPU
driverName = llvmpipe
```

This means Vulkan was working, but rendering was performed in software
on the CPU.

## 2. Verify WSL GPU Acceleration

Install Mesa utilities:

``` bash
sudo apt install -y mesa-utils
```

Force Mesa to use the D3D12 backend and select the NVIDIA adapter:

``` bash
MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA \
GALLIUM_DRIVER=d3d12 \
glxinfo -B
```

Expected output contains something similar to:

``` text
Device: D3D12 (<NVIDIA GPU>)
Accelerated: yes
OpenGL renderer string: D3D12 (<NVIDIA GPU>)
```

This verifies that the following path works:

``` text
WSL
 ↓
/dev/dxg
 ↓
Direct3D 12
 ↓
Windows NVIDIA driver
 ↓
NVIDIA GPU
```

## 3. Check for the Dozen Vulkan driver

List available Vulkan ICDs:

``` bash
ls /usr/share/vulkan/icd.d/
```

The required driver is:

``` text
dzn_icd.json
```

The default Ubuntu Mesa packages did not provide it in this setup.

## 4. Install Vulkan driver

Add the Kisak Mesa PPA:

``` bash
sudo add-apt-repository ppa:kisak/kisak-mesa
sudo apt update
```

Check the available Mesa version:

``` bash
apt policy mesa-vulkan-drivers
```

Install/upgrade the Vulkan drivers:

``` bash
sudo apt install mesa-vulkan-drivers
```

Verify that Dozen is now available:

``` bash
ls /usr/share/vulkan/icd.d/ | grep dzn
```

Expected result:

``` text
dzn_icd.json
```

## 5. Configure Vulkan to Use Dozen

For the current shell:

``` bash
export MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/dzn_icd.json
```

These variables are **temporary**. They only apply to the current shell
and processes started from it.

Verify Vulkan:

``` bash
vulkaninfo --summary
```

The detected devices should include:

``` text
Microsoft Direct3D12 (<NVIDIA GPU>)
Microsoft Direct3D12 (<INTEL GPU>)
```

The Vulkan driver should be:

``` text
driverName = Dozen
```

A warning such as:

``` text
WARNING: dzn is not a conformant Vulkan implementation, testing use only.
```

is expected with this configuration.

## 6. Test Vulkan Rendering

Run:

``` bash
vkcube
```

If the Vulkan cube appears and renders correctly, the Vulkan → Dozen →
D3D12 → GPU path is operational.

Dozen performs the translation to Direct3D 12 underneath the Vulkan API.
