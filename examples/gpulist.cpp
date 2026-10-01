#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>

int main()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "WSL Vulkan Test";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "vkCreateInstance failed\n";
        return 1;
    }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());

    std::cout << "Found " << count << " GPU(s):\n";

    for (auto device : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);

        std::cout << "  " << props.deviceName << "\n";
        std::cout << "    vendorID: 0x" << std::hex << props.vendorID << std::dec << "\n";
        std::cout << "    deviceID: 0x" << std::hex << props.deviceID << std::dec << "\n";
        std::cout << "    API: "
                  << VK_VERSION_MAJOR(props.apiVersion) << "."
                  << VK_VERSION_MINOR(props.apiVersion) << "."
                  << VK_VERSION_PATCH(props.apiVersion) << "\n";
    }

    vkDestroyInstance(instance, nullptr);
    return 0;
}
