#include <iostream>
#include <vulkan/vulkan.h>

typedef struct VkContext {
    VkInstance instance;
} VkContext;

bool initWin32VkInstance(VkContext* context) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, nullptr, &context->instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        return false;
    } else {
        std::cout << "Vulkan instance created successfully!" << std::endl;
    }

    return true;
}