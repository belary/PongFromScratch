#include <vulkan/vulkan.h>

#include "platform.h"
#include "dds_structs.h"
#include "vk_types.h"

#ifdef WINDOWS_BUILD
// ============================================================================
// Windows 平台头文件包含顺序（学习重点）
// ============================================================================
//
// 为什么顺序很重要？—— C++ 编译器的"所见即所得"原则
//
// 编译器从上到下逐行解析代码，只能"看到"之前已经定义/包含的内容
//
// 依赖链：
//   vulkan_win32.h  →  使用了 HWND, HANDLE, HINSTANCE 等类型
//                         ↓
//                    这些类型定义在 <windows.h> 中
//
// 错误示例：
//   #include <vulkan_win32.h>  ❌ 编译器：HWND 是什么？不知道！报错！
//   #include <windows.h>       ← 现在定义 HWND 了，但上面已经编译失败了
//
// 正确示例（当前代码）：
//   #include <windows.h>       ← 先定义 Windows 类型
//   #include <vulkan_win32.h>  ✅ 编译器：我知道 HWND 是什么，继续解析
//
// vulkan_win32.h 内部简化示例：
//   typedef struct VkWin32SurfaceCreateInfoKHR {
//       HWND hwnd;              // ⚠️ 需要先定义 HWND
//       HINSTANCE hinstance;    // ⚠️ 需要先定义 HINSTANCE
//   } VkWin32SurfaceCreateInfoKHR;
//
// 为什么教程可能没这个问题？
//   1. 其他头文件间接包含了 <windows.h>
//   2. 使用了预编译头（stdafx.h, pch.h）
//   3. 不同编译器的包含顺序检查更宽松
// ============================================================================

#include <windows.h>
#include <vulkan/vulkan_win32.h>

#elif LINUX_BUILD
#endif

#include <iostream>

#include "vk_init.cpp"

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

// 点击“关闭”按钮的瞬间，窗口的大小或状态会发生剧烈波动（比如最小化动画），这时驱动可能会先返回一个
// SUBOPTIMAL
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

