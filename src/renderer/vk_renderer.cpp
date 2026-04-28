#include <vulkan/vulkan.h>

#include "platform.h"
#include "dds_structs.h"
#include "vk_types.h"

#ifdef WINDOWS_BUILD

#include <windows.h>
#include <vulkan/vulkan_win32.h>

#elif LINUX_BUILD
#endif

#include <iostream>

#include "vk_init.cpp"

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define VK_CHECK(res_expr)                                                                         \
    do                                                                                             \
    {                                                                                              \
        VkResult result = (res_expr);                                                              \
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)                                   \
        {                                                                                          \
            std::cout << "Vulkan Error: " << result << std::endl;                                  \
            __debugbreak();                                                                        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT msgServerity, VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{

    std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;

    return false;
}

typedef struct VkContext
{
    VkExtent2D screenSize;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapChain;
    VkCommandPool commandPool;
    VkCommandBuffer cmd;
    VkSemaphore acquireSemaphore;
    VkSemaphore submitSemaphores[5];
    VkFence inFlightFence;
    VkRenderPass renderPass;
    VkPipelineLayout pipeLayout;
    VkPipeline pipeline;

    VkDescriptorPool descPool;
    VkSampler sampler;
    VkDescriptorSet descSet;
    VkDescriptorSetLayout setLayout;

    uint32_t scImgCount;
    VkImage scImages[5];
    VkImageView scImgViews[5];
    VkFramebuffer framebuffers[5];

    Image image;
    Buffer stagingBuffer;
    int graphicsIdx;

} VkContext;

bool vk_init(VkContext* vkContext, void* window)
{
    platform_get_window_size(&vkContext->screenSize.width, &vkContext->screenSize.height);
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    appInfo.apiVersion = VK_API_VERSION_1_2;

    char* extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME};

    char* layers[] = {"VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = ArraySize(extensions);
    createInfo.enabledLayerCount = ArraySize(layers);
    createInfo.ppEnabledLayerNames = layers;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &vkContext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        vkContext->instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT)
    {

        VkDebugUtilsMessengerCreateInfoEXT debug = {};
        debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;

        debug.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)vk_debug_callback;

        vkCreateDebugUtilsMessengerEXT(vkContext->instance, &debug, 0, &vkContext->debugMessenger);
    }

    // create surface
    {
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandleA(0);
        VK_CHECK(
            vkCreateWin32SurfaceKHR(vkContext->instance, &surfaceInfo, 0, &vkContext->surface));
#elif LINUX_BUILD
#endif
    }

    // choose gpu
    {

        vkContext->graphicsIdx = -1;
        uint32_t gpuCount = 0;
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];
            uint32_t queueFamilyCount = 0;
            VkQueueFamilyProperties queueProp[10];

            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProp);

            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {

                VkBool32 surfaceSupport = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkContext->surface,
                                                              &surfaceSupport));
                if (surfaceSupport)
                {
                    vkContext->graphicsIdx = j;
                    vkContext->gpu = gpu;
                    break;
                }
            }

            if (vkContext->graphicsIdx >= 0)
            {
                break;
            }
        }

        if (vkContext->graphicsIdx < 0)
        {

            return false;
        }
    }

    // logical device
    {

        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkContext->graphicsIdx;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;

        VK_CHECK(vkCreateDevice(vkContext->gpu, &deviceInfo, 0, &vkContext->device));

        vkGetDeviceQueue(vkContext->device, vkContext->graphicsIdx, 0, &vkContext->graphicsQueue);
    }

    // swapchain
    {

        uint32_t formatCount = 0;
        VkSurfaceFormatKHR surfaceFormats[10];

        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, 0));

        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, surfaceFormats));

        bool foundFormat = false;

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkContext->surfaceFormat = format;
                foundFormat = true;
                std::cout << "Find Correct Surface Format!" << std::endl;
                break;
            }
        }

        if (!foundFormat && formatCount > 0)
        {
            std::cout << "no suitable format." << std::endl;
            vkContext->surfaceFormat = surfaceFormats[0];
        }

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext->gpu, vkContext->surface,
                                                           &surfaceCaps));

        uint32_t imgCount = surfaceCaps.minImageCount + 1;

        if (surfaceCaps.maxImageCount > 0 && imgCount > surfaceCaps.maxImageCount)
        {
            imgCount = surfaceCaps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.surface = vkContext->surface;
        scInfo.minImageCount = imgCount;
        scInfo.imageFormat = vkContext->surfaceFormat.format;
        scInfo.imageColorSpace = vkContext->surfaceFormat.colorSpace;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.imageArrayLayers = 1;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        scInfo.preTransform = surfaceCaps.currentTransform;

        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.clipped = VK_TRUE;
        scInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(vkContext->device, &scInfo, 0, &vkContext->swapChain));
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, vkContext->scImages));
    }

    // Image view
    {

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = vkContext->surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        for (uint32_t i = 0; i < vkContext->scImgCount; i++)
        {
            viewInfo.image = vkContext->scImages[i];
            VK_CHECK(vkCreateImageView(vkContext->device, &viewInfo, 0, &vkContext->scImgViews[i]));
        }
    }

    // render pass
    {

        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = vkContext->surfaceFormat.format;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription attachments[] = {colorAttachment};

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;
        rpInfo.attachmentCount = ArraySize(attachments);
        rpInfo.pSubpasses = &subpassDesc;
        rpInfo.subpassCount = 1;

        VK_CHECK(vkCreateRenderPass(vkContext->device, &rpInfo, 0, &vkContext->renderPass));
    }

    // Framebuffer
    {

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vkContext->renderPass;
        fbInfo.width = vkContext->screenSize.width;
        fbInfo.height = vkContext->screenSize.height;
        fbInfo.layers = 1;
        fbInfo.attachmentCount = 1;

        for (uint32_t i = 0; i < vkContext->scImgCount; i++)
        {
            fbInfo.pAttachments = &vkContext->scImgViews[i];
            VK_CHECK(
                vkCreateFramebuffer(vkContext->device, &fbInfo, 0, &vkContext->framebuffers[i]));
        }
    }

    // Command Pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // new
        poolInfo.queueFamilyIndex = vkContext->graphicsIdx;

        VK_CHECK(vkCreateCommandPool(vkContext->device, &poolInfo, 0, &vkContext->commandPool));
    }

    // Command buffer, new
    {
        VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkContext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkContext->device, &allocInfo, &vkContext->cmd));
    }

    // Sync objects
    {

        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0, &vkContext->acquireSemaphore));

        for (uint32_t i = 0; i < 5; i++)
        {
            VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0,
                                       &vkContext->submitSemaphores[i]));
        }

        VkFenceCreateInfo fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(vkContext->device, &fenceInfo, 0, &vkContext->inFlightFence));
    }

    // Descriptor set layouts
    {

        VkDescriptorSetLayoutBinding binding = {};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        VK_CHECK(
            vkCreateDescriptorSetLayout(vkContext->device, &layoutInfo, 0, &vkContext->setLayout));
    }

    // Pipeline layout
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &vkContext->setLayout;

        VK_CHECK(vkCreatePipelineLayout(vkContext->device, &layoutInfo, 0, &vkContext->pipeLayout));
    }

    // Create Pipeline
    {

        VkPipelineVertexInputStateCreateInfo vertextInputState = {};
        vertextInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendState.logicOpEnable = VK_FALSE;
        colorBlendState.attachmentCount = 1;
        colorBlendState.pAttachments = &colorBlendAttachment;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport = {};
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizationState.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // load shader
        // console cmd: .\glslc.exe .\shader.frag -o .\shader.frag.spv
        VkShaderModule vertextshader, fragmentShader;
        {

            uint32_t lengthInBytes;
            uint32_t* vertexCode =
                (uint32_t*)platform_read_file("assets/shaders/shader.vert.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = vertexCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkContext->device, &shaderInfo, 0, &vertextshader));

            delete vertexCode;
        }

        {

            uint32_t lengthInBytes;
            uint32_t* fragmentCode =
                (uint32_t*)platform_read_file("assets/shaders/shader.frag.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = fragmentCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkContext->device, &shaderInfo, 0, &fragmentShader));

            delete fragmentCode;
        }

        VkPipelineShaderStageCreateInfo vertStage = {};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertextshader;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragmentShader;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[2] = {vertStage, fragStage};

        VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates);
        dynamicState.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO; // graphic pipeline
        pipelineInfo.renderPass = vkContext->renderPass;
        pipelineInfo.pVertexInputState = &vertextInputState;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizationState;
        pipelineInfo.pMultisampleState = &multisampleState;
        pipelineInfo.pColorBlendState = &colorBlendState;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.stageCount = ArraySize(shaderStages);
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.layout = vkContext->pipeLayout;

        VK_CHECK(vkCreateGraphicsPipelines(vkContext->device, 0, 1, &pipelineInfo, 0,
                                           &vkContext->pipeline));

        vkDestroyShaderModule(vkContext->device, vertextshader, 0);
        vkDestroyShaderModule(vkContext->device, fragmentShader, 0);
    }

    // Staging Buffer
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = MB(1);
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VK_CHECK(
            vkCreateBuffer(vkContext->device, &bufferInfo, 0, &vkContext->stagingBuffer.buffer));

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vkContext->device, vkContext->stagingBuffer.buffer,
                                      &memRequirements);
        VkPhysicalDeviceMemoryProperties gpuMemProps;
        vkGetPhysicalDeviceMemoryProperties(vkContext->gpu, &gpuMemProps);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = MB(1);

        for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
        {
            uint32_t isCompatible = memRequirements.memoryTypeBits & (1 << i);
            VkMemoryPropertyFlags requiredFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            uint32_t hasRequiredFlags =
                (gpuMemProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags;

            if (isCompatible && hasRequiredFlags)
            {
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }

        VK_CHECK(
            vkAllocateMemory(vkContext->device, &allocInfo, 0, &vkContext->stagingBuffer.memory));
        VK_CHECK(vkMapMemory(vkContext->device, vkContext->stagingBuffer.memory, 0, MB(1), 0,
                             &vkContext->stagingBuffer.data));
        VK_CHECK(vkBindBufferMemory(vkContext->device, vkContext->stagingBuffer.buffer,
                                    vkContext->stagingBuffer.memory, 0));
    }

    // Create Image
    {

        uint32_t fileSize;
        DDSFile* data = (DDSFile*)platform_read_file("assets/textures/cakez.DDS", &fileSize);
        uint32_t textureSize = data->header.Width * data->header.Height * 4;
        memcpy(vkContext->stagingBuffer.data, &data->dataBegin, textureSize);

        VkImageCreateInfo imgInfo = {};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgInfo.extent = {data->header.Width, data->header.Height, 1};
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        VK_CHECK(vkCreateImage(vkContext->device, &imgInfo, 0, &vkContext->image.image));

        // 获取图片的内存需求
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(vkContext->device, vkContext->image.image, &memRequirements);

        // 获取GPU所提供的内存需求
        VkPhysicalDeviceMemoryProperties gpuMemProps;
        vkGetPhysicalDeviceMemoryProperties(vkContext->gpu, &gpuMemProps);

        // 过滤出GPU上适合图片的内存类型索引
        VkMemoryAllocateInfo allocInfo = {};
        for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
        {
            if (memRequirements.memoryTypeBits & (1 << i) &&
                (gpuMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) ==
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            {
                allocInfo.memoryTypeIndex = i;
            }
        }

        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = textureSize;
        VK_CHECK(vkAllocateMemory(vkContext->device, &allocInfo, 0, &vkContext->image.memory));

        VK_CHECK(vkBindImageMemory(vkContext->device, vkContext->image.image,
                                   vkContext->image.memory, 0));

        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(vkContext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkContext->device, &cmdAlloc, &cmd));

        VkCommandBufferBeginInfo beginIngo = cmd_begin_info();
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginIngo));

        VkImageSubresourceRange range = {};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.layerCount = 1;
        range.levelCount = 1;

        VkImageMemoryBarrier imgMemBarrier = {};
        imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgMemBarrier.image = vkContext->image.image;
        imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarrier.subresourceRange = range;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, 0, 0, 0, 1, &imgMemBarrier);

        // copy staging data to gpu
        VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent = {data->header.Width, data->header.Height, 1};
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vkCmdCopyBufferToImage(cmd, vkContext->stagingBuffer.buffer, vkContext->image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 准备好给shader读取了
        imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1,
                             &imgMemBarrier);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkFence uploadFence;
        VkFenceCreateInfo fenceInfo = fence_info();
        VK_CHECK(vkCreateFence(vkContext->device, &fenceInfo, 0, &uploadFence));

        VkSubmitInfo submitInfo = submit_info(&cmd);
        VK_CHECK(vkQueueSubmit(vkContext->graphicsQueue, 1, &submitInfo, uploadFence));
        VK_CHECK(vkWaitForFences(vkContext->device, 1, &uploadFence, true, UINT64_MAX));
    }

    // Create Image View
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = vkContext->image.image;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        VK_CHECK(vkCreateImageView(vkContext->device, &viewInfo, 0, &vkContext->image.view));
    }

    // Create Sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;

        VK_CHECK(vkCreateSampler(vkContext->device, &samplerInfo, 0, &vkContext->sampler));
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;

        VK_CHECK(vkCreateDescriptorPool(vkContext->device, &poolInfo, 0, &vkContext->descPool));
    }

    // Create Descriptor Set
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pSetLayouts = &vkContext->setLayout;
        allocInfo.descriptorSetCount = 1;
        allocInfo.descriptorPool = vkContext->descPool;
        VK_CHECK(vkAllocateDescriptorSets(vkContext->device, &allocInfo, &vkContext->descSet));
    }

    // Update Descriptor Set
    {
        VkDescriptorImageInfo imgInfo = {};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView = vkContext->image.view;
        imgInfo.sampler = vkContext->sampler;

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = vkContext->descSet;
        write.pImageInfo = &imgInfo;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

        vkUpdateDescriptorSets(vkContext->device, 1, &write, 0, 0);
    }

    return true;
}

