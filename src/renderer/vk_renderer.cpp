#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif

#include "dds_structs.h"
#include "logger.h"
#include "platform.h"

#include "vk_types.h"
#include "vk_init.cpp"
#include "vk_util.cpp"
#include "vk_shader_util.cpp"

#include "game/game.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT msgServerity, VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    CAKEZ_ERROR(pCallbackData->pMessage);
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
    VkSampler sampler, samplerFire, samplerWater, samplerWood, samplerLight, samplerDark,
        samplerHeart;
    VkDescriptorSet boardDescSet;
    VkDescriptorSet fireDescSet;
    VkDescriptorSet waterDescSet;
    VkDescriptorSet woodDescSet;
    VkDescriptorSet lightDescSet;
    VkDescriptorSet darkDescSet;
    VkDescriptorSet heartDescSet;
    VkDescriptorSetLayout setLayout;

    uint32_t scImgCount;
    VkImage scImages[5];
    VkImageView scImgViews[5];
    VkFramebuffer framebuffers[5];

    Image image;
    Image imageFire, imageWater, imageWood, imageLight, imageDark, imageHeart;
    Buffer stagingBuffer;
    Buffer tranformStorageBuffer;
    Buffer globalUBO;
    Buffer indexBuffer;

    int graphicsIdx;

} VkContext;

