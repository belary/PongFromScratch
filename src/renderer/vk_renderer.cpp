#include <vulkan/vulkan.h>

#include "platform.h"

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

    uint32_t scImgCount;
    VkImage scImages[5];

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
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
        // ↑ 附件在使用时的布局
        // ATTACHMENT_OPTIMAL: 针对附件操作优化的布局
        // 驱动会自动选择最优布局，不需要我们指定具体布局

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

    // Frame Buffers
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vkContext->renderPass;
        // fbInfo.width = vkContext->
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

    // 转换布局：UNDEFINED -> TRANSFER_DST
    // 注意：必须在 vkBeginCommandBuffer 之后！
    transition_image_layout(cmdBuffer, vkContext->scImages[imgIdx], VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // ============================================================================
    // 第四步：记录渲染命令
    // ============================================================================
    //
    // 这里我们只做最简单的操作：清除图像为黄色
    // 实际项目中会记录：
    //   - 开始渲染通道
    //   - 绑定管线
    //   - 绑定资源
    //   - 绘制物体
    //   - 结束渲染通道
    //
    {
        // --------------------------------------------------------------
        // 清除图像
        // --------------------------------------------------------------
        // 注意：此时图像布局已转换为 TRANSFER_DST_OPTIMAL

        // 清除颜色（RGBA）
        // {R, G, B, A} = {1, 1, 0, 1} = 黄色，完全不透明
        VkClearColorValue color = {1, 1, 0, 1};

        // 图像子资源范围（要清除图像的哪一部分）
        //
        // VkImageSubresourceRange 指定图像的子资源范围
        // 一张图像可能有多个 mip 层和多个图层（用于 3D 纹理或 VR）
        //
        VkImageSubresourceRange range = {};

        // aspectMask: 图像的哪些方面
        //
        // 图像可以有多个"方面"（aspect）：
        //   - VK_IMAGE_ASPECT_COLOR_BIT:     颜色数据（RGB/RGBA）
        //   - VK_IMAGE_ASPECT_DEPTH_BIT:     深度数据（深度缓冲）
        //   - VK_IMAGE_ASPECT_STENCIL_BIT:   模板数据（模板缓冲）
        //   - VK_IMAGE_ASPECT_METADATA_BIT:  元数据（稀疏纹理用）
        //
        // 我们清除的是颜色图像，所以只选择 COLOR
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        // levelCount: Mip 层级数量
        //
        // Mip Mapping（多级渐远纹理）：
        //   - Level 0 = 原始尺寸（最大）
        //   - Level 1 = 一半尺寸
        //   - Level 2 = 四分之一尺寸
        //   - ...
        //
        // 举例：
        //   1024x1024 的纹理：
        //     Level 0: 1024x1024
        //     Level 1: 512x512
        //     Level 2: 256x256
        //     Level 3: 128x128
        //     ...
        //
        // 这里设为 1 = 只清除 Level 0（原始尺寸）
        // 交换链图像通常只有 1 个 mip 层级
        range.levelCount = 1;

        // layerCount: 图层数量
        //
        // 图层用于立体渲染（VR）或纹理数组：
        //   - Layer 0: 左眼视图 / 第一张纹理
        //   - Layer 1: 右眼视图 / 第二张纹理
        //   - ...
        //
        // 普通窗口渲染只需要 1 层
        // VR 应用需要 2 层（左眼 + 右眼）
        // 立方体纹理需要 6 层（6 个面）
        //
        // 这里设为 1 = 只清除 Layer 0
        range.layerCount = 1;

        // vkCmdClearColorImage: 清除图像命令
        //
        // 参数：
        //   1. commandBuffer: 命令缓冲区
        //   2. image: 要清除的图像
        //   3. imageLayout: 图像布局
        //      - 必须是 TRANSFER_DST_OPTIMAL（不是 PRESENT_SRC_KHR！）
        //      - 之前已通过 Pipeline Barrier 转换布局
        //   4. pColor: 清除颜色
        //   5. rangeCount: 范围数量（通常是 1）
        //   6. pRanges: 范围数组
        //
        vkCmdClearColorImage(cmdBuffer,                   // 命令缓冲区
                             vkContext->scImages[imgIdx], // 要清除的图像（使用获取的索引）
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             &color, // 清除颜色（黄色）
                             1,      // 范围数量
                             &range  // 范围（整个图像）
        );

        // 【新增】转换布局：TRANSFER_DST -> PRESENT_SRC
        transition_image_layout(cmdBuffer, vkContext->scImages[imgIdx],
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

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