bool vk_render(VkContext* vkContext)
{

    vkWaitForFences(vkContext->device, 1, &vkContext->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkContext->device, 1, &vkContext->inFlightFence);

    uint32_t imgIdx;

    VK_CHECK(vkAcquireNextImageKHR(vkContext->device, vkContext->swapChain, 0,
                                   vkContext->acquireSemaphore, 0, &imgIdx));

    VkCommandBuffer cmd = vkContext->cmd;
    vkResetCommandBuffer(cmd, 0); // new

    // VkCommandBufferAllocateInfo allocInfo = {};
    // allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // allocInfo.commandPool = vkContext->commandPool;
    // allocInfo.commandBufferCount = 1;

    // VK_CHECK(vkAllocateCommandBuffers(vkContext->device, &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // 清屏的颜色：黄
    VkClearValue color = {1, 1, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = vkContext->renderPass;
    rpBeginInfo.framebuffer = vkContext->framebuffers[imgIdx];
    rpBeginInfo.renderArea.offset = {0, 0};
    rpBeginInfo.renderArea.extent = vkContext->screenSize;
    rpBeginInfo.clearValueCount = 1;
    rpBeginInfo.pClearValues = &color;

    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Rendering Command
    {

        VkViewport viewPort = {};
        viewPort.width = (float)vkContext->screenSize.width;
        viewPort.height = (float)vkContext->screenSize.height;
        viewPort.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewPort);

        VkRect2D scissor = {};
        scissor.extent = vkContext->screenSize;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeLayout, 0, 1,
                                &vkContext->descSet, 0, 0);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeline);

        vkCmdDraw(cmd, 6, 1, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkContext->acquireSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &vkContext->submitSemaphores[imgIdx];

    VK_CHECK(vkQueueSubmit(vkContext->graphicsQueue, 1, &submitInfo, vkContext->inFlightFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &vkContext->submitSemaphores[imgIdx];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &vkContext->swapChain;
    presentInfo.pImageIndices = &imgIdx;

    VK_CHECK(vkQueuePresentKHR(vkContext->graphicsQueue, &presentInfo));

    return true;
}
