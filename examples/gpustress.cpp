#include <vulkan/vulkan.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <chrono>
// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------
constexpr uint32_t ELEMENT_COUNT = 1024 * 1024;
constexpr uint32_t LOCAL_SIZE    = 256;
constexpr const char* SPIRV_FILENAME = "../shaders/gpustress.spv";

// -----------------------------------------------------------------------------
// Load SPIR-V
// -----------------------------------------------------------------------------
std::vector<uint32_t> loadSpirv(const char* filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot open SPIR-V file");
    const std::streamsize size = file.tellg();
    if (size <= 0 || size % sizeof(uint32_t) != 0)
        throw std::runtime_error("Invalid SPIR-V file size");
    std::vector<uint32_t> code(size / sizeof(uint32_t));
    file.seekg(0);
    file.read(
        reinterpret_cast<char*>(code.data()),
        size
    );
    if (!file)
        throw std::runtime_error("Failed to read SPIR-V file");
    return code;
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------
int main()
{
    try
    {
        // ---------------------------------------------------------------------
        // Vulkan instance
        // ---------------------------------------------------------------------
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Vulkan GPU Stress";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "None";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;
        VkInstance instance = VK_NULL_HANDLE;
        if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }
        std::cout << "Vulkan instance created\n";
        // ---------------------------------------------------------------------
        // Select NVIDIA physical device
        // ---------------------------------------------------------------------
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
            throw std::runtime_error("No Vulkan physical devices found");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        for (VkPhysicalDevice candidate : devices)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            std::cout << "Found GPU: " << properties.deviceName << '\n';
            // NVIDIA PCI vendor ID
            if (properties.vendorID == 0x10de)
            {
                physicalDevice = candidate;
                std::cout << "Selected NVIDIA GPU: " << properties.deviceName << '\n';
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE)
            throw std::runtime_error("NVIDIA GPU not found");
        // ---------------------------------------------------------------------
        // Find compute queue family
        // ---------------------------------------------------------------------
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        uint32_t computeQueueFamily = UINT32_MAX;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                computeQueueFamily = i;
                break;
            }
        }
        if (computeQueueFamily == UINT32_MAX)
            throw std::runtime_error("No compute queue family found");
        std::cout << "Compute queue family: " << computeQueueFamily << '\n';
        // ---------------------------------------------------------------------
        // Logical device
        // ---------------------------------------------------------------------
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = computeQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create logical device");
        }
        VkQueue computeQueue = VK_NULL_HANDLE;
        vkGetDeviceQueue(device, computeQueueFamily, 0, &computeQueue);
        std::cout << "Logical device created\n";
        std::cout << "Compute queue acquired\n";
        // ---------------------------------------------------------------------
        // Load shader
        // ---------------------------------------------------------------------
        auto spirv = loadSpirv(SPIRV_FILENAME);
        std::cout << "Loaded " << SPIRV_FILENAME << ": " << spirv.size() * sizeof(uint32_t) << " bytes\n";
        VkShaderModuleCreateInfo shaderInfo{};
        shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize = spirv.size() * sizeof(uint32_t);
        shaderInfo.pCode = spirv.data();
        VkShaderModule shaderModule = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }
        std::cout << "Shader module created\n";
        // ---------------------------------------------------------------------
        // Descriptor set layout
        //
        // Corresponds to:
        //
        // layout(set = 0, binding = 0) buffer DataBuffer
        // ---------------------------------------------------------------------
        VkDescriptorSetLayoutBinding bufferBinding{};
        bufferBinding.binding = 0;
        bufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferBinding.descriptorCount = 1;
        bufferBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = 1;
        descriptorLayoutInfo.pBindings = &bufferBinding;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout");
        }
        // ---------------------------------------------------------------------
        // Pipeline layout
        // ---------------------------------------------------------------------
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
        // ---------------------------------------------------------------------
        // Compute pipeline
        // ---------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        shaderStage.module = shaderModule;
        shaderStage.pName = "main";
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStage;
        pipelineInfo.layout = pipelineLayout;
        VkPipeline computePipeline = VK_NULL_HANDLE;
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline");
        }
        std::cout << "Compute pipeline created\n";
        // ---------------------------------------------------------------------
        // Storage buffer
        // ---------------------------------------------------------------------
        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(ELEMENT_COUNT) * sizeof(float);
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkBuffer dataBuffer = VK_NULL_HANDLE;
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &dataBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create storage buffer");
        }
        // ---------------------------------------------------------------------
        // Allocate buffer memory
        // ---------------------------------------------------------------------
        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device, dataBuffer, &memoryRequirements);
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
        uint32_t memoryTypeIndex = UINT32_MAX;
        const VkMemoryPropertyFlags requiredProperties =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            const bool supported = (memoryRequirements.memoryTypeBits & (1u << i)) != 0;
            const auto flags = memoryProperties.memoryTypes[i].propertyFlags;
            const bool suitable = (flags & requiredProperties) == requiredProperties;
            if (supported && suitable)
            {
                memoryTypeIndex = i;
                break;
            }
        }
        if (memoryTypeIndex == UINT32_MAX)
            throw std::runtime_error("No HOST_VISIBLE | HOST_COHERENT memory");
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = memoryTypeIndex;
        VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate buffer memory");
        }
        if (vkBindBufferMemory(device, dataBuffer, bufferMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("Failed to bind buffer memory");
        }
        std::cout << "Storage buffer: " << bufferSize / (1024 * 1024) << " MiB\n";
        // ---------------------------------------------------------------------
        // Initialize buffer
        // ---------------------------------------------------------------------
        void* mapped = nullptr;
        if (vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map memory");
        }
        float* data = static_cast<float*>(mapped);
        for (uint32_t i = 0; i < ELEMENT_COUNT; ++i)
        {
            data[i] = 0.001f * static_cast<float>(i + 1);
        }
        std::cout << "Before GPU: " << data[0] << ", " << data[1] << ", " << data[2] << '\n';
        vkUnmapMemory(device, bufferMemory);
        // ---------------------------------------------------------------------
        // Descriptor pool
        // ---------------------------------------------------------------------
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }
        // ---------------------------------------------------------------------
        // Allocate descriptor set
        // ---------------------------------------------------------------------
        VkDescriptorSetAllocateInfo descriptorAlloc{};
        descriptorAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorAlloc.descriptorPool = descriptorPool;
        descriptorAlloc.descriptorSetCount = 1;
        descriptorAlloc.pSetLayouts = &descriptorSetLayout;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        if (vkAllocateDescriptorSets(device, &descriptorAlloc, &descriptorSet) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor set");
        }
        // ---------------------------------------------------------------------
        // Connect storage buffer to binding 0
        // ---------------------------------------------------------------------
        VkDescriptorBufferInfo descriptorBuffer{};
        descriptorBuffer.buffer = dataBuffer;
        descriptorBuffer.offset = 0;
        descriptorBuffer.range = bufferSize;
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.pBufferInfo = &descriptorBuffer;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        std::cout << "Descriptor set configured\n";
        // ---------------------------------------------------------------------
        // Command pool
        // ---------------------------------------------------------------------
        VkCommandPoolCreateInfo commandPoolInfo{};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.queueFamilyIndex = computeQueueFamily;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool");
        }
        // ---------------------------------------------------------------------
        // Command buffer
        // ---------------------------------------------------------------------
        VkCommandBufferAllocateInfo commandBufferInfo{};
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferInfo.commandPool = commandPool;
        commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        if (vkAllocateCommandBuffers(device, &commandBufferInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer");
        }
        // ---------------------------------------------------------------------
        // Record compute commands
        // ---------------------------------------------------------------------
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        const uint32_t groupCount = (ELEMENT_COUNT + LOCAL_SIZE - 1) / LOCAL_SIZE;
        vkCmdDispatch(commandBuffer, groupCount, 1, 1);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
        std::cout << "Dispatch: " << groupCount << " workgroups x " << LOCAL_SIZE << " invocations\n";
        // ---------------------------------------------------------------------
        // Submit to GPU
        // ---------------------------------------------------------------------
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        std::cout << "Starting GPU computation...\n";
        const auto start = std::chrono::high_resolution_clock::now();
        if (vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit compute work");
        }
        vkQueueWaitIdle(computeQueue);
        const auto end = std::chrono::high_resolution_clock::now();
        const double seconds = std::chrono::duration<double>(end - start).count();
        std::cout << "GPU computation finished in " << seconds << " seconds\n";
        // ---------------------------------------------------------------------
        // Read result back
        // ---------------------------------------------------------------------
        if (vkMapMemory(device, bufferMemory, 0, bufferSize, 0, &mapped) != VK_SUCCESS) {
            throw std::runtime_error("Failed to map result buffer");
        }
        data = static_cast<float*>(mapped);
        std::cout << "After GPU: " << data[0] << ", " << data[1] << ", " << data[2] << '\n';
        vkUnmapMemory(device, bufferMemory);
        // ---------------------------------------------------------------------
        // Cleanup
        // ---------------------------------------------------------------------
        vkDestroyCommandPool(device, commandPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyBuffer(device, dataBuffer, nullptr);
        vkFreeMemory(device, bufferMemory, nullptr);
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, shaderModule, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        std::cout << "Done\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