/**
 * Vulkan 调试回调函数
 *
 * 此函数被 Vulkan Validation Layer 调用，用于接收和显示调试信息。
 * 当应用程序违反 Vulkan 规范或存在潜在问题时，验证层会调用此函数。
 *
 * -----
 * 宏定义说明（学习重点）：
 *
 * VKAPI_ATTR:
 *   - 在 Windows 上定义为 __declspec(dllexport) 或空
 *   - 用于控制函数的导出/导入属性
 *   - 确保函数在 DLL 边界正确工作
 *
 * VKAPI_CALL:
 *   - 在 Windows 上定义为 __stdcall（WINAPI）
 *   - 在 Linux/macOS 上通常为空（使用默认调用约定）
 *   - __stdcall 调用约定：调用者压栈，被调者清理栈（固定参数）
 *   - C++ 默认 __cdecl：调用者压栈，调用者清理栈（可变参数）
 *
 * 为什么需要这些宏？
 *   Vulkan 需要确保跨平台、跨编译器的二进制兼容性
 *   Windows API 统一使用 __stdcall，所以 Vulkan 在 Windows 上也必须使用
 *
 * -----
 *
 * @param msgServerity  消息严重性级别（Error/Warning/Info/Verbose）
 * @param msgFlags      消息类型（通用/性能/规范验证）
 * @param pCallbackData 包含详细调试信息的结构体（pMessage 字段包含错误消息）
 * @param pUserData     用户自定义数据（创建 messenger 时传入的指针）
 *
 * @return VkBool32     返回 VK_TRUE 表示应该终止 Vulkan 调用
 *                      返回 VK_FALSE 表示继续执行（通常使用此选项）
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagsEXT msgServerity, VkDebugUtilsMessageTypeFlagsEXT msgFlags,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    // 输出验证层的错误/警告消息到控制台
    std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;

    // 返回 false 表示不终止 Vulkan 调用，让程序继续执行
    // 这样可以一次性看到所有验证错误，而不是在第一个错误处停止
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
    VkSemaphore acquireSemaphore;
    VkSemaphore submitSemaphores[5]; // 每个交换链图像一个独立的提交信号量
    VkFence inFlightFence;           // 围栏：CPU 等待 GPU 完成上一帧
    VkRenderPass renderPass;
    VkPipelineLayout pipeLayout;
    VkPipeline pipeline;

    uint32_t scImgCount;
    VkImage scImages[5];
    VkImageView scImgViews[5];
    VkFramebuffer framebuffers[5];

    Buffer stagingBuffer;
    int graphicsIdx;

} VkContext;

void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
                             VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool vk_init(VkContext* vkContext, void* window)
{
    platform_get_window_size(&vkContext->screenSize.width, &vkContext->screenSize.height);
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong From Scratch";
    appInfo.pEngineName = "Pong Engine";

    // apiVersion: 应用请求的 Vulkan API 版本
    //
    // 重要：必须设置此字段，否则默认为 Vulkan 1.0
    //
    // VK_API_VERSION_1_0 (0x400000): SPIR-V 1.0 支持
    // VK_API_VERSION_1_1 (0x401000): SPIR-V 1.3 支持
    // VK_API_VERSION_1_2 (0x402000): SPIR-V 1.5 支持
    // VK_API_VERSION_1_3 (0x403000): SPIR-V 1.6 支持
    //
    // 宏定义格式：VK_MAKE_VERSION(major, minor, patch)
    // 例如：VK_API_VERSION_1_2 = VK_MAKE_VERSION(1, 2, 0)
    //
    appInfo.apiVersion = VK_API_VERSION_1_2;
    // ↑ 使用 Vulkan 1.2，支持大多数现代着色器特性

    // Vulkan 实例扩展列表
    //
    // 扩展说明：
    // 1. VK_KHR_win32_surface (Windows): 创建 Vulkan 窗口表面所需
    // 2. VK_EXT_debug_utils: 调试消息功能
    // 3. VK_KHR_surface: 跨平台表面抽象层
    //
    // 为什么需要这些扩展？
    // - Vulkan 核心不包含平台特定的窗口系统集成
    // - 需要通过扩展来启用 Win32/Linux 等平台支持
    //
    char* extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME, // Windows 平台表面扩展
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, // 调试工具扩展
        VK_KHR_SURFACE_EXTENSION_NAME      // 表面抽象层扩展
    };

    char* layers[] = {"VK_LAYER_KHRONOS_validation"};

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledExtensionNames = extensions;
    createInfo.enabledExtensionCount = ArraySize(extensions); // ⚠️ 关键：必须设置扩展数量！
    createInfo.enabledLayerCount = ArraySize(layers);
    createInfo.ppEnabledLayerNames = layers;
    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &vkContext->instance));

    // 动态加载 Vulkan 扩展函数
    //
    // Vulkan 核心函数（如 vkCreateInstance）直接链接到 Vulkan 驱动
    // 扩展函数（如 vkCreateDebugUtilsMessengerEXT）需要运行时动态加载
    //
    // vkGetInstanceProcAddr:
    //   - 根据函数名字符串返回函数指针
    //   - 如果扩展不存在，返回 NULL
    //
    // PFN_ 前缀说明：
    //   - PFN = Pointer to FuNction（Vulkan 命名约定）
    //   - 所有 Vulkan 函数指针类型都有 PFN_ 前缀
    //   - 例如：PFN_vkCreateDebugUtilsMessengerEXT
    //
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        vkContext->instance, "vkCreateDebugUtilsMessengerEXT");
    // 检查扩展是否可用（可选扩展可能不存在）
    if (vkCreateDebugUtilsMessengerEXT)
    {
        // 配置调试消息接收器
        VkDebugUtilsMessengerCreateInfoEXT debug = {};
        debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

        // messageSeverity: 控制接收哪些严重级别的消息
        //   - ERROR: 错误（必须处理）
        //   - WARNING: 警告（建议修复）
        //   - INFO: 信息（可选）
        //   - VERBOSE: 详细诊断（开发调试用）
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;

        // messageType: 控制接收哪些类型的消息
        //   - GENERAL: 通用消息（非规范相关的）
        //   - VALIDATION: 规范验证错误（违反 Vulkan 规范）
        //   - PERFORMANCE: 性能警告（非最优用法）
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        // 设置回调函数指针
        //
        // 为什么需要强制转换？
        // ----------------------
        // 1. PFN_vkDebugUtilsMessengerCallbackEXT 是 Vulkan 定义的函数指针类型：
        //    typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(...);
        //
        // 2. VKAPI_PTR 在 Windows 上展开为 __stdcall（与 VKAPI_CALL 一致）
        // 3. 但 MSVC 编译器推断函数指针时可能使用 __cdecl（C++ 默认）
        // 4. __stdcall 和 __cdecl 的调用约定不同，编译器认为类型不兼容
        // 5. 因此需要显式转换告诉编译器："我知道这是安全的，请转换"
        //
        // 这是 Windows + MSVC 特有的问题，GCC/Clang 通常不需要
        debug.pfnUserCallback = (PFN_vkDebugUtilsMessengerCallbackEXT)vk_debug_callback;

        // 创建调试消息接收器
        // 参数说明：
        //   1. instance: Vulkan 实例
        //   2. &debug: 创建信息结构体
        //   3. 0: 分配器（NULL = 使用默认分配器）
        //   4. &debugMessenger: 输出参数，返回 messenger 句柄
        vkCreateDebugUtilsMessengerEXT(vkContext->instance, &debug, 0, &vkContext->debugMessenger);
    }
    // 注意：即使调试扩展加载失败也不应该阻止程序运行
    // 调试功能是可选的，失败时只打印警告，继续执行

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

    // ============================================================================
    // 选择物理设备（GPU）和查找支持渲染的队列族
    // ============================================================================
    //
    // Vulkan 核心概念：
    //
    // 1. Physical Device（物理设备）
    //    - 代表系统中的 GPU（可能有多个：集显 + 独显）
    //    - VkPhysicalDevice 是 GPU 的句柄，不是创建对象，只是引用
    //
    // 2. Queue Family（队列族）
    //    - GPU 中不同类型的工作队列分组
    //    - 常见类型：Graphics（渲染）、Compute（计算）、Transfer（数据传输）
    //    - 同一族内的队列共享资源，不同族之间相对独立
    //
    // 3. 为什么需要检查 Surface 支持？
    //    - 不是所有队列都能向窗口渲染
    //    - 有些 GPU 的计算队列不支持图形输出
    //    - 必须找到既支持图形操作又能渲染到 surface 的队列族
    //
    // 4. Vulkan 两阶段查询模式（常见模式）
    //    - 第一次调用：获取数量（传 NULL 作为数组指针）
    //    - 第二次调用：获取数据（传实际数组地址）
    //
    // ============================================================================
    {
        // 初始化队列索引为无效值（-1）
        vkContext->graphicsIdx = -1;

        // 第一步：枚举系统中的所有 GPU
        uint32_t gpuCount = 0;
        VkPhysicalDevice gpus[10];

        // 两阶段查询：先获取 GPU 数量
        // vkEnumeratePhysicalDevices 参数：
        //   1. instance: Vulkan 实例
        //   2. pPhysicalDeviceCount: 输入/输出参数，返回 GPU 数量
        //   3. pPhysicalDevices: NULL 表示只查询数量
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, 0));

        // 第二次调用：获取所有 GPU 句柄
        // 现在 gpuCount 包含了实际数量，可以分配数组并获取数据
        VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &gpuCount, gpus));

        // 第二步：遍历每个 GPU，查找合适的队列族
        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];
            uint32_t queueFamilyCount = 0;

            // 获取该 GPU 的队列族数量
            VkQueueFamilyProperties queueProp[10];

            // 两阶段查询：先获取队列族数量
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);

            // 第二次调用：获取所有队列族的属性
            // VkQueueFamilyProperties 包含：
            //   - queueFlags: 该队列支持的操作类型（GRAPHICS, COMPUTE, TRANSFER）
            //   - queueCount: 该族的队列数量
            //   - timestampValidBits: 时间戳精度
            //   - minImageTransferGranularity: 图像传输粒度
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProp);

            // 第三步：遍历该 GPU 的所有队列族，查找支持 surface 的队列
            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                // 检查该队列族是否支持向 surface 渲染
                VkBool32 surfaceSupport = VK_FALSE;

                // vkGetPhysicalDeviceSurfaceSupportKHR 检查特定队列族能否在指定 surface 上渲染
                // 参数：
                //   1. physicalDevice: GPU 句柄
                //   2. queueFamilyIndex: 要检查的队列族索引
                //   3. surface: 窗口表面
                //   4. pSupported: 输出参数，VK_TRUE = 支持，VK_FALSE = 不支持
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkContext->surface,
                                                              &surfaceSupport));

                // 如果找到第一个支持 surface 的队列族，选择它
                if (surfaceSupport)
                {
                    vkContext->graphicsIdx = j; // 记录队列族索引
                    vkContext->gpu = gpu;       // 选择这个 GPU
                    break;                      // 跳出内层循环
                }
            }

            // 如果已找到合适的队列族，跳出外层循环
            // 注意：这里简化了逻辑，只选择第一个找到的
            // 实际项目中可能需要更复杂的策略（如选择独显而非集显）
            if (vkContext->graphicsIdx >= 0)
            {
                break;
            }
        }

        // 检查是否成功找到支持渲染的队列族
        if (vkContext->graphicsIdx < 0)
        {
            // 没有找到支持 surface 的队列族
            // 可能原因：
            //   - GPU 驱动不支持 Vulkan
            //   - 没有支持图形输出的队列
            //   - surface 创建失败
            return false;
        }
    }

    // ============================================================================
    // 创建逻辑设备（Logical Device）
    // ============================================================================
    //
    // Vulkan 两种设备概念：
    //
    // 1. Physical Device（物理设备）
    //    - 代表 GPU 硬件
    //    - 通过 vkEnumeratePhysicalDevices 查询
    //    - 不能直接使用，只用于查询信息
    //
    // 2. Logical Device（逻辑设备）
    //    - 应用程序与 GPU 交互的接口
    //    - 通过 vkCreateDevice 创建
    //    - 指定要使用的队列族和扩展
    //    - 类似于"打开设备"，获得操作权限
    //
    // 为什么需要逻辑设备？
    // - 允许同一个 GPU 被多个应用程序使用
    // - 每个应用可以创建不同的逻辑设备配置
    // - 提供隔离和资源管理
    //
    // ============================================================================
    {
        // 队列优先级：[0.0, 1.0] 范围内的浮点数
        // 当多个队列竞争资源时，驱动根据优先级调度
        // 1.0 = 最高优先级
        float queuePriority = 1.0f;

        // 配置要创建的队列
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkContext->graphicsIdx; // 使用之前找到的支持渲染的队列族
        queueInfo.queueCount = 1;                            // 创建 1 个队列（足够大多数应用使用）
        queueInfo.pQueuePriorities = &queuePriority;         // 设置队列优先级

        // 设备级扩展：在逻辑设备上启用的扩展
        //
        // VK_KHR_swapchain: 交换链扩展
        //   - 交换链（Swapchain）是用于显示图像的机制
        //   - 管理一系列图像缓冲区（前后缓冲）
        //   - 与显示同步（避免撕裂）
        //   - 这是渲染到窗口必需的扩展
        //
        char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

        // 创建逻辑设备
        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.enabledExtensionCount = ArraySize(extensions); // 启用的扩展数量
        deviceInfo.ppEnabledExtensionNames = extensions;          // 扩展名称列表
        deviceInfo.pQueueCreateInfos = &queueInfo;                // 队列创建信息
        deviceInfo.queueCreateInfoCount = 1; // ⚠️ 关键：必须设置队列创建信息数量！

        // vkCreateDevice: 创建逻辑设备
        // 参数：
        //   1. physicalDevice: 要使用的 GPU
        //   2. pCreateInfo: 创建信息（队列、扩展、特性等）
        //   3. pAllocator: 内存分配器（NULL = 使用默认）
        //   4. pDevice: 输出参数，返回逻辑设备句柄
        VK_CHECK(vkCreateDevice(vkContext->gpu, &deviceInfo, 0, &vkContext->device));

        // 从逻辑设备获取队列句柄
        //
        // 为什么需要单独获取？
        // - 队列是在设备创建时自动创建的
        // - 但需要获取句柄才能提交命令
        // - 同一个队列族可以创建多个队列，需要指定索引
        //
        // vkGetDeviceQueue 参数：
        //   1. device: 逻辑设备
        //   2. queueFamilyIndex: 队列族索引（我们之前选择的 graphicsIdx）
        //   3. queueIndex: 队列在族内的索引（我们只创建了 1 个，所以是 0）
        //   4. pQueue: 输出参数，返回队列句柄
        vkGetDeviceQueue(vkContext->device, vkContext->graphicsIdx, 0, &vkContext->graphicsQueue);
    }

    // ============================================================================
    // 创建交换链（Swapchain）
    // ============================================================================
    //
    // 什么是交换链？
    // --------------
    // 交换链是 Vulkan 用于显示图像的机制，管理一系列图像缓冲区。
    //
    // 类比：传统动画制作
    // - 画家在多张透明纸上画连续的动作
    // - 快速翻动纸张，产生动画效果
    // - 交换链就是这些"纸张"的管理器
    //
    // 为什么需要交换链？
    // ------------------
    // 1. 避免撕裂（Screen Tearing）
    //    - 撕裂：显示器正在显示图像时，GPU 又更新了内容
    //    - 交换链确保只在显示器刷新时切换图像（垂直同步）
    //
    // 2. 提高性能
    //    - GPU 渲染到下一帧缓冲区的同时，显示器显示当前帧
    //    - 并行工作，提高帧率
    //
    // 3. 灵活的图像数量
    //    - 双缓冲（2 张图像）：标准配置
    //    - 三缓冲（3 张图像）：更高帧率，但延迟稍高
    //
    // 工作流程：
    // ---------
    // GPU 渲染到 Image A → GPU 渲染到 Image B → 显示 Image A → 显示 Image B → 循环...
    //    (后台缓冲)              (后台缓冲)            (前台缓冲)      (前台缓冲)
    //
    // ============================================================================
    {
        // ----------------------------------------------------------------------
        // 第一步：查询 surface 支持的图像格式
        // ----------------------------------------------------------------------
        //
        // 为什么需要查询？
        // - 不同显示器/窗口系统支持不同的像素格式
        // - 常见格式：
        //   * VK_FORMAT_B8G8R8_SRGB: 8位 BGR，sRGB 颜色空间（Windows 常用）
        //   * VK_FORMAT_R8G8B8_SRGB: 8位 RGB，sRGB 颜色空间
        //
        // VkSurfaceFormatKHR 包含：
        //   - format: 像素格式（颜色通道顺序和位深）
        //   - colorSpace: 颜色空间（sRGB, HDR 等）
        //
        uint32_t formatCount = 0;
        VkSurfaceFormatKHR surfaceFormats[10];

        // 两阶段查询：先获取支持的格式数量
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, 0));

        // 第二次调用：获取所有支持的格式
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkContext->gpu, vkContext->surface,
                                                      &formatCount, surfaceFormats));

        // 选择合适的格式
        //
        // 策略：
        // 1. 优先选择 VK_FORMAT_B8G8R8_SRGB（Windows 标准格式）
        // 2. 如果没找到，使用第一个可用的格式作为后备
        //
        bool foundFormat = false;

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            // VK_FORMAT_B8G8R8_SRGB: Windows 标准格式
            // B8G8R8: 蓝 8位，绿 8位，红 8位（注意顺序是 BGR）
            // SRGB: sRGB 颜色空间，适合显示（经过 gamma 校正）
            if (format.format == VK_FORMAT_B8G8R8_SRGB)
            {
                vkContext->surfaceFormat = format;
                foundFormat = true;
                break;
            }
        }

        // 如果没找到首选格式，使用第一个可用的格式
        // ⚠️ 这很重要：不能使用未初始化的 surfaceFormat！
        if (!foundFormat && formatCount > 0)
        {
            std::cout << "no suitable format." << std::endl;
            vkContext->surfaceFormat = surfaceFormats[0];
        }

        // ----------------------------------------------------------------------
        // 第二步：查询 surface 能力（capabilities）
        // ----------------------------------------------------------------------
        //
        // VkSurfaceCapabilitiesKHR 包含：
        //   - minImageCount / maxImageCount: 支持的图像数量范围
        //   - currentExtent: 当前窗口大小（理想图像尺寸）
        //   - minImageExtent / maxImageExtent: 支持的图像尺寸范围
        //   - maxImageArrayLayers: 支持的图层数（立体渲染需要 >1）
        //   - supportedTransforms: 支持的图像变换（旋转、翻转等）
        //   - currentTransform: 当前变换（通常为无变换）
        //
        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkContext->gpu, vkContext->surface,
                                                           &surfaceCaps));

        // ----------------------------------------------------------------------
        // 第三步：计算图像数量
        // ----------------------------------------------------------------------
        //
        // 为什么是 minImageCount + 1？
        // - minImageCount: 最低要求（通常为 2，双缓冲）
        // - +1: 额外一张图像，让 GPU 可以提前开始下一帧
        //   避免等待显示器完成当前帧
        //
        // 举例：
        //   minImageCount = 2（双缓冲）
        //   我们使用 3 张图像（三缓冲）
        //   GPU 可以同时渲染、等待显示、正在显示
        //
        uint32_t imgCount = surfaceCaps.minImageCount + 1;

        // 确保不超过最大值
        // maxImageCount = 0 表示没有限制（除了内存）
        // if (surfaceCaps.maxImageCount > 0 && imgCount > surfaceCaps.maxImageCount)
        // {
        //     imgCount = surfaceCaps.maxImageCount;
        // }

        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        // ----------------------------------------------------------------------
        // 第四步：创建交换链
        // ----------------------------------------------------------------------
        //
        // 配置交换链创建信息
        // 每个参数的含义和选择原因见下文详细注释
        //
        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

        // surface: 要关联的窗口表面
        // 交换链必须绑定到一个 surface，这样才能将图像显示到窗口上
        scInfo.surface = vkContext->surface;

        // minImageCount: 交换链中的图像数量
        // 我们之前计算为 minImageCount + 1（实现三缓冲）
        scInfo.minImageCount = imgCount;

        // imageFormat: 图像的像素格式
        // 使用之前查询和选择的格式（如 VK_FORMAT_B8G8R8_SRGB）
        scInfo.imageFormat = vkContext->surfaceFormat.format;

        // imageColorSpace: 颜色空间
        // 包含在 surfaceFormat 中，通常是 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        scInfo.imageColorSpace = vkContext->surfaceFormat.colorSpace;

        // imageExtent: 图像的尺寸（宽度和高度）
        // 使用 surface 的当前尺寸，通常与窗口大小一致
        scInfo.imageExtent = surfaceCaps.currentExtent;

        // imageArrayLayers: 图层数量
        // 1 = 普通 2D 渲染
        // 2+ = 立体渲染（VR、3D 电影等需要）
        scInfo.imageArrayLayers = 1;

        // imageUsage: 图像用途标志
        //
        // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:
        //   - 图像将用作颜色附件（渲染目标）
        //   - 这是最常见的用途，表示我们会渲染到这个图像
        //
        // 其他选项：
        //   - VK_IMAGE_USAGE_TRANSFER_DST_BIT: 可以作为复制目标（用于后处理）
        //   - VK_IMAGE_USAGE_STORAGE_BIT: 可以作为存储图像（计算着色器）
        //   - VK_IMAGE_USAGE_SAMPLED_BIT: 可以作为纹理采样（读取渲染结果）
        //
        // 注意：必须同时启用 TRANSFER_DST_BIT
        // 因为 vkCmdClearColorImage 本质上是"传输"操作
        // 清除 = 将颜色数据"传输"到图像，所以需要 TRANSFER_DST_BIT
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // preTransform: 图像变换
        //
        // 控制图像在呈现前的变换（旋转、翻转等）
        //
        // 常见值：
        //   - VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR: 无变换
        //   - VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR: 旋转 90 度
        //   - VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR: 水平翻转
        //
        // surfaceCaps.currentTransform:
        //   - 通常为 IDENTITY（无变换）
        //   - 移动设备可能需要旋转（如手机横屏/竖屏）
        //
        scInfo.preTransform = surfaceCaps.currentTransform;

        // compositeAlpha: Alpha 合成模式
        //
        // 控制窗口与其他窗口的透明度混合方式
        //
        // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR:
        //   - 不透明（窗口内容完全覆盖背后内容）
        //   - 最常见的选择
        //
        // 其他选项：
        //   - VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR: 预乘 Alpha
        //   - VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR: 后乘 Alpha
        //   - VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR: 使用窗口系统的默认值
        //
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // presentMode: 呈现模式
        //
        // 控制图像如何呈现到屏幕
        //
        // VK_PRESENT_MODE_IMMEDIATE_KHR:
        //   - 立即呈现（不等待垂直同步）
        //   - 可能撕裂，但延迟最低
        //
        // VK_PRESENT_MODE_FIFO_KHR:
        //   - 先进先出队列（垂直同步）
        //   - 保证不撕裂，但延迟可能较高
        //   - 所有平台都支持
        //
        // VK_PRESENT_MODE_MAILBOX_KHR:
        //   - 信箱模式（三缓冲的变体）
        //   - 当队列满时替换旧图像，而不是等待
        //   - 低延迟 + 不撕裂，推荐选择
        //
        // 注意：我们这里使用默认值（FIFO），因为它是唯一保证支持的
        // scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;  // 默认值

        // clipped: 是否裁剪
        //
        // VK_TRUE:
        //   - 如果窗口被其他窗口遮挡，不渲染被遮挡的部分
        //   - 性能更好（不需要渲染看不见的内容）
        //
        // VK_FALSE:
        //   - 即使被遮挡也渲染全部内容
        //   - 用于需要截图或特殊效果的场景
        //
        scInfo.clipped = VK_TRUE; // 启用裁剪以优化性能

        // oldSwapchain: 旧的交换链
        //
        // 用于重建交换链时（如窗口大小改变）
        // 可以传入旧交换链，驱动会保留其资源
        //
        // 我们的场景：首次创建，传 VK_NULL_HANDLE
        scInfo.oldSwapchain = VK_NULL_HANDLE;

        // 创建交换链
        //
        // vkCreateSwapchainKHR 参数：
        //   1. device: 逻辑设备
        //   2. pCreateInfo: 创建信息（上面配置的所有参数）
        //   3. pAllocator: 内存分配器（NULL = 使用默认分配器）
        //   4. pSwapchain: 输出参数，返回交换链句柄
        //
        VK_CHECK(vkCreateSwapchainKHR(vkContext->device, &scInfo, 0, &vkContext->swapChain));
        // ----------------------------------------------------------------------
        // 第五步：获取交换链图像句柄
        // ----------------------------------------------------------------------
        //
        // 为什么需要再次获取？
        // - 创建交换链时，Vulkan 自动创建了图像
        // - 但我们需要获取这些图像的句柄才能渲染到它们
        //
        // VkImage: 代表 GPU 内存中的图像资源
        // 类比：画布的句柄，我们需要知道画布在哪里才能画画
        //
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkContext->device, vkContext->swapChain,
                                         &vkContext->scImgCount, vkContext->scImages));

        // 现在 vkContext->scImages[] 数组包含了所有交换链图像的句柄
        // 后续渲染时会使用这些图像作为渲染目标
    }

    // ============================================================================
    // 创建图像视图（ImageView）
    // ============================================================================
    //
    // 什么是图像视图（ImageView）？
    // ----------------------------------
    // 图像视图是对图像的"视图"或"包装器"，定义了如何查看/访问图像。
    //
    // 类比理解：
    // - Image  = 实际的图像数据（原始像素数据，存在 GPU 内存中）
    // - ImageView = 看待图像的方式（格式、范围、类型等）
    //
    // 为什么需要 ImageView？
    // ----------------------------------
    // 1. 格式重映射：将 BGR 图像视为 RGB（颜色通道重排）
    // 2. 范围限制：只使用图像的一部分（mipmap 的某一层）
    // 3. 类型转换：将深度纹理视为普通纹理
    // 4. Framebuffer 要求：Framebuffer 绑定的是 ImageView 而不是 Image
    //
    // 示例：ImageView 的用途
    // ----------------------------------
    // 原始图像：B8G8R8A8 格式（蓝-绿-红-Alpha）
    // ImageView 可以告诉 Vulkan："把蓝当红，把红当蓝"（组件重映射）
    //
    {
        // ----------------------------------------------------------------------
        // 配置 ImageView 创建信息（循环外设置，所有图像共享相同配置）
        // ----------------------------------------------------------------------
        //
        // 为什么在循环外设置 viewInfo？
        // - 所有交换链图像的 ImageView 配置完全相同
        // - 只有 image 字段不同（指向不同的图像）
        // - 这样避免重复代码，提高效率
        //
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType =
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO; // ← 修复：原为 EVENT_CREATE_INFO（错误）
        // ↑ sType 必须匹配结构体类型，否则 Vulkan 会验证失败

        // viewType: 视图类型
        //
        // VK_IMAGE_VIEW_TYPE_2D:        普通 2D 纹理（当前使用）
        // VK_IMAGE_VIEW_TYPE_3D:        3D 纹理
        // VK_IMAGE_VIEW_TYPE_CUBE:      立方体贴图（6 个面）
        // VK_IMAGE_VIEW_TYPE_2D_ARRAY:  2D 纹理数组
        //
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        // format: 视图的格式（必须与图像创建时的格式兼容）
        //
        // 注意：格式必须匹配或兼容，否则 vkCreateImageView 会失败
        //       例如：不能将 B8G8R8_SRGB 图像创建为 R8G8B8A8_SNORM 视图
        //
        viewInfo.format = vkContext->surfaceFormat.format;

        // components: 颜色通道重映射（可选，这里使用默认值）
        //
        // 允许重新排列颜色通道，例如：
        // - 将 BGR 图像映射为 RGB（Windows DIB 图像常用）
        // - 提取单个通道（将 RGB 图像视为灰度图）
        //
        // 默认值为 IDENTITY（不重映射），所以这里不需要显式设置
        // viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;  // 默认
        // viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        // viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        // viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // subresourceRange: 要访问的图像子资源范围
        //
        // aspectMask: 要访问的图像"方面"
        // VK_IMAGE_ASPECT_COLOR_BIT:    颜色数据（当前使用）
        // VK_IMAGE_ASPECT_DEPTH_BIT:    深度数据（深度缓冲）
        // VK_IMAGE_ASPECT_STENCIL_BIT:  模板数据（模板缓冲）
        // VK_IMAGE_ASPECT_METADATA_BIT: 元数据（罕见）
        //
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        // levelCount: mipmap 层级数量
        //
        // 1 = 只使用 1 层（无 mipmap）
        // VK_REMAINING_MIP_LEVELS = 从 baseMipLevel 到最高层
        //
        viewInfo.subresourceRange.levelCount = 1;

        // layerCount: 数组层数量
        //
        // 1 = 只使用 1 层（当前使用）
        // VK_REMAINING_ARRAY_LAYERS = 从 baseArrayLayer 到最后一层
        // 对于立方体贴图：应该是 6
        //
        viewInfo.subresourceRange.layerCount = 1;

        // ----------------------------------------------------------------------
        // 为每个交换链图像创建 ImageView
        // ----------------------------------------------------------------------
        for (uint32_t i = 0; i < vkContext->scImgCount; i++)
        {
            // image: 要创建视图的图像
            // 这是唯一需要为每个 ImageView 单独设置的字段
            viewInfo.image = vkContext->scImages[i];

            // 创建 ImageView
            // 参数：
            // - device: 逻辑设备
            // - pCreateInfo: 创建信息（指向 viewInfo）
            // - pAllocator: 内存分配器（NULL = 使用默认分配器）
            // - pImageView: 输出的 ImageView 句柄
            VK_CHECK(vkCreateImageView(vkContext->device, &viewInfo, 0, &vkContext->scImgViews[i]));
        }
    }

    // ============================================================================
    // 创建渲染通道（Render Pass）
    // ============================================================================
    //
    // 什么是渲染通道（Render Pass）？
    // ----------------------------------
    // 渲染通道是 Vulkan 中描述"如何渲染"的蓝图，它定义了：
    //   1. 使用哪些附件（Attachments）- 颜色、深度、模板等
    //   2. 附件的初始/最终状态（加载/存储操作）
    //   3. 渲染流程的各个子阶段（Subpasses）
    //
    // 类比理解：
    // - Render Pass = 作画计划书
    // - Subpass = 具体步骤（步骤1：画背景，步骤2：画人物...）
    // - Attachment = 画布（颜色、深度、模板等）
    //
    // 为什么需要渲染通道？
    // ----------------------------------
    // 1. 性能优化：驱动知道整个渲染流程，可以优化硬件操作
    // 2. 同步控制：明确指定每个阶段的依赖关系
    // 3. 内存管理：自动处理附件之间的数据传输
    // 4. 清除操作：定义如何初始化附件（清除、保留等）
    //
    // 渲染通道的完整生命周期：
    // ----------------------------------
    // 创建 → 开始录制命令 → vkCmdBeginRenderPass → [渲染命令] → vkCmdEndRenderPass → 提交
    //
    {
        // ----------------------------------------------------------------------
        // 第一步：配置附件（Attachment）
        // ----------------------------------------------------------------------
        //
        // 什么是附件？
        //   附件是渲染过程中使用的图像资源，通常包括：
        //   - 颜色附件（Color Attachment）：存储渲染结果（像素颜色）
        //   - 深度附件（Depth Attachment）：深度缓冲（深度测试）
        //   - 模板附件（Stencil Attachment）：模板缓冲（模板测试）
        //
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = vkContext->surfaceFormat.format; // 图像格式（如 B8G8R8_SRGB）

        // loadOp: 附件开始渲染时的操作
        //
        // VK_ATTACHMENT_LOAD_OP_CLEAR:     清除附件（填充特定值）
        // VK_ATTACHMENT_LOAD_OP_LOAD:       保留之前的内容
        // VK_ATTACHMENT_LOAD_OP_DONT_CARE: 不关心（性能优化）
        //
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 清除为特定值

        // storeOp: 渲染完成后的操作
        //
        // VK_ATTACHMENT_STORE_OP_STORE:      存储结果（保存到内存）
        // VK_ATTACHMENT_STORE_OP_DONT_CARE:  不保存（性能优化，只用于临时附件）
        //
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 保存结果

        // samples: 采样数量（多重采样抗锯齿）
        //
        // VK_SAMPLE_COUNT_1_BIT: 无多重采样（性能最好）
        // VK_SAMPLE_COUNT_4_BIT: 4x 多重采样（抗锯齿，性能开销）
        //
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        // initialLayout: 渲染开始前图像的布局
        //
        // VK_IMAGE_LAYOUT_UNDEFINED: 附件的初始状态是未定义的
        // 这意味着开始渲染时，我们需要清除它（由 loadOp = CLEAR 决定）
        //
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        // finalLayout: 渲染结束后图像的布局
        //
        // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: 呈现源布局
        // 渲染完成后，图像将被显示到屏幕，所以布局应该设置为呈现源
        //
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // 注意：initialLayout 和 finalLayout 可以不同
        // 这允许驱动在渲染过程中自动转换布局以优化性能

        // 将附件信息放入数组
        VkAttachmentDescription attachments[] = {colorAttachment};

        // ----------------------------------------------------------------------
        // 第二步：配置附件引用（Attachment Reference）
        // ----------------------------------------------------------------------
        //
        // 附件引用告诉子阶段如何使用附件
        //
        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // 附件在数组中的索引
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // ↑ 附件在使用时的布局
        // COLOR_ATTACHMENT_OPTIMAL: 颜色附件的最优布局
        // 适用于作为渲染目标的图像（写入颜色数据）
        //
        // 注意：VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL 是新版本 Vulkan 的值
        //       需要启用 VK_KHR_synchronization2 扩展
        //       这里使用传统的 COLOR_ATTACHMENT_OPTIMAL，无需额外扩展

        // ----------------------------------------------------------------------
        // 第三步：配置子阶段（Subpass）
        // ----------------------------------------------------------------------
        //
        // 什么是子阶段？
        //   子阶段是渲染流程中的一个步骤，一个渲染通道可以有多个子阶段
        //
        // 典型多子阶段示例：
        //   - Subpass 0: 几何通道（处理三角形、光照）
        //   - Subpass 1: 后期处理（模糊、色调映射）
        //   - Subpass 2: UI 覆盖层
        //
        // 当前项目：只有一个子阶段（直接清除为黄色）
        //
        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;                // 颜色附件数量
        subpassDesc.pColorAttachments = &colorAttachmentRef; // 颜色附件引用数组

        // ----------------------------------------------------------------------
        // 第四步：创建渲染通道
        // ----------------------------------------------------------------------
        //
        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;               // 附件数组
        rpInfo.attachmentCount = ArraySize(attachments); // 附件数量
        rpInfo.pSubpasses = &subpassDesc;                // 子阶段数组
        rpInfo.subpassCount = 1;                         // 子阶段数量

        // vkCreateRenderPass 参数：
        //   1. device: 逻辑设备
        //   2. pCreateInfo: 创建信息（上面配置的所有参数）
        //   3. pAllocator: 内存分配器（NULL = 使用默认分配器）
        //   4. pRenderPass: 输出参数，返回渲染通道句柄
        //
        VK_CHECK(vkCreateRenderPass(vkContext->device, &rpInfo, 0, &vkContext->renderPass));
    }

    // ============================================================================
    // 创建帧缓冲（Framebuffer）
    // ============================================================================
    //
    // 什么是帧缓冲（Framebuffer）？
    // ----------------------------------
    // 帧缓冲将 Render Pass 中定义的附件与实际的图像视图绑定起来。
    //
    // 类比理解：
    // - Render Pass  = 作画计划书（定义要用到哪些画布）
    // - Framebuffer  = 准备好的实际画布（指向真实的图像）
    // - ImageView    = 画布的视图（如何查看/访问图像）
    //
    // 为什么需要 Framebuffer？
    // ----------------------------------
    // Render Pass 只是描述"需要什么类型的附件"，而 Framebuffer 提供"具体的图像"。
    // 一个 Render Pass 可以配合多个 Framebuffer 使用（例如：不同的交换链图像）。
    //
    // 关键概念：
    // ----------------------------------
    // 1. 附件绑定：Framebuffer 将 ImageView 绑定到 Render Pass 的附件槽位
    // 2. 每个交换链图像一个 Framebuffer：因为每个图像都需要独立的 Framebuffer
    // 3. 尺寸匹配：Framebuffer 尺寸必须与附件图像的尺寸一致
    //
    // Framebuffer 与 Render Pass 的关系：
    // ----------------------------------
    // RenderPass 定义： "我需要一个颜色附件"
    // Framebuffer 提供： "这是实际的图像视图（scImgViews[0]）"
    //
    // 渲染时的完整流程：
    // ----------------------------------
    // vkCmdBeginRenderPass 需要指定 Framebuffer
    //   ↓
    // Vulkan 知道要渲染到哪些具体的图像
    //   ↓
    // 渲染结果写入 Framebuffer 绑定的图像
    //
    {
        // ----------------------------------------------------------------------
        // 配置 Framebuffer 创建信息
        // ----------------------------------------------------------------------
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

        // renderPass: 此 Framebuffer 兼容的 Render Pass
        // Framebuffer 必须与 Render Pass 的附件定义兼容：
        // - 附件数量匹配
        // - 格式匹配
        // - 采样数匹配
        fbInfo.renderPass = vkContext->renderPass;

        // width/height: Framebuffer 的尺寸（必须与附件图像的尺寸一致）
        fbInfo.width = vkContext->screenSize.width;
        fbInfo.height = vkContext->screenSize.height;

        // layers: 图像层数（通常为 1，立体渲染需要更多）
        fbInfo.layers = 1;

        // attachmentCount: 附件数量（必须与 Render Pass 中定义的附件数量一致）
        fbInfo.attachmentCount = 1;

        // ----------------------------------------------------------------------
        // 为每个交换链图像创建一个 Framebuffer
        // ----------------------------------------------------------------------
        // 为什么需要多个 Framebuffer？
        // - 交换链有多个图像（双缓冲 = 2，三缓冲 = 3）
        // - 每个图像需要独立的 Framebuffer
        // - 渲染时根据当前获取的图像索引使用对应的 Framebuffer
        //
        // 示例流程：
        // Frame 0: 获取 imageIdx=0 → 使用 framebuffers[0] → 渲染到 scImages[0]
        // Frame 1: 获取 imageIdx=1 → 使用 framebuffers[1] → 渲染到 scImages[1]
        // Frame 2: 获取 imageIdx=0 → 使用 framebuffers[0] → 渲染到 scImages[0]
        //   └─ GPU 已完成上一帧，image 0 可以重新使用
        //
        for (uint32_t i = 0; i < vkContext->scImgCount; i++)
        {
            // pAttachments: 指向实际的图像视图数组
            // 这里我们将交换链图像的 ImageView 绑定到 Framebuffer
            // Render Pass 中的第一个附件将使用这个 ImageView
            fbInfo.pAttachments = &vkContext->scImgViews[i];

            // 创建 Framebuffer
            // 参数：
            // - device: 逻辑设备
            // - pCreateInfo: 创建信息
            // - pAllocator: 内存分配器（NULL = 使用默认分配器）
            // - pFramebuffer: 输出的 Framebuffer 句柄
            VK_CHECK(
                vkCreateFramebuffer(vkContext->device, &fbInfo, 0, &vkContext->framebuffers[i]));
        }
    }

    // ============================================================================
    // 创建命令池（Command Pool）
    // ============================================================================
    //
    // 什么是命令池？
    // ------------
    // 命令池是用于分配命令缓冲区的内存管理器。
    //
    // 类比理解：
    // - Command Pool = 纸张仓库（管理一堆"画纸"）
    // - Command Buffer = 画纸（在上面记录绘制命令）
    // - Queue = 画家（执行命令缓冲区上的命令）
    //
    // 为什么需要命令池？
    // ------------------
    // 1. 内存管理效率
    //    - 命令缓冲区从池中分配，避免频繁的内存分配/释放
    //    - 批量管理命令缓冲区的生命周期
    //
    // 2. 队列族绑定
    //    - 每个命令池必须绑定到特定的队列族
    //    - 从该池分配的命令缓冲区只能提交到对应的队列
    //
    // 3. 性能优化
    //    - 驱动可以针对特定队列族优化命令缓冲区分配
    //    - 重置和回收更高效
    //
    // 工作流程：
    // ---------
    // 1. 创建命令池（绑定到队列族）
    // 2. 从池中分配命令缓冲区
    // 3. 记录命令到缓冲区
    // 4. 提交缓冲区到队列
    // 5. 执行完成后回收缓冲区（可重用）
    //
    // ============================================================================
    {
        // 配置命令池创建信息
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        // queueFamilyIndex: 命令池绑定的队列族索引
        //
        // 为什么要指定队列族？
        // - 命令缓冲区必须提交到队列才能执行
        // - 不同的队列族（Graphics、Compute、Transfer）可能有不兼容的命令类型
        // - 绑定到队列族后，驱动可以优化命令缓冲区结构以匹配该队列
        //
        // 举例：
        // - graphicsIdx = 0（图形队列族）
        // - 从此池分配的命令缓冲区可以包含绘制命令
        // - 这些缓冲区只能提交到图形队列，不能提交到计算队列
        //
        poolInfo.queueFamilyIndex = vkContext->graphicsIdx;

        // flags: 命令池的创建标志（可选）
        //
        // 常用选项：
        // - VK_COMMAND_POOL_CREATE_TRANSIENT_BIT:
        //   命令缓冲区生命周期短，频繁重录（适合每帧重新录制）
        //
        // - VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT:
        //   允许单独重置命令缓冲区（而不是整个池）
        //   不设置此标志时，只能重置整个池
        //
        // 本例：不设置 flags，使用默认行为
        // - 命令缓冲区不在池销毁前自动重置
        // - 可以通过 vkResetCommandPool 或 vkFreeCommandBuffers 管理
        //
        // poolInfo.flags = 0;  // 默认行为

        // 创建命令池
        //
        // vkCreateCommandPool 参数：
        //   1. device: 逻辑设备
        //   2. pCreateInfo: 创建信息（上面配置的队列族索引等）
        //   3. pAllocator: 内存分配器（NULL = 使用默认分配器）
        //   4. pCommandPool: 输出参数，返回命令池句柄
        //
        VK_CHECK(vkCreateCommandPool(vkContext->device, &poolInfo, 0, &vkContext->commandPool));
    }

    // ============================================================================
    // 创建同步对象（Synchronization Primitives）
    // ============================================================================
    //
    // 什么是信号量（Semaphore）？
    // ----------------------
    // 信号量是用于 GPU 内部同步的原语，用于协调不同操作之间的执行顺序。
    // 获取信号量 (AccquireSemaphore) 触发 → GPU 开始渲染 → 渲染结束并触发提交信号量
    // (SubmitSemaphore) → 显示硬件观察到 B，读取显存并显示。 类比理解：
    // - 信号量 = 传递接力棒的机制
    // - "等待信号量" = 等待接力棒到达才能继续
    // - "发送信号" = 传递接力棒给下一阶段
    //
    // 为什么需要信号量？
    // ------------------
    // Vulkan 是异步架构，GPU 操作需要时间完成：
    //
    // 1. 获取交换链图像 (Acquire Next Image)
    //    - 动作：向显示引擎申请一张图，并指定信号量 A。
    //    - 逻辑：此操作在 CPU 上立即返回，但图像此时可能还在显示器上。
    //    - 触发：当显示器刷完旧帧，显示引擎会自动点亮“信号量 A”。

    // 2. 提交渲染任务 (Queue Submit)
    //    - 等待：设置 pWaitSemaphores = A。
    //    - 逻辑：告诉 GPU：“你可以先排队，但【必须等 A 亮了】才能往显存里写像素。”
    //    - 动作：执行渲染命令（如 vkCmdClearColorImage）。
    //    - 触发：GPU 全部画完后，硬件会自动点亮“信号量 B”。

    // 3. 呈现图像 (Queue Present)
    //    - 等待：设置 pWaitSemaphores = B。
    //    - 逻辑：告诉显示引擎：“等 B 亮了（渲染完了），你再去读这块显存并贴到屏幕上。”
    //    - 后果：显示引擎读取完成后，会将图像标记为“Available”（空闲），从而允许【下一帧】的 Step 1
    //    成功。
    //
    // ------------------------------
    //
    // acquireSemaphore（获取信号量）：CPU发起请求, 当图像准备好被写时，GPU主动触发该信号量
    //   - 作用：等待交换链图像可用
    //   - 流程：vkAcquireNextImageKHR(acquireSemaphore) → 图像准备好 → (GPU)信号量触发
    //   - 确保：不会获取到还在显示的图像
    //   -
    //   物理意义：显存里的那块内存区域正在被显示器的扫描电路读取，为了防止你边画边读导致画面花掉，Vulkan
    //   强制锁定它。
    //   - 同步意义：这就是为什么你需要
    //   acquireSemaphore。它本质上是硬件在告诉你：“嘿，显示器终于用完这张图了，现在你可以安全地往里面写新的像素了。”

    // submitSemaphore（提交信号量）：CPU提交任务给GPU
    //   - 作用：等待渲染完成
    //   - 流程：渲染完成 → 信号量触发 → vkQueueSubmit(submitSemaphore) → 显示
    //   - 确保：渲染完成后才显示图像
    //
    // 双信号量模式（标准渲染循环）
    // 执行流程示例：
    // ------------------
    //
    // 第 N 帧：
    //   1. vkAcquireNextImageKHR(waitFor=acquireSemaphore_N)
    //      ↓ 等待上一帧的 acquireSemaphore_N 触发
    //   2. vkQueueSubmit(waitFor=acquireSemaphore_N, signal=submitSemaphore_N)
    //      ↓ 等待获取图像，完成后触发 submitSemaphore_N
    //   3. vkQueuePresentKHR(waitFor=submitSemaphore_N)
    //      ↓ 等待渲染完成，完成后触发 acquireSemaphore_N+1
    //
    // 第 N+1 帧：
    //   回到步骤 1，使用 acquireSemaphore_N+1
    //
    // 双缓冲 vs 三缓冲
    // ------------------
    //
    // 双缓冲（2 个信号量对）：
    //   - acquireSemaphore[0], submitSemaphore[0] → 帧 N
    //   - acquireSemaphore[1], submitSemaphore[1] → 帧 N+1
    //   - 最小配置，帧率受显示刷新率限制
    //
    //
    // ============================================================================
    {
        // 配置信号量创建信息
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO; // ⚠️ 注意：不是 SEMAPHORE_TYPE

        // 创建获取信号量
        //
        // 用途：同步图像获取操作
        //
        // vkCreateSemaphore 参数：
        //   1. device: 逻辑设备
        //   2. pCreateInfo: 创建信息（上面配置的 sType 和 flags）
        //   3. pAllocator: 内存分配器（NULL = 使用默认分配器）
        //   4. pSemaphore: 输出参数，返回信号量句柄
        //
        VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0, &vkContext->acquireSemaphore));

        // 创建提交信号量
        //
        // 用途：同步渲染完成操作
        // 注意：每个交换链图像需要独立的信号量，避免信号量复用冲突
        // 参见：https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
        for (uint32_t i = 0; i < 5; i++)
        {
            VK_CHECK(vkCreateSemaphore(vkContext->device, &semaInfo, 0,
                                       &vkContext->submitSemaphores[i]));
        }

        // 创建围栏（Fence）
        //
        // 作用：CPU-GPU 同步，让 CPU 等待 GPU 完成上一帧
        // vkAcquireNextImageKHR 要求信号量必须是"未触发"状态
        // 如果上一帧的 GPU 操作还没完成，信号量仍然处于"已触发"状态
        // 必须用围栏等待上一帧完全结束后才能开始下一帧
        //
        // VK_FENCE_CREATE_SIGNALED_BIT: 创建时已处于"触发"状态
        // 为什么？第一帧渲染前不需要等待，避免死锁
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(vkContext->device, &fenceInfo, 0, &vkContext->inFlightFence));
    }

    // ============================================================================
    // 创建图形管线（Graphics Pipeline）
    // ============================================================================
    //
    // 什么是图形管线？
    // ----------------------------------
    // 图形管线是 Vulkan 中最复杂的对象之一，它定义了完整的渲染流程。
    //
    // 类比理解：
    // - Graphics Pipeline = 工厂的完整生产线配置
    // - 每个状态 = 生产线上的一个工位设置
    // - 着色器 = 工人的操作手册
    //
    // 为什么需要这么多状态？
    // ----------------------------------
    // Vulkan 的设计理念是"显式且高效"：
    // - 所有状态必须在创建管线时指定
    // - 驱动可以提前优化（编译着色器、设置硬件状态）
    // - 运行时切换管线非常快（只需一次硬件状态切换）
    //
    // 管线状态分类：
    // ----------------------------------
    // 1. 着色器阶段（Shader Stages）：顶点、片段、几何、计算等
    // 2. 顶点输入状态（Vertex Input）：顶点数据格式
    // 3. 输入装配状态（Input Assembly）：图元类型（三角形、线等）
    // 4. 视口状态（Viewport）：屏幕映射
    // 5. 光栅化状态（Rasterization）：剔除模式、多边形模式
    // 6. 多重采样状态（Multisample）：抗锯齿
    // 7. 颜色混合状态（Color Blend）：透明度、颜色混合
    // 8. 管线布局（Pipeline Layout）：描述符、推送常量
    // 9. 动态状态（Dynamic State）：运行时修改的状态
    //
    // 创建流程：
    // ----------------------------------
    // 1. 创建 Pipeline Layout（描述符布局）
    // 2. 配置各个状态结构
    // 3. 加载着色器
    // 4. 创建 Graphics Pipeline
    //

    // ============================================================================
    // 步骤 1：创建管线布局（Pipeline Layout）
    // ============================================================================
    //
    // 什么是 Pipeline Layout？
    // ----------------------------------
    // Pipeline Layout 定义了管线如何访问资源（Uniform、纹理、推送常量等）。
    //
    // 作用：
    // - 声明着色器使用的 Descriptor Set Layout
    // - 声明推送常量（Push Constants）的范围
    // - 提供资源绑定的"接口契约"
    //
    // 当前项目：
    // - 不使用 Uniform 缓冲区
    // - 不使用纹理
    // - 不使用推送常量
    // - 所以 Pipeline Layout 是空的（只需基础配置）
    //
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        //.setLayoutCount = 0;  // 默认：无 Descriptor Set Layout
        // .pSetLayouts = NULL;
        // .pushConstantRangeCount = 0;  // 默认：无推送常量
        // .pPushConstantRanges = NULL;

        VK_CHECK(vkCreatePipelineLayout(vkContext->device, &layoutInfo, 0, &vkContext->pipeLayout));
    }

    // ============================================================================
    // 步骤 2：配置管线状态
    // ============================================================================
    {
        // ========================================================================
        // 2.1 顶点输入状态（Vertex Input State）
        // ========================================================================
        //
        // 定义顶点数据的格式和布局
        //
        // 当前项目：
        // - 顶点数据硬编码在着色器中
        // - 不从顶点缓冲区读取
        // - 所以顶点输入状态是空的
        //
        VkPipelineVertexInputStateCreateInfo vertextInputState = {};
        vertextInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        //.vertexBindingDescriptionCount = 0;  // 无顶点绑定
        //.pVertexBindingDescriptions = NULL;
        //.vertexAttributeDescriptionCount = 0;  // 无顶点属性
        //.pVertexAttributeDescriptions = NULL;

        // ========================================================================
        // 2.2 输入装配状态（Input Assembly State）
        // ========================================================================
        //
        // 定义如何从顶点组装图元（三角形、线、点等）
        //
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

        // topology: 图元拓扑（如何从顶点组装图元）
        //
        // 常用拓扑类型：
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:     每 3 个顶点组成一个三角形
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:    三角形带（共享边）
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:      三角形扇
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST:         每 2 个顶点组成一条线
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST:        每个顶点是一个点
        //
        // 示例：
        // TRIANGLE_LIST:     [v0,v1,v2] [v3,v4,v5] [v6,v7,v8] → 3 个三角形
        // TRIANGLE_STRIP:    [v0,v1,v2,v3,v4,v5] → 4 个三角形（共享边）
        //
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        //.primitiveRestartEnable = VK_FALSE;  // 是否启用图元重启（用于 strip/fan）

        // ========================================================================
        // 2.3 视口状态（Viewport State）
        // ========================================================================
        //
        // 定义如何将 NDC 坐标映射到屏幕坐标
        //
        // 注意：这里设置为占位符（0），实际值在运行时通过 vkCmdSetViewport 设置
        //
        VkViewport viewport = {};
        viewport.x = 0.0f;        // 视口 X 坐标（占位符）
        viewport.y = 0.0f;        // 视口 Y 坐标（占位符）
        viewport.width = 0;       // 视口宽度（占位符，运行时设置）
        viewport.height = 0;      // 视口高度（占位符，运行时设置）
        viewport.minDepth = 0.0f; // 深度最小值
        viewport.maxDepth = 1.0f; // 深度最大值

        VkRect2D scissor = {};
        scissor.offset = {0, 0}; // 裁剪矩形起始位置（占位符）
        scissor.extent = {0, 0}; // 裁剪矩形大小（占位符，运行时设置）

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;      // 视口数量
        viewportState.pViewports = &viewport; // 视口数组（占位符）
        viewportState.scissorCount = 1;       // 裁剪矩形数量
        viewportState.pScissors = &scissor;   // 裁剪矩形数组（占位符）

        // ========================================================================
        // 2.4 光栅化状态（Rasterization State）
        // ========================================================================
        //
        // 控制几何形状如何转换为片段（像素）
        //
        VkPipelineRasterizationStateCreateInfo rasterizationState = {};
        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

        // depthClampEnable: 是否启用深度夹取
        //
        // VK_TRUE:  将深度值夹取到 [0, 1] 范围（而不是裁剪）
        // VK_FALSE: 正常裁剪（当前使用）
        //
        rasterizationState.depthClampEnable = VK_FALSE;

        // rasterizerDiscardEnable: 是否丢弃光栅化输出
        //
        // VK_TRUE:  不输出任何片段（用于阴影贴图等特殊用途）
        // VK_FALSE: 正常光栅化（当前使用）
        //
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;

        // polygonMode: 多边形渲染模式
        //
        // VK_POLYGON_MODE_FILL:    填充多边形（正常渲染）
        // VK_POLYGON_MODE_LINE:    线框模式（只绘制边）
        // VK_POLYGON_MODE_POINT:   点模式（只绘制顶点）
        //
        rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;

        // cullMode: 剔除模式（哪些面被剔除）
        //
        // VK_CULL_MODE_NONE:       不剔除任何面
        // VK_CULL_MODE_FRONT_BIT:  剔除正面
        // VK_CULL_MODE_BACK_BIT:   剔除背面（当前使用）
        // VK_CULL_MODE_FRONT_AND_BACK: 剔除所有面
        //
        // 为什么要剔除？
        // - 性能优化：避免渲染看不见的面
        // - 避免透明度问题：背面可能干扰正面
        //
        rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;

        // frontFace: 哪个面是"正面"
        //
        // VK_FRONT_FACE_COUNTER_CLOCKWISE: 逆时针为正面
        // VK_FRONT_FACE_CLOCKWISE:          顺时针为正面（当前使用）
        //
        // 注意：Vulkan 的 Y 轴向下，所以顶点顺序与 OpenGL 相反
        //
        rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;

        // lineWidth: 线宽（用于 LINE 模式）
        //
        // 范围：[1.0, maxWidth]（maxWidth 取决于硬件，通常是 1）
        // 宽线需要启用 GPU 特性：wideLines
        //
        rasterizationState.lineWidth = 1.0f;

        //.depthBiasEnable = VK_FALSE;  // 是否启用深度偏移（用于阴影贴图）

        // ========================================================================
        // 2.5 多重采样状态（Multisample State）
        // ========================================================================
        //
        // 控制抗锯齿（多重采样）
        //
        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

        // rasterizationSamples: 每个像素的采样数
        //
        // VK_SAMPLE_COUNT_1_BIT:  无多重采样（当前使用）
        // VK_SAMPLE_COUNT_4_BIT:  4x 多重采样
        // VK_SAMPLE_COUNT_8_BIT:  8x 多重采样
        // VK_SAMPLE_COUNT_16_BIT: 16x 多重采样
        //
        // 多重采样抗锯齿（MSAA）：
        // - 采样越多，边缘越平滑
        // - 但性能开销越大（内存、带宽）
        // - 需要配合 RenderPass 的 samples 字段
        //
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        //.sampleShadingEnable = VK_FALSE;  // 是否启用采样着色（高级特性）
        //.minSampleShading = 0.0f;          // 最小采样比率

        // ========================================================================
        // 2.6 颜色混合状态（Color Blend State）
        // ========================================================================
        //
        // 控制新片段与现有像素的颜色如何混合
        //
        // 分为两个层级：
        // 1. 单个附件的混合配置（VkPipelineColorBlendAttachmentState）
        // 2. 全局混合配置（VkPipelineColorBlendStateCreateInfo）
        //

        // ----------------------------------------------------------------------
        // 单个附件的混合配置
        // ----------------------------------------------------------------------
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};

        // blendEnable: 是否启用混合
        //
        // VK_TRUE:  启用混合（用于透明度、特效）
        // VK_FALSE: 禁用混合（直接覆盖，当前使用）
        //
        colorBlendAttachment.blendEnable = VK_FALSE;

        // colorWriteMask: 颜色写入掩码（哪些通道被写入）
        //
        // 可以禁用某些通道的写入（例如：只写入红色通道）
        //
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | // 红色通道
                                              VK_COLOR_COMPONENT_G_BIT | // 绿色通道
                                              VK_COLOR_COMPONENT_B_BIT | // 蓝色通道
                                              VK_COLOR_COMPONENT_A_BIT;  // Alpha 通道

        // 当 blendEnable = VK_TRUE 时，还需要配置：
        // .srcColorBlendFactor: 源颜色混合因子
        // .dstColorBlendFactor: 目标颜色混合因子
        // .colorBlendOp:        颜色混合运算
        // .srcAlphaBlendFactor: 源 Alpha 混合因子
        // .dstAlphaBlendFactor: 目标 Alpha 混合因子
        // .alphaBlendOp:        Alpha 混合运算

        // ----------------------------------------------------------------------
        // 全局混合配置
        // ----------------------------------------------------------------------
        VkPipelineColorBlendStateCreateInfo colorBlendState = {};
        colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

        // logicOpEnable: 是否启用逻辑运算
        //
        // VK_TRUE:  使用逻辑运算（AND, OR, XOR 等）
        // VK_FALSE: 使用混合（当前使用）
        // 注意：logicOp 和 blend 不能同时启用
        //
        colorBlendState.logicOpEnable = VK_FALSE;
        //.logicOp = VK_LOGIC_OP_COPY;  // 逻辑运算类型

        // blendConstants: 常量混合因子（当使用 CONSTANT_ALPHA/COLOR 时使用）
        // colorBlendState.blendConstants[0] = 0.0f;  // R
        // colorBlendState.blendConstants[1] = 0.0f;  // G
        // colorBlendState.blendConstants[2] = 0.0f;  // B
        // colorBlendState.blendConstants[3] = 0.0f;  // A

        // 附件混合配置数组
        colorBlendState.attachmentCount = 1;                  // 附件数量
        colorBlendState.pAttachments = &colorBlendAttachment; // 附件配置数组

        // ============================================================================
        // 创建着色器模块（Shader Module）
        // ============================================================================
        //
        // 什么是要色器模块？
        // ----------------------------------
        // Shader Module 是 Vulkan 中对着色器代码（SPIR-V 字节码）的包装器。
        //
        // SPIR-V 是什么？
        // - Vulkan 使用的着色器中间表示
        // - 由 GLSL/HLSL 等高级着色器语言编译而来
        // - 跨平台、二进制格式
        //
        // 编译流程：
        // GLSL 源码 (.vert/.frag) → glslc/glslangValidator → SPIR-V (.spv) → Vulkan
        //
        // Vulkan 版本与 SPIR-V 版本的对应关系：
        // ----------------------------------
        // Vulkan 1.0 → SPIR-V 1.0
        // Vulkan 1.1 → SPIR-V 1.3
        // Vulkan 1.2 → SPIR-V 1.5
        // Vulkan 1.3 → SPIR-V 1.6
        //
        // 注意：VkApplicationInfo.apiVersion 必须与 SPIR-V 版本兼容！
        //
        VkShaderModule vertextshader, fragmentShader;

        // ----------------------------------------------------------------------
        // 创建顶点着色器模块（Vertex Shader）
        // ----------------------------------------------------------------------
        // 顶点着色器作用：
        // - 处理每个顶点的位置、颜色、纹理坐标等属性
        // - 将顶点从模型空间转换到裁剪空间
        // - 传递数据到片段着色器
        //
        {
            // 读取 SPIR-V 字节码文件
            // platform_read_file 返回文件内容的字节数组和大小
            // 注意：返回的是 uint32_t*，因为 SPIR-V 是 32 位字对齐的
            uint32_t lengthInBytes;
            uint32_t* vertexCode =
                (uint32_t*)platform_read_file("assets/shaders/shader.vert.spv", &lengthInBytes);

            // 配置着色器模块创建信息
            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

            // pCode: 指向 SPIR-V 字节码的指针
            // 注意：SPIR-V 要求字对齐，所以使用 uint32_t* 而非 void*
            shaderInfo.pCode = vertexCode;

            // codeSize: 字节码的大小（字节数，不是字数！）
            //
            // 常见错误：使用 sizeof(vertexCode) 会得到指针大小（4/8 字节）
            //          必须使用文件读取返回的实际大小
            //
            shaderInfo.codeSize = lengthInBytes;

            // 创建着色器模块
            // 参数：
            // - device: 逻辑设备
            // - pCreateInfo: 创建信息
            // - pAllocator: 内存分配器（NULL = 使用默认分配器）
            // - pShaderModule: 输出的着色器模块句柄
            VK_CHECK(vkCreateShaderModule(vkContext->device, &shaderInfo, 0, &vertextshader));

            // 释放文件缓冲区
            // 注意：Shader Module 创建后，Vulkan 会复制 SPIR-V 代码
            //       所以可以安全删除原始文件缓冲区
            delete vertexCode;
        }

        // ----------------------------------------------------------------------
        // 创建片段着色器模块（Fragment Shader）
        // ----------------------------------------------------------------------
        // 片段着色器作用：
        // - 处理每个像素（片段）的颜色
        // - 计算光照、纹理采样、材质效果
        // - 输出最终颜色到帧缓冲
        //
        {
            // 读取 SPIR-V 字节码文件
            uint32_t lengthInBytes;
            uint32_t* fragmentCode =
                (uint32_t*)platform_read_file("assets/shaders/shader.frag.spv", &lengthInBytes);

            // 配置着色器模块创建信息
            VkShaderModuleCreateInfo shaderInfo = {};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.pCode = fragmentCode;
            shaderInfo.codeSize = lengthInBytes;

            // 创建着色器模块
            VK_CHECK(vkCreateShaderModule(vkContext->device, &shaderInfo, 0, &fragmentShader));

            // 释放文件缓冲区
            delete fragmentCode;
        }

        // ========================================================================
        // 2.8 配置着色器阶段（Shader Stages）
        // ========================================================================
        //
        // 着色器阶段定义了管线使用的着色器模块和入口函数
        //

        // ----------------------------------------------------------------------
        // 顶点着色器阶段
        // ----------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo vertStage = {};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        // stage: 着色器阶段类型
        //
        // VK_SHADER_STAGE_VERTEX_BIT:      顶点着色器（当前使用）
        // VK_SHADER_STAGE_FRAGMENT_BIT:    片段着色器
        // VK_SHADER_STAGE_GEOMETRY_BIT:    几何着色器（可选）
        // VK_SHADER_STAGE_COMPUTE_BIT:     计算着色器（独立管线）
        //
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;

        // module: 着色器模块句柄
        vertStage.module = vertextshader;

        // pName: 着色器入口函数名
        //
        // 必须与 GLSL 中的函数名匹配：
        // void main() { ... }  ← pName = "main"
        //
        vertStage.pName = "main";

        //.pSpecializationInfo = NULL;  // 特殊化信息（用于编译时常量）

        // ----------------------------------------------------------------------
        // 片段着色器阶段
        // ----------------------------------------------------------------------
        VkPipelineShaderStageCreateInfo fragStage = {};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragmentShader;
        fragStage.pName = "main";

        // 着色器阶段数组（管线可以包含多个着色器阶段）
        VkPipelineShaderStageCreateInfo shaderStages[2] = {vertStage, fragStage};

        // ========================================================================
        // 2.9 动态状态（Dynamic State）
        // ========================================================================
        //
        // 某些状态可以在运行时修改，而不需要重新创建管线
        //
        // 优势：
        // - 灵活性：可以在渲染循环中修改视口、裁剪矩形等
        // - 性能：不需要创建多个管线（不同视口大小）
        //
        // 常用动态状态：
        // VK_DYNAMIC_STATE_VIEWPORT:        视口（当前使用）
        // VK_DYNAMIC_STATE_SCISSOR:         裁剪矩形（当前使用）
        // VK_DYNAMIC_STATE_LINE_WIDTH:      线宽
        // VK_DYNAMIC_STATE_BLEND_CONSTANTS: 混合常量
        //
        // 注意：动态状态在创建管线时仍需提供初始值（占位符）
        //       实际值通过 vkCmdSetViewport、vkCmdSetScissor 等命令设置
        //
        VkDynamicState dynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT, // 视口可以在运行时修改
            VK_DYNAMIC_STATE_SCISSOR   // 裁剪矩形可以在运行时修改
        };

        VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = ArraySize(dynamicStates); // 动态状态数量
        dynamicState.pDynamicStates = dynamicStates;               // 动态状态数组

        // ========================================================================
        // 步骤 3：创建 Graphics Pipeline
        // ========================================================================
        //
        // vkCreateGraphicsPipelines 是 Vulkan 中最复杂的函数之一
        // 它一次性创建整个图形管线，包括所有状态和着色器
        //
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

        // ========================================================================
        // 步骤 3.1：绑定 Render Pass
        // ========================================================================
        //
        // renderPass: 此管线兼容的渲染通道
        //
        // 为什么需要 Render Pass？
        // - 管线需要知道附件的格式、数量
        // - 管线需要知道子阶段的配置
        // - 驱动可以根据 RenderPass 优化管线
        //
        // 注意：管线与 RenderPass 兼容性检查：
        //       - 附件数量必须匹配
        //       - 附件格式必须兼容
        //       - 子阶段配置必须匹配
        //
        pipelineInfo.renderPass = vkContext->renderPass;

        //.subpass = 0;  // 子阶段索引（默认为 0）

        // ========================================================================
        // 步骤 3.2：绑定各个状态
        // ========================================================================
        //
        // 将之前配置的所有状态绑定到管线
        //
        pipelineInfo.pVertexInputState = &vertextInputState;    // 顶点输入状态
        pipelineInfo.pInputAssemblyState = &inputAssembly;      // 输入装配状态
        pipelineInfo.pViewportState = &viewportState;           // 视口状态
        pipelineInfo.pRasterizationState = &rasterizationState; // 光栅化状态
        pipelineInfo.pMultisampleState = &multisampleState;     // 多重采样状态
        pipelineInfo.pColorBlendState = &colorBlendState;       // 颜色混合状态
        pipelineInfo.pDynamicState = &dynamicState;             // 动态状态

        //.pDepthStencilState = NULL;  // 深度/模板状态（未使用）

        // ========================================================================
        // 步骤 3.3：绑定着色器阶段
        // ========================================================================
        //
        pipelineInfo.stageCount = ArraySize(shaderStages); // 着色器阶段数量（2）
        pipelineInfo.pStages = shaderStages;               // 着色器阶段数组

        // ========================================================================
        // 步骤 3.4：绑定管线布局
        // ========================================================================
        //
        // layout: 管线布局（定义资源访问）
        //
        pipelineInfo.layout = vkContext->pipeLayout;

        // ========================================================================
        // 步骤 3.5：其他可选配置
        // ========================================================================
        //
        //.basePipelineHandle = VK_NULL_HANDLE;  // 基础管线（用于派生管线）
        //.basePipelineIndex = -1;               // 基础管线索引
        //
        // 派生管线：
        // - 如果多个管线配置相似，可以基于一个管线创建另一个
        // - 性能优化：驱动可以复用部分配置
        //

        // ========================================================================
        // 步骤 3.6：创建管线
        // ========================================================================
        //
        // vkCreateGraphicsPipelines 参数：
        // - device: 逻辑设备
        // - pipelineCache: 管线缓存（NULL = 不使用缓存）
        //                   管线缓存可以加速后续的管线创建
        // - createInfoCount: 创建的管线数量（支持批量创建）
        // - pCreateInfos: 管线创建信息数组
        // - pAllocator: 内存分配器（NULL = 使用默认分配器）
        // - pPipelines: 输出的管线句柄数组
        //
        VK_CHECK(vkCreateGraphicsPipelines(vkContext->device,
                                           0, // pipelineCache（不使用缓存）
                                           1, // 创建 1 个管线
                                           &pipelineInfo,
                                           0, // allocator
                                           &vkContext->pipeline));

        // ========================================================================
        // 步骤 4：清理着色器模块
        // ========================================================================
        //
        // 着色器模块在创建管线后就可以销毁了
        // 管线已经编译并存储了着色器代码
        //
        vkDestroyShaderModule(vkContext->device, vertextshader, 0);
        vkDestroyShaderModule(vkContext->device, fragmentShader, 0);
    }

    // ============================================================================
    // 创建 Staging Buffer（临时缓冲区）
    // ============================================================================
    //
    // 什么是 Staging Buffer？
    // ----------------------------------
    // Staging Buffer 是一种特殊的缓冲区，用于在 CPU 和 GPU 之间传输数据。
    //
    // 类比理解：
    // - Staging Buffer = 中转站
    // - CPU 把数据放到中转站 → GPU 从中转站取数据 → 传到目的地
    //
    // 为什么需要 Staging Buffer？
    // ----------------------------------
    // GPU 内存通常分为两种类型：
    // 1. HOST_VISIBLE（CPU 可访问）：慢，但 CPU 可以直接写入
    // 2. DEVICE_LOCAL（GPU 本地）：快，但 CPU 不能直接访问
    //
    // 最佳实践：
    // CPU → Staging Buffer (HOST_VISIBLE) → GPU Copy → Device Local Buffer
    //
    // Staging Buffer 的特点：
    // ----------------------------------
    // - HOST_VISIBLE: CPU 可以映射并写入
    // - HOST_COHERENT: CPU 写入后自动同步到 GPU（无需手动 flush）
    // - TRANSFER_SRC: 可以作为传输操作的源
    //
    {
        // ========================================================================
        // 步骤 1：创建 Buffer 对象
        // ========================================================================
        //
        // VkBuffer: 缓冲区对象，描述了一块内存的用途和大小
        // 注意：此时还没有分配实际的内存！
        //
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // ← 修复：原为 BUFFER_VIEW_CREATE_INFO

        // size: 缓冲区大小（字节）
        // MB(1) = 1 兆字节 = 1024 * 1024 字节
        bufferInfo.size = MB(1);

        // usage: 缓冲区用途（位掩码）
        //
        // VK_BUFFER_USAGE_TRANSFER_SRC_BIT: 可以作为传输操作的源
        // - 用于 vkCmdCopyBufferToImage 等命令
        // - 表示这个缓冲区的数据可以复制到其他地方
        //
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        //.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 默认：独占模式

        // 创建 Buffer 对象
        // 注意：此时 Buffer 还没有绑定内存，不能使用！
        VK_CHECK(vkCreateBuffer(vkContext->device, &bufferInfo, 0, &vkContext->stagingBuffer.buffer));

        // ========================================================================
        // 步骤 2：查询内存需求
        // ========================================================================
        //
        // vkGetBufferMemoryRequirements: 查询 Buffer 需要什么样的内存
        //
        // VkMemoryRequirements 结构：
        // - size:          需要的内存大小（可能大于 bufferInfo.size，因为对齐要求）
        // - alignment:     内存对齐要求（偏移量必须是 alignment 的倍数）
        // - memoryTypeBits: 位掩码，指示哪些内存类型兼容
        //
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(vkContext->device, vkContext->stagingBuffer.buffer, &memRequirements);

        // ========================================================================
        // 步骤 3：查询 GPU 内存属性
        // ========================================================================
        //
        // vkGetPhysicalDeviceMemoryProperties: 获取 GPU 的内存类型信息
        //
        // VkPhysicalDeviceMemoryProperties 结构：
        // - memoryTypes[]:    内存类型数组（最多 32 种）
        // - memoryHeaps[]:    内存堆数组（物理内存块）
        //
        // 每个 VkMemoryType 包含：
        // - propertyFlags:    内存属性（HOST_VISIBLE, DEVICE_LOCAL 等）
        // - heapIndex:        指向所属的内存堆
        //
        VkPhysicalDeviceMemoryProperties gpuMemProps;
        vkGetPhysicalDeviceMemoryProperties(vkContext->gpu, &gpuMemProps);

        // ========================================================================
        // 步骤 4：查找合适的内存类型
        // ========================================================================
        //
        // 内存类型查找逻辑：
        // 1. 内存类型必须与 Buffer 兼容（memoryTypeBits）
        // 2. 内存类型必须具备所需的属性（HOST_VISIBLE + HOST_COHERENT）
        //
        // memoryTypeBits: 位掩码，第 i 位为 1 表示内存类型 i 兼容
        // 例如：memoryTypeBits = 0b00000101 表示内存类型 0 和 2 兼容
        //
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = MB(1);  // 分配 1MB 内存

        // 遍历所有内存类型，找到第一个符合条件的
        for (uint32_t i = 0; i < gpuMemProps.memoryTypeCount; i++)
        {
            // 检查 1：这个内存类型与 Buffer 兼容吗？
            // memoryTypeBits & (1 << i) 检查第 i 位是否为 1
            uint32_t isCompatible = memRequirements.memoryTypeBits & (1 << i);

            // 检查 2：这个内存类型具备所需的属性吗？
            //
            // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT: CPU 可以映射（可见）
            // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: 自动同步（一致性）
            //
            // 注意：需要同时具备两个属性，所以用 AND 运算
            VkMemoryPropertyFlags requiredFlags =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

            uint32_t hasRequiredFlags =
                (gpuMemProps.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags;

            if (isCompatible && hasRequiredFlags)
            {
                // 找到了！使用这个内存类型
                allocInfo.memoryTypeIndex = i;
                break;
            }
        }

        // ========================================================================
        // 步骤 5：分配内存
        // ========================================================================
        //
        // vkAllocateMemory: 从 GPU 堆中分配内存
        //
        // 注意：内存分配是昂贵操作，应该尽量减少分配次数
        //       实际项目中通常会创建一个大的内存池，从中分配小块内存
        //
        VK_CHECK(vkAllocateMemory(vkContext->device, &allocInfo, 0, &vkContext->stagingBuffer.memory));

        // ========================================================================
        // 步骤 6：映射内存（获取 CPU 指针）
        // ========================================================================
        //
        // vkMapMemory: 将 GPU 内存映射到 CPU 地址空间
        //
        // 参数：
        // - memory:      要映射的内存
        // - offset:      偏移量（0 = 从头开始）
        // - size:        映射大小（WHOLE_SIZE = 0 表示全部）
        // - flags:       保留标志（通常为 0）
        // - ppData:      输出的 CPU 指针
        //
        // 映射后，CPU 可以通过 stagingBuffer.data 指针直接写入数据
        //
        VK_CHECK(vkMapMemory(vkContext->device,
                            vkContext->stagingBuffer.memory,
                            0,              // offset
                            MB(1),          // size
                            0,              // flags
                            &vkContext->stagingBuffer.data));

        // ========================================================================
        // 步骤 7：绑定内存到 Buffer
        // ========================================================================
        //
        // vkBindBufferMemory: 将内存绑定到 Buffer 对象
        //
        // 参数：
        // - buffer:  要绑定的 Buffer
        // - memory:  要绑定的内存
        // - offset:  内存的起始偏移（0 = 从头开始，必须满足 alignment 要求）
        //
        // 绑定后，Buffer 才能真正使用！
        //
        VK_CHECK(vkBindBufferMemory(vkContext->device,
                                    vkContext->stagingBuffer.buffer,
                                    vkContext->stagingBuffer.memory,
                                    0));  // offset

        // ========================================================================
        // 现在可以使用 Staging Buffer 了！
        // ========================================================================
        //
        // 使用流程：
        // 1. memcpy(stagingBuffer.data, srcData, size);  ← CPU 写入数据
        // 2. vkCmdCopyBufferToImage(...);                 ← GPU 复制数据
        //
    }

    // create image
    {
        uint32_t fileSize;
        DDSFile* data = (DDSFile*)platform_read_file("assets/textures/cakez.DDS", &fileSize);
        uint32_t textureSize = data->header.Width * data->header.Height * 4;
        memcpy(vkContext->stagingBuffer.data, &data->dataBegin, textureSize);
        
    }

    return true;
}

// ============================================================================
// 渲染函数（vk_render）
// ============================================================================
//
// 此函数执行每一帧的渲染操作，是游戏循环的核心。
//
// 渲染流程概述：
// 1. 从交换链获取可渲染的图像
// 2. 分配并录制命令缓冲区
// 3. 提交命令到队列执行
// 4. 将渲染完成的图像呈现到屏幕
//
// ============================================================================
bool vk_render(VkContext* vkContext)
{
    // ============================================================================
    // 第零步：等待上一帧完成（CPU-GPU 同步）
    // ============================================================================
    //
    // vkAcquireNextImageKHR 要求信号量必须是”未触发”状态
    // 如果上一帧 GPU 还在执行，acquireSemaphore 处于”已触发”状态
    // 必须用围栏等待 GPU 完成后再开始新的一帧
    //
    // vkWaitForFences: CPU 阻塞等待，直到围栏被 GPU 触发
    // vkResetFences: 围栏需要手动重置才能再次使用（与信号量不同）
    //
    vkWaitForFences(vkContext->device, 1, &vkContext->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(vkContext->device, 1, &vkContext->inFlightFence);

    // ============================================================================
    // 第一步：获取交换链图像
    // ============================================================================
    //
    // vkAcquireNextImageKHR:
    //   从交换链中获取一个可以渲染的图像
    //
    // 为什么需要这一步？
    //   - 交换链中的图像可能正在被显示
    //   - 必须等待显示完成后才能使用
    //   - acquireSemaphore 用于同步这个等待过程
    //
    uint32_t imgIdx; // 输出参数：获取到的图像索引

    // vkAcquireNextImageKHR 参数：
    //   1. device: 逻辑设备
    //   2. swapchain: 交换链句柄
    //   3. timeout: 超时时间（纳秒）
    //      - 0 = 立即返回（如果没有可用图像）
    //      - UINT64_MAX = 无限等待
    //      - 这里使用 0 表示不阻塞（如果没准备好就返回错误）
    //   4. semaphore: 信号量（获取完成后触发）
    //      - 当图像可用时，驱动会触发这个信号量
    //      - 后续的渲染操作会等待这个信号量
    //   5. fence: 围栏（NULL = 不使用）
    //      - 用于 CPU-GPU 同步，这里不需要
    //   6. pImageIndex: 输出参数，返回图像索引
    //      - 这个索引用于后续操作指定使用哪个图像
    //
    VK_CHECK(vkAcquireNextImageKHR(vkContext->device,           // 逻辑设备
                                   vkContext->swapChain,        // 交换链
                                   0,                           // 超时 = 0（不等待）
                                   vkContext->acquireSemaphore, // 获取完成信号量
                                   0,                           // 无围栏
                                   &imgIdx                      // 输出：图像索引
                                   ));

    // ============================================================================
    // 第二步：分配命令缓冲区
    // ============================================================================
    //
    // 什么是命令缓冲区？
    //   命令缓冲区用于记录 GPU 操作命令（绘制、复制、清除等）
    //
    // 为什么每帧都要重新分配？
    //   - 我们使用 ONE_TIME_SUBMIT_BIT 标志（只提交一次）
    //   - 执行后命令缓冲区不能重用
    //   - 每帧需要新的命令缓冲区
    //
    // 性能优化方向：
    //   - 实际项目中，应该预分配多个命令缓冲区
    //   - 使用命令缓冲区池循环使用
    //   - 避免每帧分配/释放的开销
    //
    VkCommandBuffer cmdBuffer; // 输出：命令缓冲区句柄

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = vkContext->commandPool; // 从哪个池分配
    allocInfo.commandBufferCount = 1;               // 分配 1 个缓冲区

    // vkAllocateCommandBuffers 参数：
    //   1. device: 逻辑设备
    //   2. pAllocateInfo: 分配信息
    //   3. pCommandBuffers: 输出数组（返回命令缓冲区句柄）
    //
    VK_CHECK(vkAllocateCommandBuffers(vkContext->device, &allocInfo, &cmdBuffer));

    // ============================================================================
    // 第三步：开始录制命令
    // ============================================================================
    //
    // 命令缓冲区必须先"开始录制"才能记录命令
    //
    VkCommandBufferBeginInfo beginInfo = cmd_begin_info(); // 使用辅助函数

    // vkBeginCommandBuffer 参数：
    //   1. commandBuffer: 命令缓冲区
    //   2. pBeginInfo: 开始信息（包含标志等）
    //
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

    // ============================================================================
    // 第四步：记录渲染命令（使用 Render Pass）
    // ============================================================================
    //
    // 重要变化：不再使用 vkCmdClearColorImage + 手动布局转换
    //
    // 旧方法（已弃用）：
    //   1. transition_image_layout(UNDEFINED → TRANSFER_DST)
    //   2. vkCmdClearColorImage()
    //   3. transition_image_layout(TRANSFER_DST → PRESENT_SRC)
    //
    // 新方法（当前使用）：
    //   1. vkCmdBeginRenderPass()  → 自动处理布局转换 + 清除
    //   2. [渲染命令]
    //   3. vkCmdEndRenderPass()    → 自动转换到 PRESENT_SRC
    //
    // RenderPass 的优势：
    // - 自动处理图像布局转换（无需手动 transition）
    // - 驱动可以优化整个渲染过程
    // - 更符合 Vulkan 的设计理念
    // - 支持多子阶段渲染（延迟渲染、后处理等）
    //
    // ========================================================================
    // 步骤 4.1：配置清除值
    // ========================================================================
    //
    // VkClearValue: 定义如何清除附件
    //
    // union {  // 联合体，可以是不同类型的清除值
    //     VkClearColorValue color;    // 颜色清除值（RGBA）
    //     VkClearDepthStencilValue depthStencil;  // 深度/模板清除值
    // };
    //
    // VkClearColorValue 定义：
    // union {
    //     float float32[4];   // 浮点数 RGBA (当前使用)
    //     int32_t int32[4];   // 整数 RGBA
    //     uint32_t uint32[4]; // 无符号整数 RGBA
    // };
    //
    VkClearValue color = {1, 1, 0, 1};
    //                 ↑  ↑  ↑  ↑
    //                 R  G  B  A
    //                 = 黄色，完全不透明
    //
    // 注意：这里使用 C++ 的初始化列表语法，等价于：
    //       color.color.float32[0] = 1.0f;  // R
    //       color.color.float32[1] = 1.0f;  // G
    //       color.color.float32[2] = 0.0f;  // B
    //       color.color.float32[3] = 1.0f;  // A

    // ========================================================================
    // 步骤 4.2：配置 RenderPass 开始信息
    // ========================================================================
    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;

    // renderPass: 要使用的渲染通道
    // 必须与创建 Framebuffer 时使用的 RenderPass 兼容
    rpBeginInfo.renderPass = vkContext->renderPass;

    // framebuffer: 要渲染到的帧缓冲
    // 注意：根据当前获取的图像索引选择对应的 Framebuffer
    rpBeginInfo.framebuffer = vkContext->framebuffers[imgIdx];

    // renderArea: 受影响的渲染区域
    //
    // VkRect2D 结构：
    // struct {
    //     VkOffset2D offset;  // 区域起始偏移（通常为 0, 0）
    //     VkExtent2D extent;  // 区域大小（宽度、高度）
    // };
    //
    // 清除操作会清除整个 renderArea，所以这里设置为整个屏幕
    rpBeginInfo.renderArea.offset = {0, 0};                // 从左上角开始
    rpBeginInfo.renderArea.extent = vkContext->screenSize; // 整个屏幕大小

    // clearValueCount: 清除值数组的大小
    // 必须与 RenderPass 中附件的数量匹配
    // （当前 RenderPass 只有 1 个颜色附件）
    rpBeginInfo.clearValueCount = 1;

    // pClearValues: 指向清除值数组的指针
    // 每个附件对应一个清除值（按附件索引顺序）
    rpBeginInfo.pClearValues = &color;

    // ========================================================================
    // 步骤 4.3：开始渲染通道
    // ========================================================================
    //
    // vkCmdBeginRenderPass 会自动：
    // 1. 将图像布局转换到初始布局（UNDEFINED → COLOR_ATTACHMENT_OPTIMAL）
    // 2. 执行清除操作（使用 clearValue 中指定的颜色）
    // 3. 准备好接收渲染命令
    //
    // 参数：
    // - commandBuffer: 命令缓冲区
    // - pRenderPassBegin: 渲染通道开始信息
    // - contents: 子阶段内容类型
    //   * VK_SUBPASS_CONTENTS_INLINE: 渲染命令直接记录在主命令缓冲区（当前使用）
    //   * VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: 使用次要命令缓冲区
    //
    vkCmdBeginRenderPass(cmdBuffer, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ========================================================================
    // 步骤 4.4：记录渲染命令
    // ========================================================================
    {
        // ----------------------------------------------------------------------
        // 配置视口（Viewport）
        // ----------------------------------------------------------------------
        //
        // 视口定义了如何将 NDC 坐标映射到屏幕坐标
        //
        // VkViewport 结构：
        // - x, y: 视口左上角位置
        // - width, height: 视口大小
        // - minDepth: 深度最小值（映射到 NDC 的 -1，默认 0.0）
        // - maxDepth: 深度最大值（映射到 NDC 的 +1，默认 1.0）
        //
        VkViewport viewPort = {};
        viewPort.x = 0.0f;                                     // 左上角 X
        viewPort.y = 0.0f;                                     // 左上角 Y
        viewPort.width = (float)vkContext->screenSize.width;   // 宽度（屏幕宽度）
        viewPort.height = (float)vkContext->screenSize.height; // 高度（屏幕高度）
        viewPort.minDepth = 0.0f;                              // 深度最小值
        viewPort.maxDepth = 1.0f;                              // 深度最大值

        // vkCmdSetViewport: 设置当前绑定的管线的视口
        // 参数：
        // - commandBuffer: 命令缓冲区
        // - firstViewport: 第一个视口的索引（支持多个视口）
        // - viewportCount: 视口数量
        // - pViewports: 视口数组
        //
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewPort);

        // ----------------------------------------------------------------------
        // 配置裁剪矩形（Scissor）
        // ----------------------------------------------------------------------
        //
        // 裁剪矩形定义了哪些像素会被渲染（之外的像素被丢弃）
        //
        // VkRect2D 结构：
        // struct {
        //     VkOffset2D offset;  // 裁剪区域起始偏移
        //     VkExtent2D extent;  // 裁剪区域大小
        // };
        //
        // 注意：如果设置裁剪区域小于视口，只有区域内的像素会被渲染
        //
        VkRect2D scissor = {};
        scissor.offset = {0, 0};                // 从左上角开始
        scissor.extent = vkContext->screenSize; // 整个屏幕（不裁剪）

        // vkCmdSetScissor: 设置当前绑定的管线的裁剪矩形
        // 参数：
        // - commandBuffer: 命令缓冲区
        // - firstScissor: 第一个裁剪矩形的索引
        // - scissorCount: 裁剪矩形数量
        // - pScissors: 裁剪矩形数组
        //
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // ----------------------------------------------------------------------
        // 绑定图形管线
        // ----------------------------------------------------------------------
        //
        // vkCmdBindPipeline: 将管线绑定到命令缓冲区
        // 参数：
        // - commandBuffer: 命令缓冲区
        // - pipelinePoint: 管线绑定点
        //   * VK_PIPELINE_BIND_POINT_GRAPHICS: 图形管线（当前使用）
        //   * VK_PIPELINE_BIND_POINT_COMPUTE: 计算管线
        // - pipeline: 管线对象句柄
        //
        // 注意：绑定管线后，后续的绘制命令会使用这个管线的状态
        //       （着色器、混合模式、光栅化状态等）
        //
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkContext->pipeline);

        // ----------------------------------------------------------------------
        // 绘制三角形
        // ----------------------------------------------------------------------
        //
        // vkCmdDraw: 发起绘制调用
        // 参数：
        // - commandBuffer: 命令缓冲区
        // - vertexCount: 要绘制的顶点数量（3 个顶点 = 三角形）
        // - instanceCount: 实例数量（1 = 单个实例）
        // - firstVertex: 第一个顶点的索引（0 = 从第 0 个顶点开始）
        // - firstInstance: 第一个实例的索引（0 = 从第 0 个实例开始）
        //
        // 执行流程：
        // 1. Vulkan 调用顶点着色器 3 次（vertexCount=3）
        // 2. 每次调用 gl_VertexIndex 从 0 递增到 2
        // 3. 顶点着色器从 vertices[gl_VertexIndex] 读取坐标
        // 4. 3 个顶点组装成三角形
        // 5. 光栅化为像素
        // 6. 每个像素调用片段着色器
        // 7. 输出颜色到 Framebuffer
        //
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    }

    // ========================================================================
    // 步骤 4.5：结束渲染通道
    // ========================================================================
    //
    // vkCmdEndRenderPass 会自动：
    // 1. 完成所有渲染命令
    // 2. 将图像布局转换到最终布局（COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR）
    // 3. 准备好图像用于呈现
    //
    // 重要：不需要手动转换布局到 PRESENT_SRC_KHR，RenderPass 会自动处理！
    //
    vkCmdEndRenderPass(cmdBuffer);

    // ============================================================================
    // 第五步：结束录制命令
    // ============================================================================
    //
    // 必须调用此函数告诉 Vulkan 命令录制完成
    //
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    // ============================================================================
    // 第六步：提交命令到队列
    // ============================================================================
    //
    // 提交后，GPU 会开始执行命令缓冲区中的命令
    //
    // 提交信息配置：
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // ↑ 等待阶段：在哪个管线阶段等待信号量
    //   - COLOR_ATTACHMENT_OUTPUT: 颜色附件输出阶段
    //   - 表示在写入颜色之前等待图像获取完成

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // 等待的信号量（在执行前需要等待哪些信号量）
    submitInfo.waitSemaphoreCount = 1;                         // 等待 1 个信号量
    submitInfo.pWaitSemaphores = &vkContext->acquireSemaphore; // 等待获取信号量
    submitInfo.pWaitDstStageMask = &waitStage;                 // 在哪个阶段等待

    // 命令缓冲区（要执行哪些命令）
    submitInfo.commandBufferCount = 1;       // 1 个命令缓冲区
    submitInfo.pCommandBuffers = &cmdBuffer; // 命令缓冲区指针

    // 触发的信号量（执行完成后触发哪些信号量）
    submitInfo.signalSemaphoreCount = 1;                                 // 触发 1 个信号量
    submitInfo.pSignalSemaphores = &vkContext->submitSemaphores[imgIdx]; // 使用当前图像的信号量

    // vkQueueSubmit 参数：
    //   1. queue: 队列（图形队列）
    //   2. submitCount: 提交信息数量
    //   3. pSubmits: 提交信息数组
    //   4. fence: 围栏（GPU 完成后会触发，下一帧 CPU 等待此围栏）
    //
    VK_CHECK(vkQueueSubmit(vkContext->graphicsQueue, 1, &submitInfo, vkContext->inFlightFence));

    // 执行流程：
    // 1. 等待 acquireSemaphore 触发（图像可用）
    // 2. 在颜色附件输出阶段之前等待
    // 3. 执行命令缓冲区（清除图像为黄色）
    // 4. 触发 submitSemaphore（渲染完成）

    // ============================================================================
    // 第七步：呈现图像到屏幕
    // ============================================================================
    //
    // 将渲染完成的图像提交给显示系统
    //
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // 等待的信号量（渲染完成后才能呈现）
    presentInfo.waitSemaphoreCount = 1;                                 // 等待 1 个信号量
    presentInfo.pWaitSemaphores = &vkContext->submitSemaphores[imgIdx]; // 等待当前图像的信号量

    // 交换链信息（要呈现哪个图像）
    presentInfo.swapchainCount = 1;                  // 1 个交换链
    presentInfo.pSwapchains = &vkContext->swapChain; // 交换链指针
    presentInfo.pImageIndices = &imgIdx;             // 图像索引

    // vkQueuePresentKHR 参数：
    //   1. queue: 队列（通常是图形队列，也支持呈现）
    //   2. pPresentInfo: 呈现信息
    //
    VK_CHECK(vkQueuePresentKHR(vkContext->graphicsQueue, &presentInfo));

    // ============================================================================
    // 渲染循环完成！
    // ============================================================================
    //
    // 完整流程总结：
    //
    // 1. 获取图像（等待上一帧显示完成）
    //    └─ 触发 acquireSemaphore
    //
    // 2. 分配命令缓冲区
    //
    // 3. 录制命令（清除为黄色）
    //
    // 4. 提交命令
    //    ├─ 等待 acquireSemaphore
    //    └─ 触发 submitSemaphore
    //
    // 5. 呈现图像
    //    └─ 等待 submitSemaphore
    //
    // 注意：
    // - 这个实现缺少一些优化和错误处理
    // - 实际项目中需要：
    //   1. 命令缓冲池（避免每帧分配）
    //   2. 围栏（避免 CPU 超前 GPU 太多）
    //   3. 多帧同步（双缓冲或三缓冲）
    //   4. 资源清理（命令缓冲区、临时对象）
    //
    // - 当前实现会在下一帧覆盖命令缓冲区
    //   如果 GPU 还在使用，会导致问题
    //   需要使用围栏或多个命令缓冲区来同步
    //
    return true;
}