bool vk_init(VkContext* vkContext, void* window)
{
    vk_compile_shader("assets/shaders/shader.vert", "assets/shaders/compiled/shader.vert.spv");
    vk_compile_shader("assets/shaders/shader.frag", "assets/shaders/compiled/shader.frag.spv");

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
                CAKEZ_TRACE("Find Correct Surface Format!");
                break;
            }
        }

        if (!foundFormat && formatCount > 0)
        {
            CAKEZ_WARN("no suitable format.");
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

    // Command buffer
    {
        VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkContext->commandPool);
        VK_CHECK(vkAllocateCommandBuffers(vkContext->device, &allocInfo, &vkContext->cmd));
    }

    // Sync objects
    {

        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0, &vkContext->acquireSemaphore));

        // Create one submit semaphore per swapchain image
        for (uint32_t i = 0; i < vkContext->scImgCount; i++)
        {
            VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0,
                                       &vkContext->submitSemaphores[i]));
        }

        VkFenceCreateInfo fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
        VK_CHECK(vkCreateFence(vkContext->device, &fenceInfo, 0, &vkContext->inFlightFence));
    }

    // Descriptor set layouts
    // 这里绑定多个layout:顶点的2个，片段的1个
    {

        VkDescriptorSetLayoutBinding bindings[] = {
            layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 0),
            layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, 1),
            layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT,
                           1, 2),
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = ArraySize(bindings);
        layoutInfo.pBindings = bindings;

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
        rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationState.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // load shader
        // console cmd: .\glslc.exe .\shader.frag -o .\shader.frag.spv
        VkShaderModule vertextshader, fragmentShader;
        {

            uint32_t lengthInBytes;
            uint32_t* vertexCode = (uint32_t*)platform_read_file(
                "assets/shaders/compiled/shader.vert.spv", &lengthInBytes);

            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = vertexCode;
            shaderInfo.codeSize = lengthInBytes;
            VK_CHECK(vkCreateShaderModule(vkContext->device, &shaderInfo, 0, &vertextshader));

            delete vertexCode;
        }

        {

            uint32_t lengthInBytes;
            uint32_t* fragmentCode = (uint32_t*)platform_read_file(
                "assets/shaders/compiled/shader.frag.spv", &lengthInBytes);

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
        vkContext->stagingBuffer = vk_allocate_buffer(
            vkContext->device, vkContext->gpu, MB(1), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }

    // Create Image
    {
        //=====base
        vk_upload_dds(vkContext->device, vkContext->gpu, vkContext->commandPool,
                      vkContext->graphicsQueue, &vkContext->stagingBuffer,
                      "assets/textures/base.dds", &vkContext->image);
        //=====fire
        vk_upload_dds(vkContext->device, vkContext->gpu, vkContext->commandPool,
                      vkContext->graphicsQueue, &vkContext->stagingBuffer, "assets/textures/f.dds",
                      &vkContext->imageFire);
        //=====water
        vk_upload_dds(vkContext->device, vkContext->gpu, vkContext->commandPool,
                      vkContext->graphicsQueue, &vkContext->stagingBuffer, "assets/textures/w.dds",
                      &vkContext->imageWater);
    }

    // Create Image View
    {
        vk_create_image_view(vkContext->device, vkContext->image.image, VK_FORMAT_B8G8R8A8_SRGB,
                             &vkContext->image.view);
        vk_create_image_view(vkContext->device, vkContext->imageFire.image, VK_FORMAT_B8G8R8A8_SRGB,
                             &vkContext->imageFire.view);
        vk_create_image_view(vkContext->device, vkContext->imageWater.image,
                             VK_FORMAT_B8G8R8A8_SRGB, &vkContext->imageWater.view);
    }

    // Create Sampler
    {
        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.magFilter = VK_FILTER_NEAREST;

        // 这里标识4x4的棋盘可以repeat到6x6
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        VK_CHECK(vkCreateSampler(vkContext->device, &samplerInfo, 0, &vkContext->sampler));

        VkSamplerCreateInfo samplerInfoFire = {};
        samplerInfoFire.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfoFire.minFilter = VK_FILTER_NEAREST;
        samplerInfoFire.magFilter = VK_FILTER_NEAREST;

        samplerInfoFire.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfoFire.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfoFire.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VK_CHECK(vkCreateSampler(vkContext->device, &samplerInfoFire, 0, &vkContext->samplerFire));

        VkSamplerCreateInfo samplerInfoWater = {};
        samplerInfoWater.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfoWater.minFilter = VK_FILTER_NEAREST;
        samplerInfoWater.magFilter = VK_FILTER_NEAREST;

        samplerInfoWater.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfoWater.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfoWater.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        VK_CHECK(
            vkCreateSampler(vkContext->device, &samplerInfoWater, 0, &vkContext->samplerWater));
    }

    // Create Transform storage buffer
    {
        vkContext->tranformStorageBuffer = vk_allocate_buffer(
            vkContext->device, vkContext->gpu, sizeof(Transform) * MAX_ENTITIES,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    // Create Uniform Buffer
    {
        vkContext->globalUBO = vk_allocate_buffer(
            vkContext->device, vkContext->gpu, sizeof(GlobalData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        GlobalData globalData = {
            (int)vkContext->screenSize.width,
            (int)vkContext->screenSize.height,
        };
        vk_copy_to_buffer(&vkContext->globalUBO, &globalData, sizeof(globalData));
    }

    // Create Index buffer
    {
        vkContext->indexBuffer = vk_allocate_buffer(
            vkContext->device, vkContext->gpu, sizeof(uint32_t) * 6,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        // copy indices to the buffer
        {
            uint32_t indices[] = {0, 1, 2, 2, 3, 0};
            vk_copy_to_buffer(&vkContext->indexBuffer, indices, sizeof(uint32_t) * 6);
        }
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                                            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
                                            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3}};

        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 3; // 多个set
        poolInfo.poolSizeCount = ArraySize(poolSizes);
        poolInfo.pPoolSizes = poolSizes;

        VK_CHECK(vkCreateDescriptorPool(vkContext->device, &poolInfo, 0, &vkContext->descPool));
    }

    // Create Descriptor Set
    {
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pSetLayouts = &vkContext->setLayout;
        allocInfo.descriptorSetCount = 1;
        allocInfo.descriptorPool = vkContext->descPool;

        VK_CHECK(vkAllocateDescriptorSets(vkContext->device, &allocInfo, &vkContext->boardDescSet));
        VK_CHECK(vkAllocateDescriptorSets(vkContext->device, &allocInfo, &vkContext->fireDescSet));
        VK_CHECK(vkAllocateDescriptorSets(vkContext->device, &allocInfo, &vkContext->waterDescSet));
    }

    // Update Descriptor Set
    {
        DescriptorInfo descInfos[] = {DescriptorInfo(vkContext->globalUBO.buffer),
                                      DescriptorInfo(vkContext->tranformStorageBuffer.buffer),
                                      DescriptorInfo(vkContext->sampler, vkContext->image.view)};

        VkWriteDescriptorSet writes[] = {
            write_set(vkContext->boardDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descInfos[0], 0,
                      1),
            write_set(vkContext->boardDescSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfos[1], 1,
                      1),
            write_set(vkContext->boardDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      &descInfos[2], 2, 1),
        };

        vkUpdateDescriptorSets(vkContext->device, ArraySize(descInfos), writes, 0, 0);

        // fire
        descInfos[2] = {// DescriptorInfo(vkContext->globalUBO.buffer),
                        // DescriptorInfo(vkContext->tranformStorageBuffer.buffer),
                        DescriptorInfo(vkContext->samplerFire, vkContext->imageFire.view)};

        VkWriteDescriptorSet writesFire[] = {
            write_set(vkContext->fireDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descInfos[0], 0,
                      1),
            write_set(vkContext->fireDescSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfos[1], 1,
                      1),
            write_set(vkContext->fireDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      &descInfos[2], 2, 1),
        };

        vkUpdateDescriptorSets(vkContext->device, ArraySize(descInfos), writesFire, 0, 0);

        // fire
        descInfos[2] = {// DescriptorInfo(vkContext->globalUBO.buffer),
                        // DescriptorInfo(vkContext->tranformStorageBuffer.buffer),
                        DescriptorInfo(vkContext->samplerWater, vkContext->imageWater.view)};

        VkWriteDescriptorSet writesWater[] = {
            write_set(vkContext->waterDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descInfos[0], 0,
                      1),
            write_set(vkContext->waterDescSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descInfos[1], 1,
                      1),
            write_set(vkContext->waterDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                      &descInfos[2], 2, 1),
        };

        vkUpdateDescriptorSets(vkContext->device, ArraySize(descInfos), writesWater, 0, 0);
    }

    return true;
}

bool vk_render(VkContext* vkContext, GameState* gameState)
{
    uint32_t imgIdx;

    // wait on the GPU to be done
    vkWaitForFences(vkContext->device, 1, &vkContext->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkContext->device, 1, &vkContext->inFlightFence);

    // copy transforms to the buffer
    {
        Transform gpuTransforms[MAX_ENTITIES];
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            gpuTransforms[i] = gameState->entities[i].transform;
        }
        vk_copy_to_buffer(&vkContext->tranformStorageBuffer, gpuTransforms,
                          sizeof(Transform) * gameState->entityCount);
        // vk_copy_to_buffer(&vkContext->tranformStorageBuffer, &gameState->entities,
        //                   sizeof(Transform) * gameState->entityCount);
    }

    VK_CHECK(vkAcquireNextImageKHR(vkContext->device, vkContext->swapChain, UINT64_MAX,
                                   vkContext->acquireSemaphore, 0, &imgIdx));

    VkCommandBuffer cmd = vkContext->cmd;
    vkResetCommandBuffer(cmd, 0); // new

    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // 清屏的颜色
    VkClearValue color = {0, 0, 0, 1};

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
                                &vkContext->boardDescSet, 0, 0);
        // 绑定索引缓冲
        vkCmdBindIndexBuffer(cmd, vkContext->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeline);
        vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);

        // fire
        for (uint32_t i = 1; i < gameState->entityCount; i++)
        {
            Entity* e = &gameState->entities[i];
            switch (e->orbType)
            {
            case ORB_FIRE:
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeLayout,
                                        0, 1, &vkContext->fireDescSet, 0, 0);
                break;
            case ORB_WATER:
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeLayout,
                                        0, 1, &vkContext->waterDescSet, 0, 0);
                break;
            }
            vkCmdDrawIndexed(cmd, 6, 1, 0, 0, i); // 这里i是实例 glInstance
        }
    }

    vkCmdEndRenderPass(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = submit_info(&cmd);
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.pSignalSemaphores = &vkContext->submitSemaphores[imgIdx];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkContext->acquireSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(vkContext->graphicsQueue, 1, &submitInfo, vkContext->inFlightFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkContext->swapChain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imgIdx;
    presentInfo.pWaitSemaphores = &vkContext->submitSemaphores[imgIdx];
    presentInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueuePresentKHR(vkContext->graphicsQueue, &presentInfo));

    return true;
}
